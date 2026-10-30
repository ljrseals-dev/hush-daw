# hush-immersive

Object-based immersive audio engine for the hush-daw project.

## Features

- Complete speaker layouts for 11 channel formats (Mono … 9.1.6, FOA/SOA/HOA3)
- Real VBAP object panning with distance attenuation and screen-anchor
- Correct ACN / SN3D Ambisonics encoding + HOA decoding and rotation matrices
- Binaural fold-down (optional libmysofa SOFA with FFT convolution + always-available spherical-head model)
- Full multi-rate resampling (linear / cubic Hermite) between bed and object files
- ADM / BW64 export with axml metadata
- JSON scene parser and structured levels.json writer
- Minimal real-time UDP PCM transport (`--stream host:port`)
- CLI renderer `hush-immersive-render`

## Build

```bash
cmake -S engines/immersive -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

With or without libmysofa the build succeeds. When libmysofa is absent the spherical-head model is used automatically. FFT-based convolution is used when SOFA IRs are loaded.

## CLI

```bash
./build/hush-immersive-render --help
./build/hush-immersive-render scene.txt --out out.wav
./build/hush-immersive-render scene.json --out out.wav --binaural hrir.sofa --adm out.w64 --levels levels.json
./build/hush-immersive-render scene.txt --out out.wav --stream 127.0.0.1:9000
```

### Text scene example

```
format SevenPointOneFour
bed bed.wav gain -6
object voice voice.wav az 30 el 10 dist 1.5 size 0.2 gain -3
object fx   fx.wav    az -45 el 0  anchor 0.8 gain 0
```

### JSON scene example

```json
{
  "format": "SevenPointOneFour",
  "bed": { "path": "bed.wav", "gainDb": -6 },
  "objects": [
    {
      "name": "voice",
      "path": "voice.wav",
      "az": 30,
      "el": 10,
      "dist": 1.5,
      "size": 0.2,
      "gainDb": -3
    }
  ]
}
```

Sample-rate mismatches between bed and object files are automatically corrected with cubic Hermite resampling.

## ADM verification

```bash
ffprobe -show_streams out.w64
```

Expect the correct channel count and the presence of an `axml` chunk.

## Design notes

- Ambisonics panning deliberately ignores divergence and size.
- Distance attenuation uses inverse-square with a –60 dB floor.
- Screen-anchor linearly blends azimuth toward 0 before gain computation.
- HOA rotation currently applies a full 3×3 matrix to the first-order components; higher-order coefficients are left unchanged in this reference build (full Wigner-D matrices can be added later).
- UDP transport is a minimal framed PCM stream intended for low-latency local networks; it does not implement FEC or clock recovery.

## License

MIT — Copyright (c) 2026 ljrseals-dev

## Ambisonic rotation

The `Rotation` class implements per-order block-diagonal Wigner-D matrices
for orders 1–3 in the real ACN/SN3D basis.

**Convention.** Default is YPR_ZYX (intrinsic yaw-pitch-roll ≈ ZYZ Euler):
α = yaw, β = pitch, γ = roll, all in degrees. An extrinsic YPR_XYZ option
reverses the composition order.

**Closed forms.** Path A: explicit closed-form Wigner small-d matrices for
l ≤ 3 (Zotter & Frank, *Ambisonics*, Springer 2019, Ch. 5 / standard
tabulated forms). Order-1 uses the exact 3×3 Cartesian rotation of the
(Y,Z,X) triplet for numerical tightness.

**Correctness test (T3).** For random source direction d, decode direction
d', and rotation R:
```
  encode(d) → rotate(R) → decode(d')
  == encode(d) → decode(R⁻¹ · d')
```
to within 1.5×10⁻³ linear across 50 rotations × 4 direction pairs × 3
orders. This is the only test that proves the math; see
`tests/rotation_tests.cpp`.
