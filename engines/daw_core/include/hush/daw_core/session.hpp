// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include "types.hpp"
#include "transport.hpp"
#include "render_graph.hpp"
#include "session_io.hpp"

#include <memory>
#include <string>

namespace hush::immersive { class Renderer; }
namespace hush::score     { class Score; }
namespace hush::plugins   { class PluginHost; }
namespace hush::video     { class Movie; }

namespace hush::daw_core {

class Session {
public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) noexcept;
    Session& operator=(Session&&) noexcept;

    void setSampleRate(SampleRate r);
    SampleRate sampleRate() const noexcept;

    void setLength(FrameIndex frames);
    FrameIndex length() const noexcept;

    void setMasterChannels(int ch);
    int  masterChannels() const noexcept;

    ImmersiveSceneHandle createImmersiveScene(const std::string& name = {});
    ScoreHandle          createScore(const std::string& name = {});
    PluginChainHandle    createPluginChain(const std::string& name = {});
    VideoTimelineHandle  createVideoTimeline(const std::string& name = {});

    hush::immersive::Renderer& immersive(const ImmersiveSceneHandle& h);
    hush::score::Score&        score(const ScoreHandle& h);
    hush::plugins::PluginHost& plugins(const PluginChainHandle& h);
    hush::video::Movie&        video(const VideoTimelineHandle& h);

    Transport& transport() noexcept;
    const Transport& transport() const noexcept;

    // Rebuilds the graph from every owned subsystem.
    void rebuildGraph();

    AudioBuffer render(FrameIndex start, FrameIndex count);
    void renderToWav(const std::string& path);

    friend void session_io::save(const Session&, const std::string&);
    friend Session session_io::load(const std::string&);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hush::daw_core
