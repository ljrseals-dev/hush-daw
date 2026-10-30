#pragma once
#include <cstddef>
#include <cstring>
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

  [[nodiscard]] bool deinterleave(const float* interleaved, std::size_t frames) noexcept {
    if (!interleaved || frames > max_frames_) return false;
    float* __restrict L = left_.data();
    float* __restrict R = right_.data();
    const float* __restrict in = interleaved;
    // Process 4 frames at a time when possible
    std::size_t i = 0;
    for (; i + 4 <= frames; i += 4) {
      L[i] = in[0]; R[i] = in[1];
      L[i + 1] = in[2]; R[i + 1] = in[3];
      L[i + 2] = in[4]; R[i + 2] = in[5];
      L[i + 3] = in[6]; R[i + 3] = in[7];
      in += 8;
    }
    for (; i < frames; ++i) {
      L[i] = *in++;
      R[i] = *in++;
    }
    return true;
  }

  [[nodiscard]] bool interleave(float* interleaved, std::size_t frames) const noexcept {
    if (!interleaved || frames > max_frames_) return false;
    const float* __restrict L = left_.data();
    const float* __restrict R = right_.data();
    float* __restrict out = interleaved;
    std::size_t i = 0;
    for (; i + 4 <= frames; i += 4) {
      out[0] = L[i]; out[1] = R[i];
      out[2] = L[i + 1]; out[3] = R[i + 1];
      out[4] = L[i + 2]; out[5] = R[i + 2];
      out[6] = L[i + 3]; out[7] = R[i + 3];
      out += 8;
    }
    for (; i < frames; ++i) {
      *out++ = L[i];
      *out++ = R[i];
    }
    return true;
  }

  void clear(std::size_t frames) noexcept {
    if (frames > max_frames_) frames = max_frames_;
    if (frames == 0) return;
    std::memset(left_.data(), 0, frames * sizeof(float));
    std::memset(right_.data(), 0, frames * sizeof(float));
  }

private:
  std::vector<float> left_, right_;
  std::size_t max_frames_{0};
};

}  // namespace hush
