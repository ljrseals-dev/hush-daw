# Release gates (hush-daw)

1. Offline configure + build (no network package bootstrap)
2. All engine CTests PASS
3. Privacy denylist PASS
4. Offline CMake audit PASS
5. No external DAW-checker dependency
6. Realtime: process() paths documented no-alloc; queue full returns false

Hardware gates (ALSA xrun soak): mark **not_run** until executed on device.
