#pragma once
// HARDENED FORK (Hushroom 1.1.1). Changes vs. upstream: DetectorResult::label is a pointer to a static string
// (no std::string, so no allocation on the audio thread). See ../../PROVENANCE.md.
#include <cmath>
#include <cstdint>

namespace dawchecker::dsp {

enum class DetectorStatus : std::uint8_t { Idle, Active, Alert };

struct DetectorResult
{
    DetectorStatus status     = DetectorStatus::Idle;
    double         confidence = 0.0;      // 0..100 indicator, NOT a probability
    double         locationHz = 0.0;      // dominant frequency of the finding, if applicable
    const char*    label      = "";       // static string, never owned/freed
};

struct BiquadState
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    inline double process(double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

BiquadState makeNotch(double sampleRate, double freqHz, double q);
BiquadState makeOnePoleLowpass(double sampleRate, double cutoffHz);

inline constexpr double kPi = 3.14159265358979323846;

// Flush values that would become denormal to exact zero (denormals cause large CPU spikes).
inline double flushDenormal(double x) noexcept { return (x < 1e-30 && x > -1e-30) ? 0.0 : x; }

// Sanitise a sample rate coming from outside: finite and within a sane range, else 48 kHz.
inline double sanitizeSampleRate(double sr) noexcept
{
    return (std::isfinite(sr) && sr >= 8000.0 && sr <= 768000.0) ? sr : 48000.0;
}

// Enable flush-to-zero / denormals-are-zero for the CALLING thread (x86 SSE). No-op elsewhere.
void enableFlushDenormalsForThisThread() noexcept;

} // namespace dawchecker::dsp
