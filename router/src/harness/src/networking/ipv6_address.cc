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

#include "mysql/harness/networking/ipv6_address.h"

#include "utilities.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <ws2tcpip.h>  // in6_addr, inet_pton, etc
#endif
#include <cerrno>
#include <cstring>
#include <string>

namespace mysql_harness {

IPv6Address::IPv6Address(const char *data) {
  if (inet_pton(AF_INET6, data, &address_) <= 0) {
    throw std::invalid_argument(std::string("ipv6 parsing error"));
  }
}

std::string IPv6Address::str() const {
  char tmp[INET6_ADDRSTRLEN];

  if (inet_ntop(AF_INET6, const_cast<in6_addr *>(&address_), tmp,
                INET6_ADDRSTRLEN)) {
    return tmp;
  }

  throw std::runtime_error(std::string("inet_ntop failed: ") + strerror(errno));
}

}  // namespace mysql_harness
