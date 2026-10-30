// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/binaural.hpp"
#include "hush/immersive/speakers.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

#ifdef HUSH_HAS_LIBSOFA
#include <mysofa.h>
#endif

namespace hush {
namespace immersive {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kSpeedOfSound = 343.f;
constexpr float kHeadRadius = 0.0875f;
constexpr float kSampleRateDefault = 48000.f;

float woodworthItd(float azRad) {
  const float theta = std::abs(azRad);
  return (kHeadRadius / kSpeedOfSound) * (theta + std::sin(theta));
}

float headShadowCoeff(float azRad, float sampleRate) {
  const float contralateral = std::max(0.f, std::sin(azRad));
  const float fc = 1500.f + (1.f - contralateral) * 4000.f;
  return std::exp(-2.f * kPi * fc / sampleRate);
}

}  // namespace

int BinauralRenderer::nextPow2(int n) {
  int p = 1;
  while (p < n) p <<= 1;
  return p;
}

void BinauralRenderer::fft(std::vector<float>& re, std::vector<float>& im) {
  const int n = static_cast<int>(re.size());
  // Bit-reversal
  for (int i = 1, j = 0; i < n; ++i) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) {
      std::swap(re[static_cast<std::size_t>(i)], re[static_cast<std::size_t>(j)]);
      std::swap(im[static_cast<std::size_t>(i)], im[static_cast<std::size_t>(j)]);
    }
  }
  for (int len = 2; len <= n; len <<= 1) {
    const float ang = -2.f * kPi / len;
    const float wlenRe = std::cos(ang), wlenIm = std::sin(ang);
    for (int i = 0; i < n; i += len) {
      float wRe = 1.f, wIm = 0.f;
      for (int j = 0; j < len / 2; ++j) {
        const int u = i + j, v = i + j + len / 2;
        const float tRe = wRe * re[static_cast<std::size_t>(v)] -
                          wIm * im[static_cast<std::size_t>(v)];
        const float tIm = wRe * im[static_cast<std::size_t>(v)] +
                          wIm * re[static_cast<std::size_t>(v)];
        re[static_cast<std::size_t>(v)] = re[static_cast<std::size_t>(u)] - tRe;
        im[static_cast<std::size_t>(v)] = im[static_cast<std::size_t>(u)] - tIm;
        re[static_cast<std::size_t>(u)] += tRe;
        im[static_cast<std::size_t>(u)] += tIm;
        const float nextWRe = wRe * wlenRe - wIm * wlenIm;
        wIm = wRe * wlenIm + wIm * wlenRe;
        wRe = nextWRe;
      }
    }
  }
}

void BinauralRenderer::ifft(std::vector<float>& re, std::vector<float>& im) {
  for (float& v : im) v = -v;
  fft(re, im);
  for (float& v : im) v = -v;
  const float inv = 1.f / static_cast<float>(re.size());
  for (std::size_t i = 0; i < re.size(); ++i) {
    re[i] *= inv;
    im[i] *= inv;
  }
}

BinauralRenderer::BinauralRenderer(const std::string& sofaPath) {
#ifdef HUSH_HAS_LIBSOFA
  if (!sofaPath.empty()) {
    int err = 0;
    sofaHandle_ = mysofa_open(sofaPath.c_str(), kSampleRateDefault, &err);
    if (!sofaHandle_ || err != 0) {
      throw std::runtime_error("SOFA load failed: " + sofaPath);
    }
    usingSofa_ = true;
    useFft_ = true;
    return;
  }
#else
  (void)sofaPath;
#endif
  usingSofa_ = false;
  useFft_ = false;
}

BinauralRenderer::~BinauralRenderer() {
#ifdef HUSH_HAS_LIBSOFA
  if (sofaHandle_) {
    mysofa_close(static_cast<MYSOFA_EASY*>(sofaHandle_));
    sofaHandle_ = nullptr;
  }
#endif
}

void BinauralRenderer::initSpherical(int numSpeakers, int sampleRate) {
  sampleRate_ = sampleRate;
  sphericalCache_.resize(static_cast<std::size_t>(numSpeakers));
  for (int i = 0; i < numSpeakers; ++i) {
    float azDeg = 0.f;
    if (numSpeakers == 2) {
      azDeg = (i == 0) ? 30.f : -30.f;
    } else if (numSpeakers >= 6) {
      static const float az[] = {30.f, -30.f, 0.f, 0.f, 90.f, -90.f,
                                 150.f, -150.f, 45.f, -45.f, 135.f, -135.f};
      azDeg = az[i % 12];
    } else {
      azDeg = 180.f * (2.f * i / static_cast<float>(numSpeakers) - 1.f);
    }
    const float azRad = azDeg * kPi / 180.f;
    const float itd = woodworthItd(azRad);
    const int delaySamples = static_cast<int>(std::round(itd * sampleRate_));
    SphericalIR& ir = sphericalCache_[static_cast<std::size_t>(i)];
    if (azRad >= 0.f) {
      ir.delayL = 0;
      ir.delayR = delaySamples;
    } else {
      ir.delayL = delaySamples;
      ir.delayR = 0;
    }
    ir.lpCoeff = headShadowCoeff(azRad, static_cast<float>(sampleRate_));
    ir.lpStateL = 0.f;
    ir.lpStateR = 0.f;
  }
}

void BinauralRenderer::processSpherical(const float* in, int numSpeakers,
                                        int numFrames, float* out) {
  if (sphericalCache_.size() != static_cast<std::size_t>(numSpeakers)) {
    initSpherical(numSpeakers, sampleRate_);
  }

  std::memset(out, 0, static_cast<std::size_t>(numFrames) * 2 * sizeof(float));

  constexpr int kMaxDelay = 256;
  std::vector<float> delayBufL(static_cast<std::size_t>(kMaxDelay), 0.f);
  std::vector<float> delayBufR(static_cast<std::size_t>(kMaxDelay), 0.f);
  int writePos = 0;

  for (int f = 0; f < numFrames; ++f) {
    float left = 0.f;
    float right = 0.f;

    for (int s = 0; s < numSpeakers; ++s) {
      const float sample = in[f * numSpeakers + s];
      SphericalIR& ir = sphericalCache_[static_cast<std::size_t>(s)];

      delayBufL[static_cast<std::size_t>(writePos)] = sample;
      delayBufR[static_cast<std::size_t>(writePos)] = sample;

      int readL = (writePos - ir.delayL + kMaxDelay) % kMaxDelay;
      int readR = (writePos - ir.delayR + kMaxDelay) % kMaxDelay;
      float sL = delayBufL[static_cast<std::size_t>(readL)];
      float sR = delayBufR[static_cast<std::size_t>(readR)];

      ir.lpStateL = ir.lpCoeff * ir.lpStateL + (1.f - ir.lpCoeff) * sL;
      ir.lpStateR = ir.lpCoeff * ir.lpStateR + (1.f - ir.lpCoeff) * sR;

      if (ir.delayR > ir.delayL) {
        left  += sL;
        right += ir.lpStateR;
      } else {
        left  += ir.lpStateL;
        right += sR;
      }
    }

    out[f * 2 + 0] = left;
    out[f * 2 + 1] = right;
    writePos = (writePos + 1) % kMaxDelay;
  }
}

void BinauralRenderer::processFftConvolution(const float* in, int numSpeakers,
                                             int numFrames, float* out) {
  // Expect fftCache_ already populated with IR spectra.
  if (fftCache_.size() != static_cast<std::size_t>(numSpeakers)) {
    processSpherical(in, numSpeakers, numFrames, out);
    return;
  }

  std::memset(out, 0, static_cast<std::size_t>(numFrames) * 2 * sizeof(float));

  for (int s = 0; s < numSpeakers; ++s) {
    FftIR& ir = fftCache_[static_cast<std::size_t>(s)];
    const int fftSize = ir.fftSize;
    if (fftSize <= 0) continue;

    // Overlap-add block processing
    const int hop = fftSize / 2;
    std::vector<float> block(static_cast<std::size_t>(fftSize), 0.f);
    std::vector<float> re(static_cast<std::size_t>(fftSize)), im(static_cast<std::size_t>(fftSize));
    std::vector<float> overlapL(static_cast<std::size_t>(fftSize), 0.f);
    std::vector<float> overlapR(static_cast<std::size_t>(fftSize), 0.f);

    for (int pos = 0; pos < numFrames; pos += hop) {
      const int n = std::min(hop, numFrames - pos);
      std::fill(block.begin(), block.end(), 0.f);
      for (int i = 0; i < n; ++i) {
        block[static_cast<std::size_t>(i)] = in[(pos + i) * numSpeakers + s];
      }

      // FFT of input block
      re = block;
      std::fill(im.begin(), im.end(), 0.f);
      fft(re, im);

      // Complex multiply with IR spectra
      std::vector<float> outReL(static_cast<std::size_t>(fftSize)), outImL(static_cast<std::size_t>(fftSize));
      std::vector<float> outReR(static_cast<std::size_t>(fftSize)), outImR(static_cast<std::size_t>(fftSize));
      for (int k = 0; k < fftSize; ++k) {
        outReL[static_cast<std::size_t>(k)] =
            re[static_cast<std::size_t>(k)] * ir.reL[static_cast<std::size_t>(k)] -
            im[static_cast<std::size_t>(k)] * ir.imL[static_cast<std::size_t>(k)];
        outImL[static_cast<std::size_t>(k)] =
            re[static_cast<std::size_t>(k)] * ir.imL[static_cast<std::size_t>(k)] +
            im[static_cast<std::size_t>(k)] * ir.reL[static_cast<std::size_t>(k)];
        outReR[static_cast<std::size_t>(k)] =
            re[static_cast<std::size_t>(k)] * ir.reR[static_cast<std::size_t>(k)] -
            im[static_cast<std::size_t>(k)] * ir.imR[static_cast<std::size_t>(k)];
        outImR[static_cast<std::size_t>(k)] =
            re[static_cast<std::size_t>(k)] * ir.imR[static_cast<std::size_t>(k)] +
            im[static_cast<std::size_t>(k)] * ir.reR[static_cast<std::size_t>(k)];
      }

      ifft(outReL, outImL);
      ifft(outReR, outImR);

      // Overlap-add
      for (int i = 0; i < fftSize; ++i) {
        overlapL[static_cast<std::size_t>(i)] += outReL[static_cast<std::size_t>(i)];
        overlapR[static_cast<std::size_t>(i)] += outReR[static_cast<std::size_t>(i)];
      }
      for (int i = 0; i < n; ++i) {
        out[(pos + i) * 2 + 0] += overlapL[static_cast<std::size_t>(i)];
        out[(pos + i) * 2 + 1] += overlapR[static_cast<std::size_t>(i)];
      }
      // Shift overlap
      std::copy(overlapL.begin() + hop, overlapL.end(), overlapL.begin());
      std::fill(overlapL.begin() + hop, overlapL.end(), 0.f);
      std::copy(overlapR.begin() + hop, overlapR.end(), overlapR.begin());
      std::fill(overlapR.begin() + hop, overlapR.end(), 0.f);
    }
  }
}

#ifdef HUSH_HAS_LIBSOFA
void BinauralRenderer::loadSofaIRs(int numSpeakers) {
  auto* easy = static_cast<MYSOFA_EASY*>(sofaHandle_);
  fftCache_.resize(static_cast<std::size_t>(numSpeakers));
  for (int s = 0; s < numSpeakers; ++s) {
    float az = (numSpeakers == 2) ? ((s == 0) ? 30.f : -30.f) : (30.f - 60.f * s);
    float el = 0.f;
    std::vector<float> irL(512), irR(512);
    int filterLen = 0;
    float dummyDelay = 0.f;
    mysofa_getfilter_float(easy, az, el, 0.f, irL.data(), irR.data(),
                           &filterLen, &dummyDelay);
    if (filterLen <= 0) filterLen = 1;

    FftIR& ir = fftCache_[static_cast<std::size_t>(s)];
    ir.irL.assign(irL.begin(), irL.begin() + filterLen);
    ir.irR.assign(irR.begin(), irR.begin() + filterLen);
    ir.fftSize = nextPow2(filterLen * 2);
    ir.reL.assign(static_cast<std::size_t>(ir.fftSize), 0.f);
    ir.imL.assign(static_cast<std::size_t>(ir.fftSize), 0.f);
    ir.reR.assign(static_cast<std::size_t>(ir.fftSize), 0.f);
    ir.imR.assign(static_cast<std::size_t>(ir.fftSize), 0.f);
    for (int i = 0; i < filterLen; ++i) {
      ir.reL[static_cast<std::size_t>(i)] = ir.irL[static_cast<std::size_t>(i)];
      ir.reR[static_cast<std::size_t>(i)] = ir.irR[static_cast<std::size_t>(i)];
    }
    fft(ir.reL, ir.imL);
    fft(ir.reR, ir.imR);
  }
}

void BinauralRenderer::processSofa(const float* in, int numSpeakers,
                                   int numFrames, float* out) {
  if (fftCache_.empty()) {
    loadSofaIRs(numSpeakers);
  }
  if (useFft_) {
    processFftConvolution(in, numSpeakers, numFrames, out);
  } else {
    // Fallback time-domain (short IR)
    std::memset(out, 0, static_cast<std::size_t>(numFrames) * 2 * sizeof(float));
    for (int s = 0; s < numSpeakers; ++s) {
      const FftIR& ir = fftCache_[static_cast<std::size_t>(s)];
      const int len = static_cast<int>(ir.irL.size());
      for (int f = 0; f < numFrames; ++f) {
        float accL = 0.f, accR = 0.f;
        const int maxT = std::min(len, numFrames - f);
        for (int t = 0; t < maxT; ++t) {
          const float smp = in[(f + t) * numSpeakers + s];
          accL += smp * ir.irL[static_cast<std::size_t>(t)];
          accR += smp * ir.irR[static_cast<std::size_t>(t)];
        }
        out[f * 2 + 0] += accL;
        out[f * 2 + 1] += accR;
      }
    }
  }
}
#endif

void BinauralRenderer::process(const float* inInterleaved, int numSpeakers,
                               int numFrames, float* outStereo) {
  if (!inInterleaved || !outStereo || numSpeakers <= 0 || numFrames <= 0) {
    throw std::invalid_argument("BinauralRenderer::process invalid arguments");
  }

#ifdef HUSH_HAS_LIBSOFA
  if (usingSofa_) {
    processSofa(inInterleaved, numSpeakers, numFrames, outStereo);
    return;
  }
#endif
  processSpherical(inInterleaved, numSpeakers, numFrames, outStereo);
}

}  // namespace immersive
}  // namespace hush
