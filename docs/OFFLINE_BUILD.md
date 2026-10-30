# Offline build — HUSH DAW aligned with Cosmic Studio

## System dependencies (preinstalled; nothing is downloaded)

| Dependency | Required for | Notes |
|------------|--------------|-------|
| CMake ≥ 3.16 | all | |
| C++20 compiler (g++/clang) | all | |
| Threads | cosmic tests | system |
| X11 | optional no-frills GUI | system |
| ALSA | optional live host | system; opt-in |
| Qt6 Widgets | optional mixer GUI | system; opt-in |

## Build (air-gap safe)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`FETCHCONTENT_FULLY_DISCONNECTED=ON` is forced. No `FetchContent` / `ExternalProject` / `file(DOWNLOAD)` in in-tree CMakeLists.

## Explicitly excluded

- **DAW checker** / validation harness / test-orchestration daemon
- Telemetry SDKs, crash uploads, call-home
- Configure-time package bootstrap

## CI note

GitHub Actions workflows under `.github/workflows/` may download release archives for integration jobs. That is **CI network use**, not the local offline build path. Local/default builds must not require network.
