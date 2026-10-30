# tools/README.md

SPDX-License-Identifier: MIT  
Copyright (c) 2026 ljrseals-dev

Build-time training pipeline for the hush-neural ONNX model. Not
required at runtime; the shipped `model.onnx` is checked in.

## Setup

```bash
cd engines/composer/neural/tools
python -m venv .venv
source .venv/bin/activate        # Windows: .venv\Scripts\activate
pip install -r requirements.txt
```

Pinned versions matter: torch==2.2.2 and onnx==1.16.1 produce
byte-deterministic ONNX output given the same seed. Bumping them
changes the weights and invalidates the fixtures.

Corpus

corpus/seed.json holds 50 four-bar progressions. Each example:

```json
{ "style": "pop",
  "measures": [
    [[60, 5, 2], [64, 5, 2], [67, 5, 2]],
    [[65, 5, 2], [69, 5, 2], [72, 5, 2]],
    ...
  ] }
```

Each inner triple is [pitch, duration_bucket, velocity_bucket]
where the buckets match the tokenizer layout in ../TOKENIZER.md:

Duration bucket Meaning
0 1/32
1 1/16
2 1/8
3 1/4
4 1/2
5 whole
6 dotted quarter
7 triplet eighth

Velocity bucket Meaning
0 p
1 mp
2 mf
3 f

Generating seed.json

If corpus/seed.json is absent, generate it deterministically with
this one-liner (save it as gen_corpus.py in this directory):

```python
#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import json

# Triad pitch offsets from the tonic, in semitones.
TRIADS = {
    0: (0, 4, 7),    # I   / i
    1: (2, 5, 9),    # ii  / ii°
    2: (4, 7, 11),   # iii / III
    3: (5, 9, 0),    # IV  / iv
    4: (7, 11, 2),   # V   / v
    5: (9, 0, 4),    # vi  / VI
    6: (11, 2, 5),   # vii°/ VII
}

# Five progressions covering the common functional patterns.
PROGRESSIONS = [
    (0, 3, 4, 0),    # I IV V I
    (0, 5, 1, 4),    # I vi ii V
    (0, 4, 5, 3),    # I V vi IV
    (0, 1, 2, 0),    # I ii iii I
    (3, 4, 0, 0),    # IV V I I
]

STYLES = ["pop", "jazz", "classical", "ambient"]

def triad_pitches(degree, key_midi=60):
    return [key_midi + offs for offs in TRIADS[degree]]

def main():
    examples = []
    for i in range(50):
        style = STYLES[i % len(STYLES)]
        prog = PROGRESSIONS[i % len(PROGRESSIONS)]
        measures = []
        for degree in prog:
            # Each triad is a block chord: three notes, duration
            # bucket 5 (whole measure), velocity bucket 2 (mf).
            measure = [[p, 5, 2] for p in triad_pitches(degree)]
            measures.append(measure)
        examples.append({"style": style, "measures": measures})

    with open("corpus/seed.json", "w") as f:
        json.dump({"examples": examples}, f, indent=2)
    print(f"wrote corpus/seed.json with {len(examples)} examples")

if __name__ == "__main__":
    main()
```

Then:

```bash
mkdir -p corpus
python gen_corpus.py
```

Train

```bash
python train.py --seed 42 --out ../model/model.onnx
```

· Runs in under 10 minutes on CPU.
· Emits model.onnx ≤ 2 MB.
· Prints three fixed-seed token vectors to stderr. Copy each into
  ../tests/fixtures/expected_seed{42,7,1234}.txt so the test suite
  can verify the shipped model matches this training run.

Re-export from a checkpoint

```bash
python export_onnx.py --ckpt checkpoint.pt --out ../model/model.onnx
```

Corpus license

All examples are original and placed in the public domain (CC0).
No third-party MIDI files were used.

```

---
