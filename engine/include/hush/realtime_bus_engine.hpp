#pragma once
#include "hush/bus_graph.hpp"
#include "hush/parameter_queue.hpp"
#include "hush/preallocated_bus_processor.hpp"
#include "hush/stereo_adapter.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hush {

// Control thread: configure(). Audio thread: process_block() only.
// Drains ParameterQueue (UI producer → audio consumer) then runs the bus processor.
class RealtimeBusEngine {
public:
  using ParamQ = ParameterQueue<256>;

  bool configure(const BusGraph& graph, std::size_t max_frames, ProcessorSetupError& error);

  // UI / control thread only
  [[nodiscard]] bool push_param(const ParamCommand& cmd) noexcept {
    return params_.try_push(cmd);
  }

  // Audio callback — no alloc, no I/O, no logging
  // inputs: one BusInputView per bus (planar). master_index selects output bus.
  [[nodiscard]] RealtimeProcessStatus process_block(BusInputView* inputs,
                                                    std::size_t input_count,
                                                    std::size_t frames,
                                                    std::size_t master_index,
                                                    float* out_left,
                                                    float* out_right) noexcept;

  // Interleaved stereo convenience path (device buffers)
  [[nodiscard]] bool process_interleaved(const float* in, float* out,
                                         std::size_t frames,
                                         std::size_t input_bus_index,
                                         std::size_t master_bus_index) noexcept;

  [[nodiscard]] std::uint64_t dropped_params() const noexcept {
    return dropped_params_.load(std::memory_order_relaxed);
  }
  [[nodiscard]] bool configured() const noexcept { return configured_; }
  [[nodiscard]] std::size_t max_frames() const noexcept { return max_frames_; }

private:
  void drain_params() noexcept;

  PreallocatedBusProcessor processor_;
  ParamQ params_;
  StereoScratch scratch_in_;
  StereoScratch scratch_out_;
  std::vector<BusInputView> bus_views_;
  std::vector<float> silence_;
  std::vector<float> gains_;  // per-bus fader from param queue
  std::size_t max_frames_{0};
  bool configured_{false};
  std::atomic<std::uint64_t> dropped_params_{0};
};

}  // namespace hush
