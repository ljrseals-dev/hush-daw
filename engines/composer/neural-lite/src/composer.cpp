// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/composer/lite.hpp"
#include "harmony.hpp"
#include "style_loader.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <unordered_map>

namespace hush::composer {

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

namespace {

std::string styleToString(Style s) {
    switch (s) {
        case Style::Pop:       return "pop";
        case Style::Jazz:      return "jazz";
        case Style::Classical: return "classical";
        case Style::Ambient:   return "ambient";
    }
    return "pop";
}

/// Scale-degree triad as MIDI pitches, placed in a comfortable
/// register around middle C. Root position by default.
std::vector<int> chordMidiPitches(int degree, const Key& key,
                                  bool minorSeventh,
                                  bool firstInversion) {
    const auto pcs = triadPitchClasses(degree, key);
    if (pcs.empty()) return {};

    auto toMidi = [](int pc, int base) {
        return base + ((pc - base % 12) % 12 + 12) % 12;
    };

    const int base = 48;   // C3
    std::vector<int> midis;
    midis.push_back(toMidi(pcs[0], base));
    midis.push_back(toMidi(pcs[1], base));
    midis.push_back(toMidi(pcs[2], base));

    if (minorSeventh) {
        const int seventh = midis[0] + 10;
        if (seventh <= 84) midis.push_back(seventh);
    }

    if (firstInversion && midis.size() >= 3) {
        midis[0] += 12;
        std::sort(midis.begin(), midis.end());
    }

    std::sort(midis.begin(), midis.end());
    for (int& m : midis) {
        while (m < 48) m += 12;
        while (m > 84) m -= 12;
    }
    return midis;
}

notation::Measure chordMeasure(int degree, const Key& key,
                               int ticksPerMeasure,
                               bool minorSeventh,
                               bool firstInversion) {
    notation::Measure m;
    m.ticksPerMeasure = ticksPerMeasure;

    const auto pitches = chordMidiPitches(degree, key,
                                          minorSeventh, firstInversion);
    for (int p : pitches) {
        notation::Note n;
        n.pitch         = p;
        n.durationTicks = ticksPerMeasure;
        n.velocity      = 80;
        n.startTick     = 0;
        m.notes.push_back(n);
    }
    return m;
}

notation::Measure tonicMeasure(const Key& key, int ticksPerMeasure) {
    return chordMeasure(0, key, ticksPerMeasure,
                        /*minorSeventh=*/false,
                        /*firstInversion=*/false);
}

/// Extract pitch classes present in the final measure.
std::vector<int> lastMeasurePitchClasses(const notation::Score& s) {
    if (s.measures.empty()) return {};
    std::vector<int> pcs;
    for (const auto& n : s.measures.back().notes) {
        const int pc = ((n.pitch % 12) + 12) % 12;
        if (std::find(pcs.begin(), pcs.end(), pc) == pcs.end())
            pcs.push_back(pc);
    }
    std::sort(pcs.begin(), pcs.end());
    return pcs;
}

/// Sample a next scale degree from a normalized 7-row.
int sampleDegree(const std::array<float, 7>& weights,
                 float temperature,
                 std::mt19937_64& rng) {
    if (temperature <= 1e-6f) {
        return static_cast<int>(
            std::max_element(weights.begin(), weights.end())
            - weights.begin());
    }

    std::array<double, 7> logits{};
    for (int i = 0; i < 7; ++i) {
        const double w = std::max(1e-9, static_cast<double>(weights[i]));
        logits[i] = std::log(w) / temperature;
    }
    const double maxLogit = *std::max_element(logits.begin(),
                                              logits.end());
    std::array<double, 7> probs{};
    double sum = 0;
    for (int i = 0; i < 7; ++i) {
        probs[i] = std::exp(logits[i] - maxLogit);
        sum += probs[i];
    }
    if (sum < 1e-12) return 0;
    for (double& p : probs) p /= sum;

    std::discrete_distribution<int> dist(probs.begin(), probs.end());
    return dist(rng);
}

std::mt19937_64 makeRng(uint64_t seed) {
    std::mt19937_64 rng;
    if (seed != 0) rng.seed(seed);
    else           rng.seed(std::random_device{}());
    return rng;
}

} // namespace

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct LiteComposer::Impl {
    std::string styleDir;
    mutable std::unordered_map<std::string, LoadedStyle> cache;

    const LoadedStyle& getStyle(const std::string& name) const {
        auto it = cache.find(name);
        if (it != cache.end()) return it->second;
        LoadedStyle s = loadStyle(styleDir, name);
        auto [ins, ok] = cache.emplace(name, std::move(s));
        (void)ok;
        return ins->second;
    }
};

// ---------------------------------------------------------------------------
// LiteComposer
// ---------------------------------------------------------------------------

LiteComposer::LiteComposer(const std::string& styleDir)
    : impl_(std::make_unique<Impl>()) {
    impl_->styleDir = styleDir;
}

LiteComposer::~LiteComposer() = default;

std::string LiteComposer::backend() const { return "lite"; }

// The `::` prefix is required: without it, unqualified lookup inside
// these member functions finds the member, not the free function, and
// the call recurses into itself.
int LiteComposer::detectKey(const notation::Score& score) const {
    return ::hush::composer::detectKey(score).tonic;
}

bool LiteComposer::isMinorKey(const notation::Score& score) const {
    return ::hush::composer::detectKey(score).minor;
}

// ---------------------------------------------------------------------------
// suggest — the core entry point
// ---------------------------------------------------------------------------

std::vector<notation::Score> LiteComposer::suggest(
    const notation::Score& context,
    const std::string& style,
    int maxResults,
    int bars,
    uint64_t seed) const {

    if (maxResults <= 0) return {};
    if (bars <= 0) bars = 1;

    const LoadedStyle& loaded = impl_->getStyle(style);
    const StyleTables& tables = loaded.tables;

    // Empty context defaults to C major so the caller gets something
    // musical rather than an exception.
    const Key key = context.measures.empty()
        ? Key{0, false}
        : ::hush::composer::detectKey(context);

    std::vector<int> prevPcs = lastMeasurePitchClasses(context);
    if (prevPcs.empty()) prevPcs = triadPitchClasses(0, key);

    // Identify the closest diatonic degree to the previous measure.
    int prevDegree = 0;
    {
        double best = 1e9;
        for (int d = 0; d < 7; ++d) {
            const auto pcs = triadPitchClasses(d, key);
            const float cost = voiceLeadingCost(prevPcs, pcs);
            if (cost < best) { best = cost; prevDegree = d; }
        }
    }

    const auto row = normalizedRow(tables, prevDegree);

    struct RawCand {
        int   degree;
        bool  firstInversion;
        bool  minorSeventh;
        float score;
    };
    std::vector<RawCand> raw;
    raw.reserve(28);

    for (int d = 0; d < 7; ++d) {
        const auto pcs = triadPitchClasses(d, key);
        const float cost = voiceLeadingCost(prevPcs, pcs);
        const float normalizedCost = std::min(1.0f, cost / 6.0f);
        const float w = row[d];

        for (bool inv : {false, true}) {
            if (inv && d == 6) continue;   // skip inversion on vii°

            for (bool seventh : {false, true}) {
                if (seventh) {
                    const bool jazzOk = (style == "jazz");
                    const bool dominantOk = (d == 4 || d == 1);
                    if (!jazzOk && !dominantOk) continue;
                }

                float s = w * 0.7f + (1.0f - normalizedCost) * 0.3f;
                if (inv)     s -= 0.02f;
                if (seventh) s += 0.01f;

                raw.push_back(RawCand{d, inv, seventh, s});
            }
        }
    }

    std::sort(raw.begin(), raw.end(),
              [](const RawCand& a, const RawCand& b) {
                  return a.score > b.score;
              });

    auto rng = makeRng(seed);

    const size_t window = std::min<size_t>(
        raw.size(), static_cast<size_t>(maxResults * 2));
    if (seed != 0 && window > 1) {
        std::shuffle(raw.begin(), raw.begin() + window, rng);
    }

    std::vector<notation::Score> out;
    out.reserve(static_cast<size_t>(maxResults));

    const int ticksPerMeasure = context.measures.empty()
        ? (context.ppq * 4)
        : context.measures.front().ticksPerMeasure;

    for (int i = 0; i < maxResults && i < static_cast<int>(raw.size()); ++i) {
        const RawCand& c = raw[static_cast<size_t>(i)];

        notation::Score s;
        s.ppq      = context.ppq;
        s.tempoBpm = context.tempoBpm;

        int  currentDegree = c.degree;
        bool currentInv    = c.firstInversion;
        bool current7th    = c.minorSeventh;

        for (int bar = 0; bar < bars; ++bar) {
            s.measures.push_back(
                chordMeasure(currentDegree, key, ticksPerMeasure,
                             current7th, currentInv));

            if (bar + 1 < bars) {
                const auto nextRow = normalizedRow(tables, currentDegree);
                currentDegree = sampleDegree(nextRow, 0.4f, rng);
                currentInv    = false;
                current7th    = false;
            }
        }
        out.push_back(std::move(s));
    }

    return out;
}

// ---------------------------------------------------------------------------
// continueFrom — prompt + best continuation
// ---------------------------------------------------------------------------

notation::Score LiteComposer::continueFrom(
    const notation::Score& prompt,
    const SampleConfig& sample) const {

    const std::string style =
        sample.style.empty() ? std::string("pop") : sample.style;

    const int bars = std::max(1, sample.bars);
    auto candidates = suggest(prompt, style, 1, bars, sample.seed);

    if (candidates.empty()) {
        // Defensive: suggest() always produces at least one candidate
        // for any input because there are always 7 degrees. If it
        // happens, fall back to a tonic measure rather than throwing.
        notation::Score s = prompt;
        const Key key = prompt.measures.empty()
            ? Key{0, false}
            : ::hush::composer::detectKey(prompt);
        const int tpm = prompt.measures.empty()
            ? prompt.ppq * 4
            : prompt.measures.front().ticksPerMeasure;
        for (int i = 0; i < bars; ++i)
            s.measures.push_back(tonicMeasure(key, tpm));
        return s;
    }

    notation::Score result = prompt;
    for (const auto& m : candidates[0].measures)
        result.measures.push_back(m);
    return result;
}

// ---------------------------------------------------------------------------
// generate — fresh progression from a style
// ---------------------------------------------------------------------------

notation::Score LiteComposer::generate(
    Style style, int bars, const SampleConfig& sample) const {

    if (bars <= 0) bars = 4;

    const std::string styleName = styleToString(style);
    const LoadedStyle& loaded = impl_->getStyle(styleName);
    const StyleTables& tables = loaded.tables;

    const Key key{0, false};

    const int ppq = 480;
    const int ticksPerMeasure = ppq * 4;

    notation::Score s;
    s.ppq      = ppq;
    s.tempoBpm = 120;

    auto rng = makeRng(sample.seed);

    const float temp = std::clamp(sample.temperature, 0.05f, 2.0f);

    int currentDegree = 0;   // start on tonic

    for (int bar = 0; bar < bars; ++bar) {
        const bool finalBar = (bar == bars - 1);
        if (finalBar && bars > 1) currentDegree = 0;

        const bool useSeventh =
            (style == Style::Jazz) &&
            (currentDegree == 4 || currentDegree == 1);

        s.measures.push_back(
            chordMeasure(currentDegree, key, ticksPerMeasure,
                         useSeventh, /*firstInversion=*/false));

        if (!finalBar) {
            const auto row = normalizedRow(tables, currentDegree);
            currentDegree = sampleDegree(row, temp, rng);
        }
    }

    return s;
}

} // namespace hush::composer
