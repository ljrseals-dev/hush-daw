#include "Neutralizers.h"
#include <cmath>
#include <algorithm>

namespace dawchecker::dsp {

BiquadState makeNotch(double sampleRate, double freqHz, double q)
{
    BiquadState s;
    const double w0 = 2.0 * M_PI * freqHz / sampleRate;
    const double alpha = std::sin(w0) / (2.0 * q);
    const double cosw0 = std::cos(w0);

    const double b0 = 1.0, b1 = -2.0 * cosw0, b2 = 1.0;
    const double a0 = 1.0 + alpha, a1 = -2.0 * cosw0, a2 = 1.0 - alpha;

    s.b0 = b0 / a0; s.b1 = b1 / a0; s.b2 = b2 / a0;
    s.a1 = a1 / a0; s.a2 = a2 / a0;
    return s;
}

BiquadState makeOnePoleLowpass(double sampleRate, double cutoffHz)
{
    // 2nd-order Butterworth lowpass (steeper than a true one-pole while
    // staying cheap enough for real-time use on the master bus).
    BiquadState s;
    const double w0 = 2.0 * M_PI * cutoffHz / sampleRate;
    const double alpha = std::sin(w0) / std::sqrt(2.0);
    const double cosw0 = std::cos(w0);

    const double b0 = (1.0 - cosw0) / 2.0;
    const double b1 = 1.0 - cosw0;
    const double b2 = (1.0 - cosw0) / 2.0;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosw0;
    const double a2 = 1.0 - alpha;

    s.b0 = b0 / a0; s.b1 = b1 / a0; s.b2 = b2 / a0;
    s.a1 = a1 / a0; s.a2 = a2 / a0;
    return s;
}

ENFNotchFilter::ENFNotchFilter(double sampleRate) { prepare(sampleRate); }

void ENFNotchFilter::prepare(double sampleRate)
{
    static constexpr std::array<double, 6> freqs = {50.0, 60.0, 100.0, 120.0, 150.0, 180.0};
    for (std::size_t i = 0; i < notches_.size(); ++i)
        notches_[i] = makeNotch(sanitizeSampleRate(sampleRate), freqs[i], 12.0);
}

double ENFNotchFilter::process(double x) noexcept
{
    if (!enabled) return x;
    double y = x;
    for (auto& notch : notches_)
        y = notch.process(y);
    return y;
}

UltrasonicRolloff::UltrasonicRolloff(double sampleRate, double cutoffHz) { prepare(sampleRate, cutoffHz); }

void UltrasonicRolloff::prepare(double sampleRate, double cutoffHz)
{
    lowpass_ = makeOnePoleLowpass(sampleRate, cutoffHz);
}

double UltrasonicRolloff::process(double x) noexcept
{
    if (!enabled) return x;
    return lowpass_.process(x);
}

std::vector<double> render_neutralized(const std::vector<double>& in, double sampleRate, const NeutralizeParams& p)
{
    std::vector<double> out = in;                       // copy: the input is never modified
    if (!p.enfNotch && !p.ultrasonicRolloff) return out; // empty enable set: bit-identical
    const double sr = sanitizeSampleRate(sampleRate);
    ENFNotchFilter notch(sr);
    notch.enabled = p.enfNotch;
    UltrasonicRolloff roll(sr, p.rolloffCutoffHz);
    roll.enabled = p.ultrasonicRolloff;
    for (double& x : out) x = roll.process(notch.process(x));
    return out;
}

} // namespace dawchecker::dsp
