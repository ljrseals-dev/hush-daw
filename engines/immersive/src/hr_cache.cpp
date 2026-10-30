// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/hr_cache.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#if defined(HUSH_HAS_LIBSOFA)
extern "C" {
#include <mysofa.h>
}
#endif

namespace hush {
namespace immersive {

namespace {

constexpr float kPi = 3.14159265358979323846f;

// ---- Built-in spherical-head HRIR (fallback when no SOFA) ----------
//
// Woodworth-style ITD plus a first-order head-shadow LPF. This is a
// deliberately simple model — it is not a substitute for a measured
// HRTF, but it is monotonic, correct in sign, and has no ringing.
// Documented in the README as "approximate binaural".

std::vector<float> makeSphericalIR(float az, float el, double sampleRate,
                                   std::size_t& preDelayOut) {
  constexpr float kHeadRadius = 0.0875f;   // metres
  constexpr float kSpeedOfSound = 343.0f;  // m/s
  constexpr float kMaxITD = 2.0f * kHeadRadius / kSpeedOfSound;  // ~510 µs
  const float azRad = az * kPi / 180.0f;
  const float elRad = el * kPi / 180.0f;

  const float sinAz = std::sin(azRad);
  const float cosEl = std::cos(elRad);
  const float itd = kMaxITD * sinAz * cosEl;  // seconds, signed

  const auto n = static_cast<std::size_t>(
      std::ceil((kMaxITD + 0.002f) * sampleRate));
  const auto delay = static_cast<std::size_t>(std::abs(itd) * sampleRate);

  const float shadow = std::abs(sinAz);
  const float a = std::clamp(0.15f + 0.75f * shadow, 0.05f, 0.95f);

  const std::size_t leftDelay = (sinAz >= 0.0f) ? 0u : delay;
  const std::size_t rightDelay = (sinAz >= 0.0f) ? delay : 0u;

  auto place = [&](std::vector<float>& ir, std::size_t start) {
    if (start + 1 < ir.size()) {
      ir[start] = 1.0f - a * 0.5f;
      ir[start + 1] = a * 0.5f;
    } else if (start < ir.size()) {
      ir[start] = 1.0f;
    }
  };

  std::vector<float> left(n, 0.0f), right(n, 0.0f);
  place(left, leftDelay);
  place(right, rightDelay);

  std::vector<float> out(2 * n, 0.0f);
  for (std::size_t i = 0; i < n; ++i) {
    out[2 * i] = left[i];
    out[2 * i + 1] = right[i];
  }
  preDelayOut = 0;
  return out;
}

void normalizePeak(std::vector<float>& ir) {
  float peak = 0.0f;
  for (float s : ir) peak = std::max(peak, std::abs(s));
  if (peak > 1e-9f) {
    const float inv = 1.0f / peak;
    for (float& s : ir) s *= inv;
  }
}

}  // namespace

// ---- IRCacheMiss ---------------------------------------------------

IRCacheMiss::IRCacheMiss(float az, float el, const std::string& detail)
    : std::runtime_error([&] {
        std::ostringstream os;
        os << "HRIR cache miss at (az=" << az << ", el=" << el << "): "
           << detail;
        return os.str();
      }()),
      az_(az),
      el_(el) {}

// ---- IRKeyHash -----------------------------------------------------

std::size_t IRKeyHash::operator()(const IRKey& k) const noexcept {
  std::uint32_t a, b;
  std::memcpy(&a, &k.az, 4);
  std::memcpy(&b, &k.el, 4);
  std::uint64_t h = (static_cast<std::uint64_t>(a) << 32) | b;
  h ^= h >> 33;
  h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33;
  h *= 0xc4ceb9fe1a85ec53ULL;
  h ^= h >> 33;
  return static_cast<std::size_t>(h);
}

// ---- Impl ----------------------------------------------------------

struct HRIRCache::Impl {
  struct Entry {
    std::vector<float> interleaved;  // L,R,L,R,...
    std::size_t length{0};
    std::size_t preDelay{0};
  };

  std::unordered_map<IRKey, Entry, IRKeyHash> map_;
  ChannelFormat fmt_{ChannelFormat::Stereo};
  bool sofa_{false};
};

// ---- HRIRCache -----------------------------------------------------

HRIRCache::HRIRCache(const std::string& sofaPath, ChannelFormat fmt,
                     double sampleRate)
    : impl_(std::make_unique<Impl>()) {
  impl_->fmt_ = fmt;

  const auto& spks = speakersFor(fmt);
  if (spks.empty())
    throw std::invalid_argument("HRIRCache: format has no speakers");

  const bool hasSofa = !sofaPath.empty();

#if defined(HUSH_HAS_LIBSOFA)
  struct MySofaHandle {
    MYSOFA_EASY* h{nullptr};
    ~MySofaHandle() {
      if (h) mysofa_close(h);
    }
  } sofa;

  if (hasSofa) {
    int err = MYSOFA_OK;
    sofa.h = mysofa_open(sofaPath.c_str(), static_cast<float>(sampleRate),
                         &err);
    if (!sofa.h || err != MYSOFA_OK) {
      std::ostringstream os;
      os << "SOFA open failed (code " << err << ") for " << sofaPath;
      throw std::runtime_error(os.str());
    }
    impl_->sofa_ = true;
  }
#else
  if (hasSofa) {
    throw std::runtime_error(
        "HRIRCache: SOFA path given but build has no libmysofa. "
        "Rebuild with libmysofa, or pass an empty path to use the "
        "built-in spherical-head model.");
  }
#endif

  impl_->map_.reserve(spks.size());

  for (const auto& s : spks) {
    // Ambisonic logical channels have no physical direction; skip
    // with a zero IR so size() still matches speaker count.
    if (s.isAmbisonic) {
      Impl::Entry e;
      e.length = 1;
      e.preDelay = 0;
      e.interleaved = {0.f, 0.f};
      IRKey key{s.azimuthDeg, s.elevationDeg};
      auto [it, ok] = impl_->map_.emplace(key, std::move(e));
      (void)ok;
      (void)it;
      continue;
    }

    Impl::Entry e;

#if defined(HUSH_HAS_LIBSOFA)
    if (sofa.h) {
      // Fixed-size scratch; real SOFA IRs are typically a few hundred
      // samples. We query length first via a large buffer.
      constexpr int kMaxTaps = 2048;
      std::vector<float> left(kMaxTaps), right(kMaxTaps);
      float delayLeft = 0.f, delayRight = 0.f;
      // mysofa_getfilter_float(easy, x, y, z, left, right, &delayL, &delayR)
      // Coordinate convention: x=front, y=left, z=up (OpenGL-ish).
      // Convert from our (azDeg, elDeg) with az=0 front, +az left.
      const float azRad = s.azimuthDeg * kPi / 180.f;
      const float elRad = s.elevationDeg * kPi / 180.f;
      const float cosEl = std::cos(elRad);
      const float x = cosEl * std::cos(azRad);   // front
      const float y = cosEl * std::sin(azRad);   // left
      const float z = std::sin(elRad);           // up
      int n = 0;
      mysofa_getfilter_float(sofa.h, x, y, z, left.data(), right.data(),
                             &delayLeft, &delayRight);
      // libmysofa writes filter length into the internal structure;
      // the return is void in some versions. Use a conservative length
      // from the non-zero tail.
      n = kMaxTaps;
      while (n > 1 && left[n - 1] == 0.f && right[n - 1] == 0.f) --n;
      if (n <= 0) {
        std::ostringstream os;
        os << "SOFA returned empty IR for speaker " << s.id << " at ("
           << s.azimuthDeg << "," << s.elevationDeg << ")";
        throw std::runtime_error(os.str());
      }
      e.length = static_cast<std::size_t>(n);
      e.preDelay = 0;
      e.interleaved.resize(2 * e.length);
      for (std::size_t i = 0; i < e.length; ++i) {
        e.interleaved[2 * i] = left[i];
        e.interleaved[2 * i + 1] = right[i];
      }
      normalizePeak(e.interleaved);
    } else
#endif
    {
      std::size_t preDelay = 0;
      e.interleaved =
          makeSphericalIR(s.azimuthDeg, s.elevationDeg, sampleRate, preDelay);
      e.length = e.interleaved.size() / 2;
      e.preDelay = preDelay;
      normalizePeak(e.interleaved);
    }

    IRKey key{s.azimuthDeg, s.elevationDeg};
    if (impl_->map_.count(key)) {
      // Ambisonic channels all sit at (0,0); allow the first, skip rest.
      continue;
    }
    auto [it, ok] = impl_->map_.emplace(key, std::move(e));
    (void)ok;
    (void)it;
  }
}

HRIRCache::~HRIRCache() = default;
HRIRCache::HRIRCache(HRIRCache&&) noexcept = default;
HRIRCache& HRIRCache::operator=(HRIRCache&&) noexcept = default;

IRPair HRIRCache::get(float azimuth, float elevation) const {
  IRKey key{azimuth, elevation};
  auto it = impl_->map_.find(key);
  if (it == impl_->map_.end())
    throw IRCacheMiss(azimuth, elevation,
                      "not preloaded at construction; check the (az,el) "
                      "you passed matches the speaker position exactly");
  IRPair p;
  p.left = it->second.interleaved.data();
  p.right = it->second.interleaved.data() + 1;
  p.length = it->second.length;
  p.preDelay = it->second.preDelay;
  return p;
}

bool HRIRCache::has(float azimuth, float elevation) const noexcept {
  return impl_->map_.count(IRKey{azimuth, elevation}) != 0;
}

std::size_t HRIRCache::size() const noexcept { return impl_->map_.size(); }

ChannelFormat HRIRCache::format() const noexcept { return impl_->fmt_; }

bool HRIRCache::usingSofa() const noexcept { return impl_->sofa_; }

}  // namespace immersive
}  // namespace hush
