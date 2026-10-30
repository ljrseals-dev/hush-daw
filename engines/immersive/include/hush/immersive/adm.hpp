// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include "channel_format.hpp"
#include "panner.hpp"
#include <string>
#include <vector>

namespace hush {
namespace immersive {

struct AdmObject {
  std::string name;
  PannerState panner;
  int firstChannel{0};
  int numChannels{1};
};

void exportAdm(const std::string& path,
               const std::vector<float>& interleavedSamples,
               int numChannels, int numFrames, int sampleRate,
               ChannelFormat bedFormat,
               const std::vector<AdmObject>& objects);

}  // namespace immersive
}  // namespace hush
