/*
  Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/net_ts/internet.h"

#include <gmock/gmock.h>

#include <csignal>  // signal

#include "mysql/harness/stdx/expected_ostream.h"

// helper to be used with ::testing::Truly to check if a std::expected<> has a
// value and triggering the proper printer used in case of failure
static auto res_has_value = [](const auto &t) { return bool(t); };

namespace std {

std::ostream &operator<<(std::ostream &os, const std::error_condition &cond) {
  // std::error_condition has no operator<< for ostream, only std::error_code
  // has but here we need it for EXPECT-printer-on-failure

  os << cond.message();

  return os;
}
}  // namespace std

// default constructed address is ipv4-any
TEST(NetTS_internet, address_construct_default) {
  net::ip::address addr;

  EXPECT_FALSE(addr.is_loopback());
  EXPECT_TRUE(addr.is_unspecified());
  EXPECT_FALSE(addr.is_multicast());
  EXPECT_TRUE(addr.is_v4());
  EXPECT_FALSE(addr.is_v6());
}

TEST(NetTS_internet, address_v4_construct_default) {
  net::ip::address_v4 addr;

  EXPECT_FALSE(addr.is_loopback());
  EXPECT_TRUE(addr.is_unspecified());
  EXPECT_FALSE(addr.is_multicast());
}

TEST(NetTS_internet, address_v6_construct_default) {
  net::ip::address_v6 addr;

  EXPECT_FALSE(addr.is_loopback());
  EXPECT_TRUE(addr.is_unspecified());
  EXPECT_FALSE(addr.is_multicast());
}

TEST(NetTS_internet, address_v4_to_string) {
  net::ip::address_v4 addr;

  EXPECT_EQ(addr.to_string(), "0.0.0.0");

  addr = net::ip::address_v4::loopback();

  EXPECT_EQ(addr.to_string(), "127.0.0.1");
}

TEST(NetTS_internet, address_comp_v4_lt_v6) {
  constexpr net::ip::address a4(net::ip::address_v4{});
  constexpr net::ip::address a6(net::ip::address_v6{});

  static_assert(a4 < a6, "");

  EXPECT_LT(a4, a6);
}

TEST(NetTS_internet, address_comp_v4_eq) {
  constexpr net::ip::address a_1(net::ip::address_v4{});
  constexpr net::ip::address a_2(net::ip::address_v4{});

  static_assert(a_1 == a_2, "");

  EXPECT_EQ(a_1, a_2);
}

TEST(NetTS_internet, address_comp_v4_ne) {
  constexpr net::ip::address a_1(net::ip::address_v4{});
  constexpr net::ip::address a_2(net::ip::address_v4{}.loopback());

  static_assert(a_1 != a_2, "");
  static_assert(a_1 < a_2, "");

  EXPECT_NE(a_1, a_2);
  EXPECT_LT(a_1, a_2);
}

TEST(NetTS_internet, address_comp_v6_eq) {
  constexpr net::ip::address a_1(net::ip::address_v6{});
  constexpr net::ip::address a_2(net::ip::address_v6{});

  static_assert(a_1 == a_2, "");

  EXPECT_EQ(a_1, a_2);
}

TEST(NetTS_internet, address_comp_v6_ne) {
  constexpr net::ip::address a_1(net::ip::address_v6{});
  constexpr net::ip::address a_2(net::ip::address_v6{}.loopback());

  static_assert(a_1 != a_2, "");
  static_assert(a_1 < a_2, "");

  EXPECT_NE(a_1, a_2);
  EXPECT_LT(a_1, a_2);
}

TEST(NetTS_internet, tcp_endpoint_default_construct) {
  net::ip::tcp::endpoint endpoint;

  EXPECT_EQ(endpoint.address(), net::ip::address());
  EXPECT_EQ(endpoint.port(), 0);
}

TEST(NetTS_internet, tcp_endpoint_from_address_port) {
  net::ip::tcp::endpoint endpoint(net::ip::address_v4::loopback(), 12345);

  EXPECT_EQ(endpoint.address(), net::ip::address_v4::loopback());
  EXPECT_EQ(endpoint.port(), 12345);
}

TEST(NetTS_internet, network_v4_default_construct) {
  net::ip::network_v4 net;

  EXPECT_FALSE(net.is_host());
  EXPECT_EQ(net.to_string(), "0.0.0.0/0");
}

TEST(NetTS_internet, network_v4_with_prefix) {
  auto addr_res = net::ip::make_address("127.0.0.1");
  ASSERT_THAT(addr_res, ::testing::Truly(res_has_value));

  auto addr = std::move(*addr_res);
  ASSERT_TRUE(addr.is_v4());
  net::ip::network_v4 net(addr.to_v4(), 32);

  EXPECT_TRUE(net.is_host());
  EXPECT_EQ(net.to_string(), "127.0.0.1/32");
}

TEST(NetTS_internet, network_v4_invalid) {
  ASSERT_EQ(
      net::ip::make_address("127.0.0."),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("127.0.0.1."),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("127.0.0,1"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("256.0.0.1"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
}

TEST(NetTS_internet, network_v6_default_construct) {
  net::ip::network_v6 net;

  EXPECT_FALSE(net.is_host());
  EXPECT_EQ(net.to_string(), "::/0");
}

TEST(NetTS_internet, network_v6_with_prefix) {
  auto addr_res = net::ip::make_address("::1");
  ASSERT_THAT(addr_res, ::testing::Truly(res_has_value));

  auto addr = std::move(*addr_res);
  ASSERT_TRUE(addr.is_v6());
  net::ip::network_v6 net(addr.to_v6(), 128);

  EXPECT_TRUE(net.is_host());
  EXPECT_EQ(net.to_string(), "::1/128");
}

TEST(NetTS_internet, network_v6_with_prefix_and_scope_id) {
  auto addr_res = net::ip::make_address("::1%1");
  ASSERT_THAT(addr_res, ::testing::Truly(res_has_value));

  auto addr = std::move(*addr_res);
  ASSERT_TRUE(addr.is_v6());
  ASSERT_EQ(addr.to_v6().scope_id(), 1);
  net::ip::network_v6 net(addr.to_v6(), 128);

  EXPECT_TRUE(net.is_host());
  EXPECT_EQ(net.to_string(), "::1%1/128");
}

TEST(NetTS_internet, make_address_v6_invalid) {
  ASSERT_EQ(
      net::ip::make_address("zzz"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("::1::2"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("::1%-1"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("::1%+1"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("::1%abc"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
  ASSERT_EQ(
      net::ip::make_address("::1%"),
      stdx::make_unexpected(make_error_code(std::errc::invalid_argument)));
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

#ifndef _WIN32
  signal(SIGPIPE, SIG_IGN);
#endif

  net::impl::socket::init();

  return RUN_ALL_TESTS();
}
