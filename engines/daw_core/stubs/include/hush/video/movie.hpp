// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
// Minimal self-contained stub for hush::video::Movie.
// No FFmpeg dependency; provides timeline markers only.
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace hush::video {

struct Marker {
    int64_t     frame = 0;
    std::string label;
};

class Movie {
public:
    Movie() = default;

    void setFrameRate(double fps) {
        if (fps <= 0.0) throw std::invalid_argument("Movie::setFrameRate: fps <= 0");
        fps_ = fps;
    }
    double frameRate() const noexcept { return fps_; }

    void setLength(int64_t frames) {
        if (frames < 0) throw std::invalid_argument("Movie::setLength: negative");
        length_ = frames;
    }
    int64_t length() const noexcept { return length_; }

    void addMarker(const Marker& m) {
        if (m.frame < 0) throw std::invalid_argument("Movie::addMarker: negative frame");
        markers_.push_back(m);
    }
    const std::vector<Marker>& markers() const noexcept { return markers_; }
    void clearMarkers() { markers_.clear(); }

    // Video does not produce audio in this stub; present only for timeline ownership.
    void render(/* intentionally empty */) {}

private:
    double   fps_    = 24.0;
    int64_t  length_ = 0;
    std::vector<Marker> markers_;
};

} // namespace hush::video
