#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hush::cosmic {

enum class CommandType : std::uint8_t {
    SetTrackGain,
    SetTrackPan,
    SetTrackMute,
    SetMasterGain,
    TransportPlay,
    TransportStop,
    TransportSeek
};

struct EngineCommand {
    CommandType type{CommandType::SetMasterGain};
    std::uint32_t target{0};
    float value{0.0F};
    std::uint64_t sequence{0};
};

static_assert(std::is_trivially_copyable_v<EngineCommand>);

// Single-producer/single-consumer queue. Capacity includes one reserved slot,
// so usable capacity is Capacity - 1. push() and pop() do not allocate or wait.
// Exactly one thread may call try_push(), and exactly one other thread may
// call try_pop(). This is not a multi-producer/multi-consumer queue.
template <typename T, std::size_t Capacity>
class SpscQueue final {
    static_assert(Capacity >= 2, "SPSC queue needs at least two slots");
    static_assert(std::is_trivially_copyable_v<T>,
                  "SPSC queue payloads must be trivially copyable");

public:
    [[nodiscard]] bool try_push(const T& value) noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = increment(head);
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        storage_[head] = value;
        head_.store(next, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& value) noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        value = storage_[tail];
        tail_.store(increment(tail), std::memory_order_release);
        return true;
    }

    [[nodiscard]] static constexpr std::size_t usable_capacity() noexcept {
        return Capacity - 1;
    }

private:
    [[nodiscard]] static constexpr std::size_t increment(std::size_t index) noexcept {
        return (index + 1U) % Capacity;
    }

    alignas(64) std::array<T, Capacity> storage_{};
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
};

struct TelemetrySnapshot {
    bool valid{false};
    float output_peak{0.0F};
    float output_rms{0.0F};
    float cpu_load{0.0F};
    std::uint32_t active_tracks{0};
    std::uint32_t sample_rate{0};
    std::uint32_t block_frames{0};
    std::uint64_t rendered_frames{0};
};

// A single audio-thread writer publishes measurements through atomics. A UI
// reader never accesses mutable non-atomic memory and never blocks. The bounded
// retry count means a snapshot may be unavailable under heavy contention;
// unavailable telemetry is not represented as a real zero reading.
class TelemetryChannel final {
public:
    void publish(const TelemetrySnapshot& snapshot) noexcept {
        sequence_.fetch_add(1U, std::memory_order_acq_rel); // odd: write in progress
        peak_.store(bits(clamp_unit(snapshot.output_peak)), std::memory_order_relaxed);
        rms_.store(bits(clamp_unit(snapshot.output_rms)), std::memory_order_relaxed);
        cpu_.store(bits(clamp_unit(snapshot.cpu_load)), std::memory_order_relaxed);
        tracks_.store(snapshot.active_tracks, std::memory_order_relaxed);
        sample_rate_.store(snapshot.sample_rate, std::memory_order_relaxed);
        block_frames_.store(snapshot.block_frames, std::memory_order_relaxed);
        rendered_frames_.store(snapshot.rendered_frames, std::memory_order_relaxed);
        valid_.store(snapshot.valid, std::memory_order_relaxed);
        sequence_.fetch_add(1U, std::memory_order_release); // even: published
    }

    [[nodiscard]] bool try_read(TelemetrySnapshot& out,
                                unsigned attempts = 3U) const noexcept {
        for (unsigned attempt = 0; attempt < attempts; ++attempt) {
            const auto before = sequence_.load(std::memory_order_acquire);
            if ((before & 1U) != 0U) {
                continue;
            }

            TelemetrySnapshot candidate;
            candidate.output_peak = value(peak_.load(std::memory_order_relaxed));
            candidate.output_rms = value(rms_.load(std::memory_order_relaxed));
            candidate.cpu_load = value(cpu_.load(std::memory_order_relaxed));
            candidate.active_tracks = tracks_.load(std::memory_order_relaxed);
            candidate.sample_rate = sample_rate_.load(std::memory_order_relaxed);
            candidate.block_frames = block_frames_.load(std::memory_order_relaxed);
            candidate.rendered_frames = rendered_frames_.load(std::memory_order_relaxed);
            candidate.valid = valid_.load(std::memory_order_relaxed);

            const auto after = sequence_.load(std::memory_order_acquire);
            if (before == after && (after & 1U) == 0U) {
                out = candidate;
                return candidate.valid;
            }
        }
        out = TelemetrySnapshot{};
        return false;
    }

private:
    [[nodiscard]] static float clamp_unit(float v) noexcept {
        if (!std::isfinite(v)) return 0.0F;
        return v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v);
    }

    [[nodiscard]] static std::uint64_t bits(float v) noexcept {
        // Store a float's exact bit pattern in an integer atomic. This avoids
        // relying on std::atomic<float> being lock-free on every target.
        union FloatBits { float f; std::uint32_t u; };
        FloatBits cvt{};
        cvt.f = v;
        return static_cast<std::uint64_t>(cvt.u);
    }

    [[nodiscard]] static float value(std::uint64_t raw) noexcept {
        union FloatBits { float f; std::uint32_t u; };
        FloatBits cvt{};
        cvt.u = static_cast<std::uint32_t>(raw);
        return cvt.f;
    }

    mutable std::atomic<std::uint64_t> sequence_{0};
    std::atomic<std::uint64_t> peak_{0};
    std::atomic<std::uint64_t> rms_{0};
    std::atomic<std::uint64_t> cpu_{0};
    std::atomic<std::uint32_t> tracks_{0};
    std::atomic<std::uint32_t> sample_rate_{0};
    std::atomic<std::uint32_t> block_frames_{0};
    std::atomic<std::uint64_t> rendered_frames_{0};
    std::atomic<bool> valid_{false};
};

} // namespace hush::cosmic
