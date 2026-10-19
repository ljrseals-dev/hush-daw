# HUSH No-Frills GUI Starter

A small native Linux/X11 interface for the HUSH DAW work, based on the Phase 1 UI requirements recorded in `Releases/11-16/A/B/Hush_phase1.txt`.

## Included in this starter

- Dark slate palette (`#11141a`) with teal accent (`#5eead4`).
- HUSH title bar and simple Play / Stop / Record / Loop controls.
- Local browser panel: Projects, Instruments, Effects, Samples, Recent Files.
- Four-track arrangement with ruler, clips/waveform-like overview, and Mute / Solo / Record-arm toggles.
- Right-side processing rack for Spectral Denoise, Hiss Reduction, Click Removal, and Hum Eliminator.
- Master output meter, mixer strip, and Mixer / Plugins / MIDI / Forensics / Daw Checker tabs.
- Keyboard shortcuts: Space toggles Play, `r` toggles Record, `q` or Escape quits.

## Build (Linux/X11)

```sh
cmake -S integration/no-frills-gui -B build/hush-no-frills-gui -DCMAKE_BUILD_TYPE=Release
cmake --build build/hush-no-frills-gui --parallel 2
./build/hush-no-frills-gui/hush_no_frills_gui
```

Requires a C++20 compiler, CMake 3.16+, X11 development headers/libraries, and an active X11 display.

## Scope / honesty

This is a **standalone UI shell**, not a finished DAW integration. The transport, M/S/R controls, and tab selection are local UI state only. The audio engine, real clip/session model, device I/O, rack DSP calls, and DawChecker results still need explicit adapters to the canonical engine APIs. No audio is captured or processed by this shell.

## Repository findings from the prompt/release grep

- Phase 1 reference prompt: [`Hush_phase1.txt`](https://github.com/ljrseals-dev/hush-daw/blob/main/Releases/11-16/A/B/Hush_phase1.txt)
- Reference GUI screenshot: [`HUSH-2.2.0-GUI-screenshot (1).png`](https://github.com/ljrseals-dev/hush-daw/blob/main/Releases/11-16/A/B/HUSH-2.2.0-GUI-screenshot%20(1).png)
- The ZIP entries in Releases/11-14, 11-15, and 11-16 that were inspected have GitHub blob size **2 bytes** and share the same blob SHA. They are not usable release archives and should not be represented as valid builds.
- Releases/11-13 contains a 701,506-byte `HUSH-2.2.0-release-gate.zip`; Releases/11-14 contains a 55,494-byte `hush-midterm-deliverable.zip`. These are candidates for source inspection, not proof of a production-ready application.
- The main README describes the Linux/X11 interface as partial and the production release decision as HOLD.

The no-frills GUI is deliberately separate from the existing bridge starter so it can be built and reviewed without pretending the processor connection is complete.
