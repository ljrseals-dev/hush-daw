# Cosmic Studio alignment (hush-daw)

## Contracts mirrored

| Contract | Implementation in hush-daw |
|----------|----------------------------|
| Zero telemetry | Privacy denylist CTest + docs |
| Offline build | Root CMake + offline_cmake_audit |
| Realtime callback discipline | SPSC ParameterQueue + cosmic SpscQueue docs |
| UI/audio separation | UI pushes commands; audio pops only |
| Bus-graph validation | `engine/src/bus_graph.cpp` cycle detection |
| No DAW checker | Not present; audits are source denylist only |

## Modules

- `cosmic-studio/` — existing bridge 0.2.1 (SpscQueue, ProcessorAdapter, Observatory)
- `engine/` — BusGraph + ParameterQueue (Cosmic API parity)

## Not claimed

Full production mixer path, VST3 host, or live ALSA soak without hardware evidence.
