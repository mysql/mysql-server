/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include "router_app.h"

#include <fstream>

#include "gmock/gmock.h"

using ::testing::HasSubstr;
using ::testing::StrEq;
using mysql_harness::Path;
using std::string;

string g_cwd;
Path g_origin;

class PluginsConfigTest : public ConsoleOutputTest {
 protected:
  virtual void SetUp() {
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
    config_path.reset(new Path(g_cwd));
    config_path->append("PluginsConfigTest.conf");
  }

  void reset_config() {
    std::ofstream ofs_config(config_path->str());
    if (ofs_config.good()) {
      ofs_config << "[DEFAULT]\n";
      ofs_config << "logging_folder =\n";
      ofs_config << "plugin_folder = " << plugin_dir->str() << "\n";
      ofs_config << "runtime_folder = " << temp_dir->str() << "\n";
      ofs_config << "config_folder = " << temp_dir->str() << "\n\n";
      ofs_config.close();
    }
  }

  std::unique_ptr<Path> config_path;
};

TEST_F(PluginsConfigTest, NoPluginLoaded) {
  reset_config();

  string cmd = app_mysqlrouter->str() + " -c " + config_path->str();
  auto cmd_result = cmd_exec(cmd, true);

  ASSERT_THAT(cmd_result.output,
              HasSubstr("Error: MySQL Router not configured to load or start "
                        "any plugin. Exiting."));

  EXPECT_EQ(1, cmd_result.exit_code);
}

TEST_F(PluginsConfigTest, OnePluginLoaded) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[magic]\n\n";  // any plugin would do
  c.close();

  string cmd = app_mysqlrouter->str() + " -c " + config_path->str();
  auto cmd_result = cmd_exec(cmd, true);

  EXPECT_EQ(0, cmd_result.exit_code);
}

TEST_F(PluginsConfigTest, TwoMetadadaCacheSections) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[metadata_cache:one]\n\n";
  c << "[metadata_cache:two]\n\n";
  c.close();

  string cmd = app_mysqlrouter->str() + " -c " + config_path->str();
  auto cmd_result = cmd_exec(cmd, true);

  ASSERT_THAT(
      cmd_result.output,
      HasSubstr(
          "MySQL Router currently supports only one metadata_cache instance."));
}

TEST_F(PluginsConfigTest, SingleMetadataChacheSection) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[metadata_cache:one]\n\n";
  c.close();

  string cmd = app_mysqlrouter->str() + " -c " + config_path->str();
  auto cmd_result = cmd_exec(cmd, true);

  // shoule be ok but complain about missing user option
  ASSERT_THAT(cmd_result.output,
              HasSubstr("option user in [metadata_cache:one] is required"));
}

int main(int argc, char *argv[]) {
  g_origin = Path(argv[0]).dirname();
  g_cwd = Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
