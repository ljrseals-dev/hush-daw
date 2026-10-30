# TOKENIZER.md — fixed 147-token layout

SPDX-License-Identifier: MIT
Copyright (c) 2026 ljrseals-dev

This is the authoritative contract between the tokenizer and the ONNX model.
Changing any index requires regenerating the model.

| Index range | Token                          | Notes |
|-------------|--------------------------------|-------|
| 0           | BOS                            | always first |
| 1           | EOS                            | always last (before PAD) |
| 2           | STYLE                          | followed by a single style byte |
| 3           | BAR                            | precedes every measure, including the first |
| 4–131       | pitch 0–127                    | MIDI pitch |
| 132–139     | duration bucket 0–7            | 1/32, 1/16, 1/8, 1/4, 1/2, whole, dotted-quarter, triplet-eighth |
| 140–143     | velocity bucket 0–3            | p, mp, mf, f |
| 144         | REST                           | followed by a duration bucket |
| 145         | END                            | reserved |
| 146         | PAD                            | trailing only; ignored by decoder |

**Total vocabulary size = 147.**

## Style bytes (after STYLE token)

| Byte | Style      |
|------|------------|
| `p`  | pop        |
| `j`  | jazz       |
| `c`  | classical  |
| `a`  | ambient    |

## Encoding shape

```
[BOS] [STYLE s] [BAR] [pitch dur vel]* ... [BAR] [pitch dur vel]* [EOS] [PAD]*
```

- BAR appears before every measure (including the first).
- An empty measure is encoded as `[BAR] [REST] [dur]`.
- The decoder is strict: any malformed token raises `TokenizerError` with the index.

## Round-trip guarantee

For any well-formed `Score` that fits inside `maxTokens`,

```
detokenize(tokenize(score, maxTokens, styleByte)) == score
```

(field-for-field on pitch, durationTicks, velocity, startTick, and measure boundaries).
