// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/panner.hpp"
#include "hush/immersive/ambisonics.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace hush {
namespace immersive {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.f;
constexpr float kMinDistanceGain = 0.001f;  // 10^(-60/20)

float distanceGain(float distance) {
  if (distance <= 0.f) return 1.f;
  const float d2 = distance * distance;
  const float g = 1.f / std::max(1.f, d2);
  return std::max(g, kMinDistanceGain);
}

std::vector<float> vbapGains(float azDeg, float elDeg,
                             const std::vector<Speaker>& speakers,
                             float divergence, float size, bool isObject) {
  const float az = azDeg * kDeg2Rad;
  const float el = elDeg * kDeg2Rad;
  const float cosEl = std::cos(el);
  const float sx = cosEl * std::sin(az);
  const float sy = cosEl * std::cos(az);
  const float sz = std::sin(el);

  float effectiveDiv = divergence;
  if (isObject) {
    effectiveDiv = std::max(divergence, size * 0.5f);
  }

  const int n = static_cast<int>(speakers.size());
  std::vector<float> gains(static_cast<std::size_t>(n), 0.f);
  float sumSq = 0.f;

  for (int i = 0; i < n; ++i) {
    const Speaker& sp = speakers[static_cast<std::size_t>(i)];
    if (sp.isLfe || sp.isAmbisonic) {
      gains[static_cast<std::size_t>(i)] = 0.f;
      continue;
    }
    float cosAng = sx * sp.x + sy * sp.y + sz * sp.z;
    cosAng = std::max(0.f, cosAng);

    if (effectiveDiv > 0.f) {
      const float equal = 1.f / static_cast<float>(n);
      cosAng = cosAng * (1.f - effectiveDiv) + equal * effectiveDiv;
    }

    gains[static_cast<std::size_t>(i)] = cosAng;
    sumSq += cosAng * cosAng;
  }

  if (sumSq > 1e-12f) {
    const float inv = 1.f / std::sqrt(sumSq);
    for (float& g : gains) g *= inv;
  }
  return gains;
}

}  // namespace

std::vector<float> pan(const PannerState& state, ChannelFormat format) {
  if (format == ChannelFormat::AmbisonicsFOA ||
      format == ChannelFormat::AmbisonicsSOA ||
      format == ChannelFormat::AmbisonicsHOA3) {
    return encodeAmbisonics(state.azimuthDeg, state.elevationDeg, format);
  }

  const auto& speakers = speakersFor(format);
  const int n = static_cast<int>(speakers.size());

  float azEff = state.azimuthDeg * (1.f - state.screenAnchor);

  auto gains = vbapGains(azEff, state.elevationDeg, speakers,
                         state.divergence, state.size, state.isObject);

  float distG = 1.f;
  if (state.isObject) {
    distG = distanceGain(state.distance);
  }

  for (float& g : gains) {
    g *= state.gain * distG;
  }

  for (int i = 0; i < n; ++i) {
    if (speakers[static_cast<std::size_t>(i)].isLfe) {
      gains[static_cast<std::size_t>(i)] = state.gain * distG * 0.1f;
    }
  }

  return gains;
}

std::vector<float> pan(float azimuthDeg, float elevationDeg, float divergence,
                       float size, float gain, ChannelFormat format,
                       bool isObject) {
  PannerState st;
  st.azimuthDeg = azimuthDeg;
  st.elevationDeg = elevationDeg;
  st.divergence = divergence;
  st.size = size;
  st.gain = gain;
  st.isObject = isObject;
  st.distance = 0.f;
  st.screenAnchor = 0.f;
  return pan(st, format);
}

}  // namespace immersive
}  // namespace hush
