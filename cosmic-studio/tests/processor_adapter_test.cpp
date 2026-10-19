#include "hush_cosmic/processor_adapter.hpp"

#include <cassert>
#include <cstddef>

namespace {
struct Gain {
    float amount{1.0F};
};

void apply_gain(void* opaque, float* samples, std::size_t frames,
                std::size_t channels) noexcept {
    auto* gain = static_cast<Gain*>(opaque);
    for (std::size_t i = 0; i < frames * channels; ++i) {
        samples[i] *= gain->amount;
    }
}
} // namespace

int main() {
    hush::cosmic::ProcessorAdapter adapter;
    float samples[] = {1.0F, -0.5F, 0.25F, -0.25F};

    // Unbound means safe pass-through, not a fabricated processor result.
    adapter.process(samples, 2, 2);
    assert(samples[0] == 1.0F && samples[1] == -0.5F);

    Gain gain{0.5F};
    adapter.bind_while_stopped({&gain, &apply_gain});
    assert(adapter.available());
    assert(adapter.enabled());

    adapter.process(samples, 2, 2);
    assert(samples[0] == 0.5F);
    assert(samples[1] == -0.25F);
    assert(samples[2] == 0.125F);
    assert(samples[3] == -0.125F);

    adapter.set_enabled(false);
    adapter.process(samples, 2, 2);
    assert(samples[0] == 0.5F); // disabled = bypass

    adapter.process(nullptr, 2, 2);
    adapter.process(samples, 0, 2);
    adapter.process(samples, 2, 0);
}
