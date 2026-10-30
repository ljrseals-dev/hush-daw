// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/hr_cache.hpp"
#include "hush/immersive/speakers.hpp"

#include <cmath>
#include <iostream>
#include <string>

using namespace hush::immersive;

static int failures = 0;
#define CHECK(c)                                                         \
  do {                                                                   \
    if (!(c)) {                                                          \
      std::cerr << "FAIL: " #c " at line " << __LINE__ << "\n";          \
      ++failures;                                                        \
    }                                                                    \
  } while (0)

static void test_size_matches_format() {
  HRIRCache c("", ChannelFormat::SevenPointOneFour, 48000.0);
  CHECK(c.size() == 12);
  CHECK(c.format() == ChannelFormat::SevenPointOneFour);
  CHECK(!c.usingSofa());
  std::cout << "[hr_cache] 7.1.4 size & format OK\n";
}

static void test_exact_lookup_every_speaker() {
  for (auto fmt : {ChannelFormat::Stereo, ChannelFormat::FivePointOne,
                   ChannelFormat::SevenPointOneFour,
                   ChannelFormat::NinePointOneSix}) {
    HRIRCache c("", fmt, 48000.0);
    for (const auto& s : speakersFor(fmt)) {
      if (s.isAmbisonic) continue;
      CHECK(c.has(s.azimuthDeg, s.elevationDeg));
      IRPair p = c.get(s.azimuthDeg, s.elevationDeg);
      CHECK(p.left != nullptr);
      CHECK(p.right != nullptr);
      CHECK(p.length > 0);
      CHECK(p.right == p.left + 1);
    }
  }
  std::cout << "[hr_cache] exact lookup for every speaker OK\n";
}

static void test_miss_throws() {
  HRIRCache c("", ChannelFormat::Stereo, 48000.0);
  bool threw = false;
  try {
    (void)c.get(123.456f, -78.9f);
  } catch (const IRCacheMiss& e) {
    threw = true;
    CHECK(std::abs(e.azimuth() - 123.456f) < 1e-6f);
    CHECK(std::abs(e.elevation() - (-78.9f)) < 1e-6f);
  }
  CHECK(threw);
  CHECK(!c.has(123.456f, -78.9f));
  std::cout << "[hr_cache] miss throws IRCacheMiss OK\n";
}

static void test_ir_peak_normalized() {
  HRIRCache c("", ChannelFormat::SevenPointOneFour, 48000.0);
  for (const auto& s : speakersFor(ChannelFormat::SevenPointOneFour)) {
    if (s.isAmbisonic || s.isLfe) continue;
    IRPair p = c.get(s.azimuthDeg, s.elevationDeg);
    float peak = 0.0f;
    for (std::size_t i = 0; i < p.length; ++i) {
      peak = std::max(peak, std::abs(p.left[i * 2]));
      peak = std::max(peak, std::abs(p.right[i * 2]));
    }
    CHECK(peak > 0.99f && peak < 1.01f);
  }
  std::cout << "[hr_cache] IR peak normalized OK\n";
}

static void test_itd_sign() {
  // At az=+90 the left ear should lead; at az=-90 the right ear.
  // NinePointOneSix has Ls at (90,0) and Rs at (-90,0).
  HRIRCache c("", ChannelFormat::NinePointOneSix, 48000.0);
  IRPair ls = c.get(90.0f, 0.0f);
  IRPair rs = c.get(-90.0f, 0.0f);

  auto energy = [](const float* p, std::size_t stride, std::size_t n) {
    float e = 0.0f;
    for (std::size_t i = 0; i < n; ++i)
      e += p[i * stride] * p[i * stride];
    return e;
  };

  // Left speaker: left-ear energy >= right-ear energy (near ear dominates).
  CHECK(energy(ls.left, 2, ls.length) >= energy(ls.right, 2, ls.length) * 0.5f);
  // Right speaker: mirror.
  CHECK(energy(rs.right, 2, rs.length) >= energy(rs.left, 2, rs.length) * 0.5f);
  std::cout << "[hr_cache] ITD sign OK\n";
}

static void test_sofa_without_libmysofa_throws() {
#ifndef HUSH_HAS_LIBSOFA
  bool threw = false;
  try {
    HRIRCache c("/tmp/nonexistent.sofa", ChannelFormat::Stereo, 48000.0);
  } catch (const std::runtime_error& e) {
    threw = true;
    CHECK(std::string(e.what()).find("libmysofa") != std::string::npos);
  }
  CHECK(threw);
  std::cout << "[hr_cache] SOFA-without-libmysofa throws OK\n";
#else
  std::cout << "[hr_cache] SOFA-without-libmysofa skipped (libmysofa present)\n";
#endif
}

static void test_stereo_has_lr() {
  HRIRCache c("", ChannelFormat::Stereo, 48000.0);
  CHECK(c.has(30.0f, 0.0f));
  CHECK(c.has(-30.0f, 0.0f));
  CHECK(c.size() == 2);
  std::cout << "[hr_cache] Stereo L/R positions OK\n";
}

int main() {
  std::cout << "HRIR cache tests\n";
  test_size_matches_format();
  test_exact_lookup_every_speaker();
  test_miss_throws();
  test_ir_peak_normalized();
  test_itd_sign();
  test_sofa_without_libmysofa_throws();
  test_stereo_has_lr();
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "ALL HRIR CACHE TESTS PASSED\n";
  return 0;
}
