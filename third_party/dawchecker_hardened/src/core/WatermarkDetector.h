#pragma once
// HARDENED FORK. Reports how large the difference (live - reference) is relative to the reference, and whether
// it is persistent. It does NOT identify a watermark: lossy coding, gain, dither or mixing differences also
// produce low-level residuals. Capped at Active. Argument order is (live, reference), same as the other detectors.
#include <cstddef>
#include "DetectorTypes.h"
#include "RunningWindow.h"

namespace dawchecker::dsp {

class WatermarkDetector
{
public:
    explicit WatermarkDetector(double sampleRate = 48000.0, double sensitivity = 5.0);
    void prepare(double sampleRate, double sensitivity = 5.0);
    DetectorResult process(double liveSample, double referenceSample) noexcept;   // real-time safe

private:
    void evaluateBlock() noexcept;

    std::size_t block_ = 2400, count_ = 0;
    double sumRes_ = 0.0, sumRef_ = 0.0;
    double floorDb_ = -60.0, alpha_ = 0.5;
    int hits_ = 0, misses_ = 0;
    RunningWindow<8> levels_;
    DetectorResult result_{};
};

} // namespace dawchecker::dsp
