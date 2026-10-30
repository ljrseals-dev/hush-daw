// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
// Minimal self-contained stub for hush::immersive::Renderer.
// Replaced automatically when the real Tier-1/2 engine is linked.
#pragma once

#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>

namespace hush::immersive {

struct Object {
    float x = 0.f, y = 0.f, z = 0.f;   // Cartesian metres
    float gain = 1.f;
    std::string name;
};

class Renderer {
public:
    Renderer() = default;

    void setChannelCount(int ch) {
        if (ch < 1) throw std::invalid_argument("Renderer: channels < 1");
        channels_ = ch;
    }
    int channelCount() const noexcept { return channels_; }

    void addObject(const Object& obj) { objects_.push_back(obj); }
    void clearObjects() { objects_.clear(); }
    size_t objectCount() const noexcept { return objects_.size(); }

    // Simple equal-power pan into interleaved buffer.
    // start/count are in frames; out must already be sized.
    void render(int64_t /*start*/, int64_t count, float* outInterleaved) {
        if (!outInterleaved || count <= 0) return;
        const size_t samples = static_cast<size_t>(count) * static_cast<size_t>(channels_);
        std::fill(outInterleaved, outInterleaved + samples, 0.f);

        for (const auto& obj : objects_) {
            // Very simple left/right balance from x (-1..+1)
            float pan = std::clamp(obj.x, -1.f, 1.f);
            float l = std::sqrt((1.f - pan) * 0.5f) * obj.gain;
            float r = std::sqrt((1.f + pan) * 0.5f) * obj.gain;

            for (int64_t i = 0; i < count; ++i) {
                // Soft test tone so render is audibly non-silent
                float t = static_cast<float>(i) / 48000.f;
                float sample = 0.1f * std::sin(2.f * 3.14159265f * 440.f * t);
                if (channels_ >= 1) outInterleaved[i * channels_ + 0] += sample * l;
                if (channels_ >= 2) outInterleaved[i * channels_ + 1] += sample * r;
            }
        }
    }

private:
    int channels_ = 2;
    std::vector<Object> objects_;
};

} // namespace hush::immersive
