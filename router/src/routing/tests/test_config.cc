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

#include "../../router/src/router_app.h"
#include "../../routing/src/mysql_routing.h"
#include "../../routing/src/utils.h"
#include "cmd_exec.h"
#include "gtest_consoleoutput.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/plugin.h"
#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/routing.h"
#include "router_test_helpers.h"
#include "test/helpers.h"

#include <cstdio>
#include <fstream>
#include <future>
#include <memory>
#include <string>
#include <thread>
#ifndef _WIN32
#include <netinet/in.h>
#else
#include <WinSock2.h>
#endif

#include "gmock/gmock.h"

using ::testing::ContainerEq;
using ::testing::HasSubstr;
using ::testing::StrEq;
using mysql_harness::Path;
using std::string;

string g_cwd;
Path g_origin;

// Used in tests; does not change for each test.
const string kDefaultRoutingConfig =
    "\ndestinations=127.0.0.1:3306\nrouting_strategy=first-available\n";
const string kDefaultRoutingConfigStrategy =
    "\ndestinations=127.0.0.1:3306\nbind_address=127.0.0.1\nbind_port=6000\n";

class TestConfig : public ConsoleOutputTest {
 protected:
  virtual void SetUp() {
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
    config_path.reset(new Path(*config_dir));
    config_path->append("Bug22020088.conf");
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

TEST_F(TestConfig, NoDestination) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\n";
  c << kDefaultRoutingConfig;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(
      r.start(), std::invalid_argument,
      "either bind_address or socket option needs to be supplied, or both");
}

TEST_F(TestConfig, MissingPortInBindAddress) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nbind_address=127.0.0.1\n";
  c << kDefaultRoutingConfig;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(
      r.start(), std::invalid_argument,
      "either bind_address or socket option needs to be supplied, or both");
}

TEST_F(TestConfig, InvalidPortInBindAddress) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nbind_address=127.0.0.1:999292\n";
  c << kDefaultRoutingConfig;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
                    "option bind_address in [routing] is incorrect (invalid "
                    "TCP port: invalid characters or too long)");
}

TEST_F(TestConfig, InvalidDefaultPort) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nbind_port=23123124123123\n";
  c << kDefaultRoutingConfig;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
                    "option bind_port in [routing] needs value between 1 and "
                    "65535 inclusive, was '23123124123123'");
}

TEST_F(TestConfig, InvalidMode) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nmode=invalid";
  c << kDefaultRoutingConfigStrategy;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
                    "option mode in [routing] is invalid; valid are read-write "
                    "and read-only (was 'invalid')");
}

TEST_F(TestConfig, InvalidStrategyOption) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nrouting_strategy=invalid";
  c << kDefaultRoutingConfigStrategy;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
                    "option routing_strategy in [routing] is invalid; valid "
                    "are first-available, "
                    "next-available, and round-robin (was 'invalid')");
}

TEST_F(TestConfig, EmptyStrategyOption) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nrouting_strategy=";
  c << kDefaultRoutingConfigStrategy;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
                    "option routing_strategy in [routing] needs a value");
}

TEST_F(TestConfig, EmptyMode) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nmode=";
  c << kDefaultRoutingConfigStrategy;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
                    "option mode in [routing] needs a value");
}

TEST_F(TestConfig, NoStrategyOptionAndNoMode) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\n";
  c << kDefaultRoutingConfigStrategy;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument,
                    "option routing_strategy in [routing] is required");
}

TEST_F(TestConfig, UnsupportedStrategyOption) {
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[routing]\nrouting_strategy=round-robin-with-fallback";
  c << kDefaultRoutingConfigStrategy;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(
      r.start(), std::invalid_argument,
      "option routing_strategy in [routing] is invalid; valid are "
      "first-available, "
      "next-available, and round-robin (was 'round-robin-with-fallback')");
}

struct ThreadStackSizeInfo {
  std::string thread_stack_size;
  std::string message;
};

class TestConfigThreadStackSize
    : public ConsoleOutputTest,
      public testing::WithParamInterface<ThreadStackSizeInfo> {
 public:
  void SetUp() override {
    set_origin(g_origin);
    ConsoleOutputTest::SetUp();
    config_path.reset(new Path(g_cwd));
    config_path->append("mysqlrouter.conf");
  }

  void reset_config() {
    std::ofstream ofs_config(config_path->str());
    if (ofs_config.good()) {
      ofs_config << "[DEFAULT]\n";
      ofs_config << "logging_folder =\n";
      ofs_config << "plugin_folder = " << plugin_dir->str() << "\n";
      ofs_config << "runtime_folder = " << temp_dir->str() << "\n";
      ofs_config << "config_folder = " << config_dir->str() << "\n\n";
      ofs_config.close();
    }
  }

  std::unique_ptr<Path> config_path;
};

ThreadStackSizeInfo test_data[] = {
    {"-1",
     "option thread_stack_size in [default] needs value between 1 and 65535 "
     "inclusive, was '-1'"},
    {"4.5",
     "option thread_stack_size in [default] needs value between 1 and 65535 "
     "inclusive, was '4.5'"},
    {"dfs4",
     "option thread_stack_size in [default] needs value between 1 and 65535 "
     "inclusive, was 'dfs4'"}};

TEST_P(TestConfigThreadStackSize, ParseThreadStackSize) {
  ThreadStackSizeInfo input = GetParam();
  reset_config();
  std::ofstream c(config_path->str(), std::fstream::app | std::fstream::out);
  c << "[DEFAULT]\nthread_stack_size=";
  c << input.thread_stack_size << "\n[routing]\nrouting_strategy=round-robin\n";
  c << kDefaultRoutingConfigStrategy;
  c.close();

  MySQLRouter r(g_origin, {"-c", config_path->str()});
  ASSERT_THROW_LIKE(r.start(), std::invalid_argument, input.message);
}

INSTANTIATE_TEST_CASE_P(ConfigThreadStackSizeTests, TestConfigThreadStackSize,
                        ::testing::ValuesIn(test_data));

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin = Path(argv[0]).dirname();
  g_cwd = Path(argv[0]).dirname().str();
  register_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
