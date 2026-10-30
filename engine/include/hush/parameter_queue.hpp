#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hush {

struct ParamCommand {
  std::uint32_t target{0};
  float value{0.0F};
  std::uint8_t kind{0};  // 0=gain 1=pan 2=mute
};

static_assert(std::is_trivially_copyable_v<ParamCommand>);

// Bounded SPSC — UI producer, audio consumer. Usable capacity = Capacity-1.
template <std::size_t Capacity>
class ParameterQueue {
  static_assert(Capacity >= 2);
public:
  [[nodiscard]] bool try_push(const ParamCommand& v) noexcept {
    const auto h = head_.load(std::memory_order_relaxed);
    const auto n = (h + 1) % Capacity;
    if (n == tail_.load(std::memory_order_acquire)) return false;
    storage_[h] = v;
    head_.store(n, std::memory_order_release);
    return true;
  }
  [[nodiscard]] bool try_pop(ParamCommand& v) noexcept {
    const auto t = tail_.load(std::memory_order_relaxed);
    if (t == head_.load(std::memory_order_acquire)) return false;
    v = storage_[t];
    tail_.store((t + 1) % Capacity, std::memory_order_release);
    return true;
  }
  static constexpr std::size_t usable_capacity() noexcept { return Capacity - 1; }
private:
  alignas(64) std::array<ParamCommand, Capacity> storage_{};
  alignas(64) std::atomic<std::size_t> head_{0};
  alignas(64) std::atomic<std::size_t> tail_{0};
};

}  // namespace hush
