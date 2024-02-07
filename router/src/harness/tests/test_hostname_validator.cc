/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "hostname_validator.h"

#include <gmock/gmock.h>
#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

/**
 * @file
 * @brief Unit tests for simple hostname validator
 */

struct ValidatorParam {
  const char *test_name;
  std::string name;

  bool is_host_name;
  bool is_domain_name;
};

class ValidatorTest : public ::testing::Test,
                      public ::testing::WithParamInterface<ValidatorParam> {};

/**
 * @test verify that is_valid_hostname() returns true for valid hostnames
 */
TEST_P(ValidatorTest, is_host_name) {
  EXPECT_EQ(mysql_harness::is_valid_hostname(GetParam().name),
            GetParam().is_host_name)
      << GetParam().name;
}

TEST_P(ValidatorTest, is_domain_name) {
  EXPECT_EQ(mysql_harness::is_valid_domainname(GetParam().name),
            GetParam().is_domain_name)
      << GetParam().name;
}

const ValidatorParam validator_param[] = {
    {"one_part_lowercase",  //
     "foo", true, true},
    {"one_part_uppercase",  //
     "FOO", true, true},
    {"with_dot",  //
     "foo.BAR", true, true},
    {"with_dash",  //
     "foo-BAR", true, true},
    {"ipv4",  //
     "1.2.3.4", true, true},
    {"ipv6",  //
     "::1", false, true},
    {"ipv6_scope",  //
     "::1%1", false, true},
    {"one_lowercase",  //
     "x", true, true},
    {"one_uppercase",  //
     "X", true, true},
    {"leading_digits",  //
     "foo.bar.1.3", true, true},
    {"empty",  //
     "", false, false},
    {"space",  //
     " ", false, true},
    {"parts_with_space",  //
     "foo bar", false, true},
    {"caret",  //
     "^", false, true},
    {"parts_with_caret",  //
     "foo^bar", false, true},
    {"leading_dot",  //
     ".foo", false, false},
    {"trailing_dot",  // invalid hostname, but valid domain name.
     "foo.", false, true},
    {"leading_and_trailing_dot",  //
     ".foo.bar.", false, false},
    {"dot",  // DNS root, not a valid domainname
     ".", false, false},
    {"dotdot",  // label too short
     "..", false, false},
    {"dotdot_start",  //
     "..start", false, false},
    {"dotdot_end",  //
     "start..", false, false},
    {"dotdot_middle",  //
     "start..end", false, false},
    {"dash",  //
     "-", false, true},
    {"underscore",  //
     "1_2-3.4", false, true},
    {"label_63_chars",  //
     "a123456789"       // 0
     "a123456789"       // 1
     "a123456789"       // 2
     "a123456789"       // 3
     "a123456789"       // 4
     "a123456789"       // 5
     "a12",             // 6
     true, true},
    {"label_63_chars_multi",  //
     "a.a123456789"           // 0
     "a123456789"             // 1
     "a123456789"             // 2
     "a123456789"             // 3
     "a123456789"             // 4
     "a123456789"             // 5
     "a12",                   // 6
     true, true},
    {"label_too_long",  //
     "a123456789"       // 0
     "a123456789"       // 1
     "a123456789"       // 2
     "a123456789"       // 3
     "a123456789"       // 4
     "a123456789"       // 5
     "a123",            // 6
     false, false},
    {"name_max",                         //
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."  // 32
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."  // 64
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."  //
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."  // 128
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."  //
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."  //
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."  //
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a",  // 255
     true, true},
    {"name_too_long",                     //
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."   // 32
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."   // 64
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."   //
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."   // 128
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."   //
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."   //
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.a."   //
     "a.a.a.a.a.a.a.a.a.a.a.a.a.a.a.aa",  // 256
     false, false},
};

INSTANTIATE_TEST_SUITE_P(Spec, ValidatorTest,
                         ::testing::ValuesIn(validator_param),
                         [](auto const &info) { return info.param.test_name; });

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
