#pragma once
// HARDENED FORK. Neutralizers are OFF by default and are never applied by MSV_Engine::process(). They are only
// reachable through render_neutralized(), an explicit OFFLINE call that returns a NEW buffer. The upstream
// WatermarkScrubber (watermark removal) has been removed from this fork.
#include <array>
#include <vector>
#include "DetectorTypes.h"

namespace dawchecker::dsp {

class ENFNotchFilter
{
public:
    explicit ENFNotchFilter(double sampleRate = 48000.0);
    void prepare(double sampleRate);
    double process(double x) noexcept;
    bool enabled = false;
private:
    std::array<BiquadState, 6> notches_{};
};

class UltrasonicRolloff
{
public:
    explicit UltrasonicRolloff(double sampleRate = 48000.0, double cutoffHz = 17000.0);
    void prepare(double sampleRate, double cutoffHz = 17000.0);
    double process(double x) noexcept;
    bool enabled = false;
private:
    BiquadState lowpass_{};
};

struct NeutralizeParams
{
    bool   enfNotch = false;          // notches at 50/60 Hz and harmonics
    bool   ultrasonicRolloff = false; // low-pass
    double rolloffCutoffHz = 17000.0;
};

// Offline, explicit. Never modifies `in`. With nothing enabled the result is bit-identical to `in`.
std::vector<double> render_neutralized(const std::vector<double>& in, double sampleRate, const NeutralizeParams& params);

} // namespace dawchecker::dsp
