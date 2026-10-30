#include "hush/forensic/ForensicRack.hpp"
#include <cmath>
#include <sstream>

namespace hush::forensic {

void ForensicRack::set_slot(Slot s, SlotState st) noexcept {
  const auto i = static_cast<std::size_t>(s);
  if (i >= slots_.size()) return;
  if (st.amount < 0.f) st.amount = 0.f;
  if (st.amount > 1.f) st.amount = 1.f;
  slots_[i] = st;
}

SlotState ForensicRack::slot(Slot s) const noexcept {
  const auto i = static_cast<std::size_t>(s);
  return i < slots_.size() ? slots_[i] : SlotState{};
}

void ForensicRack::process(float* interleaved, std::size_t frames,
                           std::size_t channels) noexcept {
  if (!interleaved || frames == 0 || channels == 0) return;
  ++blocks_;
  const std::size_t n = frames * channels;

  // Spectral denoise: gentle high-shelf attenuation of low-level noise floor proxy
  if (slots_[0].enabled && slots_[0].amount > 0.f) {
    const float a = slots_[0].amount * 0.15f;
    for (std::size_t i = 0; i < n; ++i) {
      float x = interleaved[i];
      if (x > -0.02f && x < 0.02f) x *= (1.f - a);
      interleaved[i] = x;
    }
  }
  // Hiss: one-pole lowpass mix
  if (slots_[1].enabled && slots_[1].amount > 0.f) {
    const float mix = slots_[1].amount * 0.35f;
    float z = 0.f;
    for (std::size_t i = 0; i < n; ++i) {
      z = z + 0.08f * (interleaved[i] - z);
      interleaved[i] = interleaved[i] * (1.f - mix) + z * mix;
    }
  }
  // Click: soft clip of impulse-like samples
  if (slots_[2].enabled && slots_[2].amount > 0.f) {
    const float thr = 0.85f - slots_[2].amount * 0.4f;
    for (std::size_t i = 0; i < n; ++i) {
      float x = interleaved[i];
      if (x > thr) x = thr + (x - thr) * 0.2f;
      if (x < -thr) x = -thr + (x + thr) * 0.2f;
      interleaved[i] = x;
    }
  }
  // Hum: subtract weak 60 Hz component estimate
  if (slots_[3].enabled && slots_[3].amount > 0.f) {
    const float depth = slots_[3].amount * 0.04f;
    const float w = 2.f * 3.14159265f * 60.f / 48000.f;
    for (std::size_t i = 0; i < frames; ++i) {
      const float h = std::sin(hum_phase_) * depth;
      hum_phase_ += w;
      if (hum_phase_ > 2.f * 3.14159265f) hum_phase_ -= 2.f * 3.14159265f;
      for (std::size_t c = 0; c < channels; ++c)
        interleaved[i * channels + c] -= h;
    }
  }
}

std::string Report::to_text() const {
  std::ostringstream o;
  o << "HUSH Forensic Report " << analysis_version << "\nSHA-256: " << file_sha256 << "\n";
  for (const auto& e : evidence) {
    o << "- [" << e.status << "] " << e.module << " conf=" << e.confidence
      << " " << e.label << "\n";
  }
  o << "Limitations:\n";
  for (const auto& l : limitations) o << "  * " << l << "\n";
  return o.str();
}

Report analyze_buffer(const float* interleaved, std::size_t frames, std::size_t channels,
                      double sample_rate, const std::string& file_sha256) {
  Report r;
  r.file_sha256 = file_sha256;
  r.limitations = {
      "Indicators only — not proof of tampering, authenticity, or authorship.",
      "Uncalibrated confidence scores.",
      "No network / cloud analysis; local offline only."};
  if (!interleaved || frames < 16 || channels == 0 || sample_rate < 8000.0) {
    r.evidence.push_back({"buffer", "Idle", "insufficient input", 0, 0, false,
                          "need frames>=16 and valid rate"});
    return r;
  }
  // Peak / RMS rough scan
  double peak = 0, sum2 = 0;
  const std::size_t n = frames * channels;
  for (std::size_t i = 0; i < n; ++i) {
    const double x = interleaved[i];
    const double a = x < 0 ? -x : x;
    if (a > peak) peak = a;
    sum2 += x * x;
  }
  const double rms = std::sqrt(sum2 / static_cast<double>(n));
  Evidence e;
  e.module = "level scan";
  e.status = peak > 0.99 ? "Alert" : "Active";
  e.label = "peak/rms survey";
  e.confidence = peak * 100.0;
  e.note = "rms=" + std::to_string(rms);
  r.evidence.push_back(std::move(e));
  return r;
}

}  // namespace hush::forensic
