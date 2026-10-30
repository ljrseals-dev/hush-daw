// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/composer/neural.hpp"
#include "onnx_session.hpp"
#include "tokenizer.hpp"

// Shared harmony from neural-lite. Key detection is classical music
// theory and does not need a neural network.
#include "harmony.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace hush::composer {

namespace {

// ---------------------------------------------------------------------------
// Style -> token byte
// ---------------------------------------------------------------------------

int styleByteFor(Style s) {
    switch (s) {
        case Style::Pop:       return neural::kStylePop;
        case Style::Jazz:      return neural::kStyleJazz;
        case Style::Classical: return neural::kStyleClassical;
        case Style::Ambient:   return neural::kStyleAmbient;
    }
    return neural::kStylePop;
}

int styleByteFromString(const std::string& s) {
    if (s == "jazz")      return neural::kStyleJazz;
    if (s == "classical") return neural::kStyleClassical;
    if (s == "ambient")   return neural::kStyleAmbient;
    return neural::kStylePop;
}

// ---------------------------------------------------------------------------
// Softmax
// ---------------------------------------------------------------------------

void softmaxInPlace(std::vector<float>& v) {
    if (v.empty()) return;
    const float m = *std::max_element(v.begin(), v.end());
    double sum = 0.0;
    for (float& x : v) {
        const double e = std::exp(static_cast<double>(x - m));
        x = static_cast<float>(e);
        sum += e;
    }
    if (sum < 1e-30) {
        const float uniform = 1.0f / static_cast<float>(v.size());
        std::fill(v.begin(), v.end(), uniform);
        return;
    }
    const float inv = static_cast<float>(1.0 / sum);
    for (float& x : v) x *= inv;
}

// ---------------------------------------------------------------------------
// Sampling: temperature -> top-k -> top-p -> multinomial
// ---------------------------------------------------------------------------
//
// Order matters. Temperature rescales the logits. Top-k keeps the k
// highest. Top-p keeps the smallest set whose cumulative probability
// exceeds p. Each stage is applied literally so the tests can prove
// each parameter is live.

int sampleToken(const std::vector<float>& logits,
                float temperature,
                int topK,
                float topP,
                std::mt19937_64& rng) {
    const size_t n = logits.size();
    if (n == 0)
        throw std::runtime_error("sampleToken: empty logits");

    // Guard against pathological inputs. NaN anywhere poisons the
    // whole distribution, so treat it as a hard error rather than
    // silently sampling garbage.
    for (float v : logits) {
        if (std::isnan(v)) {
            throw std::runtime_error(
                "sampleToken: model produced NaN in logits");
        }
    }

    const bool greedy = (temperature <= 1e-6f);

    std::vector<float> scaled(n);
    if (greedy) {
        std::copy(logits.begin(), logits.end(), scaled.begin());
    } else {
        const float invT = 1.0f / temperature;
        for (size_t i = 0; i < n; ++i) scaled[i] = logits[i] * invT;
    }

    // Top-k.
    if (topK > 0 && static_cast<size_t>(topK) < n) {
        std::vector<size_t> idx(n);
        for (size_t i = 0; i < n; ++i) idx[i] = i;
        std::nth_element(
            idx.begin(), idx.begin() + topK, idx.end(),
            [&](size_t a, size_t b) { return scaled[a] > scaled[b]; });

        const float negInf =
            -std::numeric_limits<float>::infinity();
        for (size_t i = static_cast<size_t>(topK); i < n; ++i)
            scaled[idx[i]] = negInf;
    }

    // Softmax.
    std::vector<float> probs = scaled;
    softmaxInPlace(probs);

    // Top-p.
    if (topP > 0.0f && topP < 1.0f) {
        std::vector<size_t> idx(n);
        for (size_t i = 0; i < n; ++i) idx[i] = i;
        std::sort(idx.begin(), idx.end(),
                  [&](size_t a, size_t b) {
                      return probs[a] > probs[b];
                  });

        float cum  = 0.0f;
        size_t keep = 0;
        for (size_t i = 0; i < n; ++i) {
            cum += probs[idx[i]];
            keep = i + 1;
            if (cum >= topP) break;
        }
        for (size_t i = keep; i < n; ++i) probs[idx[i]] = 0.0f;

        float sum = 0.0f;
        for (float p : probs) sum += p;
        if (sum < 1e-30f) {
            // Degenerate: top-p truncated everything. Fall back to
            // the single most likely token rather than throwing.
            const size_t best = static_cast<size_t>(
                std::max_element(probs.begin(), probs.end())
                - probs.begin());
            return static_cast<int>(best);
        }
        const float inv = 1.0f / sum;
        for (float& p : probs) p *= inv;
    }

    if (greedy) {
        return static_cast<int>(
            std::max_element(probs.begin(), probs.end())
            - probs.begin());
    }

    std::discrete_distribution<int> dist(probs.begin(), probs.end());
    return dist(rng);
}

// ---------------------------------------------------------------------------
// RNG
// ---------------------------------------------------------------------------

std::mt19937_64 makeRng(uint64_t seed) {
    std::mt19937_64 rng;
    if (seed != 0) rng.seed(seed);
    else           rng.seed(std::random_device{}());
    return rng;
}

// ---------------------------------------------------------------------------
// Prompt construction
// ---------------------------------------------------------------------------

/// Tokenize a prompt, then strip the trailing PAD and EOS so the
/// model produces its own continuation rather than echoing the
/// prompt's terminator.
std::vector<int> promptTokens(const notation::Score& prompt,
                              int styleByte,
                              int maxTokens) {
    auto toks = neural::tokenize(prompt, maxTokens, styleByte);
    while (!toks.empty() && toks.back() == neural::kTokPad)
        toks.pop_back();
    if (!toks.empty() && toks.back() == neural::kTokEos)
        toks.pop_back();
    return toks;
}

/// Seed sequence for fresh generation: BOS, STYLE s, then one BAR
/// token per requested measure so the model knows the target length.
std::vector<int> generateSeedTokens(Style style, int bars) {
    std::vector<int> toks;
    toks.push_back(neural::kTokBos);
    toks.push_back(neural::kTokStyle);
    toks.push_back(styleByteFor(style));
    for (int i = 0; i < bars; ++i)
        toks.push_back(neural::kTokBar);
    return toks;
}

int barCount(const std::vector<int>& toks) {
    int n = 0;
    for (int t : toks) if (t == neural::kTokBar) ++n;
    return n;
}

constexpr int kHardTokenCeiling = 510;

} // namespace

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct NeuralComposer::Impl {
    std::string modelPath;
    bool        useCuda = false;
    std::unique_ptr<neural::OnnxSession> session;

    Impl(const std::string& path, bool cuda)
        : modelPath(path), useCuda(cuda) {
        session = std::make_unique<neural::OnnxSession>(path, cuda, 0);
        if (session->vocabSize() != neural::kVocabSize) {
            std::ostringstream os;
            os << "NeuralComposer: model vocab size is "
               << session->vocabSize()
               << ", tokenizer expects " << neural::kVocabSize
               << ". Regenerate the model, or update the tokenizer "
               << "contract to match.";
            throw std::runtime_error(os.str());
        }
    }
};

// ---------------------------------------------------------------------------
// NeuralComposer
// ---------------------------------------------------------------------------

NeuralComposer::NeuralComposer(const std::string& modelPath,
                               bool useCuda)
    : impl_(std::make_unique<Impl>(modelPath, useCuda)) {}

NeuralComposer::~NeuralComposer() = default;

std::string NeuralComposer::backend() const { return "neural"; }

std::string NeuralComposer::modelPath() const {
    return impl_->modelPath;
}

int NeuralComposer::vocabSize() const {
    return impl_->session->vocabSize();
}

// The `::hush::composer::` prefix is required: without it, unqualified
// lookup finds the member function, not the free function.
int NeuralComposer::detectKey(const notation::Score& score) const {
    return ::hush::composer::detectKey(score).tonic;
}

bool NeuralComposer::isMinorKey(const notation::Score& score) const {
    return ::hush::composer::detectKey(score).minor;
}

// ---------------------------------------------------------------------------
// continueFrom
// ---------------------------------------------------------------------------

notation::Score NeuralComposer::continueFrom(
    const notation::Score& prompt,
    const SampleConfig& sample) const {

    const int styleByte = styleByteFromString(
        sample.style.empty() ? std::string("pop") : sample.style);

    constexpr int kMaxPromptTokens = 480;
    auto tokens = promptTokens(prompt, styleByte, kMaxPromptTokens);

    if (tokens.empty()) {
        tokens.push_back(neural::kTokBos);
        tokens.push_back(neural::kTokStyle);
        tokens.push_back(styleByte);
    }

    auto rng = makeRng(sample.seed);

    const int maxNew = std::max(1, sample.maxNewTokens);
    int generated = 0;

    while (generated < maxNew) {
        int vocab = 0;
        const auto logits = impl_->session->run(
            tokens.data(),
            static_cast<int>(tokens.size()),
            vocab);

        if (vocab != neural::kVocabSize) {
            throw std::runtime_error(
                "NeuralComposer: model output vocab changed mid-run");
        }

        const int next = sampleToken(
            logits,
            sample.temperature,
            sample.topK,
            sample.topP,
            rng);

        if (next == neural::kTokEos) break;

        if (tokens.size() >= static_cast<size_t>(kHardTokenCeiling))
            break;

        tokens.push_back(next);
        ++generated;
    }

    tokens.push_back(neural::kTokEos);
    return neural::detokenize(tokens);
}

// ---------------------------------------------------------------------------
// generate
// ---------------------------------------------------------------------------

notation::Score NeuralComposer::generate(Style style,
                                         int bars,
                                         const SampleConfig& sample) const {
    if (bars <= 0) bars = 4;

    // The IComposer interface passes style positionally here, but
    // SampleConfig also carries a style string. Prefer the string
    // when it is set, because continueFrom has no positional
    // argument and consistency between the two entry points matters
    // more than honoring a positional that callers may forget.
    //
    // If both are set and they disagree, sample.style wins. That is
    // documented in the README.
    const int effectiveStyleByte = sample.style.empty()
        ? styleByteFor(style)
        : styleByteFromString(sample.style);

    auto tokens = generateSeedTokens(style, bars);
    (void)effectiveStyleByte;   // seed tokens already carry the style

    // If sample.style was set and differs from the positional style,
    // overwrite the STYLE token we just emitted.
    if (!sample.style.empty() &&
        styleByteFromString(sample.style) != styleByteFor(style)) {
        tokens[2] = styleByteFromString(sample.style);
    }

    auto rng = makeRng(sample.seed);

    const int maxNew = std::max(1, sample.maxNewTokens);
    int generated = 0;

    const int targetBars = bars + 1;

    while (generated < maxNew) {
        int vocab = 0;
        const auto logits = impl_->session->run(
            tokens.data(),
            static_cast<int>(tokens.size()),
            vocab);

        if (vocab != neural::kVocabSize) {
            throw std::runtime_error(
                "NeuralComposer: model output vocab changed mid-run");
        }

        const int next = sampleToken(
            logits,
            sample.temperature,
            sample.topK,
            sample.topP,
            rng);

        if (next == neural::kTokEos) break;

        if (tokens.size() >= static_cast<size_t>(kHardTokenCeiling))
            break;

        tokens.push_back(next);
        ++generated;

        if (barCount(tokens) >= targetBars) break;
    }

    tokens.push_back(neural::kTokEos);
    return neural::detokenize(tokens);
}

} // namespace hush::composer
