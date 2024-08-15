/* Copyright (c) 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <gtest/gtest.h>
#include <string>

#include "sql/join_optimizer/print_utils.h"
#include "unittest/gunit/benchmark.h"

namespace print_utils_unittest {

// Benchmark for formatting 'double' as a decimal numeral.
static void BM_FormatSmallDouble(size_t num_iterations) {
  for (size_t i = 0; i < num_iterations; ++i) {
    FormatNumberReadably(1.1);
  }
}
BENCHMARK(BM_FormatSmallDouble)

// Benchmark for formatting 'double' in engineering notation.
static void BM_FormatBigDouble(size_t num_iterations) {
  for (size_t i = 0; i < num_iterations; ++i) {
    FormatNumberReadably(1.123e17);
  }
}
BENCHMARK(BM_FormatBigDouble)

// Benchmark for formatting 'uit64_t' as a decimal numeral.
static void BM_FormatSmallUint64(size_t num_iterations) {
  for (size_t i = 0; i < num_iterations; ++i) {
    FormatNumberReadably(uint64_t{17});
  }
}
BENCHMARK(BM_FormatSmallUint64)

// Benchmark for formatting 'uit64_t' in engineering notation.
static void BM_FormatBigUint64(size_t num_iterations) {
  for (size_t i = 0; i < num_iterations; ++i) {
    FormatNumberReadably(uint64_t{1234567890});
  }
}
BENCHMARK(BM_FormatBigUint64)

/// Test that numbers are formatted correctly.
TEST(PrintUtilsTest, NumberFormat) {
  EXPECT_EQ(FormatNumberReadably(uint64_t{999999}), "999999");
  EXPECT_EQ(FormatNumberReadably(uint64_t{1000000}), "1e+6");
  EXPECT_EQ(FormatNumberReadably(uint64_t{1234567890}), "1.23e+9");
  EXPECT_EQ(FormatNumberReadably(999999.49), "999999");
  EXPECT_EQ(FormatNumberReadably(999999.51), "1e+6");
  EXPECT_EQ(FormatNumberReadably(-999999.49), "-999999");
  EXPECT_EQ(FormatNumberReadably(-999999.51), "-1e+6");
  EXPECT_EQ(FormatNumberReadably(0.001), "0.001");
  EXPECT_EQ(FormatNumberReadably(-0.001), "-0.001");
  EXPECT_EQ(FormatNumberReadably(0.000999), "999e-6");
  EXPECT_EQ(FormatNumberReadably(-0.000999), "-999e-6");
  EXPECT_EQ(FormatNumberReadably(9.99e-13), "0");
  EXPECT_EQ(FormatNumberReadably(-9.99e-13), "0");
  EXPECT_EQ(FormatNumberReadably(12345678.9), "12.3e+6");
}

}  // namespace print_utils_unittest
