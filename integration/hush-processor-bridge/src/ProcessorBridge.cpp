#include "hush/ProcessorBridge.hpp"
#include <cmath>
namespace hush::integration {
bool ProcessorBridge::prepare(double rate, std::uint32_t channels, std::size_t max_frames) noexcept {
    if (!std::isfinite(rate) || rate < 8000.0 || rate > 384000.0 || channels == 0 || channels > 64 || max_frames == 0) return false;
    if (!processor_.prepare(rate, channels, max_frames)) return false;
    sample_rate_ = rate; channels_ = channels; max_frames_ = max_frames; return true;
}
bool ProcessorBridge::queue_slot(Slot slot, SlotState state) noexcept {
    if (slot >= Slot::Count || !std::isfinite(state.amount) || state.amount < 0.0F || state.amount > 1.0F) return false;
    return commands_.push(SlotCommand{slot, state});
}
void ProcessorBridge::process(AudioBlock block) noexcept {
    if (!block.valid() || block.channels != channels_ || block.frames > max_frames_ || block.sample_rate != sample_rate_) {
        rejected_blocks_.fetch_add(1, std::memory_order_relaxed); return;
    }
    SlotCommand command{};
    while (commands_.pop(command)) (void)processor_.set_slot(command.slot, command.state);
    processor_.process(block);
}
}
