#pragma once
// hush::forensic — midterm forensic rack
// Findings are indicators with stated limitations — never proof of authenticity.
// RT slots process in-place; analysis path is offline / control-thread only.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hush::forensic {

enum class Slot : std::uint8_t {
  SpectralDenoise = 0,
  HissReduction,
  ClickRemoval,
  HumEliminator,
  Count
};

struct SlotState {
  bool enabled{false};
  float amount{0.f};   // 0..1
  float aux{0.f};      // e.g. hum notch bias
};

// Lightweight in-process rack (spectral placeholders are gain/shelf shaped — not ML).
class ForensicRack {
public:
  void set_slot(Slot s, SlotState st) noexcept;
  [[nodiscard]] SlotState slot(Slot s) const noexcept;

  // Audio callback safe: no alloc. interleaved stereo or mono.
  void process(float* interleaved, std::size_t frames, std::size_t channels) noexcept;

  [[nodiscard]] std::uint64_t blocks_processed() const noexcept { return blocks_; }

private:
  std::array<SlotState, static_cast<std::size_t>(Slot::Count)> slots_{};
  std::uint64_t blocks_{0};
  float hum_phase_{0.f};
};

struct Evidence {
  std::string module;
  std::string status;     // Idle / Active / Alert
  std::string label;
  double confidence{0};   // 0..100 uncalibrated
  double location_hz{0};
  bool applicable{true};
  std::string note;
};

struct Report {
  std::string file_sha256;
  std::string analysis_version{"hush-forensic-2.2.1"};
  std::vector<Evidence> evidence;
  std::vector<std::string> limitations;
  std::string to_text() const;
};

// Offline indicator scan (control thread). Does not claim authenticity.
Report analyze_buffer(const float* interleaved, std::size_t frames, std::size_t channels,
                      double sample_rate, const std::string& file_sha256);

}  // namespace hush::forensic
