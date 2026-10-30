// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include "channel_format.hpp"
#include "panner.hpp"
#include <string>
#include <vector>

namespace hush {
namespace immersive {

struct LiveLevel {
  float peak{0.f};
  float rms{0.f};
};

class Renderer {
public:
  explicit Renderer(ChannelFormat format, int sampleRate = 48000);

  ChannelFormat format() const { return format_; }
  int sampleRate() const { return sampleRate_; }
  int numChannels() const { return numChannels_; }

  // Process a mono source with the given panner state into the output buffer
  // (interleaved, numChannels * numFrames). Gains are applied per sample.
  void process(const float* monoIn, int numFrames, const PannerState& state,
               float* interleavedOut);

  // Clear internal level meters
  void resetLevels();
  const std::vector<LiveLevel>& levels() const { return levels_; }

private:
  ChannelFormat format_;
  int sampleRate_;
  int numChannels_;
  std::vector<LiveLevel> levels_;
};

}  // namespace immersive
}  // namespace hush
