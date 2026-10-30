// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include "channel_format.hpp"
#include <string>
#include <vector>

namespace hush {
namespace immersive {

struct Speaker {
  std::string id;
  float azimuthDeg{0.f};   // 0 = front, +az = left
  float elevationDeg{0.f};
  float x{0.f};
  float y{0.f};
  float z{0.f};
  bool isLfe{false};
  bool isAmbisonic{false};
};

int channelCount(ChannelFormat format);
const std::vector<Speaker>& speakersFor(ChannelFormat format);
Speaker speakerFor(ChannelFormat format, int index);

}  // namespace immersive
}  // namespace hush
