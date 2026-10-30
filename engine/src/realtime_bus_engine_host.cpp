#include "hush/realtime_bus_engine_host.hpp"
#include <cmath>

namespace hush {

bool RealtimeBusEngineHost::replace_graph(const BusGraph& graph, std::size_t max_frames,
                                          const std::string& master_bus_id,
                                          ProcessorSetupError& error) {
  if (!graph.valid()) {
    error = {"graph_invalid", "", "BusGraph must be valid"};
    return false;
  }
  if (master_bus_id.empty()) {
    error = {"master_empty", "", "master_bus_id required"};
    return false;
  }
  std::size_t master_index = static_cast<std::size_t>(-1);
  const auto& buses = graph.buses();
  for (std::size_t i = 0; i < buses.size(); ++i) {
    if (buses[i].id == master_bus_id) {
      master_index = i;
      break;
    }
  }
  if (master_index == static_cast<std::size_t>(-1)) {
    error = {"master_unknown", master_bus_id, "master bus not in graph"};
    return false;
  }

  auto snap = std::make_unique<Snapshot>();
  if (!snap->engine.configure(graph, max_frames, error)) return false;
  snap->master_bus_index = master_index;
  publisher_.publish(std::move(snap));
  error = {};
  return true;
}

bool RealtimeBusEngineHost::enqueue_fader_change(std::size_t bus_index,
                                                 float gain) noexcept {
  if (!std::isfinite(gain) || gain < 0.f || gain > 16.f) return false;
  auto* snap = publisher_.active_for_control();
  if (!snap) return false;
  return snap->engine.push_param(
      ParamCommand{static_cast<unsigned>(bus_index), gain, 0});
}

RealtimeProcessStatus RealtimeBusEngineHost::process_to_stereo(
    const BusInputView* inputs, std::size_t input_count, std::size_t frames,
    float* output_left, float* output_right) noexcept {
  auto* snap = publisher_.hold_raw();
  if (!snap || !output_left || !output_right)
    return RealtimeProcessStatus::NotConfigured;
  return snap->engine.process_block(const_cast<BusInputView*>(inputs), input_count, frames,
                                    snap->master_bus_index, output_left, output_right);
}

std::size_t RealtimeBusEngineHost::bus_count_for_control() const noexcept {
  auto* snap = publisher_.active_for_control();
  return snap ? snap->engine.bus_count() : 0;
}

std::size_t RealtimeBusEngineHost::max_frames_for_control() const noexcept {
  auto* snap = publisher_.active_for_control();
  return snap ? snap->engine.max_frames() : 0;
}

}  // namespace hush
