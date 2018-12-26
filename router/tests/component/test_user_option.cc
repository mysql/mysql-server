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

#include "gmock/gmock.h"
#include "router_component_test.h"

Path g_origin_path;

class RouterUserOptionTest : public RouterComponentTest,
                             public ::testing::Test {
 protected:
  virtual void SetUp() {
    set_origin(g_origin_path);
    RouterComponentTest::SetUp();
  }
};

// --user option is not supported on Windows
#ifndef _WIN32

// check that using --user with no sudo gives a proper error
TEST_F(RouterUserOptionTest, UserOptionNoSudo) {
  auto router = launch_router("--bootstrap=127.0.0.1:5000 --user=mysqlrouter");

  EXPECT_EQ(router.wait_for_exit(), 1);
  EXPECT_TRUE(router.expect_output(
      "Error: One can only use the -u/--user switch if running as root"))
      << router.get_full_output();

  // That's more to test the framework itself:
  // The consecutive calls to exit_code() should be possible  and return the
  // same value
  EXPECT_EQ(router.exit_code(), 1);
  EXPECT_EQ(router.exit_code(), 1);
}

// check that using --user parameter before --bootstrap gives a proper error
TEST_F(RouterUserOptionTest, UserOptionBeforeBootstrap) {
  auto router = launch_router("--user=mysqlrouter --bootstrap=127.0.0.1:5000");

  EXPECT_EQ(router.wait_for_exit(), 1);
  EXPECT_TRUE(router.expect_output(
      "Error: One can only use the -u/--user switch if running as root"))
      << router.get_full_output();

  EXPECT_EQ(router.exit_code(), 1);
}

#else
// check that it really is not supported on Windows
TEST_F(RouterUserOptionTest, UserOptionOnWindows) {
  auto router = launch_router("--bootstrap=127.0.0.1:5000 --user=mysqlrouter");

  ASSERT_TRUE(router.expect_output("Error: unknown option '--user'."))
      << router.get_full_output();
  ASSERT_EQ(router.wait_for_exit(), 1);
}
#endif

int main(int argc, char *argv[]) {
  g_origin_path = Path(argv[0]).dirname();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
