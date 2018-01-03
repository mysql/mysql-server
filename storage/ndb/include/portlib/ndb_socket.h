/*
   Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_SOCKET_H
#define NDB_SOCKET_H

#ifdef _WIN32
#include "ndb_socket_win32.h"
#else
#include "ndb_socket_posix.h"
#endif


static inline
void ndb_socket_close_with_reset(ndb_socket_t sock, bool with_reset = false)
{
  if (with_reset)
  {
    // Force hard reset of the socket by turning on linger
    // with timeout 0
    struct linger hard_reset = {1, 0};
    ndb_setsockopt(sock, SOL_SOCKET, SO_LINGER,
                  (void*)&hard_reset, sizeof(hard_reset));
  }

  ndb_socket_close(sock);
}

// Create ndb_socket_t given ndb_native_socket_t
static inline
ndb_socket_t ndb_socket_create_from_native(ndb_native_socket_t native_socket)
{
  ndb_socket_t s;
#ifdef _WIN32
  s.s = native_socket;
#else
  s.fd = native_socket;
#endif
  return s;
}

/*
  create a pair of connected sockets
*/
int ndb_socketpair(ndb_socket_t s[2]);

#endif
