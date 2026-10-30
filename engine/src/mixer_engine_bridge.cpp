#include "hush/mixer_engine_bridge.hpp"
#include <cmath>

namespace hush {

bool MixerEngineBridge::configure(RealtimeBusEngine& engine,
                                  std::vector<TrackBusBinding> bindings,
                                  std::string& error) {
  if (!engine.configured()) {
    error = "engine not configured";
    return false;
  }
  decltype(index_) idx;
  idx.reserve(bindings.size() * 2);
  for (const auto& b : bindings) {
    if (b.track_id.empty()) {
      error = "empty track_id";
      return false;
    }
    if (!idx.emplace(b.track_id, b.bus_index).second) {
      error = "duplicate track_id";
      return false;
    }
  }
  engine_ = &engine;
  index_ = std::move(idx);
  error.clear();
  return true;
}

bool MixerEngineBridge::enqueue_gain(std::string_view track_id, float gain) noexcept {
  if (!engine_ || !std::isfinite(gain) || gain < 0.f || gain > 16.f) return false;
  auto it = index_.find(track_id);
  if (it == index_.end()) return false;
  return engine_->push_param(ParamCommand{static_cast<unsigned>(it->second), gain, 0});
}

bool MixerEngineBridge::enqueue_mute(std::string_view track_id, bool muted) noexcept {
  if (!engine_) return false;
  auto it = index_.find(track_id);
  if (it == index_.end()) return false;
  return engine_->push_param(
      ParamCommand{static_cast<unsigned>(it->second), muted ? 1.f : 0.f, 2});
}

}  // namespace hush
