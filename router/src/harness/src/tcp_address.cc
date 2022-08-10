/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include <sstream>
#include <system_error>
#include <type_traits>

#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/stdx/expected.h"

static constexpr int8_t from_digit(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'z') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'Z') {
    return c - 'A' + 10;
  }

  return -1;
}

/**
 * convert a numeric string to a number.
 *
 * variant for unsigned integers like port numbers.
 *
 * Contrary to strtol() it
 *
 * - has no locale support
 * - '-0' doesn't parse as valid (as strtol() does)
 * - does not handle prefixes like 0x for hex, no 0 for octal.
 */
template <class T>
static std::enable_if_t<std::is_unsigned<T>::value,
                        stdx::expected<T, std::error_code>>
from_chars(const std::string &value, int base = 10) {
  if (value.empty()) {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  if (base < 2 || base > 36) {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  uint64_t num{};
  for (const auto c : value) {
    num *= base;

    const auto digit = from_digit(c);
    if (digit == -1) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }

    if (digit >= base) {
      return stdx::make_unexpected(
          make_error_code(std::errc::invalid_argument));
    }

    num += digit;
  }

  // check for overflow
  if (static_cast<T>(num) != num) {
    return stdx::make_unexpected(make_error_code(std::errc::value_too_large));
  }

  return {static_cast<T>(num)};
}

namespace mysql_harness {

static stdx::expected<TCPAddress, std::error_code> make_tcp_address_ipv6(
    const std::string &endpoint) {
  if (endpoint[0] != '[') {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  // IPv6 with port
  size_t pos = endpoint.find(']');
  if (pos == std::string::npos) {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  const auto addr = endpoint.substr(1, pos - 1);
  const auto addr_res = net::ip::make_address_v6(addr.c_str());
  if (!addr_res) {
    return addr_res.get_unexpected();
  }

  ++pos;
  if (pos == endpoint.size()) {
    // ] was last character,  no port
    return {std::in_place, addr, 0};
  }

  if (endpoint[pos] != ':') {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  const auto port_str = endpoint.substr(++pos);
  const auto port_res = from_chars<uint16_t>(port_str);

  if (!port_res) {
    return port_res.get_unexpected();
  }

  auto port = port_res.value();

  return {std::in_place, addr, port};
}

stdx::expected<TCPAddress, std::error_code> make_tcp_address(
    const std::string &endpoint) {
  if (endpoint.empty()) {
    return {std::in_place, "", 0};
  }

  if (endpoint[0] == '[') {
    return make_tcp_address_ipv6(endpoint);
  } else if (std::count(endpoint.begin(), endpoint.end(), ':') > 1) {
    // IPv6 without port
    const auto addr_res = net::ip::make_address_v6(endpoint.c_str());
    if (!addr_res) {
      return addr_res.get_unexpected();
    }

    return {std::in_place, endpoint, 0};
  } else {
    // IPv4 or address
    const auto pos = endpoint.find(":");
    if (pos == std::string::npos) {
      // no port
      return {std::in_place, endpoint, 0};
    }

    auto addr = endpoint.substr(0, pos);
    auto port_str = endpoint.substr(pos + 1);
    const auto port_res = from_chars<uint16_t>(port_str);
    if (!port_res) {
      return port_res.get_unexpected();
    }

    return {std::in_place, addr, port_res.value()};
  }
}

std::string TCPAddress::str() const {
  std::ostringstream os;

  auto make_res = net::ip::make_address_v6(addr_.c_str());
  if (make_res) {
    // looks like a IPv6 address, wrap in []
    os << "[" << addr_ << "]";
  } else {
    os << addr_;
  }

  if (port_ > 0) {
    os << ":" << port_;
  }

  return os.str();
}

}  // namespace mysql_harness
