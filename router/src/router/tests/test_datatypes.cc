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

#include "gmock/gmock.h"

#include "router_test_helpers.h"
#include "tcp_address.h"

#ifdef _WIN32
#include <WinSock2.h>
#endif

using mysql_harness::TCPAddress;

class TCPAddressTest : public ::testing::Test {
 protected:
  virtual void SetUp() {}
};

TEST_F(TCPAddressTest, EmptyAddress) {
  TCPAddress a;
  EXPECT_EQ("", a.addr);
  EXPECT_EQ(0, a.port);
  EXPECT_FALSE(a.is_valid());
  EXPECT_EQ(TCPAddress::Family::INVALID, a.get_family());
  EXPECT_FALSE(a.is_family<TCPAddress::Family::IPV4>());
  EXPECT_FALSE(a.is_family<TCPAddress::Family::IPV6>());
}

TEST_F(TCPAddressTest, IPv4LocalhostMySQL) {
  TCPAddress a("127.0.0.1", 3306);
  EXPECT_EQ("127.0.0.1", a.addr);
  EXPECT_EQ(3306, a.port);
  EXPECT_TRUE(a.is_valid());
  EXPECT_EQ(TCPAddress::Family::IPV4, a.get_family());
  EXPECT_TRUE(a.is_family<TCPAddress::Family::IPV4>());
  EXPECT_FALSE(a.is_family<TCPAddress::Family::IPV6>());
}

TEST_F(TCPAddressTest, IPv6LocalhostMySQL) {
  TCPAddress a("::1", 3306);
  EXPECT_EQ("::1", a.addr);
  EXPECT_EQ(3306, a.port);
  EXPECT_TRUE(a.is_valid());
  EXPECT_EQ(TCPAddress::Family::IPV6, a.get_family());
  EXPECT_FALSE(a.is_family<TCPAddress::Family::IPV4>());
  EXPECT_TRUE(a.is_family<TCPAddress::Family::IPV6>());
}

TEST_F(TCPAddressTest, IPv4InvalidAddress) {
  TCPAddress a("999.999.999.999", 3306);
  EXPECT_EQ("999.999.999.999", a.addr);
  EXPECT_EQ(3306, a.port);
  EXPECT_FALSE(a.is_valid());
  EXPECT_EQ(TCPAddress::Family::INVALID, a.get_family());
  EXPECT_FALSE(a.is_family<TCPAddress::Family::IPV4>());
  EXPECT_FALSE(a.is_family<TCPAddress::Family::IPV6>());
}

TEST_F(TCPAddressTest, IPv4InvalidPort) {
  TCPAddress a("192.168.1.2", 0);
  EXPECT_EQ("192.168.1.2", a.addr);
  EXPECT_EQ(0, a.port);
  EXPECT_FALSE(a.is_valid());
  EXPECT_EQ(TCPAddress::Family::IPV4, a.get_family());
  EXPECT_TRUE(a.is_family<TCPAddress::Family::IPV4>());
  EXPECT_FALSE(a.is_family<TCPAddress::Family::IPV6>());
}

TEST_F(TCPAddressTest, IPv6InvalidPort) {
  TCPAddress a("fdc2:f6c4:a09e:b67b:1:2:3:4", 99999);
  EXPECT_EQ("fdc2:f6c4:a09e:b67b:1:2:3:4", a.addr);
  EXPECT_EQ(0, a.port);
  EXPECT_FALSE(a.is_valid());
  EXPECT_EQ(TCPAddress::Family::IPV6, a.get_family());
  EXPECT_FALSE(a.is_family<TCPAddress::Family::IPV4>());
  EXPECT_TRUE(a.is_family<TCPAddress::Family::IPV6>());
}

TEST_F(TCPAddressTest, IPv6ValidPort) {
  TCPAddress a("fdc2:f6c4:a09e:b67b:1:2:3:4", 3306);
  EXPECT_EQ("fdc2:f6c4:a09e:b67b:1:2:3:4", a.addr);
  EXPECT_EQ(3306, a.port);
  EXPECT_TRUE(a.is_valid());
  EXPECT_EQ(TCPAddress::Family::IPV6, a.get_family());
  EXPECT_FALSE(a.is_family<TCPAddress::Family::IPV4>());
  EXPECT_TRUE(a.is_family<TCPAddress::Family::IPV6>());
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
