/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifdef _WIN32
// ensure windows.h doesn't expose min() nor max()
#define NOMINMAX
#endif

#include <thread>

#include "gmock/gmock.h"
#include "router_component_test.h"
#include "router_config.h"

Path g_origin_path;

class MockServerCLITest : public RouterComponentTest, public ::testing::Test {
 protected:
  void SetUp() override {
    set_origin(g_origin_path);
    RouterComponentTest::SetUp();
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

int main(int argc, char *argv[]) {
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
