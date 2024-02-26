/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/impl/io_service_base.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(FdEvent, eq) {
  EXPECT_EQ(net::fd_event(1, POLLIN), net::fd_event(1, POLLIN));
}

TEST(FdEvent, ne) {
  EXPECT_NE(net::fd_event(1, POLLIN), net::fd_event(1, POLLOUT));
  EXPECT_NE(net::fd_event(1, POLLIN), net::fd_event(2, POLLIN));
  EXPECT_NE(net::fd_event(1, POLLIN), net::fd_event(2, POLLOUT));
}

TEST(FdEvent, emplace) {
  std::vector<net::fd_event> v;

  v.emplace_back(1, POLLIN);
  v.push_back({1, POLLIN});
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
