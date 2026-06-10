// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>         // TEST
#include "mysql/math/int_pow.h"  // int_log_max

/// Requirements
/// - int_pow, int_log_max, and int_log should compute the correct values.

namespace {

using mysql::math::int_log;
using mysql::math::int_log_max;
using mysql::math::int_pow;

TEST(LibsMath, IntPow) {
  ASSERT_EQ(int_pow(10, 0), 1);
  ASSERT_EQ(int_pow(10, 1), 10);
  ASSERT_EQ(int_pow(10, 2), 100);
  ASSERT_EQ(int_pow(10ULL, 10), 10'000'000'000);

  ASSERT_EQ(int_pow(-10, 0), 1);
  ASSERT_EQ(int_pow(-10, 1), -10);
  ASSERT_EQ(int_pow(-10, 2), 100);
  ASSERT_EQ(int_pow(-10LL, 10), 10'000'000'000);
}

TEST(LibsMath, IntLogMax) {
  ASSERT_EQ((int_log_max<uint8_t(2)>()), 7);
  ASSERT_EQ((int_log_max<uint16_t(2)>()), 15);
  ASSERT_EQ((int_log_max<uint32_t(2)>()), 31);
  ASSERT_EQ((int_log_max<uint64_t(2)>()), 63);

  ASSERT_EQ((int_log_max<uint8_t(10)>()), 2);
  ASSERT_EQ((int_log_max<uint16_t(10)>()), 4);
  ASSERT_EQ((int_log_max<uint32_t(10)>()), 9);
  ASSERT_EQ((int_log_max<uint64_t(10)>()), 19);
}

TEST(LibsMath, IntLog) {
  ASSERT_EQ(int_log<10U>(0U), 0);
  ASSERT_EQ(int_log<10U>(1U), 0);
  ASSERT_EQ(int_log<10U>(10U), 1);
  ASSERT_EQ(int_log<10U>(99U), 1);
  ASSERT_EQ(int_log<10U>(100U), 2);
  ASSERT_EQ(int_log<10U>(999U), 2);
  ASSERT_EQ(int_log<10U>(1'000U), 3);
  ASSERT_EQ(int_log<10U>(9'999U), 3);
  ASSERT_EQ(int_log<10U>(10'000U), 4);
  ASSERT_EQ(int_log<10U>(99'999U), 4);
  ASSERT_EQ(int_log<10U>(100'000U), 5);
  ASSERT_EQ(int_log<10U>(999'999U), 5);
  ASSERT_EQ(int_log<10U>(1'000'000U), 6);
  ASSERT_EQ(int_log<10U>(9'999'999U), 6);
  ASSERT_EQ(int_log<10U>(10'000'000U), 7);
  ASSERT_EQ(int_log<10U>(99'999'999U), 7);
  ASSERT_EQ(int_log<10U>(100'000'000U), 8);
  ASSERT_EQ(int_log<10U>(999'999'999U), 8);
  ASSERT_EQ(int_log<10U>(1'000'000'000U), 9);
  ASSERT_EQ(int_log<10ULL>(9'999'999'999ULL), 9);
  ASSERT_EQ(int_log<10ULL>(10'000'000'000ULL), 10);
  ASSERT_EQ(int_log<10ULL>(99'999'999'999ULL), 10);
  ASSERT_EQ(int_log<10ULL>(100'000'000'000ULL), 11);
  ASSERT_EQ(int_log<10ULL>(999'999'999'999ULL), 11);
  ASSERT_EQ(int_log<10ULL>(1'000'000'000'000ULL), 12);
  ASSERT_EQ(int_log<10ULL>(9'999'999'999'999ULL), 12);
  ASSERT_EQ(int_log<10ULL>(10'000'000'000'000ULL), 13);
  ASSERT_EQ(int_log<10ULL>(99'999'999'999'999ULL), 13);
  ASSERT_EQ(int_log<10ULL>(100'000'000'000'000ULL), 14);
  ASSERT_EQ(int_log<10ULL>(999'999'999'999'999ULL), 14);
  ASSERT_EQ(int_log<10ULL>(1'000'000'000'000'000ULL), 15);
  ASSERT_EQ(int_log<10ULL>(9'999'999'999'999'999ULL), 15);
  ASSERT_EQ(int_log<10ULL>(10'000'000'000'000'000ULL), 16);
  ASSERT_EQ(int_log<10ULL>(99'999'999'999'999'999ULL), 16);
  ASSERT_EQ(int_log<10ULL>(100'000'000'000'000'000ULL), 17);
  ASSERT_EQ(int_log<10ULL>(999'999'999'999'999'999ULL), 17);
  ASSERT_EQ(int_log<10ULL>(1'000'000'000'000'000'000ULL), 18);
  ASSERT_EQ(int_log<10ULL>(9'999'999'999'999'999'999ULL), 18);
  ASSERT_EQ(int_log<10ULL>(10'000'000'000'000'000'000ULL), 19);
  ASSERT_EQ(int_log<uint64_t(10)>(std::numeric_limits<uint64_t>::max()), 19);
}

}  // namespace
