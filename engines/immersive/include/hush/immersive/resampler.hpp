// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include <memory>
#include <vector>

namespace hush {
namespace immersive {

struct ResampleSpec {
  double inputRate{48000.0};
  double outputRate{48000.0};
  int channels{2};
  int quality{10};  // 1..20 → taps = 8*quality+1
};

class Resampler {
 public:
  explicit Resampler(const ResampleSpec& spec);

  Resampler(const Resampler&) = delete;
  Resampler& operator=(const Resampler&) = delete;
  Resampler(Resampler&&) noexcept;
  Resampler& operator=(Resampler&&) noexcept;
  ~Resampler();

  /// Push numInputFrames of interleaved input. Writes available
  /// output frames into out. Returns frames written.
  /// Caller must size out for ceil(numInputFrames * ratio) + tailFrames().
  int process(const float* in, int numInputFrames, float* out);

  /// Drain the filter tail. Call once at end of stream.
  int flush(float* out);

  /// Reset filter state to silence. Keeps coefficients.
  void reset();

  int tailFrames() const noexcept;
  int filterTaps() const noexcept;
  double ratio() const noexcept;
  const std::vector<float>& coefficients() const noexcept;

  /// Sub-sample offset (input samples) applied for a given output index.
  double phaseForOutputFrame(long long outputIndex) const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/// One-shot convenience (allocates; not for real-time).
std::vector<float> resampleBuffer(const float* in, int numInputFrames,
                                  const ResampleSpec& spec);

/// Resample a whole Renderer output buffer, preserving channel order
/// and phase coherence across channels.
std::vector<float> resampleRendererOutput(const std::vector<float>& in,
                                          int numChannels, int numInputFrames,
                                          double inRate, double outRate);

}  // namespace immersive
}  // namespace hush
