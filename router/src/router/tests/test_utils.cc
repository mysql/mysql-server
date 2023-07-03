/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#include <fstream>
#include <stdexcept>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

#include "mysql/harness/filesystem.h"
#include "mysql/harness/string_utils.h"
#include "mysqlrouter/utils.h"  // get_tcp_port

using ::testing::ContainerEq;
using ::testing::Pair;

class GetTCPPortTest : public ::testing::Test {};

TEST_F(GetTCPPortTest, GetTCPPort) {
  using mysqlrouter::get_tcp_port;

  ASSERT_EQ(get_tcp_port("3306"), static_cast<uint16_t>(3306));
  ASSERT_EQ(get_tcp_port("0"), static_cast<uint16_t>(0));
  ASSERT_EQ(get_tcp_port(""), static_cast<uint16_t>(0));
  ASSERT_EQ(get_tcp_port("65535"), 65535);
}

TEST_F(GetTCPPortTest, GetTCPPortFail) {
  using mysqlrouter::get_tcp_port;

  ASSERT_THROW(get_tcp_port("65536"), std::runtime_error);
  ASSERT_THROW(get_tcp_port("33 06"), std::runtime_error);
  ASSERT_THROW(get_tcp_port(":3306"), std::runtime_error);
  ASSERT_THROW(get_tcp_port("99999999"), std::runtime_error);
  ASSERT_THROW(get_tcp_port("abcdef"), std::runtime_error);
}

class UtilsTests : public ::testing::Test {};

static bool files_equal(const std::string &f1, const std::string &f2) {
  std::ifstream if1(f1);
  std::ifstream if2(f2);

  std::istreambuf_iterator<char> s1(if1), s1end;
  std::istreambuf_iterator<char> s2(if1);

  return std::equal(s1, s1end, s2);
}

TEST_F(UtilsTests, copy_file) {
  std::ofstream("empty.tf").close();
  std::ofstream dataf("data.tf");
  for (int i = 0; i < 2000; i++) dataf << "somedata\n";
  dataf.close();

  mysqlrouter::copy_file("empty.tf", "empty.tf2");
  mysqlrouter::copy_file("data.tf", "data.tf2");

  try {
    EXPECT_TRUE(files_equal("empty.tf", "empty.tf2"));
    EXPECT_TRUE(files_equal("data.tf", "data.tf2"));
  } catch (...) {
    mysql_harness::delete_file("empty.tf");
    mysql_harness::delete_file("empty.tf2");
    mysql_harness::delete_file("data.tf");
    mysql_harness::delete_file("data.tf2");
    throw;
  }
  mysql_harness::delete_file("empty.tf");
  mysql_harness::delete_file("empty.tf2");
  mysql_harness::delete_file("data.tf");
  mysql_harness::delete_file("data.tf2");
}

template <typename FUNC>
static void test_int_conv_common(FUNC func) noexcept {
  // when func = strtoui_checked(), EXPECT_EQ() throws -Wsign-Compare warnings
  // because "66" is a signed literal. To work around this, we cast the literal
  // to whatever (signedness) type it needs to be.
  typedef decltype(func("", 0)) OUT_TYPE;
#define EXPECT_EQ2(a, b) EXPECT_EQ(static_cast<OUT_TYPE>(a), (b))

  // bad input tests
  EXPECT_EQ2(66, func("", 66));
  EXPECT_EQ2(66, func(nullptr, 66));
  EXPECT_EQ2(66, func("bad", 66));
  EXPECT_EQ2(0, func("bad", 0));

  // bad input: no sign
  EXPECT_EQ2(66, func("1bad1", 66));
  EXPECT_EQ2(66, func("12.345", 66));
  EXPECT_EQ2(66, func("12.0", 66));
  EXPECT_EQ2(66, func("  12", 66));
  EXPECT_EQ2(66, func(" 12 ", 66));
  EXPECT_EQ2(66, func(" 12", 66));
  EXPECT_EQ2(66, func("12 ", 66));
  EXPECT_EQ2(66, func("1 2", 66));

  // tabs instead of spaces
  EXPECT_EQ2(66, func("\t\t12", 66));
  EXPECT_EQ2(66, func("\t12\t", 66));
  EXPECT_EQ2(66, func("\t12", 66));
  EXPECT_EQ2(66, func("12\t", 66));
  EXPECT_EQ2(66, func("1\t2", 66));

  // bad input: - sign
  EXPECT_EQ2(66, func("-12.345", 66));
  EXPECT_EQ2(66, func("-12.0", 66));
  EXPECT_EQ2(66, func("  -12", 66));
  EXPECT_EQ2(66, func(" -12 ", 66));
  EXPECT_EQ2(66, func(" -12", 66));
  EXPECT_EQ2(66, func("-12 ", 66));
  EXPECT_EQ2(66, func("-1 2", 66));
  EXPECT_EQ2(66, func("- 12", 66));

  // bad input: + sign
  EXPECT_EQ2(66, func("+12.345", 66));
  EXPECT_EQ2(66, func("+12.0", 66));
  EXPECT_EQ2(66, func("  +12", 66));
  EXPECT_EQ2(66, func(" +12 ", 66));
  EXPECT_EQ2(66, func(" +12", 66));
  EXPECT_EQ2(66, func("+12 ", 66));
  EXPECT_EQ2(66, func("+1 2", 66));
  EXPECT_EQ2(66, func("+ 12", 66));

  // bad input: both signs
  EXPECT_EQ2(66, func("-+12", 66));
  EXPECT_EQ2(66, func("+-12", 66));

  // verify erno is preserved
  auto saved_errno = errno;
  errno = 123;
  EXPECT_EQ2(12, func("12", 66));
  EXPECT_EQ2(66, func("bad", 66));
  EXPECT_EQ(123, errno);
  errno = saved_errno;

#undef EXPECT_EQ2
}

TEST_F(UtilsTests, int_conversion) {
  using mysqlrouter::strtoi_checked;

  test_int_conv_common(strtoi_checked);

  // range tests: no sign
  EXPECT_EQ(12, strtoi_checked("12", 66));
  EXPECT_EQ(66, strtoi_checked("66", 66));
  EXPECT_EQ(0, strtoi_checked("0", 66));
  EXPECT_EQ(INT_MAX, strtoi_checked(std::to_string(INT_MAX).c_str(), 66));
  EXPECT_EQ(INT_MIN, strtoi_checked(std::to_string(INT_MIN).c_str(), 66));
  EXPECT_EQ(66,
            strtoi_checked(std::to_string((long long)INT_MAX + 1).c_str(), 66));
  EXPECT_EQ(66,
            strtoi_checked(std::to_string((long long)INT_MIN - 1).c_str(), 66));
  EXPECT_EQ(66, strtoi_checked(
                    std::to_string(100ll * (long long)INT_MAX).c_str(), 66));

  // - sign
  EXPECT_EQ(-12, strtoi_checked("-12", 66));
  EXPECT_EQ(0, strtoi_checked("-0", 66));

  // extra + sign
  EXPECT_EQ(12, strtoi_checked("+12", 66));
  EXPECT_EQ(0, strtoi_checked("+0", 66));
}

TEST_F(UtilsTests, uint_conversion) {
  using mysqlrouter::strtoui_checked;

  test_int_conv_common(strtoui_checked);

  // range tests
  EXPECT_EQ(12u, strtoui_checked("12", 66));
  EXPECT_EQ(66u, strtoui_checked("66", 66));
  EXPECT_EQ(0u, strtoui_checked("0", 66));
  EXPECT_EQ(UINT_MAX, strtoui_checked(std::to_string(UINT_MAX).c_str(), 66));
  EXPECT_EQ(66u, strtoui_checked(
                     std::to_string((long long)UINT_MAX + 1).c_str(), 66));
  EXPECT_EQ(66u, strtoui_checked("-1", 66));
  EXPECT_EQ(66u, strtoui_checked(
                     std::to_string(100ll * (long long)UINT_MAX).c_str(), 66));

  // extra + sign
  EXPECT_EQ(12u, strtoui_checked("+12", 66));
  EXPECT_EQ(0u, strtoui_checked("+0", 66));
}

TEST_F(UtilsTests, uint64_conversion) {
  using mysqlrouter::strtoull_checked;
  const uint64_t kDefault{66};

  EXPECT_EQ(kDefault, strtoull_checked(nullptr, kDefault));
  EXPECT_EQ(kDefault, strtoull_checked(nullptr, kDefault));
  EXPECT_EQ(kDefault, strtoull_checked("18446744073709551617", kDefault));

  EXPECT_EQ(static_cast<uint64_t>(0), strtoull_checked("0", kDefault));
  EXPECT_EQ(static_cast<uint64_t>(4294967298),
            strtoull_checked("4294967298", kDefault));
  EXPECT_EQ(static_cast<uint64_t>(0x7fffffffffffffff),
            strtoull_checked("9223372036854775807", kDefault));
  EXPECT_EQ(static_cast<uint64_t>(66), strtoull_checked("66", kDefault));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
