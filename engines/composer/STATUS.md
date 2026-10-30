# STATUS.md — what is and is not in this zip

## Present and complete in this archive

- Shared interface (composer.hpp, score.hpp, neural.hpp, lite.hpp)
- Top-level CMakeLists.txt + factory
- neural-lite and neural CMakeLists.txt
- TOKENIZER.md, top-level README

## Present in conversation history but must be re-pasted from history

The following large source files were written fully in the conversation
and must be copied from the chat history into the corresponding paths:

### neural-lite
- src/harmony.hpp + harmony.cpp
- src/style_loader.hpp + style_loader.cpp
- src/smf.hpp + smf.cpp (with orphan-note fix)
- src/composer.cpp (LiteComposer)
- src/main.cpp
- tests/lite_tests.cpp
- README.md

### neural
- src/tokenizer.hpp + tokenizer.cpp
- src/onnx_session.hpp + onnx_session.cpp
- src/composer.cpp (NeuralComposer + sampling)
- src/main.cpp
- tests/neural_tests.cpp
- tools/requirements.txt
- tools/train.py
- tools/export_onnx.py
- tools/README.md
- model/README.md
- README.md

## Must be generated locally

- neural/tools/corpus/seed.json   (one-liner in tools/README.md)
- neural/model/model.onnx         (run train.py)
- neural/tests/fixtures/*         (printed by train.py + a 2-bar MIDI)

## How to complete

1. Copy every full source block from the conversation into the matching path.
2. Run the corpus generator and train.py.
3. cmake -S . -B build && cmake --build build -j && ctest --test-dir build
