/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/socket.h"

#include <system_error>

#include <gmock/gmock.h>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"

// transfer_all sets a buffer size that's reasonable large
// ... 0 if ec was not "success"
TEST(NetTS_socket, transfer_all) {
  net::io_context io_ctx;

  net::transfer_all compl_condition;
  std::error_code ec{};

  // size is ignored
  EXPECT_GT(compl_condition(ec, 0), 0);
  EXPECT_GT(compl_condition(ec, 26), 0);
  EXPECT_GT(compl_condition(ec, 16), 0);

  ec = make_error_code(std::errc::bad_file_descriptor);
  EXPECT_EQ(compl_condition(ec, 0), 0);
}

// transfer_at_least() to continue reading until set least n bytes are received,
// or n bytes set sent
// ... 0 if ec was not "success"
TEST(NetTS_socket, transfer_at_least) {
  net::io_context io_ctx;

  net::transfer_at_least compl_condition(16);
  std::error_code ec{};

  //
  EXPECT_GT(compl_condition(ec, 1), 0);
  EXPECT_GT(compl_condition(ec, 15), 0);
  // no need to transfer more
  EXPECT_EQ(compl_condition(ec, 200), 0);

  // trigger an error
  ec = make_error_code(std::errc::bad_file_descriptor);
  EXPECT_EQ(compl_condition(ec, 0), 0);
}

// transfer_exactly()
// ... 0 if ec was not "success"
TEST(NetTS_socket, transfer_exactly) {
  net::io_context io_ctx;

  net::transfer_exactly compl_condition(256);
  std::error_code ec{};

  EXPECT_EQ(compl_condition(ec, 1), 256 - 1);
  EXPECT_EQ(compl_condition(ec, 15), 256 - 15);

  // too much
  EXPECT_EQ(compl_condition(ec, 512), 0);

  // trigger an error
  ec = make_error_code(std::errc::bad_file_descriptor);
  EXPECT_EQ(compl_condition(ec, 0), 0);
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  net::impl::socket::init();

  return RUN_ALL_TESTS();
}
