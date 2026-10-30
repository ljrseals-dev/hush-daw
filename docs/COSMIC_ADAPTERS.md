# Cosmic adapters — implemented on hush-daw

All Cosmic Studio audio/UI adapters ported onto the hardened engine core.
Security (PathSandbox, offline/privacy) is unchanged.

| Adapter | Header | Role |
|---------|--------|------|
| **RealtimeBusAudioAdapter** | `realtime_bus_audio_adapter.hpp` | Interleaved stereo ↔ `RealtimeBusEngine`; `alsa_callback` |
| **MixerEngineBridge** | `mixer_engine_bridge.hpp` | Track ID → bus index; `enqueue_gain` / `enqueue_mute` |
| **RealtimeGraphPublisher** | `realtime_graph_publisher.hpp` | Control publish / audio hold of snapshots |
| **RealtimeBusEngineHost** | `realtime_bus_engine_host.hpp` | Graph replace + `process_to_stereo` |
| **RealtimeBusEngineHostAudioAdapter** | `realtime_bus_engine_host_audio_adapter.hpp` | Interleaved path for host; `alsa_callback` |

Evidence: `hush_cosmic_adapters_tests` — 0 failures.
