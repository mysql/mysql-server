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
#include "tcp_address.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysql/harness/stdx/expected_ostream.h"

using mysql_harness::TCPAddress;

struct TCPAddressParam {
  const char *test_name;

  std::string endpoint;

  std::string expected_address;
  uint16_t expected_port;
  std::string expected_endpoint;
};

class TCPAddressTest : public ::testing::Test,
                       public ::testing::WithParamInterface<TCPAddressParam> {};

TEST_P(TCPAddressTest, check) {
  auto make_res = mysql_harness::make_tcp_address(GetParam().endpoint);
  ASSERT_TRUE(make_res) << make_res.error();

  auto endp = make_res.value();
  EXPECT_EQ(endp.address(), GetParam().expected_address);
  EXPECT_EQ(endp.port(), GetParam().expected_port);

  EXPECT_EQ(endp.str(), GetParam().expected_endpoint);
}

const TCPAddressParam tcp_address_param[] = {
    {"empty_address", "", "", 0, ""},
    {"ipv4_with_port", "127.0.0.1:3306", "127.0.0.1", 3306, "127.0.0.1:3306"},
    {"ipv4_with_port_zero", "127.0.0.1:0", "127.0.0.1", 0, "127.0.0.1"},
    {"ipv4_with_port_max", "127.0.0.1:65535", "127.0.0.1", 65535,
     "127.0.0.1:65535"},
    {"ipv6_with_port", "[::1]:3306", "::1", 3306, "[::1]:3306"},
    {"ipv6_no_port", "[::1]", "::1", 0, "[::1]"},
    {"ipv6_no_port_no_square", "::1", "::1", 0, "[::1]"},
    {"host_with_port", "example.org:3306", "example.org", 3306,
     "example.org:3306"},
    {"host_no_port", "example.org", "example.org", 0, "example.org"},
    {"num_host_no_port", "999.999.999.999", "999.999.999.999", 0,
     "999.999.999.999"},
    {"ipv6_scope_id", "::1%0", "::1%0", 0, "[::1%0]"},
};

INSTANTIATE_TEST_SUITE_P(Spec, TCPAddressTest,
                         ::testing::ValuesIn(tcp_address_param),
                         [](auto const &info) { return info.param.test_name; });

struct TCPAddressFailParam {
  const char *test_name;

  std::string endpoint;

  std::error_code expected_ec;
};

class TCPAddressFailTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<TCPAddressFailParam> {};

TEST_P(TCPAddressFailTest, check) {
  auto make_res = mysql_harness::make_tcp_address(GetParam().endpoint);
  ASSERT_FALSE(make_res);

  EXPECT_EQ(make_res.error(), GetParam().expected_ec);
}

const TCPAddressFailParam tcp_address_fail_param[] = {
    {"ipv4_with_port_hex", "127.0.0.1:a",
     make_error_code(std::errc::invalid_argument)},
    {"ipv4_with_port_negative", "127.0.0.1:-3306",
     make_error_code(std::errc::invalid_argument)},
    {"ipv4_with_port_too_large", "127.0.0.1:65536",
     make_error_code(std::errc::value_too_large)},
    {"ipv4_colon_no_port",
     "127.0.0.1:", make_error_code(std::errc::invalid_argument)},
    {"ipv4_colon_dash", "127.0.0.1:-",
     make_error_code(std::errc::invalid_argument)},
    {"ipv4_colon_minus_zero", "127.0.0.1:-0",
     make_error_code(std::errc::invalid_argument)},
    {"ipv6_bracket_invalid", "[z::abc]",
     make_error_code(std::errc::invalid_argument)},
    {"ipv6_no_backer_invalid", "z::abc",
     make_error_code(std::errc::invalid_argument)},
    {"ipv6_opening_bracket", "[::1",
     make_error_code(std::errc::invalid_argument)},
    {"ipv6_closing_bracket", "::1]",
     make_error_code(std::errc::invalid_argument)},
    {"ipv6_with_port_bogus_extra", "[::1]asd:123",
     make_error_code(std::errc::invalid_argument)},
    {"ipv6_with_port_negative_zero", "[::1]:-0",
     make_error_code(std::errc::invalid_argument)},
    {"ipv6_with_port_too_large", "[::1]:65536",
     make_error_code(std::errc::value_too_large)},
    {"ipv6_colon_no_port",
     "[::1]:", make_error_code(std::errc::invalid_argument)},
    {"ipv6_no_port_bogus_extra", "::z",
     make_error_code(std::errc::invalid_argument)},
};

INSTANTIATE_TEST_SUITE_P(Spec, TCPAddressFailTest,
                         ::testing::ValuesIn(tcp_address_fail_param),
                         [](auto const &info) { return info.param.test_name; });

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
