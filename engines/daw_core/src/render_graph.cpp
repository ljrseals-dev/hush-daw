// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/daw_core/render_graph.hpp"
#include <stdexcept>

namespace hush::daw_core {

void RenderGraph::clear() {
    nodes_.clear();
}

void RenderGraph::addNode(RenderNodePtr node) {
    if (!node)
        throw std::invalid_argument("RenderGraph::addNode: null node");
    nodes_.push_back(std::move(node));
}

void RenderGraph::setMasterChannels(int ch) {
    if (ch < 1)
        throw std::invalid_argument("RenderGraph::setMasterChannels: channels must be >= 1");
    masterChannels_ = ch;
}

int RenderGraph::masterChannels() const noexcept {
    return masterChannels_;
}

size_t RenderGraph::nodeCount() const noexcept {
    return nodes_.size();
}

AudioBuffer RenderGraph::render(FrameIndex start, FrameIndex count) const {
    if (start < 0 || count < 0)
        throw std::invalid_argument("RenderGraph::render: negative range");

    AudioBuffer master;
    master.resize(masterChannels_, count);

    for (const auto& node : nodes_) {
        if (node->channelCount() != masterChannels_)
            throw std::runtime_error("RenderGraph::render: channel count mismatch");

        AudioBuffer tmp;
        tmp.resize(masterChannels_, count);
        node->process(start, count, tmp);

        for (size_t i = 0; i < master.data.size(); ++i)
            master.data[i] += tmp.data[i];
    }
    return master;
}

} // namespace hush::daw_core
