// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/resample.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace hush {
namespace immersive {

namespace {

float hermite(float y0, float y1, float y2, float y3, float t) {
  const float c0 = y1;
  const float c1 = 0.5f * (y2 - y0);
  const float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
  const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
  return ((c3 * t + c2) * t + c1) * t + c0;
}

}  // namespace

int resample(const float* srcInterleaved, int srcFrames, int srcRate,
             float* dstInterleaved, int dstFrames, int dstRate,
             int numChannels, ResampleQuality quality) {
  if (!srcInterleaved || !dstInterleaved || srcFrames <= 0 || dstFrames <= 0 ||
      srcRate <= 0 || dstRate <= 0 || numChannels <= 0) {
    throw std::invalid_argument("resample: invalid arguments");
  }

  if (srcRate == dstRate) {
    const int n = std::min(srcFrames, dstFrames);
    const std::size_t bytes =
        static_cast<std::size_t>(n) * numChannels * sizeof(float);
    std::memcpy(dstInterleaved, srcInterleaved, bytes);
    if (dstFrames > n) {
      std::memset(dstInterleaved + n * numChannels, 0,
                  static_cast<std::size_t>(dstFrames - n) * numChannels *
                      sizeof(float));
    }
    return n;
  }

  const double ratio = static_cast<double>(srcRate) / static_cast<double>(dstRate);

  for (int df = 0; df < dstFrames; ++df) {
    const double srcPos = df * ratio;
    const int i1 = static_cast<int>(srcPos);
    const float t = static_cast<float>(srcPos - i1);

    for (int ch = 0; ch < numChannels; ++ch) {
      float sample = 0.f;
      if (quality == ResampleQuality::Linear) {
        const int i0 = std::max(0, i1);
        const int i2 = std::min(srcFrames - 1, i1 + 1);
        const float s0 = srcInterleaved[i0 * numChannels + ch];
        const float s1 = srcInterleaved[i2 * numChannels + ch];
        sample = s0 + t * (s1 - s0);
      } else {
        // Cubic Hermite
        const int i0 = std::max(0, i1 - 1);
        const int i2 = std::min(srcFrames - 1, i1);
        const int i3 = std::min(srcFrames - 1, i1 + 1);
        const int i4 = std::min(srcFrames - 1, i1 + 2);
        const float y0 = srcInterleaved[i0 * numChannels + ch];
        const float y1 = srcInterleaved[i2 * numChannels + ch];
        const float y2 = srcInterleaved[i3 * numChannels + ch];
        const float y3 = srcInterleaved[i4 * numChannels + ch];
        sample = hermite(y0, y1, y2, y3, t);
      }
      dstInterleaved[df * numChannels + ch] = sample;
    }
  }
  return dstFrames;
}

std::vector<float> resampleTo(const std::vector<float>& srcInterleaved,
                              int srcFrames, int srcRate, int dstRate,
                              int numChannels, ResampleQuality quality) {
  if (srcRate <= 0 || dstRate <= 0) {
    throw std::invalid_argument("resampleTo: invalid sample rate");
  }
  const int dstFrames = static_cast<int>(
      std::ceil(static_cast<double>(srcFrames) * dstRate / srcRate));
  std::vector<float> dst(static_cast<std::size_t>(dstFrames) * numChannels, 0.f);
  resample(srcInterleaved.data(), srcFrames, srcRate, dst.data(), dstFrames,
           dstRate, numChannels, quality);
  return dst;
}

}  // namespace immersive
}  // namespace hush
