#pragma once
#include "hush/realtime_bus_engine_host.hpp"
#include "hush/stereo_adapter.hpp"
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace hush {

// Preallocated interleaved-stereo bridge for RealtimeBusEngineHost.
// Configure before starting the device. Topology must stay stable while active.
class RealtimeBusEngineHostAudioAdapter {
public:
  bool configure(RealtimeBusEngineHost& host, std::size_t input_bus_index,
                 std::size_t bus_count, std::size_t max_frames, std::string& error);

  [[nodiscard]] bool process(const float* interleaved_input, float* interleaved_output,
                             std::size_t frames) noexcept;

  static void alsa_callback(const float* input, float* output, std::size_t frames,
                            void* user_data) noexcept;

private:
  RealtimeBusEngineHost* host_{nullptr};
  std::size_t input_bus_index_{0};
  std::size_t bus_count_{0};
  std::size_t max_frames_{0};
  StereoScratch scratch_in_;
  StereoScratch scratch_out_;
  std::vector<BusInputView> bus_views_;
  std::vector<float> silence_;
};

}  // namespace hush
