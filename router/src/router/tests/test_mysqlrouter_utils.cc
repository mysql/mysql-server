/*
  Copyright (c) 2015, 2021, Oracle and/or its affiliates.

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

#include <cstdlib>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysql/harness/filesystem.h"
#include "mysqlrouter/utils.h"

using std::string;

using mysqlrouter::substitute_envvar;

using ::testing::ContainerEq;
using ::testing::StrEq;

class SubstituteEnvVarTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static char env[64];

    snprintf(env, sizeof(env), "%s=%s", env_name.c_str(), env_value.c_str());
    putenv(env);
  }
  string env_name{"MYRTEST_ENVAR"};
  string env_value{"MySQLRouterTest"};
};

class StringFormatTest : public ::testing::Test {
 protected:
};

/*! \brief Tests mysqlrouter::substitute_envvar()
 *
 */

TEST_F(SubstituteEnvVarTest, Simple) {
  string exp{env_value};
  string test{"ENV{" + env_name + "}"};
  substitute_envvar(test);
  ASSERT_STREQ(exp.c_str(), test.c_str());
}

TEST_F(SubstituteEnvVarTest, SimpleMiddleOfString) {
  string exp{"ham/" + env_value + "/spam"};
  string test{"ham/ENV{" + env_name + "}/spam"};
  bool ok = substitute_envvar(test);

  ASSERT_TRUE(ok);
  ASSERT_STREQ(exp.c_str(), test.c_str());
}

TEST_F(SubstituteEnvVarTest, NoPlaceholder) {
  string test{"hamspam"};
  bool ok = substitute_envvar(test);  // nothing to do -> ok, just a no-op

  ASSERT_TRUE(ok);
  ASSERT_STREQ("hamspam",
               test.c_str());  // no error, value should be left intact
}

TEST_F(SubstituteEnvVarTest, UnclosedPlaceholder) {
  string test{"hamENV{" + env_name + "spam"};
  bool ok = substitute_envvar(test);

  ASSERT_FALSE(ok);
  // value of test is now undefined
}

TEST_F(SubstituteEnvVarTest, EmptyVariableName) {
  string test{"hamENV{}spam"};
  bool ok = substitute_envvar(test);

  ASSERT_FALSE(ok);
  // value of test is now undefined
}

TEST_F(SubstituteEnvVarTest, UnknownEnvironmentVariable) {
  string unknown_name{"UNKNOWN_VARIABLE_12343xyzYEKfk"};
  string test{"hamENV{" + unknown_name + "}spam"};
  bool ok = substitute_envvar(test);

  ASSERT_FALSE(ok);
  // value of test is now undefined
}

/*
 * Tests mysqlrouter::string_format()
 */
TEST_F(StringFormatTest, Simple) {
  EXPECT_EQ(std::string("5 + 5 = 10"),
            mysqlrouter::string_format("%d + %d = %d", 5, 5, 10));
  EXPECT_EQ(std::string("Spam is 5"),
            mysqlrouter::string_format("%s is %d", "Spam", 5));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
