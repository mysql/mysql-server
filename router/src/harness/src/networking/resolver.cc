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

#include "mysql/harness/networking/resolver.h"
#include "mysql/harness/networking/ip_address.h"
#include "mysql/harness/networking/ipv4_address.h"
#include "mysql/harness/networking/ipv6_address.h"

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#endif
#include <sys/types.h>
#include <algorithm>
#include <string>
#include <vector>

#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif

namespace mysql_harness {

std::vector<IPAddress> Resolver::hostname(const char *name) const {
  struct addrinfo hints, *result;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if (auto err = getaddrinfo(name, nullptr, &hints, &result) != 0) {
    throw std::invalid_argument(std::string("hostname resolve failed for ") +
                                name + ": " + gai_strerror(err));
  }

  std::vector<IPAddress> result_ips{};
  struct addrinfo *res;
  for (res = result; res; res = res->ai_next) {
    if (res->ai_family == AF_INET) {
      // IPv4
      result_ips.emplace_back(
          IPv4Address(((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr));
    } else if (res->ai_family == AF_INET6) {
      // IPv6
      result_ips.emplace_back(IPv6Address(
          ((struct sockaddr_in6 *)res->ai_addr)->sin6_addr.s6_addr));
    }
  }

  freeaddrinfo(result);

  return result_ips;
}

uint16_t Resolver::tcp_service_name(const char *name) const {
  if (uint16_t cached = cached_tcp_service_by_name(name)) {
    return cached;
  }

  struct servent *servinfo = getservbyname(name, "tcp");

  if (!servinfo) {
    throw std::invalid_argument(
        std::string("service name resolve failed for ") + name);
  }

  uint16_t port = htons(static_cast<uint16_t>(servinfo->s_port));
  cache_tcp_services_.push_back(std::make_pair(port, std::string(name)));

  return port;
}

std::string Resolver::tcp_service_port(uint16_t port) const {
  std::string cached = cached_tcp_service_by_port(port);
  if (!cached.empty()) {
    return cached;
  }

  struct servent *servinfo = getservbyport(htons(port), "tcp");

  if (!servinfo) {
    // we cache the empty string so we don't look it up later
    cache_tcp_services_.push_back(std::make_pair(port, std::string()));
    // we simply return the port number as string
    return std::to_string(port);
  }

  std::string service_name(servinfo->s_name);
  cache_tcp_services_.push_back(std::make_pair(port, service_name));

  return service_name;
}

uint16_t Resolver::cached_tcp_service_by_name(const std::string &name) const {
  auto result = std::find_if(
      cache_tcp_services_.begin(), cache_tcp_services_.end(),
      [&name](ServiceCacheEntry service) { return service.second == name; });

  if (result == cache_tcp_services_.end()) {
    return 0;
  }

  return result->first;
}

std::string Resolver::cached_tcp_service_by_port(uint16_t port) const {
  auto result = std::find_if(
      cache_tcp_services_.begin(), cache_tcp_services_.end(),
      [&port](ServiceCacheEntry service) { return service.first == port; });

  if (result == cache_tcp_services_.end()) {
    return {};
  }

  return result->second;
}

}  // namespace mysql_harness
