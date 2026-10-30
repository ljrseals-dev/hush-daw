// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/renderer.hpp"
#include "hush/immersive/panner.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace hush {
namespace immersive {

Renderer::Renderer(ChannelFormat format, int sampleRate)
    : format_(format), sampleRate_(sampleRate),
      numChannels_(channelCount(format)),
      levels_(static_cast<std::size_t>(numChannels_)) {}

void Renderer::resetLevels() {
  for (auto& l : levels_) {
    l.peak = 0.f;
    l.rms = 0.f;
  }
}

void Renderer::process(const float* monoIn, int numFrames,
                       const PannerState& state, float* interleavedOut) {
  if (!monoIn || !interleavedOut || numFrames <= 0) return;

  const auto gains = pan(state, format_);
  const int nCh = numChannels_;

  for (int f = 0; f < numFrames; ++f) {
    const float s = monoIn[f];
    for (int c = 0; c < nCh; ++c) {
      const float g = gains[static_cast<std::size_t>(c)];
      const float out = s * g;
      interleavedOut[f * nCh + c] += out;
      LiveLevel& lv = levels_[static_cast<std::size_t>(c)];
      lv.peak = std::max(lv.peak, std::abs(out));
      lv.rms += out * out;
    }
  }

  if (numFrames > 0) {
    const float inv = 1.f / static_cast<float>(numFrames);
    for (auto& lv : levels_) {
      lv.rms = std::sqrt(lv.rms * inv);
    }
  }
}

}  // namespace immersive
}  // namespace hush
