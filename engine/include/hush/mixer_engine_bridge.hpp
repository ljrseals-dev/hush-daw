#pragma once
#include "hush/realtime_bus_engine.hpp"
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace hush {

struct TrackBusBinding {
  std::string track_id;
  std::size_t bus_index{};
};

struct StringHash {
  using is_transparent = void;
  std::size_t operator()(std::string_view s) const noexcept {
    return std::hash<std::string_view>{}(s);
  }
  std::size_t operator()(const std::string& s) const noexcept {
    return std::hash<std::string_view>{}(s);
  }
};

class MixerEngineBridge {
public:
  bool configure(RealtimeBusEngine& engine, std::vector<TrackBusBinding> bindings,
                 std::string& error);

  [[nodiscard]] bool enqueue_gain(std::string_view track_id, float gain) noexcept;
  [[nodiscard]] bool enqueue_mute(std::string_view track_id, bool muted) noexcept;
  [[nodiscard]] bool ready() const noexcept { return engine_ != nullptr; }

private:
  RealtimeBusEngine* engine_{nullptr};
  std::unordered_map<std::string, std::size_t, StringHash, std::equal_to<>> index_;
};

}  // namespace hush
