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

#ifndef MYSQL_HARNESS_NETWORKING_IPV4_ADDRESS_INCLUDED
#define MYSQL_HARNESS_NETWORKING_IPV4_ADDRESS_INCLUDED

#include "harness_export.h"

#include <array>
#include <iostream>
#include <string>

#ifndef _WIN32
#include <arpa/inet.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace mysql_harness {

/**
 * IPv4Address for IP version 4 addresses
 *
 * This class manages IP v4 addresses.
 *
 * The following will create an `IPv4Address` instance for the localhost
 * address:
 *
 * ```
 * mysql_harness::IPv4Address ip4("127.0.0.1");
 *
 * std::cout << "IPv4: " << ip4 << std::endl;
 * ```
 *
 * `mysql_harness::IPAddress` should be used to manage IP addresses
 * when using both IPv4 and IPv6.
 *
 */
class HARNESS_EXPORT IPv4Address {
 public:
  /**
   * Constructs a new IPv4Address object leaving the internal structure
   * initialized to zero.
   *
   */
  IPv4Address() { address_.s_addr = 0; }

  /**
   * Constructs a new IPv4Address object using the given unsigned
   * integer.
   *
   * @param addr unsigned 32-bit integer representing an IPv4 address
   */
  explicit IPv4Address(uint32_t addr) { address_.s_addr = addr; }

  /**
   * Constructs a new IPv4Address object using the null-terminated
   * character string or a `std::string` representing the IPv4 address.
   *
   * @throws std::invalid_argument when data could not be converted
   * to an IPv4 address
   * @param data string representing a IPv4 address
   */
  explicit IPv4Address(const char *data);

  /** @overload */
  explicit IPv4Address(const std::string &data) : IPv4Address(data.c_str()) {}

  /** Copy constructor */
  IPv4Address(const IPv4Address &other) : address_(other.address_) {}

  /** Copy assignment */
  IPv4Address &operator=(const IPv4Address &other) {
    if (this != &other) {
      address_ = other.address_;
    }
    return *this;
  }

  /**
   * Returns text representation of the IPv4 address
   *
   * Throws `std::system_error` when it was not possible to
   * get the textual representation of the IPv4 address.
   *
   * @return IPv4 address as a `std::string`
   */
  std::string str() const;

  /**
   * Compare IPv4 addresses for equality
   *
   * @return true if IPv4 addresses are equal
   */
  friend bool operator==(const IPv4Address &a, const IPv4Address &b) {
    return a.address_.s_addr == b.address_.s_addr;
  }

  /**
   * Compare IPv4 addresses for inequality
   *
   * @return true if IPv4 addresses are not equal
   */
  friend bool operator!=(const IPv4Address &a, const IPv4Address &b) {
    return a.address_.s_addr != b.address_.s_addr;
  }

  /**
   * Overload stream insertion operator
   */
  friend std::ostream &operator<<(std::ostream &out,
                                  const IPv4Address &address) {
    out << address.str();
    return out;
  }

 private:
  /** Storage of the IPv4 address. */
  in_addr address_;
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_NETWORKING_IPV4_ADDRESS_INCLUDED
