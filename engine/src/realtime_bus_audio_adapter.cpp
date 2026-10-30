#include "hush/realtime_bus_audio_adapter.hpp"
#include <cstring>

namespace hush {

bool RealtimeBusAudioAdapter::configure(RealtimeBusEngine& engine,
                                        std::size_t input_bus_index,
                                        std::size_t master_bus_index,
                                        std::size_t max_frames,
                                        std::string& error) {
  if (!engine.configured()) {
    error = "engine not configured";
    return false;
  }
  if (max_frames == 0 || max_frames > engine.max_frames()) {
    error = "max_frames invalid";
    return false;
  }
  // bus indices validated at process time against engine state
  engine_ = &engine;
  input_bus_index_ = input_bus_index;
  master_bus_index_ = master_bus_index;
  max_frames_ = max_frames;
  error.clear();
  return true;
}

bool RealtimeBusAudioAdapter::process(const float* interleaved_input,
                                      float* interleaved_output,
                                      std::size_t frames) noexcept {
  if (!engine_ || !interleaved_output) return false;
  if (frames == 0) return true;
  if (frames > max_frames_) {
    // Only clear the configured capacity — caller buffer size is unknown beyond that
    std::memset(interleaved_output, 0, max_frames_ * 2 * sizeof(float));
    return false;
  }
  return engine_->process_interleaved(interleaved_input, interleaved_output, frames,
                                      input_bus_index_, master_bus_index_);
}

void RealtimeBusAudioAdapter::alsa_callback(const float* input, float* output,
                                            std::size_t frames,
                                            void* user_data) noexcept {
  auto* self = static_cast<RealtimeBusAudioAdapter*>(user_data);
  if (!self) {
    if (output && frames) std::memset(output, 0, frames * 2 * sizeof(float));
    return;
  }
  (void)self->process(input, output, frames);
}

}  // namespace hush
