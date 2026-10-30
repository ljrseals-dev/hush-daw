// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once
#include "hush/composer/composer.hpp"

#include <memory>
#include <string>
#include <vector>

namespace hush::composer {

class NeuralComposer final : public IComposer {
public:
    explicit NeuralComposer(const std::string& modelPath, bool useCuda = false);
    ~NeuralComposer() override;

    NeuralComposer(const NeuralComposer&)            = delete;
    NeuralComposer& operator=(const NeuralComposer&) = delete;

    notation::Score continueFrom(const notation::Score& prompt,
                                 const SampleConfig& sample) const override;
    notation::Score generate(Style style, int bars,
                             const SampleConfig& sample) const override;
    int  detectKey(const notation::Score& score) const override;
    bool isMinorKey(const notation::Score& score) const override;
    std::string backend() const override;

    int vocabSize() const;
    std::string modelPath() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hush::composer
