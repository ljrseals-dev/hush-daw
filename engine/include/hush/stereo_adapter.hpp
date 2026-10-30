#pragma once
#include <cstddef>
#include <vector>

namespace hush {

// Control-thread setup: preallocate planar scratch for interleaved stereo.
// deinterleave / interleave: no alloc, no I/O (audio-thread safe).
class StereoScratch {
public:
  void configure(std::size_t max_frames) {
    left_.assign(max_frames, 0.f);
    right_.assign(max_frames, 0.f);
    max_frames_ = max_frames;
  }

  [[nodiscard]] std::size_t max_frames() const noexcept { return max_frames_; }
  [[nodiscard]] float* left() noexcept { return left_.data(); }
  [[nodiscard]] float* right() noexcept { return right_.data(); }
  [[nodiscard]] const float* left() const noexcept { return left_.data(); }
  [[nodiscard]] const float* right() const noexcept { return right_.data(); }

  // interleaved LRLR… → planar; returns false if frames > capacity or null
  [[nodiscard]] bool deinterleave(const float* interleaved, std::size_t frames) noexcept {
    if (!interleaved || frames > max_frames_) return false;
    for (std::size_t i = 0; i < frames; ++i) {
      left_[i] = interleaved[i * 2];
      right_[i] = interleaved[i * 2 + 1];
    }
    return true;
  }

  [[nodiscard]] bool interleave(float* interleaved, std::size_t frames) const noexcept {
    if (!interleaved || frames > max_frames_) return false;
    for (std::size_t i = 0; i < frames; ++i) {
      interleaved[i * 2] = left_[i];
      interleaved[i * 2 + 1] = right_[i];
    }
    return true;
  }

  void clear(std::size_t frames) noexcept {
    if (frames > max_frames_) frames = max_frames_;
    for (std::size_t i = 0; i < frames; ++i) {
      left_[i] = 0.f;
      right_[i] = 0.f;
    }
  }

private:
  std::vector<float> left_, right_;
  std::size_t max_frames_{0};
};

}  // namespace hush
