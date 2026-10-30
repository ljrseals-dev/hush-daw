// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace notation {

struct Note {
    int pitch = 60;          // MIDI 0-127
    int durationTicks = 480; // 480 = quarter at default PPQ
    int velocity = 80;       // 0-127
    int startTick = 0;
};

struct Measure {
    std::vector<Note> notes;
    int ticksPerMeasure = 1920; // 4/4 at 480 PPQ
};

struct Score {
    std::vector<Measure> measures;
    int ppq = 480;
    int tempoBpm = 120;
    std::string title;

    bool operator==(const Score& o) const {
        if (measures.size() != o.measures.size()) return false;
        for (size_t i = 0; i < measures.size(); ++i) {
            if (measures[i].notes.size() != o.measures[i].notes.size()) return false;
            for (size_t j = 0; j < measures[i].notes.size(); ++j) {
                const auto& a = measures[i].notes[j];
                const auto& b = o.measures[i].notes[j];
                if (a.pitch != b.pitch || a.durationTicks != b.durationTicks ||
                    a.velocity != b.velocity || a.startTick != b.startTick)
                    return false;
            }
        }
        return true;
    }
};

} // namespace notation
