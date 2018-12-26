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
#include "gtest_consoleoutput.h"
#include "router_test_helpers.h"

#include <cstring>
#include <sstream>
#include <streambuf>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using mysql_harness::Path;

Path g_origin;
// Path g_stage_dir;
Path g_mysqlrouter_exec;
Path g_source_dir;
bool g_skip_git_tests = false;

const int kFirstYear = 2015;

std::string g_help_output_raw;
std::vector<std::string> g_help_output;

class ConsoleOutputTestX : public ConsoleOutputTest {
 protected:
  virtual void SetUp() {
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
    g_mysqlrouter_exec = app_mysqlrouter.get()->real_path();

    if (g_help_output.empty()) {
      std::ostringstream cmd;
      cmd << g_mysqlrouter_exec << " --help";
      auto result = cmd_exec(cmd.str());
      std::string line;
      std::istringstream iss(result.output);
      while (std::getline(iss, line)) {
        g_help_output.push_back(line);
      }
      g_help_output_raw = result.output;
    }
  }
};

#ifndef _WIN32
// In Windows, the git command is executed in its own shell, it is not available
// in the standard PATH of Windows.
TEST_F(ConsoleOutputTestX, Copyright) {
  SKIP_GIT_TESTS(g_skip_git_tests)
  int last_year = 0;

  // We need year of last commit. This year has to be present in copyright.
  std::ostringstream os_cmd;
  os_cmd << "git log --pretty=format:%ad --date=short -1";
  auto result = cmd_exec(os_cmd.str(), false, g_source_dir.str());
  try {
    last_year = std::stoi(result.output.substr(0, 4));
  } catch (const std::invalid_argument &exc) {
    FAIL() << "Failed getting year using '" << result.output.substr(0, 4)
           << "'";
  }

  for (auto &line : g_help_output) {
    if (starts_with(line, "Copyright")) {
      ASSERT_THAT(line, ::testing::HasSubstr(std::to_string(kFirstYear) + ","))
          << "Start year not in copyright";
      // following is checked only when in Git repository
      if (last_year > kFirstYear) {
        ASSERT_THAT(line, ::testing::HasSubstr(std::to_string(last_year) + ","))
            << "Last year not in copyright";
      }
      break;
    }
  }
}
#endif

TEST_F(ConsoleOutputTestX, Trademark) {
  for (auto &line : g_help_output) {
    if (starts_with(line, "Oracle is a registered trademark of Oracle")) {
      break;
    }
  }
}

TEST_F(ConsoleOutputTestX, ConfigurationFileList) {
  bool found = false;
  std::vector<std::string> config_files;
  std::string indent = "  ";

  for (auto it = g_help_output.begin(); it < g_help_output.end(); ++it) {
    auto line = *it;
    if (found) {
      if (line.empty()) {
        break;
      }
      if (starts_with(line, indent)) {
        auto file = line.substr(indent.size(), line.size());
        config_files.push_back(file);
      }
    }
    if (starts_with(line, "Configuration read")) {
      it++;  // skip next line
      found = true;
    }
  }

  ASSERT_TRUE(found) << "Failed reading location configuration locations";
  ASSERT_TRUE(config_files.size() >= 2)
      << "Failed getting at least 2 configuration file locations";
}

TEST_F(ConsoleOutputTestX, BasicUsage) {
  std::vector<std::string> options{
      "[-V|--version]",
      "[-?|--help]",
      "[-c|--config=<path>]",
      "[-a|--extra-config=<path>]",
  };

  for (auto option : options) {
    ASSERT_THAT(g_help_output_raw, ::testing::HasSubstr(option));
  }
}

TEST_F(ConsoleOutputTestX, BasicOptionDescriptions) {
  std::vector<std::string> options{
      "  -V, --version",
      "        Display version information and exit.",
      "  -?, --help",
      "        Display this help and exit.",
      "  -c <path>, --config <path>",
      "        Only read configuration from given file.",
      "  -a <path>, --extra-config <path>",
      "        Read this file after configuration files are read",
  };

  for (auto option : options) {
    ASSERT_THAT(g_help_output_raw, ::testing::HasSubstr(option));
  }
}

int main(int argc, char *argv[]) {
  g_origin = Path(argv[0]).dirname();
  try {
    g_source_dir = get_cmake_source_dir();
    if (g_source_dir.is_set() &&
        !Path(g_source_dir).join(".git").is_directory()) {
      throw std::runtime_error("no git repository");
    }
  } catch (const std::runtime_error &exc) {
    std::cerr << "WARNING: mysqlrouter source repository not available. "
              << std::endl
              << "Use CMAKE_SOURCE_DIR environment variable to point to source "
                 "repository. "
              << std::endl
              << "Skipping tests using Git." << std::endl;
    g_skip_git_tests = true;
  }

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
