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

#include <gtest/gtest.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "my_pointer_arithmetic.h"
#include "vector-common/vector_distance.h"

namespace vector_distance_unittest {

// ---------------------------------------------------------------------------
// Reference implementations — always scalar, double precision.
// Used to verify SIMD paths; independent of vector_distance.cc internals.
// ---------------------------------------------------------------------------

static double ref_euclidean_squared(const float *a, const float *b,
                                    uint32_t n) {
  double sum = 0.0;
  for (uint32_t i = 0; i < n; i++) {
    const double d = (double)a[i] - b[i];
    sum += d * d;
  }
  return sum;
}

static double ref_euclidean(const float *a, const float *b, uint32_t n) {
  return std::sqrt(ref_euclidean_squared(a, b, n));
}

// Smallest positive offset from base that is not alignof(float)-aligned.
// A fixed +1 byte offset is not reliable: when the buffer sits at address
// % alignof(float) == alignof(float) - 1 (seen on Apple Silicon stacks),
// base+1 is still float-aligned.
static size_t misaligned_float_offset(uintptr_t base) {
  for (size_t offset = 1; offset < alignof(float); ++offset) {
    if ((base + offset) % alignof(float) != 0) return offset;
  }
  assert(false);
  return 1;
}

// Mirrors Item_func_vector_distance EUCLIDEAN branch.
static double euclidean_l2(const char *a, const char *b, uint32_t dims) {
  return std::sqrt(vector_distance_euclidean_squared(a, b, dims));
}

static double ref_dot_product(const float *a, const float *b, uint32_t n) {
  double ab = 0.0;
  for (uint32_t i = 0; i < n; i++) ab += (double)a[i] * b[i];
  return ab;
}

static double ref_cosine(const float *a, const float *b, uint32_t n) {
  double ab = 0.0, na = 0.0, nb = 0.0;
  for (uint32_t i = 0; i < n; i++) {
    ab += (double)a[i] * b[i];
    na += (double)a[i] * a[i];
    nb += (double)b[i] * b[i];
  }
  const double denom = std::sqrt(na * nb);
  if (denom == 0.0) return std::numeric_limits<double>::infinity();
  return 1.0 - ab / denom;
}

static double ref_manhattan(const float *a, const float *b, uint32_t n) {
  double result = 0.0;
  for (uint32_t i = 0; i < n; i++) result += std::fabs((double)a[i] - b[i]);
  return result;
}

// ---------------------------------------------------------------------------
// Fixture — initialises dispatch pointers once per test suite
// ---------------------------------------------------------------------------

class VectorDistanceTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { init_vector_distance_functions(); }
};

// ---------------------------------------------------------------------------
// Known-value correctness
// ---------------------------------------------------------------------------

TEST_F(VectorDistanceTest, EuclideanSquaredKnownValues) {
  // [0,0] → [3,4] = 25.0 (squared 3-4-5 right triangle)
  alignas(32) float a[] = {0.0f, 0.0f};
  alignas(32) float b[] = {3.0f, 4.0f};
  EXPECT_NEAR(
      vector_distance_euclidean_squared((const char *)a, (const char *)b, 2),
      25.0, 1e-6);

  // Identical vectors → distance 0
  alignas(32) float c[] = {1.0f, 2.0f, 3.0f};
  EXPECT_NEAR(
      vector_distance_euclidean_squared((const char *)c, (const char *)c, 3),
      0.0, 1e-9);
}

TEST_F(VectorDistanceTest, EuclideanKnownValues) {
  // [0,0] → [3,4] = 5.0 (3-4-5 right triangle)
  alignas(32) float a[] = {0.0f, 0.0f};
  alignas(32) float b[] = {3.0f, 4.0f};
  EXPECT_NEAR(euclidean_l2((const char *)a, (const char *)b, 2), 5.0, 1e-6);

  // Identical vectors → distance 0
  alignas(32) float c[] = {1.0f, 2.0f, 3.0f};
  EXPECT_NEAR(euclidean_l2((const char *)c, (const char *)c, 3), 0.0, 1e-9);
}

TEST_F(VectorDistanceTest, CosineKnownValues) {
  // Identical unit vectors → distance 0
  alignas(32) float same[] = {1.0f, 0.0f, 0.0f};
  EXPECT_NEAR(vector_distance_cosine((const char *)same, (const char *)same, 3),
              0.0, 1e-6);

  // Orthogonal vectors → distance 1
  alignas(32) float x[] = {1.0f, 0.0f};
  alignas(32) float y[] = {0.0f, 1.0f};
  EXPECT_NEAR(vector_distance_cosine((const char *)x, (const char *)y, 2), 1.0,
              1e-6);

  // Anti-parallel → distance 2
  alignas(32) float pos[] = {1.0f, 1.0f};
  alignas(32) float neg[] = {-1.0f, -1.0f};
  EXPECT_NEAR(vector_distance_cosine((const char *)pos, (const char *)neg, 2),
              2.0, 1e-6);
}

TEST_F(VectorDistanceTest, DotProductKnownValues) {
  // Orthogonal vectors → dot product 0
  alignas(32) float x[] = {1.0f, 0.0f};
  alignas(32) float y[] = {0.0f, 1.0f};
  EXPECT_NEAR(vector_distance_dot((const char *)x, (const char *)y, 2), 0.0,
              1e-9);

  // Identical unit vector → dot product 1
  alignas(32) float u[] = {1.0f, 0.0f, 0.0f};
  EXPECT_NEAR(vector_distance_dot((const char *)u, (const char *)u, 3), 1.0,
              1e-9);

  // Known values: [1,2,3]·[4,5,6] = 4+10+18 = 32
  alignas(32) float a[] = {1.0f, 2.0f, 3.0f};
  alignas(32) float b[] = {4.0f, 5.0f, 6.0f};
  EXPECT_NEAR(vector_distance_dot((const char *)a, (const char *)b, 3), 32.0,
              1e-6);
}

TEST_F(VectorDistanceTest, ManhattanKnownValues) {
  // Identical vectors → distance 0
  alignas(32) float same[] = {1.0f, 2.0f, 3.0f};
  EXPECT_NEAR(
      vector_distance_manhattan((const char *)same, (const char *)same, 3), 0.0,
      1e-9);

  // [0,0] → [3,4] = |3| + |4| = 7 (compare: Euclidean gives 5)
  alignas(32) float a[] = {0.0f, 0.0f};
  alignas(32) float b[] = {3.0f, 4.0f};
  EXPECT_NEAR(vector_distance_manhattan((const char *)a, (const char *)b, 2),
              7.0, 1e-6);

  // [1,7,3,16,5] → [1,2,3,4,5] = 0+5+0+12+0 = 17
  alignas(32) float c[] = {1.0f, 7.0f, 3.0f, 16.0f, 5.0f};
  alignas(32) float d[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
  EXPECT_NEAR(vector_distance_manhattan((const char *)c, (const char *)d, 5),
              17.0, 1e-6);
}

// ---------------------------------------------------------------------------
// Zero-vector guard: cosine must return +Inf sentinel (val_real maps to NULL)
// ---------------------------------------------------------------------------

TEST_F(VectorDistanceTest, CosineZeroVectorReturnsInf) {
  alignas(32) float z[] = {0.0f, 0.0f};
  alignas(32) float a[] = {1.0f, 2.0f};
  EXPECT_TRUE(
      std::isinf(vector_distance_cosine((const char *)z, (const char *)a, 2)));
  EXPECT_TRUE(
      std::isinf(vector_distance_cosine((const char *)a, (const char *)z, 2)));
  EXPECT_TRUE(
      std::isinf(vector_distance_cosine((const char *)z, (const char *)z, 2)));
}

// ---------------------------------------------------------------------------
// Unaligned path: misaligned buffer must give the same result as aligned
// ---------------------------------------------------------------------------

static void CheckUnalignedMatchesAligned(uint32_t dims) {
  SCOPED_TRACE(::testing::Message() << "dims=" << dims);

  std::mt19937 rng(dims);
  std::uniform_real_distribution<float> dist(-10.0f, 10.0f);
  std::vector<float> fa(dims), fb(dims);
  for (auto &x : fa) x = dist(rng);
  for (auto &x : fb) x = dist(rng);

  std::vector<char> buf_a(dims * sizeof(float) + alignof(float));
  std::vector<char> buf_b(dims * sizeof(float) + alignof(float));
  const size_t off_a =
      misaligned_float_offset(reinterpret_cast<uintptr_t>(buf_a.data()));
  const size_t off_b =
      misaligned_float_offset(reinterpret_cast<uintptr_t>(buf_b.data()));
  std::memcpy(buf_a.data() + off_a, fa.data(), dims * sizeof(float));
  std::memcpy(buf_b.data() + off_b, fb.data(), dims * sizeof(float));

  const char *ma = buf_a.data() + off_a;
  const char *mb = buf_b.data() + off_b;
  ASSERT_FALSE(is_aligned_to(ma, alignof(float)));
  ASSERT_FALSE(is_aligned_to(mb, alignof(float)));

  const char *aa = (const char *)fa.data();
  const char *ab = (const char *)fb.data();

  EXPECT_DOUBLE_EQ(vector_distance_euclidean_squared(aa, ab, dims),
                   vector_distance_euclidean_squared(ma, mb, dims));
  EXPECT_DOUBLE_EQ(euclidean_l2(aa, ab, dims), euclidean_l2(ma, mb, dims));
  EXPECT_DOUBLE_EQ(vector_distance_cosine(aa, ab, dims),
                   vector_distance_cosine(ma, mb, dims));
  EXPECT_DOUBLE_EQ(vector_distance_dot(aa, ab, dims),
                   vector_distance_dot(ma, mb, dims));
  EXPECT_DOUBLE_EQ(vector_distance_manhattan(aa, ab, dims),
                   vector_distance_manhattan(ma, mb, dims));
}

TEST_F(VectorDistanceTest, UnalignedMatchesAligned) {
  // dims=8 is below the 16-lane block, so only the tail loop runs.  dims=35
  // runs two blocks and a 3-element tail.
  CheckUnalignedMatchesAligned(8);
  CheckUnalignedMatchesAligned(35);
}

// ---------------------------------------------------------------------------
// SIMD parity: aligned result matches double-precision scalar reference.
// On CPUs without SIMD both sides run the same scalar code, so the test
// degenerates into an identity check — still a useful correctness signal.
// ---------------------------------------------------------------------------

TEST_F(VectorDistanceTest, EuclideanSquaredParityWithReference) {
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(-10.0f, 10.0f);

  for (uint32_t dims : {4u, 8u, 16u, 32u, 128u, 512u}) {
    // Use heap vectors; malloc guarantees at least 16-byte alignment,
    // which satisfies our alignof(float)=4 dispatch gate.
    std::vector<float> a(dims), b(dims);
    for (auto &x : a) x = dist(rng);
    for (auto &x : b) x = dist(rng);

    const double got = vector_distance_euclidean_squared(
        (const char *)a.data(), (const char *)b.data(), dims);
    const double ref = ref_euclidean_squared(a.data(), b.data(), dims);
    // Allow 0.01% relative tolerance for float32 lane accumulation.
    EXPECT_NEAR(got, ref, ref * 1e-4 + 1e-9) << "dims=" << dims;
  }
}

TEST_F(VectorDistanceTest, EuclideanParityWithReference) {
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(-10.0f, 10.0f);

  for (uint32_t dims : {4u, 8u, 16u, 32u, 128u, 512u}) {
    std::vector<float> a(dims), b(dims);
    for (auto &x : a) x = dist(rng);
    for (auto &x : b) x = dist(rng);

    const double got =
        euclidean_l2((const char *)a.data(), (const char *)b.data(), dims);
    const double ref = ref_euclidean(a.data(), b.data(), dims);
    EXPECT_NEAR(got, ref, ref * 1e-4 + 1e-9) << "dims=" << dims;
  }
}

TEST_F(VectorDistanceTest, CosineParityWithReference) {
  std::mt19937 rng(123);
  std::uniform_real_distribution<float> dist(-5.0f, 5.0f);

  for (uint32_t dims : {4u, 8u, 16u, 32u, 128u, 512u}) {
    std::vector<float> a(dims), b(dims);
    for (auto &x : a) x = dist(rng);
    for (auto &x : b) x = dist(rng);

    const double got = vector_distance_cosine((const char *)a.data(),
                                              (const char *)b.data(), dims);
    const double ref = ref_cosine(a.data(), b.data(), dims);
    EXPECT_NEAR(got, ref, 1e-4) << "dims=" << dims;
  }
}

TEST_F(VectorDistanceTest, DotProductParityWithReference) {
  std::mt19937 rng(77);
  std::uniform_real_distribution<float> dist(-10.0f, 10.0f);

  for (uint32_t dims : {4u, 8u, 16u, 32u, 128u, 512u}) {
    std::vector<float> a(dims), b(dims);
    for (auto &x : a) x = dist(rng);
    for (auto &x : b) x = dist(rng);

    const double got = vector_distance_dot((const char *)a.data(),
                                           (const char *)b.data(), dims);
    const double ref = ref_dot_product(a.data(), b.data(), dims);
    EXPECT_NEAR(got, ref, std::abs(ref) * 1e-4 + 1e-9) << "dims=" << dims;
  }
}

TEST_F(VectorDistanceTest, ManhattanParityWithReference) {
  std::mt19937 rng(55);
  std::uniform_real_distribution<float> dist(-10.0f, 10.0f);

  for (uint32_t dims : {4u, 8u, 16u, 32u, 128u, 512u}) {
    std::vector<float> a(dims), b(dims);
    for (auto &x : a) x = dist(rng);
    for (auto &x : b) x = dist(rng);

    const double got = vector_distance_manhattan((const char *)a.data(),
                                                 (const char *)b.data(), dims);
    const double ref = ref_manhattan(a.data(), b.data(), dims);
    EXPECT_NEAR(got, ref, ref * 1e-4 + 1e-9) << "dims=" << dims;
  }
}

// ---------------------------------------------------------------------------
// Per-tier tests
//
// A separate parameterized fixture calls init_vector_distance_functions_tier()
// for each registered tier, skipping tiers that are unavailable on this CPU or
// build. This ensures every SIMD kernel is tested independently — including
// inferior tiers on CPUs that support a higher one.
// ---------------------------------------------------------------------------

static const char *tier_name(VectorDistanceTier tier) {
  switch (tier) {
    case VectorDistanceTier::Scalar:
      return "Scalar";
    case VectorDistanceTier::Sse42:
      return "Sse42";
    case VectorDistanceTier::Avx2:
      return "Avx2";
    case VectorDistanceTier::Avx512f:
      return "Avx512f";
    case VectorDistanceTier::Neon:
      return "Neon";
  }
  return "Unknown";
}

class VectorDistanceTierParityTest
    : public ::testing::TestWithParam<VectorDistanceTier> {
 protected:
  void SetUp() override {
    const VectorDistanceTier t = GetParam();
    if (!vector_distance_tier_available(t))
      GTEST_SKIP() << tier_name(t) << " not available on this CPU/build";
    init_vector_distance_functions_tier(t);
    EXPECT_EQ(vector_distance_tier(), t);
  }
  void TearDown() override { init_vector_distance_functions(); }
};

using metric_fn = double (*)(const char *, const char *, uint32_t);
using ref_fn = double (*)(const float *, const float *, uint32_t);

struct Metric {
  const char *name;
  metric_fn fn;
  ref_fn ref;
};

static const Metric kMetrics[] = {
    {"euclidean_squared", vector_distance_euclidean_squared,
     ref_euclidean_squared},
    {"cosine", vector_distance_cosine, ref_cosine},
    {"dot", vector_distance_dot, ref_dot_product},
    {"manhattan", vector_distance_manhattan, ref_manhattan},
};

static double call(const Metric &m, const std::vector<float> &a,
                   const std::vector<float> &b) {
  return m.fn((const char *)a.data(), (const char *)b.data(),
              (uint32_t)a.size());
}

TEST_P(VectorDistanceTierParityTest, ParityWithReferencePerTier) {
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist(-10.0f, 10.0f);

  // 16383 = 16·1023 + 15 runs the 16-lane blocks and the longest tail.
  for (uint32_t dims : {4u, 8u, 32u, 128u, 1024u, 16383u}) {
    std::vector<float> a(dims), b(dims);
    for (auto &x : a) x = dist(rng);
    for (auto &x : b) x = dist(rng);
    for (const Metric &m : kMetrics) {
      // The kernels sum in float32 lanes, the reference in double.  Every
      // term is at most 400 in magnitude (cosine: the result is <= 2).
      const double scale = m.fn == vector_distance_cosine ? 2.0 : 400.0 * dims;
      EXPECT_NEAR(call(m, a, b), m.ref(a.data(), b.data(), dims), scale * 1e-5)
          << "tier=" << tier_name(GetParam()) << " metric=" << m.name
          << " dims=" << dims;
    }
  }
}

TEST_P(VectorDistanceTierParityTest, BitIdenticalToScalarPerTier) {
  // Every tier uses the same lane layout and summation order, so it must
  // return exactly the scalar result: for lengths around the 16-lane block and
  // the register widths, and for magnitudes across the whole float32 range.
  std::mt19937 rng(11);
  std::uniform_real_distribution<float> mantissa(-1.0f, 1.0f);
  std::uniform_int_distribution<int> exponent(-149, 127);
  std::vector<std::pair<std::vector<float>, std::vector<float>>> inputs;
  for (uint32_t dims : {1u, 3u, 4u, 7u, 8u, 15u, 16u, 17u, 31u, 32u, 33u, 47u,
                        768u, 1536u, 16383u}) {
    for (int scaled = 0; scaled < 2; scaled++) {
      std::vector<float> a(dims), b(dims);
      for (uint32_t i = 0; i < dims; i++) {
        a[i] = mantissa(rng);
        b[i] = mantissa(rng);
        if (scaled) {
          a[i] = std::ldexp(a[i], exponent(rng));
          b[i] = std::ldexp(b[i], exponent(rng));
        }
      }
      inputs.emplace_back(a, b);
    }
  }
  inputs.emplace_back(std::vector<float>(16383, 1e-21f),
                      std::vector<float>(16383, 2e-21f));
  inputs.emplace_back(std::vector<float>(40, 2.5e38f),
                      std::vector<float>(40, -2.5e38f));

  std::vector<double> got;
  for (const auto &[a, b] : inputs)
    for (const Metric &m : kMetrics) got.push_back(call(m, a, b));
  init_vector_distance_functions_tier(VectorDistanceTier::Scalar);
  size_t k = 0;
  for (const auto &[a, b] : inputs)
    for (const Metric &m : kMetrics) {
      EXPECT_EQ(got[k++], call(m, a, b))
          << "tier=" << tier_name(GetParam()) << " metric=" << m.name
          << " dims=" << a.size();
    }
}

TEST_P(VectorDistanceTierParityTest, ExtremeValuesPerTier) {
  // Products, squares and differences of float32 values near FLT_MAX overflow
  // float32, so the kernels recompute them in double.  One extreme element, at
  // the start (in a 16-lane block) or at the end (in the tail), so every result
  // is exact.
  constexpr float big = 2.5e38f;
  for (uint32_t dims : {5u, 16u, 17u}) {
    for (uint32_t pos : {0u, dims - 1}) {
      std::vector<float> a(dims, 1.0f), b(dims, 0.0f), c(dims, 0.0f);
      a[pos] = big;
      b[pos] = -big;
      c[pos] = big;
      for (const Metric &m : kMetrics) {
        for (const auto *y : {&b, &c}) {
          const double got = call(m, a, *y);
          EXPECT_TRUE(std::isfinite(got))
              << "tier=" << tier_name(GetParam()) << " metric=" << m.name
              << " dims=" << dims << " pos=" << pos;
          EXPECT_NEAR(got, m.ref(a.data(), y->data(), dims),
                      std::fabs(got) * 1e-15 + 1e-15)
              << "tier=" << tier_name(GetParam()) << " metric=" << m.name
              << " dims=" << dims << " pos=" << pos;
        }
      }
      EXPECT_NEAR(vector_distance_cosine((const char *)a.data(),
                                         (const char *)a.data(), dims),
                  0.0, 1e-15)
          << "tier=" << tier_name(GetParam()) << " dims=" << dims;
    }
  }
}

TEST_P(VectorDistanceTierParityTest, TinyValuesPerTier) {
  // Products and squares of tiny float32 values are subnormal (below
  // sqrt(FLT_MIN) ~ 1.1e-19) or 0 (below 2^-75 ~ 2.6e-23) in float32, so the
  // kernels recompute them in double.  Non-zero vectors must keep a non-zero
  // norm (cosine not NULL) and distinct vectors a non-zero distance.
  for (float tiny :
       {1e-20f, 1e-23f, std::numeric_limits<float>::denorm_min()}) {
    for (uint32_t dims : {4u, 16u, 20u, 16383u}) {
      std::vector<float> a(dims, tiny), b(dims, -2 * tiny), z(dims, 0.0f);
      const char *pa = (const char *)a.data();
      const char *pb = (const char *)b.data();
      const char *pz = (const char *)z.data();
      EXPECT_NEAR(vector_distance_cosine(pa, pa, dims), 0.0, 1e-15)
          << "tier=" << tier_name(GetParam()) << " tiny=" << tiny
          << " dims=" << dims;
      EXPECT_NEAR(vector_distance_cosine(pa, pb, dims), 2.0, 1e-15)
          << "tier=" << tier_name(GetParam()) << " tiny=" << tiny
          << " dims=" << dims;
      EXPECT_TRUE(std::isinf(vector_distance_cosine(pz, pa, dims)))
          << "tier=" << tier_name(GetParam()) << " tiny=" << tiny
          << " dims=" << dims;
      const double t2 = (double)tiny * tiny;
      EXPECT_NEAR(vector_distance_euclidean_squared(pa, pz, dims), dims * t2,
                  dims * t2 * 1e-12)
          << "tier=" << tier_name(GetParam()) << " tiny=" << tiny
          << " dims=" << dims;
      EXPECT_NEAR(vector_distance_dot(pa, pb, dims), -2.0 * dims * t2,
                  dims * t2 * 1e-12)
          << "tier=" << tier_name(GetParam()) << " tiny=" << tiny
          << " dims=" << dims;
      // Manhattan has no products, so it stays in float32 lanes.
      EXPECT_NEAR(vector_distance_manhattan(pa, pb, dims), 3.0 * dims * tiny,
                  3.0 * dims * tiny * 1e-4)
          << "tier=" << tier_name(GetParam()) << " tiny=" << tiny
          << " dims=" << dims;
    }
  }
}

TEST_P(VectorDistanceTierParityTest, NonFiniteIdenticalVectorsPerTier) {
  // NaN or Inf compared with itself is not distance 0: every metric returns a
  // non-finite value, which SQL reports as ER_DATA_OUT_OF_RANGE.
  for (float bad : {std::numeric_limits<float>::quiet_NaN(),
                    std::numeric_limits<float>::infinity(),
                    -std::numeric_limits<float>::infinity()}) {
    for (uint32_t dims : {1u, 5u, 17u}) {
      for (uint32_t pos : {0u, dims - 1}) {
        std::vector<float> a(dims, 1.0f);
        a[pos] = bad;
        for (const Metric &m : kMetrics) {
          EXPECT_FALSE(std::isfinite(call(m, a, a)))
              << "tier=" << tier_name(GetParam()) << " metric=" << m.name
              << " bad=" << bad << " dims=" << dims << " pos=" << pos;
        }
      }
    }
  }
  // Byte-different zeros are still identical values.
  std::vector<float> z1(20, 0.0f), z2(20, 0.0f);
  z2[10] = -0.0f;
  EXPECT_EQ(vector_distance_euclidean_squared((const char *)z1.data(),
                                              (const char *)z2.data(), 20),
            0.0)
      << "tier=" << tier_name(GetParam());
}

TEST_P(VectorDistanceTierParityTest, CosineRangePerTier) {
  // Rounding can put 1 - cos just below 0 for parallel vectors or above 2 for
  // anti-parallel ones; the result is clamped so ACOS(1 - d) stays defined.
  std::mt19937 rng(3);
  std::uniform_real_distribution<float> dist(-10.0f, 10.0f);
  for (uint32_t dims : {3u, 16u, 100u, 1024u}) {
    for (int rep = 0; rep < 20; rep++) {
      std::vector<float> a(dims), b(dims), c(dims);
      for (uint32_t i = 0; i < dims; i++) {
        a[i] = dist(rng);
        b[i] = 3 * a[i];
        c[i] = -3 * a[i];
      }
      const double parallel = vector_distance_cosine(
          (const char *)a.data(), (const char *)b.data(), dims);
      const double anti = vector_distance_cosine((const char *)a.data(),
                                                 (const char *)c.data(), dims);
      // 3 * a[i] rounds in float32, so the vectors are only nearly parallel.
      EXPECT_GE(parallel, 0.0) << "tier=" << tier_name(GetParam());
      EXPECT_NEAR(parallel, 0.0, 1e-6) << "tier=" << tier_name(GetParam());
      EXPECT_LE(anti, 2.0) << "tier=" << tier_name(GetParam());
      EXPECT_NEAR(anti, 2.0, 1e-6) << "tier=" << tier_name(GetParam());
    }
  }
}

static std::string tier_param_name(
    const ::testing::TestParamInfo<VectorDistanceTier> &info) {
  return tier_name(info.param);
}

#if defined(__x86_64__) || defined(_M_X64)
INSTANTIATE_TEST_SUITE_P(AllTiers, VectorDistanceTierParityTest,
                         ::testing::Values(VectorDistanceTier::Scalar,
                                           VectorDistanceTier::Sse42,
                                           VectorDistanceTier::Avx2,
                                           VectorDistanceTier::Avx512f),
                         tier_param_name);
#elif defined(__aarch64__) || defined(_M_ARM64)
INSTANTIATE_TEST_SUITE_P(AllTiers, VectorDistanceTierParityTest,
                         ::testing::Values(VectorDistanceTier::Scalar,
                                           VectorDistanceTier::Neon),
                         tier_param_name);
#else
INSTANTIATE_TEST_SUITE_P(AllTiers, VectorDistanceTierParityTest,
                         ::testing::Values(VectorDistanceTier::Scalar),
                         tier_param_name);
#endif

// ---------------------------------------------------------------------------
// Dispatch description / tier reporting
// ---------------------------------------------------------------------------

class VectorDistanceDispatchTest : public ::testing::Test {
 protected:
  void TearDown() override { init_vector_distance_functions(); }
};

TEST_F(VectorDistanceDispatchTest, ProductionInitIsIdempotent) {
  init_vector_distance_functions();
  const auto tier = vector_distance_tier();
  init_vector_distance_functions();
  EXPECT_EQ(vector_distance_tier(), tier);
}

TEST_F(VectorDistanceDispatchTest, ProductionInitRestoredAfterScalarOverride) {
  init_vector_distance_functions();
  const VectorDistanceTier prod = vector_distance_tier();

  init_vector_distance_functions_tier(VectorDistanceTier::Scalar);
  EXPECT_EQ(vector_distance_tier(), VectorDistanceTier::Scalar);

  init_vector_distance_functions();
  EXPECT_EQ(vector_distance_tier(), prod);
}

TEST_F(VectorDistanceDispatchTest, DescriptionIsFragmentForLogMessage) {
  init_vector_distance_functions();
  char msg[256];
  const size_t len = vector_distance_dispatch_description(msg, sizeof(msg));
  EXPECT_GT(len, 0u);
  // The description is spliced into ER_VECTOR_DISTANCE_SIMD_DISPATCH's
  // "For VECTOR_DISTANCE using %s", so it is a sentence fragment: a leading
  // space, no repeated "DISTANCE()"/"VECTOR_DISTANCE" wording of its own.
  EXPECT_EQ(msg[0], ' ');
  EXPECT_EQ(std::string(msg).find("DISTANCE"), std::string::npos);
  EXPECT_NE(
      std::string(msg).find(vector_distance_tier_label(vector_distance_tier())),
      std::string::npos);
}

TEST_F(VectorDistanceDispatchTest, ForcedScalarTierUpdatesDescription) {
  init_vector_distance_functions_tier(VectorDistanceTier::Scalar);
  EXPECT_EQ(vector_distance_tier(), VectorDistanceTier::Scalar);

  char msg[256];
  vector_distance_dispatch_description(msg, sizeof(msg));
  EXPECT_NE(std::string(msg).find("software scalar"), std::string::npos);
}

}  // namespace vector_distance_unittest
