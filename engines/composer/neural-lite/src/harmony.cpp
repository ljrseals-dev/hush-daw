// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "harmony.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace hush::composer {

// ---------------------------------------------------------------------------
// Key
// ---------------------------------------------------------------------------

std::string Key::name() const {
    static const char* names[] = {
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
    };
    std::string s = names[((tonic % 12) + 12) % 12];
    s += minor ? " minor" : " major";
    return s;
}

// ---------------------------------------------------------------------------
// Krumhansl-Kessler key detection
// ---------------------------------------------------------------------------
//
// Profiles from Krumhansl (1990), "Cognitive Foundations of Musical
// Pitch". Values are the canonical published figures. Do not round;
// the correlation is sensitive enough that rounding changes which key
// wins in borderline cases.

namespace {

constexpr std::array<double, 12> kKkMajor = {
    6.35, 2.23, 3.48, 2.33, 4.38, 4.09,
    2.52, 5.19, 2.39, 3.66, 2.29, 2.88
};
constexpr std::array<double, 12> kKkMinor = {
    6.33, 2.68, 3.52, 5.38, 2.60, 3.53,
    2.54, 4.75, 3.98, 2.69, 3.34, 3.17
};

double pearson(const std::array<double, 12>& a,
               const std::array<double, 12>& b) {
    double ma = 0, mb = 0;
    for (int i = 0; i < 12; ++i) { ma += a[i]; mb += b[i]; }
    ma /= 12.0; mb /= 12.0;
    double num = 0, da = 0, db = 0;
    for (int i = 0; i < 12; ++i) {
        const double xa = a[i] - ma;
        const double xb = b[i] - mb;
        num += xa * xb;
        da  += xa * xa;
        db  += xb * xb;
    }
    if (da < 1e-12 || db < 1e-12) return 0.0;
    return num / std::sqrt(da * db);
}

std::array<double, 12> rotate(const std::array<double, 12>& p, int by) {
    std::array<double, 12> out{};
    for (int i = 0; i < 12; ++i)
        out[i] = p[((i - by) % 12 + 12) % 12];
    return out;
}

} // namespace

Key detectKey(const notation::Score& score) {
    std::array<double, 12> histogram{};
    for (const auto& m : score.measures) {
        for (const auto& n : m.notes) {
            if (n.pitch < 0 || n.pitch > 127) continue;
            histogram[n.pitch % 12] += static_cast<double>(n.durationTicks);
        }
    }

    double total = 0;
    for (double v : histogram) total += v;
    if (total < 1e-9) return Key{0, false};

    double bestScore = -2.0;
    Key    best{0, false};

    for (int tonic = 0; tonic < 12; ++tonic) {
        const auto majProfile = rotate(kKkMajor, tonic);
        const auto minProfile = rotate(kKkMinor, tonic);
        const double cmaj = pearson(histogram, majProfile);
        const double cmin = pearson(histogram, minProfile);
        if (cmaj > bestScore) { bestScore = cmaj; best = Key{tonic, false}; }
        if (cmin > bestScore) { bestScore = cmin; best = Key{tonic, true}; }
    }

    // Tie-break: prefer the key whose tonic appears in the final
    // measure. This resolves the relative major/minor ambiguity in
    // favor of whichever key actually cadences.
    if (!score.measures.empty()) {
        const auto& last = score.measures.back();
        bool tonicPresent = false;
        for (const auto& n : last.notes) {
            if (n.pitch % 12 == best.tonic) { tonicPresent = true; break; }
        }

        if (!tonicPresent) {
            for (int tonic = 0; tonic < 12; ++tonic) {
                for (bool minor : {false, true}) {
                    const auto profile = rotate(
                        minor ? kKkMinor : kKkMajor, tonic);
                    const double c = pearson(histogram, profile);
                    if (std::abs(c - bestScore) < 1e-9) {
                        for (const auto& n : last.notes) {
                            if (n.pitch % 12 == tonic) {
                                best = Key{tonic, minor};
                                return best;
                            }
                        }
                    }
                }
            }
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Roman numerals and scale degrees
// ---------------------------------------------------------------------------

std::string romanForDegree(int degree, bool minorKey, bool diminished) {
    if (degree < 0 || degree > 6) return "?";
    static const char* major[] = {"I","ii","iii","IV","V","vi","vii"};
    static const char* minor[] = {"i","ii","III","iv","v","VI","VII"};
    std::string s = minorKey ? minor[degree] : major[degree];
    if (diminished && !s.empty() && s.back() != 'o') s += "o";
    return s;
}

namespace {
constexpr int kMajorScale[7] = {0, 2, 4, 5, 7, 9, 11};
constexpr int kMinorScale[7] = {0, 2, 3, 5, 7, 8, 10};
} // namespace

std::vector<int> triadPitchClasses(int degree, const Key& key) {
    if (degree < 0 || degree > 6) return {};
    const int* scale = key.minor ? kMinorScale : kMajorScale;
    const int root  = (key.tonic + scale[degree])           % 12;
    const int third = (key.tonic + scale[(degree + 2) % 7]) % 12;
    const int fifth = (key.tonic + scale[(degree + 4) % 7]) % 12;
    return { ((root  % 12) + 12) % 12,
             ((third % 12) + 12) % 12,
             ((fifth % 12) + 12) % 12 };
}

// ---------------------------------------------------------------------------
// Chord labeling
// ---------------------------------------------------------------------------

namespace {

std::array<double, 12> measureHistogram(const notation::Measure& m) {
    std::array<double, 12> h{};
    for (const auto& n : m.notes) {
        if (n.pitch < 0 || n.pitch > 127) continue;
        h[n.pitch % 12] += static_cast<double>(n.durationTicks);
    }
    return h;
}

double distanceToTriad(const std::array<double, 12>& h,
                       const std::vector<int>& triad) {
    double total = 0;
    for (double v : h) total += v;
    if (total < 1e-9) return 1e9;

    std::array<double, 12> ideal{};
    for (int pc : triad) ideal[pc] = 1.0;
    double idealTotal = 0;
    for (double v : ideal) idealTotal += v;

    double d = 0;
    for (int i = 0; i < 12; ++i)
        d += std::abs(h[i] / total - ideal[i] / idealTotal);
    return d;
}

} // namespace

std::vector<MeasureAnalysis> analyzeMeasures(const notation::Score& score,
                                             const Key& key,
                                             bool allowInversions) {
    std::vector<MeasureAnalysis> out;
    out.reserve(score.measures.size());

    for (const auto& m : score.measures) {
        MeasureAnalysis ma;
        const auto h = measureHistogram(m);

        double bestDist = 1e9;
        int    bestDeg  = -1;
        bool   bestInv  = false;

        for (int deg = 0; deg < 7; ++deg) {
            auto triad = triadPitchClasses(deg, key);
            const double d0 = distanceToTriad(h, triad);
            if (d0 < bestDist) {
                bestDist = d0;
                bestDeg  = deg;
                bestInv  = false;
            }
            if (allowInversions && triad.size() == 3) {
                std::rotate(triad.begin(), triad.begin() + 1, triad.end());
                const double d1 = distanceToTriad(h, triad);
                if (d1 < bestDist) {
                    bestDist = d1;
                    bestDeg  = deg;
                    bestInv  = true;
                }
            }
        }

        constexpr double kLabelThreshold = 0.55;
        if (bestDist > kLabelThreshold || bestDeg < 0) {
            ma.chord.degree = -1;
            ma.chord.text   = "?";
        } else {
            ma.chord.degree      = bestDeg;
            ma.chord.inversion   = bestInv;
            ma.chord.diminished  = key.minor ? (bestDeg == 1)
                                             : (bestDeg == 6);
            ma.chord.minorThird  = key.minor
                ? (bestDeg == 0 || bestDeg == 3 || bestDeg == 4)
                : (bestDeg == 1 || bestDeg == 2 || bestDeg == 5);
            ma.chord.text = romanForDegree(bestDeg, key.minor,
                                           ma.chord.diminished);
        }
        out.push_back(ma);
    }

    for (size_t i = 1; i < out.size(); ++i) {
        const int prev = out[i - 1].chord.degree;
        const int cur  = out[i].chord.degree;
        if (prev < 0 || cur < 0) continue;
        const bool vToI  = (prev == 4 && cur == 0);
        const bool ivToI = (prev == 3 && cur == 0);
        const bool vToVi = (prev == 4 && cur == 5);
        if (vToI || ivToI || vToVi) out[i].isCadence = true;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Voice-leading cost
// ---------------------------------------------------------------------------

float voiceLeadingCost(const std::vector<int>& pcA,
                       const std::vector<int>& pcB) {
    if (pcA.empty() || pcB.empty()) return 0.0f;

    auto nearestSum = [](const std::vector<int>& from,
                         const std::vector<int>& to) {
        double s = 0;
        for (int a : from) {
            int best = 127;
            for (int b : to) {
                const int raw     = std::abs(a - b);
                const int wrapped = std::min(raw, 12 - raw);
                best = std::min(best, wrapped);
            }
            s += best;
        }
        return s / static_cast<double>(from.size());
    };

    const double fwd = nearestSum(pcA, pcB);
    const double rev = nearestSum(pcB, pcA);
    return static_cast<float>((fwd + rev) * 0.5);
}

// ---------------------------------------------------------------------------
// Style transition tables
// ---------------------------------------------------------------------------
//
// 7x7 matrices over scale degrees (0 = I, 6 = vii). Values are
// unnormalized weights. Higher = more likely. The shape of each row
// is the musical content of this file; the exact numbers matter less
// than the relative weightings, which encode functional-harmony
// practice.

namespace {

const StyleTables kPop = StyleTables{
    {{
        //          I    ii   iii  IV   V    vi   vii
        { 0.05f,0.05f,0.05f,0.35f,0.30f,0.20f,0.00f },
        { 0.10f,0.05f,0.05f,0.20f,0.55f,0.05f,0.00f },
        { 0.10f,0.10f,0.05f,0.25f,0.35f,0.15f,0.00f },
        { 0.40f,0.05f,0.05f,0.05f,0.35f,0.10f,0.00f },
        { 0.55f,0.05f,0.05f,0.10f,0.05f,0.20f,0.00f },
        { 0.15f,0.15f,0.05f,0.30f,0.25f,0.05f,0.05f },
        { 0.50f,0.05f,0.05f,0.10f,0.20f,0.10f,0.00f },
    }}
};

const StyleTables kJazz = StyleTables{
    {{
        //          I    ii   iii  IV   V    vi   vii
        { 0.05f,0.30f,0.10f,0.15f,0.20f,0.15f,0.05f },
        { 0.05f,0.05f,0.05f,0.10f,0.60f,0.10f,0.05f },
        { 0.10f,0.30f,0.05f,0.10f,0.30f,0.10f,0.05f },
        { 0.15f,0.30f,0.05f,0.05f,0.30f,0.10f,0.05f },
        { 0.45f,0.15f,0.05f,0.10f,0.05f,0.15f,0.05f },
        { 0.05f,0.40f,0.05f,0.10f,0.25f,0.05f,0.10f },
        { 0.40f,0.10f,0.05f,0.10f,0.20f,0.10f,0.05f },
    }}
};

const StyleTables kClassical = StyleTables{
    {{
        //          I    ii   iii  IV   V    vi   vii
        { 0.05f,0.20f,0.05f,0.25f,0.25f,0.10f,0.10f },
        { 0.05f,0.05f,0.05f,0.10f,0.65f,0.05f,0.05f },
        { 0.15f,0.20f,0.05f,0.20f,0.25f,0.10f,0.05f },
        { 0.15f,0.15f,0.05f,0.05f,0.45f,0.10f,0.05f },
        { 0.60f,0.05f,0.05f,0.05f,0.05f,0.15f,0.05f },
        { 0.05f,0.30f,0.05f,0.15f,0.30f,0.05f,0.10f },
        { 0.70f,0.05f,0.05f,0.05f,0.10f,0.05f,0.00f },
    }}
};

const StyleTables kAmbient = StyleTables{
    {{
        //          I    ii   iii  IV   V    vi   vii
        { 0.30f,0.05f,0.20f,0.20f,0.05f,0.20f,0.00f },
        { 0.15f,0.25f,0.10f,0.25f,0.15f,0.10f,0.00f },
        { 0.15f,0.10f,0.30f,0.20f,0.10f,0.15f,0.00f },
        { 0.20f,0.10f,0.15f,0.30f,0.10f,0.15f,0.00f },
        { 0.25f,0.10f,0.15f,0.20f,0.20f,0.10f,0.00f },
        { 0.15f,0.10f,0.20f,0.20f,0.05f,0.30f,0.00f },
        { 0.20f,0.10f,0.15f,0.20f,0.15f,0.20f,0.00f },
    }}
};

} // namespace

const StyleTables& styleTable(const std::string& style) {
    if (style == "jazz")      return kJazz;
    if (style == "classical") return kClassical;
    if (style == "ambient")   return kAmbient;
    return kPop;
}

std::array<float, 7> normalizedRow(const StyleTables& t, int fromDegree) {
    std::array<float, 7> row{};
    if (fromDegree < 0 || fromDegree > 6) {
        row.fill(1.0f / 7.0f);
        return row;
    }
    float sum = 0;
    for (int j = 0; j < 7; ++j) {
        row[j] = t.w[fromDegree][j];
        sum += row[j];
    }
    if (sum < 1e-9f) {
        row.fill(1.0f / 7.0f);
        return row;
    }
    for (float& v : row) v /= sum;
    return row;
}

} // namespace hush::composer
