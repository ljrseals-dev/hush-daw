#include "WatermarkDetector.h"
#include <algorithm>
#include <cmath>

namespace dawchecker::dsp {

namespace {
constexpr double kBlockSec   = 0.050;
constexpr double kTauSec     = 0.050;
constexpr double kCeilingDb  = -20.0;   // louder than this = the signals simply differ
constexpr double kMaxStdDb   = 6.0;     // residual level must be steady over the last 8 blocks
constexpr double kMinRefRms  = 1e-6;    // about -120 dBFS: no usable reference energy
constexpr const char* kIdle   = "live matches reference, or no usable reference";
constexpr const char* kDiffer = "live and reference differ strongly (not a low-level residual)";
constexpr const char* kActive = "persistent low-level residual vs reference (not evidence of a watermark by itself)";
}

WatermarkDetector::WatermarkDetector(double sampleRate, double sensitivity) { prepare(sampleRate, sensitivity); }

void WatermarkDetector::prepare(double sampleRate, double sensitivity)
{
    const double sr = sanitizeSampleRate(sampleRate);
    const double sens = std::clamp(std::isfinite(sensitivity) ? sensitivity : 5.0, 1.0, 100.0);
    floorDb_ = std::clamp(-60.0 + 2.0 * (sens - 5.0), -80.0, -40.0);
    block_ = static_cast<std::size_t>(std::llround(kBlockSec * sr));
    alpha_ = 1.0 - std::exp(-kBlockSec / kTauSec);
    count_ = 0; sumRes_ = sumRef_ = 0.0; hits_ = misses_ = 0;
    levels_.clear();
    result_ = DetectorResult{};
    result_.label = kIdle;
}

DetectorResult WatermarkDetector::process(double live, double reference) noexcept
{
    const double d = flushDenormal(live - reference);
    sumRes_ += d * d;
    sumRef_ += reference * reference;
    if (++count_ >= block_) { evaluateBlock(); count_ = 0; sumRes_ = sumRef_ = 0.0; }
    return result_;
}

void WatermarkDetector::evaluateBlock() noexcept
{
    const double n = static_cast<double>(block_);
    const double refRms = std::sqrt(sumRef_ / n), resRms = std::sqrt(sumRes_ / n);
    bool hit = false, differ = false;
    if (refRms >= kMinRefRms)
    {
        const double lvl = 20.0 * std::log10(std::max(resRms, 1e-12) / refRms);
        if (lvl > kCeilingDb) { differ = true; levels_.clear(); }
        else if (lvl >= floorDb_)
        {
            levels_.push(lvl);
            hit = levels_.full() && std::sqrt(levels_.variance()) <= kMaxStdDb;
        }
        else levels_.clear();
    }
    else levels_.clear();

    if (hit) { ++hits_; misses_ = 0; }
    else { hits_ = 0; if (++misses_ >= 3) { misses_ = 0; result_.status = DetectorStatus::Idle; } }
    if (hits_ >= 10) result_.status = DetectorStatus::Active;   // 0.5 s sustained; capped at Active
    result_.confidence += ((hit ? 100.0 : 0.0) - result_.confidence) * alpha_;
    result_.confidence = flushDenormal(result_.confidence);
    result_.label = (result_.status == DetectorStatus::Active) ? kActive : (differ ? kDiffer : kIdle);
}

} // namespace dawchecker::dsp
