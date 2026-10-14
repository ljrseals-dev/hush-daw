HUSH

<div align="center">

HUSH

OFFLINE-FIRST DIGITAL AUDIO WORKSTATION

Quiet by design. Precise by engineering.

A C++20 audio workstation project focused on local audio processing, session editing, deterministic rendering, and an extensible DSP architecture.

Current Build · Architecture · Roadmap · Build Instructions · Release Gate

</div>

The platform

HUSH is an offline-first audio workstation under active development. Version 2.2.0 brings together a WAV/session core, clip-editing APIs, offline audio rendering, a real-time mixer core, and an early Linux/X11 graphical interface.

The project's engineering priorities are explicit DSP behavior, predictable audio processing, bounded real-time operations, and reproducible validation.

Visual identity

Add the promotional hero artwork to docs/images/hush-hero.png and use this image slot for the GitHub landing page:

Replace this artwork with the final HUSH-branded promotional image or a genuine screenshot from the application.

Current build

HUSH 2.2.0 is a functional development foundation, not yet a production-complete DAW.

ComponentCurrent statusWAV import and exportImplemented and testedSession serializationFKSESSION 1 reader; FKSESSION 2 writerClip editingCore APIs for move, trim, split, fades and undo/redoOffline rendererImplemented for documented track, automation and clip operationsReal-time mixerCore transport, seek, gain/mute and peak meteringLinux/X11 interfacePartial implementationPortAudio integrationOptional; physical hardware testing remains outstandingAudio recordingExperimental capture and WAV writingMIDIEvent model and bounded queue; no operating-system MIDI I/OPlugin hostingIn-process DSP interface; no third-party binary plugin hostWindows/macOS interfacesNot verified or complete

Current-build image gallery

The supplied release archive does not include a captured screenshot of the running GUI. Use genuine application captures in these slots as they become available.

<div align="center">

ArrangementMixerTimeline, clips and waveform editingChannel strips, gain and peak meters

DSP and mastering

Insert a real capture of the current processing interface when available.

</div>

These image slots are documentation references, not claims that the corresponding screenshots are already included in the repository.

Architecture

The current processing flow can be summarized as follows:

flowchart TD A[WAV Files / Session Project] --> B[Session and Clip Model] B --> C[Offline Renderer] B --> D[Real-Time Mixer Core] C --> E[Rendered WAV Output] D --> F[Optional PortAudio Backend] D --> G[Peak Metering] B --> H[Linux / X11 GUI]

Core subsystems

• Audio engine: real-time mixer core and audio-processing infrastructure.

• Session model: project state, tracks, clips and editing operations.

• Offline renderer: file-based rendering using implemented track and clip processing.

• DSP framework: in-process processing interfaces and audio utilities.

• Audio I/O: WAV file operations and optional PortAudio integration.

• GUI: early native Linux/X11 application.

• MIDI infrastructure: event representation and bounded event queue.

The architecture is evolving. The diagram illustrates the documented subsystem relationships; it is not a trace of every runtime connection.

Roadmap

The roadmap below prioritizes completing the DAW workflow before expanding platform support and plugin compatibility.

flowchart TD A["HUSH 2.2.0<br/>Core foundation"] --> B["Phase 1<br/>Editing and recording"] B --> C["Phase 2<br/>MIDI and musical workflow"] C --> D["Phase 3<br/>Plugin hosting and compatibility"] D --> E["Phase 4<br/>Windows and macOS"] E --> F["Phase 5<br/>Release hardening"] F --> G["Production release review"]

Phase 1 — Complete the recording and editing workflow

• Expose clip selection, dragging, splitting and trimming in the GUI.

• Complete fade editing, zoom, scrolling and loop controls.

• Implement track arming and explicit recording-input routing.

• Add punch-in/out, pre-roll, post-roll and loop recording.

• Implement take management and comping.

• Add input monitoring with latency and feedback tests.

• Add audio-device selection and disconnect/reconnect recovery.

• Implement autosave and crash-recovery testing.

Phase 2 — Musical workflow

• Add operating-system MIDI input and output.

• Develop MIDI tracks and an editing interface.

• Improve instrument and event-routing workflows.

• Validate synchronization, event timing and project-state restoration.

Phase 3 — Plugin ecosystem

• Select plugin standards and establish licensing requirements.

• Define plugin discovery and lifecycle management.

• Implement third-party binary plugin loading.

• Test automation, bypass, state recall and failure recovery.

• Evaluate plugin isolation and sandboxing requirements.

Phase 4 — Platform expansion

• Establish native Windows build and runtime validation.

• Establish native macOS build and runtime validation.

• Add continuous integration for supported compilers and platforms.

• Test physical audio devices, buffer sizes and sample rates.

• Produce repeatable installation packages.

Phase 5 — Release hardening

• Complete parser fuzzing and sanitizer coverage.

• Validate recording boundaries and device lifecycle behavior.

• Conduct extended playback and stress tests.

• Complete dependency and license reviews.

• Document known issues and supported configurations.

• Publish reproducible build and validation evidence.

• Obtain the required release sign-offs.

Roadmap items are planned work. They should be marked complete only when implementation and appropriate validation evidence exist.

Build from source

Requirements

• CMake 3.16 or newer

• A C++20 compiler

• Linux for the currently documented native GUI build

• X11 development libraries for the optional GUI

• System PortAudio installation for optional audio-device integration

The supplied package documents Linux x86-64 builds using GCC 14.2 and Clang 17. Other operating systems and architectures remain unverified.

Build without optional audio and GUI backends

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \ -DHUSH_WARNINGS_AS_ERRORS=ON \ -DHUSH_ENABLE_PORTAUDIO=OFF \ -DHUSH_BUILD_X11_GUI=OFF cmake --build build --parallel 2 ctest --test-dir build --output-on-failure

Optional Linux/X11 GUI and PortAudio

Install the necessary system dependencies first, then configure:

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \ -DHUSH_WARNINGS_AS_ERRORS=ON \ -DHUSH_BUILD_X11_GUI=ON \ -DHUSH_ENABLE_PORTAUDIO=ON cmake --build build --parallel 2

PortAudio is an optional system dependency. Enabling it does not establish that a particular audio device or driver works correctly.

CLI examples

hush help hush tone /tmp/test.wav 1 440 0.25 48000 2 hush session-new /tmp/song.fks 48000 120 hush session-add-track /tmp/song.fks Vocals hush session-add-clip /tmp/song.fks 0 /tmp/test.wav 0 hush session-render /tmp/song.fks /tmp/render.wav

Launch the graphical interface with a session:

hush_gui /tmp/song.fks

Confirm command availability against the specific build and installed binaries.

Validation and release gate

Audio software requires more than a successful compilation. HUSH separates automated test results from real-device and release-readiness claims.

The supplied package documents:

• Linux GCC and Clang builds with warnings treated as errors.

• Automated CTest execution.

• A GUI launch smoke test under Xvfb.

• A repeatable release-gate script.

• Additional detector calibration and manual hardware requirements.

Run the documented release gate:

scripts/release_gate.sh

Run detector calibration separately:

ctest --test-dir build --output-on-failure \ -R hush_detector_calibration

Outstanding release evidence

• Physical audio playback and recording verified.

• Device disconnection and reconnection tested.

• Recording boundaries and input monitoring validated.

• Project recovery and state restoration tested.

• MIDI and plugin architecture decisions completed.

• Parser fuzzing and sanitizer results documented.

• Dependency and security reviews completed.

• Release artifacts built and independently checked.

Current release decision: HOLD for production.

A successful build or automated test run does not establish production readiness, plugin compatibility, or reliable operation on untested hardware.

Security and privacy

HUSH is designed around local processing and has no telemetry, updater, remote-control service or network client in the active build described by the release package.

Optional utilities in optional/ are separate from the default DAW build and should be reviewed independently before use.

The project does not claim independent security certification or complete parser-fuzzing sign-off.

Repository structure

HUSH-2.2.0/ ├── src/ Audio engine, session, DSP, MIDI and GUI sources ├── tests/ Unit and regression tests ├── scripts/ Build, smoke-test and release-gate scripts ├── evidence/ Build and validation records ├── docs/ │ └── images/ README artwork and genuine UI screenshots ├── optional/ Separate utilities ├── CMakeLists.txt ├── README.md ├── CHANGELOG.md ├── PORTING_STATUS.md └── RELEASE_GATE.md

The actual repository contents are authoritative; consult the source tree if file names or directories differ.

Contributing

Contributions should include a focused change, relevant tests and reproducible validation results.

For DSP changes, include the sample rate, buffer size, channel configuration and test signal. For audio-device issues, include the operating system, driver, device and reproduction steps.

License

See [LICENSE](LICENSE) for the applicable license and attribution terms.

<div align="center">

HUSH

Quiet by design. Precise by engineering.

</div>

Add these images to your repository

Create docs/images/ and add the following files:

FilenameContenthush-hero.pngHUSH-branded promotional heroarrangement.pngActual running arrangement/timeline screenshotmixer.pngActual mixer and meter screenshotdsp-mastering.pngActual DSP interface screenshot

GitHub will display the pictures automatically once the files are committed at those paths. The Mermaid architecture and roadmap diagrams render directly from the README without additional image files.

Recommended next step: capture the current HUSH GUI running on Linux and replace the three screenshot placeholders. That will make the repository look like a real software product while keeping the documented build status honest.

From damaged images to cleaner audio, create more possibilities.
