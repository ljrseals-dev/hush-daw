// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once
#include <memory>
#include <string>
#include "hush/notation/score.hpp"

namespace hush::composer {

struct SampleConfig {
    float    temperature  = 1.0f;
    int      topK         = 40;
    float    topP         = 0.95f;
    uint64_t seed         = 0;
    int      maxNewTokens = 256;
    int      bars         = 4;
    std::string style     = "pop";
};

enum class Style { Pop, Jazz, Classical, Ambient };

class IComposer {
public:
    virtual ~IComposer() = default;
    virtual notation::Score continueFrom(const notation::Score& prompt,
                                         const SampleConfig& sample) const = 0;
    virtual notation::Score generate(Style style, int bars,
                                     const SampleConfig& sample) const = 0;
    virtual int  detectKey(const notation::Score& score) const = 0;
    virtual bool isMinorKey(const notation::Score& score) const = 0;
    virtual std::string backend() const = 0;
};

enum class Backend { Lite, Neural };

std::unique_ptr<IComposer> makeComposer(Backend which);
std::unique_ptr<IComposer> makeComposer(Backend which, const std::string& modelPath);

struct Available {
    bool lite   = false;
    bool neural = false;
};
Available availableBackends();

// Test helper
std::unique_ptr<IComposer> makeLiteForTest();

} // namespace hush::composer
