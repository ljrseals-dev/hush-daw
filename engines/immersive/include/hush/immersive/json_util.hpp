// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include "panner.hpp"
#include "channel_format.hpp"
#include <string>
#include <vector>

namespace hush {
namespace immersive {

// Minimal hand-rolled JSON helpers (no external dependency).

struct JsonSceneObject {
  std::string name;
  std::string path;
  PannerState panner;
};

struct JsonScene {
  ChannelFormat format{ChannelFormat::SevenPointOne};
  std::string bedPath;
  float bedGainDb{0.f};
  std::vector<JsonSceneObject> objects;
};

// Parse a scene JSON file. Throws std::runtime_error on malformed input.
JsonScene parseSceneJson(const std::string& path);

// Write levels to a JSON file.
void writeLevelsJson(const std::string& path,
                     const std::vector<float>& peaks,
                     const std::vector<float>& rms);

// Serialise a PannerState to a compact JSON object string.
std::string pannerToJson(const PannerState& p);

}  // namespace immersive
}  // namespace hush
