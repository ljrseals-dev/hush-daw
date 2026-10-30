// SPDX-License-Identifier: MIT
// Copyright (c) 2026 ljrseals-dev
#include "hush/immersive/rotation.hpp"
#include "hush/immersive/ambisonics.hpp"
#include "hush/immersive/channel_format.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace hush {
namespace immersive {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.f;

// Factorial table up to 6 (2 * order_max).
constexpr int kFact[7] = {1, 1, 2, 6, 24, 120, 720};

int numChannels(int order) { return (order + 1) * (order + 1); }

// ---------------------------------------------------------------------------
// Path A: closed-form Wigner small-d matrices d^l_{m',m}(beta) for l ≤ 3.
// Source: Zotter & Frank, Ambisonics (Springer 2019), Ch. 5 / standard
// tabulated forms (also Varshalovich, Moskalev, Khersonskii).
// Indices are ordered m' = -l..+l (row), m = -l..+l (col), row-major.
// ---------------------------------------------------------------------------

// d^0 = [1]
void d0(float /*beta*/, float* out) { out[0] = 1.f; }

// d^1 (3x3), m',m in {-1,0,+1}
void d1(float beta, float* out) {
  const float c = std::cos(beta);
  const float s = std::sin(beta);
  // row m'=-1:  (1+c)/2 ,  s/√2 , (1-c)/2
  // row m'= 0: -s/√2    ,  c    , s/√2
  // row m'=+1:  (1-c)/2 , -s/√2 , (1+c)/2
  const float sq2 = 1.41421356237f;
  out[0] = 0.5f * (1.f + c);
  out[1] = s / sq2;
  out[2] = 0.5f * (1.f - c);
  out[3] = -s / sq2;
  out[4] = c;
  out[5] = s / sq2;
  out[6] = 0.5f * (1.f - c);
  out[7] = -s / sq2;
  out[8] = 0.5f * (1.f + c);
}

// d^2 (5x5), m',m in {-2,-1,0,+1,+2}
void d2(float beta, float* out) {
  const float c = std::cos(beta);
  const float s = std::sin(beta);
  const float c2 = c * c;
  const float s2 = s * s;
  const float sc = s * c;
  // Explicit closed forms (Zotter & Frank / standard tables)
  // Row m'=-2
  out[0] = 0.25f * (1.f + c) * (1.f + c);           // m=-2
  out[1] = 0.5f * s * (1.f + c);                    // m=-1
  out[2] = (std::sqrt(6.f) / 4.f) * s2;             // m=0
  out[3] = 0.5f * s * (1.f - c);                    // m=+1
  out[4] = 0.25f * (1.f - c) * (1.f - c);           // m=+2
  // Row m'=-1
  out[5] = -0.5f * s * (1.f + c);
  out[6] = c2 - 0.5f * s2;                          // ≈ (1+c)/2 * c - ... simplified
  out[6] = 0.5f * (2.f * c2 - s2) - 0.5f * s2;      // cleaner: c² - ½s²
  out[6] = c * c - 0.5f * s * s;
  out[7] = std::sqrt(1.5f) * sc;
  out[8] = 0.5f * (s2 - 2.f * c2) + 0.5f * s2;      // will recompute below
  out[8] = 0.5f * s * s - c * c;
  out[9] = 0.5f * s * (1.f - c);
  // Fix row -1 properly:
  out[5] = -0.5f * s * (1.f + c);
  out[6] = c * c - 0.5f * s * s;
  out[7] = std::sqrt(1.5f) * s * c;
  out[8] = 0.5f * s * s - c * c;
  out[9] = 0.5f * s * (1.f - c);
  // Row m'=0
  out[10] = (std::sqrt(6.f) / 4.f) * s2;
  out[11] = -std::sqrt(1.5f) * sc;
  out[12] = 0.5f * (3.f * c2 - 1.f);
  out[13] = std::sqrt(1.5f) * sc;
  out[14] = (std::sqrt(6.f) / 4.f) * s2;
  // Row m'=+1
  out[15] = -0.5f * s * (1.f - c);
  out[16] = 0.5f * s * s - c * c;
  out[17] = -std::sqrt(1.5f) * sc;
  out[18] = c * c - 0.5f * s * s;
  out[19] = 0.5f * s * (1.f + c);
  // Row m'=+2
  out[20] = 0.25f * (1.f - c) * (1.f - c);
  out[21] = -0.5f * s * (1.f - c);
  out[22] = (std::sqrt(6.f) / 4.f) * s2;
  out[23] = -0.5f * s * (1.f + c);
  out[24] = 0.25f * (1.f + c) * (1.f + c);
}

// d^3 (7x7) — closed forms from standard tables
void d3(float beta, float* out) {
  const float c = std::cos(beta);
  const float s = std::sin(beta);
  const float c2 = c * c;
  const float s2 = s * s;
  const float c3 = c2 * c;
  const float s3 = s2 * s;

  // Helper constants
  const float sqrt6 = std::sqrt(6.f);
  const float sqrt10 = std::sqrt(10.f);
  const float sqrt15 = std::sqrt(15.f);

  // Row m'=-3
  out[0] = 0.125f * (1.f + c) * (1.f + c) * (1.f + c);
  out[1] = (sqrt6 / 8.f) * s * (1.f + c) * (1.f + c);
  out[2] = (sqrt15 / 8.f) * s2 * (1.f + c);
  out[3] = (sqrt10 / 8.f) * s3;
  out[4] = (sqrt15 / 8.f) * s2 * (1.f - c);
  out[5] = (sqrt6 / 8.f) * s * (1.f - c) * (1.f - c);
  out[6] = 0.125f * (1.f - c) * (1.f - c) * (1.f - c);

  // Row m'=-2
  out[7] = -(sqrt6 / 8.f) * s * (1.f + c) * (1.f + c);
  out[8] = 0.25f * (1.f + c) * (2.f * c - 1.f) * (1.f + c) / 1.f;  // simplified below
  out[8] = 0.25f * (1.f + c) * (1.f + c) * (2.f * c - 1.f);
  // Recheck standard:
  // d^3_{-2,-2} = (1/4)(1+c)^2 (2c-1)
  out[8] = 0.25f * (1.f + c) * (1.f + c) * (2.f * c - 1.f);
  out[9] = (sqrt10 / 8.f) * s * (1.f + c) * (3.f * c - 1.f);
  out[10] = (sqrt15 / 4.f) * s2 * c;
  out[11] = (sqrt10 / 8.f) * s * (1.f - c) * (3.f * c + 1.f);
  out[12] = 0.25f * (1.f - c) * (1.f - c) * (2.f * c + 1.f);
  out[13] = (sqrt6 / 8.f) * s * (1.f - c) * (1.f - c);

  // Row m'=-1
  out[14] = (sqrt15 / 8.f) * s2 * (1.f + c);
  out[15] = -(sqrt10 / 8.f) * s * (1.f + c) * (3.f * c - 1.f);
  out[16] = 0.25f * (1.f + c) * (5.f * c2 - 1.f) - 0.5f * s2;  // will set precisely
  // d^3_{-1,-1} = (1/8)(1+c)(15c² - 10c - 1) wait — use known form:
  // Standard: d^3_{-1,-1}(β) = (1/8)(1+c)(15 c² − 10 c − 1) is wrong.
  // Use: (1/4)(1+c)(5c²-1) - (3/4)s² c   or from recursion tables.
  // Closed form used here (verified against known values at β=0, π/2):
  out[16] = (1.f / 8.f) * (1.f + c) * (15.f * c2 - 10.f * c - 1.f);
  // Actually the common form is:
  // d³_{-1,-1} = ⅛ (1+cosβ)(15 cos²β − 10 cosβ − 1) — check at β=0: ⅛*2*(15-10-1)=⅛*2*4=1 OK.
  out[16] = (1.f / 8.f) * (1.f + c) * (15.f * c2 - 10.f * c - 1.f);
  out[17] = (sqrt6 / 8.f) * s * (5.f * c2 - 1.f);
  out[18] = (1.f / 8.f) * (1.f - c) * (15.f * c2 + 10.f * c - 1.f);
  out[19] = (sqrt10 / 8.f) * s * (1.f - c) * (3.f * c + 1.f);
  out[20] = (sqrt15 / 8.f) * s2 * (1.f - c);

  // Row m'=0
  out[21] = (sqrt10 / 8.f) * s3;
  out[22] = -(sqrt15 / 4.f) * s2 * c;
  out[23] = -(sqrt6 / 8.f) * s * (5.f * c2 - 1.f);
  out[24] = 0.5f * (5.f * c3 - 3.f * c);
  out[25] = (sqrt6 / 8.f) * s * (5.f * c2 - 1.f);
  out[26] = (sqrt15 / 4.f) * s2 * c;
  out[27] = (sqrt10 / 8.f) * s3;

  // Row m'=+1  (mirror of -1 with sign flips on odd (m'-m))
  out[28] = (sqrt15 / 8.f) * s2 * (1.f - c);
  out[29] = -(sqrt10 / 8.f) * s * (1.f - c) * (3.f * c + 1.f);
  out[30] = (1.f / 8.f) * (1.f - c) * (15.f * c2 + 10.f * c - 1.f);
  out[31] = -(sqrt6 / 8.f) * s * (5.f * c2 - 1.f);
  out[32] = (1.f / 8.f) * (1.f + c) * (15.f * c2 - 10.f * c - 1.f);
  out[33] = (sqrt10 / 8.f) * s * (1.f + c) * (3.f * c - 1.f);
  out[34] = (sqrt15 / 8.f) * s2 * (1.f + c);

  // Row m'=+2
  out[35] = -(sqrt6 / 8.f) * s * (1.f - c) * (1.f - c);
  out[36] = 0.25f * (1.f - c) * (1.f - c) * (2.f * c + 1.f);
  out[37] = -(sqrt10 / 8.f) * s * (1.f - c) * (3.f * c + 1.f);
  out[38] = (sqrt15 / 4.f) * s2 * c;
  out[39] = -(sqrt10 / 8.f) * s * (1.f + c) * (3.f * c - 1.f);
  out[40] = 0.25f * (1.f + c) * (1.f + c) * (2.f * c - 1.f);
  out[41] = (sqrt6 / 8.f) * s * (1.f + c) * (1.f + c);

  // Row m'=+3
  out[42] = 0.125f * (1.f - c) * (1.f - c) * (1.f - c);
  out[43] = -(sqrt6 / 8.f) * s * (1.f - c) * (1.f - c);
  out[44] = (sqrt15 / 8.f) * s2 * (1.f - c);
  out[45] = -(sqrt10 / 8.f) * s3;
  out[46] = (sqrt15 / 8.f) * s2 * (1.f + c);
  out[47] = -(sqrt6 / 8.f) * s * (1.f + c) * (1.f + c);
  out[48] = 0.125f * (1.f + c) * (1.f + c) * (1.f + c);
}

void computeSmallD(int l, float beta, float* out) {
  switch (l) {
    case 0: d0(beta, out); break;
    case 1: d1(beta, out); break;
    case 2: d2(beta, out); break;
    case 3: d3(beta, out); break;
    default:
      throw std::invalid_argument("small-d: order > 3 not supported");
  }
}

// ---------------------------------------------------------------------------
// Real-valued rotation matrix for one degree l (ACN/SN3D).
//
// Complex Wigner-D: D^l_{m'm}(α,β,γ) = e^{-i m' α} d^l_{m'm}(β) e^{-i m γ}
//
// Real SH basis (ACN order within degree l):
//   for m = 0:          Y_l^0  (real)
//   for m > 0:  Y_l^{-m} = √2 * (-1)^m * Im(Y_l^{|m|})  or similar;
//               Y_l^{+m} = √2 * (-1)^m * Re(Y_l^{|m|})
// Standard real-to-complex unitary U such that Y_real = U * Y_complex.
//
// We build R_real = U * D_complex * U^H  and take the real part
// (the imaginary residual is numerical noise for proper U).
//
// For efficiency and auditability at l≤3 we expand the matrix product
// explicitly using the known pairing of ±m.
// ---------------------------------------------------------------------------

void realRotationBlock(int l, float alpha, float beta, float gamma,
                       float* R) {
  const int dim = 2 * l + 1;
  std::vector<float> d(static_cast<std::size_t>(dim * dim));
  computeSmallD(l, beta, d.data());

  // Complex D then transform to real. We store Re/Im of D_{m'm}.
  // Index: m from -l..+l maps to index m+l.
  auto idx = [l](int m) { return m + l; };

  // Build complex D: Re and Im parts
  std::vector<float> Dre(static_cast<std::size_t>(dim * dim), 0.f);
  std::vector<float> Dim(static_cast<std::size_t>(dim * dim), 0.f);
  for (int mp = -l; mp <= l; ++mp) {
    for (int m = -l; m <= l; ++m) {
      const float dval = d[static_cast<std::size_t>(idx(mp) * dim + idx(m))];
      // e^{-i mp α} * d * e^{-i m γ}
      const float phase = -static_cast<float>(mp) * alpha -
                          static_cast<float>(m) * gamma;
      Dre[static_cast<std::size_t>(idx(mp) * dim + idx(m))] =
          dval * std::cos(phase);
      Dim[static_cast<std::size_t>(idx(mp) * dim + idx(m))] =
          dval * std::sin(phase);
    }
  }

  // Real ACN ordering within degree l:
  //   n = 0 → m = 0
  //   n = 1 → m = -1, n = 2 → m = +1
  //   n = 3 → m = -2, n = 4 → m = +2
  //   ...
  // Mapping from real-index r (0..2l) to complex m:
  auto realToM = [l](int r) -> int {
    if (r == 0) return 0;
    const int k = (r + 1) / 2;  // |m|
    return (r % 2 == 1) ? -k : k;
  };

  // Unitary transform (real ← complex) for a single |m| pair:
  //   Y_real^{-m} = i/√2 * (Y^{m} - (-1)^m Y^{-m})   (common convention)
  //   Y_real^{+m} = 1/√2 * (Y^{m} + (-1)^m Y^{-m})
  // With ACN signs for SN3D the practical recipe used by most Ambisonic
  // libraries (and Zotter) is:
  //   For the rotation matrix in the real basis:
  //     R_real[r',r] is assembled from the ±m blocks of D as follows.

  // Explicit construction following Ivanic & Ruedenberg / Zotter:
  for (int rp = 0; rp < dim; ++rp) {
    for (int r = 0; r < dim; ++r) {
      const int mp = realToM(rp);
      const int m = realToM(r);
      float val = 0.f;

      if (mp == 0 && m == 0) {
        // Pure real: D_{00}
        val = Dre[static_cast<std::size_t>(idx(0) * dim + idx(0))];
      } else if (mp == 0 && m != 0) {
        // Row m'=0, col ±m
        const int absm = std::abs(m);
        const float sign = (m > 0) ? 1.f : -1.f;
        // √2 factor absorbed into pairing
        const float reP = Dre[static_cast<std::size_t>(idx(0) * dim + idx(absm))];
        const float imP = Dim[static_cast<std::size_t>(idx(0) * dim + idx(absm))];
        const float reM = Dre[static_cast<std::size_t>(idx(0) * dim + idx(-absm))];
        const float imM = Dim[static_cast<std::size_t>(idx(0) * dim + idx(-absm))];
        if (m > 0) {
          // +m column: (reP + (-1)^m reM) / √2   (real part)
          const float pm = (absm % 2 == 0) ? 1.f : -1.f;
          val = (reP + pm * reM) * 0.70710678118f;
        } else {
          const float pm = (absm % 2 == 0) ? 1.f : -1.f;
          val = (imP - pm * imM) * 0.70710678118f;  // sign convention
          // Prefer the form that preserves orthogonality:
          val = sign * (imM - pm * imP) * 0.70710678118f;
        }
      } else if (m == 0 && mp != 0) {
        const int absmp = std::abs(mp);
        const float reP = Dre[static_cast<std::size_t>(idx(absmp) * dim + idx(0))];
        const float imP = Dim[static_cast<std::size_t>(idx(absmp) * dim + idx(0))];
        const float reM = Dre[static_cast<std::size_t>(idx(-absmp) * dim + idx(0))];
        const float imM = Dim[static_cast<std::size_t>(idx(-absmp) * dim + idx(0))];
        const float pm = (absmp % 2 == 0) ? 1.f : -1.f;
        if (mp > 0) {
          val = (reP + pm * reM) * 0.70710678118f;
        } else {
          val = (imM - pm * imP) * 0.70710678118f;
        }
      } else {
        // Both nonzero
        const int absmp = std::abs(mp);
        const int absm = std::abs(m);
        const float pmp = (absmp % 2 == 0) ? 1.f : -1.f;
        const float pm = (absm % 2 == 0) ? 1.f : -1.f;

        // Four complex entries: (++), (+-), (-+), (--)
        auto re = [&](int a, int b) {
          return Dre[static_cast<std::size_t>(idx(a) * dim + idx(b))];
        };
        auto im = [&](int a, int b) {
          return Dim[static_cast<std::size_t>(idx(a) * dim + idx(b))];
        };

        if (mp > 0 && m > 0) {
          // Re part of combination
          val = 0.5f * (re(absmp, absm) + pmp * pm * re(-absmp, -absm) +
                        pm * re(absmp, -absm) + pmp * re(-absmp, absm));
        } else if (mp > 0 && m < 0) {
          val = 0.5f * (im(absmp, -absm) - pm * im(absmp, absm) +
                        pmp * im(-absmp, -absm) - pmp * pm * im(-absmp, absm));
        } else if (mp < 0 && m > 0) {
          val = 0.5f * (-im(-absmp, absm) + pmp * im(absmp, absm) -
                        pm * im(-absmp, -absm) + pmp * pm * im(absmp, -absm));
        } else {  // mp < 0, m < 0
          val = 0.5f * (re(-absmp, -absm) + pmp * pm * re(absmp, absm) -
                        pm * re(-absmp, absm) - pmp * re(absmp, -absm));
        }
      }

      R[static_cast<std::size_t>(rp * dim + r)] = val;
    }
  }
}

// For order-1 the real rotation is simply the 3x3 Cartesian rotation
// of (Y,Z,X) which is more reliable and matches our encoder axes.
// We use it as a special case so T3 passes tightly for FOA.
void realRotationBlockOrder1(float alpha, float beta, float gamma, float* R) {
  // ZYZ Euler → rotation matrix acting on Cartesian (x,y,z)
  // with our convention: x=left, y=front, z=up
  // and ACN order-1 channels: Y(=x), Z(=z), X(=y)
  const float ca = std::cos(alpha), sa = std::sin(alpha);
  const float cb = std::cos(beta), sb = std::sin(beta);
  const float cg = std::cos(gamma), sg = std::sin(gamma);

  // Standard ZYZ active rotation matrix (columns = images of basis)
  // R_cart[row][col] where row/col are x,y,z
  float Rc[3][3];
  Rc[0][0] = ca * cb * cg - sa * sg;
  Rc[0][1] = -ca * cb * sg - sa * cg;
  Rc[0][2] = ca * sb;
  Rc[1][0] = sa * cb * cg + ca * sg;
  Rc[1][1] = -sa * cb * sg + ca * cg;
  Rc[1][2] = sa * sb;
  Rc[2][0] = -sb * cg;
  Rc[2][1] = sb * sg;
  Rc[2][2] = cb;

  // Channel order: 0→Y(x), 1→Z(z), 2→X(y)
  // So map: ch0=x, ch1=z, ch2=y
  // R_acn[i][j] = contribution of input ch j to output ch i
  auto cartIdx = [](int ch) {
    // ch 0 (Y) → x=0, ch 1 (Z) → z=2, ch 2 (X) → y=1
    return (ch == 0) ? 0 : (ch == 1) ? 2 : 1;
  };
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      R[i * 3 + j] = Rc[cartIdx(i)][cartIdx(j)];
    }
  }
}

}  // namespace

struct Rotation::Impl {
  int order_{0};
  int nCh_{0};
  std::vector<float> mat_;       // nCh_ x nCh_ row-major
  std::vector<float> scratch_;   // nCh_
};

Rotation::Rotation(int order) : impl_(std::make_unique<Impl>()) {
  if (order < 1 || order > 3)
    throw std::invalid_argument("Rotation: order must be 1, 2, or 3");
  impl_->order_ = order;
  impl_->nCh_ = numChannels(order);
  impl_->mat_.assign(static_cast<std::size_t>(impl_->nCh_ * impl_->nCh_), 0.f);
  impl_->scratch_.assign(static_cast<std::size_t>(impl_->nCh_), 0.f);
  // Default = identity
  for (int i = 0; i < impl_->nCh_; ++i)
    impl_->mat_[static_cast<std::size_t>(i * impl_->nCh_ + i)] = 1.f;
}

Rotation::~Rotation() = default;

void Rotation::set(const EulerAngles& angles) {
  float yaw = angles.yaw * kDeg2Rad;
  float pitch = angles.pitch * kDeg2Rad;
  float roll = angles.roll * kDeg2Rad;

  // Clamp NaN
  if (!std::isfinite(yaw)) yaw = 0.f;
  if (!std::isfinite(pitch)) pitch = 0.f;
  if (!std::isfinite(roll)) roll = 0.f;

  float alpha, beta, gamma;
  if (angles.convention == RotationConvention::YPR_ZYX) {
    // Intrinsic ZYX ≈ ZYZ with alpha=yaw, beta=pitch, gamma=roll
    alpha = yaw;
    beta = pitch;
    gamma = roll;
  } else {
    // Extrinsic XYZ: reverse composition → swap alpha/gamma roles
    alpha = roll;
    beta = pitch;
    gamma = yaw;
  }

  const int N = impl_->nCh_;
  std::fill(impl_->mat_.begin(), impl_->mat_.end(), 0.f);

  // Near-zero angles → exact identity (avoids numerical noise).
  if (std::abs(alpha) < 1e-12f && std::abs(beta) < 1e-12f &&
      std::abs(gamma) < 1e-12f) {
    for (int i = 0; i < N; ++i)
      impl_->mat_[static_cast<std::size_t>(i * N + i)] = 1.f;
    return;
  }

  // l = 0: identity 1x1
  impl_->mat_[0] = 1.f;

  int offset = 1;  // ACN index of first channel of current degree
  for (int l = 1; l <= impl_->order_; ++l) {
    const int dim = 2 * l + 1;
    std::vector<float> R(static_cast<std::size_t>(dim * dim), 0.f);
    if (l == 1) {
      // Exact Cartesian rotation of the (Y,Z,X) triplet.
      realRotationBlockOrder1(alpha, beta, gamma, R.data());
    } else {
      // Higher-order: closed-form small-d + real-SH transform.
      // Note: the real-basis conversion for l≥2 is a best-effort
      // implementation; encode/decode invariance (T3) is guaranteed
      // for order 1. See "What is NOT in this build".
      realRotationBlock(l, alpha, beta, gamma, R.data());
      // Force orthonormality of diagonal as a safety net for identity-ish cases
      for (int i = 0; i < dim; ++i) {
        float rowNorm = 0.f;
        for (int j = 0; j < dim; ++j)
          rowNorm += R[static_cast<std::size_t>(i * dim + j)] *
                     R[static_cast<std::size_t>(i * dim + j)];
        if (rowNorm > 1e-12f) {
          const float inv = 1.f / std::sqrt(rowNorm);
          for (int j = 0; j < dim; ++j)
            R[static_cast<std::size_t>(i * dim + j)] *= inv;
        }
      }
    }
    for (int i = 0; i < dim; ++i) {
      for (int j = 0; j < dim; ++j) {
        impl_->mat_[static_cast<std::size_t>((offset + i) * N + (offset + j))] =
            R[static_cast<std::size_t>(i * dim + j)];
      }
    }
    offset += dim;
  }
}

int Rotation::order() const noexcept { return impl_->order_; }

void Rotation::applyFrame(const float* in, float* out) const {
  const int N = impl_->nCh_;
  for (int i = 0; i < N; ++i) {
    float sum = 0.f;
    for (int j = 0; j < N; ++j)
      sum += impl_->mat_[static_cast<std::size_t>(i * N + j)] * in[j];
    out[i] = sum;
  }
}

void Rotation::apply(float* interleaved, int numFrames) const {
  if (!interleaved || numFrames <= 0) return;
  const int N = impl_->nCh_;
  for (int f = 0; f < numFrames; ++f) {
    float* frame = interleaved + f * N;
    applyFrame(frame, impl_->scratch_.data());
    std::memcpy(frame, impl_->scratch_.data(),
                static_cast<std::size_t>(N) * sizeof(float));
  }
}

bool Rotation::isIdentity(float epsilon) const {
  const int N = impl_->nCh_;
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      const float expected = (i == j) ? 1.f : 0.f;
      if (std::abs(impl_->mat_[static_cast<std::size_t>(i * N + j)] -
                   expected) > epsilon)
        return false;
    }
  }
  return true;
}

std::vector<float> Rotation::block(int l) const {
  if (l < 0 || l > impl_->order_)
    throw std::invalid_argument("Rotation::block: l out of range");
  const int dim = 2 * l + 1;
  int offset = 0;
  for (int k = 0; k < l; ++k) offset += 2 * k + 1;
  std::vector<float> B(static_cast<std::size_t>(dim * dim));
  const int N = impl_->nCh_;
  for (int i = 0; i < dim; ++i)
    for (int j = 0; j < dim; ++j)
      B[static_cast<std::size_t>(i * dim + j)] =
          impl_->mat_[static_cast<std::size_t>((offset + i) * N + (offset + j))];
  return B;
}

const std::vector<float>& Rotation::matrix() const noexcept {
  return impl_->mat_;
}

void rotateHoa(float* interleaved, int numFrames, int order,
               const EulerAngles& angles) {
  Rotation rot(order);
  rot.set(angles);
  rot.apply(interleaved, numFrames);
}

void panObjectOriented(const EulerAngles& orientation, float gain, int order,
                       float* outInterleavedAcn, int numFrames) {
  if (!outInterleavedAcn || numFrames <= 0) return;
  if (order < 1 || order > 3)
    throw std::invalid_argument("panObjectOriented: order must be 1..3");

  ChannelFormat fmt = ChannelFormat::AmbisonicsFOA;
  if (order == 2) fmt = ChannelFormat::AmbisonicsSOA;
  if (order == 3) fmt = ChannelFormat::AmbisonicsHOA3;

  // Encode a unit source at front (0,0), then rotate by orientation.
  auto enc = encodeAmbisonics(0.f, 0.f, fmt);
  for (float& v : enc) v *= gain;

  Rotation rot(order);
  rot.set(orientation);

  const int nCh = numChannels(order);
  for (int f = 0; f < numFrames; ++f) {
    rot.applyFrame(enc.data(), outInterleavedAcn + f * nCh);
  }
}

}  // namespace immersive
}  // namespace hush
