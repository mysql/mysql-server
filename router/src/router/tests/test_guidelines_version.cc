/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
#include "mysqlrouter/routing_guidelines_version.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

struct ValidVersionTestInput {
  std::string version_string;
  uint32_t expected_major;
  uint32_t expected_minor;
};

class ValidVersionValues
    : public ::testing::Test,
      public ::testing::WithParamInterface<ValidVersionTestInput> {};

TEST_P(ValidVersionValues, version_test) {
  mysqlrouter::RoutingGuidelinesVersion parsed_version =
      mysqlrouter::routing_guidelines_version_from_string(
          GetParam().version_string);

  EXPECT_EQ(parsed_version.major, GetParam().expected_major);
  EXPECT_EQ(parsed_version.minor, GetParam().expected_minor);
  EXPECT_EQ(parsed_version.patch, 0);
  EXPECT_EQ(mysqlrouter::to_string(parsed_version), GetParam().version_string);
}

INSTANTIATE_TEST_SUITE_P(
    ValidVersionValuesTest, ValidVersionValues,
    ::testing::Values(ValidVersionTestInput{"1.0", 1, 0},
                      ValidVersionTestInput{"0.9", 0, 9},
                      ValidVersionTestInput{"1.3", 1, 3},
                      ValidVersionTestInput{"72.3", 72, 3},
                      ValidVersionTestInput{"5.84", 5, 84},
                      ValidVersionTestInput{"11.88", 11, 88},
                      ValidVersionTestInput{"190.5", 190, 5}));

class InvalidVersionValues : public ::testing::Test,
                             public ::testing::WithParamInterface<std::string> {
};

TEST_P(InvalidVersionValues, invalid_version_test) {
  ASSERT_THROW(mysqlrouter::routing_guidelines_version_from_string(GetParam()),
               std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(InvalidVersionValuesTest, InvalidVersionValues,
                         ::testing::Values(".1", "1.1o", "v2.4", "1.o", "2,2",
                                           "3.", "foo.1", "bar.9", "-1.9",
                                           "1.-9"));

struct CompareVersionsInput {
  mysqlrouter::RoutingGuidelinesVersion supported;
  mysqlrouter::RoutingGuidelinesVersion available;
  bool result;
};

class CompareVersions
    : public ::testing::Test,
      public ::testing::WithParamInterface<CompareVersionsInput> {};

TEST_P(CompareVersions, compare_versions) {
  const mysqlrouter::RoutingGuidelinesVersion supported = GetParam().supported;
  const mysqlrouter::RoutingGuidelinesVersion available = GetParam().available;

  EXPECT_EQ(GetParam().result,
            mysqlrouter::routing_guidelines_version_is_compatible(supported,
                                                                  available));
}

INSTANTIATE_TEST_SUITE_P(
    CompareVersionsTest, CompareVersions,
    ::testing::Values(
        CompareVersionsInput{{1, 0, 0},
                             {2, 0, 0},
                             false},  // available is greated than supported
        CompareVersionsInput{{1, 0, 0},
                             {1, 1, 0},
                             false},  // available is greated than supported
        CompareVersionsInput{{1, 0, 0},
                             {1, 0, 1},
                             false},  // available is greated than supported
        CompareVersionsInput{
            {5, 0, 0},
            {3, 9, 9},
            false},  // difference between major versions is > 1
        CompareVersionsInput{
            {4, 1, 3},
            {2, 2, 1},
            false},  // difference between major versions is > 1
        CompareVersionsInput{
            {4, 1, 3},
            {3, 2, 1},
            true},  // difference between major versions is less or equal to one
        CompareVersionsInput{
            {4, 1, 3},
            {4, 0, 1},
            true},  // difference between major versions is less or equal to one
        CompareVersionsInput{
            {4, 1, 3},
            {4, 1, 2},
            true},  // difference between major versions is less or equal to one
        CompareVersionsInput{{2, 0, 0}, {2, 0, 0}, true},       // equal
        CompareVersionsInput{{12, 13, 14}, {12, 13, 14}, true}  // equal
        ));

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
