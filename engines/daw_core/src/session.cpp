// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/daw_core/session.hpp"

#include <hush/immersive/renderer.hpp>
#include <hush/score/score.hpp>
#include <hush/plugins/plugin_host.hpp>
#include <hush/video/movie.hpp>

#include <unordered_map>
#include <stdexcept>
#include <fstream>
#include <cstring>

namespace hush::daw_core {

// ------------------------------------------------------------------
// Concrete render-node adapters
// ------------------------------------------------------------------

class ImmersiveNode final : public RenderNode {
public:
    explicit ImmersiveNode(hush::immersive::Renderer* r, int ch)
        : renderer_(r), channels_(ch) {
        if (!renderer_) throw std::invalid_argument("ImmersiveNode: null renderer");
        renderer_->setChannelCount(ch);
    }
    void process(FrameIndex start, FrameIndex count, AudioBuffer& out) override {
        renderer_->render(start, count, out.data.data());
    }
    int channelCount() const override { return channels_; }
private:
    hush::immersive::Renderer* renderer_;
    int channels_;
};

class ScoreNode final : public RenderNode {
public:
    explicit ScoreNode(hush::score::Score* s, int ch)
        : score_(s), channels_(ch) {
        if (!score_) throw std::invalid_argument("ScoreNode: null score");
    }
    void process(FrameIndex start, FrameIndex count, AudioBuffer& out) override {
        score_->render(start, count, out.data.data(), channels_);
    }
    int channelCount() const override { return channels_; }
private:
    hush::score::Score* score_;
    int channels_;
};

class PluginNode final : public RenderNode {
public:
    explicit PluginNode(hush::plugins::PluginHost* p, int ch)
        : host_(p), channels_(ch) {
        if (!host_) throw std::invalid_argument("PluginNode: null host");
    }
    void process(FrameIndex start, FrameIndex count, AudioBuffer& out) override {
        // Plugin processes in-place on the buffer that already contains
        // the sum of previous nodes. For simplicity we apply gain only.
        host_->process(start, count, out.data.data(), channels_);
    }
    int channelCount() const override { return channels_; }
private:
    hush::plugins::PluginHost* host_;
    int channels_;
};

// ------------------------------------------------------------------
// Session::Impl
// ------------------------------------------------------------------

struct Session::Impl {
    SampleRate rate     = 48000.0;
    FrameIndex length   = 0;
    int        channels = 2;
    Transport  transport;

    std::unordered_map<std::string, std::unique_ptr<hush::immersive::Renderer>> immersiveScenes;
    std::unordered_map<std::string, std::unique_ptr<hush::score::Score>>         scores;
    std::unordered_map<std::string, std::unique_ptr<hush::plugins::PluginHost>>  pluginChains;
    std::unordered_map<std::string, std::unique_ptr<hush::video::Movie>>          videos;

    RenderGraph graph;
    int nextId = 1;

    std::string makeId(const std::string& prefix) {
        return prefix + "_" + std::to_string(nextId++);
    }
};

Session::Session() : impl_(std::make_unique<Impl>()) {}
Session::~Session() = default;
Session::Session(Session&&) noexcept = default;
Session& Session::operator=(Session&&) noexcept = default;

void Session::setSampleRate(SampleRate r) {
    if (r <= 0.0)
        throw std::invalid_argument("Session::setSampleRate: rate must be positive");
    impl_->rate = r;
    impl_->transport.setSampleRate(r);
}

SampleRate Session::sampleRate() const noexcept {
    return impl_->rate;
}

void Session::setLength(FrameIndex frames) {
    if (frames < 0)
        throw std::invalid_argument("Session::setLength: negative length");
    impl_->length = frames;
}

FrameIndex Session::length() const noexcept {
    return impl_->length;
}

void Session::setMasterChannels(int ch) {
    if (ch < 1)
        throw std::invalid_argument("Session::setMasterChannels: channels < 1");
    impl_->channels = ch;
    impl_->graph.setMasterChannels(ch);
}

int Session::masterChannels() const noexcept {
    return impl_->channels;
}

ImmersiveSceneHandle Session::createImmersiveScene(const std::string&) {
    const auto id = impl_->makeId("immersive");
    impl_->immersiveScenes[id] = std::make_unique<hush::immersive::Renderer>();
    return {id};
}

ScoreHandle Session::createScore(const std::string&) {
    const auto id = impl_->makeId("score");
    impl_->scores[id] = std::make_unique<hush::score::Score>();
    return {id};
}

PluginChainHandle Session::createPluginChain(const std::string&) {
    const auto id = impl_->makeId("plugins");
    impl_->pluginChains[id] = std::make_unique<hush::plugins::PluginHost>();
    return {id};
}

VideoTimelineHandle Session::createVideoTimeline(const std::string&) {
    const auto id = impl_->makeId("video");
    impl_->videos[id] = std::make_unique<hush::video::Movie>();
    return {id};
}

hush::immersive::Renderer& Session::immersive(const ImmersiveSceneHandle& h) {
    auto it = impl_->immersiveScenes.find(h.id);
    if (it == impl_->immersiveScenes.end())
        throw std::out_of_range("Session::immersive: unknown id");
    return *it->second;
}

hush::score::Score& Session::score(const ScoreHandle& h) {
    auto it = impl_->scores.find(h.id);
    if (it == impl_->scores.end())
        throw std::out_of_range("Session::score: unknown id");
    return *it->second;
}

hush::plugins::PluginHost& Session::plugins(const PluginChainHandle& h) {
    auto it = impl_->pluginChains.find(h.id);
    if (it == impl_->pluginChains.end())
        throw std::out_of_range("Session::plugins: unknown id");
    return *it->second;
}

hush::video::Movie& Session::video(const VideoTimelineHandle& h) {
    auto it = impl_->videos.find(h.id);
    if (it == impl_->videos.end())
        throw std::out_of_range("Session::video: unknown id");
    return *it->second;
}

Transport& Session::transport() noexcept {
    return impl_->transport;
}

const Transport& Session::transport() const noexcept {
    return impl_->transport;
}

void Session::rebuildGraph() {
    impl_->graph.clear();
    impl_->graph.setMasterChannels(impl_->channels);

    // Immersive scenes first (beds / objects)
    for (auto& [id, renderer] : impl_->immersiveScenes) {
        (void)id;
        impl_->graph.addNode(std::make_unique<ImmersiveNode>(renderer.get(), impl_->channels));
    }

    // Score (MIDI-derived audio)
    for (auto& [id, sc] : impl_->scores) {
        (void)id;
        impl_->graph.addNode(std::make_unique<ScoreNode>(sc.get(), impl_->channels));
    }

    // Plugin chains applied last (in-place processing)
    for (auto& [id, host] : impl_->pluginChains) {
        (void)id;
        impl_->graph.addNode(std::make_unique<PluginNode>(host.get(), impl_->channels));
    }

    // Video contributes markers only; no audio node.
}

AudioBuffer Session::render(FrameIndex start, FrameIndex count) {
    if (start < 0 || count < 0)
        throw std::invalid_argument("Session::render: negative range");
    if (impl_->length > 0 && start + count > impl_->length)
        throw std::out_of_range("Session::render: range exceeds session length");
    return impl_->graph.render(start, count);
}

void Session::renderToWav(const std::string& path) {
    if (impl_->length <= 0)
        throw std::runtime_error("Session::renderToWav: zero-length session");

    // Ensure graph is current
    rebuildGraph();

    AudioBuffer buf = render(0, impl_->length);
    buf.rate = impl_->rate;

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs)
        throw std::runtime_error("Session::renderToWav: cannot open " + path);

    const int     channels   = buf.channels;
    const int     sampleRate = static_cast<int>(buf.rate);
    const int32_t dataBytes  = static_cast<int32_t>(buf.data.size() * sizeof(float));

    auto write32 = [&](uint32_t v) {
        char b[4];
        std::memcpy(b, &v, 4);
        ofs.write(b, 4);
    };
    auto write16 = [&](uint16_t v) {
        char b[2];
        std::memcpy(b, &v, 2);
        ofs.write(b, 2);
    };

    ofs.write("RIFF", 4);
    write32(36 + static_cast<uint32_t>(dataBytes));
    ofs.write("WAVE", 4);
    ofs.write("fmt ", 4);
    write32(16);
    write16(3);                                    // IEEE float
    write16(static_cast<uint16_t>(channels));
    write32(static_cast<uint32_t>(sampleRate));
    write32(static_cast<uint32_t>(sampleRate * channels * 4));
    write16(static_cast<uint16_t>(channels * 4));
    write16(32);
    ofs.write("data", 4);
    write32(static_cast<uint32_t>(dataBytes));
    ofs.write(reinterpret_cast<const char*>(buf.data.data()), dataBytes);

    if (!ofs)
        throw std::runtime_error("Session::renderToWav: write failed");
}

} // namespace hush::daw_core
