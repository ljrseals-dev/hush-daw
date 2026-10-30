// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include "types.hpp"
#include <mutex>

namespace hush::daw_core {

enum class TransportState : uint8_t {
    Stopped = 0,
    Playing,
    Recording
};

class Transport {
public:
    Transport() = default;

    void setSampleRate(SampleRate r);
    SampleRate sampleRate() const noexcept;

    void setPosition(FrameIndex f);
    FrameIndex position() const noexcept;

    void setLoop(FrameIndex start, FrameIndex end);
    void clearLoop();
    bool looping() const noexcept;
    FrameIndex loopStart() const noexcept;
    FrameIndex loopEnd() const noexcept;

    void play();
    void stop();
    void record();
    TransportState state() const noexcept;

    struct AdvanceResult {
        FrameIndex begin  = 0;
        FrameIndex end    = 0;
        bool       looped = false;
    };
    AdvanceResult advance(FrameIndex n);

private:
    mutable std::mutex mutex_;
    SampleRate         rate_      = 48000.0;
    FrameIndex         position_  = 0;
    FrameIndex         loopStart_ = 0;
    FrameIndex         loopEnd_   = 0;
    bool               loop_      = false;
    TransportState     state_     = TransportState::Stopped;
};

} // namespace hush::daw_core
