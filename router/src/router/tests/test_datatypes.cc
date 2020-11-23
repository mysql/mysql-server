/*
  Copyright (c) 2015, 2020, Oracle and/or its affiliates.

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
#include "tcp_address.h"

#include <gmock/gmock.h>

#include "router_test_helpers.h"

#ifdef _WIN32
#include <WinSock2.h>
#endif

using mysql_harness::TCPAddress;

class TCPAddressTest : public ::testing::Test {};

TEST_F(TCPAddressTest, EmptyAddress) {
  TCPAddress a;
  EXPECT_EQ("", a.address());
  EXPECT_EQ(0, a.port());

  EXPECT_EQ(a.str(), "");
}

TEST_F(TCPAddressTest, IPv4LocalhostMySQL) {
  TCPAddress a("127.0.0.1", 3306);
  EXPECT_EQ("127.0.0.1", a.address());
  EXPECT_EQ(3306, a.port());

  EXPECT_EQ(a.str(), "127.0.0.1:3306");
}

TEST_F(TCPAddressTest, IPv6LocalhostMySQL) {
  TCPAddress a("::1", 3306);
  EXPECT_EQ("::1", a.address());
  EXPECT_EQ(3306, a.port());

  EXPECT_EQ(a.str(), "[::1]:3306");
}

TEST_F(TCPAddressTest, NonIpAddress) {
  // looks like an invalid IPv4 address, but is treated as a hostname.
  TCPAddress a("999.999.999.999", 3306);
  EXPECT_EQ("999.999.999.999", a.address());
  EXPECT_EQ(3306, a.port());

  EXPECT_EQ(a.str(), "999.999.999.999:3306");
}

TEST_F(TCPAddressTest, IPv4PortZero) {
  TCPAddress a("192.168.1.2", 0);
  EXPECT_EQ("192.168.1.2", a.address());
  EXPECT_EQ(0, a.port());

  EXPECT_EQ(a.str(), "192.168.1.2");
}

TEST_F(TCPAddressTest, IPv6ValidPort) {
  TCPAddress a("fdc2:f6c4:a09e:b67b:1:2:3:4", 3306);
  EXPECT_EQ("fdc2:f6c4:a09e:b67b:1:2:3:4", a.address());
  EXPECT_EQ(3306, a.port());

  EXPECT_EQ(a.str(), "[fdc2:f6c4:a09e:b67b:1:2:3:4]:3306");
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
