# tests/fixtures/

SPDX-License-Identifier: MIT  
Copyright (c) 2026 ljrseals-dev

## prompt.mid

A 2-bar C-major motif used by the model-presence tests. To generate
it, build the lite CLI first, then:

```bash
echo 'not implemented'   # placeholder — see below
```

Better: use the lite CLI's generate verb with a fixed seed and
save the output:

```bash
hush-neural-lite generate --style pop --bars 2 \
    --seed 42 --out tests/fixtures/prompt.mid
```

That produces a valid 2-bar SMF in C major. Any 2-bar MIDI file
works as prompt.mid; it does not need to match the model's
training distribution, only be a valid SMF.

expected_seed{42,7,1234}.txt

Each file contains 16 space-separated token ids that train.py
printed for that seed. They prove the checked-in model.onnx is
the one produced by the documented training run.

To regenerate:

```bash
cd tools
python train.py --seed 42 --out ../model/model.onnx 2>&1 \
    | grep '^seed=' | tee ../tests/fixtures/expected_seeds.txt
```

Then split expected_seeds.txt into three files by seed value:
the line seed=42: 0 1 2 ... becomes expected_seed42.txt
containing only 0 1 2 ....

Regenerating everything

If you delete the model and fixtures, the full rebuild is:

```bash
cd tools
python gen_corpus.py
python train.py --seed 42 --out ../model/model.onnx
# then split the three seed lines into the three fixture files
```

License

All content in this directory is MIT-licensed alongside the rest
of the engine. The MIDI fixture is an original composition.

```

---
