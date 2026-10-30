#include "hush/preallocated_bus_processor.hpp"
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace hush {

bool PreallocatedBusProcessor::configure(const BusGraph& graph, std::size_t max_frames,
                                         ProcessorSetupError& error) {
  configured_ = false;
  buses_.clear();
  routes_.clear();
  render_order_.clear();
  max_frames_ = 0;
  if (!graph.valid()) {
    error = {"graph_invalid", "", "BusGraph must be valid before configure"};
    return false;
  }
  if (max_frames == 0) {
    error = {"max_frames_zero", "", "max_frames must be > 0"};
    return false;
  }

  const auto& gbuses = graph.buses();
  const auto& groutes = graph.routes();
  const auto order_ids = graph.render_order();

  std::unordered_map<std::string, std::size_t> index;
  buses_.resize(gbuses.size());
  for (std::size_t i = 0; i < gbuses.size(); ++i) {
    buses_[i].id = gbuses[i].id;
    buses_[i].pre_left.assign(max_frames, 0.f);
    buses_[i].pre_right.assign(max_frames, 0.f);
    buses_[i].post_left.assign(max_frames, 0.f);
    buses_[i].post_right.assign(max_frames, 0.f);
    index[gbuses[i].id] = i;
  }
  for (const auto& id : order_ids) {
    auto it = index.find(id);
    if (it == index.end()) {
      error = {"order_unknown", id, "render order references unknown bus"};
      return false;
    }
    render_order_.push_back(it->second);
  }
  routes_.reserve(groutes.size());
  for (const auto& r : groutes) {
    routes_.push_back({index[r.source_bus_id], index[r.destination_bus_id], r.tap, r.gain});
  }
  max_frames_ = max_frames;
  configured_ = true;
  error = {};
  return true;
}

RealtimeProcessStatus PreallocatedBusProcessor::process(const BusInputView* inputs,
                                                        std::size_t input_count,
                                                        std::size_t frame_count) noexcept {
  if (!configured_) return RealtimeProcessStatus::NotConfigured;
  if (frame_count > max_frames_) return RealtimeProcessStatus::FrameCountExceedsCapacity;
  if (input_count != buses_.size()) return RealtimeProcessStatus::InvalidInputCount;
  if (inputs == nullptr && input_count > 0) return RealtimeProcessStatus::NullChannel;

  for (std::size_t i = 0; i < input_count; ++i) {
    if (inputs[i].left == nullptr || inputs[i].right == nullptr)
      return RealtimeProcessStatus::NullChannel;
    if (!std::isfinite(inputs[i].fader_gain) || inputs[i].fader_gain < 0.f ||
        inputs[i].fader_gain > 16.f)
      return RealtimeProcessStatus::InvalidFaderGain;
  }

  // Clear bus accumulators (preallocated; no heap)
  for (auto& b : buses_) {
    std::memset(b.pre_left.data(), 0, frame_count * sizeof(float));
    std::memset(b.pre_right.data(), 0, frame_count * sizeof(float));
    std::memset(b.post_left.data(), 0, frame_count * sizeof(float));
    std::memset(b.post_right.data(), 0, frame_count * sizeof(float));
  }

  // Mix inputs into bus pre buffers in render order
  for (std::size_t bi : render_order_) {
    auto& bus = buses_[bi];
    const auto& in = inputs[bi];
    for (std::size_t f = 0; f < frame_count; ++f) {
      bus.pre_left[f] += in.left[f];
      bus.pre_right[f] += in.right[f];
    }
    const float g = in.fader_gain;
    for (std::size_t f = 0; f < frame_count; ++f) {
      bus.post_left[f] = bus.pre_left[f] * g;
      bus.post_right[f] = bus.pre_right[f] * g;
    }
  }

  // Apply routes (send accumulation into destination pre, then recompute post)
  for (const auto& r : routes_) {
    auto& src = buses_[r.source];
    auto& dst = buses_[r.destination];
    const float* sl = (r.tap == SendTap::PreFader) ? src.pre_left.data() : src.post_left.data();
    const float* sr = (r.tap == SendTap::PreFader) ? src.pre_right.data() : src.post_right.data();
    for (std::size_t f = 0; f < frame_count; ++f) {
      dst.pre_left[f] += sl[f] * r.gain;
      dst.pre_right[f] += sr[f] * r.gain;
    }
  }
  // Refresh post for destinations that received sends (simple: all buses)
  for (std::size_t bi : render_order_) {
    auto& bus = buses_[bi];
    const float g = inputs[bi].fader_gain;
    for (std::size_t f = 0; f < frame_count; ++f) {
      bus.post_left[f] = bus.pre_left[f] * g;
      bus.post_right[f] = bus.pre_right[f] * g;
    }
  }

  return RealtimeProcessStatus::Ok;
}

StereoConstView PreallocatedBusProcessor::pre_fader(std::size_t bus_index,
                                                    std::size_t frames) const noexcept {
  if (!configured_ || bus_index >= buses_.size() || frames > max_frames_)
    return {};
  return {buses_[bus_index].pre_left.data(), buses_[bus_index].pre_right.data(), frames};
}

StereoConstView PreallocatedBusProcessor::post_fader(std::size_t bus_index,
                                                     std::size_t frames) const noexcept {
  if (!configured_ || bus_index >= buses_.size() || frames > max_frames_)
    return {};
  return {buses_[bus_index].post_left.data(), buses_[bus_index].post_right.data(), frames};
}

}  // namespace hush
