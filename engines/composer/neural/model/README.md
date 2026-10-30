# model/

`model.onnx` — the tiny transformer exported by tools/train.py.

Size budget: ≤ 2 MB.

Regenerate with:

```bash
cd ../tools
python train.py --seed 42 --out ../model/model.onnx
```

The three fixture files under tests/fixtures/expected_seed*.txt must
be updated from the seed= lines printed to stderr after training.
