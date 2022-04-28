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

/*
 * Test free functions found in utils.cc
 */
#include "mysqlrouter/utils.h"

#include <array>
#include <cstdlib>

#include <gmock/gmock.h>
#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

class SubstituteEnvVarTest : public ::testing::Test {
 public:
  static const std::string env_name;
  static const std::string env_value;

 protected:
  void SetUp() override {
    static std::array<char, 64> env;

    snprintf(env.data(), env.size(), "%s=%s", env_name.c_str(),
             env_value.c_str());
    putenv(env.data());
  }
};

const std::string SubstituteEnvVarTest::env_name{"MYRTEST_ENVAR"};
const std::string SubstituteEnvVarTest::env_value{"MySQLRouterTest"};

struct SubstituteOkParam {
  std::string test_name;

  std::string input;
  std::string expected_output;
};

class SubstituteEnvVarOkTest
    : public SubstituteEnvVarTest,
      public ::testing::WithParamInterface<SubstituteOkParam> {};

TEST_P(SubstituteEnvVarOkTest, check) {
  std::string inout{GetParam().input};
  EXPECT_TRUE(mysqlrouter::substitute_envvar(inout));

  EXPECT_STREQ(GetParam().expected_output.c_str(), inout.c_str());
}

static const SubstituteOkParam substitute_ok_param[] = {
    {"simple", "ENV{" + SubstituteEnvVarTest::env_name + "}",
     SubstituteEnvVarTest::env_value},

    {"simple_middle_of_string",
     "ham/ENV{" + SubstituteEnvVarTest::env_name + "}/spam",
     "ham/" + SubstituteEnvVarTest::env_value + "/spam"},

    {"no_placeholder", "hamspam", "hamspam"},
};

INSTANTIATE_TEST_SUITE_P(Ok, SubstituteEnvVarOkTest,
                         ::testing::ValuesIn(substitute_ok_param),
                         [](auto const &tinfo) {
                           return tinfo.param.test_name;
                         });

struct SubstituteFailParam {
  std::string test_name;

  std::string input;
};

class SubstituteEnvVarFailTest
    : public SubstituteEnvVarTest,
      public ::testing::WithParamInterface<SubstituteFailParam> {};

TEST_P(SubstituteEnvVarFailTest, check) {
  std::string inout{GetParam().input};
  EXPECT_FALSE(mysqlrouter::substitute_envvar(inout));

  EXPECT_STREQ(GetParam().input.c_str(), inout.c_str());
}

static const SubstituteFailParam substitute_fail_param[] = {
    {"unclosed_placeholder", "hamENV{" + SubstituteEnvVarTest::env_name},

    {"empty_variable_name", "ham/ENV{}/spam"},

    {"unknown_envvar", "hamENV{UNKNOWN_VARIABLE_12343xyzYEKfk}"},
};

INSTANTIATE_TEST_SUITE_P(Fail, SubstituteEnvVarFailTest,
                         ::testing::ValuesIn(substitute_fail_param),
                         [](auto const &tinfo) {
                           return tinfo.param.test_name;
                         });

class StringFormatTest : public ::testing::Test {};

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
