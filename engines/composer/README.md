# hush-composer

Shared interface + two backends for the Hush DAW neural MIDI composer.

## Layout

```
engines/composer/
├── include/hush/
│   ├── composer/
│   │   ├── composer.hpp          # shared IComposer + SampleConfig
│   │   └── neural.hpp            # NeuralComposer declaration
│   └── notation/
│       └── score.hpp             # minimal Score type
├── src/
│   └── composer_factory.cpp      # makeComposer + makeLiteForTest
├── TOKENIZER.md
├── neural-lite/                  # pure C++17 statistical composer
│   ├── include/hush/composer/lite.hpp
│   ├── src/
│   │   ├── harmony.hpp / .cpp
│   │   ├── style_loader.hpp / .cpp
│   │   ├── smf.hpp / .cpp
│   │   ├── composer.cpp
│   │   └── main.cpp
│   ├── tests/lite_tests.cpp
│   ├── CMakeLists.txt
│   └── README.md
└── neural/                       # ONNX Runtime neural composer
    ├── src/
    │   ├── tokenizer.hpp / .cpp
    │   ├── onnx_session.hpp / .cpp
    │   ├── composer.cpp
    │   └── main.cpp
    ├── tests/neural_tests.cpp
    ├── tools/
    │   ├── requirements.txt
    │   ├── train.py
    │   ├── export_onnx.py
    │   └── README.md
    ├── model/README.md
    ├── CMakeLists.txt
    └── README.md
```

## Build

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Requires ONNX Runtime for the neural target. Lite builds without it.

## Generate the model (optional)

```bash
cd neural/tools
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
# generate corpus (see tools/README.md one-liner)
python train.py --seed 42 --out ../model/model.onnx
```

## License

MIT. Copyright (c) 2026 ljrseals-dev.
