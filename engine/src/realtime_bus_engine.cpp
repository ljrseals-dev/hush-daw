#include "hush/realtime_bus_engine.hpp"
#include <cmath>
#include <cstring>

namespace hush {

bool RealtimeBusEngine::configure(const BusGraph& graph, std::size_t max_frames,
                                  ProcessorSetupError& error) {
  configured_ = false;
  if (!processor_.configure(graph, max_frames, error)) return false;
  max_frames_ = max_frames;
  const std::size_t n = processor_.bus_count();
  bus_views_.assign(n, {});
  gains_.assign(n, 1.0F);
  silence_.assign(max_frames, 0.f);
  scratch_in_.configure(max_frames);
  scratch_out_.configure(max_frames);
  configured_ = true;
  return true;
}

void RealtimeBusEngine::drain_params() noexcept {
  ParamCommand cmd;
  while (params_.try_pop(cmd)) {
    if (cmd.target >= gains_.size()) continue;
    if (cmd.kind == 0) {  // gain
      if (std::isfinite(cmd.value) && cmd.value >= 0.f && cmd.value <= 16.f)
        gains_[cmd.target] = cmd.value;
    } else if (cmd.kind == 2) {  // mute: value>0.5 → mute
      gains_[cmd.target] = (cmd.value > 0.5f) ? 0.f : 1.f;
    }
    // pan (kind 1) reserved — stereo bus processor is sum path for now
  }
}

RealtimeProcessStatus RealtimeBusEngine::process_block(BusInputView* inputs,
                                                       std::size_t input_count,
                                                       std::size_t frames,
                                                       std::size_t master_index,
                                                       float* out_left,
                                                       float* out_right) noexcept {
  if (!configured_ || !out_left || !out_right)
    return RealtimeProcessStatus::NotConfigured;
  if (frames > max_frames_) return RealtimeProcessStatus::FrameCountExceedsCapacity;

  drain_params();

  if (inputs && input_count == gains_.size()) {
    for (std::size_t i = 0; i < input_count; ++i)
      inputs[i].fader_gain = gains_[i];
  }

  const auto st = processor_.process(inputs, input_count, frames);
  if (st != RealtimeProcessStatus::Ok) {
    std::memset(out_left, 0, frames * sizeof(float));
    std::memset(out_right, 0, frames * sizeof(float));
    return st;
  }

  if (master_index >= processor_.bus_count()) {
    std::memset(out_left, 0, frames * sizeof(float));
    std::memset(out_right, 0, frames * sizeof(float));
    return RealtimeProcessStatus::InvalidInputCount;
  }

  auto post = processor_.post_fader(master_index, frames);
  if (!post.left || !post.right) {
    std::memset(out_left, 0, frames * sizeof(float));
    std::memset(out_right, 0, frames * sizeof(float));
    return RealtimeProcessStatus::NullChannel;
  }
  std::memcpy(out_left, post.left, frames * sizeof(float));
  std::memcpy(out_right, post.right, frames * sizeof(float));
  return RealtimeProcessStatus::Ok;
}

bool RealtimeBusEngine::process_interleaved(const float* in, float* out,
                                            std::size_t frames,
                                            std::size_t input_bus_index,
                                            std::size_t master_bus_index) noexcept {
  if (!configured_ || !out) return false;
  if (frames > max_frames_) {
    if (out) std::memset(out, 0, frames * 2 * sizeof(float));
    return false;
  }

  if (in) {
    if (!scratch_in_.deinterleave(in, frames)) {
      std::memset(out, 0, frames * 2 * sizeof(float));
      return false;
    }
  } else {
    scratch_in_.clear(frames);
  }

  const std::size_t n = bus_views_.size();
  for (std::size_t i = 0; i < n; ++i) {
    if (i == input_bus_index) {
      bus_views_[i] = {scratch_in_.left(), scratch_in_.right(), gains_[i]};
    } else {
      bus_views_[i] = {silence_.data(), silence_.data(), gains_[i]};
    }
  }

  const auto st = process_block(bus_views_.data(), n, frames, master_bus_index,
                                scratch_out_.left(), scratch_out_.right());
  if (st != RealtimeProcessStatus::Ok) {
    std::memset(out, 0, frames * 2 * sizeof(float));
    return false;
  }
  return scratch_out_.interleave(out, frames);
}

}  // namespace hush
