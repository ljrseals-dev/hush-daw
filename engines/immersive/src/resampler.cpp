// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/resampler.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace hush {
namespace immersive {

namespace {

constexpr double kPi = 3.14159265358979323846;

double blackmanHarris(int n, int N) {
  if (N <= 1) return 1.0;
  const double x = static_cast<double>(n) / static_cast<double>(N - 1);
  constexpr double a0 = 0.35875;
  constexpr double a1 = 0.48829;
  constexpr double a2 = 0.14128;
  constexpr double a3 = 0.01168;
  return a0 - a1 * std::cos(2.0 * kPi * x) + a2 * std::cos(4.0 * kPi * x) -
         a3 * std::cos(6.0 * kPi * x);
}

std::vector<float> designFilter(double inputRate, double outputRate, int taps) {
  if (taps % 2 == 0) ++taps;
  const double nyquist = std::min(inputRate, outputRate) * 0.5;
  const double cutoff = nyquist * 0.95;
  const double fc = cutoff / inputRate;
  const int mid = taps / 2;

  std::vector<float> h(static_cast<std::size_t>(taps));
  double sum = 0.0;
  for (int n = 0; n < taps; ++n) {
    const double t = static_cast<double>(n - mid);
    double sinc;
    if (std::abs(t) < 1e-12)
      sinc = 2.0 * fc;
    else
      sinc = std::sin(2.0 * kPi * fc * t) / (kPi * t);
    h[static_cast<std::size_t>(n)] =
        static_cast<float>(sinc * blackmanHarris(n, taps));
    sum += h[static_cast<std::size_t>(n)];
  }
  if (std::abs(sum) > 1e-12) {
    const float inv = static_cast<float>(1.0 / sum);
    for (float& v : h) v *= inv;
  }
  return h;
}

}  // namespace

struct Resampler::Impl {
  ResampleSpec spec_;
  double ratio_{1.0};
  int taps_{0};
  int mid_{0};
  std::vector<float> coeffs_;

  // Full input ring large enough for streaming (2 seconds at max rate)
  static constexpr int kRingSize = 1 << 16;  // 65536
  std::vector<std::vector<float>> ring_;    // [ch][ring]
  long long totalIn_{0};                    // absolute samples pushed
  long long outIndex_{0};
  bool flushed_{false};

  void pushInput(const float* in, int frames) {
    const int ch = spec_.channels;
    for (int f = 0; f < frames; ++f) {
      const int idx = static_cast<int>(totalIn_ & (kRingSize - 1));
      for (int c = 0; c < ch; ++c)
        ring_[static_cast<std::size_t>(c)][static_cast<std::size_t>(idx)] =
            in[f * ch + c];
      ++totalIn_;
    }
  }

  float sampleAt(int ch, long long absIdx) const {
    if (absIdx < 0 || absIdx >= totalIn_) return 0.f;
    const int idx = static_cast<int>(absIdx & (kRingSize - 1));
    // Guard against ring wrap overwriting old data
    if (totalIn_ - absIdx >= kRingSize) return 0.f;
    return ring_[static_cast<std::size_t>(ch)][static_cast<std::size_t>(idx)];
  }

  // Shared-clock convolution: same srcPos / frac for every channel.
  float convolve(int ch, double srcPos) const {
    const long long base = static_cast<long long>(std::floor(srcPos));
    const double frac = srcPos - static_cast<double>(base);
    float sum = 0.f;
    for (int k = 0; k < taps_; ++k) {
      // Tap k is centred at mid_; sample index = base - mid_ + k
      const long long absIdx = base - mid_ + k;
      // Linear-interpolate coefficient for fractional delay
      const double src = static_cast<double>(k) - frac;
      const int i0 = static_cast<int>(std::floor(src));
      const double f = src - i0;
      float c0 = (i0 >= 0 && i0 < taps_)
                     ? coeffs_[static_cast<std::size_t>(i0)]
                     : 0.f;
      float c1 = (i0 + 1 >= 0 && i0 + 1 < taps_)
                     ? coeffs_[static_cast<std::size_t>(i0 + 1)]
                     : 0.f;
      const float coeff = static_cast<float>((1.0 - f) * c0 + f * c1);
      sum += coeff * sampleAt(ch, absIdx);
    }
    return sum;
  }
};

Resampler::Resampler(const ResampleSpec& spec)
    : impl_(std::make_unique<Impl>()) {
  if (spec.inputRate <= 0.0 || spec.outputRate <= 0.0)
    throw std::invalid_argument("Resampler: rates must be positive");
  if (spec.channels <= 0)
    throw std::invalid_argument("Resampler: channels must be positive");
  if (spec.quality < 1 || spec.quality > 20)
    throw std::invalid_argument("Resampler: quality must be 1..20");

  impl_->spec_ = spec;
  impl_->ratio_ = spec.outputRate / spec.inputRate;
  impl_->taps_ = 8 * spec.quality + 1;
  if (impl_->taps_ % 2 == 0) ++impl_->taps_;
  impl_->mid_ = impl_->taps_ / 2;
  impl_->coeffs_ =
      designFilter(spec.inputRate, spec.outputRate, impl_->taps_);
  impl_->ring_.assign(
      static_cast<std::size_t>(spec.channels),
      std::vector<float>(static_cast<std::size_t>(Impl::kRingSize), 0.f));
  impl_->totalIn_ = 0;
  impl_->outIndex_ = 0;
  impl_->flushed_ = false;
}

Resampler::~Resampler() = default;
Resampler::Resampler(Resampler&&) noexcept = default;
Resampler& Resampler::operator=(Resampler&&) noexcept = default;

int Resampler::process(const float* in, int numInputFrames, float* out) {
  if (!in || !out || numInputFrames < 0)
    throw std::invalid_argument("Resampler::process: bad args");
  if (impl_->flushed_)
    throw std::runtime_error("Resampler::process after flush without reset");

  if (numInputFrames > 0) impl_->pushInput(in, numInputFrames);

  int written = 0;
  const int ch = impl_->spec_.channels;
  // Can produce output while srcPos + mid < totalIn_
  while (true) {
    const double srcPos =
        static_cast<double>(impl_->outIndex_) / impl_->ratio_;
    if (srcPos + impl_->mid_ + 1 > static_cast<double>(impl_->totalIn_))
      break;
    for (int c = 0; c < ch; ++c)
      out[written * ch + c] = impl_->convolve(c, srcPos);
    ++written;
    ++impl_->outIndex_;
  }
  return written;
}

int Resampler::flush(float* out) {
  if (!out) throw std::invalid_argument("Resampler::flush: null out");
  if (impl_->flushed_) return 0;
  impl_->flushed_ = true;

  // Push zeros for the remaining group delay so the kernel can finish
  const int zerosNeeded = impl_->taps_ + 2;
  std::vector<float> z(static_cast<std::size_t>(impl_->spec_.channels), 0.f);
  for (int i = 0; i < zerosNeeded; ++i) impl_->pushInput(z.data(), 1);

  int written = 0;
  const int ch = impl_->spec_.channels;
  const int maxTail = tailFrames() + impl_->taps_;
  while (written < maxTail) {
    const double srcPos =
        static_cast<double>(impl_->outIndex_) / impl_->ratio_;
    if (srcPos + impl_->mid_ + 1 > static_cast<double>(impl_->totalIn_))
      break;
    for (int c = 0; c < ch; ++c)
      out[written * ch + c] = impl_->convolve(c, srcPos);
    ++written;
    ++impl_->outIndex_;
  }
  return written;
}

void Resampler::reset() {
  for (auto& r : impl_->ring_) std::fill(r.begin(), r.end(), 0.f);
  impl_->totalIn_ = 0;
  impl_->outIndex_ = 0;
  impl_->flushed_ = false;
}

int Resampler::tailFrames() const noexcept {
  return static_cast<int>(
             std::ceil(static_cast<double>(impl_->mid_) * impl_->ratio_)) +
         2;
}

int Resampler::filterTaps() const noexcept { return impl_->taps_; }

double Resampler::ratio() const noexcept { return impl_->ratio_; }

const std::vector<float>& Resampler::coefficients() const noexcept {
  return impl_->coeffs_;
}

double Resampler::phaseForOutputFrame(long long outputIndex) const noexcept {
  const double srcPos =
      static_cast<double>(outputIndex) / impl_->ratio_;
  return srcPos - std::floor(srcPos);
}

std::vector<float> resampleBuffer(const float* in, int numInputFrames,
                                  const ResampleSpec& spec) {
  Resampler r(spec);
  const int maxOut =
      static_cast<int>(
          std::ceil(static_cast<double>(numInputFrames) * r.ratio())) +
      r.tailFrames() + r.filterTaps() + 16;
  std::vector<float> out(
      static_cast<std::size_t>(maxOut) * spec.channels, 0.f);
  int n = r.process(in, numInputFrames, out.data());
  n += r.flush(out.data() + n * spec.channels);
  out.resize(static_cast<std::size_t>(n) * spec.channels);
  return out;
}

std::vector<float> resampleRendererOutput(const std::vector<float>& in,
                                          int numChannels, int numInputFrames,
                                          double inRate, double outRate) {
  ResampleSpec spec;
  spec.inputRate = inRate;
  spec.outputRate = outRate;
  spec.channels = numChannels;
  spec.quality = 10;
  return resampleBuffer(in.data(), numInputFrames, spec);
}

}  // namespace immersive
}  // namespace hush
