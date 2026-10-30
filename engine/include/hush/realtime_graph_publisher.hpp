#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace hush {

// Double-slot publish: audio loads index then reads ptr[slot]. Control publishes into
// inactive slot then flips index. Retired slots reclaimed on control thread only.
// Audio must not retain the pointer past process() return.
template <typename T>
class RealtimeGraphPublisher {
public:
  RealtimeGraphPublisher() = default;
  RealtimeGraphPublisher(const RealtimeGraphPublisher&) = delete;
  RealtimeGraphPublisher& operator=(const RealtimeGraphPublisher&) = delete;

  void publish(std::unique_ptr<T> next) noexcept {
    const std::uint32_t cur = index_.load(std::memory_order_acquire);
    const std::uint32_t dest = cur ^ 1u;
    // Retire whatever was in dest (previous previous)
    if (slots_[dest]) {
      retired_.push_back(std::move(slots_[dest]));
    }
    slots_[dest] = std::move(next);
    index_.store(dest, std::memory_order_release);
  }

  // Audio: pointer valid until next reclaim that drops the slot; keep process short.
  [[nodiscard]] T* hold_raw() const noexcept {
    const std::uint32_t i = index_.load(std::memory_order_acquire);
    return slots_[i].get();
  }

  void reclaim_retired() noexcept { retired_.clear(); }

  [[nodiscard]] T* active_for_control() const noexcept {
    return hold_raw();
  }

private:
  std::unique_ptr<T> slots_[2]{};
  std::atomic<std::uint32_t> index_{0};
  std::vector<std::unique_ptr<T>> retired_;
};

}  // namespace hush
