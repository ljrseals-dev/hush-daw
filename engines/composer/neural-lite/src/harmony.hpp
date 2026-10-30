// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once
#include "hush/notation/score.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace hush::composer {

/// Detected key: pitch class 0..11 (C..B) and whether minor.
struct Key {
    int  tonic = 0;
    bool minor = false;

    std::string name() const;
};

/// A chord label at one measure boundary.
struct ChordLabel {
    int         degree      = -1;    // 0..6; -1 = unknown
    bool        inversion   = false;
    bool        minorThird  = false;
    bool        diminished  = false;
    std::string text;                // "I", "ii", "V", "?", ...
};

/// Per-measure analysis result.
struct MeasureAnalysis {
    ChordLabel chord;
    bool       isCadence = false;
};

struct StyleTables {
    // Row = from degree 0..6, col = to degree 0..6.
    // Unnormalized weights; normalize at query time.
    std::array<std::array<float, 7>, 7> w{};
};

/// Krumhansl-Schmuckler key detection with Krumhansl-Kessler profiles.
Key detectKey(const notation::Score& score);

/// Roman-numeral label each measure against the given key.
std::vector<MeasureAnalysis> analyzeMeasures(const notation::Score& score,
                                             const Key& key,
                                             bool allowInversions = true);

/// Symmetric voice-leading cost between two pitch-class sets.
float voiceLeadingCost(const std::vector<int>& pcA,
                       const std::vector<int>& pcB);

/// The built-in style table for a name. Unknown names map to pop.
const StyleTables& styleTable(const std::string& style);

/// Normalize one row of a table so it sums to 1.
std::array<float, 7> normalizedRow(const StyleTables& t, int fromDegree);

/// Roman numeral text for a degree in a key.
std::string romanForDegree(int degree, bool minorKey, bool diminished);

/// Pitch-class set of a diatonic triad in a key.
std::vector<int> triadPitchClasses(int degree, const Key& key);

} // namespace hush::composer
