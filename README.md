# HUSH DAW — Complete Canonical Build

**Version:** 2.2.1  
**Status:** Offline-first · Security-hardened · Tier 3 complete · Cosmic adapters optimized  
**License:** MIT (where present)

Canonical **hush-daw**: same processor family as public Cosmic Studio, with the **full security posture**. Cosmic is a de-hardened fork for creators. **Backport defects only — never de-harden.**

---

## Everything in this tree

### Engine core (`engine/`)

| Module | Purpose |
|--------|---------|
| **BusGraph** | Cycle detection, gain ∈ [0,16], topological render order |
| **ParameterQueue** | SPSC UI→audio; full returns false (no block/alloc) |
| **PreallocatedBusProcessor** | configure on control; process() no alloc/I/O/log |
| **RealtimeBusEngine** | Drain params + bus process; interleaved stereo path |
| **StereoScratch** | Optimized deinterleave/interleave (4-wide unroll) |
| **PathSandbox** | Reject `..`; optional session root jail |

### Cosmic adapters (ported + optimized)

| Adapter | Role |
|---------|------|
| **RealtimeBusAudioAdapter** | Device interleaved ↔ engine; `alsa_callback` |
| **MixerEngineBridge** | Track ID → bus; O(1) gain/mute enqueue |
| **RealtimeGraphPublisher** | Double-slot publish; audio holds raw pointer only |
| **RealtimeBusEngineHost** | Hot graph replace + `process_to_stereo` |
| **RealtimeBusEngineHostAudioAdapter** | Host interleaved path + `alsa_callback` |

Perf (reference): **~0.4 µs/block** @ 64 frames stereo (-O3); oversize reject silences only `max_frames` capacity (ASan-clean).

### Tier 3 — `engines/daw_core`

- Session / Transport / RenderGraph  
- HUSHSESS save/load  
- CLI **`hush-session`**: `new` · `info` · `render` · `demo`  
- Stubs for immersive/score/video/plugins when Tier 1 absent  

### Tier 3 — `engines/composer`

- Interface + harmony  
- **neural-lite** (pure C++, offline statistical)  
- Factory; ONNX neural only if `onnxruntime` found (else skipped)  
- `hush-neural-lite` CLI when built  

### Cosmic bridge + integration

- `cosmic-studio/` — processor adapter, observatory, engine bridge  
- `integration/` — ProcessorBridge (slot FX SPSC), no-frills GUI shell  

### Security & offline

- No FetchContent / ExternalProject / `file(DOWNLOAD)` in product CMake  
- Privacy source denylist audit  
- Offline CMake audit  
- **No external DAW checker**  

---

## Layout

```
hush-daw-align/
├── engine/           Bus graph, RT engine, path sandbox, Cosmic adapters
├── engines/
│   ├── daw_core/     Session umbrella + hush-session CLI
│   └── composer/     neural-lite + factory
├── cosmic-studio/    Integration bridge
├── integration/      Processor bridge, optional GUI
├── tools/            privacy + offline audits
├── docs/             Status, triage, adapters, Tier 3
├── CMakeLists.txt
└── README.md
```

---

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DHUSH_BUILD_ENGINE_CORE=ON \
  -DHUSH_BUILD_COSMIC_BRIDGE=ON \
  -DHUSH_BUILD_TIER3=ON \
  -DHUSH_BUILD_COMPOSER=ON \
  -DHUSH_USE_STUBS=ON \
  -DHUSH_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

| Option | Default | Meaning |
|--------|---------|---------|
| `HUSH_BUILD_ENGINE_CORE` | ON | Engine + adapters |
| `HUSH_BUILD_COSMIC_BRIDGE` | ON | cosmic-studio bridge |
| `HUSH_BUILD_TIER3` | ON | daw_core |
| `HUSH_BUILD_COMPOSER` | ON | neural-lite |
| `HUSH_USE_STUBS` | ON | Tier1 stubs in daw_core |
| `HUSH_BUILD_TESTS` | ON | Unit + privacy/offline audits |

---

## CLI

```bash
hush-session new song.hushsess --rate 48000 --length 480000
hush-session info song.hushsess
hush-session render song.hushsess out.wav
hush-session demo demo.wav
hush-neural-lite --help   # when built
```

---

## Tests (verified)

| Suite | Result |
|-------|--------|
| Bus graph / param queue / prealloc / RT engine / stereo / path sandbox | PASS |
| Cosmic adapters (+ ASan, microbench) | PASS |
| test_daw_core / lite_tests (52) | PASS |
| privacy + offline CMake audits | PASS |
| Live ALSA soak | not_run |
| ONNX neural | not_run (optional) |

---

## Docs

| File | Topic |
|------|--------|
| `docs/IMPLEMENTATION_STATUS.md` | Feature matrix |
| `docs/COSMIC_ADAPTERS.md` | Adapter API map |
| `docs/COSMIC_TRIAGE.md` | Defect vs de-harden rule |
| `docs/TIER3_COMPLETE.md` | Session + composer |
| `docs/NO_DAW_CHECKER.md` | Checker exclusion |
| `docs/OFFLINE_BUILD.md` | Offline contract |
| `docs/RELEASE_GATES.md` | Release criteria |

---

*HUSH DAW — complete stack, hardened boundary.*

---

## Midterm pillars (this package)

| Pillar | Location |
|--------|----------|
| **Immersive (no stubs)** | `engines/immersive/` — full object/HOA/binaural/ADM |
| **DawChecker** | `security/` ban-IP + hardening; `third_party/dawchecker_hardened/` |
| **Forensic wrap** | `engines/forensic/` — rack + offline indicator report |
| **Themes** | `themes/hush-cyan.json`, `hush-slate.json`, `hush-midnight.json` |
| **Hardening** | PathSandbox, privacy/offline audits, ASan-clean adapters |

See `docs/MIDTERM_COMPLETE.md`.
