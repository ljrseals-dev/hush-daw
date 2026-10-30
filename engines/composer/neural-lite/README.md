# neural-lite

Pure C++17 statistical MIDI composer. No machine-learning dependencies.

Uses Krumhansl-Schmuckler key detection, Roman-numeral analysis, and
style-conditioned Markov transition tables (pop / jazz / classical / ambient).

## Build

Built as part of the top-level `engines/composer` CMake project:

```bash
cmake -S . -B build
cmake --build build -j --target hush-neural-lite lite_tests
./build/neural-lite/lite_tests
```

## CLI

```bash
./build/neural-lite/hush-neural-lite generate --style pop --bars 4 --seed 42 --out out.mid
./build/neural-lite/hush-neural-lite continue --in prompt.mid --style jazz --bars 2 --out cont.mid
```

## License

MIT. Copyright (c) 2026 ljrseals-dev.
