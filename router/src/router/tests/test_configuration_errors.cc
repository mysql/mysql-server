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

/**
 * BUG21771595 Exit application on configuration errors
 *
 */

#include "cmd_exec.h"
#include "gtest_consoleoutput.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/plugin.h"
#include "router_app.h"
#include "router_test_helpers.h"
#include "test/helpers.h"

#include <fstream>
#include <memory>
#include <string>
#ifdef _WIN32
#include <WinSock2.h>
#endif
#include "gmock/gmock.h"

using ::testing::HasSubstr;
using ::testing::StrEq;
using mysql_harness::Path;
using std::string;

string g_cwd;
Path g_origin;

class Bug21771595 : public ConsoleOutputTest {
 protected:
  virtual void SetUp() {
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
    config_path.reset(new Path(*config_dir));
    config_path->append("Bug21771595.conf");
  }

  void reset_config() {
    std::ofstream ofs_config(config_path->str());
    if (ofs_config.good()) {
      ofs_config << "[DEFAULT]\n";
      ofs_config << "logging_folder =\n";
      ofs_config << "plugin_folder = " << plugin_dir->str() << "\n";
      ofs_config << "runtime_folder = " << config_dir->str() << "\n";
      ofs_config << "config_folder = " << config_dir->str() << "\n\n";

      ofs_config.close();
    }
  }

  std::unique_ptr<Path> config_path;
};

TEST_F(Bug21771595, ExceptionRoutingInvalidTimeout) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nbind_address=127.0.0.1:7001\ndestinations=127.0.0.1:"
       "3306\nmode=read-only\n";
  c << "connect_timeout=0\n";
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});

  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
                    "option connect_timeout in [routing] needs value between 1 "
                    "and 65535 inclusive, was '0'");
}

TEST_F(Bug21771595, ExceptionMetadataCacheInvalidBindAddress) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[metadata_cache]\nbootstrap_server_addresses=mysql://"
       "127.0.0.1:13000,mysql://127.0.0.1:99999\n\n";
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
                    "option bootstrap_server_addresses in [metadata_cache] is "
                    "incorrect (invalid URI: invalid port: impossible port "
                    "number for: mysql://127.0.0.1:99999)");
}

TEST_F(Bug21771595, AppExecRoutingInvalidTimeout) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nbind_address=127.0.0.1:7001\ndestinations=127.0.0.1:"
       "3306\nmode=read-only\n";
  c << "connect_timeout=0\n";
  c.close();
  string cmd = app_mysqlrouter->str() + " -c " + config_path->str();
  auto cmd_result = cmd_exec(cmd, true, "");

  ASSERT_EQ(1, cmd_result.exit_code);
  ASSERT_THAT(
      cmd_result.output,
      HasSubstr("Configuration error: option connect_timeout in [routing] "
                "needs value between 1 and 65535 inclusive, was '0'"));
}

TEST_F(Bug21771595, AppExecMetadataCacheInvalidBindAddress) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[metadata_cache]\nbootstrap_server_addresses=mysql://"
       "127.0.0.1:13000,mysql://127.0.0.1:99999\n\n";
  c.close();
  string cmd = app_mysqlrouter->str() + " -c " + config_path->str();
  auto cmd_result = cmd_exec(cmd, true);

  // ASSERT_EQ(cmd_result.exit_code, 1);
  ASSERT_THAT(
      cmd_result.output,
      HasSubstr(
          "option bootstrap_server_addresses in [metadata_cache] is incorrect "
          "(invalid URI: invalid port: impossible port number"));
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin = Path(argv[0]).dirname();
  g_cwd = Path(argv[0]).dirname().str();
  register_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
