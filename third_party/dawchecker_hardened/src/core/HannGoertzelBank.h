#pragma once
// Hann-windowed Goertzel bank over fixed-length blocks. Fixed storage, no allocation, amplitude-calibrated:
// dbfs(k) returns the amplitude of a sinusoid at frequency k in dBFS (a full-scale sine reads about 0 dBFS).
#include <array>
#include <cmath>
#include <cstddef>
#include "DetectorTypes.h"

namespace dawchecker::dsp {

template <std::size_t K>
class HannGoertzelBank
{
public:
    void configure(const std::array<double, K>& freqsHz, double sampleRate, std::size_t blockLen) noexcept
    {
        n_ = blockLen >= 16 ? blockLen : 16;
        for (std::size_t k = 0; k < K; ++k)
        {
            const double w = 2.0 * kPi * freqsHz[k] / sampleRate;
            coeff_[k] = 2.0 * std::cos(w); cosw_[k] = std::cos(w); sinw_[k] = std::sin(w);
        }
        const double d = 2.0 * kPi / static_cast<double>(n_ - 1);
        dc_ = std::cos(d); ds_ = std::sin(d);
        reset();
    }

    void reset() noexcept
    {
        s1_.fill(0.0); s2_.fill(0.0);
        idx_ = 0; wc_ = 1.0; ws_ = 0.0; wsum_ = 0.0;
    }

    std::size_t blockLength() const noexcept { return n_; }

    // Returns true when a block is complete (call dbfs(), then reset()).
    bool push(double x) noexcept
    {
        const double w  = 0.5 - 0.5 * wc_;          // Hann: 0.5 - 0.5 cos(2 pi n/(N-1))
        wsum_ += w;
        const double xw = x * w;
        for (std::size_t k = 0; k < K; ++k)
        {
            const double s0 = xw + coeff_[k] * s1_[k] - s2_[k];
            s2_[k] = s1_[k];
            s1_[k] = s0;
        }
        const double c = wc_ * dc_ - ws_ * ds_;      // rotate the phasor: no cos() per sample
        ws_ = ws_ * dc_ + wc_ * ds_;
        wc_ = c;
        return ++idx_ >= n_;
    }

    double dbfs(std::size_t k) const noexcept
    {
        const double re = s1_[k] - s2_[k] * cosw_[k];
        const double im = s2_[k] * sinw_[k];
        const double amp = wsum_ > 0.0 ? 2.0 * std::sqrt(re * re + im * im) / wsum_ : 0.0;
        return amp > 1e-12 ? 20.0 * std::log10(amp) : -240.0;
    }

private:
    std::array<double, K> coeff_{}, cosw_{}, sinw_{}, s1_{}, s2_{};
    std::size_t n_ = 16, idx_ = 0;
    double dc_ = 1.0, ds_ = 0.0, wc_ = 1.0, ws_ = 0.0, wsum_ = 0.0;
};

} // namespace dawchecker::dsp
