#include "UltrasonicScanner.h"
#include <algorithm>
#include <cmath>

namespace dawchecker::dsp {

namespace {
constexpr std::array<double, 5> kBandCentres = {17400.0, 18000.0, 18500.0, 19200.0, 20150.0};
constexpr double kFloorDbfs  = -90.0;   // ignore anything quieter than this
constexpr double kBlockSec   = 0.085;
constexpr double kTauSec     = 0.250;   // confidence smoothing time constant (gate 4)
constexpr const char* kIdle   = "no sustained narrowband tone in the 17-20 kHz bands";
constexpr const char* kActive = "sustained narrowband tone in a 17-20 kHz band";
constexpr const char* kAlert  = "persistent narrowband tone in a 17-20 kHz band (indicator only)";
constexpr const char* kNA     = "not applicable: sample rate too low for the 17-20 kHz bands";
}

UltrasonicScanner::UltrasonicScanner(double sampleRate, double sensitivity) { prepare(sampleRate, sensitivity); }

void UltrasonicScanner::prepare(double sampleRate, double sensitivity)
{
    const double sr = sanitizeSampleRate(sampleRate);
    const double sens = std::clamp(std::isfinite(sensitivity) ? sensitivity : 5.0, 1.0, 100.0);
    marginDb_ = std::clamp(20.0 - 2.0 * (sens - 5.0), 10.0, 30.0);
    const std::size_t block = static_cast<std::size_t>(std::llround(kBlockSec * sr));
    alpha_ = 1.0 - std::exp(-(static_cast<double>(block) / sr) / kTauSec);
    const double limit = 0.49 * sr;
    std::array<double, kBands * kPerBand> freqs{};
    anyValid_ = false;
    for (std::size_t b = 0; b < kBands; ++b)
    {
        const double c = kBandCentres[b];
        valid_[b] = (c + 800.0) < limit;
        anyValid_ = anyValid_ || valid_[b];
        bandHz_[b] = c;
        const double cc = valid_[b] ? c : 1000.0;     // harmless placeholder for disabled bands
        freqs[b * kPerBand + 0] = cc;
        freqs[b * kPerBand + 1] = cc - 400.0; freqs[b * kPerBand + 2] = cc + 400.0;
        freqs[b * kPerBand + 3] = cc - 800.0; freqs[b * kPerBand + 4] = cc + 800.0;
    }
    bank_.configure(freqs, sr, block);
    hits_ = misses_ = 0;
    result_ = DetectorResult{};
    result_.label = anyValid_ ? kIdle : kNA;
}

DetectorResult UltrasonicScanner::process(double x) noexcept
{
    if (!anyValid_) return result_;
    if (bank_.push(flushDenormal(x))) { evaluateBlock(); bank_.reset(); }
    return result_;
}

void UltrasonicScanner::evaluateBlock() noexcept
{
    bool hit = false; double bestAmp = -240.0, bestHz = 0.0;
    for (std::size_t b = 0; b < kBands; ++b)
    {
        if (!valid_[b]) continue;
        const double amp = bank_.dbfs(b * kPerBand);
        double ref = -240.0;
        for (std::size_t j = 1; j < kPerBand; ++j) ref = std::max(ref, bank_.dbfs(b * kPerBand + j));
        if (amp > kFloorDbfs && (amp - ref) >= marginDb_ && amp > bestAmp) { hit = true; bestAmp = amp; bestHz = bandHz_[b]; }
    }
    if (hit) { ++hits_; misses_ = 0; result_.locationHz = bestHz; }
    else
    {
        hits_ = 0;
        if (++misses_ >= 3)   // hysteresis: three clean blocks step the verdict down one level
        {
            misses_ = 0;
            if (result_.status == DetectorStatus::Alert) result_.status = DetectorStatus::Active;
            else result_.status = DetectorStatus::Idle;
        }
    }
    if (hits_ >= alertBlocks_)       result_.status = DetectorStatus::Alert;
    else if (hits_ >= activeBlocks_ && result_.status == DetectorStatus::Idle) result_.status = DetectorStatus::Active;

    result_.confidence += ((hit ? 100.0 : 0.0) - result_.confidence) * alpha_;
    result_.confidence = flushDenormal(result_.confidence);
    if (result_.status == DetectorStatus::Idle) { result_.locationHz = 0.0; result_.label = kIdle; }
    else result_.label = (result_.status == DetectorStatus::Alert) ? kAlert : kActive;
}

} // namespace dawchecker::dsp
