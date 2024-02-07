/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include <chrono>

#include <gmock/gmock.h>
#include "router_component_test.h"

using namespace std::chrono_literals;

class RouterUserOptionTest : public RouterComponentTest {};

// --user option is not supported on Windows
#ifndef _WIN32

// check that using --user with no sudo gives a proper error
TEST_F(RouterUserOptionTest, UserOptionNoSudo) {
  auto &router =
      launch_router({"--bootstrap=127.0.0.1:5000", "--user=mysqlrouter"},
                    EXIT_FAILURE, true, false, -1s);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_TRUE(router.expect_output(
      "Error: One can only use the -u/--user switch if running as root"));

  // That's more to test the framework itself:
  // The consecutive calls to exit_code() should be possible  and return the
  // same value
  EXPECT_EQ(router.exit_code(), EXIT_FAILURE);
  EXPECT_EQ(router.exit_code(), EXIT_FAILURE);
}

// check that using --user parameter before --bootstrap gives a proper error
TEST_F(RouterUserOptionTest, UserOptionBeforeBootstrap) {
  auto &router =
      launch_router({"--user=mysqlrouter", "--bootstrap=127.0.0.1:5000"},
                    EXIT_FAILURE, true, false, -1s);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_TRUE(router.expect_output(
      "Error: One can only use the -u/--user switch if running as root"));

  check_exit_code(router, EXIT_FAILURE);
}

#else
// check that it really is not supported on Windows
TEST_F(RouterUserOptionTest, UserOptionOnWindows) {
  auto &router =
      launch_router({"--bootstrap=127.0.0.1:5000", "--user=mysqlrouter"},
                    EXIT_FAILURE, true, false, -1s);

  ASSERT_TRUE(router.expect_output("Error: unknown option '--user'."));
  check_exit_code(router, EXIT_FAILURE);
}
#endif

int main(int argc, char *argv[]) {
  ProcessManager::set_origin(Path(argv[0]).dirname());

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
