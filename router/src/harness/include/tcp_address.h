/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_TCPADDRESS_INCLUDED
#define MYSQL_HARNESS_TCPADDRESS_INCLUDED

#include <cstdint>
#include <string>
#include <system_error>

#include "harness_export.h"

#include "mysql/harness/stdx/expected.h"

namespace mysql_harness {

/** @brief Defines an IP address with port number  */
class HARNESS_EXPORT TCPAddress {
 public:
  TCPAddress() = default;

  TCPAddress(std::string address, uint16_t tcp_port)
      : addr_(std::move(address)), port_(tcp_port) {}

  TCPAddress(const TCPAddress &other) = default;
  TCPAddress(TCPAddress &&other) = default;
  TCPAddress &operator=(const TCPAddress &other) = default;
  TCPAddress &operator=(TCPAddress &&other) = default;

  std::string address() const { return addr_; }

  uint16_t port() const { return port_; }

  void port(uint16_t p) { port_ = p; }

  /** @brief Returns the address as a string
   *
   * Returns the address as a string.
   *
   * @return instance of std::string
   */
  std::string str() const;

  /** @brief Compares two addresses for equality
   *
   */
  friend bool operator==(const TCPAddress &left, const TCPAddress &right) {
    return (left.addr_ == right.addr_) && (left.port_ == right.port_);
  }

  /**
   * @brief Function for performing comparison of TCPAddresses
   */
  friend bool operator<(const TCPAddress &left, const TCPAddress &right) {
    if (left.addr_ < right.addr_)
      return true;
    else if (left.addr_ > right.addr_)
      return false;
    return left.port_ < right.port_;
  }

 private:
  /** @brief Network name IP */
  std::string addr_;

  /** @brief TCP port */
  uint16_t port_{};
};

/**
 * create TCPAddress from endpoint string.
 *
 * - [::1]:1234
 * - ::1
 * - 10.0.1.1
 * - 10.0.1.1:1234
 * - example.org:1234
 */
HARNESS_EXPORT stdx::expected<TCPAddress, std::error_code> make_tcp_address(
    const std::string &endpoint);

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_TCPADDRESS_INCLUDED
