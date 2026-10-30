// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/composer/neural.hpp"
#include "hush/composer/composer.hpp"
#include "tokenizer.hpp"
#include "smf.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace hush::composer;
using namespace hush::composer::neural;
using notation::Score;
using notation::Measure;
using notation::Note;

// Provided by composer_factory.cpp. Declared inside the namespace so
// it links against the correct symbol.
namespace hush::composer {
std::unique_ptr<IComposer> makeLiteForTest();
}

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
// Fixtures
// ---------------------------------------------------------------------------

static Score makeSimpleBar() {
    Score s;
    s.ppq      = 480;
    s.tempoBpm = 120;
    Measure m;
    m.ticksPerMeasure = 1920;
    // Durations chosen to land on bucket centers so tokenizer
    // round-trip is exact. 240 ticks = eighth at 480 ppq = bucket 2.
    for (int p : {60, 64, 67}) {
        Note n;
        n.pitch         = p;
        n.durationTicks = 240;   // bucket 2 (1/8)
        n.velocity      = 80;    // bucket 1 (mp)
        n.startTick     = 0;
        m.notes.push_back(n);
    }
    s.measures.push_back(m);
    return s;
}

static Score makeRichScore() {
    Score s;
    s.ppq      = 480;
    s.tempoBpm = 96;
    for (int bar = 0; bar < 4; ++bar) {
        Measure m;
        m.ticksPerMeasure = 1920;
        for (int i = 0; i < 4; ++i) {
            Note n;
            n.pitch         = 60 + ((bar * 3 + i * 2) % 12);
            n.durationTicks = 480;   // bucket 3 (1/4)
            n.velocity      = 60 + (i * 15);
            n.startTick     = i * 480;
            m.notes.push_back(n);
        }
        s.measures.push_back(m);
    }
    return s;
}

// Path relative to the build directory. CMake's add_test runs from
// the build dir, so "model/model.onnx" resolves to the source dir
// only if CMake copies or symlinks it. If not present, the tests
// that need it skip.
static const char* kModelPath = "model/model.onnx";

static bool modelPresent() {
    std::ifstream in(kModelPath);
    return in.good();
}

// ---------------------------------------------------------------------------
// N1 — tokenizer round-trip
// ---------------------------------------------------------------------------

static void test_N1_tokenizer_roundtrip() {
    std::mt19937 rng(1234);
    // Bucket-aligned durations (in ticks at 480 ppq).
    // 120 = 1/16, 240 = 1/8, 480 = 1/4, 960 = 1/2, 1920 = whole
    const int durs[] = {120, 240, 480, 960, 1920};
    // Bucket-aligned velocities.
    const int vels[] = {40, 70, 95, 120};

    for (int trial = 0; trial < 100; ++trial) {
        Score s;
        s.ppq      = 480;
        s.tempoBpm = 120;
        const int nMeasures = 1 + (trial % 4);
        for (int bar = 0; bar < nMeasures; ++bar) {
            Measure m;
            m.ticksPerMeasure = 1920;
            const int nNotes = 1 + (trial % 4);
            int cursor = 0;
            for (int i = 0; i < nNotes; ++i) {
                Note n;
                n.pitch         = 48 + (trial * 7 + i * 3) % 37;
                n.durationTicks = durs[(trial + i) % 5];
                n.velocity      = vels[(trial + i) % 4];
                n.startTick     = cursor;
                cursor += n.durationTicks;
                m.notes.push_back(n);
            }
            s.measures.push_back(m);
        }

        const auto toks = tokenize(s, 512, kStylePop);
        const Score back = detokenize(toks);

        CHECK(back.measures.size() == s.measures.size());
        if (back.measures.size() != s.measures.size()) continue;

        for (size_t i = 0; i < s.measures.size(); ++i) {
            CHECK(back.measures[i].notes.size()
                  == s.measures[i].notes.size());
            if (back.measures[i].notes.size()
                != s.measures[i].notes.size()) continue;

            for (size_t j = 0; j < s.measures[i].notes.size(); ++j) {
                CHECK(back.measures[i].notes[j].pitch
                      == s.measures[i].notes[j].pitch);
                CHECK(back.measures[i].notes[j].durationTicks
                      == s.measures[i].notes[j].durationTicks);
                CHECK(back.measures[i].notes[j].velocity
                      == s.measures[i].notes[j].velocity);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// N2 — token bounds and malformed rejection
// ---------------------------------------------------------------------------

static void test_N2_tokenizer_bounds() {
    Score s = makeRichScore();
    const auto toks = tokenize(s, 512, kStyleJazz);
    for (int t : toks) {
        CHECK(t >= 0);
        CHECK(t < kVocabSize);
    }

    auto expectThrow = [](const std::vector<int>& bad) {
        bool threw = false;
        try { (void)detokenize(bad); }
        catch (const TokenizerError&) { threw = true; }
        catch (const std::exception&) { threw = true; }
        CHECK(threw);
    };

    expectThrow({});                                     // empty
    expectThrow({999});                                  // no BOS
    expectThrow({kTokBos, 999, kTokEos});                // unknown token
    expectThrow({kTokBos, kTokPitchBase + 127});         // pitch, no dur
    expectThrow({kTokBos, kTokPitchBase + 10, 999});     // bad dur token
    expectThrow({kTokBos, kTokRest});                    // rest, no dur
}

// ---------------------------------------------------------------------------
// N3 — determinism
// ---------------------------------------------------------------------------

static void test_N3_determinism() {
    if (!modelPresent()) {
        std::cout << "[N3] SKIP: model not present at "
                  << kModelPath << "\n";
        return;
    }
    Score prompt = makeRichScore();

    SampleConfig sc;
    sc.temperature  = 0.9f;
    sc.topK         = 40;
    sc.topP         = 0.95f;
    sc.seed         = 42;
    sc.maxNewTokens = 64;
    sc.style        = "pop";

    NeuralComposer c(kModelPath);
    Score a = c.continueFrom(prompt, sc);
    Score b = c.continueFrom(prompt, sc);

    CHECK(a == b);

    auto bytesA = serialiseMidi(a);
    auto bytesB = serialiseMidi(b);
    CHECK(bytesA == bytesB);
}

// ---------------------------------------------------------------------------
// N4 — temperature changes the output distribution
// ---------------------------------------------------------------------------

static double scoreEntropy(const Score& s) {
    int hist[12] = {0};
    int total    = 0;
    for (const auto& m : s.measures) {
        for (const auto& n : m.notes) {
            hist[n.pitch % 12]++;
            total++;
        }
    }
    if (total == 0) return 0.0;
    double H = 0.0;
    for (int i = 0; i < 12; ++i) {
        if (hist[i] == 0) continue;
        const double p = static_cast<double>(hist[i]) / total;
        H -= p * std::log2(p);
    }
    return H;
}

static void test_N4_temperature() {
    if (!modelPresent()) {
        std::cout << "[N4] SKIP: model not present\n";
        return;
    }
    Score prompt = makeSimpleBar();
    NeuralComposer c(kModelPath);

    SampleConfig cold;
    cold.temperature  = 0.1f;
    cold.topK         = 147;   // disabled
    cold.topP         = 0.0f;  // disabled
    cold.seed         = 7;
    cold.maxNewTokens = 200;
    cold.style        = "pop";

    SampleConfig hot = cold;
    hot.temperature = 2.0f;

    double coldH = 0, hotH = 0;
    for (int i = 0; i < 5; ++i) {
        cold.seed = 7 + i;
        hot.seed  = 7 + i;
        coldH += scoreEntropy(c.continueFrom(prompt, cold));
        hotH  += scoreEntropy(c.continueFrom(prompt, hot));
    }
    CHECK(hotH > coldH);
}

// ---------------------------------------------------------------------------
// N5 — top-k=1 gives greedy, seed-independent output
// ---------------------------------------------------------------------------

static void test_N5_topk() {
    if (!modelPresent()) {
        std::cout << "[N5] SKIP: model not present\n";
        return;
    }
    Score prompt = makeSimpleBar();
    NeuralComposer c(kModelPath);

    SampleConfig a;
    a.temperature  = 1.0f;
    a.topK         = 1;
    a.topP         = 0.0f;
    a.seed         = 1;
    a.maxNewTokens = 64;
    a.style        = "pop";

    SampleConfig b = a;
    b.seed = 999;

    Score sa = c.continueFrom(prompt, a);
    Score sb = c.continueFrom(prompt, b);
    CHECK(sa == sb);
}

// ---------------------------------------------------------------------------
// N6 — top-p=0 behaves like greedy
// ---------------------------------------------------------------------------

static void test_N6_topp() {
    if (!modelPresent()) {
        std::cout << "[N6] SKIP: model not present\n";
        return;
    }
    Score prompt = makeSimpleBar();
    NeuralComposer c(kModelPath);

    SampleConfig greedy;
    greedy.temperature  = 1.0f;
    greedy.topK         = 1;
    greedy.topP         = 0.0f;
    greedy.seed         = 42;
    greedy.maxNewTokens = 64;
    greedy.style        = "pop";

    SampleConfig topp;
    topp.temperature  = 1.0f;
    topp.topK         = 147;   // disabled
    topp.topP         = 0.0f;  // collapses to argmax in the pipeline
    topp.seed         = 42;
    topp.maxNewTokens = 64;
    topp.style        = "pop";

    Score sg = c.continueFrom(prompt, greedy);
    Score sp = c.continueFrom(prompt, topp);
    CHECK(sg == sp);
}

// ---------------------------------------------------------------------------
// N7 — model missing throws with the path in the message
// ---------------------------------------------------------------------------

static void test_N7_model_missing() {
    bool threw = false;
    std::string msg;
    try {
        NeuralComposer c("/tmp/hush-does-not-exist-1234.onnx");
    } catch (const std::exception& e) {
        threw = true;
        msg = e.what();
    }
    CHECK(threw);
    CHECK(msg.find("hush-does-not-exist-1234") != std::string::npos);
}

// ---------------------------------------------------------------------------
// N8 — malformed model throws
// ---------------------------------------------------------------------------

static void test_N8_model_malformed() {
    const std::string path = "/tmp/hush-bad-model.onnx";
    {
        std::ofstream out(path, std::ios::binary);
        const char garbage[] = "not an onnx file at all, just text";
        out.write(garbage, sizeof(garbage) - 1);
    }

    bool threw = false;
    try {
        NeuralComposer c(path);
    } catch (const std::exception&) {
        threw = true;
    }
    std::remove(path.c_str());
    CHECK(threw);
}

// ---------------------------------------------------------------------------
// N9 — CLI smoke
// ---------------------------------------------------------------------------

static void test_N9_cli_smoke() {
    const char* candidates[] = {"./hush-neural", "../hush-neural",
                                 "hush-neural"};
    std::string cli;
    for (const char* c : candidates) {
        std::ifstream probe(c);
        if (probe.good()) { cli = c; break; }
    }
    if (cli.empty()) {
        std::cout << "[N9] SKIP: hush-neural binary not found\n";
        return;
    }
    if (!modelPresent()) {
        std::cout << "[N9] SKIP: model not present\n";
        return;
    }

    const std::string out = "/tmp/hush-n9-out.mid";
    std::string cmd = cli
        + " generate --style jazz --bars 2 --out " + out
        + " --model " + kModelPath
        + " --seed 1 --tokens 32";
    const int rc = std::system(cmd.c_str());
    CHECK(rc == 0);

    try {
        Score s = readMidi(out);
        size_t totalNotes = 0;
        for (const auto& m : s.measures) totalNotes += m.notes.size();
        CHECK(totalNotes >= 1);
    } catch (const SmfError&) {
        CHECK(false);
    }
    std::remove(out.c_str());
}

// ---------------------------------------------------------------------------
// N10 — vocab contract is 147
// ---------------------------------------------------------------------------

static void test_N10_vocab_contract() {
    CHECK(kVocabSize == 147);

    // Construction with a missing file must throw.
    bool threw = false;
    try {
        NeuralComposer c("/tmp/hush-no-such.onnx");
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}

// ---------------------------------------------------------------------------
// N11 — interface conformance
// ---------------------------------------------------------------------------

static void test_N11_interface_conformance() {
    auto lite = makeLiteForTest();
    CHECK(lite != nullptr);
    CHECK(lite->backend() == "lite");

    Score s = makeSimpleBar();
    SampleConfig sc;
    sc.seed  = 42;
    sc.bars  = 1;
    sc.style = "pop";

    IComposer& ref = *lite;
    const int k = ref.detectKey(s);
    CHECK(k >= 0 && k <= 11);
    (void)ref.isMinorKey(s);
    (void)ref.generate(Style::Pop, 1, sc);

    if (modelPresent()) {
        NeuralComposer nc(kModelPath);
        IComposer& nref = nc;
        CHECK(nref.backend() == "neural");
        const int nk = nref.detectKey(s);
        CHECK(nk >= 0 && nk <= 11);
    }
}

// ---------------------------------------------------------------------------
// N12 — backend strings
// ---------------------------------------------------------------------------

static void test_N12_backend_strings() {
    auto lite = makeLiteForTest();
    CHECK(lite->backend() == std::string("lite"));

    if (modelPresent()) {
        NeuralComposer nc(kModelPath);
        CHECK(nc.backend() == std::string("neural"));
    }
}

// ---------------------------------------------------------------------------

int main() {
    test_N1_tokenizer_roundtrip();
    test_N2_tokenizer_bounds();
    test_N3_determinism();
    test_N4_temperature();
    test_N5_topk();
    test_N6_topp();
    test_N7_model_missing();
    test_N8_model_malformed();
    test_N9_cli_smoke();
    test_N10_vocab_contract();
    test_N11_interface_conformance();
    test_N12_backend_strings();

    std::cout << "neural_tests: " << g_pass << " passed, "
              << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
