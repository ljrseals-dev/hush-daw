# HUSH Core Tier 1 — Build Integration Index

This page connects the three published Tier 1 engine packages in [`ljrseals-dev/hush-core`](https://github.com/ljrseals-dev/hush-core) to the HUSH DAW build workspace.

> **Scope/status:** Tier 1 is the published foundational engine layer. The separate Hush Core model/advanced roadmap work is **Tier 3 and is not marked complete here**. These links are integration references; they do not, by themselves, mean the ZIP contents have been vendored, compiled, or linked into HUSH 2.2.0.

## Tier 1 packages

| Engine | Download package | Intended build target | DAW connection point |
|---|---|---|---|
| **hush-render** | [Download ZIP](https://github.com/ljrseals-dev/hush-core/raw/refs/heads/main/hush-render-complete.zip) · [Repository file page](https://github.com/ljrseals-dev/hush-core/blob/main/hush-render-complete.zip) | CLI `hush-render`; static library `hush_render` | Offline render/export workflow; connect through a deliberate adapter to HUSH's session/render model. |
| **hush-score** | [Download ZIP](https://github.com/ljrseals-dev/hush-core/raw/refs/heads/main/hush-score-cli.zip) · [Repository file page](https://github.com/ljrseals-dev/hush-core/blob/main/hush-score-cli.zip) | CLI `hush-score`; static library `hush_score_cli` | MIDI import/export and notation workflow; integrate after confirming HUSH's MIDI event/timebase mapping. |
| **hush-probe** | [Download ZIP](https://github.com/ljrseals-dev/hush-core/raw/refs/heads/main/hush-probe-complete.zip) · [Repository file page](https://github.com/ljrseals-dev/hush-core/blob/main/hush-probe-complete.zip) | CLI `hush-probe`; static library `hush_probe` | Video metadata and thumbnail support for timeline/media-browser UI; FFmpeg is optional per the upstream description. |

Upstream project: [hush-core README](https://github.com/ljrseals-dev/hush-core#completed-core-builds-tier-1).

## Integration order

1. Download and inspect each package before extraction; retain its upstream license and notices.
2. Vendor the source under a clearly named location such as `third_party/hush-core/engines/{render,score-cli,probe}` (or use upstream's actual layout if the archives already contain it).
3. Reconcile the upstream CMake targets with HUSH's C++20 build. Keep the engines optional behind explicit CMake options until each compiles cleanly.
4. Add narrow adapters rather than coupling upstream engine internals directly to the session/UI layer:
   - renderer adapter: session/render request → `hush_render`
   - score adapter: MIDI events/file → `hush_score_cli`
   - media adapter: video path → `hush_probe` metadata/thumbnails
5. Add tests for each adapter and a full build/CTest run with all options both enabled and disabled.
6. Only then change the status below from **linked reference** to **integrated and verified**.

## Current integration status

- [x] Tier 1 package locations and upstream targets documented.
- [x] HUSH README links to this integration index.
- [ ] Package source extracted into the HUSH repository.
- [ ] CMake targets wired into the HUSH build.
- [ ] Adapters implemented and exercised from the DAW.
- [ ] Clean build and automated tests verified with Tier 1 enabled.
- [ ] Tier 3 model/advanced roadmap work completed — **not claimed**.

## Build compatibility notes

HUSH's documented build uses C++20; upstream hush-core describes C++17 and CMake 3.16+. C++17 libraries are generally consumable from a C++20 application, but compatibility still needs to be demonstrated from the actual source packages. Do not mark the engines as linked merely because their download URLs are present.
