/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include <stdexcept>

#include <gmock/gmock.h>

#include "../../routing/src/mysql_routing.h"
#include "router_test_helpers.h"
#include "test/helpers.h"

class TestSetupNamedSocketService : public ::testing::Test {};

#ifndef _WIN32  // named sockets are not supported on Windows;
                // on Unix, they're implemented using Unix sockets

TEST_F(TestSetupNamedSocketService, unix_socket_permissions_failure) {
  /**
   * @test Verify that failure while setting unix socket permissions throws
   * correctly
   */

  EXPECT_THROW_LIKE(MySQLRouting::set_unix_socket_permissions("/no/such/file"),
                    std::runtime_error,
                    "Failed setting file permissions on socket file "
                    "'/no/such/file': No such file or directory");
}
#endif  // #ifndef _WIN32

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  init_test_logger();
  return RUN_ALL_TESTS();
}
