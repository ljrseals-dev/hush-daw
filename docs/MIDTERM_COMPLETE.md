# HUSH Midterm — DAW Checker · Immersion · Forensic · Themes · Hardening

## Scope delivered

| Pillar | What | Stubs? |
|--------|------|--------|
| **Immersive** | Full `engines/immersive` (speakers, FOA/HOA, binaural, ADM, object panner, rotation) | **No** — real engine vendored |
| **DawChecker** | `security/` ban-IP list + hardening report; vendored `third_party/dawchecker_hardened` detectors | Hardened sources included |
| **Forensic wrap** | `engines/forensic` rack slots + offline indicator report with **explicit limitations** | Real midterm rack |
| **Hardening** | PathSandbox, privacy denylist, offline CMake, oversize-safe adapters | Forensic-grade posture |
| **Themes** | `themes/hush-cyan.json`, `hush-slate.json`, `hush-midnight.json` | In-tree only |

## Guarantees

- Forensic findings are **indicators**, not proof of authenticity or tampering.
- DawChecker ban list is **local**; no telemetry.
- Immersive builds when `engines/immersive` is present; `HUSH_USE_STUBS` forced OFF for that path.
- Cosmic adapters remain optimized and ASan-clean.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DHUSH_BUILD_IMMERSIVE=ON \
  -DHUSH_BUILD_FORENSIC=ON \
  -DHUSH_BUILD_DAWCHECKER=ON \
  -DHUSH_BUILD_TIER3=ON \
  -DHUSH_BUILD_COMPOSER=ON
```

## Cosmic theme

`themes/hush-cosmic.json` — violet / nebula Cosmic Studio aesthetic (`glow_accent`, secondary cyan).

## Forensic-grade reporting

`hush::forensic::ForensicGradeReport` (`ForensicReport.hpp`):

- Schema `HUSH-FORENSIC-REPORT-1`
- Evidence items with severity, methodology, confidence
- Chain-of-custody note list (caller timestamps)
- Mandatory limitations block
- Hardening notes (PathSandbox, offline, local ban list)
- Export: `to_text()` and `to_json()`
- Builder: `build_grade_report(...)` offline / control thread only
