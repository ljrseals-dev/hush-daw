// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include <memory>
#include <vector>

namespace hush {
namespace immersive {

enum class RotationConvention {
  YPR_ZYX,  // intrinsic: yaw about Z, then pitch about Y, then roll about X
  YPR_XYZ   // extrinsic: roll about X, then pitch about Y, then yaw about Z
};

struct EulerAngles {
  float yaw{0.f};    // degrees
  float pitch{0.f};  // degrees
  float roll{0.f};   // degrees
  RotationConvention convention{RotationConvention::YPR_ZYX};
};

/// Order of the rotation: 1 (FOA, 4 ch), 2 (SOA, 9 ch), 3 (HOA3, 16 ch).
class Rotation {
 public:
  explicit Rotation(int order);  // 1, 2, or 3. Throws otherwise.
  ~Rotation();

  /// Set the rotation. Recomputes the internal block-diagonal matrix.
  void set(const EulerAngles& angles);

  int order() const noexcept;

  /// Rotate interleaved ACN/SN3D samples in place.
  /// channels == (order+1)^2.
  void apply(float* interleaved, int numFrames) const;

  /// Rotate a single frame.
  void applyFrame(const float* in, float* out) const;

  /// |matrix - I| < epsilon for every entry.
  bool isIdentity(float epsilon = 1e-6f) const;

  /// Row-major block for degree l, size (2l+1)x(2l+1).
  /// Throws if l < 0 or l > order().
  std::vector<float> block(int l) const;

  /// Full matrix, row-major, size N x N where N = (order+1)^2.
  const std::vector<float>& matrix() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/// Convenience: rotate an existing HOA buffer with one call.
void rotateHoa(float* interleaved, int numFrames, int order,
               const EulerAngles& angles);

/// Object panner that encodes a unit source then applies orientation.
void panObjectOriented(const EulerAngles& orientation, float gain, int order,
                       float* outInterleavedAcn, int numFrames);

}  // namespace immersive
}  // namespace hush
