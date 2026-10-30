#include "hush/realtime_bus_engine_host_audio_adapter.hpp"

namespace hush {

bool RealtimeBusEngineHostAudioAdapter::configure(RealtimeBusEngineHost& host,
                                                  std::size_t input_bus_index,
                                                  std::size_t bus_count,
                                                  std::size_t max_frames,
                                                  std::string& error) {
  if (max_frames == 0 || bus_count == 0) {
    error = "max_frames/bus_count must be > 0";
    return false;
  }
  if (input_bus_index >= bus_count) {
    error = "input_bus_index out of range";
    return false;
  }
  host_ = &host;
  input_bus_index_ = input_bus_index;
  bus_count_ = bus_count;
  max_frames_ = max_frames;
  scratch_in_.configure(max_frames);
  scratch_out_.configure(max_frames);
  silence_.assign(max_frames, 0.f);
  bus_views_.assign(bus_count, {});
  error.clear();
  return true;
}

bool RealtimeBusEngineHostAudioAdapter::process(const float* interleaved_input,
                                                float* interleaved_output,
                                                std::size_t frames) noexcept {
  if (!host_ || !interleaved_output) return false;
  if (frames == 0) return true;
  if (frames > max_frames_) {
    std::memset(interleaved_output, 0, max_frames_ * 2 * sizeof(float));
    return false;
  }

  if (interleaved_input) {
    if (!scratch_in_.deinterleave(interleaved_input, frames)) {
      std::memset(interleaved_output, 0, frames * 2 * sizeof(float));
      return false;
    }
  } else {
    scratch_in_.clear(frames);
  }

  for (std::size_t i = 0; i < bus_count_; ++i) {
    if (i == input_bus_index_) {
      bus_views_[i] = {scratch_in_.left(), scratch_in_.right(), 1.f};
    } else {
      bus_views_[i] = {silence_.data(), silence_.data(), 1.f};
    }
  }

  const auto st =
      host_->process_to_stereo(bus_views_.data(), bus_count_, frames, scratch_out_.left(),
                               scratch_out_.right());
  if (st != RealtimeProcessStatus::Ok) {
    std::memset(interleaved_output, 0, frames * 2 * sizeof(float));
    return false;
  }
  return scratch_out_.interleave(interleaved_output, frames);
}

void RealtimeBusEngineHostAudioAdapter::alsa_callback(const float* input, float* output,
                                                      std::size_t frames,
                                                      void* user_data) noexcept {
  auto* self = static_cast<RealtimeBusEngineHostAudioAdapter*>(user_data);
  if (!self) {
    if (output && frames) std::memset(output, 0, frames * 2 * sizeof(float));
    return;
  }
  (void)self->process(input, output, frames);
}

}  // namespace hush
