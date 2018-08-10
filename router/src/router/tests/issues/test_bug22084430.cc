/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
 * BUG22084430 IPV6 ADDRESS IN LOGS DOES NOT USE []
 *
 */

#include "gmock/gmock.h"

#include "router_test_helpers.h"
#include "tcp_address.h"

#ifdef _WIN32
#include <WinSock2.h>
#endif

using mysql_harness::TCPAddress;

class Bug22084430 : public ::testing::Test {
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(Bug22084430, LogCorrectIPv6Address) {
  std::map<std::string, TCPAddress> address{
      {"[::]:7002", TCPAddress("::", 7002)},
      {"[FE80:0000:0000:0000:0202:B3FF:FE1E:8329]:8329",
       TCPAddress("FE80:0000:0000:0000:0202:B3FF:FE1E:8329", 8329)},
      {"[FE80::0202:B3FF:FE1E:8329]:80",
       TCPAddress("FE80::0202:B3FF:FE1E:8329", 80)},
  };

  for (auto &it : address) {
    EXPECT_EQ(it.second.str(), it.first);
  }
}

TEST_F(Bug22084430, LogCorrectIPv4Address) {
  std::map<std::string, TCPAddress> address{
      {"127.0.0.1:7002", TCPAddress("127.0.0.1", 7002)},
      {"192.168.1.128:8329", TCPAddress("192.168.1.128", 8329)},
  };

  for (auto &it : address) {
    EXPECT_EQ(it.second.str(), it.first);
  }
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
