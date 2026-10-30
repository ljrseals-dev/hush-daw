// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "tokenizer.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace hush::composer::neural {

namespace {

// Duration bucket centers, in multiples of a quarter note.
// 0: 1/32  1: 1/16  2: 1/8  3: 1/4  4: 1/2  5: whole
// 6: dotted quarter  7: triplet eighth
constexpr double kDurQuarters[8] = {
    1.0 / 8.0,
    1.0 / 4.0,
    1.0 / 2.0,
    1.0,
    2.0,
    4.0,
    1.5,
    1.0 / 3.0,
};

// Representative velocity for each bucket (p, mp, mf, f).
constexpr int kBucketVel[4] = {40, 70, 95, 120};

int nearestDurationBucket(int ticks, int ppq) {
    if (ppq <= 0) ppq = 480;
    const double quarters = static_cast<double>(ticks)
                          / static_cast<double>(ppq);
    int    best     = 0;
    double bestDist = 1e18;
    for (int i = 0; i < 8; ++i) {
        const double d = std::fabs(quarters - kDurQuarters[i]);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

} // namespace

// ---------------------------------------------------------------------------
// Bucket conversions
// ---------------------------------------------------------------------------

int durationBucket(int durationTicks, int ppq) {
    return nearestDurationBucket(durationTicks, ppq);
}

int bucketToTicks(int bucket, int ppq) {
    if (bucket < 0) bucket = 0;
    if (bucket > 7) bucket = 7;
    if (ppq <= 0) ppq = 480;
    const double quarters = kDurQuarters[bucket];
    const int ticks = static_cast<int>(std::round(quarters * ppq));
    return ticks < 1 ? 1 : ticks;
}

int velocityBucket(int velocity) {
    if (velocity <  1) return 0;
    if (velocity < 56) return 0;   // p
    if (velocity < 84) return 1;   // mp
    if (velocity <108) return 2;   // mf
    return 3;                       // f
}

int bucketToVelocity(int bucket) {
    if (bucket < 0) bucket = 0;
    if (bucket > 3) bucket = 3;
    return kBucketVel[bucket];
}

// ---------------------------------------------------------------------------
// Tokenize
// ---------------------------------------------------------------------------

std::vector<int> tokenize(const notation::Score& score,
                          int maxTokens,
                          int styleByte) {
    std::vector<int> out;
    out.reserve(64);

    out.push_back(kTokBos);
    out.push_back(kTokStyle);
    out.push_back(styleByte & 0x7F);

    const int ppq = score.ppq > 0 ? score.ppq : 480;

    for (const auto& m : score.measures) {
        if (static_cast<int>(out.size()) + 2 >= maxTokens) break;
        out.push_back(kTokBar);

        if (m.notes.empty()) {
            // A whole-measure rest.
            out.push_back(kTokRest);
            out.push_back(kTokDurBase + 5);   // whole
            continue;
        }

        for (const auto& n : m.notes) {
            if (static_cast<int>(out.size()) + 4 >= maxTokens) break;

            const int pitch = std::clamp(n.pitch, 0, 127);
            out.push_back(kTokPitchBase + pitch);
            out.push_back(kTokDurBase + durationBucket(n.durationTicks, ppq));
            out.push_back(kTokVelBase + velocityBucket(n.velocity));
        }
    }

    out.push_back(kTokEos);

    if (maxTokens > 0 && static_cast<int>(out.size()) < maxTokens) {
        out.resize(static_cast<size_t>(maxTokens), kTokPad);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Detokenize
// ---------------------------------------------------------------------------

notation::Score detokenize(const std::vector<int>& tokens) {
    if (tokens.empty())
        throw TokenizerError("empty token stream");
    if (tokens[0] != kTokBos)
        throw TokenizerError("token 0 is not BOS: got "
                             + std::to_string(tokens[0]));

    notation::Score score;
    score.ppq      = 480;
    score.tempoBpm = 120;

    size_t i = 1;

    // Optional STYLE token and its argument.
    if (i + 1 < tokens.size() && tokens[i] == kTokStyle) {
        i += 2;
    }

    // Trim trailing PAD.
    size_t end = tokens.size();
    while (end > i && tokens[end - 1] == kTokPad) --end;

    notation::Measure current;
    current.ticksPerMeasure = score.ppq * 4;
    bool haveMeasure       = false;
    int  cursorInMeasure   = 0;

    auto flushMeasure = [&]() {
        if (haveMeasure) {
            score.measures.push_back(current);
            current = notation::Measure{};
            current.ticksPerMeasure = score.ppq * 4;
            cursorInMeasure = 0;
            haveMeasure     = false;
        }
    };

    for (; i < end; ++i) {
        const int t = tokens[i];

        if (t == kTokEos) break;
        if (t == kTokPad) continue;   // interior pad: ignore

        if (t == kTokBar) {
            flushMeasure();
            haveMeasure = true;
            continue;
        }

        if (t == kTokRest) {
            if (i + 1 >= end)
                throw TokenizerError("REST at index "
                                     + std::to_string(i)
                                     + " has no duration");
            const int durTok = tokens[++i];
            if (durTok < kTokDurBase || durTok >= kTokVelBase)
                throw TokenizerError("bad duration token after REST at "
                                     + std::to_string(i));
            cursorInMeasure +=
                bucketToTicks(durTok - kTokDurBase, score.ppq);
            continue;
        }

        if (t >= kTokPitchBase && t < kTokDurBase) {
            if (i + 2 >= end)
                throw TokenizerError("pitch at index "
                                     + std::to_string(i)
                                     + " missing duration or velocity");
            const int pitch  = t - kTokPitchBase;
            const int durTok = tokens[++i];
            const int velTok = tokens[++i];

            if (durTok < kTokDurBase || durTok >= kTokVelBase)
                throw TokenizerError("bad duration token at index "
                                     + std::to_string(i - 1));
            if (velTok < kTokVelBase || velTok > kTokRest)
                throw TokenizerError("bad velocity token at index "
                                     + std::to_string(i));

            notation::Note n;
            n.pitch         = pitch;
            n.durationTicks = bucketToTicks(durTok - kTokDurBase,
                                            score.ppq);
            n.velocity      = bucketToVelocity(velTok - kTokVelBase);
            n.startTick     = cursorInMeasure;
            current.notes.push_back(n);
            cursorInMeasure += n.durationTicks;
            continue;
        }

        std::ostringstream os;
        os << "unknown token " << t << " at index " << i;
        throw TokenizerError(os.str());
    }

    flushMeasure();

    if (score.measures.empty()) {
        notation::Measure m;
        m.ticksPerMeasure = score.ppq * 4;
        score.measures.push_back(m);
    }

    return score;
}

} // namespace hush::composer::neural
