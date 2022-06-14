/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "blocked_endpoints.h"
#include "mysql/harness/filesystem.h"  // Path

using ::testing::ContainerEq;
using ::testing::HasSubstr;
using ::testing::StrEq;

std::string g_cwd;
mysql_harness::Path g_origin;

class TestBlockClients : public ::testing::Test {};

TEST_F(TestBlockClients, BlockClientHost) {
  uint64_t max_connect_errors = 2;

  auto ipv6_1_res = net::ip::make_address("::1");
  ASSERT_TRUE(ipv6_1_res);
  auto ipv6_2_res = net::ip::make_address("::2");
  ASSERT_TRUE(ipv6_2_res);

  auto ipv6_1 = net::ip::tcp::endpoint(ipv6_1_res.value(), 0);
  auto ipv6_2 = net::ip::tcp::endpoint(ipv6_2_res.value(), 0);

  BlockedEndpoints blocked_endpoints{max_connect_errors};

  blocked_endpoints.increment_error_count(ipv6_1);
  ASSERT_FALSE(blocked_endpoints.is_blocked(ipv6_1));

  blocked_endpoints.increment_error_count(ipv6_1);
  ASSERT_TRUE(blocked_endpoints.is_blocked(ipv6_1));

  {
    auto blocked_hosts = blocked_endpoints.get_blocked_client_hosts();
    EXPECT_THAT(blocked_hosts,
                ::testing::UnorderedElementsAre(ipv6_1.address().to_string()));
  }

  // block a 2nd endpoint
  blocked_endpoints.increment_error_count(ipv6_2);
  ASSERT_FALSE(blocked_endpoints.is_blocked(ipv6_2));

  blocked_endpoints.increment_error_count(ipv6_2);
  ASSERT_TRUE(blocked_endpoints.is_blocked(ipv6_2));

  {
    auto blocked_hosts = blocked_endpoints.get_blocked_client_hosts();
    EXPECT_THAT(blocked_hosts,
                ::testing::UnorderedElementsAre(ipv6_1.address().to_string(),
                                                ipv6_2.address().to_string()));
  }

  // clearing counter for ipv6_1
  EXPECT_TRUE(blocked_endpoints.reset_error_count(ipv6_1));
  EXPECT_FALSE(blocked_endpoints.is_blocked(ipv6_1));
  EXPECT_TRUE(blocked_endpoints.is_blocked(ipv6_2));

  EXPECT_FALSE(blocked_endpoints.reset_error_count(ipv6_1));
}

int main(int argc, char *argv[]) {
  g_origin = mysql_harness::Path(argv[0]).dirname();
  g_cwd = mysql_harness::Path(argv[0]).dirname().str();
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
