#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 ljrseals-dev
"""
Deterministic training for the hush-neural tiny transformer.

Trains a 2-layer, 4-head, d_model=64 transformer on the seed corpus,
exports to ONNX, and prints three fixed-seed token vectors that are
copied into tests/fixtures/expected_seed{42,7,1234}.txt.

Run:
    python train.py --seed 42 --out ../model/model.onnx

Determinism requirements:
    - Same --seed always produces byte-identical model.onnx
    - Environment: torch==2.2.2, onnx==1.16.1 (pinned in requirements.txt)
    - CPU-only execution (no CUDA calls, so no nondeterministic kernels)
"""
import argparse
import json
import os
import random
import sys

import numpy as np
import torch
import torch.nn as nn

# ---------------------------------------------------------------------------
# Tokenizer constants — must match TOKENIZER.md exactly
# ---------------------------------------------------------------------------

VOCAB = 147

BOS   = 0
EOS   = 1
STYLE = 2
BAR   = 3

PITCH_BASE = 4       # 4..131
DUR_BASE   = 132     # 132..139
VEL_BASE   = 140     # 140..143
REST       = 144
END        = 145
PAD        = 146

STYLE_BYTES = {
    "pop":       ord("p"),
    "jazz":      ord("j"),
    "classical": ord("c"),
    "ambient":   ord("a"),
}

# ---------------------------------------------------------------------------
# Model
# ---------------------------------------------------------------------------

class TinyTransformer(nn.Module):
    def __init__(self, vocab=VOCAB, d_model=64, n_heads=4,
                 n_layers=2, max_len=256):
        super().__init__()
        self.tok_emb = nn.Embedding(vocab, d_model)
        self.pos_emb = nn.Embedding(max_len, d_model)
        layer = nn.TransformerEncoderLayer(
            d_model=d_model,
            nhead=n_heads,
            dim_feedforward=d_model * 4,
            dropout=0.0,
            batch_first=True,
            activation="gelu",
        )
        self.encoder = nn.TransformerEncoder(layer,
                                             num_layers=n_layers,
                                             enable_nested_tensor=False)
        self.ln   = nn.LayerNorm(d_model)
        self.head = nn.Linear(d_model, vocab)
        self.max_len = max_len

    def forward(self, x):
        B, T = x.shape
        if T > self.max_len:
            raise ValueError(
                f"sequence length {T} exceeds max_len {self.max_len}")
        pos = torch.arange(T, device=x.device).unsqueeze(0).expand(B, T)
        h = self.tok_emb(x) + self.pos_emb(pos)
        # Causal mask: the model cannot attend to future positions.
        mask = torch.triu(
            torch.ones(T, T, device=x.device, dtype=torch.bool),
            diagonal=1)
        h = self.encoder(h, mask=mask)
        h = self.ln(h)
        return self.head(h)

# ---------------------------------------------------------------------------
# Corpus encoding
# ---------------------------------------------------------------------------
#
# The corpus is a list of {"style": str, "measures": [[[p,d,v], ...], ...]}
# where p is MIDI pitch, d is duration bucket 0..7, v is velocity
# bucket 0..3.

def encode_example(ex):
    toks = [BOS, STYLE, STYLE_BYTES[ex["style"]]]
    for measure in ex["measures"]:
        toks.append(BAR)
        if not measure:
            toks.extend([REST, DUR_BASE + 5])    # whole-measure rest
        else:
            for pitch, dur_b, vel_b in measure:
                if not (0 <= pitch <= 127):
                    raise ValueError(f"pitch out of range: {pitch}")
                if not (0 <= dur_b <= 7):
                    raise ValueError(f"duration bucket out of range: {dur_b}")
                if not (0 <= vel_b <= 3):
                    raise ValueError(f"velocity bucket out of range: {vel_b}")
                toks.extend([
                    PITCH_BASE + pitch,
                    DUR_BASE + dur_b,
                    VEL_BASE + vel_b,
                ])
    toks.append(EOS)
    return toks

def make_dataset(corpus_path, max_len):
    with open(corpus_path) as f:
        raw = json.load(f)
    if "examples" not in raw or not raw["examples"]:
        raise ValueError(f"{corpus_path}: no examples")

    seqs = [encode_example(e) for e in raw["examples"]]

    # Pad or truncate to max_len. Truncation preserves EOS.
    fixed = []
    for s in seqs:
        if len(s) > max_len:
            s = s[:max_len - 1] + [EOS]
        if len(s) < max_len:
            s = s + [PAD] * (max_len - len(s))
        fixed.append(s)
    return torch.tensor(fixed, dtype=torch.long)

# ---------------------------------------------------------------------------
# Training
# ---------------------------------------------------------------------------

def train(seed, corpus_path, out_path, epochs=40, lr=3e-3, max_len=256):
    # ---- Determinism ----
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.use_deterministic_algorithms(True)
    torch.backends.cudnn.deterministic = True
    torch.backends.cudnn.benchmark = False

    data = make_dataset(corpus_path, max_len)
    model = TinyTransformer(max_len=max_len)
    opt = torch.optim.AdamW(model.parameters(), lr=lr)
    lossf = nn.CrossEntropyLoss(ignore_index=PAD)

    model.train()
    for epoch in range(epochs):
        inp = data[:, :-1]
        tgt = data[:, 1:]
        logits = model(inp)
        loss = lossf(logits.reshape(-1, VOCAB), tgt.reshape(-1))
        opt.zero_grad()
        loss.backward()
        opt.step()
        if (epoch + 1) % 10 == 0 or epoch == 0:
            print(f"epoch {epoch+1:3d}  loss {loss.item():.4f}",
                  file=sys.stderr)

    model.eval()

    # ---- ONNX export ----
    dummy = torch.zeros(1, max_len, dtype=torch.long)
    try:
        torch.onnx.export(
            model,
            dummy,
            out_path,
            input_names=["input_ids"],
            output_names=["logits"],
            dynamic_axes={
                "input_ids": {1: "seq"},
                "logits":    {1: "seq"},
            },
            opset_version=17,
            do_constant_folding=True,
        )
    except Exception as e:
        print(f"error: ONNX export failed: {e}", file=sys.stderr)
        sys.exit(1)

    size_mb = os.path.getsize(out_path) / 1e6
    print(f"wrote {out_path} ({size_mb:.2f} MB)", file=sys.stderr)
    if size_mb > 2.0:
        print(f"warning: model is {size_mb:.2f} MB, over the 2 MB "
              f"budget stated in the README", file=sys.stderr)

    # ---- Fixture emission ----
    # For each of three fixed seeds, greedily decode 16 tokens from
    # the seeded prompt and print them. The test suite compares the
    # shipped model's output against these vectors.
    print("", file=sys.stderr)
    print("--- fixture vectors (copy into ../tests/fixtures/) ---",
          file=sys.stderr)
    for s in (42, 7, 1234):
        torch.manual_seed(s)
        ids = torch.tensor(
            [[BOS, STYLE, STYLE_BYTES["pop"]]],
            dtype=torch.long)
        gen = []
        for _ in range(16):
            with torch.no_grad():
                out = model(ids)
            nxt = int(out[0, -1].argmax().item())
            gen.append(nxt)
            ids = torch.cat(
                [ids, torch.tensor([[nxt]], dtype=torch.long)], dim=1)
        print(f"seed={s}: {' '.join(str(g) for g in gen)}",
              file=sys.stderr)
    print("--- end ---", file=sys.stderr)

# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--corpus", default="corpus/seed.json")
    ap.add_argument("--out", default="../model/model.onnx")
    ap.add_argument("--epochs", type=int, default=40)
    ap.add_argument("--lr", type=float, default=3e-3)
    ap.add_argument("--max-len", type=int, default=256)
    args = ap.parse_args()

    if not os.path.isfile(args.corpus):
        print(f"error: corpus not found: {args.corpus}", file=sys.stderr)
        print(f"       run gen_corpus.py first (see tools/README.md)",
              file=sys.stderr)
        sys.exit(1)

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    train(args.seed, args.corpus, args.out,
          epochs=args.epochs, lr=args.lr, max_len=args.max_len)

if __name__ == "__main__":
    main()
