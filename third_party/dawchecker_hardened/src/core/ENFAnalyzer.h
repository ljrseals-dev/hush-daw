#pragma once
// HARDENED FORK. Looks for a narrowband component near 50 Hz or 60 Hz (fundamental only) using 2-second Hann
// windows. Mains hum is common in ordinary recordings, so this detector never goes above Active.
#include <array>
#include "DetectorTypes.h"
#include "HannGoertzelBank.h"

namespace dawchecker::dsp {

class ENFAnalyzer
{
public:
    explicit ENFAnalyzer(double sampleRate = 48000.0, double sensitivity = 5.0);
    void prepare(double sampleRate, double sensitivity = 5.0);
    DetectorResult process(double liveSample) noexcept;   // real-time safe

private:
    static constexpr std::size_t kScan = 41, kRef = 4, kPerBase = kScan + kRef, kBases = 2;
    void evaluateBlock() noexcept;

    HannGoertzelBank<kBases * kPerBase> bank_;
    std::array<double, kBases> baseHz_{};
    std::array<double, kBases * kPerBase> freqs_{};
    bool valid_ = false;
    double marginDb_ = 15.0, alpha_ = 0.6;
    int hits_ = 0, misses_ = 0;
    DetectorResult result_{};
};

} // namespace dawchecker::dsp
