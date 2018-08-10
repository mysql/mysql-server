/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_HARNESS_TCPADDRESS_INCLUDED
#define MYSQL_HARNESS_TCPADDRESS_INCLUDED

#include <cstdint>
#include <string>

#include "harness_export.h"

namespace mysql_harness {

/** @brief Defines an IP address with port number  */
class HARNESS_EXPORT TCPAddress {
 public:
  enum class Family {
    UNKNOWN = 0,
    IPV4 = 1,
    IPV6 = 2,
    INVALID = 9,
  };

  TCPAddress(const std::string &address = "", uint32_t tcp_port = 0)
      : addr(address),
        port(validate_port(tcp_port)),
        ip_family_(Family::UNKNOWN) {
    detect_family();
  }

  /** @brief Copy constructor */
  TCPAddress(const TCPAddress &other)
      : addr(other.addr), port(other.port), ip_family_(other.ip_family_) {}

  /** @brief Move constructor */
  TCPAddress(TCPAddress &&other)
      : addr(std::move(other.addr)),
        port(other.port),
        ip_family_(other.ip_family_) {}

  /** @brief Copy assignment */
  TCPAddress &operator=(const TCPAddress &other) {
    std::string *my_addr = const_cast<std::string *>(&this->addr);
    *my_addr = other.addr;
    uint16_t *my_port = const_cast<uint16_t *>(&this->port);
    *my_port = other.port;
    Family *my_family = const_cast<Family *>(&this->ip_family_);
    *my_family = other.ip_family_;
    return *this;
  }

  /** @brief Move assignment */
  TCPAddress &operator=(TCPAddress &&other) {
    std::string *my_addr = const_cast<std::string *>(&this->addr);
    *my_addr = other.addr;
    uint16_t *my_port = const_cast<uint16_t *>(&this->port);
    *my_port = other.port;
    Family *my_family = const_cast<Family *>(&this->ip_family_);
    *my_family = other.ip_family_;
    return *this;
  }

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
    return (left.addr == right.addr) && (left.port == right.port);
  }

  /**
   * @brief Function for performing comparision of TCPAddresses
   */
  friend bool operator<(const TCPAddress &left, const TCPAddress &right) {
    if (left.addr < right.addr)
      return true;
    else if (left.addr > right.addr)
      return false;
    return left.port < right.port;
  }

  /** @brief Returns whether the TCPAddress is valid
   *
   * Returns whether the address and port are valid. This function also
   * detects the family when it was still Family::UNKNOWN.
   */
  bool is_valid() noexcept;

  /** @brief Returns whether the TCPAddress is IPv4
   *
   * Returns true when the address is IPv4; false
   * when it is IPv6.
   */
  bool is_ipv4();

  template <Family T>
  bool is_family() {
    if (ip_family_ == T) {
      return true;
    }
    return false;
  }

  /* @brief Returns the address family
   *
   * returns TCPAddress::Family
   */
  Family get_family() const noexcept { return ip_family_; }

  /** @brief Network name IP */
  const std::string addr;
  /** @brief TCP port */
  const uint16_t port;

 private:
  /** @brief Initialize the address family */
  void detect_family() noexcept;

  /** @brief Validates the given port number */
  uint16_t validate_port(uint32_t tcp_port);

  /** @brief Address family for this IP Address */
  Family ip_family_;
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_TCPADDRESS_INCLUDED
