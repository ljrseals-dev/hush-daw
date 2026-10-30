// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include "types.hpp"
#include <memory>
#include <vector>

namespace hush::daw_core {

class RenderNode {
public:
    virtual ~RenderNode() = default;
    virtual void process(FrameIndex start, FrameIndex count, AudioBuffer& out) = 0;
    virtual int  channelCount() const = 0;
};

using RenderNodePtr = std::unique_ptr<RenderNode>;

class RenderGraph {
public:
    void clear();
    void addNode(RenderNodePtr node);
    void setMasterChannels(int ch);
    int  masterChannels() const noexcept;
    size_t nodeCount() const noexcept;

    AudioBuffer render(FrameIndex start, FrameIndex count) const;

private:
    std::vector<RenderNodePtr> nodes_;
    int masterChannels_ = 2;
};

} // namespace hush::daw_core
