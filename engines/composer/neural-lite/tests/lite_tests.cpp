// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/composer/lite.hpp"
#include "harmony.hpp"
#include "smf.hpp"
#include "style_loader.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace hush::composer;
using notation::Score;
using notation::Measure;
using notation::Note;

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------

static int g_fail = 0;
static int g_pass = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAIL  " << __FILE__ << ":" << __LINE__ \
                  << "  " << #cond << "\n"; \
        ++g_fail; \
    } else { ++g_pass; } \
} while (0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Score makeTriad(int rootMidi, int thirdInterval, int fifthInterval) {
    Score s;
    s.ppq      = 480;
    s.tempoBpm = 120;
    Measure m;
    m.ticksPerMeasure = 1920;
    for (int offs : {0, thirdInterval, fifthInterval}) {
        Note n;
        n.pitch         = rootMidi + offs;
        n.durationTicks = 1920;
        n.velocity      = 80;
        n.startTick     = 0;
        m.notes.push_back(n);
    }
    s.measures.push_back(m);
    return s;
}

static Score makeEmpty() {
    Score s;
    s.ppq      = 480;
    s.tempoBpm = 120;
    Measure m;
    m.ticksPerMeasure = 1920;
    s.measures.push_back(m);
    return s;
}

// ---------------------------------------------------------------------------
// L1 — empty context still produces at least one candidate
// ---------------------------------------------------------------------------

static void test_L1_empty_context() {
    LiteComposer c;
    auto cands = c.suggest(makeEmpty(), "pop", 4, 1, 42);
    CHECK(cands.size() >= 1u);
}

// ---------------------------------------------------------------------------
// L2 — single-note context does not crash
// ---------------------------------------------------------------------------

static void test_L2_single_note() {
    Score s;
    s.ppq = 480;
    Measure m;
    m.ticksPerMeasure = 1920;
    Note n;
    n.pitch         = 60;
    n.durationTicks = 480;
    n.velocity      = 80;
    n.startTick     = 0;
    m.notes.push_back(n);
    s.measures.push_back(m);

    LiteComposer c;
    auto cands = c.suggest(s, "pop", 2, 1, 1);
    CHECK(cands.size() >= 1u);
}

// ---------------------------------------------------------------------------
// L3 — determinism: same context + same seed -> identical Score and bytes
// ---------------------------------------------------------------------------

static void test_L3_determinism() {
    Score ctx = makeTriad(60, 4, 7);   // C major
    LiteComposer c;

    auto a = c.suggest(ctx, "pop", 1, 2, 42);
    auto b = c.suggest(ctx, "pop", 1, 2, 42);

    CHECK(a.size() == 1u);
    CHECK(b.size() == 1u);
    if (a.size() == 1u && b.size() == 1u) {
        CHECK(a[0] == b[0]);

        auto bytesA = serialiseMidi(a[0]);
        auto bytesB = serialiseMidi(b[0]);
        CHECK(bytesA == bytesB);
    }
}

// ---------------------------------------------------------------------------
// L4 — every built-in style produces at least one candidate
// ---------------------------------------------------------------------------

static void test_L4_all_styles() {
    LiteComposer c;
    Score ctx = makeTriad(60, 4, 7);
    for (const char* style : {"pop", "jazz", "classical", "ambient"}) {
        auto cands = c.suggest(ctx, style, 1, 1, 7);
        CHECK(cands.size() >= 1u);
    }
}

// ---------------------------------------------------------------------------
// L5 — malformed style dir falls back to the built-in tables
// ---------------------------------------------------------------------------

static void test_L5_malformed_style_dir() {
    // Direct API check: a nonexistent directory is a soft failure.
    auto loaded = loadStyle("/this/path/does/not/exist/hush-styles", "jazz");
    CHECK(!loaded.fromFile);
    CHECK(!loaded.fallbackReason.empty());

    // Behavior check: a composer pointed at a missing dir must
    // produce the same output as a composer with no dir at all.
    // This proves the fallback is transparent, not just non-crashing.
    LiteComposer broken("/this/path/does/not/exist/hush-styles");
    LiteComposer clean;

    Score ctx = makeTriad(60, 4, 7);
    auto a = broken.suggest(ctx, "pop", 1, 1, 42);
    auto b = clean.suggest(ctx, "pop", 1, 1, 42);

    CHECK(a.size() == b.size());
    if (a.size() == b.size() && !a.empty()) {
        CHECK(a[0] == b[0]);
    }
}

// ---------------------------------------------------------------------------
// L6 — C major triad detects as C major
// ---------------------------------------------------------------------------

static void test_L6_c_major() {
    Score s = makeTriad(60, 4, 7);   // C E G
    Key k = ::hush::composer::detectKey(s);
    CHECK(k.tonic == 0);
    CHECK(!k.minor);
}

// ---------------------------------------------------------------------------
// L7 — A minor triad detects as A minor
// ---------------------------------------------------------------------------

static void test_L7_a_minor() {
    Score s = makeTriad(57, 3, 7);   // A C E
    Key k = ::hush::composer::detectKey(s);
    CHECK(k.tonic == 9);             // A
    CHECK(k.minor);
}

// ---------------------------------------------------------------------------
// L8 — MIDI round-trip
// ---------------------------------------------------------------------------

static void test_L8_midi_roundtrip() {
    Score original = makeTriad(60, 4, 7);
    original.tempoBpm = 96;
    original.ppq      = 480;

    auto bytes  = serialiseMidi(original);
    Score round = parseMidi(bytes);

    CHECK(round.ppq == original.ppq);
    CHECK(round.tempoBpm == original.tempoBpm);
    CHECK(round.measures.size() == original.measures.size());
    if (round.measures.empty()) return;
    CHECK(round.measures[0].notes.size() == 3u);

    std::vector<int> origPitches, roundPitches;
    for (const auto& n : original.measures[0].notes)
        origPitches.push_back(n.pitch);
    for (const auto& n : round.measures[0].notes)
        roundPitches.push_back(n.pitch);

    std::sort(origPitches.begin(), origPitches.end());
    std::sort(roundPitches.begin(), roundPitches.end());
    CHECK(origPitches == roundPitches);
}

// ---------------------------------------------------------------------------
// L9 — Markov rows sum to 1 after normalization
// ---------------------------------------------------------------------------

static void test_L9_markov_normalization() {
    for (const char* s : {"pop", "jazz", "classical", "ambient"}) {
        std::string style(s);
        const StyleTables& t = styleTable(style);
        for (int from = 0; from < 7; ++from) {
            auto row = normalizedRow(t, from);
            float sum = 0.f;
            for (float v : row) sum += v;
            CHECK(std::fabs(sum - 1.0f) < 1e-5f);
        }
    }
}

// ---------------------------------------------------------------------------
// L10 — backend() == "lite"
// ---------------------------------------------------------------------------

static void test_L10_backend() {
    LiteComposer c;
    CHECK(c.backend() == std::string("lite"));
}

// ---------------------------------------------------------------------------

int main() {
    test_L1_empty_context();
    test_L2_single_note();
    test_L3_determinism();
    test_L4_all_styles();
    test_L5_malformed_style_dir();
    test_L6_c_major();
    test_L7_a_minor();
    test_L8_midi_roundtrip();
    test_L9_markov_normalization();
    test_L10_backend();

    std::cout << "lite_tests: " << g_pass << " passed, "
              << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
