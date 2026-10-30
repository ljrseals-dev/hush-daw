#include "MSV_Engine.h"
#include <algorithm>
#include <cstring>

namespace dawchecker::dsp {

namespace {
inline std::uint64_t d2u(double d) noexcept { std::uint64_t u; std::memcpy(&u, &d, sizeof u); return u; }
inline double u2d(std::uint64_t u) noexcept { double d; std::memcpy(&d, &u, sizeof d); return d; }
inline std::uint64_t p2u(const char* p) noexcept { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p)); }
inline const char* u2p(std::uint64_t u) noexcept { return reinterpret_cast<const char*>(static_cast<std::uintptr_t>(u)); }
}

MSV_Engine::MSV_Engine(double sampleRate, double sensitivity) { prepare(sampleRate, sensitivity); }

void MSV_Engine::prepare(double sampleRate, double sensitivity)
{
    enableFlushDenormalsForThisThread();   // affects the calling thread; call prepare() or this on the audio thread too
    sampleRate_  = sanitizeSampleRate(sampleRate);
    sensitivity_ = std::clamp(std::isfinite(sensitivity) ? sensitivity : 5.0, 1.0, 100.0);
    watermarkDetector_.prepare(sampleRate_, sensitivity_);
    enfAnalyzer_.prepare(sampleRate_, sensitivity_);
    ultrasonicScanner_.prepare(sampleRate_, sensitivity_);
    EngineSnapshot s;
    s.watermark = DetectorResult{}; s.enf = DetectorResult{}; s.ultrasonic = DetectorResult{};
    s.watermark.label = s.enf.label = s.ultrasonic.label = "";
    publish(s);
}

double MSV_Engine::aggregate(const DetectorResult& wm, const DetectorResult& enf, const DetectorResult& us) noexcept
{
    const double maxConf = std::max({wm.confidence, enf.confidence, us.confidence});
    const double avgConf = (wm.confidence + enf.confidence + us.confidence) / 3.0;
    return std::clamp(maxConf * 0.7 + avgConf * 0.3, 0.0, 100.0);
}

void MSV_Engine::process(double liveSample, double referenceSample, double& out) noexcept
{
    out = liveSample;   // detection only: audio is never modified here
    EngineSnapshot s;
    s.watermark  = watermarkDetector_.process(liveSample, referenceSample);
    s.enf        = enfAnalyzer_.process(liveSample);
    s.ultrasonic = ultrasonicScanner_.process(liveSample);
    s.tamperScore = aggregate(s.watermark, s.enf, s.ultrasonic);
    s.isCompromised = s.watermark.status == DetectorStatus::Alert || s.enf.status == DetectorStatus::Alert ||
                      s.ultrasonic.status == DetectorStatus::Alert;
    publish(s);
}

// Single-writer seqlock. Every payload word is an atomic, so there is no data race (TSan-clean).
void MSV_Engine::publish(const EngineSnapshot& s) noexcept
{
    const std::uint64_t w[kWords] = {
        d2u(s.tamperScore), s.isCompromised ? 1u : 0u,
        static_cast<std::uint64_t>(s.watermark.status),  d2u(s.watermark.confidence),  d2u(s.watermark.locationHz),  p2u(s.watermark.label),
        static_cast<std::uint64_t>(s.enf.status),        d2u(s.enf.confidence),        d2u(s.enf.locationHz),        p2u(s.enf.label),
        static_cast<std::uint64_t>(s.ultrasonic.status), d2u(s.ultrasonic.confidence), d2u(s.ultrasonic.locationHz), p2u(s.ultrasonic.label)};
    const std::uint32_t q = seq_.load(std::memory_order_relaxed);
    seq_.store(q + 1, std::memory_order_relaxed);          // odd: write in progress
    std::atomic_thread_fence(std::memory_order_release);
    for (std::size_t i = 0; i < kWords; ++i) words_[i].store(w[i], std::memory_order_relaxed);
    seq_.store(q + 2, std::memory_order_release);          // even: stable
}

EngineSnapshot MSV_Engine::snapshot() const noexcept
{
    std::uint64_t w[kWords];
    std::uint32_t a, b;
    do
    {
        a = seq_.load(std::memory_order_acquire);
        for (std::size_t i = 0; i < kWords; ++i) w[i] = words_[i].load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        b = seq_.load(std::memory_order_relaxed);
    } while ((a & 1u) != 0u || a != b);
    EngineSnapshot s;
    s.tamperScore = u2d(w[0]);  s.isCompromised = w[1] != 0;
    s.watermark  = {static_cast<DetectorStatus>(w[2]),  u2d(w[3]),  u2d(w[4]),  u2p(w[5])};
    s.enf        = {static_cast<DetectorStatus>(w[6]),  u2d(w[7]),  u2d(w[8]),  u2p(w[9])};
    s.ultrasonic = {static_cast<DetectorStatus>(w[10]), u2d(w[11]), u2d(w[12]), u2p(w[13])};
    return s;
}

} // namespace dawchecker::dsp
