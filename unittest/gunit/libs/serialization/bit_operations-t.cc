// Copyright (c) 2023, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>
#include <bitset>
#include <random>
#include <sstream>
#include <string>
#include "mysql/utils/bit_operations.h"

namespace mysql::utils::bitops {

TEST(BitOperations, Clz) {
#if defined(CALCULATE_BITS_CUSTOM_IMPL)
  std::cout << "custom implementation" << std::endl;
#elif defined(__clang__) || defined(__GNUC__)
  std::cout << "compiler bultin" << std::endl;
#elif defined(_WIN64)
  std::cout << "windows 64 _BitScanReverse" << std::endl;
#elif defined(_WIN32)
  std::cout << "windows 32 _BitScanReverse" << std::endl;
#else
  std::cout << "custom implementation" << std::endl;
#endif

  std::cout << "sizeof(long): " << sizeof(long)
            << " sizeof(size_t): " << sizeof(std::size_t) << std::endl;

#if (defined(_WIN64) || defined(_WIN32))
  {
    unsigned long result = 0;
    uint32_t test_v = 8;
    _BitScanReverse(&result, test_v);
    std::cout << "32 result for 8, leading: " << result << std::endl;
  }
#endif

#if defined(_WIN64)
  {
    unsigned long result = 0;
    uint64_t test_v = 8;
    _BitScanReverse64(&result, test_v);
    std::cout << "64 result for 8, leading: " << result << std::endl;
  }
#endif

  // simple tests

  uint32_t test_1 = 1;
  ASSERT_EQ(countr_zero(test_1), 0);
  ASSERT_EQ(countr_one(~test_1), 0);
  uint32_t test_2 = 1LL << 1;
  ASSERT_EQ(countr_zero(test_2), 1);
  ASSERT_EQ(countr_one(~test_2), 1);
  uint64_t test_3 = 1LL << 33;
  ASSERT_EQ(countr_zero(test_3), 33);
  ASSERT_EQ(countr_one(~test_3), 33);

  // zero tests

  uint32_t test_4 = 0;
  ASSERT_EQ(countr_zero(test_4), 0);
  ASSERT_EQ(countl_zero(test_4), 31);
  uint64_t test_5 = 0;
  ASSERT_EQ(countr_zero(test_5), 0);
  ASSERT_EQ(countl_zero(test_5), 63);

  // custom tests

  uint64_t test_6 = 23641781698560ULL;
  ASSERT_EQ(countr_zero(test_6), 27);
  ASSERT_EQ(countl_zero(test_6), 19);

  uint64_t test_7 = 8;
  ASSERT_EQ(countr_zero(test_7), 3);
  ASSERT_EQ(countl_zero(test_7), 60);

  uint32_t test_8 = 8;
  ASSERT_EQ(countr_zero(test_8), 3);
  ASSERT_EQ(countl_zero(test_8), 28);
}

// Requirements:
//
// R1.a. For every 64-bit number having N trailing 0 bits, where 0<=N<64,
//       calc_trailing_zeros shall return N.
//    b. For every 32-bit number having N trailing 0 bits, where 0<=N<32,
//       calc_trailing_zeros shall return N.
// R2.a. For every 64-bit number having N trailing 1 bits, where 0<=N<64,
//       calc_trailing_ones shall return N.
//    b. For every 32-bit number having N trailing 1 bits, where 0<=N<32,
//       calc_trailing_ones shall return N.
// R2.a. For every 64-bit number having N leading 0 bits, where 0<=N<64,
//       calc_leading_zeros shall return N.
//    b. For every 32-bit number having N leading 0 bits, where 0<=N<32,
//       calc_leading_zeros shall return N.
//
// Both the result and the execution of these functions actually
// depends on the number of leading/trailing zeros/ones.  In
// particular, the functions use a lookup table of magic numbers, and
// will inspect different entries in that lookup table depending on
// the number of leading/trailing zeros/ones.  In order to verify that
// the contents of the lookup table are correct, we test all possible
// lengths of leading/trailing runs of zeros/ones.  The function
// result or execution does not depend on the other bits, so we make
// them random.
TEST(BitOperations, Exhaustive) {
  // Number with leftmost bit set to 1
  constexpr uint64_t high_one_64 = 1ull << 63;

  std::random_device random_device;
  std::mt19937 generator(random_device());
  std::uniform_int_distribution<> distribution;

  // Try 1000 random numbers.
  for (int i = 0; i < 1000; ++i) {
    uint64_t number = distribution(generator);

    // Iterate over the number of bits in the prefix/suffix of
    // zeros/ones in the number.
    for (int64_t bit = 0; bit < 64; ++bit) {
      // Exactly `bit` trailing zeros, the rest random.
      uint64_t low_zeros = (number | 1) << bit;
      // Exactly `bit` trailing ones, the rest random.
      uint64_t low_ones = ~low_zeros;
      // Exactly `bit` leading zeros, the rest random.
      uint64_t high_zeros = (number | high_one_64) >> bit;
      ASSERT_EQ(countr_zero(low_zeros), bit) << number;
      ASSERT_EQ(countr_one(low_ones), bit) << number;
      ASSERT_EQ(countl_zero(high_zeros), bit) << number;
      ASSERT_EQ(bit_width(high_zeros), 64 - bit) << number;
      if (bit < 32) {
        ASSERT_EQ(countr_zero(static_cast<uint32_t>(low_zeros)), bit) << number;
        ASSERT_EQ(countr_one(static_cast<uint32_t>(low_ones)), bit) << number;
        ASSERT_EQ(countl_zero(static_cast<uint32_t>(high_zeros >> 32)), bit)
            << number;
      }
    }
  }
}

}  // namespace mysql::utils::bitops
