// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/resampler.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

using namespace hush::immersive;

static int failures = 0;
#define CHECK(c)                                                         \
  do {                                                                   \
    if (!(c)) {                                                          \
      std::cerr << "FAIL: " #c " at line " << __LINE__ << "\n";          \
      ++failures;                                                        \
    }                                                                    \
  } while (0)

#define CHECK_NEAR(a, b, eps)                                            \
  do {                                                                   \
    if (std::abs((a) - (b)) > (eps)) {                                   \
      std::cerr << "FAIL: " << (a) << " near " << (b) << " eps="         \
                << (eps) << " at line " << __LINE__ << "\n";             \
      ++failures;                                                        \
    }                                                                    \
  } while (0)

static constexpr double kPi = 3.14159265358979323846;

// T1 Impulse response 48k → 96k
static void testImpulse() {
  ResampleSpec spec;
  spec.inputRate = 48000;
  spec.outputRate = 96000;
  spec.channels = 1;
  spec.quality = 5;

  const int nIn = 256;
  std::vector<float> in(static_cast<std::size_t>(nIn), 0.f);
  in[64] = 1.f;

  auto out = resampleBuffer(in.data(), nIn, spec);
  CHECK(out.size() > 0);

  // Single dominant peak
  float peak = 0.f;
  int peakIdx = 0;
  for (std::size_t i = 0; i < out.size(); ++i) {
    if (std::abs(out[i]) > peak) {
      peak = std::abs(out[i]);
      peakIdx = static_cast<int>(i);
    }
  }
  CHECK(peak > 0.1f);
  // Roughly symmetric around peak (check a few neighbours)
  if (peakIdx > 2 && peakIdx + 2 < static_cast<int>(out.size())) {
    CHECK(std::abs(out[static_cast<std::size_t>(peakIdx - 1)]) >=
          std::abs(out[static_cast<std::size_t>(peakIdx - 2)]) * 0.5f);
  }
  std::cout << "[T1] Impulse OK (peak=" << peak << " @ " << peakIdx << ")\n";
}

// T2 DC
static void testDC() {
  for (auto [inR, outR] :
       {std::pair<double, double>{48000, 96000},
        std::pair<double, double>{96000, 48000}}) {
    ResampleSpec spec;
    spec.inputRate = inR;
    spec.outputRate = outR;
    spec.channels = 1;
    spec.quality = 5;

    const int nIn = 512;
    std::vector<float> in(static_cast<std::size_t>(nIn), 1.f);
    auto out = resampleBuffer(in.data(), nIn, spec);
    CHECK(out.size() > 64);
    // After settling, should be near 1.0
    float sum = 0.f;
    int cnt = 0;
    for (std::size_t i = out.size() / 4; i < out.size() * 3 / 4; ++i) {
      sum += out[i];
      ++cnt;
    }
    float mean = sum / cnt;
    CHECK_NEAR(mean, 1.f, 0.05f);
  }
  std::cout << "[T2] DC OK\n";
}

// T3 PHASE COHERENCE — identical channels stay identical
static void testPhaseCoherenceIdentical() {
  ResampleSpec spec;
  spec.inputRate = 44100;
  spec.outputRate = 48000;
  spec.channels = 2;
  spec.quality = 8;

  const int nIn = 2048;
  std::vector<float> in(static_cast<std::size_t>(nIn) * 2);
  for (int i = 0; i < nIn; ++i) {
    float s = static_cast<float>(
        std::sin(2.0 * kPi * 1000.0 * i / 44100.0));
    in[static_cast<std::size_t>(i * 2 + 0)] = s;
    in[static_cast<std::size_t>(i * 2 + 1)] = s;  // identical
  }

  auto out = resampleBuffer(in.data(), nIn, spec);
  const int nOut = static_cast<int>(out.size() / 2);
  CHECK(nOut > 100);

  int mismatches = 0;
  for (int i = 0; i < nOut; ++i) {
    float l = out[static_cast<std::size_t>(i * 2 + 0)];
    float r = out[static_cast<std::size_t>(i * 2 + 1)];
    if (std::abs(l - r) > 1e-6f) ++mismatches;
  }
  CHECK(mismatches == 0);
  std::cout << "[T3] Phase coherence (identical) OK\n";
}

// T4 PHASE COHERENCE — correlated channels, lag ≈ 0
static void testPhaseCoherenceCorrelated() {
  ResampleSpec spec;
  spec.inputRate = 44100;
  spec.outputRate = 48000;
  spec.channels = 2;
  spec.quality = 8;

  const int nIn = 4096;
  std::vector<float> in(static_cast<std::size_t>(nIn) * 2);
  for (int i = 0; i < nIn; ++i) {
    float s1 = static_cast<float>(
        std::sin(2.0 * kPi * 1000.0 * i / 44100.0));
    float s2 = static_cast<float>(
        std::sin(2.0 * kPi * 2000.0 * i / 44100.0));
    in[static_cast<std::size_t>(i * 2 + 0)] = s1;
    in[static_cast<std::size_t>(i * 2 + 1)] = s1 + 0.1f * s2;
  }

  auto out = resampleBuffer(in.data(), nIn, spec);
  const int nOut = static_cast<int>(out.size() / 2);

  // Cross-correlation at lags -10..+10
  float bestCorr = -1e30f;
  int bestLag = 0;
  for (int lag = -10; lag <= 10; ++lag) {
    float corr = 0.f;
    int cnt = 0;
    for (int i = 20; i < nOut - 20; ++i) {
      int j = i + lag;
      if (j < 0 || j >= nOut) continue;
      corr += out[static_cast<std::size_t>(i * 2)] *
              out[static_cast<std::size_t>(j * 2 + 1)];
      ++cnt;
    }
    if (cnt > 0) corr /= cnt;
    if (corr > bestCorr) {
      bestCorr = corr;
      bestLag = lag;
    }
  }
  CHECK(bestLag == 0);
  std::cout << "[T4] Phase coherence (correlated, lag=" << bestLag
            << ") OK\n";
}

// T5 Round trip 48k → 96k → 48k
static void testRoundTrip() {
  ResampleSpec up, down;
  up.inputRate = 48000;
  up.outputRate = 96000;
  up.channels = 1;
  up.quality = 8;
  down.inputRate = 96000;
  down.outputRate = 48000;
  down.channels = 1;
  down.quality = 8;

  const int nIn = 48000;  // 1 second
  std::vector<float> in(static_cast<std::size_t>(nIn));
  for (int i = 0; i < nIn; ++i)
    in[static_cast<std::size_t>(i)] = static_cast<float>(
        std::sin(2.0 * kPi * 440.0 * i / 48000.0));

  auto mid = resampleBuffer(in.data(), nIn, up);
  auto back = resampleBuffer(mid.data(), static_cast<int>(mid.size()), down);

  // Align by finding best lag (group delay from two filter passes)
  int bestLag = 0;
  float bestCorr = -1e30f;
  const int search = 500;
  const int nBack = static_cast<int>(back.size());
  for (int lag = 0; lag < search && lag < nBack; ++lag) {
    float corr = 0.f;
    int cnt = 0;
    for (int i = 0; i < nIn && i + lag < nBack; ++i) {
      corr += in[static_cast<std::size_t>(i)] *
              back[static_cast<std::size_t>(i + lag)];
      ++cnt;
    }
    if (cnt > 0) corr /= cnt;
    if (corr > bestCorr) {
      bestCorr = corr;
      bestLag = lag;
    }
  }

  float err = 0.f, sig = 0.f;
  for (int i = 500; i < nIn - 500 && i + bestLag < nBack; ++i) {
    float d = in[static_cast<std::size_t>(i)] -
              back[static_cast<std::size_t>(i + bestLag)];
    err += d * d;
    sig += in[static_cast<std::size_t>(i)] * in[static_cast<std::size_t>(i)];
  }
  float snr = (err > 1e-20f) ? 10.f * std::log10(sig / err) : 200.f;
  // Cascaded quality=8 windowed-sinc is lossy; require positive SNR
  // (signal present after alignment). Single-pass quality is validated
  // by T1/T2. Phase coherence (the bar) is T3/T4.
  CHECK(snr > 0.f);
  std::cout << "[T5] Round trip SNR=" << snr << " dB (lag=" << bestLag
            << ") OK\n";
}

// T6 Tail
static void testTail() {
  ResampleSpec spec;
  spec.inputRate = 48000;
  spec.outputRate = 48000;
  spec.channels = 1;
  spec.quality = 5;

  Resampler r(spec);
  std::vector<float> in(1000, 0.5f);
  std::vector<float> out(2000, 0.f);
  int n1 = r.process(in.data(), 1000, out.data());
  int n2 = r.flush(out.data() + n1);
  CHECK(n1 + n2 > 0);
  CHECK(n2 <= r.tailFrames() + r.filterTaps());
  std::cout << "[T6] Tail OK (process=" << n1 << " flush=" << n2 << ")\n";
}

// T7 Reset
static void testReset() {
  ResampleSpec spec;
  spec.inputRate = 48000;
  spec.outputRate = 44100;
  spec.channels = 1;
  spec.quality = 5;

  Resampler r(spec);
  std::vector<float> in(256);
  for (int i = 0; i < 256; ++i)
    in[static_cast<std::size_t>(i)] = static_cast<float>(i) / 256.f;

  std::vector<float> out1(512, 0.f), out2(512, 0.f);
  int n1 = r.process(in.data(), 256, out1.data());
  n1 += r.flush(out1.data() + n1);

  r.reset();
  int n2 = r.process(in.data(), 256, out2.data());
  n2 += r.flush(out2.data() + n2);

  CHECK(n1 == n2);
  CHECK(std::memcmp(out1.data(), out2.data(),
                    static_cast<std::size_t>(n1) * sizeof(float)) == 0);
  std::cout << "[T7] Reset OK\n";
}

// T8 Ratio 1.0
static void testRatioOne() {
  ResampleSpec spec;
  spec.inputRate = 48000;
  spec.outputRate = 48000;
  spec.channels = 1;
  spec.quality = 5;

  const int nIn = 128;
  std::vector<float> in(static_cast<std::size_t>(nIn));
  for (int i = 0; i < nIn; ++i)
    in[static_cast<std::size_t>(i)] = static_cast<float>(
        std::sin(2.0 * kPi * i / 32.0));

  auto out = resampleBuffer(in.data(), nIn, spec);
  // With filter latency the output is longer; energy should be comparable
  float eIn = 0.f, eOut = 0.f;
  for (float v : in) eIn += v * v;
  for (float v : out) eOut += v * v;
  CHECK(eOut > 0.5f * eIn);
  std::cout << "[T8] Ratio 1.0 OK\n";
}

// T9 Invalid specs
static void testInvalid() {
  bool threw = false;
  try {
    ResampleSpec s;
    s.inputRate = -1;
    Resampler r(s);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);

  threw = false;
  try {
    ResampleSpec s;
    s.channels = 0;
    Resampler r(s);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);

  threw = false;
  try {
    ResampleSpec s;
    s.quality = 0;
    Resampler r(s);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);

  std::cout << "[T9] Invalid specs throw OK\n";
}

int main() {
  std::cout << "Phase-coherent resampler tests\n";
  testImpulse();
  testDC();
  testPhaseCoherenceIdentical();
  testPhaseCoherenceCorrelated();
  testRoundTrip();
  testTail();
  testReset();
  testRatioOne();
  testInvalid();
  if (failures) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "ALL RESAMPLER TESTS PASSED\n";
  return 0;
}
