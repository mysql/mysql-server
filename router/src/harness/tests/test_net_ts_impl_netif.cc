/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/impl/netif.h"

#include <type_traits>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysql/harness/stdx/expected.h"

#ifndef _WIN32
/*
 * check if std::is_constructible<> sees the protected constructor
 *
 *   NetworkInferfaceResults(ifaddrs *)
 *
 * [it does not].
 *
 * Background:
 *
 *   std::is_constructible<> is used by stdx::expected<> to enable a
 * constructor.
 *
 * That means
 *
 * @code
 * stdx::expected<NetworkInterfaceResults, std::error_code> some_friend_method()
 * { ifaddrs *ifs = nullptr;
 *   // ...
 *   return {ifs};
 * }
 * @endcode
 *
 * will fail as the expected's value-conversion constructor will be disabled.
 *
 * This isn't a requirement check, just a "capture the current state".
 */
static_assert(
    !std::is_constructible_v<net::NetworkInterfaceResults, ifaddrs *>);
#endif

TEST(NetIfs, query) {
  auto query_res = net::NetworkInterfaceResolver().query();
  if (!query_res) GTEST_SKIP();

  for (const auto &entry : *query_res) {
    EXPECT_THAT(entry.display_name(), ::testing::Not(::testing::IsEmpty()));
  }
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
