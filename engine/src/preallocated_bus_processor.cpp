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
    const float g = inputs[i].fader_gain;
    if (!std::isfinite(g) || g < 0.f || g > 16.f)
      return RealtimeProcessStatus::InvalidFaderGain;
  }

  const std::size_t bytes = frame_count * sizeof(float);

  // Clear only pre (post written fully below)
  for (auto& b : buses_) {
    std::memset(b.pre_left.data(), 0, bytes);
    std::memset(b.pre_right.data(), 0, bytes);
  }

  // Stage 1: inputs → pre, then post = pre * fader (in render order)
  for (std::size_t bi : render_order_) {
    auto& bus = buses_[bi];
    const auto& in = inputs[bi];
    float* __restrict pl = bus.pre_left.data();
    float* __restrict pr = bus.pre_right.data();
    float* __restrict ol = bus.post_left.data();
    float* __restrict or_ = bus.post_right.data();
    const float* __restrict il = in.left;
    const float* __restrict ir = in.right;
    const float g = in.fader_gain;

    if (g == 1.0f) {
      for (std::size_t f = 0; f < frame_count; ++f) {
        pl[f] += il[f];
        pr[f] += ir[f];
        ol[f] = pl[f];
        or_[f] = pr[f];
      }
    } else if (g == 0.0f) {
      for (std::size_t f = 0; f < frame_count; ++f) {
        pl[f] += il[f];
        pr[f] += ir[f];
        ol[f] = 0.f;
        or_[f] = 0.f;
      }
    } else {
      for (std::size_t f = 0; f < frame_count; ++f) {
        pl[f] += il[f];
        pr[f] += ir[f];
        ol[f] = pl[f] * g;
        or_[f] = pr[f] * g;
      }
    }
  }

  // Stage 2: routes accumulate into destination pre, then refresh destination post
  if (!routes_.empty()) {
    for (const auto& r : routes_) {
      auto& src = buses_[r.source];
      auto& dst = buses_[r.destination];
      const float* __restrict sl =
          (r.tap == SendTap::PreFader) ? src.pre_left.data() : src.post_left.data();
      const float* __restrict sr =
          (r.tap == SendTap::PreFader) ? src.pre_right.data() : src.post_right.data();
      float* __restrict dl = dst.pre_left.data();
      float* __restrict dr = dst.pre_right.data();
      const float rg = r.gain;
      if (rg == 1.0f) {
        for (std::size_t f = 0; f < frame_count; ++f) {
          dl[f] += sl[f];
          dr[f] += sr[f];
        }
      } else {
        for (std::size_t f = 0; f < frame_count; ++f) {
          dl[f] += sl[f] * rg;
          dr[f] += sr[f] * rg;
        }
      }
    }
    // Refresh post only for destinations that received sends (all destinations in routes)
    // Safer: refresh all buses' post from pre * fader (single pass)
    for (std::size_t bi : render_order_) {
      auto& bus = buses_[bi];
      const float g = inputs[bi].fader_gain;
      float* __restrict ol = bus.post_left.data();
      float* __restrict or_ = bus.post_right.data();
      const float* __restrict pl = bus.pre_left.data();
      const float* __restrict pr = bus.pre_right.data();
      if (g == 1.0f) {
        std::memcpy(ol, pl, bytes);
        std::memcpy(or_, pr, bytes);
      } else if (g == 0.0f) {
        std::memset(ol, 0, bytes);
        std::memset(or_, 0, bytes);
      } else {
        for (std::size_t f = 0; f < frame_count; ++f) {
          ol[f] = pl[f] * g;
          or_[f] = pr[f] * g;
        }
      }
    }
  }

  return RealtimeProcessStatus::Ok;
}

StereoConstView PreallocatedBusProcessor::pre_fader(std::size_t bus_index,
                                                    std::size_t frames) const noexcept {
  if (!configured_ || bus_index >= buses_.size() || frames > max_frames_) return {};
  return {buses_[bus_index].pre_left.data(), buses_[bus_index].pre_right.data(), frames};
}

StereoConstView PreallocatedBusProcessor::post_fader(std::size_t bus_index,
                                                     std::size_t frames) const noexcept {
  if (!configured_ || bus_index >= buses_.size() || frames > max_frames_) return {};
  return {buses_[bus_index].post_left.data(), buses_[bus_index].post_right.data(), frames};
}

}  // namespace hush
