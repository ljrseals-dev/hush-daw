// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
// Minimal self-contained stub for hush::plugins::PluginHost.
#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <cmath>
#include <cstring>

namespace hush::plugins {

class PluginHost {
public:
    PluginHost() = default;

    // In-process gain plugin only (no real CLAP/VST3 binary loading).
    void setGainDb(float db) {
        gain_ = std::pow(10.f, db / 20.f);
    }
    float gainDb() const noexcept {
        return 20.f * std::log10(std::max(gain_, 1e-8f));
    }

    void process(int64_t /*start*/, int64_t count, float* interleaved, int channels) {
        if (!interleaved || count <= 0 || channels < 1) return;
        const size_t n = static_cast<size_t>(count) * static_cast<size_t>(channels);
        for (size_t i = 0; i < n; ++i)
            interleaved[i] *= gain_;
    }

    // State round-trip (simple float)
    std::vector<uint8_t> saveState() const {
        std::vector<uint8_t> buf(sizeof(float));
        std::memcpy(buf.data(), &gain_, sizeof(float));
        return buf;
    }
    void restoreState(const std::vector<uint8_t>& buf) {
        if (buf.size() != sizeof(float))
            throw std::runtime_error("PluginHost::restoreState: bad size");
        std::memcpy(&gain_, buf.data(), sizeof(float));
    }

private:
    float gain_ = 1.f;
};

} // namespace hush::plugins
