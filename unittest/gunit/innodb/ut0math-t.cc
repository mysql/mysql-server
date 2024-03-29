/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
#include "unittest/gunit/benchmark.h"

#include "storage/innobase/include/ut0rnd.h"

namespace innodb_ut0math_unittest {

/* Correctness test for math functions. */

void test_multiply_uint64(uint64_t x, uint64_t y) {
  const auto xy = x * y;
  uint64_t hi;
  uint64_t lo = ut::detail::multiply_uint64_portable(x, y, hi);
  ASSERT_EQ(hi, 0);
  ASSERT_EQ(lo, xy);
}

TEST(ut0math, multiply_uint64_portable) {
  for (int i = 0; i < 100000; i++) {
    const uint64_t x = ut::random_64() >> 2;
    if (x < 10) continue;

    /* The x*max_y will fit into 64bits. */
    const uint64_t max_y = std::numeric_limits<uint64_t>::max() / x;

    test_multiply_uint64(x, max_y);
    for (int j = 0; j < 10; j++) {
      const uint64_t y = (ut::random_64() >> 2) % (max_y + 1);
      test_multiply_uint64(x, y);
    }
  }
}

/* Correctness of the multiply_uint64_portable tested using Chinese Rest
Theorem. */
TEST(ut0math, multiply_uint64_portable_chinese) {
  for (int k = 0; k < 100; k++) {
    /* Choose a random 32bit prime and calculate 2^64 % P. This value will be
    used to calculate value of (128bit integer % P). ut::find_prime finds a
    prime bigger than specified argument, so we pass value shifted by more than
    32 bits. */
    const uint64_t P = ut::find_prime(ut::random_64() >> 34);
    ASSERT_LT(P, 1ULL << 32);
    const uint64_t two_to_32_mod_P = (1ULL << 32) % P;
    const uint64_t two_to_64_mod_P = (two_to_32_mod_P * two_to_32_mod_P) % P;
    for (int i = 0; i < 10000; i++) {
      const uint64_t x = ut::random_64();
      const uint64_t y = ut::random_64();

      uint64_t hi;
      const uint64_t lo = ut::detail::multiply_uint64_portable(x, y, hi);
      /* Does the result agree modulo P? */
      const uint64_t expected = ((x % P) * (y % P)) % P;
      const uint64_t actual = (hi % P * two_to_64_mod_P % P + lo % P) % P;

      ASSERT_EQ(actual, expected);
      /* Does the result agree modulo 2^64? */
      ASSERT_EQ(lo, static_cast<uint64_t>(x * y));
      /* If the above two conditions are met, then it means the result is
      correct modulo (P * 2^64). We could add a second random prime Q to mix
      to have the multiplication result checked against (P * Q * 2^64), which
      would be almost 2^128 bit number, but the 96bits we have now seems enough
      having the P is chosen randomly. */
    }
  }
}

TEST(ut0math, divide_uint128) {
  for (int i = 0; i < 1000000; i++) {
    const uint64_t x = ut::random_64();
    const uint64_t y = ut::random_64();
    if (x == 0 || y == 0) continue;
    uint64_t high;
    uint64_t low = ut::detail::multiply_uint64_portable(x, y, high);
    ASSERT_EQ(ut::divide_128(high, low, x), y);
    ASSERT_EQ(ut::divide_128(high, low, y), x);
  }
}

TEST(ut0math, fast_modulo) {
  for (int i = 0; i < 1000000; i++) {
    const uint64_t x = ut::random_64();
    const uint64_t y = ut::random_64();
    if (y == 0) continue;
    ut::fast_modulo_t mod{y};
    ASSERT_EQ(x % y, x % mod);
  }
  for (int i = 0; i < 1000000; i++) {
    const uint64_t x = ut::random_64();
    const uint64_t y = ut::random_64() % 1000 + 1;
    ASSERT_EQ(x % y, x % ut::fast_modulo_t{y});
  }
}

/* Micro-benchmark raw fast modulo performance. */

static void BM_FAST_MODULO_CALCULATE(const size_t num_iterations) {
  uint32_t fold = 0;
  ut::fast_modulo_t mod{123 + num_iterations};
  for (size_t n = 0; n < num_iterations * 1000; n++) {
    fold += n % mod;
  }
  EXPECT_NE(0U, fold);  // To keep the compiler from optimizing it away.
  SetBytesProcessed(num_iterations * 1000);
}
BENCHMARK(BM_FAST_MODULO_CALCULATE)

static void BM_MODULO_CALCULATE_CONSTEXPR_MOD(const size_t num_iterations) {
  uint32_t fold = 0;
  constexpr uint64_t mod{123};
  for (size_t n = 0; n < num_iterations * 1000; n++) {
    fold += n % mod;
  }
  EXPECT_NE(0U, fold);  // To keep the compiler from optimizing it away.
  SetBytesProcessed(num_iterations * 1000);
}
BENCHMARK(BM_MODULO_CALCULATE_CONSTEXPR_MOD)

static void BM_MODULO_CALCULATE_VARIABLE_MOD(const size_t num_iterations) {
  uint32_t fold = 0;
  const uint64_t mod{123 + num_iterations};
  for (size_t n = 0; n < num_iterations * 1000; n++) {
    fold += n % mod;
  }
  EXPECT_NE(0U, fold);  // To keep the compiler from optimizing it away.
  SetBytesProcessed(num_iterations * 1000);
}
BENCHMARK(BM_MODULO_CALCULATE_VARIABLE_MOD)

}  // namespace innodb_ut0math_unittest
