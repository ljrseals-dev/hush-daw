#include "hush/ProcessorBridge.hpp"
#include <array>
#include <cassert>
using namespace hush::integration;
class TestProcessor final : public IProcessorAdapter {
public:
    int processed{0}, changed{0};
    bool prepare(double, std::uint32_t, std::size_t) noexcept override { return true; }
    void process(AudioBlock b) noexcept override {
        ++processed; for (std::size_t i=0; i < b.frames*b.channels; ++i) b.samples[i] *= 0.5F;
    }
    bool set_slot(Slot, SlotState) noexcept override { ++changed; return true; }
};
int main() {
    TestProcessor p; ProcessorBridge bridge(p);
    assert(bridge.prepare(48000.0, 2, 256));
    assert(bridge.queue_slot(Slot::HissReduction, SlotState{true, 0.4F}));
    std::array<float,16> samples{}; samples.fill(1.0F);
    bridge.process(AudioBlock{samples.data(),8,2,48000.0});
    assert(p.processed == 1 && p.changed == 1 && samples[0] == 0.5F);
    bridge.process(AudioBlock{samples.data(),8,2,44100.0});
    assert(bridge.rejected_blocks() == 1 && p.processed == 1);
    assert(!bridge.queue_slot(Slot::HumEliminator, SlotState{true,1.5F}));
}
