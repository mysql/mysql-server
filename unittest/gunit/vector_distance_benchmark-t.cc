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
  @file vector_distance_benchmark-t.cc

  Per-tier microbenchmarks for vector_distance_euclidean_squared(),
  vector_distance_cosine(), vector_distance_dot(), and
  vector_distance_manhattan().

  Euclidean (L2) benchmarks apply std::sqrt() on top of the squared kernel,
  mirroring the SQL EUCLIDEAN path in Item_func_vector_distance::val_real.
  EuclideanSquared benchmarks measure the kernel alone (SQL EUCLIDEAN_SQUARED).

  Every combination of (metric × tier × size) is registered as a separate
  Google Test case via the BENCHMARK() macro.  At the start of each case,
  vector_distance_tier_available() is checked and GTEST_SKIP() is called when
  the tier is not supported by the current CPU or build.  This means:

  - On a Scalar-only x86 host: only Scalar tests execute; Sse42/Avx2/Avx512f
    are skipped.
  - On an AVX-512 capable host: all four x86 tiers run, so inferior-tier
    throughput is also measured.

  Sizes benchmarked: 4, 8, 32, 128, 1024, 16383 (float32 elements).
  Metrics: Euclidean, EuclideanSquared, Cosine, DotProduct, Manhattan.

  Tiers registered per platform:
    x86_64  — Scalar, Sse42, Avx2, Avx512f
    aarch64 — Scalar, Neon
    other   — Scalar only
*/

#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "unittest/gunit/benchmark.h"
#include "vector-common/vector_distance.h"

namespace vector_distance_tier_bench {

// Volatile sink prevents the optimizer from discarding computed distances.
static volatile double bench_sink;

enum class Metric {
  Euclidean,
  EuclideanSquared,
  Cosine,
  DotProduct,
  Manhattan
};

static void fill_random(float *data, uint32_t n, uint32_t seed) {
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
  for (uint32_t i = 0; i < n; i++) data[i] = dist(rng);
}

// ---------------------------------------------------------------------------
// Generic benchmark body
// ---------------------------------------------------------------------------

template <VectorDistanceTier kTier, uint32_t kDims, Metric kMetric>
static void bench_impl(size_t num_iterations) {
#ifndef NDEBUG
  // benchmark.cc calls StartBenchmarkTiming() before invoking func(), which
  // conflicts with our inner StartBenchmarkTiming() and triggers the
  // assert(!timer_running) in debug builds. Timings are meaningless in debug
  // mode regardless; skip cleanly instead.
  GTEST_SKIP() << "Benchmarks skipped in debug builds "
                  "(build with -DWITH_DEBUG=OFF for meaningful results)";
#endif
  if (!vector_distance_tier_available(kTier)) {
    const char *names[] = {"Scalar", "Sse42", "Avx2", "Avx512f", "Neon"};
    const int idx = static_cast<int>(kTier);
    GTEST_SKIP() << names[idx] << " not available on this CPU/build";
  }

  init_vector_distance_functions_tier(kTier);

  std::vector<float> a(kDims), b(kDims);
  fill_random(a.data(), kDims, 1);
  fill_random(b.data(), kDims, 2);

  StartBenchmarkTiming();
  for (size_t i = 0; i < num_iterations; i++) {
    if constexpr (kMetric == Metric::Cosine)
      bench_sink = vector_distance_cosine((const char *)a.data(),
                                          (const char *)b.data(), kDims);
    else if constexpr (kMetric == Metric::DotProduct)
      bench_sink = vector_distance_dot((const char *)a.data(),
                                       (const char *)b.data(), kDims);
    else if constexpr (kMetric == Metric::Manhattan)
      bench_sink = vector_distance_manhattan((const char *)a.data(),
                                             (const char *)b.data(), kDims);
    else if constexpr (kMetric == Metric::Euclidean)
      bench_sink = std::sqrt(vector_distance_euclidean_squared(
          (const char *)a.data(), (const char *)b.data(), kDims));
    else
      bench_sink = vector_distance_euclidean_squared(
          (const char *)a.data(), (const char *)b.data(), kDims);
  }
  StopBenchmarkTiming();
  SetBytesProcessed(num_iterations * kDims * sizeof(float) * 2);
}

// ---------------------------------------------------------------------------
// Macro machinery
// ---------------------------------------------------------------------------

// Expands M(size) for each of the six benchmark sizes.
#define FOR_EACH_SIZE(M) M(4) M(8) M(32) M(128) M(1024) M(16383)

// Registers one Euclidean (L2 with sqrt) benchmark for a given tier + size.
#define BENCH_EUCLIDEAN_ONE(tier_enum, tier_label, dims)                   \
  static void BenchEuclidean_##tier_label##_##dims(size_t n) {             \
    bench_impl<VectorDistanceTier::tier_enum, dims, Metric::Euclidean>(n); \
  }                                                                        \
  BENCHMARK(BenchEuclidean_##tier_label##_##dims)

// Registers one EuclideanSquared benchmark for a given tier + size.
#define BENCH_EUCLIDEAN_SQUARED_ONE(tier_enum, tier_label, dims)               \
  static void BenchEuclideanSquared_##tier_label##_##dims(size_t n) {          \
    bench_impl<VectorDistanceTier::tier_enum, dims, Metric::EuclideanSquared>( \
        n);                                                                    \
  }                                                                            \
  BENCHMARK(BenchEuclideanSquared_##tier_label##_##dims)

// Registers one Cosine benchmark function for a given tier + size.
#define BENCH_COSINE_ONE(tier_enum, tier_label, dims)                   \
  static void BenchCosine_##tier_label##_##dims(size_t n) {             \
    bench_impl<VectorDistanceTier::tier_enum, dims, Metric::Cosine>(n); \
  }                                                                     \
  BENCHMARK(BenchCosine_##tier_label##_##dims)

// Per-size expanders (one per size, named so the tier macro can paste them).
#define BENCH_EUCLIDEAN_Scalar(dims) BENCH_EUCLIDEAN_ONE(Scalar, Scalar, dims)
#define BENCH_EUCLIDEAN_Sse42(dims) BENCH_EUCLIDEAN_ONE(Sse42, Sse42, dims)
#define BENCH_EUCLIDEAN_Avx2(dims) BENCH_EUCLIDEAN_ONE(Avx2, Avx2, dims)
#define BENCH_EUCLIDEAN_Avx512f(dims) \
  BENCH_EUCLIDEAN_ONE(Avx512f, Avx512f, dims)
#define BENCH_EUCLIDEAN_Neon(dims) BENCH_EUCLIDEAN_ONE(Neon, Neon, dims)

#define BENCH_EUCLIDEAN_SQUARED_Scalar(dims) \
  BENCH_EUCLIDEAN_SQUARED_ONE(Scalar, Scalar, dims)
#define BENCH_EUCLIDEAN_SQUARED_Sse42(dims) \
  BENCH_EUCLIDEAN_SQUARED_ONE(Sse42, Sse42, dims)
#define BENCH_EUCLIDEAN_SQUARED_Avx2(dims) \
  BENCH_EUCLIDEAN_SQUARED_ONE(Avx2, Avx2, dims)
#define BENCH_EUCLIDEAN_SQUARED_Avx512f(dims) \
  BENCH_EUCLIDEAN_SQUARED_ONE(Avx512f, Avx512f, dims)
#define BENCH_EUCLIDEAN_SQUARED_Neon(dims) \
  BENCH_EUCLIDEAN_SQUARED_ONE(Neon, Neon, dims)

#define BENCH_COSINE_Scalar(dims) BENCH_COSINE_ONE(Scalar, Scalar, dims)
#define BENCH_COSINE_Sse42(dims) BENCH_COSINE_ONE(Sse42, Sse42, dims)
#define BENCH_COSINE_Avx2(dims) BENCH_COSINE_ONE(Avx2, Avx2, dims)
#define BENCH_COSINE_Avx512f(dims) BENCH_COSINE_ONE(Avx512f, Avx512f, dims)
#define BENCH_COSINE_Neon(dims) BENCH_COSINE_ONE(Neon, Neon, dims)

// Registers one DotProduct benchmark function for a given tier + size.
#define BENCH_DOT_PRODUCT_ONE(tier_enum, tier_label, dims)                  \
  static void BenchDotProduct_##tier_label##_##dims(size_t n) {             \
    bench_impl<VectorDistanceTier::tier_enum, dims, Metric::DotProduct>(n); \
  }                                                                         \
  BENCHMARK(BenchDotProduct_##tier_label##_##dims)

#define BENCH_DOT_PRODUCT_Scalar(dims) \
  BENCH_DOT_PRODUCT_ONE(Scalar, Scalar, dims)
#define BENCH_DOT_PRODUCT_Sse42(dims) BENCH_DOT_PRODUCT_ONE(Sse42, Sse42, dims)
#define BENCH_DOT_PRODUCT_Avx2(dims) BENCH_DOT_PRODUCT_ONE(Avx2, Avx2, dims)
#define BENCH_DOT_PRODUCT_Avx512f(dims) \
  BENCH_DOT_PRODUCT_ONE(Avx512f, Avx512f, dims)
#define BENCH_DOT_PRODUCT_Neon(dims) BENCH_DOT_PRODUCT_ONE(Neon, Neon, dims)

// Registers one Manhattan benchmark function for a given tier + size.
#define BENCH_MANHATTAN_ONE(tier_enum, tier_label, dims)                   \
  static void BenchManhattan_##tier_label##_##dims(size_t n) {             \
    bench_impl<VectorDistanceTier::tier_enum, dims, Metric::Manhattan>(n); \
  }                                                                        \
  BENCHMARK(BenchManhattan_##tier_label##_##dims)

#define BENCH_MANHATTAN_Scalar(dims) BENCH_MANHATTAN_ONE(Scalar, Scalar, dims)
#define BENCH_MANHATTAN_Sse42(dims) BENCH_MANHATTAN_ONE(Sse42, Sse42, dims)
#define BENCH_MANHATTAN_Avx2(dims) BENCH_MANHATTAN_ONE(Avx2, Avx2, dims)
#define BENCH_MANHATTAN_Avx512f(dims) \
  BENCH_MANHATTAN_ONE(Avx512f, Avx512f, dims)
#define BENCH_MANHATTAN_Neon(dims) BENCH_MANHATTAN_ONE(Neon, Neon, dims)

// ---------------------------------------------------------------------------
// Tier registrations — all tiers are always declared; unavailable ones skip.
// ---------------------------------------------------------------------------

// Tier 0 — Scalar (all platforms)
FOR_EACH_SIZE(BENCH_EUCLIDEAN_Scalar)
FOR_EACH_SIZE(BENCH_EUCLIDEAN_SQUARED_Scalar)
FOR_EACH_SIZE(BENCH_COSINE_Scalar)
FOR_EACH_SIZE(BENCH_DOT_PRODUCT_Scalar)
FOR_EACH_SIZE(BENCH_MANHATTAN_Scalar)

// x86_64 tiers
#if defined(__x86_64__) || defined(_M_X64)

FOR_EACH_SIZE(BENCH_EUCLIDEAN_Sse42)
FOR_EACH_SIZE(BENCH_EUCLIDEAN_SQUARED_Sse42)
FOR_EACH_SIZE(BENCH_COSINE_Sse42)
FOR_EACH_SIZE(BENCH_DOT_PRODUCT_Sse42)
FOR_EACH_SIZE(BENCH_MANHATTAN_Sse42)

FOR_EACH_SIZE(BENCH_EUCLIDEAN_Avx2)
FOR_EACH_SIZE(BENCH_EUCLIDEAN_SQUARED_Avx2)
FOR_EACH_SIZE(BENCH_COSINE_Avx2)
FOR_EACH_SIZE(BENCH_DOT_PRODUCT_Avx2)
FOR_EACH_SIZE(BENCH_MANHATTAN_Avx2)

FOR_EACH_SIZE(BENCH_EUCLIDEAN_Avx512f)
FOR_EACH_SIZE(BENCH_EUCLIDEAN_SQUARED_Avx512f)
FOR_EACH_SIZE(BENCH_COSINE_Avx512f)
FOR_EACH_SIZE(BENCH_DOT_PRODUCT_Avx512f)
FOR_EACH_SIZE(BENCH_MANHATTAN_Avx512f)

#endif  // x86_64

// aarch64 tiers
#if defined(__aarch64__) || defined(_M_ARM64)

FOR_EACH_SIZE(BENCH_EUCLIDEAN_Neon)
FOR_EACH_SIZE(BENCH_EUCLIDEAN_SQUARED_Neon)
FOR_EACH_SIZE(BENCH_COSINE_Neon)
FOR_EACH_SIZE(BENCH_DOT_PRODUCT_Neon)
FOR_EACH_SIZE(BENCH_MANHATTAN_Neon)

#endif  // aarch64

// ---------------------------------------------------------------------------
// Cleanup macros
// ---------------------------------------------------------------------------

#undef BENCH_MANHATTAN_Neon
#undef BENCH_MANHATTAN_Avx512f
#undef BENCH_MANHATTAN_Avx2
#undef BENCH_MANHATTAN_Sse42
#undef BENCH_MANHATTAN_Scalar
#undef BENCH_MANHATTAN_ONE
#undef BENCH_DOT_PRODUCT_Neon
#undef BENCH_DOT_PRODUCT_Avx512f
#undef BENCH_DOT_PRODUCT_Avx2
#undef BENCH_DOT_PRODUCT_Sse42
#undef BENCH_DOT_PRODUCT_Scalar
#undef BENCH_DOT_PRODUCT_ONE
#undef BENCH_COSINE_Neon
#undef BENCH_COSINE_Avx512f
#undef BENCH_COSINE_Avx2
#undef BENCH_COSINE_Sse42
#undef BENCH_COSINE_Scalar
#undef BENCH_EUCLIDEAN_SQUARED_Neon
#undef BENCH_EUCLIDEAN_SQUARED_Avx512f
#undef BENCH_EUCLIDEAN_SQUARED_Avx2
#undef BENCH_EUCLIDEAN_SQUARED_Sse42
#undef BENCH_EUCLIDEAN_SQUARED_Scalar
#undef BENCH_EUCLIDEAN_SQUARED_ONE
#undef BENCH_EUCLIDEAN_Neon
#undef BENCH_EUCLIDEAN_Avx512f
#undef BENCH_EUCLIDEAN_Avx2
#undef BENCH_EUCLIDEAN_Sse42
#undef BENCH_EUCLIDEAN_Scalar
#undef BENCH_COSINE_ONE
#undef BENCH_EUCLIDEAN_ONE
#undef FOR_EACH_SIZE

}  // namespace vector_distance_tier_bench
