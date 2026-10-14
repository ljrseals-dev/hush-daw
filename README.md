# HUSH 2.2.0

**OFFLINE-FIRST DIGITAL AUDIO WORKSTATION**

*Quiet by design. Precise by engineering.*

A C++20 audio workstation project focused on local audio processing, session editing, deterministic rendering, and an extensible DSP architecture[span_0](start_span)[span_0](end_span)[span_1](start_span)[span_1](end_span).

[Current Build](#current-build) · [Architecture](#architecture) · [Roadmap](#roadmap) · [Build Instructions](#build-from-source) · [Release Gate](#validation-and-release-gate)

---

## The Platform

HUSH is an offline-first audio workstation under active development[span_2](start_span)[span_2](end_span)[span_3](start_span)[span_3](end_span). Version 2.2.0 brings together a WAV/session core, clip-editing APIs, offline audio rendering, a real-time mixer core, and an early Linux/X11 graphical interface[span_4](start_span)[span_4](end_span)[span_5](start_span)[span_5](end_span).

The project's engineering priorities are explicit DSP behavior, predictable audio processing, bounded real-time operations, and reproducible validation[span_6](start_span)[span_6](end_span)[span_7](start_span)[span_7](end_span).

---

## Visual Identity

![HUSH Promotional Artwork](docs/images/hush-hero.png)

---

## Current Build

HUSH 2.2.0 is a functional development foundation, not yet a production-complete DAW[span_8](start_span)[span_8](end_span)[span_9](start_span)[span_9](end_span).

| Component | Current Status |
| :--- | :--- |
| **WAV import and export** | Implemented and tested[span_10](start_span)[span_10](end_span)[span_11](start_span)[span_11](end_span) |
| **Session serialization** | FKSESSION 1 reader; FKSESSION 2 writer[span_12](start_span)[span_12](end_span)[span_13](start_span)[span_13](end_span) |
| **Clip editing** | Core APIs for move, trim, split, fades and undo/redo[span_14](start_span)[span_14](end_span)[span_15](start_span)[span_15](end_span) |
| **Offline renderer** | Implemented for documented track, automation and clip operations[span_16](start_span)[span_16](end_span)[span_17](start_span)[span_17](end_span) |
| **Real-time mixer** | Core transport, seek, gain/mute and peak metering[span_18](start_span)[span_18](end_span)[span_19](start_span)[span_19](end_span) |
| **Linux/X11 interface** | Partial implementation[span_20](start_span)[span_20](end_span)[span_21](start_span)[span_21](end_span) |
| **PortAudio integration** | Optional; physical hardware testing remains outstanding[span_22](start_span)[span_22](end_span)[span_23](start_span)[span_23](end_span) |
| **Audio recording** | Experimental capture and WAV writing[span_24](start_span)[span_24](end_span)[span_25](start_span)[span_25](end_span) |
| **MIDI** | Event model and bounded queue; no operating-system MIDI I/O[span_26](start_span)[span_26](end_span)[span_27](start_span)[span_27](end_span) |
| **Plugin hosting** | In-process DSP interface; no third-party binary plugin host[span_28](start_span)[span_28](end_span)[span_29](start_span)[span_29](end_span) |
| **Windows/macOS interfaces** | Not verified or complete[span_30](start_span)[span_30](end_span)[span_31](start_span)[span_31](end_span) |

---

## Current-Build Image Gallery

### Arrangement & Editing
![Arrangement and Timeline](docs/images/arrangement.png)  
*Timeline, clips and waveform editing[span_32](start_span)[span_32](end_span)[span_33](start_span)[span_33](end_span)*

### Mixer & Routing
![Mixer and Meters](docs/images/mixer.png)  
*Channel strips, gain and peak meters[span_34](start_span)[span_34](end_span)[span_35](start_span)[span_35](end_span)*

### DSP & Mastering
![DSP Interface](docs/images/dsp-mastering.png)  
*Real-time processing interface[span_36](start_span)[span_36](end_span)[span_37](start_span)[span_37](end_span)*

---

## Architecture

The current processing flow can be summarized as follows[span_38](start_span)[span_38](end_span)[span_39](start_span)[span_39](end_span):

```mermaid
flowchart TD
    A[WAV Files / Session Project] --> B[Session and Clip Model]
    B --> C[Offline Renderer]
    B --> D[Real-Time Mixer Core]
    C --> E[Rendered WAV Output]
    D --> F[Optional PortAudio Backend]
    D --> G[Peak Metering]
    B --> H[Linux / X11 GUI]
