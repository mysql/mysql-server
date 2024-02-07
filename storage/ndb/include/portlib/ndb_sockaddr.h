/*
   Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_SOCKADDR_H
#define NDB_SOCKADDR_H

#include <cassert>
#include <cstdlib>
#include <cstring>
#include "util/require.h"

#ifdef _WIN32
#include <Winsock2.h>
#include <Ws2ipdef.h>
using socklen_t = int;
#else
#include <arpa/inet.h>   // htons, ntohs
#include <netinet/in.h>  // sockaddr, INADDR_ANY, IN6_IS_ADDR_V4MAPPED, ...
#include <sys/socket.h>  // AF_INET, AF_INET6, AF_UNSPEC, PF_INET, PF_INET6
#endif

using std::abort;
using std::memcmp;
using std::memcpy;

/*
 * ndb_sockaddr - wrapper of sockaddr_in and sockaddr_in6
 */
class ndb_sockaddr {
 public:
  union storage_type {
    sockaddr common;
    sockaddr_in in4;
    sockaddr_in6 in6;
  };

  ndb_sockaddr();                              // unspecified address
  explicit ndb_sockaddr(unsigned short port);  // unspecified address
  ndb_sockaddr(const in_addr *addr, unsigned short port);
  ndb_sockaddr(const in6_addr *addr, unsigned short port);
  ndb_sockaddr(const sockaddr *addr, socklen_t len);
  explicit ndb_sockaddr(const sockaddr_in *addr);
  explicit ndb_sockaddr(const sockaddr_in6 *addr);
  ndb_sockaddr(const ndb_sockaddr &oth) : sa(oth.sa) {}
  ndb_sockaddr &operator=(const ndb_sockaddr &oth);
  /*
   * No operator== due to ambiguity what user would expect.
   * For example should an sockaddr_in and a sockaddr_in6 structure be
   * considered equal if the actual stored IPv4 address (and port) is equal?
   * Or should two sockaddr_in6 compare equal even if flowinfo is different?
   * And sometimes caller may assume that only address part is compared,
   * ignoring port which could cause strange appearing results.
   */

  socklen_t get_sockaddr_len() const;
  const sockaddr *get_sockaddr() const;
  int get_in_addr(in_addr *addr) const;
  int get_in6_addr(in6_addr *addr) const;
  int get_port() const;
  int get_address_family() const { return sa.common.sa_family; }
  int get_protocol_family() const;
  static int get_address_family_for_unspecified_address();
  bool has_port() const { return (get_port() > 0); }
  bool has_same_addr(const ndb_sockaddr &oth) const;
  bool is_loopback() const;
  bool is_unspecified() const;
  bool need_dual_stack() const;
  int set_port(unsigned short port);
  static int set_address_family_for_unspecified_address(int af);

 private:
  static int probe_address_family();
  static int set_get_address_family_for_unspecified_address(int af);

  storage_type sa;
};

inline ndb_sockaddr::ndb_sockaddr() : sa{} {
  require(get_address_family_for_unspecified_address() != AF_UNSPEC);
  if (get_address_family_for_unspecified_address() == AF_INET6) {
    sa.in6.sin6_family = AF_INET6;
    sa.in6.sin6_addr = in6addr_any;
  } else if (get_address_family_for_unspecified_address() == AF_INET) {
    sa.in4.sin_family = AF_INET;
    sa.in4.sin_addr.s_addr = htonl(INADDR_ANY);
  } else {
    abort();
  }
}

inline ndb_sockaddr::ndb_sockaddr(unsigned short port) : sa{} {
  require(get_address_family_for_unspecified_address() != AF_UNSPEC);
  if (get_address_family_for_unspecified_address() == AF_INET6) {
    sa.in6.sin6_family = AF_INET6;
    sa.in6.sin6_port = htons(port);
    sa.in6.sin6_addr = in6addr_any;
  } else if (get_address_family_for_unspecified_address() == AF_INET) {
    sa.in4.sin_family = AF_INET;
    sa.in4.sin_port = htons(port);
    sa.in4.sin_addr.s_addr = htonl(INADDR_ANY);
  } else {
    abort();
  }
}

inline ndb_sockaddr::ndb_sockaddr(const in_addr *addr, unsigned short port)
    : sa{} {
  sa.in4.sin_family = AF_INET;
  sa.in4.sin_port = htons(port);
  sa.in4.sin_addr = *addr;
}

inline ndb_sockaddr::ndb_sockaddr(const in6_addr *addr, unsigned short port)
    : sa{} {
  sa.in6.sin6_family = AF_INET6;
  sa.in6.sin6_port = htons(port);
  sa.in6.sin6_addr = *addr;
}

inline ndb_sockaddr::ndb_sockaddr(const sockaddr *addr, socklen_t len) : sa{} {
  if (addr->sa_family == AF_INET6) {
    require(len == sizeof(sockaddr_in6));
    sa.in6 = *(const sockaddr_in6 *)addr;
  } else if (addr->sa_family == AF_INET) {
    require(len == sizeof(sockaddr_in));
    sa.in4 = *(const sockaddr_in *)addr;
  } else {
    abort();
  }
}

inline ndb_sockaddr::ndb_sockaddr(const sockaddr_in *addr) : sa{} {
  require(sa.common.sa_family == AF_INET);
  sa.in4 = *addr;
}

inline ndb_sockaddr::ndb_sockaddr(const sockaddr_in6 *addr) : sa{} {
  require(sa.common.sa_family == AF_INET6);
  sa.in6 = *addr;
}

inline ndb_sockaddr &ndb_sockaddr::operator=(const ndb_sockaddr &oth) {
  sa = oth.sa;
  return *this;
}

inline socklen_t ndb_sockaddr::get_sockaddr_len() const {
  if (sa.common.sa_family == AF_INET6) return sizeof(sa.in6);
  if (sa.common.sa_family == AF_INET) return sizeof(sa.in4);
  abort();
}

inline const sockaddr *ndb_sockaddr::get_sockaddr() const {
  if (sa.common.sa_family == AF_INET6) return (const sockaddr *)(&sa.in6);
  if (sa.common.sa_family == AF_INET) return (const sockaddr *)(&sa.in4);
  abort();
}

inline int ndb_sockaddr::get_in_addr(in_addr *addr) const {
  if (sa.common.sa_family == AF_INET) {
    *addr = sa.in4.sin_addr;
    return 0;
  }
  if (!IN6_IS_ADDR_V4MAPPED(&sa.in6.sin6_addr)) return -1;
  memcpy(addr, &sa.in6.sin6_addr.s6_addr[12], sizeof(in_addr));
  return 0;
}

inline int ndb_sockaddr::get_in6_addr(in6_addr *addr) const {
  if (sa.common.sa_family != AF_INET6) return -1;
  *addr = sa.in6.sin6_addr;
  return 0;
}

inline int ndb_sockaddr::get_port() const {
  if (sa.common.sa_family == AF_INET6) return ntohs(sa.in6.sin6_port);
  if (sa.common.sa_family == AF_INET) return ntohs(sa.in4.sin_port);
  abort();
}

inline int ndb_sockaddr::get_protocol_family() const {
  if (sa.common.sa_family == AF_INET) return PF_INET;
  if (IN6_IS_ADDR_V4MAPPED(&sa.in6.sin6_addr)) return PF_INET;
  return PF_INET6;
}

inline bool ndb_sockaddr::has_same_addr(const ndb_sockaddr &oth) const {
  if (sa.common.sa_family == AF_INET || oth.sa.common.sa_family == AF_INET) {
    in_addr a[2];
    if (get_in_addr(&a[0]) == -1) return false;
    if (oth.get_in_addr(&a[1]) == -1) return false;
    return (a[0].s_addr == a[1].s_addr);
  }
  return (memcmp(&sa.in6.sin6_addr, &oth.sa.in6.sin6_addr, sizeof(in6_addr)) ==
          0) &&
         (sa.in6.sin6_scope_id == oth.sa.in6.sin6_scope_id);
}

inline bool ndb_sockaddr::is_loopback() const {
  if (sa.common.sa_family == AF_INET)
    return (sa.in4.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
  require(sa.common.sa_family == AF_INET6);
  if (!IN6_IS_ADDR_V4MAPPED(&sa.in6.sin6_addr))
    return IN6_IS_ADDR_LOOPBACK(&sa.in6.sin6_addr);
  in_addr in4;
  memcpy(&in4, &sa.in6.sin6_addr.s6_addr[12], sizeof(in4));
  return (in4.s_addr == htonl(INADDR_LOOPBACK));
}

inline bool ndb_sockaddr::is_unspecified() const {
  if (sa.common.sa_family == AF_INET) {
    return (sa.in4.sin_addr.s_addr == htonl(INADDR_ANY));
  }
  if (!IN6_IS_ADDR_V4MAPPED(&sa.in6.sin6_addr))
    return IN6_IS_ADDR_UNSPECIFIED(&sa.in6.sin6_addr);
  in_addr in = *(const in_addr *)&sa.in6.sin6_addr.s6_addr[12];
  return (in.s_addr == htonl(INADDR_ANY));
}

inline bool ndb_sockaddr::need_dual_stack() const {
  if (sa.common.sa_family != AF_INET6) return false;
  if (IN6_IS_ADDR_UNSPECIFIED(&sa.in6.sin6_addr)) return true;
  if (IN6_IS_ADDR_V4MAPPED(&sa.in6.sin6_addr)) return true;
  return false;
}

inline int ndb_sockaddr::set_port(unsigned short port) {
  if (sa.common.sa_family == AF_INET6) {
    sa.in6.sin6_port = htons(port);
    return 0;
  }
  if (sa.common.sa_family == AF_INET) {
    sa.in4.sin_port = htons(port);
    return 0;
  }
  return -1;
}

inline int ndb_sockaddr::get_address_family_for_unspecified_address() {
  return set_get_address_family_for_unspecified_address(-1);
}

inline int ndb_sockaddr::set_address_family_for_unspecified_address(int af) {
  assert(af != -1);
  return set_get_address_family_for_unspecified_address(af);
}

inline int ndb_sockaddr::set_get_address_family_for_unspecified_address(
    int af) {
  static int address_family_for_unspecified_address = probe_address_family();
  if (af != -1) address_family_for_unspecified_address = af;
  return address_family_for_unspecified_address;
}

#endif
