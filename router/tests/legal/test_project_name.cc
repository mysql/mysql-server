/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "cmd_exec.h"
#include "mysql/harness/filesystem.h"
#include "router_test_helpers.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "gmock/gmock.h"

using mysql_harness::Path;

Path g_origin;
Path g_source_dir;
const std::string kProjectName = "MySQL Router";
const std::string kProjectTarget = "mysqlrouter";

class CheckProjectName : public ::testing::Test {
 protected:
  virtual void SetUp() {}

  virtual void TearDown() {}
};

TEST_F(CheckProjectName, CheckREADMEtxt) {
  Path readme = g_source_dir.join("README.txt");
  std::ifstream curr_file(readme.str());
  std::string line;

  // First line of README.txt starts with the project name
  std::getline(curr_file, line, '\n');
  ASSERT_THAT(line, ::testing::StartsWith(kProjectName));

  while (std::getline(curr_file, line, '\n')) {
    if (line.find("This is a release of") != std::string::npos) {
      ASSERT_THAT(line, ::testing::HasSubstr(kProjectName))
          << "Project name not in 'release of'-line";
    } else if (line.find("brought to you by Oracle") != std::string::npos) {
      ASSERT_THAT(line, ::testing::HasSubstr(kProjectName))
          << "Project name not in 'brought by'-line";
    }
  }
  curr_file.close();
}

TEST_F(CheckProjectName, SettingsCmake) {
  Path settings_cmake = g_source_dir.join("cmake").join("settings.cmake");
  std::ifstream curr_file(settings_cmake.str());
  std::string line;

  int found = 0;
  while (std::getline(curr_file, line, '\n')) {
    if (line.find("SET(MYSQL_ROUTER_NAME") != std::string::npos) {
      ASSERT_THAT(line, ::testing::HasSubstr(kProjectName))
          << "Project name not set correctly in cmake/settings.cmake";
      ++found;
    } else if (line.find("SET(MYSQL_ROUTER_TARGET") != std::string::npos) {
      ASSERT_THAT(line, ::testing::HasSubstr(kProjectTarget))
          << "Project target not set correctly in cmake/settings.cmake";
      ++found;
    }
  }
  curr_file.close();
  ASSERT_EQ(found, 2) << "Failed checking project name in cmake/settings.cmake";
}

int main(int argc, char *argv[]) {
  g_origin = Path(argv[0]).dirname();
  g_source_dir = get_cmake_source_dir();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
