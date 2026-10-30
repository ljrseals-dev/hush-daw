// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include "channel_format.hpp"
#include <array>
#include <vector>

namespace hush {
namespace immersive {

// Encode a mono source at (azimuthDeg, elevationDeg) into ACN/SN3D Ambisonics.
// Divergence and size are ignored (loudspeaker-layout concepts only).
std::vector<float> encodeAmbisonics(float azimuthDeg, float elevationDeg,
                                    ChannelFormat format);

// HOA decoding: convert ACN/SN3D Ambisonics buffer to a loudspeaker layout
// using mode-matching (pseudo-inverse of the encoding matrix).
std::vector<float> decodeAmbisonics(const float* hoaInterleaved, int numFrames,
                                    ChannelFormat hoaFormat,
                                    ChannelFormat speakerFormat);

// 3x3 rotation matrix for yaw/pitch/roll (radians). Applied to HOA coefficients
// via the appropriate order-dependent rotation matrices (order 1 = 3x3, higher
// orders use recursive construction).
struct HoaRotation {
  float yaw{0.f};    // radians, positive = left
  float pitch{0.f};  // radians, positive = up
  float roll{0.f};   // radians, positive = clockwise looking forward
};

// Rotate an interleaved HOA buffer in-place (ACN order, any order up to 3).
void rotateHoa(float* hoaInterleaved, int numChannels, int numFrames,
               const HoaRotation& rot);

}  // namespace immersive
}  // namespace hush
