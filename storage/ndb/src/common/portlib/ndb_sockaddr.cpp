/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.
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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "portlib/ndb_sockaddr.h"
#include "portlib/NdbTCP.h"
#include "portlib/ndb_socket.h"

int ndb_sockaddr::probe_address_family() {
  /*
   * If one can not resolve IPv6 any (::) address assume IPv4 only.
   *
   * Must initialize dummy with something else than implicit any since we have
   * not yet probed what address family to use for implicit any address.
   */
  [[maybe_unused]] ndb_sockaddr dummy(&in6addr_any, 0);
  if (Ndb_getAddr(&dummy, "::") != 0) return AF_INET;

  ndb_socket_t sock = ndb_socket_create(AF_INET6);
  // Assume failure creating socket is due to AF_INET6 not supported.
  if (!ndb_socket_valid(sock)) return AF_INET;
  ndb_socket_close(sock);
  return AF_INET6;
}
