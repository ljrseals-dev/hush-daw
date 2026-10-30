// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#pragma once

#include <string>
#include <vector>

namespace hush {
namespace immersive {

class BinauralRenderer {
public:
  // If sofaPath is non-empty and HUSH_HAS_LIBSOFA is defined, attempt to load
  // the SOFA file. On failure throws std::runtime_error.
  // Otherwise (or if SOFA is unavailable) the spherical-head model is used.
  explicit BinauralRenderer(const std::string& sofaPath = {});
  ~BinauralRenderer();

  BinauralRenderer(const BinauralRenderer&) = delete;
  BinauralRenderer& operator=(const BinauralRenderer&) = delete;

  // inInterleaved : numSpeakers * numFrames, speaker-major interleaved
  // outStereo     : 2 * numFrames, L/R interleaved
  void process(const float* inInterleaved, int numSpeakers, int numFrames,
               float* outStereo);

  bool usingSofa() const { return usingSofa_; }

  // Enable/disable FFT-based convolution (default true when SOFA is loaded).
  void setUseFftConvolution(bool enable) { useFft_ = enable; }
  bool useFftConvolution() const { return useFft_; }

private:
  bool usingSofa_{false};
  bool useFft_{true};
  void* sofaHandle_{nullptr};  // opaque mysofa handle when available
  int sampleRate_{48000};

  // Spherical-head IR cache: simple delay + 1-pole LPF per speaker
  struct SphericalIR {
    int delayL{0};
    int delayR{0};
    float lpStateL{0.f};
    float lpStateR{0.f};
    float lpCoeff{0.f};
  };
  std::vector<SphericalIR> sphericalCache_;

  // FFT convolution state (per-speaker IR spectra)
  struct FftIR {
    std::vector<float> irL;   // time-domain
    std::vector<float> irR;
    std::vector<float> reL, imL, reR, imR;  // frequency domain
    int fftSize{0};
  };
  std::vector<FftIR> fftCache_;

  void initSpherical(int numSpeakers, int sampleRate);
  void processSpherical(const float* in, int numSpeakers, int numFrames,
                        float* out);

  // Built-in radix-2 real FFT helpers (no external dependency)
  static void fft(std::vector<float>& re, std::vector<float>& im);
  static void ifft(std::vector<float>& re, std::vector<float>& im);
  static int nextPow2(int n);

  void processFftConvolution(const float* in, int numSpeakers, int numFrames,
                             float* out);

#ifdef HUSH_HAS_LIBSOFA
  void loadSofaIRs(int numSpeakers);
  void processSofa(const float* in, int numSpeakers, int numFrames, float* out);
#endif
};

}  // namespace immersive
}  // namespace hush
