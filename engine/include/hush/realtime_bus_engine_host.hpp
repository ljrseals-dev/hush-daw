#pragma once
#include "hush/bus_graph.hpp"
#include "hush/preallocated_bus_processor.hpp"
#include "hush/realtime_bus_engine.hpp"
#include "hush/realtime_graph_publisher.hpp"
#include <cstddef>
#include <memory>
#include <string>

namespace hush {

class RealtimeBusEngineHost {
public:
  RealtimeBusEngineHost() = default;
  RealtimeBusEngineHost(const RealtimeBusEngineHost&) = delete;
  RealtimeBusEngineHost& operator=(const RealtimeBusEngineHost&) = delete;

  bool replace_graph(const BusGraph& graph, std::size_t max_frames,
                     const std::string& master_bus_id, ProcessorSetupError& error);

  [[nodiscard]] bool enqueue_fader_change(std::size_t bus_index, float gain) noexcept;

  [[nodiscard]] RealtimeProcessStatus process_to_stereo(const BusInputView* inputs,
                                                        std::size_t input_count,
                                                        std::size_t frames,
                                                        float* output_left,
                                                        float* output_right) noexcept;

  [[nodiscard]] std::size_t bus_count_for_control() const noexcept;
  [[nodiscard]] std::size_t max_frames_for_control() const noexcept;
  void reclaim_retired() noexcept { publisher_.reclaim_retired(); }

private:
  struct Snapshot {
    RealtimeBusEngine engine;
    std::size_t master_bus_index{0};
  };
  RealtimeGraphPublisher<Snapshot> publisher_;
};

}  // namespace hush
