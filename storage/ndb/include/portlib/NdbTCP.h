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

#if defined NDB_WIN32

/**
 * Include files needed
 */
#include <winsock2.h>
#include <ws2tcpip.h>

#define InetErrno WSAGetLastError()
#define EWOULDBLOCK WSAEWOULDBLOCK
#define NDB_SOCKET_TYPE SOCKET
#define NDB_INVALID_SOCKET INVALID_SOCKET
#define _NDB_CLOSE_SOCKET(x) closesocket(x)

#else

/**
 * Include files needed
 */
#include <netdb.h>

#define NDB_NONBLOCK O_NONBLOCK
#define NDB_SOCKET_TYPE int
#define NDB_INVALID_SOCKET -1
#define _NDB_CLOSE_SOCKET(x) ::close(x)

#define InetErrno errno

#endif

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
int NDB_CLOSE_SOCKET(int fd);
#endif

int Ndb_check_socket_hup(NDB_SOCKET_TYPE sock);

#ifdef	__cplusplus
}
#endif

#endif
