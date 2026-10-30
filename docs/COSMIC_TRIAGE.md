# Cosmic → hush-daw triage (defects only)

Rule: **Backport defects, never backport de-hardening.**

| Cosmic item | Class | Action |
|-------------|-------|--------|
| BusGraph: gain finite ∈ [0,16] | Genuine defect/hardening of validation | **BACKPORT** |
| BusGraph: DFS cycle detection + specific error codes | Genuine defect fix | **BACKPORT** |
| BusGraph: unknown bus / self-cycle messages | Genuine defect fix | **BACKPORT** |
| PreallocatedBusProcessor (configure allocs; process noexcept no I/O) | Genuine engine defect / RT safety | **BACKPORT** |
| SpscQueue / ParameterQueue full + invalid paths | Genuine defect (determinism) | **BACKPORT** tests |
| Privacy denylist / offline CMake guards | Security (hush-daw already stronger) | **KEEP hush-daw** |
| Any relaxed network/fetch in Cosmic CI | Security removal | **REJECT** |
| DAW checker / creator scaffolding | Checker / cosmetic | **REJECT / Cosmic only** |
| Cosmic “TelemetryChannel” name (local atomics only) | Naming only; no network | Keep as local meters; not outbound telemetry |

De-hardening scan on hush-daw tree: **no** FetchContent/curl_easy/Sentry/analytics hits in engine/cosmic-studio sources.
