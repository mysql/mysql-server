/*
  Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/net_ts/timer.h"

#include <gmock/gmock.h>

#include "mysql/harness/stdx/expected_ostream.h"

TEST(NetTS_timer, timer_default_construct) {
  net::io_context io_ctx;
  net::system_timer timer(io_ctx);

  EXPECT_EQ(timer.expiry(), std::chrono::system_clock::time_point{});
}

TEST(NetTS_timer, timer_expires_after) {
  net::io_context io_ctx;
  net::system_timer timer(io_ctx);

  using namespace std::chrono_literals;
  const auto wait_duration = 100ms;

  timer.expires_after(wait_duration);
  auto before = std::chrono::system_clock::now();
  timer.wait();

  EXPECT_GT(std::chrono::system_clock::now() - before, wait_duration);
}

TEST(NetTS_timer, timer_expires_at) {
  net::io_context io_ctx;
  net::system_timer timer(io_ctx);

  using namespace std::chrono_literals;
  const auto wait_duration = 100ms;

  timer.expires_at(std::chrono::system_clock::now() + wait_duration);
  auto before = std::chrono::system_clock::now();
  timer.wait();

  EXPECT_GT(std::chrono::system_clock::now() - before, wait_duration);
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
