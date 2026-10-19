# HUSH Cosmic Studio — Bridge Source Update 0.2.1

This directory is a small, standalone C++20 integration module proposed for HUSH.
It deliberately does not replace the existing DAW or processor source archives.

## Included

- `SpscQueue<T, Capacity>`: fixed-capacity single-producer/single-consumer command queue. Usable capacity is `Capacity - 1`. One producer and one consumer are required.
- `TelemetryChannel`: atomic-field telemetry publication with bounded UI reads. A failed read reports unavailable data rather than a fake zero measurement.
- `orbital_point()`: small, allocation-free Observatory layout helper intended for the UI thread.
- A CTest contract executable covering queue capacity/order, telemetry validity, and orbital geometry.

## Build and test

From this directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Integration boundary

This module is not yet wired into the actual HUSH 2.2.0 engine or PortAudio callback. The adapter must be connected to the canonical engine after its source archive is unpacked and reviewed. The audio thread should be the sole queue consumer and telemetry writer; the UI thread should be the sole queue producer and telemetry reader.

The queue uses no heap allocation or blocking waits in `try_push`/`try_pop`. Whether the chosen atomics are lock-free on a specific target must be checked on that target. This module does not yet implement a native X11 window, audio device backend, SIMD DSP kernel, plugin host, or full session/timeline UI.

## Verification status

Source has been added to a branch for review. Do not treat it as compiled, tested, hardware-verified, or production-ready until the build and CTest commands above are run in a suitable C++20 environment and the adapter is integrated with the live engine.
