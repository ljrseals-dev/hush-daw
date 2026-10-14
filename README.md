HUSH 2.1.0: offline core and hardened DawChecker fork pass the Altar-1 rules tested so far. Open surfaces documented, no clean-slate claim. Record button needs a capture backend, not a one-line fix.

On "those faithful to TheRealla now have the HUSH car"

Here is the HUSH 2.2.0 README tailored to the actual platform described in your uploaded release package. It includes a premium GitHub presentation, current-build status, architecture diagrams, installation instructions, a visual roadmap, and explicit release-gate limitations.

Important: I can provide the complete README content here, but I cannot attach a newly packaged ZIP in this response. The image references below require the corresponding image files to be added to your repository. The architecture and roadmap diagrams use Mermaid, which GitHub renders directly.

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
