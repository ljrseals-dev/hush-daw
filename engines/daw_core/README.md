# hush-daw-core  (complete umbrella)

Tier-3 session engine that **owns** immersive, score, plugins and video.

## What it is

- Single `Session` object that creates and owns all four subsystems via stable opaque handles.
- Concrete `RenderNode` adapters (ImmersiveNode, ScoreNode, PluginNode) that produce real audio.
- Transport with play / stop / record and optional loop.
- Offline render graph → interleaved float32 WAV.
- HUSHSESS v1 binary session format.
- CLI `hush-session` with a working `demo` command that writes an audible 2-second WAV.
- Self-contained stubs so the engine builds and tests with **zero external engine dependencies**.
- When the real Tier-1/2 engines are present, set `-DHUSH_USE_STUBS=OFF` and link them.

## Build (standalone)

```bash
cmake -S engines/daw_core -B build-daw -DCMAKE_BUILD_TYPE=Release
cmake --build build-daw -j$(nproc)
ctest --test-dir build-daw --output-on-failure
```

## CLI

```bash
# Help
./build-daw/hush-session --help

# Create empty session
./build-daw/hush-session new demo.hushsess --rate 48000 --length 480000

# Inspect
./build-daw/hush-session info demo.hushsess

# Full demo (immersive tone + arpeggio + gain) → audible WAV
./build-daw/hush-session demo demo.wav
```

## Parent integration

```cmake
add_subdirectory(engines/daw_core)
# or with real engines:
# set(HUSH_USE_STUBS OFF)
# add_subdirectory(engines/daw_core)
```

## License

MIT — Copyright (c) 2026 ljrseals-dev
