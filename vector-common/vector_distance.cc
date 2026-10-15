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
  different number of registers: two 8-double registers for AVX-512F, four
  4-double registers for AVX2, eight 2-double registers for SSE4.2 and NEON,
  and 16 single doubles for scalar.  Every kernel is one instantiation of the
  same templates with the tier's register width:

  - Each float32 element is converted to double, and element i goes into
    lane i % kLanes, in index order, so after the main loop lane l holds the
    partial sum of elements l, l + 16, l + 32, ...  The SIMD kernels keep the
    lanes in registers in index order.
  - At the end the 16 lane sums are added as a fixed binary tree:
    lane l += lane l + 8 for l < 8, then lane l += lane l + 4 for l < 4, then
    + 2, then + 1; lane 0 holds the total.  Every tier adds the same pairs in
    the same order, whatever its register width.
  - The last dims % kLanes elements are added in order.

  IEEE 754 rounds each of these operations the same way everywhere, so the
  result depends only on this layout, not on the register width.  The file
  must be compiled with -ffp-contract=off: a fused multiply-add rounds once
  instead of twice and would make FMA-capable tiers differ.

  Precision
  ---------
  The lanes are double, not float32.  Products of two float32 values are
  exact in double, and neither the products nor their sums can overflow or
  underflow: a product or squared difference is below 4.7e77 and, unless it
  is 0, at least 2^-298.  What remains is the double rounding of the
  differences and the additions, so two candidates whose distances differ by
  much less than a float32 rounding error are still ordered correctly.
  Float32 lanes would hold twice as many elements per register, but their
  rounding (about 1e-7 relative to the sums) can reverse such candidates, and
  their products overflow or underflow at the ends of the float32 range.

  All kernels accept const char * and any byte alignment: loads use memcpy,
  which compiles to an unaligned load.

  Tiers
  -----
  +----------+---------------------------------+---------------------------+
  | Tier     | Platform                        | Doubles per register      |
  +----------+---------------------------------+---------------------------+
  | Scalar   | any (the only one on _WIN32)    | 1                         |
  | Sse42    | x86_64 with SSE4.2              | 2                         |
  | Avx2     | x86_64 with AVX2                | 4                         |
  | Avx512f  | x86_64 with AVX-512F            | 8                         |
  | Neon     | aarch64 (baseline)              | 2                         |
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
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>

#include "mysql/attribute.h"  // MY_ATTRIBUTE

// No fused multiply-add, see the file comment. GCC and Clang get
// -ffp-contract=off from the build. Clang takes the pragma as well, which
// also covers clang-cl, where the build does not add that option. MSVC before
// Visual Studio 2022 may contract under /fp:precise.
#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(_MSC_VER)
#pragma fp_contract(off)
#endif

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

// Register types of a tier with W double lanes per register:
//   H  - W floats: the input elements loaded for one register
//   D  - W doubles: one register of lanes
//   DI - W 64-bit integers, the size of D (used to clear sign bits for
//        Manhattan)
struct ScalarLanes {
  static constexpr uint32_t width = 1;
  using H = float;
  using D = double;
  using DI = int64_t;
};

#if defined(VECTOR_DISTANCE_x86_64) || defined(VECTOR_DISTANCE_AARCH64)
template <uint32_t W>
struct VectorLanes {
  static constexpr uint32_t width = W;
  typedef float H __attribute__((vector_size(W * sizeof(float))));
  typedef double D __attribute__((vector_size(W * sizeof(double))));
  typedef int64_t DI __attribute__((vector_size(W * sizeof(int64_t))));
};
#endif

// acc[s] += term s of the elements x and y: (x - y)² for Euclidean; xy, x²
// and y² for cosine; xy for dot; |x - y| for Manhattan.  V is L::D for one
// register of lanes, or double for one element of the tail.
template <class L, Metric M, class V>
MY_ATTRIBUTE((always_inline))
inline void accumulate(V (&acc)[kSums<M>], const V &x, const V &y) {
  if constexpr (M == Metric::Euclidean) {
    const V d = x - y;
    acc[0] += d * d;
  } else if constexpr (M == Metric::Cosine) {
    acc[0] += x * y;
    acc[1] += x * x;
    acc[2] += y * y;
  } else if constexpr (M == Metric::Dot) {
    acc[0] += x * y;
  } else if constexpr (std::is_floating_point_v<V>) {  // Manhattan
    acc[0] += std::fabs(x - y);
  } else {
    // Manhattan: std::fabs takes no vectors, so clear each lane's sign bit.
    // Casting between equal-size vector types reinterprets the bits, so
    // (DI)d is d's bits as integers.  -V{} is -0.0 in every lane, whose only
    // set bit is the sign bit; ~(DI)(-V{}) is therefore every bit but the
    // sign, and the AND keeps |d| (NaN stays NaN).
    using DI = typename L::DI;
    acc[0] += (V)((DI)(x - y) & ~(DI)(-V{}));
  }
}

// sum[s] = the kLanes lanes of sum s, added as the fixed tree from the file
// comment (lane l += lane l + w, w = 8, 4, 2, 1).  acc[r][s] holds lanes
// r*W .. r*W+W-1 of sum s.  It runs once per call, but a scalar version is
// measurably slower for short vectors with GCC, so it is done in registers:
//  1. Tree steps whose stride covers whole registers are register additions:
//     d[r] += d[r + w] adds W lane pairs in one instruction.
//  2. The steps inside d[0] (stride < W) are done on a scalar copy.
// The pairs added are the same on every tier; only how many are added per
// instruction differs.
template <class L, Metric M, uint32_t R>
MY_ATTRIBUTE((always_inline))
inline void reduce_lanes(const typename L::D (&acc)[R][kSums<M>],
                         double (&sum)[kSums<M>]) {
  constexpr uint32_t W = L::width;
  static_assert(R * W == kLanes);
  for (uint32_t s = 0; s < kSums<M>; s++) {
    typename L::D d[R];
    for (uint32_t r = 0; r < R; r++) d[r] = acc[r][s];
    VECTOR_DISTANCE_UNROLL
    for (uint32_t w = R / 2; w > 0; w /= 2) {
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

// The kernel of every tier, see the file comment.  always_inline: the vector
// code must be generated in the caller, which has the tier's target
// attribute.
template <class L, Metric M>
MY_ATTRIBUTE((always_inline))
inline double lanes_kernel(const char *a_raw, const char *b_raw,
                           uint32_t dims) {
  using D = typename L::D;
  constexpr uint32_t W = L::width;
  constexpr uint32_t R = kLanes / W;
  D acc[R][kSums<M>] = {};
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
      accumulate<L, M>(acc[r], x, y);
    }
  }
  double sum[kSums<M>] = {};
  // Without a block the lanes are all +0.0: skip reducing them.
  if (i > 0) reduce_lanes<L, M>(acc, sum);

  // The last dims % kLanes elements, in order.
  for (; i < dims; i++) {
    float af, bf;
    memcpy(&af, a_raw + i * sizeof(float), sizeof(float));
    memcpy(&bf, b_raw + i * sizeof(float), sizeof(float));
    accumulate<ScalarLanes, M>(sum, double{af}, double{bf});
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
VECTOR_DISTANCE_KERNELS(sse, MY_ATTRIBUTE((target("sse4.2"))), VectorLanes<2>)
VECTOR_DISTANCE_KERNELS(avx2, MY_ATTRIBUTE((target("avx2"))), VectorLanes<4>)
VECTOR_DISTANCE_KERNELS(avx512, MY_ATTRIBUTE((target("avx512f"))),
                        VectorLanes<8>)

static bool cpu_has_sse42() { return __builtin_cpu_supports("sse4.2"); }

static bool cpu_has_avx2() { return __builtin_cpu_supports("avx2"); }

static bool cpu_has_avx512f() { return __builtin_cpu_supports("avx512f"); }
#endif

#ifdef VECTOR_DISTANCE_AARCH64
// Advanced SIMD is baseline ARMv8-A.
VECTOR_DISTANCE_KERNELS(neon, , VectorLanes<2>)
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

  const int n = snprintf(
      buf, buf_len, "%s%s.",
      g_tier == VectorDistanceTier::Scalar ? "" : "hardware accelerated ",
      vector_distance_tier_label(g_tier));
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
