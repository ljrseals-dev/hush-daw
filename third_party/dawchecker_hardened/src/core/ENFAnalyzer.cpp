#include "ENFAnalyzer.h"
#include <algorithm>
#include <cmath>

namespace dawchecker::dsp {

namespace {
constexpr double kFloorDbfs = -90.0;
constexpr double kWindowSec = 2.0;
constexpr double kTauSec    = 2.0;
constexpr const char* kIdle   = "no narrowband component near 50 or 60 Hz";
constexpr const char* kActive = "narrowband component near 50/60 Hz present (mains hum is common; indicator only)";
constexpr const char* kNA     = "not applicable: sample rate too low";
}

ENFAnalyzer::ENFAnalyzer(double sampleRate, double sensitivity) { prepare(sampleRate, sensitivity); }

void ENFAnalyzer::prepare(double sampleRate, double sensitivity)
{
    const double sr = sanitizeSampleRate(sampleRate);
    const double sens = std::clamp(std::isfinite(sensitivity) ? sensitivity : 5.0, 1.0, 100.0);
    marginDb_ = std::clamp(15.0 - 1.5 * (sens - 5.0), 8.0, 25.0);
    baseHz_ = {50.0, 60.0};
    for (std::size_t b = 0; b < kBases; ++b)
    {
        for (std::size_t i = 0; i < kScan; ++i) freqs_[b * kPerBase + i] = baseHz_[b] - 1.0 + 0.05 * static_cast<double>(i);
        const double off[kRef] = {-5.0, -4.0, 4.0, 5.0};
        for (std::size_t j = 0; j < kRef; ++j) freqs_[b * kPerBase + kScan + j] = baseHz_[b] + off[j];
    }
    valid_ = sr >= 1000.0;
    const std::size_t block = static_cast<std::size_t>(std::llround(kWindowSec * sr));
    alpha_ = 1.0 - std::exp(-kWindowSec / kTauSec);
    bank_.configure(freqs_, sr, block);
    hits_ = misses_ = 0;
    result_ = DetectorResult{};
    result_.label = valid_ ? kIdle : kNA;
}

DetectorResult ENFAnalyzer::process(double x) noexcept
{
    if (!valid_) return result_;
    if (bank_.push(flushDenormal(x))) { evaluateBlock(); bank_.reset(); }
    return result_;
}

void ENFAnalyzer::evaluateBlock() noexcept
{
    bool hit = false; double bestPeak = -240.0, bestHz = 0.0;
    for (std::size_t b = 0; b < kBases; ++b)
    {
        double peak = -240.0, peakHz = baseHz_[b];
        for (std::size_t i = 0; i < kScan; ++i)
        {
            const double a = bank_.dbfs(b * kPerBase + i);
            if (a > peak) { peak = a; peakHz = freqs_[b * kPerBase + i]; }
        }
        double lin = 0.0;
        for (std::size_t j = 0; j < kRef; ++j) lin += std::pow(10.0, bank_.dbfs(b * kPerBase + kScan + j) / 20.0);
        const double refDb = 20.0 * std::log10(std::max(lin / static_cast<double>(kRef), 1e-12));
        if (peak > kFloorDbfs && (peak - refDb) >= marginDb_ && peak > bestPeak) { hit = true; bestPeak = peak; bestHz = peakHz; }
    }
    if (hit) { ++hits_; misses_ = 0; result_.locationHz = bestHz; }
    else
    {
        hits_ = 0;
        if (++misses_ >= 2) { misses_ = 0; result_.status = DetectorStatus::Idle; }
    }
    if (hits_ >= 2) result_.status = DetectorStatus::Active;   // two consecutive windows (4 s); capped at Active
    result_.confidence += ((hit ? 100.0 : 0.0) - result_.confidence) * alpha_;
    result_.confidence = flushDenormal(result_.confidence);
    if (result_.status == DetectorStatus::Idle) { result_.locationHz = 0.0; result_.label = kIdle; }
    else result_.label = kActive;
}

} // namespace dawchecker::dsp
