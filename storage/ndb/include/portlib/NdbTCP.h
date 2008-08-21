/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NDB_TCP_H
#define NDB_TCP_H

#include <ndb_global.h>
#include <ndb_net.h>
#include <my_socket.h>

#define NDB_SOCKET_TYPE my_socket
#define _NDB_CLOSE_SOCKET(x) my_socket_close(x)
#define InetErrno my_socket_errno()

#define NDB_SOCKLEN_T SOCKET_SIZE_TYPE

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

#ifdef DBUG_OFF
#define NDB_CLOSE_SOCKET(fd) _NDB_CLOSE_SOCKET(fd)
#else
int NDB_CLOSE_SOCKET(my_socket fd);
#endif

int Ndb_check_socket_hup(NDB_SOCKET_TYPE sock);

int setsocknonblock(int socket);
#ifdef NDB_WIN
#define NONBLOCKERR(E) (E!=SOCKET_EAGAIN && E!=SOCKET_EWOULDBLOCK)
#else
#define NONBLOCKERR(E) (E!=EINPROGRESS)
#endif


#ifdef	__cplusplus
}
#endif

#endif
