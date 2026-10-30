#pragma once
// HARDENED FORK. Runs the three detectors and publishes ONE coherent snapshot through a seqlock.
//  * process() never alters audio: `out` is always exactly `live`.  Neutralization lives in render_neutralized().
//  * process() performs no allocation, locking, I/O or exceptions.
//  * Argument order is (live, reference, out), matching the detectors.
//  * One writer thread calls process()/prepare(); any number of threads may call snapshot().
#include <array>
#include <atomic>
#include <cstdint>
#include "DetectorTypes.h"
#include "ENFAnalyzer.h"
#include "UltrasonicScanner.h"
#include "WatermarkDetector.h"
#include "Neutralizers.h"

namespace dawchecker::dsp {

struct EngineSnapshot
{
    double tamperScore = 0.0;   // 0..100 aggregate, uncalibrated indicator; not exposed by the Hushroom CLI
    bool   isCompromised = false;
    DetectorResult watermark;
    DetectorResult enf;
    DetectorResult ultrasonic;
};

class MSV_Engine
{
public:
    explicit MSV_Engine(double sampleRate = 48000.0, double sensitivity = 5.0);
    void prepare(double sampleRate, double sensitivity);
    void process(double liveSample, double referenceSample, double& out) noexcept;
    EngineSnapshot snapshot() const noexcept;      // never returns a torn mix of fields from different samples
    // The aggregate used for tamperScore, exposed so tests can check snapshot coherence.
    static double aggregate(const DetectorResult& wm, const DetectorResult& enf, const DetectorResult& us) noexcept;

private:
    static constexpr std::size_t kWords = 14;
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free, "seqlock payload words must be lock-free");
    void publish(const EngineSnapshot& s) noexcept;

    double sampleRate_ = 48000.0, sensitivity_ = 5.0;
    WatermarkDetector watermarkDetector_;
    ENFAnalyzer       enfAnalyzer_;
    UltrasonicScanner ultrasonicScanner_;
    std::atomic<std::uint32_t> seq_{0};
    std::array<std::atomic<std::uint64_t>, kWords> words_{};
};

} // namespace dawchecker::dsp
