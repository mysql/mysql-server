/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_TCP_H
#define NDB_TCP_H

#include <ndb_global.h>
#include <ndb_net.h>
#include <ndb_socket.h>
#include <portlib/ndb_socket_poller.h>

#define NDB_SOCKET_TYPE ndb_socket_t

#define NDB_ADDR_STRLEN 512

static inline
void NDB_CLOSE_SOCKET(ndb_socket_t s) {
  my_socket_close(s);
}

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * Convert host name or ip address to in_addr
 *
 * Returns  0 on success
 *         -1 on failure
 *
 * Implemented as:
 *   gethostbyname
 *   if not success
 *      inet_addr
 */
int Ndb_getInAddr(struct in_addr * dst, const char *address);

char* Ndb_inet_ntop(int af,
                    const void *src,
                    char *dst,
                    socklen_t size);

int Ndb_check_socket_hup(NDB_SOCKET_TYPE sock);

#ifdef	__cplusplus
}
#endif

#endif
