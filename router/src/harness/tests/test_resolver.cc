/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/networking/ip_address.h"
#include "mysql/harness/networking/ipv4_address.h"
#include "mysql/harness/networking/resolver.h"

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"

////////////////////////////////////////
// Third-party include files
#include "gmock/gmock.h"

////////////////////////////////////////
// Standard include files
#include <exception>

using mysql_harness::IPAddress;
using mysql_harness::Resolver;

// class used for testing private functionality like caching
class MockResolver : public Resolver {
 public:
  uint16_t cached_tcp_service_by_name(const std::string &name) const {
    return Resolver::cached_tcp_service_by_name(name);
  }

  std::string cached_tcp_service_by_port(uint16_t port) const {
    return Resolver::cached_tcp_service_by_port(port);
  }
};

TEST(TestResolver, Hostname) {
  using ::testing::Contains;
  Resolver resolver;

  {
    // Some systems have both IPv4 and IPv6 for 'localhost'
    mysql_harness::IPAddress ip4("127.0.0.1");
    mysql_harness::IPAddress ip6("::1");

    auto result = resolver.hostname("localhost");
    ASSERT_THAT(result, ::testing::AnyOf(Contains(ip4), Contains(ip6)));
  }
}

TEST(TestNameResolver, HostnameFail) {
  Resolver resolver;
  ASSERT_THROW({ resolver.hostname("foobar.dkkdkdk.r4nd0m"); },
               std::invalid_argument);
}

TEST(TestResolver, TCPServiceName) {
  Resolver resolver;
  EXPECT_EQ(21, resolver.tcp_service_name("ftp"));
#if !defined(_WIN32) && !defined(__sun)
  EXPECT_EQ(3306, resolver.tcp_service_name("mysql"));
#endif
}

TEST(TestResolver, TCPServiceNameFail) {
  Resolver resolver;
  ASSERT_THROW({ resolver.tcp_service_name("foo_bar"); },
               std::invalid_argument);
}

TEST(TestResolver, TCPServicePort) {
  Resolver resolver;

  EXPECT_EQ(std::string("ftp"), resolver.tcp_service_port(21));
#if !defined(_WIN32) && !defined(__sun)
  EXPECT_EQ(std::string("mysql"), resolver.tcp_service_port(3306));
#endif
  EXPECT_EQ(std::string("ssh"), resolver.tcp_service_port(22));
  // port numbers without service name
  EXPECT_EQ(std::string("49151"),
            resolver.tcp_service_port(49151));  // IANA reserved port number
}

TEST(TestResolver, TCPServiceCache) {
  MockResolver resolver;

  // query, so cache is updated
  EXPECT_EQ(21, resolver.tcp_service_name("ftp"));
#if !defined(_WIN32) && !defined(__sun)
  EXPECT_EQ(std::string("mysql"), resolver.tcp_service_port(3306));
#endif
  // check if in cache
#if !defined(_WIN32) && !defined(__sun)
  EXPECT_EQ(3306, resolver.cached_tcp_service_by_name("mysql"));
#endif
  EXPECT_EQ(std::string("ftp"), resolver.cached_tcp_service_by_port(21));
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
  WSADATA wsaData;
  int iResult;
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    std::cout << "WSAStartup() failed\n";
    return 1;
  }
#endif
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
