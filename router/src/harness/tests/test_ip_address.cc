/*
  Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

////////////////////////////////////////
// Standard include files
#include <exception>
#include <iostream>
#include <stdexcept>

////////////////////////////////////////
// Third-party include files
#include <gmock/gmock.h>

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"

using mysql_harness::IPAddress;
using mysql_harness::IPv4Address;
using mysql_harness::IPv6Address;

////////////////////////////////////////
// mysql_harness::IPv4Address

TEST(TestIPv4Address, FromToString) {
  std::string test_addr{"127.0.0.1"};

  {
    IPv4Address ip(test_addr);
    ASSERT_EQ(test_addr, ip.str());
  }

  {
    IPv4Address ip(test_addr.c_str());
    ASSERT_EQ(test_addr, ip.str());
  }
}

TEST(TestIPv4Address, FromStringFail) {
  using ::testing::HasSubstr;
  {
    // Cannot handle IPv6
    std::string test_addr{"fe80::6e40:8ff:fea2:5d7e"};
    EXPECT_THROW({ IPv4Address ip(test_addr); }, std::invalid_argument);
    try {
      IPv4Address ip(test_addr);
    } catch (const std::invalid_argument &exc) {
      EXPECT_THAT(exc.what(), HasSubstr("parsing error"));
    }
  }

  {
    // Wrong IPv4 address
    std::string test_addr{"300.1.2.3"};
    EXPECT_THROW({ IPv4Address ip(test_addr); }, std::invalid_argument);
    try {
      IPv4Address ip(test_addr);
    } catch (const std::invalid_argument &exc) {
      EXPECT_THAT(exc.what(), HasSubstr("parsing error"));
    }
  }
}

TEST(TestIPv4Address, CopyConstructor) {
  std::string test_addr{"192.168.14.5"};
  IPv4Address ip(test_addr);

  auto copy(ip);

  EXPECT_EQ(test_addr, copy.str());
}

TEST(TestIPv4Address, CopyAssigment) {
  std::string test_addr{"192.168.14.5"};
  IPv4Address ip(test_addr);

  IPv4Address copy{};
  copy = ip;

  EXPECT_EQ(test_addr, copy.str());
}

TEST(TestIPv4Address, Equality) {
  IPv4Address ip1("192.168.14.5");
  IPv4Address ip2("192.168.14.5");
  IPv4Address ip3("192.168.14.200");
  EXPECT_EQ(ip1, ip2);
  EXPECT_NE(ip1, ip3);
}

TEST(TestIPv4Address, Inequality) {
  IPv4Address ip1("192.168.14.5");
  IPv4Address ip2("192.168.14.5");
  IPv4Address ip3("192.168.14.200");
  EXPECT_NE(ip1, ip3);
  EXPECT_EQ(ip1, ip2);
}

TEST(TestIPv4Address, OperatorStreamInsertion) {
  std::ostringstream os;

  std::string test_addr{"192.168.14.5"};

  IPv4Address ip(test_addr);

  os << ip;
  ASSERT_EQ(test_addr, os.str());
}

////////////////////////////////////////
// mysql_harness::IPv6Address

TEST(TestIPv6Address, FromString) {
  std::string test_addr{"fe80::1"};

  {
    IPv6Address ip(test_addr);
    EXPECT_EQ(test_addr, ip.str());
  }

  {
    IPv6Address ip(test_addr.c_str());
    EXPECT_EQ(test_addr, ip.str());
  }

  {
    test_addr = "fe80::6e40:8ff:fea2:5d7e";
    IPv6Address ip(test_addr);
    EXPECT_EQ(test_addr, ip.str());
  }

  { EXPECT_EQ(test_addr, IPv6Address(test_addr).str()); }
}

TEST(TestIPv6Address, FromStringFail) {
  using ::testing::HasSubstr;

  {
    // Cannot handle IPv4
    std::string str_ipv4{"192.168.14.5"};
    EXPECT_THROW({ IPv6Address ip(str_ipv4); }, std::invalid_argument);
    try {
      IPv6Address ip{str_ipv4};
    } catch (const std::invalid_argument &exc) {
      EXPECT_THAT(exc.what(), HasSubstr("parsing error"));
    }
  }

  {
    // Wrong IPv6 address
    std::string str_ipv6_wrong{"fe80::6e40:8ff:fea2:5d7x"};  // x at the end
    EXPECT_THROW({ IPv6Address ip(str_ipv6_wrong); }, std::invalid_argument);
    try {
      IPv6Address ip{str_ipv6_wrong};
    } catch (const std::invalid_argument &exc) {
      EXPECT_THAT(exc.what(), HasSubstr("parsing error"));
    }
  }
}

TEST(TestIPv6Address, CopyConstructor) {
  std::string test_addr{"fe80::6e40:8ff:fea2:5d7e"};
  IPv6Address ip(test_addr);

  auto copy(ip);

  ASSERT_EQ(test_addr, copy.str());
}

TEST(TestIPv6Address, CopyAssigment) {
  std::string test_addr{"fe80::6e40:8ff:fea2:5d7e"};
  IPv6Address ip(test_addr);

  IPv6Address copy{};
  copy = ip;

  ASSERT_EQ(test_addr, copy.str());
}

TEST(TestIPv6Address, OperatorStreamInsertion) {
  std::ostringstream os;

  std::string test_addr{"fe80::6e40:8ff:fea2:5d7e"};

  IPv6Address ip(test_addr);

  os << ip;
  ASSERT_EQ(test_addr, os.str());
}

TEST(TestIPv6Address, Equality) {
  IPv6Address ip1("fe80::6e40:8ff:fea2:5d7e");
  IPv6Address ip2("fe80::6e40:8ff:fea2:5d7e");
  IPv6Address ip3("fe80::6e40:8ff:fea2:8e2a");
  ASSERT_TRUE(ip1 == ip2);
  ASSERT_FALSE(ip1 == ip3);
}

TEST(TestIPv6Address, Inequality) {
  IPv6Address ip1("fe80::6e40:8ff:fea2:5d7e");
  IPv6Address ip2("fe80::6e40:8ff:fea2:5d7e");
  IPv6Address ip3("fe80::6e40:8ff:fea2:8e2a");
  ASSERT_TRUE(ip1 != ip3);
  ASSERT_FALSE(ip1 != ip2);
}

////////////////////////////////////////
// mysql_harness::IPAddress

TEST(TestIPAddress, Constructor) {
  {
    IPAddress addr;
    EXPECT_TRUE(addr.is_ipv4());
    EXPECT_EQ(addr.str(), std::string("0.0.0.0"));
  }

  {
    IPAddress addr("127.0.0.1");
    EXPECT_TRUE(addr.is_ipv4());
  }

  {
    IPAddress addr("::1");
    EXPECT_TRUE(addr.is_ipv6());
  }

  ASSERT_THROW({ IPAddress("127.0.0.1fooo"); }, std::invalid_argument);
  ASSERT_THROW({ IPAddress(":::1"); }, std::invalid_argument);
}

TEST(TestIPAddress, ConstructorIPv4) {
  IPAddress addr(IPv4Address("127.0.0.1"));
  EXPECT_TRUE(addr.is_ipv4());
  EXPECT_FALSE(addr.is_ipv6());
}

TEST(TestIPAddress, ConstructorIPv6) {
  IPAddress addr(IPv6Address("fe80::1"));
  EXPECT_TRUE(addr.is_ipv6());
  EXPECT_FALSE(addr.is_ipv4());
}

TEST(TestIPAddress, CopyConstructor) {
  std::string test_addr4{"192.168.14.5"};
  std::string test_addr6{"fe80::6e40:8ff:fea2:5d7e"};

  IPAddress copy4{IPAddress(test_addr4)};
  IPAddress copy6{IPAddress(test_addr6)};

  EXPECT_EQ(test_addr4, copy4.str());
  EXPECT_EQ(test_addr6, copy6.str());
}

TEST(TestIPAddress, CopyAssigment) {
  std::string test_addr4{"192.168.14.5"};
  std::string test_addr6{"fe80::6e40:8ff:fea2:5d7e"};

  IPAddress ip4(test_addr4);
  IPAddress ip6(test_addr6);

  IPAddress copy4{};
  IPAddress copy6{};

  copy4 = ip4;
  copy6 = ip6;

  EXPECT_EQ(test_addr4, copy4.str());
  EXPECT_EQ(test_addr6, copy6.str());
}

TEST(TestIPAddress, OperatorStreamInsertion) {
  std::ostringstream os;

  std::string test_addr{"fe80::6e40:8ff:fea2:5d7e"};

  IPAddress ip(test_addr);

  os << ip;
  EXPECT_EQ(test_addr, os.str());
}

TEST(TestIPAddress, AsIPv4) {
  std::string test_addr4{"192.168.14.5"};

  IPv4Address ipv4(test_addr4);
  auto ip = IPAddress(ipv4);
  ASSERT_TRUE(ipv4 == ip.as_ipv4());
  ASSERT_THROW({ ip.as_ipv6(); }, std::runtime_error);
}

TEST(TestIPAddress, AsIPv6) {
  std::string test_addr6{"fe80::6e40:8ff:fea2:5d7e"};

  IPv6Address ipv6(test_addr6);
  auto ip = IPAddress(ipv6);
  ASSERT_TRUE(ipv6 == ip.as_ipv6());
  ASSERT_THROW({ ip.as_ipv4(); }, std::runtime_error);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
