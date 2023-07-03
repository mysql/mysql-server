/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/stdx/bit.h"

#include <array>
#include <cstdint>  // UINT64_CC
#include <limits>   // numeric_limits

#include <gtest/gtest.h>

template <class T>
class ByteSwapTest : public ::testing::Test {};

using byte_swap_test_types =
    ::testing::Types<int8_t, int16_t, int32_t, int64_t, char, int, long,
                     long long, intmax_t, uint8_t, uint16_t, uint32_t, uint64_t,
                     unsigned char, unsigned int, unsigned long,
                     unsigned long long, uintmax_t>;

TYPED_TEST_SUITE(ByteSwapTest, byte_swap_test_types);

TYPED_TEST(ByteSwapTest, bswap) {
  EXPECT_EQ(
      stdx::byteswap(static_cast<TypeParam>(UINT64_C(0x8f))),
      static_cast<TypeParam>(UINT64_C(0x8f) << ((sizeof(TypeParam) - 1) * 8)));
}

template <class T>
class StdxBitTest : public ::testing::Test {};

using stdx_bit_test_types =
    ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t, unsigned char,
                     unsigned int, unsigned long, unsigned long long,
                     uintmax_t>;

TYPED_TEST_SUITE(StdxBitTest, stdx_bit_test_types);

TYPED_TEST(StdxBitTest, popcount) {
  const TypeParam max_value = std::numeric_limits<TypeParam>::max();

  // use samples that spread not just containing a few bits
  std::array<TypeParam, 8> samples = {0,
                                      (max_value / 8) * 1 + 1,
                                      (max_value / 8) * 2 - 1,
                                      (max_value / 8) * 3 + 1,
                                      (max_value / 8) * 4 - 1,
                                      (max_value / 8) * 5 + 1,
                                      (max_value / 8) * 6 - 1,
                                      max_value};
  for (TypeParam v : samples) {
    EXPECT_EQ(stdx::impl::popcount_constant(v), stdx::popcount(v))
        << static_cast<uint64_t>(v);
  }
}

TYPED_TEST(StdxBitTest, countl_zero) {
  EXPECT_EQ(std::numeric_limits<TypeParam>::digits,
            stdx::countl_zero(static_cast<TypeParam>(0)));

  TypeParam v = 1;

  for (size_t r = 0; r < std::numeric_limits<TypeParam>::digits; ++r) {
    EXPECT_EQ(std::numeric_limits<TypeParam>::digits - r - 1,
              stdx::countl_zero(v));
    v <<= 1;
  }
}

TYPED_TEST(StdxBitTest, countl_zero_impl_linear) {
  EXPECT_EQ(std::numeric_limits<TypeParam>::digits,
            stdx::impl::countl_zero_linear(static_cast<TypeParam>(0)));

  TypeParam v = 1;

  for (size_t r = 0; r < std::numeric_limits<TypeParam>::digits; ++r) {
    EXPECT_EQ(std::numeric_limits<TypeParam>::digits - r - 1,
              stdx::impl::countl_zero_linear(v));
    v <<= 1;
  }
}

TYPED_TEST(StdxBitTest, countr_zero) {
  EXPECT_EQ(std::numeric_limits<TypeParam>::digits,
            stdx::countr_zero(static_cast<TypeParam>(0)));

  for (size_t r = 0; r < std::numeric_limits<TypeParam>::digits; ++r) {
    EXPECT_EQ(r, stdx::countr_zero(
                     static_cast<TypeParam>(static_cast<TypeParam>(1) << r)));
  }
}

TYPED_TEST(StdxBitTest, countr_zero_impl_linear) {
  EXPECT_EQ(std::numeric_limits<TypeParam>::digits,
            stdx::impl::countr_zero_linear(static_cast<TypeParam>(0)));

  for (size_t r = 0; r < std::numeric_limits<TypeParam>::digits; ++r) {
    EXPECT_EQ(r, stdx::impl::countr_zero_linear(
                     static_cast<TypeParam>(static_cast<TypeParam>(1) << r)));
  }
}

TYPED_TEST(StdxBitTest, countl_one) {
  EXPECT_EQ(0, stdx::countl_one(static_cast<TypeParam>(0)));

  TypeParam v = ~static_cast<TypeParam>(0);

  for (size_t r = 0; r < std::numeric_limits<TypeParam>::digits; ++r) {
    EXPECT_EQ(std::numeric_limits<TypeParam>::digits - r, stdx::countl_one(v))
        << v;
    v <<= 1;
  }
}

TYPED_TEST(StdxBitTest, countr_one) {
  EXPECT_EQ(0, stdx::countr_one(static_cast<TypeParam>(0)));

  TypeParam v = ~static_cast<TypeParam>(0);
  for (size_t r = 0; r < std::numeric_limits<TypeParam>::digits; ++r) {
    EXPECT_EQ(std::numeric_limits<TypeParam>::digits - r, stdx::countr_one(v))
        << v;
    v >>= 1;
  }
}

// check that byteswap is really constexpr
static_assert(UINT64_C(0x2200000000000000) ==
              stdx::byteswap(static_cast<uint64_t>(0x22)));
static_assert(UINT32_C(0x22000000) ==
              stdx::byteswap(static_cast<uint32_t>(0x22)));
static_assert(UINT16_C(0x2200) == stdx::byteswap(static_cast<uint16_t>(0x22)));
static_assert(UINT8_C(0x22) == stdx::byteswap(static_cast<uint8_t>(0x22)));

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
