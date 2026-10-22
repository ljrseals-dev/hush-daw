#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace hush::cosmic {

// Narrow ABI seam between the HUSH DAW audio callback and a processor-owned
// DSP entry point. The processor instance must be created/configured outside
// the real-time callback and remain alive until processing has stopped.
using ProcessorProcessFn = void (*)(void* instance,
                                    float* interleaved,
                                    std::size_t frames,
                                    std::size_t channels) noexcept;

struct ProcessorBinding {
    void* instance{nullptr};
    ProcessorProcessFn process{nullptr};
};

// Install or replace bindings only while the audio engine is stopped. This
// avoids lifetime races and avoids locks/ref-count operations in process().
class ProcessorAdapter final {
public:
    ProcessorAdapter() noexcept = default;
    ProcessorAdapter(const ProcessorAdapter&) = delete;
    ProcessorAdapter& operator=(const ProcessorAdapter&) = delete;

    // Call only while the engine is stopped; binding_ is plain data by design.
    void bind_while_stopped(ProcessorBinding binding) noexcept {
        binding_ = binding;
        enabled_.store(binding.instance != nullptr && binding.process != nullptr,
                       std::memory_order_release);
    }

    void set_enabled(bool enabled) noexcept {
        enabled_.store(enabled, std::memory_order_release);
    }

    [[nodiscard]] bool available() const noexcept {
        return binding_.instance != nullptr && binding_.process != nullptr;
    }

    [[nodiscard]] bool enabled() const noexcept {
        return enabled_.load(std::memory_order_acquire) && available();
    }

    // In-place interleaved float processing. If no processor is bound or the
    // effect is disabled, audio is left unchanged (explicit bypass behavior).
    // Caller must validate frames/channels and buffer capacity before calling.
    void process(float* interleaved,
                 std::size_t frames,
                 std::size_t channels) noexcept {
        if (interleaved == nullptr || frames == 0 || channels == 0 || !enabled()) {
            return;
        }
        binding_.process(binding_.instance, interleaved, frames, channels);
    }

private:
    ProcessorBinding binding_{};
    std::atomic<bool> enabled_{false};
};

} // namespace hush::cosmic
