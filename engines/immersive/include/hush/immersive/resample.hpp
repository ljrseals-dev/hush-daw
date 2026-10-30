// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include <vector>

namespace hush {
namespace immersive {

enum class ResampleQuality {
  Linear,   // fast, adequate for most non-critical paths
  Cubic     // 4-point Hermite, better imaging
};

// Resample interleaved audio from srcRate to dstRate.
// numChannels must match the interleaving of both buffers.
// Returns the number of output frames written (may be less than expected
// near the end due to filter delay; the implementation zero-pads).
int resample(const float* srcInterleaved, int srcFrames, int srcRate,
             float* dstInterleaved, int dstFrames, int dstRate,
             int numChannels, ResampleQuality quality = ResampleQuality::Cubic);

// Convenience: allocate and return a new buffer at the destination rate.
std::vector<float> resampleTo(const std::vector<float>& srcInterleaved,
                              int srcFrames, int srcRate, int dstRate,
                              int numChannels,
                              ResampleQuality quality = ResampleQuality::Cubic);

}  // namespace immersive
}  // namespace hush
