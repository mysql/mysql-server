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

#ifndef MYSQL_HARNESS_NETWORKING_IPV6_ADDRESS_INCLUDED
#define MYSQL_HARNESS_NETWORKING_IPV6_ADDRESS_INCLUDED

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <ws2tcpip.h>  // in6_addr
#endif
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "harness_export.h"

namespace mysql_harness {

/**
 * IPv6Address for IP version 6 addresses
 *
 * This class manages IP v6 addresses.
 *
 * The following will create an `IPv6Address` instance for the localhost
 * address:
 *
 * ```
 * mysql_harness::IPv6Address ip6("::1");
 *
 * std::cout << "IPv6: " << ip6 << std::endl;
 * ```
 *
 * `mysql_harness::IPAddress` should be used to manage IP addresses
 * when using both IPv4 and IPv6.
 *
 */
class HARNESS_EXPORT IPv6Address {
 public:
  /**
   * Constructs a new IPv6Address object leaving the internal structure
   * initialized to zero.
   *
   */
  IPv6Address() : address_() {}

  /**
   * Constructs a new IPv6Address object copying the given array
   * of 16 unsigned integers to the internal structure.
   *
   * @param s6addr array of 16 uint8_t
   */
  explicit IPv6Address(const uint8_t s6addr[16]) {
    std::memcpy(address_.s6_addr, s6addr, sizeof(address_.s6_addr));
  }

  /**
   * Constructs a new IPv6Address object using the null-terminated
   * character string or a `std::string` representing the IPv6 address.
   *
   * @throws std::invalid_argument when data could not be converted
   * to an IPv6 address
   * @param data string representing an IPv6 address
   */
  explicit IPv6Address(const char *data);

  /** @overload */
  explicit IPv6Address(const std::string &data) : IPv6Address(data.c_str()) {}

  /** Copy constructor */
  IPv6Address(const IPv6Address &other) : address_(other.address_) {}

  /** Copy assignment */
  IPv6Address &operator=(const IPv6Address &other) {
    if (this != &other) {
      address_ = other.address_;
    }
    return *this;
  }

  /**
   * Returns text representation of the IPv6 address
   *
   * Throws `std::system_error` when it was not possible to
   * get the textual representation of the IPv6 address.
   *
   * @return IPv6 address as a `std::string`
   */
  std::string str() const;

  /**
   * Compare IPv6 addresses for equality
   *
   * @return true if IPv6 addresses are equal
   */
  friend bool operator==(const IPv6Address &a, const IPv6Address &b) {
    return std::memcmp(&a.address_, &b.address_, sizeof(a.address_)) == 0;
  }

  /**
   * Compare IPv6 addresses for inequality
   *
   * @return true if IPv6 addresses are not equal
   */
  friend bool operator!=(const IPv6Address &a, const IPv6Address &b) {
    return !(a == b);
  }

  /**
   * Overload stream insertion operator
   */
  friend std::ostream &operator<<(std::ostream &out,
                                  const IPv6Address &address) {
    out << address.str();
    return out;
  }

 private:
  /** Storage of the IPv6 address */
  in6_addr address_;
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_NETWORKING_IPV4_ADDRESS_INCLUDED
