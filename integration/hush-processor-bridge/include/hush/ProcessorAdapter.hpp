#pragma once
#include <cstddef>
#include <cstdint>
namespace hush::integration {
struct AudioBlock {
    float* samples{};
    std::size_t frames{};
    std::uint32_t channels{};
    double sample_rate{};
    [[nodiscard]] bool valid() const noexcept {
        return samples != nullptr && frames > 0 && channels > 0 &&
               sample_rate >= 8000.0 && sample_rate <= 384000.0;
    }
};
enum class Slot : std::uint8_t { SpectralDenoise = 0, HissReduction, ClickRemoval, HumEliminator, Count };
struct SlotState { bool enabled{false}; float amount{0.0F}; };
class IProcessorAdapter {
public:
    virtual ~IProcessorAdapter() = default;
    virtual bool prepare(double sample_rate, std::uint32_t channels, std::size_t max_frames) noexcept = 0;
    virtual void process(AudioBlock block) noexcept = 0;
    virtual bool set_slot(Slot slot, SlotState state) noexcept = 0;
};
}
