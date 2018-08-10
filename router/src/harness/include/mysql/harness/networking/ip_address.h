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

#ifndef MYSQL_HARNESS_NETWORKING_IP_ADDRESS_INCLUDED
#define MYSQL_HARNESS_NETWORKING_IP_ADDRESS_INCLUDED

#include "harness_export.h"
#include "mysql/harness/networking/ipv4_address.h"
#include "mysql/harness/networking/ipv6_address.h"

#include <ostream>
#include <string>

namespace mysql_harness {

/**
 * IPAddress for both v4 and v6
 *
 * This class manages IP address of both version 4, and version 6.
 *
 * The following example shows the creation of two `IPAddress` objects
 * for the localhost for both IPv4 and IPv6:
 *
 * @code
 * mysql_harness::IPAddress ip4("127.0.0.1");
 * mysql_harness::IPAddress ip6 = "::1";
 *
 * std::cout << ip4 << " and " << ip6 << std::endl;
 * @endcode
 *
 */
class HARNESS_EXPORT IPAddress {
 public:
  /**
   * Constructs a new IPAddress object as IPv4 and initialized
   * to zero.
   */
  IPAddress()
      : address_type_(AddressType::kIPv4), ipv4_address_(), ipv6_address_() {}

  /**
   * Constructs a new IPAddress object from the given IPv4Address
   * instance.
   *
   * @param address an IPv4Address object
   */
  explicit IPAddress(const IPv4Address &address)
      : address_type_(AddressType::kIPv4),
        ipv4_address_(address),
        ipv6_address_() {}

  /**
   * Constructs a new IPAddress object from the given IPv6Address
   * instance.
   *
   * @param address an IPv6Address object
   */
  explicit IPAddress(const IPv6Address &address)
      : address_type_(AddressType::kIPv6),
        ipv4_address_(),
        ipv6_address_(address) {}

  /**
   * Constructs a new IPAddress object using the null-terminated
   * character string or `std::string` representing an IP address
   * of any type. The address type is deduced by the number of
   * colons `:` in the string.
   *
   * @throws std::invalid_argument when data could not be converted
   * to either an IPv4 or IPv6 address.
   *
   * @param data string representing an IP address
   */
  explicit IPAddress(const std::string &data);

  /** @overload */
  explicit IPAddress(const char *data) : IPAddress(std::string(data)) {}

  /** Copy constructor */
  IPAddress(const IPAddress &other)
      : address_type_(other.address_type_),
        ipv4_address_(other.ipv4_address_),
        ipv6_address_(other.ipv6_address_) {}

  /** Copy assignment */
  IPAddress &operator=(const IPAddress &other) {
    if (this != &other) {
      IPAddress tmp(other);
      swap(tmp);
    }
    return *this;
  }

  /** @overload */
  IPAddress &operator=(const IPv4Address &other) {
    address_type_ = AddressType::kIPv4;
    ipv4_address_ = other;
    return *this;
  }

  /** @overload */
  IPAddress &operator=(const IPv6Address &other) {
    address_type_ = AddressType::kIPv6;
    ipv6_address_ = other;
    return *this;
  }

  /**
   * Exchange data between two IPAddress objects
   *
   * @param other object to exchange with
   */
  void swap(IPAddress &other) noexcept {
    using std::swap;
    swap(address_type_, other.address_type_);
    swap(ipv4_address_, other.ipv4_address_);
    swap(ipv6_address_, other.ipv6_address_);
  }

  /**
   * Return whether address is IPv4
   *
   * @return true if address is IPv4
   */
  bool is_ipv4() const noexcept { return address_type_ == AddressType::kIPv4; }

  /**
   * Return whether address is IPv6
   *
   * @return true if address is IPv6
   */
  bool is_ipv6() const noexcept { return address_type_ == AddressType::kIPv6; }

  const IPv4Address &as_ipv4() const {
    if (address_type_ != AddressType::kIPv4) {
      throw std::runtime_error("address is not IPv4");
    }
    return ipv4_address_;
  }

  const IPv6Address &as_ipv6() const {
    if (address_type_ != AddressType::kIPv6) {
      throw std::runtime_error("address is not IPv6");
    }
    return ipv6_address_;
  }

  /**
   * Returns text representation of the IP address
   *
   * An empty string is returned incase the IP address is neither
   * IPv4 or IPv6.
   *
   * @return IP address as a `std::string`, or empty.
   */
  std::string str() const;

  /**
   * Compare IP addresses
   *
   * @return true if IP addresses are equal
   */
  HARNESS_EXPORT
  friend bool operator==(const IPAddress &a, const IPAddress &b);

  /**
   * Compare IP addresses
   *
   * @return true if IP addresses are not equal
   */
  HARNESS_EXPORT
  friend bool operator!=(const IPAddress &a, const IPAddress &b) {
    return !(a == b);
  }

  /**
   * Overload stream insertion operator
   */
  HARNESS_EXPORT
  friend std::ostream &operator<<(std::ostream &out, const IPAddress &address) {
    out << address.str();
    return out;
  }

 private:
  /** IP address types */
  enum class AddressType {
    kIPv4 = 0,
    kIPv6,
  };

  /** Type of the IP Address */
  AddressType address_type_;

  /** Holds the IPv4 address */
  IPv4Address ipv4_address_;

  /** Holds the IPv6 address */
  IPv6Address ipv6_address_;
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_NETWORKING_IP_ADDRESS_INCLUDED
