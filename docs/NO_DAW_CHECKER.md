# No DAW checker

This repository deliberately **does not** ship:

- An external “DAW checker” binary or service
- A structural conformance daemon
- A test-orchestration agent that validates product correctness outside CTest

Correctness evidence is:

1. Offline CMake configure/build
2. CTest unit/contract tests (bus graph, parameter queue, cosmic bridge)
3. Privacy denylist scan (`hush_privacy_source_audit`) — a **tripwire**, not a full security proof

If historical docs or zips mention DawChecker UI panels, they are out of scope for this Cosmic alignment path and are not linked from the default offline build.
