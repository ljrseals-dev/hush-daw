// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/daw_core/transport.hpp"
#include <stdexcept>

namespace hush::daw_core {

void Transport::setSampleRate(SampleRate r) {
    if (r <= 0.0)
        throw std::invalid_argument("Transport::setSampleRate: rate must be positive");
    std::lock_guard<std::mutex> lock(mutex_);
    rate_ = r;
}

SampleRate Transport::sampleRate() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return rate_;
}

void Transport::setPosition(FrameIndex f) {
    if (f < 0)
        throw std::invalid_argument("Transport::setPosition: negative position");
    std::lock_guard<std::mutex> lock(mutex_);
    position_ = f;
}

FrameIndex Transport::position() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return position_;
}

void Transport::setLoop(FrameIndex start, FrameIndex end) {
    if (start < 0 || end <= start)
        throw std::invalid_argument("Transport::setLoop: invalid range");
    std::lock_guard<std::mutex> lock(mutex_);
    loopStart_ = start;
    loopEnd_   = end;
    loop_      = true;
}

void Transport::clearLoop() {
    std::lock_guard<std::mutex> lock(mutex_);
    loop_ = false;
}

bool Transport::looping() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return loop_;
}

FrameIndex Transport::loopStart() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return loopStart_;
}

FrameIndex Transport::loopEnd() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return loopEnd_;
}

void Transport::play() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = TransportState::Playing;
}

void Transport::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = TransportState::Stopped;
}

void Transport::record() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = TransportState::Recording;
}

TransportState Transport::state() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

Transport::AdvanceResult Transport::advance(FrameIndex n) {
    if (n < 0)
        throw std::invalid_argument("Transport::advance: negative count");
    std::lock_guard<std::mutex> lock(mutex_);

    AdvanceResult r;
    r.begin  = position_;
    r.end    = position_ + n;
    r.looped = false;

    if (loop_ && position_ + n > loopEnd_) {
        const FrameIndex over = (position_ + n) - loopEnd_;
        position_ = loopStart_ + over;
        r.end     = loopEnd_;
        r.looped  = true;
    } else {
        position_ += n;
    }
    return r;
}

} // namespace hush::daw_core
