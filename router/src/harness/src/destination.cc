/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/destination.h"

#include <system_error>

#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/stdx/expected.h"

namespace {

constexpr int8_t from_digit(char c) {
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
stdx::expected<T, std::error_code> from_chars(const std::string &value,
                                              int base = 10)
  requires(std::is_unsigned_v<T>)
{
  if (value.empty()) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  if (base < 2 || base > 36) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  uint64_t num{};
  for (const auto c : value) {
    num *= base;

    const auto digit = from_digit(c);
    if (digit == -1) {
      return stdx::unexpected(make_error_code(std::errc::invalid_argument));
    }

    if (digit >= base) {
      return stdx::unexpected(make_error_code(std::errc::invalid_argument));
    }

    num += digit;
  }

  // check for overflow
  if (static_cast<T>(num) != num) {
    return stdx::unexpected(make_error_code(std::errc::value_too_large));
  }

  return {static_cast<T>(num)};
}

stdx::expected<mysql_harness::TcpDestination, std::error_code>
make_tcp_destination_ipv6(const std::string &endpoint) {
  if (endpoint[0] != '[') {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  // IPv6 with port
  size_t pos = endpoint.find(']');
  if (pos == std::string::npos) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  const auto addr = endpoint.substr(1, pos - 1);
  const auto addr_res = net::ip::make_address_v6(addr.c_str());
  if (!addr_res) return stdx::unexpected(addr_res.error());

  ++pos;
  if (pos == endpoint.size()) {
    // ] was last character,  no port

    return mysql_harness::TcpDestination{addr, 0};
  }

  if (endpoint[pos] != ':') {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  const auto port_str = endpoint.substr(++pos);
  const auto port_res = from_chars<uint16_t>(port_str);
  if (!port_res) return stdx::unexpected(port_res.error());

  return mysql_harness::TcpDestination{addr, *port_res};
}
}  // namespace

namespace mysql_harness {

std::string TcpDestination::str() const {
  std::string out;

  auto colon_pos = hostname_.find(':');
  if (colon_pos == std::string::npos) {
    out = hostname_;  // no colon found, no need to escape it.
  } else {
    out = "[" + hostname_ + "]";
  }

  out += ":" + std::to_string(port_);

  return out;
}

std::string LocalDestination::str() const { return path(); }

std::string Destination::str() const {
  if (is_local()) return as_local().str();

  return as_tcp().str();
}

/**
 * create a DestinationEndpoint from a string.
 *
 * - ipv4
 * - ipv6
 * - hostname
 *
 * followed by optional port.
 *
 * If IPv6 is followed by port, the address part is expected to be wrapped in
 * '[]'
 */
stdx::expected<mysql_harness::TcpDestination, std::error_code>
make_tcp_destination(std::string endpoint) {
  if (endpoint.empty()) return TcpDestination{};

  if (endpoint[0] == '[') return make_tcp_destination_ipv6(endpoint);

  if (std::count(endpoint.begin(), endpoint.end(), ':') > 1) {
    // IPv6 without port
    const auto addr_res = net::ip::make_address_v6(endpoint.c_str());
    if (!addr_res) {
      return stdx::unexpected(addr_res.error());
    }

    return TcpDestination{endpoint, 0};
  }

  // IPv4 or address
  const auto pos = endpoint.find(':');
  if (pos == std::string::npos) {
    // no port
    return TcpDestination{endpoint, 0};
  }

  auto addr = endpoint.substr(0, pos);
  auto port_str = endpoint.substr(pos + 1);
  const auto port_res = from_chars<uint16_t>(port_str);
  if (!port_res) {
    return stdx::unexpected(port_res.error());
  }

  return TcpDestination{addr, port_res.value()};
}

}  // namespace mysql_harness
