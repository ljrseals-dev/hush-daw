# HUSH Cosmic Studio — Bridge Source Update 0.2.1

This directory is a small, standalone C++20 integration module proposed for HUSH. It does not replace the existing DAW or processor source archives.

## Included

- `SpscQueue<T, Capacity>`: fixed-capacity single-producer/single-consumer command queue. Usable capacity is `Capacity - 1`.
- `TelemetryChannel`: atomic-field telemetry publication with bounded UI reads. A failed read reports unavailable data rather than a fake zero measurement.
- `orbital_point()`: allocation-free Observatory layout helper intended for the UI thread.
- `ProcessorAdapter`: in-place float-buffer adapter accepting a processor-owned noexcept function pointer. It bypasses safely when unbound or disabled.
- CTest contract executables for the queue/telemetry/layout helpers and adapter pass-through/processing behavior.

## Build and test

From this directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Processor and DAW integration status

The adapter is an integration seam, not a direct link to HUSH Processor internals yet. The HUSH Processor repository currently publishes a reconciled source archive, but its binary ZIP contents could not be unpacked and audited through the GitHub source connector in this session. Before wiring a concrete processor instance, unpack and review that archive, identify the canonical DSP process API, and bind it while the DAW audio engine is stopped. The processor instance must remain alive until audio processing has stopped.

In the eventual audio callback, keep the binding immutable while running; call `process()` only after the engine has validated buffer capacity and layout. Do not construct processor objects, allocate, perform file/network I/O, or take blocking locks in the callback. The UI thread should be the sole queue producer and telemetry reader; the audio thread should be the sole queue consumer and telemetry writer.

This module is not yet connected to the actual HUSH 2.2.0 mixer or PortAudio callback, and it does not provide a native X11 window, a SIMD DSP kernel, a third-party plugin host, or a complete session/timeline UI.

## Verification status

The source is on a review branch. No local compile or CTest run was possible in this session, so the module must not be described as compiled, tested, hardware-verified, or production-ready until CI or a local C++20 build confirms it.
