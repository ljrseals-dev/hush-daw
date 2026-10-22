#pragma once
#include "hush/ProcessorAdapter.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
namespace hush::integration {
struct SlotCommand { Slot slot{}; SlotState state{}; };
template <std::size_t Capacity> class SpscCommandQueue {
    static_assert(Capacity >= 2);
public:
    [[nodiscard]] bool push(const SlotCommand& cmd) noexcept {
        const auto head = head_.load(std::memory_order_relaxed), next = increment(head);
        if (next == tail_.load(std::memory_order_acquire)) return false;
        entries_[head] = cmd; head_.store(next, std::memory_order_release); return true;
    }
    [[nodiscard]] bool pop(SlotCommand& out) noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;
        out = entries_[tail]; tail_.store(increment(tail), std::memory_order_release); return true;
    }
private:
    static constexpr std::size_t increment(std::size_t i) noexcept { return (i + 1U) % Capacity; }
    std::array<SlotCommand, Capacity> entries_{};
    alignas(64) std::atomic<std::size_t> head_{0}, tail_{0};
};
class ProcessorBridge {
public:
    explicit ProcessorBridge(IProcessorAdapter& p) noexcept : processor_(p) {}
    [[nodiscard]] bool prepare(double, std::uint32_t, std::size_t) noexcept;
    [[nodiscard]] bool queue_slot(Slot, SlotState) noexcept;
    void process(AudioBlock) noexcept;
    [[nodiscard]] std::uint64_t rejected_blocks() const noexcept { return rejected_blocks_.load(std::memory_order_relaxed); }
private:
    IProcessorAdapter& processor_;
    SpscCommandQueue<64> commands_{};
    std::atomic<std::uint64_t> rejected_blocks_{0};
    std::uint32_t channels_{0};
    std::size_t max_frames_{0};
    double sample_rate_{0.0};
};
}
