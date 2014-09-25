/*
   Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_SOCKET_H
#define NDB_SOCKET_H

#ifdef _WIN32
#include <ndb_socket_win32.h>
#else
#include <ndb_socket_posix.h>
#endif

static inline
void ndb_socket_close(ndb_socket_t sock, bool with_reset = false)
{
  if (with_reset)
  {
    // Force hard reset of the socket by turning on linger
    // with timeout 0
    struct linger hard_reset = {1, 0};
    my_setsockopt(sock, SOL_SOCKET, SO_LINGER,
                  (void*)&hard_reset, sizeof(hard_reset));
  }

  my_socket_close(sock);
}

C_MODE_START

/*
  create a pair of connected sockets
*/
int my_socketpair(ndb_socket_t s[2]);

C_MODE_END

#endif
