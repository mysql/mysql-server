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

using namespace std::chrono_literals;

class MockServerCLITest : public RouterComponentTest {
 protected:
  TcpPortPool port_pool_;
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
  auto &cmd =
      launch_command(mysql_server_mock_path,
                     std::vector<std::string>{"--version"}, EXIT_SUCCESS, true);

  SCOPED_TRACE("// wait for exit");
  check_exit_code(cmd, EXIT_SUCCESS, 1000ms);  // should be quick, and return 0
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
  auto &cmd =
      launch_command(mysql_server_mock_path, std::vector<std::string>{"--help"},
                     EXIT_SUCCESS, true);

  SCOPED_TRACE("// wait for exit");
  check_exit_code(cmd, EXIT_SUCCESS, 1000ms);  // should be quick, and return 0
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
  auto &cmd = launch_command(mysql_server_mock_path,
                             std::vector<std::string>{"--http-port=65536"},
                             EXIT_FAILURE, true);

  SCOPED_TRACE("// wait for exit");
  check_exit_code(cmd, EXIT_FAILURE,
                  5000ms);  // should be quick, and return failure
  SCOPED_TRACE("// checking stdout contains errormsg");
  EXPECT_THAT(cmd.get_full_output(), ::testing::HasSubstr("was '65536'"));
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
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
