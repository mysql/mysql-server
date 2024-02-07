/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <cstdlib>  // getenv
#include <stdexcept>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "mysql/harness/filesystem.h"
#include "mysqlrouter/default_paths.h"

std::string g_program_name;

#ifndef _WIN32
class Environment {
 public:
  static constexpr const std::string_view path_sep{":"};

  static std::string get(const std::string &name) {
    return ::getenv(name.c_str());
  }

  static void set(const std::string &name, const std::string &value) {
    ::setenv(name.c_str(), value.c_str(), 1);
  }

  static void unset(const std::string &name) { ::unsetenv(name.c_str()); }
};
#endif

class DefaultPathTest : public ::testing::Test {
#ifndef _WIN32
 public:
  void SetUp() override { env_path_ = Environment::get("PATH"); }
  void TearDown() override { Environment::set("PATH", env_path_); }

 private:
  std::string env_path_;

 protected:
  std::string path_sep_{Environment::path_sep};
#endif
};

TEST_F(DefaultPathTest, execute_path_of_test) {
  const auto program_name = g_program_name;

  const auto found_path = mysqlrouter::find_full_executable_path(program_name);
  EXPECT_TRUE(mysql_harness::Path(found_path).exists()) << found_path;
}

#ifndef _WIN32
TEST_F(DefaultPathTest, execute_path_of_sh) {
  mysql_harness::Path p(g_program_name);

  Environment::set("PATH", p.dirname().str());

  const auto program_name = p.basename().str();

  const auto found_path = mysqlrouter::find_full_executable_path(program_name);
  EXPECT_TRUE(mysql_harness::Path(found_path).exists()) << found_path;
}

TEST_F(DefaultPathTest, executable_path_via_path_colon_at_start) {
  mysql_harness::Path p(g_program_name);

  Environment::set("PATH", path_sep_ + p.dirname().str());

  const auto program_name = p.basename().str();

  const auto found_path = mysqlrouter::find_full_executable_path(program_name);
  EXPECT_TRUE(mysql_harness::Path(found_path).exists()) << found_path;
}

TEST_F(DefaultPathTest, executable_path_via_path_empty) {
  mysql_harness::Path p(g_program_name);

  Environment::unset("PATH");

  const auto program_name = p.basename().str();

  EXPECT_THROW(mysqlrouter::find_full_executable_path(program_name),
               std::logic_error);
}

TEST_F(DefaultPathTest, executable_path_via_path_colon) {
  mysql_harness::Path p(g_program_name);

  Environment::set("PATH", path_sep_);

  const auto program_name = p.basename().str();

  EXPECT_THROW(mysqlrouter::find_full_executable_path(program_name),
               std::logic_error);
}

TEST_F(DefaultPathTest, executable_path_via_path_not_exists) {
  mysql_harness::Path p(g_program_name);

  Environment::set("PATH", path_sep_ + "does-not-exist" + path_sep_);

  const auto program_name = p.basename().str();

  EXPECT_THROW(mysqlrouter::find_full_executable_path(program_name),
               std::logic_error);
}
#endif

int main(int argc, char **argv) {
  g_program_name = argv[0];

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
