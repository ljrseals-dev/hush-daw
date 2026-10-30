// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once
/**
 * HRIR cache keyed by exact speaker (azimuth, elevation).
 *
 * Design:
 *   - The cache is populated once, at construction, for every speaker
 *     in a given ChannelFormat. There is no lazy loading.
 *   - Lookup is exact. (30.0f, 0.0f) and (30.00001f, 0.0f) are
 *     different keys on purpose — callers pass the values from
 *     speakersFor(fmt), which are stable compile-time constants.
 *   - Misses throw. There is no silent fallback. If a caller asks
 *     for a direction that wasn't preloaded, that is a bug in the
 *     caller, not a runtime condition to paper over.
 *   - The cache owns the IR buffers. IRPair::left / right are
 *     non-owning views into cache-owned storage.
 */

#include "hush/immersive/channel_format.hpp"
#include "hush/immersive/speakers.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace hush {
namespace immersive {

/// Stereo HRIR for one direction. Samples are normalized to peak 1.0
/// so downstream gain math is uniform regardless of SOFA source.
struct IRPair {
  const float* left{nullptr};   // non-owning, stride 2 (interleaved L,R,...)
  const float* right{nullptr};  // non-owning, left + 1
  std::size_t length{0};        // samples per channel
  std::size_t preDelay{0};      // samples of leading silence
};

/// Thrown when the cache is asked for a direction it does not have.
class IRCacheMiss : public std::runtime_error {
 public:
  IRCacheMiss(float az, float el, const std::string& detail);
  float azimuth() const noexcept { return az_; }
  float elevation() const noexcept { return el_; }

 private:
  float az_;
  float el_;
};

/**
 * Key: exact (azimuth, elevation) in degrees, bit-identical.
 * Hash is a mix of the two raw bit patterns. Equal keys must be
 * bit-equal, which is guaranteed by the caller using the same
 * source constants every time.
 */
struct IRKey {
  float az{0.f};
  float el{0.f};
  bool operator==(const IRKey& o) const noexcept {
    std::uint32_t a1, a2, b1, b2;
    std::memcpy(&a1, &az, 4);
    std::memcpy(&a2, &el, 4);
    std::memcpy(&b1, &o.az, 4);
    std::memcpy(&b2, &o.el, 4);
    return a1 == b1 && a2 == b2;
  }
};

struct IRKeyHash {
  std::size_t operator()(const IRKey& k) const noexcept;
};

class HRIRCache {
 public:
  /**
   * @param sofaPath   Path to a SOFA file. If empty, the cache is
   *                   built from the built-in spherical-head model.
   * @param fmt        The speaker layout to preload. Every speaker
   *                   returned by speakersFor(fmt) is loaded.
   * @param sampleRate Must match the rate the renderer will run at.
   *                   A mismatch is a hard error, not a resample.
   * @throws std::runtime_error if sofaPath is non-empty and the
   *         file cannot be opened or does not contain IRs at the
   *         requested sample rate.
   */
  HRIRCache(const std::string& sofaPath, ChannelFormat fmt, double sampleRate);

  HRIRCache(const HRIRCache&) = delete;
  HRIRCache& operator=(const HRIRCache&) = delete;
  HRIRCache(HRIRCache&&) noexcept;
  HRIRCache& operator=(HRIRCache&&) noexcept;
  ~HRIRCache();

  /// Exact lookup. Throws IRCacheMiss if not present.
  IRPair get(float azimuth, float elevation) const;

  /// True if the exact key was preloaded. No throw.
  bool has(float azimuth, float elevation) const noexcept;

  /// Number of loaded IRs.
  std::size_t size() const noexcept;

  /// The layout this cache was built for.
  ChannelFormat format() const noexcept;

  /// True if IRs came from a SOFA file, false for the built-in model.
  bool usingSofa() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace immersive
}  // namespace hush
