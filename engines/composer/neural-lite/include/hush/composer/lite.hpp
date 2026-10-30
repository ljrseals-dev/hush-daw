// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once
#include "hush/composer/composer.hpp"

#include <memory>
#include <string>
#include <vector>

namespace hush::composer {

class LiteComposer final : public IComposer {
public:
    explicit LiteComposer(const std::string& styleDir = {});
    ~LiteComposer() override;

    LiteComposer(const LiteComposer&)            = delete;
    LiteComposer& operator=(const LiteComposer&) = delete;

    notation::Score continueFrom(const notation::Score& prompt,
                                 const SampleConfig& sample) const override;
    notation::Score generate(Style style, int bars,
                             const SampleConfig& sample) const override;
    int  detectKey(const notation::Score& score) const override;
    bool isMinorKey(const notation::Score& score) const override;
    std::string backend() const override;

    std::vector<notation::Score> suggest(
        const notation::Score& context,
        const std::string& style,
        int maxResults,
        int bars,
        uint64_t seed) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hush::composer
