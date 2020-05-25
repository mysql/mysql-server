/*
  Copyright (c) 2020, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/io_context.h"

#include <gmock/gmock.h>

#include "mysql/harness/stdx/expected_ostream.h"

TEST(NetTS_io_context, construct) {
  net::io_context io_ctx;
  EXPECT_FALSE(io_ctx.stopped());
}

TEST(NetTS_io_context, stop) {
  net::io_context io_ctx;
  EXPECT_FALSE(io_ctx.stopped());
  io_ctx.stop();
  EXPECT_TRUE(io_ctx.stopped());
  io_ctx.restart();
  EXPECT_FALSE(io_ctx.stopped());
}

// net::is_executor_v<> chokes with solaris-ld on
//
//   test_net_ts_executor.cc.o: symbol
//   .XAKwqohC_Jhd06z.net::is_executor_v<net::system_executor>:
//   external symbolic relocation against non-allocatable section .debug_info;
//   cannot be processed at runtime: relocation ignored Undefined
//   first referenced symbol
//
//     .XAKwqohC_Jhd06z.net::is_executor_v<net::system_executor>
//
//   in file
//
//     .../test_net_ts_executor.cc.o
//
// using the long variant is_executor<T>::value instead
static_assert(net::is_executor<net::io_context::executor_type>::value,
              "io_context::executor_type MUST be an executor");

int main(int argc, char *argv[]) {
  net::impl::socket::init();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
