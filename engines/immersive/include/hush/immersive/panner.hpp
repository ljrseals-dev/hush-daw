// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include "channel_format.hpp"
#include "speakers.hpp"
#include <vector>

namespace hush {
namespace immersive {

struct PannerState {
  float azimuthDeg{0.f};     // 0 = front, + = left
  float elevationDeg{0.f};
  float divergence{0.f};     // 0..1
  float size{0.f};           // 0..1
  float gain{1.f};           // linear
  bool isObject{false};
  float distance{0.f};       // metres; 0 = on unit sphere
  float screenAnchor{0.f};   // 0 = free, 1 = pinned to screen
};

// Compute per-speaker gains for a loudspeaker layout (VBAP cosine law).
// For Ambisonics formats the caller must use the Ambisonics encoder instead.
std::vector<float> pan(const PannerState& state, ChannelFormat format);

// Legacy overload kept for source compatibility; delegates to the full state.
std::vector<float> pan(float azimuthDeg, float elevationDeg, float divergence,
                       float size, float gain, ChannelFormat format,
                       bool isObject = false);

}  // namespace immersive
}  // namespace hush
