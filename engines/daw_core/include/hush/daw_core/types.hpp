// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>

namespace hush::daw_core {

using Sample     = float;
using FrameIndex = int64_t;
using SampleRate = double;

struct TimePoint {
    FrameIndex frames = 0;
    SampleRate rate   = 48000.0;

    double seconds() const noexcept {
        return rate > 0.0 ? static_cast<double>(frames) / rate : 0.0;
    }
};

struct AudioBuffer {
    std::vector<Sample> data;          // interleaved
    int                 channels = 0;
    SampleRate          rate     = 48000.0;
    FrameIndex          frames   = 0;

    size_t sampleCount() const noexcept { return data.size(); }

    void resize(int ch, FrameIndex fr) {
        if (ch < 0 || fr < 0)
            throw std::invalid_argument("AudioBuffer::resize: negative size");
        channels = ch;
        frames   = fr;
        data.assign(static_cast<size_t>(ch) * static_cast<size_t>(fr), Sample{0});
    }

    void clear() {
        std::fill(data.begin(), data.end(), Sample{0});
    }
};

struct ImmersiveSceneHandle { std::string id; };
struct ScoreHandle          { std::string id; };
struct PluginChainHandle    { std::string id; };
struct VideoTimelineHandle  { std::string id; };

} // namespace hush::daw_core
