/* Copyright (c) 2013, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* See http://code.google.com/p/googletest/wiki/Primer */

#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <random>
#include "unittest/gunit/benchmark.h"

#include "storage/innobase/include/mach0data.h"
#include "storage/innobase/include/univ.i"
#include "storage/innobase/include/ut0crc32.h"
#include "storage/innobase/include/ut0rnd.h"

#include "my_xxhash.h"

namespace innodb_ut0rnd_unittest {

namespace old_impl {

/* Old implementations to compare against. */

#define UT_RND1 151117737
#define UT_RND2 119785373
#define UT_RND3 85689495
#define UT_RND4 76595339
#define UT_SUM_RND2 98781234
#define UT_SUM_RND3 126792457
#define UT_SUM_RND4 63498502
#define UT_XOR_RND1 187678878
#define UT_XOR_RND2 143537923

static inline ulint ut_rnd_gen_next_ulint(ulint rnd) {
  ulint n_bits;
  n_bits = 8 * sizeof(ulint);
  rnd = UT_RND2 * rnd + UT_SUM_RND3;
  rnd = UT_XOR_RND1 ^ rnd;
  rnd = (rnd << 20) + (rnd >> (n_bits - 20));
  rnd = UT_RND3 * rnd + UT_SUM_RND4;
  rnd = UT_XOR_RND2 ^ rnd;
  rnd = (rnd << 20) + (rnd >> (n_bits - 20));
  rnd = UT_RND1 * rnd + UT_SUM_RND2;
  return (rnd);
}

thread_local ulint ut_rnd_ulint_counter;

static inline ulint ut_rnd_gen_ulint() {
  ulint rnd = ut_rnd_ulint_counter;
  if (rnd == 0) {
    rnd = 65654363;
  }

  rnd = UT_RND1 * rnd + UT_RND2;

  ut_rnd_ulint_counter = rnd;

  return (ut_rnd_gen_next_ulint(rnd));
}

constexpr uint32_t UT_HASH_RANDOM_MASK = 1463735687;
constexpr uint32_t UT_HASH_RANDOM_MASK2 = 1653893711;

static inline ulint ut_hash_ulint(ulint key, ulint table_size) {
  ut_ad(table_size);
  key = key ^ UT_HASH_RANDOM_MASK2;

  return (key % table_size);
}
static inline ulint ut_fold_ulint_pair(ulint n1, ulint n2) {
  return (
      ((((n1 ^ n2 ^ UT_HASH_RANDOM_MASK2) << 8) + n1) ^ UT_HASH_RANDOM_MASK) +
      n2);
}

static inline ulint ut_fold_ull(uint64_t d) {
  return (ut_fold_ulint_pair((ulint)d & std::numeric_limits<uint32_t>::max(),
                             (ulint)(d >> 32)));
}

}  // namespace old_impl

static void init() {
  ut::detail::random_seed = 0;
  old_impl::ut_rnd_ulint_counter = 0;
}

struct random_data_t {
  std::array<byte, 100000> data;
  random_data_t() {
    for (byte &b : data) {
      b = rand();
    }
  }
};
random_data_t random_data{};

/* Correctness tests for hashing methods. */

TEST(ut0rnd, hash_binary_ib_basic) {
  init();
  /* This value is used in page checksum for innodb algorithm, if chosen. This
  can't ever change to not make existing databases invalid. */
  EXPECT_EQ(58956420, ut::hash_binary_ib((const byte *)"innodb", 6));
}

static double calculate_limit_variance_from_expected(uint32_t n) {
  /* We set the limit arbitrarily. The higher N the lower the distance from
  expected variation should be. The below formula has points at around:
  3.00 for N=8,
  0.65 for N=64,
  0.25 for N=1024,
  0.11 for N=10000 */
  return 4.5 / log2(n / 2.0) + 0.75;
}

/** Tests if a given hasher produces a nice distribution. The buckets can't
differ in sizes more than 10% from the expected average. */
template <typename T, uint32_t N>
double test_distribution(const std::string &test_name, T hasher,
                         uint64_t multiplier) {
  std::array<uint64_t, N> buckets{0};
  std::array<uint64_t, N> max_res_buckets{0};
  constexpr auto maxM = std::max(uint32_t{10000}, N * 5);
  constexpr auto stepM = 3 * N;
  const ut::fast_modulo_t N_mod{N};
  int max_res_M = 0;
  double max_res = 0;
  const auto variance_limit = calculate_limit_variance_from_expected(N);
  for (uint32_t M = stepM, i = 0; M <= maxM; M += stepM) {
    for (; i < M; i++) {
      uint32_t hash = hasher(i * multiplier, N_mod);
      assert(hash < N);
      buckets[hash]++;
    }
    /* Suppose I choose two of the M elements at random (with replacement) and
    ask if they "collide" by being in the same bucket.
    A bucket with k elements contributes k*k such collisions. */
    uint64_t collisions = 0;
    for (uint32_t j = 0; j < N; j++) {
      collisions += buckets[j] * buckets[j];
    }
    /* Average number of elements in each bucket. */
    const auto avg = 1.0 * M / N;
    const auto minimum_possible_collisions = avg * avg * N;
    const auto excess_collisions = collisions - minimum_possible_collisions;
    /* If we just tossed M balls into N bins randomly, so that a ball landing in
    a particular bin has a chance p=1/N, the variance would be M*(1/N)*(1-1/N).
    Note that variance*N = sum {(buckets[j]-avg)^2} = excess_collisions.
    */
    const auto good_excess_collisions = 1.0 * M * (1 - 1.0 / N);
    /* We calculate "score" as ratio of actual number of excess collisions to
    the above good excess collisions. The lower the better. Also, we adjust
    score based on value of M, so that small values of M are required to be
    closer to the good variance and higher can have up to 50% more collisions
    for the same score. */
    const auto score = excess_collisions / good_excess_collisions /
                       (1 + std::min(0.5, 1.0 * (M - stepM) / 200));
    if (score > max_res) {
      max_res = score;
      max_res_M = M;
      max_res_buckets = buckets;
    }
  }
  /* Print all distributions that get close to the limit (70% of it) */
  if (max_res > 1 + (variance_limit - 1) * 0.7) {
    std::cout << "Bad distribution found for test \"" << test_name
              << "\", N=" << N << ", mult=" << multiplier
              << ", max_res=" << max_res << " @ M=" << max_res_M
              << ", limit=" << variance_limit << std::endl;
    if (N < 100) {
      for (auto a : max_res_buckets) {
        std::cout << a << " ";
      }
      std::cout << std::endl;
    }
  }
  return max_res;
}

template <typename T, uint32_t N>
void test_distribution(bool assert_distribution, const std::string &test_name,
                       T hasher) {
  double score = 0;
  constexpr auto P = 10;
  for (int i = 0; i <= P; i++) {
    score =
        std::max(score, test_distribution<T, N>(test_name, hasher, 1ULL << i));
  }
  score = std::max(score, test_distribution<T, N>(test_name, hasher, 7));
  score = std::max(score, test_distribution<T, N>(test_name, hasher, 11));
  score =
      std::max(score, test_distribution<T, N>(test_name, hasher, 2 * 3 * 5));

  const auto variance_limit = calculate_limit_variance_from_expected(N);
  std::cout << "Overall score for N=" << N << " is: " << score
            << ", while the limit for good enough result is " << variance_limit
            << std::endl;

  if (assert_distribution) {
    EXPECT_LE(score, variance_limit);
  }
}

template <typename THash>
void test_distribution(bool assert_distribution, const std::string &test_name,
                       THash hasher) {
  test_distribution<THash, 8>(assert_distribution, test_name,
                              hasher);  // For 2^n distributions
  test_distribution<THash, 10>(assert_distribution, test_name,
                               hasher);  // For 10^n distributions
  test_distribution<THash, 11>(assert_distribution, test_name,
                               hasher);  // For small prime distribution
  test_distribution<THash, 64>(assert_distribution, test_name,
                               hasher);  // For 2^n distributions
  test_distribution<THash, 1 << 10>(assert_distribution, test_name,
                                    hasher);  // For big 2^n distribution
  test_distribution<THash, 1000>(assert_distribution, test_name,
                                 hasher);  // For big 10^n distribution
  test_distribution<THash, 1 << 13>(assert_distribution, test_name,
                                    hasher);  // For big 2^n distribution
  test_distribution<THash, 10 * 1000>(assert_distribution, test_name,
                                      hasher);  // For big 10^n distribution
}

/* Test distributions for algorithms that hash uint64_t. */

TEST(ut0rnd, hash_uint64_distribution) {
  init();
  test_distribution(true, "ut::hash_uint64(i)",
                    [](size_t i, const ut::fast_modulo_t &n) {
                      return ut::hash_uint64(i) % n;
                    });
}

TEST(ut0rnd, hash_std_hash_distribution) {
  init();
  test_distribution(false, "std::hash<uint64_t>{}(i)",
                    [](size_t i, const ut::fast_modulo_t &n) {
                      return std::hash<uint64_t>{}(i) % n;
                    });
}
TEST(ut0rnd, hash_uint32_old_distribution) {
  init();
  test_distribution(false, "old_impl::ut_hash_ulint(i, N)",
                    [](size_t i, const ut::fast_modulo_t &n) {
                      return old_impl::ut_hash_ulint(i, n.get_mod());
                    });
}

TEST(ut0rnd, hash_uint64_pair_sysbench_ahi_distribution) {
  std::array<size_t, 8> buckets{};
  for (int i = 0; i < 8; i++) {
    uint32_t hash = ut::hash_uint64_pair(149 + 2 * i, i) % 8;
    buckets[hash]++;
  }

  int res = 0;
  for (int i = 0; i < 8; i++) {
    if (buckets[i] != 0) {
      res++;
    }
  }

  EXPECT_GE(res, 6);
}

/* Test distributions for algorithms that hash pair of uint32_t that are:
increasing together or with either one being constant. */

template <typename THash>
static void hash_pair_distribution_test(bool assert_distribution,
                                        const std::string &test_name,
                                        THash hasher) {
  init();
  test_distribution(assert_distribution, test_name + "(i, i)",
                    [&hasher](size_t i, const ut::fast_modulo_t &n) {
                      return hasher(i, i) % n;
                    });
  test_distribution(assert_distribution, test_name + "(1, i)",
                    [&hasher](size_t i, const ut::fast_modulo_t &n) {
                      return hasher(1, i) % n;
                    });
  test_distribution(assert_distribution, test_name + "(i, 1)",
                    [&hasher](size_t i, const ut::fast_modulo_t &n) {
                      return hasher(i, 1) % n;
                    });
  /* Distribution basing on <index_id, space_id> generated for tables by
  sysbench. */
  test_distribution(assert_distribution, test_name + "(149+2*i, i)",
                    [&hasher](size_t i, const ut::fast_modulo_t &n) {
                      return hasher(149 + 2 * i, i) % n;
                    });
}

TEST(ut0rnd, hash_uint64_pair_distribution) {
  hash_pair_distribution_test(true, "ut::hash_uint64_pair",
                              ut::hash_uint64_pair);
}

TEST(ut0rnd, hash_uint32_pair_old_distribution) {
  hash_pair_distribution_test(false, "ut::detail::hash_uint32_pair_ib",
                              ut::detail::hash_uint32_pair_ib);
}

/* Micro-benchmark raw random generator performance. */

template <typename THash>
static void benchmark_hasher(const size_t num_iterations, THash hasher) {
  init();

  uint32_t fold = 0;
  for (size_t n = 0; n < num_iterations * 1000; n++) {
    fold += hasher(fold, n);
  }
  EXPECT_NE(0U, fold);  // To keep the compiler from optimizing it away.
  SetBytesProcessed(num_iterations * 1000);
}

static void BM_RND_GEN_OLD(const size_t num_iterations) {
  benchmark_hasher(num_iterations, [](uint64_t, uint64_t) {
    return old_impl::ut_rnd_gen_ulint();
  });
}
BENCHMARK(BM_RND_GEN_OLD)

static void BM_RND_GEN_STD_HASH(const size_t num_iterations) {
  benchmark_hasher(num_iterations, [](uint64_t, uint64_t n) {
    return std::hash<uint64_t>{}(n);
  });
}
BENCHMARK(BM_RND_GEN_STD_HASH)

#if SIZEOF_VOIDP >= 8
static void BM_RND_GEN_STD_LINEAR(const size_t num_iterations) {
  std::linear_congruential_engine<
      uint64_t, ut::detail::fast_hash_coeff_a1_64bit,
      ut::detail::fast_hash_coeff_b_64bit, std::numeric_limits<uint64_t>::max()>
      eng;
  benchmark_hasher(num_iterations,
                   [&eng](uint64_t, uint64_t n) { return eng(); });
}
BENCHMARK(BM_RND_GEN_STD_LINEAR)
#endif

static void BM_RND_GEN(const size_t num_iterations) {
  benchmark_hasher(num_iterations,
                   [](uint64_t, uint64_t n) { return ut::random_64(); });
}
BENCHMARK(BM_RND_GEN)

/* Micro-benchmark raw uint64_t hash performance. */

static void BM_HASH_UINT64(const size_t num_iterations) {
  benchmark_hasher(num_iterations, [](uint64_t fold, uint64_t) {
    return ut::hash_uint64(fold);
  });
}
BENCHMARK(BM_HASH_UINT64)

static void BM_HASH_UINT64_OLD(const size_t num_iterations) {
  benchmark_hasher(num_iterations, [](uint64_t fold, uint64_t) {
    return old_impl::ut_fold_ull(fold);
  });
}
BENCHMARK(BM_HASH_UINT64_OLD)

/* Micro-benchmark raw pair of uint32_t hash performance. */

static void BM_HASH_UINT64_PAIR(const size_t num_iterations) {
  benchmark_hasher(num_iterations, [](uint64_t fold, uint64_t n) {
    return ut::hash_uint64_pair(fold, ut::random_64());
  });
}
BENCHMARK(BM_HASH_UINT64_PAIR)

static void BM_HASH_UINT32_PAIR_OLD(const size_t num_iterations) {
  benchmark_hasher(num_iterations, [](uint64_t fold, uint64_t n) {
    return ut::detail::hash_uint32_pair_ib(fold, ut::random_64());
  });
}
BENCHMARK(BM_HASH_UINT32_PAIR_OLD)

/* Micro-benchmark raw performance of several hashing algorithms of arbitrary
 * string. */

#define BENCHMARK_HASH(NAME, N)                             \
  static void BM_HASH_##NAME##_##N(size_t num_iterations) { \
    BM_HASH_##NAME<N>(num_iterations);                      \
  }                                                         \
  BENCHMARK(BM_HASH_##NAME##_##N)
#define BENCHMARK_HASHES(NAME) \
  BENCHMARK_HASH(NAME, 5)      \
  BENCHMARK_HASH(NAME, 16)     \
  BENCHMARK_HASH(NAME, 31)     \
  BENCHMARK_HASH(NAME, 63)     \
  BENCHMARK_HASH(NAME, 127)    \
  BENCHMARK_HASH(NAME, 255)    \
  BENCHMARK_HASH(NAME, 511)    \
  BENCHMARK_HASH(NAME, 1023)   \
  BENCHMARK_HASH(NAME, 2047)   \
  BENCHMARK_HASH(NAME, 4095)   \
  BENCHMARK_HASH(NAME, 8191)   \
  BENCHMARK_HASH(NAME, 16383)  \
  BENCHMARK_HASH(NAME, 32767)  \
  BENCHMARK_HASH(NAME, 65535)

template <uint32_t N, typename THash>
static void benchmark_binary_hasher(const size_t num_iterations, THash hasher) {
  init();
  ut_crc32_init();
  uint64_t fold = 0;
  size_t i = 0;
  for (size_t n = 0; n < num_iterations * 1000; n++) {
    i = i > 20000 - N ? i + 1 : 0;
    fold = hasher(&random_data.data[i], fold);
  }
  EXPECT_NE(0U, fold);  // To keep the compiler from optimizing it away.
  SetBytesProcessed(num_iterations * N * 1000);
}

template <uint32_t N>
static void BM_HASH_BINARY_XXHASH(const size_t num_iterations) {
  benchmark_binary_hasher<N>(num_iterations, [](byte *buf, uint64_t fold) {
    return XXH64(buf, N, fold);
  });
}
BENCHMARK_HASHES(BINARY_XXHASH)

template <uint32_t N>
static void BM_HASH_BINARY_STD(const size_t num_iterations) {
  benchmark_binary_hasher<N>(num_iterations, [](byte *buf, uint64_t fold) {
    return ut::hash_uint64_pair(
        fold,
        std::hash<std::string_view>{}(std::string_view((const char *)buf, N)));
  });
}
BENCHMARK_HASHES(BINARY_STD)

template <uint32_t N>
static void BM_HASH_BINARY_OLD(const size_t num_iterations) {
  benchmark_binary_hasher<N>(num_iterations, [](byte *buf, uint64_t fold) {
    return ut::hash_uint64_pair(fold, ut::hash_binary_ib(buf, N));
  });
}
BENCHMARK_HASHES(BINARY_OLD)

template <uint32_t N>
static void BM_HASH_BINARY_UT(const size_t num_iterations) {
  benchmark_binary_hasher<N>(num_iterations, [](byte *buf, uint64_t fold) {
    return ut::hash_uint64_pair(fold, ut::hash_binary(buf, N));
  });
}
BENCHMARK_HASHES(BINARY_UT)

template <uint32_t N>
static void BM_HASH_BINARY_CRC32(const size_t num_iterations) {
  benchmark_binary_hasher<N>(num_iterations, [](byte *buf, uint64_t fold) {
    return ut::hash_uint64_pair(fold, ut_crc32(buf, N));
  });
}
BENCHMARK_HASHES(BINARY_CRC32)

}  // namespace innodb_ut0rnd_unittest
