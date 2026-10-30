# DawChecker hardened fork (FortKnox 1.1.1): provenance

Origin: copy of third_party/dawchecker/src/core (itself byte-identical to the user's DawChecker-Secure-Ultimate-with-ServerProtection-1.zip, SHA-256 9dc185fe4238f93119c48ff9e30985b1a468a9df737e4ce2ab31f9067f7f3735). License: MIT (see LICENSE, notice preserved). The original tree third_party/dawchecker/ is untouched; `diff -rq` against the archive copy is part of the test run.

Every file, upstream hash vs fork hash:

| file | upstream SHA-256 | fork SHA-256 | status |
|---|---|---|---|
| AudioDeviceManager.cpp | 9924f9b198187a90... | 9924f9b198187a90... | unchanged |
| AudioDeviceManager.h | ba7f9153cd40105d... | ba7f9153cd40105d... | unchanged |
| CodecScanner.cpp | bbf8bc660afa2d89... | bbf8bc660afa2d89... | unchanged |
| CodecScanner.h | 498ea10caa8629b0... | 498ea10caa8629b0... | unchanged |
| DetectorTypes.cpp | (none)... | 439d76bc0a05e31d... | NEW |
| DetectorTypes.h | eba1bbf07923f376... | 7e3bf6e10c45bd4d... | MODIFIED |
| ENFAnalyzer.cpp | 818d3a2ef9d21d79... | 5cd0ecafa2dd10fa... | MODIFIED |
| ENFAnalyzer.h | 12cf53c62baf1b43... | 7c3abfa5fa5d2423... | MODIFIED |
| HannGoertzelBank.h | (none)... | c632a8231d021577... | NEW |
| MSV_Engine.cpp | 50069158bcc6a235... | 3b4e3f15147ef642... | MODIFIED |
| MSV_Engine.h | 0de17c32d8931d9c... | 89a19af4a723efb0... | MODIFIED |
| Neutralizers.cpp | 21278fc336ba3f16... | 11fe3cc56991a089... | MODIFIED |
| Neutralizers.h | 3234eaaaa476d891... | 854ad5a95115f164... | MODIFIED |
| RunningWindow.h | (none)... | a507d62e0406cd51... | NEW |
| SecurityHardening.cpp | 3843c07f31357f1f... | f3e4b97f839e9399... | MODIFIED |
| SecurityHardening.h | e09070e49b491f0c... | e9d578b619a889cc... | MODIFIED |
| UltrasonicScanner.cpp | 7a1f5aeabfb583d5... | 637d29eeab8be9ea... | MODIFIED |
| UltrasonicScanner.h | 0499140445607153... | 18bc1b74895cf8ad... | MODIFIED |
| WatermarkDetector.cpp | 1dbd55234879849b... | 6b8a7a546970a699... | MODIFIED |
| WatermarkDetector.h | 1d5088939ec337d6... | a8fe9efcab151ba2... | MODIFIED |

Full unified diff: CHANGES.diff.

## Changes and rationale
- DetectorTypes.h/.cpp: DetectorResult::label is `const char*` (static strings) so process() never allocates; added flushDenormal, sanitizeSampleRate, enableFlushDenormalsForThisThread (FTZ/DAW on x86; calling thread only).
- RunningWindow.h (NEW): fixed-size ring with O(1) mean/variance; replaces std::deque + std::accumulate.
- HannGoertzelBank.h (NEW): amplitude-calibrated Hann Goertzel bank, fixed storage, phasor-recurrence window (no per-sample cos).
- UltrasonicScanner, ENFAnalyzer, WatermarkDetector: rewritten. Block-based, thresholds in dBFS or dB margin over neighbouring bins / dB re reference (never raw energy), smoothing time constants converted per sample rate, hysteresis + minimum sustained blocks, sample-rate sanitising. ENF and the residual detector are capped at Active. ENFAnalyzer::process now takes only the live sample (the old reference argument only fed an unrelated baseline). WatermarkDetector keeps (live, reference).
- Neutralizers: all default OFF; WatermarkScrubber REMOVED; explicit offline render_neutralized() added; filters unchanged.
- MSV_Engine: neutralizers removed from process() (out == live always); bypassAll and the scrub/notch/rolloff flags removed; argument order (live, reference, out); seqlock-published POD EngineSnapshot replaces per-field atomics.
- SecurityHardening.cpp/.h: stack-protector and CET fields now come from compiler macros (false if not compiled in), DEP from /proc/self/maps, FORTIFY level and PIE reported as notes; the false "hardening_flags" comment is gone.
- CodecScanner.*, AudioDeviceManager.*: unchanged.
