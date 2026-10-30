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
