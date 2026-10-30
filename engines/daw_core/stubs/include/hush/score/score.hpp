// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
// Minimal self-contained stub for hush::score::Score.
#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <cmath>

namespace hush::score {

struct Note {
    int      pitch    = 60;   // MIDI note number
    int64_t  start    = 0;    // frames
    int64_t  duration = 0;    // frames
    float    velocity = 0.8f;
};

class Score {
public:
    Score() = default;

    void addNote(const Note& n) {
        if (n.duration < 0) throw std::invalid_argument("Score::addNote: negative duration");
        notes_.push_back(n);
    }
    void clear() { notes_.clear(); }
    size_t noteCount() const noexcept { return notes_.size(); }
    const std::vector<Note>& notes() const noexcept { return notes_; }

    // Simple square-wave render of the notes into interleaved stereo.
    void render(int64_t start, int64_t count, float* outInterleaved, int channels) {
        if (!outInterleaved || count <= 0 || channels < 1) return;
        const size_t samples = static_cast<size_t>(count) * static_cast<size_t>(channels);
        std::fill(outInterleaved, outInterleaved + samples, 0.f);

        for (const auto& n : notes_) {
            int64_t nStart = n.start;
            int64_t nEnd   = n.start + n.duration;
            if (nEnd <= start || nStart >= start + count) continue;

            float freq = 440.f * std::pow(2.f, (n.pitch - 69) / 12.f);
            float amp  = n.velocity * 0.15f;

            for (int64_t i = 0; i < count; ++i) {
                int64_t f = start + i;
                if (f < nStart || f >= nEnd) continue;
                float t = static_cast<float>(f) / 48000.f;
                float s = amp * ((std::fmod(freq * t, 1.f) < 0.5f) ? 1.f : -1.f);
                for (int c = 0; c < channels; ++c)
                    outInterleaved[i * channels + c] += s;
            }
        }
    }

private:
    std::vector<Note> notes_;
};

} // namespace hush::score
