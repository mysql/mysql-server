/*
  Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <thread>

#include "dim.h"
#include "gmock/gmock.h"
#include "mysql/harness/logging/registry.h"
#include "mysql_session.h"
#include "router_component_test.h"
#include "router_config.h"
#include "tcp_port_pool.h"

Path g_origin_path;

class MockServerCLITest : public RouterComponentTest, public ::testing::Test {
 protected:
  TcpPortPool port_pool_;
  void SetUp() override {
    set_origin(g_origin_path);
    RouterComponentTest::init();
  }
};

/**
 * ensure mock-server supports --version.
 *
 * verifies:
 *
 * - WL12118
 *   - TS_1-3
 */
TEST_F(MockServerCLITest, has_version) {
  auto mysql_server_mock_path = get_mysqlserver_mock_exec().str();

  ASSERT_THAT(mysql_server_mock_path, ::testing::StrNe(""));

  SCOPED_TRACE("// start binary");
  auto cmd = launch_command(mysql_server_mock_path,
                            std::vector<std::string>{"--version"}, true);

  SCOPED_TRACE("// wait for exit");
  EXPECT_EQ(cmd.wait_for_exit(1000), 0);  // should be quick, and return 0
  SCOPED_TRACE("// checking stdout");
  EXPECT_THAT(cmd.get_full_output(),
              ::testing::HasSubstr(MYSQL_ROUTER_VERSION));
}

/**
 * ensure mock-server supports --help.
 */
TEST_F(MockServerCLITest, has_help) {
  auto mysql_server_mock_path = get_mysqlserver_mock_exec().str();

  ASSERT_THAT(mysql_server_mock_path, ::testing::StrNe(""));

  SCOPED_TRACE("// start binary with --help");
  auto cmd = launch_command(mysql_server_mock_path,
                            std::vector<std::string>{"--help"}, true);

  SCOPED_TRACE("// wait for exit");
  EXPECT_NO_THROW(
      EXPECT_EQ(cmd.wait_for_exit(1000), 0));  // should be quick, and return 0
  SCOPED_TRACE("// checking stdout contains --version");
  EXPECT_THAT(cmd.get_full_output(), ::testing::HasSubstr("--version"));
}

/**
 * ensure mock-server supports --http-port=65536 fails.
 *
 * verifies:
 *
 * - WL12118
 *   - TS_1-4
 */
TEST_F(MockServerCLITest, http_port_too_large) {
  auto mysql_server_mock_path = get_mysqlserver_mock_exec().str();

  ASSERT_THAT(mysql_server_mock_path, ::testing::StrNe(""));

  SCOPED_TRACE("// start binary with --http-port=65536");
  auto cmd =
      launch_command(mysql_server_mock_path,
                     std::vector<std::string>{"--http-port=65536"}, true);

  SCOPED_TRACE("// wait for exit");
  EXPECT_NO_THROW(EXPECT_NE(cmd.wait_for_exit(1000),
                            0));  // should be quick, and return failure (255)
  SCOPED_TRACE("// checking stdout contains errormsg");
  EXPECT_THAT(cmd.get_full_output(), ::testing::HasSubstr("was '65536'"));
}

/**
 * ensure a sending a statement after no more statements are known by mock leads
 * to proper error.
 */
TEST_F(MockServerCLITest, fail_on_no_more_stmts) {
  auto mysql_server_mock_path = get_mysqlserver_mock_exec().str();

  ASSERT_THAT(mysql_server_mock_path, ::testing::StrNe(""));

  auto server_port = port_pool_.get_next_available();
  const std::string json_stmts =
      get_data_dir().join("js_test_stmts_is_empty.json").str();

  SCOPED_TRACE("// start mock");
  auto server_mock = launch_mysql_server_mock(json_stmts, server_port, false);

  EXPECT_TRUE(wait_for_port_ready(server_port, 1000))
      << server_mock.get_full_output();

  mysqlrouter::MySQLSession client;

  SCOPED_TRACE("// connecting via mysql protocol");
  ASSERT_NO_THROW(
      client.connect("127.0.0.1", server_port, "username", "password", "", ""))
      << server_mock.get_full_output();

  SCOPED_TRACE("// select @@port, should throw");
  ASSERT_THROW_LIKE(client.execute("select @@port"),
                    mysqlrouter::MySQLSession::Error,
                    "Error executing MySQL query: Unexpected stmt, got: "
                    "\"select @@port\"; expected nothing (1064)");
}

static void init_DIM() {
  mysql_harness::DIM &dim = mysql_harness::DIM::instance();

  // logging facility
  dim.set_LoggingRegistry(
      []() {
        static mysql_harness::logging::Registry registry;
        return &registry;
      },
      [](mysql_harness::logging::Registry *) {}  // don't delete our static!
  );
  mysql_harness::logging::Registry &registry = dim.get_LoggingRegistry();

  mysql_harness::logging::create_module_loggers(
      registry, mysql_harness::logging::LogLevel::kWarning,
      {mysql_harness::logging::kMainLogger, "sql"},
      mysql_harness::logging::kMainLogger);
  mysql_harness::logging::create_main_log_handler(registry, "", "", true);

  registry.set_ready();
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  init_DIM();

  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
