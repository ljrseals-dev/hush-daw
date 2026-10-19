# Hush Processor Integration

This change adds a standalone C++20 bridge starter under integration/hush-processor-bridge/. It defines an audio-block contract, a fixed-capacity single-producer/single-consumer control queue, parameter validation, and a smoke test. Named controls: Spectral Denoise, Hiss Reduction, Click Removal, and Hum Eliminator.

Build:
    cmake -S integration/hush-processor-bridge -B build/hush-processor-bridge -DCMAKE_BUILD_TYPE=Release
    cmake --build build/hush-processor-bridge
    ctest --test-dir build/hush-processor-bridge --output-on-failure

Status: boundary layer only. It does not yet call Hush Processor 2.2.0's concrete engine. The actual source API must be mapped from the published archive before choosing constructor, prepare, process, and slot-control calls. Source: https://github.com/ljrseals-dev/hush-processor

Before a concrete adapter is merged:
1. Inspect the processor archive's CMake targets and public headers.
2. Map prepare/process/set_slot to verified engine APIs.
3. Connect the adapter to the DAW's real-time mixer and offline renderer.
4. Test tone/impulse pass-through, slot toggling, invalid configurations, and deterministic offline render.
5. Run the DAW build and CTest suite on clean Linux.

No telemetry or network access is added by this bridge. This starter is not evidence that the full DAW and processor build or run end-to-end.
