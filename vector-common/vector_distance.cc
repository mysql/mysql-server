/* Copyright (c) 2026, Percona and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file vector-common/vector_distance.cc

  Euclidean, cosine, dot-product and Manhattan distance for VECTOR columns
  (float32).

  Public entry points — vector_distance_euclidean_squared(),
  vector_distance_cosine(), vector_distance_dot(), vector_distance_manhattan()
  — are declared in vector-common/vector_distance.h.  Call
  init_vector_distance_functions() once before first use so the dispatch
  pointers are set to the best kernel for the host CPU.

  Same result on every tier
  -------------------------
  DISTANCE() may be stored (functional indexes, stored generated columns), so
  its value must not depend on the CPU.  Every tier therefore computes in the
  same fixed number of lanes, kLanes = 16, and only spreads them over a
  different number of registers: one 16-float register for AVX-512F, two
  8-float registers for AVX2, four 4-float registers for SSE4.2 and NEON, and
  16 single floats for scalar.  Every kernel is one instantiation of the same
  templates with the tier's register width:

  - Element i goes into lane i % kLanes, in index order, so after the main
    loop lane l holds the partial sum of elements l, l + 16, l + 32, ...  The
    SIMD kernels keep the lanes in registers in index order.
  - At the end the 16 lane sums are converted to double and added as a fixed
    binary tree: lane l += lane l + 8 for l < 8, then lane l += lane l + 4
    for l < 4, then + 2, then + 1; lane 0 holds the total.  Every tier adds
    the same pairs in the same order, whatever its register width.
  - The last dims % kLanes elements are added in order in double.

  IEEE 754 rounds each of these operations the same way everywhere, so the
  result depends only on this layout, not on the register width.  The file
  must be compiled with -ffp-contract=off: a fused multiply-add rounds once
  instead of twice and would make FMA-capable tiers differ.

  Fast path and exact fallback
  ----------------------------
  lanes_kernel() accumulates the lanes in float32, which is as fast as the
  loads allow.  Float32 products and squares overflow to Inf near FLT_MAX and
  become subnormal or 0 for tiny elements, so when a float32 sum is non-finite
  or below kMinExactSum, double_lanes_kernel() recomputes the result with the
  lanes in double.  The decision depends only on the float32 lane sums, which
  are the same on every tier, so the fallback keeps the result identical too.
  In double, products of float32 values are exact and neither they nor their
  sums can overflow or underflow.  Other float32 rounding is kept: it is the
  precision of the float32 VECTOR type (e.g. 1e8 + 4 - 1e8 in one lane gives
  0 or 8, as in any float32 sum).

  All kernels accept const char * and any byte alignment: loads use memcpy,
  which compiles to an unaligned load.

  Tiers
  -----
  +----------+---------------------------------+---------------------------+
  | Tier     | Platform                        | Floats per register       |
  +----------+---------------------------------+---------------------------+
  | Scalar   | any (the only one on _WIN32)    | 1                         |
  | Sse42    | x86_64 with SSE4.2              | 4                         |
  | Avx2     | x86_64 with AVX2                | 8                         |
  | Avx512f  | x86_64 with AVX-512F            | 16                        |
  | Neon     | aarch64 (baseline)              | 4                         |
  +----------+---------------------------------+---------------------------+

  The SIMD tiers use GCC/Clang vector extensions; the kernel templates are
  inlined into one function per tier with the tier's target attribute, so this
  translation unit stays at the baseline ISA.  SVE2 hosts use NEON: a fixed
  lane layout does not map onto SVE's runtime vector length.

  Euclidean kernels return sum((a[i]-b[i])²) without sqrt.  SQL applies
  std::sqrt for the EUCLIDEAN metric; EUCLIDEAN_SQUARED uses the kernel result
  directly.

  Cosine distance returns +Inf as a sentinel when either vector has zero norm
  (undefined cosine); the SQL layer (Item_func_vector_distance::val_real)
  detects it via std::isinf and maps it to NULL.  Other results are clamped to
  [0, 2], the range of 1 - cos.
*/

#include "vector-common/vector_distance.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>

#include "mysql/attribute.h"                 // MY_ATTRIBUTE
#include "vector-common/vector_constants.h"  // max_dimensions

// Platform guards — mirror ut0crc32.h:53-69
//
// On _WIN32 (x64 and ARM64) SIMD tiers are not wired up; scalar-only,
// same pragmatic approach as CRC32_DEFAULT in ut0crc32.h.
#if !defined(_WIN32)
#if defined(__x86_64__) || defined(_M_X64)
#define VECTOR_DISTANCE_x86_64
#elif defined(__aarch64__) || defined(_M_ARM64)
#define VECTOR_DISTANCE_AARCH64
#endif
#endif

// GCC and Clang (which also defines __GNUC__); MSVC takes the #else branch.
#if defined(__GNUC__)
#define VECTOR_DISTANCE_UNROLL _Pragma("GCC unroll 16")
// Converts a float vector to the double vector type T, lane by lane.
#define VECTOR_DISTANCE_TO_DOUBLE(v, T) __builtin_convertvector(v, T)
#else
#define VECTOR_DISTANCE_UNROLL
// Only the scalar tier is built here and never instantiates the conversion,
// but MSVC parses template bodies, and __builtin_convertvector takes a type.
#define VECTOR_DISTANCE_TO_DOUBLE(v, T) static_cast<T>(v)
#endif

namespace {

enum class Metric { Euclidean, Cosine, Dot, Manhattan };

// Sums per metric: cosine needs ab, |a|² and |b|².
template <Metric M>
constexpr uint32_t kSums = M == Metric::Cosine ? 3 : 1;

// Number of lanes; see the file comment.
constexpr uint32_t kLanes = 16;

// Register types of a tier:
//   float_width  - number of float lanes in one register
//   double_width - number of double lanes in one register (half of
//                  float_width; 1 for scalar)
//   F  - float vector that fills one register
//   H  - float vector of double_width floats (half of F; the same as F for
//        scalar), converted into one D
//   D  - double vector that fills one register
//   FI, DI - integer vectors of the same size as F and D (used to clear sign
//            bits for Manhattan)
struct ScalarLanes {
  static constexpr uint32_t float_width = 1;
  static constexpr uint32_t double_width = 1;
  using F = float;
  using H = float;
  using D = double;
  using FI = int32_t;
  using DI = int64_t;
};

#if defined(VECTOR_DISTANCE_x86_64) || defined(VECTOR_DISTANCE_AARCH64)
template <uint32_t N>
struct VectorLanes {
  static constexpr uint32_t float_width = N;
  static constexpr uint32_t double_width = N / 2;
  typedef float F __attribute__((vector_size(N * sizeof(float))));
  typedef float H __attribute__((vector_size(N / 2 * sizeof(float))));
  typedef double D __attribute__((vector_size(N / 2 * sizeof(double))));
  typedef int32_t FI __attribute__((vector_size(N * sizeof(int32_t))));
  typedef int64_t DI __attribute__((vector_size(N / 2 * sizeof(int64_t))));
};
#endif

// Float32 products overflow to Inf near FLT_MAX.  Below FLT_MIN (2^-126) they
// are subnormal, and a subnormal product can be off by up to 2^-150 whatever
// its size (for equal elements below sqrt(FLT_MIN) ~ 1.1e-19; below
// 2^-75 ~ 2.6e-23 the square is 0).  So n products lose at most n * 2^-150
// to underflow.
//
// A float32 value already has a rounding error of up to value * 2^-24, so we
// accept the underflow error as long as it stays below that natural error of
// the sum:
//   n * 2^-150 <= sum * 2^-24
//   sum >= n * 2^-150 / 2^-24 = n * 2^-126 = n * FLT_MIN
// n <= max_dimensions, so the threshold below is a constant.  Normally-scaled
// data never falls back.
constexpr double kMinExactSum =
    static_cast<double>(vector_constants::max_dimensions + 1) * FLT_MIN;

// |x| < kMinExactSum, Inf or NaN, as one unsigned compare.  With the sign bit
// cleared, the bits of a double order like its magnitude, and Inf/NaN sort
// above DBL_MAX.  bits - kLo maps [kLo, kHi] onto [0, kHi - kLo]: Inf/NaN
// (bits > kHi) land above that range, and bits < kLo wrap around to near 2^64.
inline bool sum_out_of_range(double x) {
  constexpr uint64_t kLo = std::bit_cast<uint64_t>(kMinExactSum);
  constexpr uint64_t kHi = std::bit_cast<uint64_t>(DBL_MAX);
  const uint64_t bits = std::bit_cast<uint64_t>(x) & ~(uint64_t{1} << 63);
  return bits - kLo > kHi - kLo;
}

// True when the float32 lane sums may be inexact beyond float32 rounding.
template <Metric M>
inline bool needs_double(const double (&sum)[kSums<M>]) {
  if constexpr (M == Metric::Cosine) {
    // ab is legitimately 0 for orthogonal vectors; with both norms in range
    // its error is bounded relative to them.
    return !std::isfinite(sum[0]) || sum_out_of_range(sum[1]) ||
           sum_out_of_range(sum[2]);
  } else if constexpr (M == Metric::Manhattan) {
    // No products: subnormal differences and their sums are exact.
    return !std::isfinite(sum[0]);
  } else {  // Euclidean and Dot
    // The result is the sum itself, so a tiny sum may be underflow (or an
    // orthogonal dot product, which the double pass returns exactly).
    return sum_out_of_range(sum[0]);
  }
}

// acc[s][r] += term s of the elements x and y (s = 0 for one-sum metrics;
// ab, |a|^2, |b|^2 for cosine).  V is the register type the lanes are held
// in (L::F in the float32 pass, L::D in the double pass), IV the integer
// vector type of the same size, and R the number of V registers that hold
// the kLanes lanes, so r selects the register.
template <Metric M, class IV, class V, uint32_t R>
MY_ATTRIBUTE((always_inline))
inline void accumulate(V (&acc)[kSums<M>][R], uint32_t r, const V &x,
                       const V &y) {
  if constexpr (M == Metric::Euclidean) {
    const V d = x - y;
    acc[0][r] += d * d;
  } else if constexpr (M == Metric::Cosine) {
    acc[0][r] += x * y;
    acc[1][r] += x * x;
    acc[2][r] += y * y;
  } else if constexpr (M == Metric::Dot) {
    acc[0][r] += x * y;
  } else if constexpr (std::is_floating_point_v<V>) {  // Manhattan
    acc[0][r] += std::fabs(x - y);
  } else {
    // Manhattan: std::fabs takes no vectors, so clear each lane's sign bit.
    // Casting between equal-size vector types reinterprets the bits, so
    // (IV)d is d's bits as integers.  -V{} is -0.0 in every lane, whose only
    // set bit is the sign bit; ~(IV)(-V{}) is therefore every bit but the
    // sign, and the AND keeps |d| (NaN stays NaN).
    acc[0][r] += (V)((IV)(x - y) & ~(IV)(-V{}));
  }
}

// sum[s] = the kLanes lanes of acc[s], converted to double and added as the
// fixed tree from the file comment (lane l += lane l + w, w = 8, 4, 2, 1).
// It runs once per call, but a scalar version is measurably slower for
// short vectors, so it is done in registers:
//  1. The lanes are copied out of acc[s] half a float register (L::H) at a
//     time and widened into RD double registers d[0..RD-1], in lane order,
//     so d[r] holds lanes r*W .. r*W+W-1.  In the double pass the lanes
//     are already doubles and are copied as is.
//  2. Tree steps whose stride covers whole double registers are register
//     additions: d[r] += d[r + w] adds W lane pairs in one instruction.
//  3. The steps inside d[0] (stride < W) are done on a scalar copy.
// The pairs added are the same on every tier; only how many are added per
// instruction differs.
template <class L, class V, uint32_t S, uint32_t R>
MY_ATTRIBUTE((always_inline))
inline void reduce_lanes(const V (&acc)[S][R], double (&sum)[S]) {
  using D = typename L::D;
  constexpr uint32_t W = L::double_width;
  constexpr uint32_t RD = kLanes / W;
  for (uint32_t s = 0; s < S; s++) {
    D d[RD];  // the lanes in double, in order
    if constexpr (sizeof(V) * R == sizeof(d)) {  // already double
      memcpy(d, acc[s], sizeof(d));
    } else {
      VECTOR_DISTANCE_UNROLL
      for (uint32_t r = 0; r < RD; r++) {
        typename L::H h;
        memcpy(&h, (const char *)acc[s] + r * sizeof(h), sizeof(h));
        if constexpr (W == 1)
          d[r] = h;
        else
          d[r] = VECTOR_DISTANCE_TO_DOUBLE(h, D);
      }
    }
    VECTOR_DISTANCE_UNROLL
    for (uint32_t w = RD / 2; w > 0; w /= 2) {
      VECTOR_DISTANCE_UNROLL
      for (uint32_t r = 0; r < w; r++) d[r] += d[r + w];
    }
    double lane[W];
    memcpy(lane, &d[0], sizeof(lane));
    VECTOR_DISTANCE_UNROLL
    for (uint32_t w = W / 2; w > 0; w /= 2) {
      VECTOR_DISTANCE_UNROLL
      for (uint32_t l = 0; l < w; l++) lane[l] += lane[l + w];
    }
    sum[s] = lane[0];
  }
}

// Adds elements [i, dims) to sum in order, in double, and returns the result.
template <Metric M>
inline double finish(double (&sum)[kSums<M>], const char *a_raw,
                     const char *b_raw, uint32_t i, uint32_t dims) {
  for (; i < dims; i++) {
    float af, bf;
    memcpy(&af, a_raw + i * sizeof(float), sizeof(float));
    memcpy(&bf, b_raw + i * sizeof(float), sizeof(float));
    const double x = af, y = bf;
    if constexpr (M == Metric::Euclidean) {
      const double d = x - y;
      sum[0] += d * d;
    } else if constexpr (M == Metric::Cosine) {
      sum[0] += x * y;
      sum[1] += x * x;
      sum[2] += y * y;
    } else if constexpr (M == Metric::Dot) {
      sum[0] += x * y;
    } else {
      sum[0] += std::fabs(x - y);
    }
  }

  if constexpr (M == Metric::Cosine) {
    const double denom = std::sqrt(sum[1] * sum[2]);
    // +Inf sentinel: zero-denom means undefined cosine (zero-vector input).
    if (denom == 0.0) return std::numeric_limits<double>::infinity();
    // Rounding can put 1 - cos just outside [0, 2]; NaN stays NaN.
    return std::clamp(1.0 - sum[0] / denom, 0.0, 2.0);
  } else {
    return sum[0];
  }
}

// Exact fallback: the lanes in double.  always_inline (also below): the
// vector code must be generated in the caller, which has the tier's target
// attribute.
template <class L, Metric M>
MY_ATTRIBUTE((always_inline))
inline double double_lanes_kernel(const char *a_raw, const char *b_raw,
                                  uint32_t dims) {
  using D = typename L::D;
  constexpr uint32_t W = L::double_width;
  constexpr uint32_t R = kLanes / W;
  D acc[kSums<M>][R] = {};
  uint32_t i = 0;
  for (; i + kLanes <= dims; i += kLanes) {
    VECTOR_DISTANCE_UNROLL
    for (uint32_t r = 0; r < R; r++) {
      typename L::H af, bf;
      memcpy(&af, a_raw + (i + r * W) * sizeof(float), sizeof(af));
      memcpy(&bf, b_raw + (i + r * W) * sizeof(float), sizeof(bf));
      D x, y;
      if constexpr (W == 1) {
        x = af;
        y = bf;
      } else {
        x = VECTOR_DISTANCE_TO_DOUBLE(af, D);
        y = VECTOR_DISTANCE_TO_DOUBLE(bf, D);
      }
      accumulate<M, typename L::DI>(acc, r, x, y);
    }
  }
  double sum[kSums<M>] = {};
  // Without a block the lanes are all +0.0: skip reducing them.
  if (i > 0) reduce_lanes<L>(acc, sum);
  return finish<M>(sum, a_raw, b_raw, i, dims);
}

// Fast path: the lanes in float32, see the file comment.
template <class L, Metric M>
MY_ATTRIBUTE((always_inline))
inline double lanes_kernel(const char *a_raw, const char *b_raw,
                           uint32_t dims) {
  using F = typename L::F;
  constexpr uint32_t W = L::float_width;
  constexpr uint32_t R = kLanes / W;
  F acc[kSums<M>][R] = {};
  uint32_t i = 0;
  for (; i + kLanes <= dims; i += kLanes) {
    VECTOR_DISTANCE_UNROLL
    for (uint32_t r = 0; r < R; r++) {
      F x, y;
      memcpy(&x, a_raw + (i + r * W) * sizeof(float), sizeof(x));
      memcpy(&y, b_raw + (i + r * W) * sizeof(float), sizeof(y));
      accumulate<M, typename L::FI>(acc, r, x, y);
    }
  }
  double sum[kSums<M>] = {};
  if (i > 0) {
    reduce_lanes<L>(acc, sum);
    if (needs_double<M>(sum)) [[unlikely]]
      return double_lanes_kernel<L, M>(a_raw, b_raw, dims);
  }
  return finish<M>(sum, a_raw, b_raw, i, dims);
}

}  // namespace

// Defines euclidean_<tier>, cosine_<tier>, dot_product_<tier> and
// manhattan_<tier> for one tier.
#define VECTOR_DISTANCE_KERNELS(tier, attributes, lanes)                    \
  attributes static double euclidean_##tier(const char *a, const char *b,   \
                                            uint32_t dims) {                \
    return lanes_kernel<lanes, Metric::Euclidean>(a, b, dims);              \
  }                                                                         \
  attributes static double cosine_##tier(const char *a, const char *b,      \
                                         uint32_t dims) {                   \
    return lanes_kernel<lanes, Metric::Cosine>(a, b, dims);                 \
  }                                                                         \
  attributes static double dot_product_##tier(const char *a, const char *b, \
                                              uint32_t dims) {              \
    return lanes_kernel<lanes, Metric::Dot>(a, b, dims);                    \
  }                                                                         \
  attributes static double manhattan_##tier(const char *a, const char *b,   \
                                            uint32_t dims) {                \
    return lanes_kernel<lanes, Metric::Manhattan>(a, b, dims);              \
  }

VECTOR_DISTANCE_KERNELS(scalar, , ScalarLanes)

#ifdef VECTOR_DISTANCE_x86_64
VECTOR_DISTANCE_KERNELS(sse, MY_ATTRIBUTE((target("sse4.2"))), VectorLanes<4>)
VECTOR_DISTANCE_KERNELS(avx2, MY_ATTRIBUTE((target("avx2"))), VectorLanes<8>)
VECTOR_DISTANCE_KERNELS(avx512, MY_ATTRIBUTE((target("avx512f"))),
                        VectorLanes<16>)

static bool cpu_has_sse42() { return __builtin_cpu_supports("sse4.2"); }

static bool cpu_has_avx2() { return __builtin_cpu_supports("avx2"); }

static bool cpu_has_avx512f() { return __builtin_cpu_supports("avx512f"); }
#endif

#ifdef VECTOR_DISTANCE_AARCH64
// Advanced SIMD is baseline ARMv8-A.
VECTOR_DISTANCE_KERNELS(neon, , VectorLanes<4>)
#endif

// ---------------------------------------------------------------------------
// Function pointers — initialized to scalar; init_vector_distance_functions()
// may promote.
// ---------------------------------------------------------------------------

using vector_distance_fn_t = double (*)(const char *, const char *, uint32_t);

static vector_distance_fn_t g_euclidean = euclidean_scalar;
static vector_distance_fn_t g_cosine = cosine_scalar;
static vector_distance_fn_t g_dot_product = dot_product_scalar;
static vector_distance_fn_t g_manhattan = manhattan_scalar;

static VectorDistanceTier g_tier = VectorDistanceTier::Scalar;

#define VECTOR_DISTANCE_APPLY(tier)     \
  do {                                  \
    g_euclidean = euclidean_##tier;     \
    g_cosine = cosine_##tier;           \
    g_dot_product = dot_product_##tier; \
    g_manhattan = manhattan_##tier;     \
  } while (0)

bool vector_distance_tier_available(VectorDistanceTier tier) {
  switch (tier) {
    case VectorDistanceTier::Scalar:
      return true;
#ifdef VECTOR_DISTANCE_x86_64
    case VectorDistanceTier::Sse42:
      return cpu_has_sse42();
    case VectorDistanceTier::Avx2:
      return cpu_has_avx2();
    case VectorDistanceTier::Avx512f:
      return cpu_has_avx512f();
#endif
#ifdef VECTOR_DISTANCE_AARCH64
    case VectorDistanceTier::Neon:
      return true;  // mandatory on ARMv8
#endif
    default:
      return false;
  }
}

void init_vector_distance_functions_tier(VectorDistanceTier tier) {
  // Unsupported tier on this build; callers must check
  // vector_distance_tier_available() first.
  assert(vector_distance_tier_available(tier));
  switch (tier) {
#ifdef VECTOR_DISTANCE_x86_64
    case VectorDistanceTier::Sse42:
      VECTOR_DISTANCE_APPLY(sse);
      break;
    case VectorDistanceTier::Avx2:
      VECTOR_DISTANCE_APPLY(avx2);
      break;
    case VectorDistanceTier::Avx512f:
      VECTOR_DISTANCE_APPLY(avx512);
      break;
#endif
#ifdef VECTOR_DISTANCE_AARCH64
    case VectorDistanceTier::Neon:
      VECTOR_DISTANCE_APPLY(neon);
      break;
#endif
    default:
      VECTOR_DISTANCE_APPLY(scalar);
      tier = VectorDistanceTier::Scalar;
      break;
  }
  g_tier = tier;
}

void init_vector_distance_functions() {
  VectorDistanceTier tier = VectorDistanceTier::Scalar;
  for (VectorDistanceTier t :
       {VectorDistanceTier::Avx512f, VectorDistanceTier::Avx2,
        VectorDistanceTier::Sse42, VectorDistanceTier::Neon}) {
    if (vector_distance_tier_available(t)) {
      tier = t;
      break;
    }
  }
  init_vector_distance_functions_tier(tier);
}

VectorDistanceTier vector_distance_tier() { return g_tier; }

const char *vector_distance_tier_label(VectorDistanceTier tier) {
  switch (tier) {
    case VectorDistanceTier::Scalar:
      return "software scalar";
    case VectorDistanceTier::Sse42:
      return "SSE4.2";
    case VectorDistanceTier::Avx2:
      return "AVX2";
    case VectorDistanceTier::Avx512f:
      return "AVX-512F";
    case VectorDistanceTier::Neon:
      return "NEON";
  }
  return "unknown";
}

size_t vector_distance_dispatch_description(char *buf, size_t buf_len) {
  if (buf == nullptr || buf_len == 0) return 0;

  int n;
  if (g_tier == VectorDistanceTier::Scalar) {
    n = snprintf(buf, buf_len, " software scalar.");
  } else {
    n = snprintf(buf, buf_len, " hardware accelerated %s.",
                 vector_distance_tier_label(g_tier));
  }
  if (n < 0) {
    buf[0] = '\0';
    return 0;
  }
  if (static_cast<size_t>(n) >= buf_len) return buf_len - 1;
  return static_cast<size_t>(n);
}

// Public wrappers

double vector_distance_euclidean_squared(const char *a, const char *b,
                                         uint32_t dims) {
  return g_euclidean(a, b, dims);
}

double vector_distance_cosine(const char *a, const char *b, uint32_t dims) {
  return g_cosine(a, b, dims);
}

double vector_distance_dot(const char *a, const char *b, uint32_t dims) {
  return g_dot_product(a, b, dims);
}

double vector_distance_manhattan(const char *a, const char *b, uint32_t dims) {
  return g_manhattan(a, b, dims);
}
