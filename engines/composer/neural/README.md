# neural

ONNX Runtime-backed neural MIDI composer. Tiny 2-layer transformer
(d_model=64, 4 heads) trained on a 50-example seed corpus of block-chord
progressions.

Requires ONNX Runtime at configure time. When absent the target is
skipped cleanly.

## Build

```bash
cmake -S . -B build -DHUSH_NEURAL_WITH_CUDA=OFF
cmake --build build -j --target hush-neural neural_tests
./build/neural/neural_tests
```

## Model

The checked-in `model/model.onnx` is produced by:

```bash
cd tools
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python gen_corpus.py
python train.py --seed 42 --out ../model/model.onnx
```

See tools/README.md for full details and determinism notes.

## License

MIT. Copyright (c) 2026 ljrseals-dev.
