#pragma once
// HARDENED FORK. Detects a sustained narrowband tone in the 17-20 kHz region. It cannot tell a "beacon" from any
// other steady tone (e.g. a test tone or a CRT whine); it is an indicator only.
#include <array>
#include "DetectorTypes.h"
#include "HannGoertzelBank.h"

namespace dawchecker::dsp {

class UltrasonicScanner
{
public:
    explicit UltrasonicScanner(double sampleRate = 48000.0, double sensitivity = 5.0);
    void prepare(double sampleRate, double sensitivity = 5.0);
    DetectorResult process(double liveSample) noexcept;   // real-time safe: no allocation, locks, or I/O

private:
    static constexpr std::size_t kBands = 5, kPerBand = 5;   // centre + 4 reference bins (+/-400 Hz, +/-800 Hz)
    void evaluateBlock() noexcept;

    HannGoertzelBank<kBands * kPerBand> bank_;
    std::array<bool, kBands> valid_{};
    std::array<double, kBands> bandHz_{};
    bool anyValid_ = false;
    double marginDb_ = 20.0, alpha_ = 0.3;
    int hits_ = 0, misses_ = 0, activeBlocks_ = 3, alertBlocks_ = 12;
    DetectorResult result_{};
};

} // namespace dawchecker::dsp
