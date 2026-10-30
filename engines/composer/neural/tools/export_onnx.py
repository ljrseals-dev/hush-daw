#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 ljrseals-dev
"""
Re-export a saved checkpoint to ONNX.

Use this when you have a .pt from a training run and want to
regenerate the ONNX without retraining.

Usage:
    python export_onnx.py --ckpt checkpoint.pt --out ../model/model.onnx
"""
import argparse
import os
import sys

import torch

def build_model(max_len: int):
    # Import here so this script fails with a clear message if the
    # train module is missing, rather than at the top of the file.
    try:
        from train import TinyTransformer
    except ImportError as e:
        print(f"error: cannot import TinyTransformer from train.py: {e}",
              file=sys.stderr)
        print("       run this script from the tools/ directory.",
              file=sys.stderr)
        sys.exit(1)
    return TinyTransformer(max_len=max_len)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ckpt", required=True,
                    help="path to a .pt saved by train.py")
    ap.add_argument("--out", required=True,
                    help="path to write model.onnx")
    ap.add_argument("--max-len", type=int, default=256,
                    help="position embedding length (must match training)")
    args = ap.parse_args()

    if not os.path.isfile(args.ckpt):
        print(f"error: checkpoint not found: {args.ckpt}", file=sys.stderr)
        sys.exit(1)

    model = build_model(args.max_len)

    try:
        state = torch.load(args.ckpt, map_location="cpu")
    except Exception as e:
        print(f"error: failed to load checkpoint: {e}", file=sys.stderr)
        sys.exit(1)

    model.load_state_dict(state)
    model.eval()

    dummy = torch.zeros(1, args.max_len, dtype=torch.long)

    try:
        torch.onnx.export(
            model,
            dummy,
            args.out,
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

    size_mb = os.path.getsize(args.out) / 1e6
    print(f"wrote {args.out} ({size_mb:.2f} MB)")

    if size_mb > 2.0:
        print(f"warning: model is {size_mb:.2f} MB, larger than the "
              f"2 MB budget in the README", file=sys.stderr)

if __name__ == "__main__":
    main()
