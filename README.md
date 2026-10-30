# HUSH DAW — Canonical Hardened Build

**Version:** 2.2.1  
**Status:** Offline-first · Security-hardened · Tier 3 complete  
**License:** MIT (see `LICENSE` where present)

This tree is the **canonical hush-daw** audio engine: same processor core as the public Cosmic Studio fork, with the **full security posture intact**. Cosmic Studio is a de-hardened derivative for creators; **defects may flow Cosmic → hush-daw after triage; security removals never flow back.**

---

## What this package is

A C++20 offline-buildable foundation for a privacy-first DAW:

| Layer | What it does |
|-------|----------------|
| **Engine core** | Bus graph, SPSC parameter queue, preallocated bus processor, realtime bus engine, stereo adapters |
| **Security** | Path sandbox, privacy source denylist, offline CMake (no FetchContent/network bootstrap) |
| **Tier 3 umbrella** | Session / Track / RenderGraph / Transport, HUSHSESS save/load, offline render CLI |
| **Composer (Tier 3)** | Neural-lite chord suggestion & style continuation (statistical, no network); optional ONNX path if present |
| **Cosmic bridge** | SPSC command queue + processor adapter (integration surface; not a public de-hardened product) |

No external **DAW checker** daemon. Internal unit tests and privacy audits only.

---

## Features (current)

### 1. Bus graph (`engine/`)

- Directed bus routing with **DFS cycle detection**
- Self-route rejection, unknown-bus errors, duplicate/empty bus IDs
- Route **gain validated** finite and in **[0, 16]**
- Topological **render order** for offline/online processing

### 2. Parameter queue (SPSC)

- Bounded single-producer / single-consumer queue (UI → audio)
- Full queue returns `false` — **no block, no alloc** on the audio path
- Deterministic tests for empty, fill, drain, capacity

### 3. Preallocated bus processor

- `configure()` on the control thread (may allocate)
- `process()` is **noexcept**: no allocation, locking, logging, or I/O
- Per-bus pre/post fader views; fader gain bounds; frame-capacity checks

### 4. Realtime bus engine

- Drains parameter queue then runs the bus processor
- **Interleaved stereo** device path via `StereoScratch`
- Mute / gain command kinds; oversize blocks silenced safely
- Dropped-param counter for UI observation

### 5. Path sandbox (security retained)

- Always rejects `..` path segments
- Optional **session root jail**: relative paths resolve only under the session directory
- Used for take/WAV path policy (Cosmic deliberately strips this; hush-daw keeps it)

### 6. Tier 3 — `engines/daw_core`

- **Session** — sample rate, length, owned subsystems
- **Transport** — playhead / transport state
- **RenderGraph** — rebuild from session content
- **session_io** — save/load with **HUSHSESS** magic
- **CLI `hush-session`**
  - `new` — create session file  
  - `info` — print rate / length / duration  
  - `render` — offline render to WAV  
  - `demo` — short demo session → WAV  
- Engine **stubs** for immersive / score / video / plugins when Tier 1 targets are absent (`HUSH_USE_STUBS=ON`)

### 7. Tier 3 — Composer (`engines/composer`)

- Shared **composer interface** + **harmony** helpers
- **neural-lite**: pure C++ statistical model  
  - Krumhansl–Schmuckler-style key awareness (lite path)  
  - Style transition tables, SMF helpers, chord suggestion / continuation  
- **Factory** selects lite always; **ONNX neural** only if `onnxruntime` is found at configure time (skipped cleanly offline)
- CLI: `hush-neural-lite` (when built)
- Tests: `lite_tests` (52 cases in the reference run)

### 8. Cosmic Studio bridge (`cosmic-studio/`)

### 8b. Cosmic adapters (ported)

- `RealtimeBusAudioAdapter` — device interleaved stereo → engine
- `MixerEngineBridge` — track ID → fader/mute SPSC
- `RealtimeBusEngineHost` + `RealtimeGraphPublisher` — graph hot-replace
- `RealtimeBusEngineHostAudioAdapter` — interleaved host path + ALSA callback


- Engine bridge, observatory meters, processor adapter
- SPSC slot commands for host integration
- **Not** a substitute for hush-daw security; Cosmic public trees omit hardening on purpose

### 9. Privacy & offline contract

- **No** FetchContent / ExternalProject / `file(DOWNLOAD)` in product CMake
- `FETCHCONTENT_FULLY_DISCONNECTED` forced ON
- **Privacy denylist** CMake audit over sources (no telemetry / analytics / outbound client stubs)
- **Offline CMake audit** fails the test suite if download APIs appear in CMakeLists

### 10. Explicitly out of scope (by design)

- External DAW checker / validation harness at runtime or in the build graph  
- Shipping Cosmic’s relaxed network/path rules into this tree  
- Claiming live ALSA xrun soak or ONNX neural without evidence (**not_run** until executed)

---

## Layout

```
hush-daw-align/
├── engine/                 # Bus graph, RT processor, path sandbox, SPSC
├── engines/
│   ├── daw_core/           # Session, transport, render graph, hush-session CLI
│   └── composer/           # Interface, harmony, neural-lite, optional neural
├── cosmic-studio/          # Bridge + adapter (integration)
├── integration/            # Processor bridge, optional no-frills GUI
├── tools/                  # privacy_source_audit, offline_cmake_audit
├── docs/                   # Status, triage, release gates, Tier notes
├── CMakeLists.txt
└── README.md               # this file
```

---

## Build (offline)

Requirements: C++20 compiler, CMake ≥ 3.16. No network package bootstrap.

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

**Options**

| Option | Default | Meaning |
|--------|---------|---------|
| `HUSH_BUILD_ENGINE_CORE` | ON | Bus graph + realtime engine |
| `HUSH_BUILD_COSMIC_BRIDGE` | ON | Cosmic integration bridge |
| `HUSH_BUILD_TIER3` | ON | daw_core session umbrella |
| `HUSH_BUILD_COMPOSER` | ON | neural-lite + factory |
| `HUSH_USE_STUBS` | ON | daw_core stubs when Tier 1 engines missing |
| `HUSH_BUILD_TESTS` | ON | Unit tests + privacy/offline audits |

If configure is slow in constrained environments, the same libraries compile with `g++ -std=c++20` against `engine/include` and `engines/*/include` (see CI/local notes in `docs/`).

---

## CLI quick start

```bash
# Session
hush-session new  song.hushsess --rate 48000 --length 480000
hush-session info song.hushsess
hush-session render song.hushsess out.wav
hush-session demo demo.wav

# Composer (neural-lite), when built
hush-neural-lite --help
```

---

## Tests (reference evidence)

| Suite | Result (last verified) |
|-------|-------------------------|
| `hush_bus_graph_tests` | PASS |
| `hush_parameter_queue_tests` | PASS |
| `hush_preallocated_bus_processor_tests` | PASS |
| `hush_realtime_bus_engine_tests` | PASS |
| `hush_stereo_adapter_tests` | PASS |
| `hush_path_sandbox_tests` | PASS |
| `test_daw_core` | PASS |
| `lite_tests` | 52 passed, 0 failed |
| `hush_privacy_source_audit` | PASS |
| `hush_offline_cmake_audit` | PASS |
| Live ALSA soak | **not_run** (hardware) |
| ONNX neural tests | **not_run** (package optional) |

---

## Relationship to Cosmic Studio

| Project | Role |
|---------|------|
| **hush-daw** (this tree) | Canonical, security-hardened processor |
| **HUSH Cosmic Studio** | Public fork without security enhancements so creators can build their own DAW |

**Triage rule:** backport genuine engine defects only; reject de-hardening (relaxed path checks, re-enabled downloads, dropped denylist, external checker). See `docs/COSMIC_TRIAGE.md`.

---

## Documentation index

| Doc | Content |
|-----|---------|
| `docs/IMPLEMENTATION_STATUS.md` | Feature matrix + evidence |
| `docs/TIER3_COMPLETE.md` | Tier 3 handoff |
| `docs/COSMIC_TRIAGE.md` | Defect vs security-removal classification |
| `docs/NO_DAW_CHECKER.md` | Checker exclusion policy |
| `docs/OFFLINE_BUILD.md` | Offline contract |
| `docs/RELEASE_GATES.md` | What must pass before a release claim |

---

## Security summary

- Path traversal blocked (`..`); optional session root jail  
- No telemetry clients in source (denylist audited)  
- Offline CMake: no download APIs in the product graph  
- Realtime path: no alloc / I/O / logging inside `process`  
- External DAW checker not required and not linked  

---

*HUSH DAW — understand the signal, keep the trust boundary.*
