/*
   Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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
#include <winsock2.h>
using socket_t = SOCKET;
#else
using socket_t = int;
#endif

struct ndb_socket_t {
  socket_t s;
};

/* Include platform-specific inline functions */
#ifdef _WIN32
#include "ndb_socket_win32.h"
#else
#include "ndb_socket_posix.h"
#endif

/* Functions for creating and initializing ndb_socket_t */

static inline
void ndb_socket_init_from_native(ndb_socket_t & ndb_sock, socket_t s)
{
  ndb_sock.s = s;
}

static inline
ndb_socket_t ndb_socket_create_from_native(socket_t native_socket)
{
  ndb_socket_t s;
  ndb_socket_init_from_native(s, native_socket);
  return s;
}

static inline
ndb_socket_t ndb_socket_create()
{
  return ndb_socket_create_from_native(INVALID_SOCKET);
}

static inline socket_t
ndb_socket_get_native(ndb_socket_t s)
{
  return s.s;
}

static inline
void ndb_socket_initialize(ndb_socket_t *s)
{
  s->s= INVALID_SOCKET;
}

static inline
void ndb_socket_invalidate(ndb_socket_t *s)
{
  s->s= INVALID_SOCKET;
}

static inline
int ndb_socket_valid(ndb_socket_t s)
{
  return (s.s != INVALID_SOCKET);
}

// Returns 0 on success, -1 on error
static inline
int ndb_getsockopt(ndb_socket_t s, int level, int optname, int *optval)
{
  socklen_t optlen = sizeof(int);
  int r = getsockopt(s.s, level, optname, (char*)optval, &optlen);
  return r ? -1 : 0;
}

// Returns 0 on success, -1 on error
static inline
int ndb_setsockopt(ndb_socket_t s, int level, int optname, const int *optval)
{
  int r = setsockopt(s.s, level, optname, (const char*)optval, sizeof(int));
  return r ? -1 : 0;
}

// Returns 0 on success, -1 on error
static inline
int ndb_socket_reuseaddr(ndb_socket_t s, int enable)
{
  const int on = enable;
  return ndb_setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on);
}

/* Create an IPv6 socket as used by NDB for network communications,
   allowing mapped IPv4 addresses (socket option IPV6_V6ONLY is false).
*/
static inline
ndb_socket_t ndb_socket_create_dual_stack(int type, int protocol)
{
  ndb_socket_t s = ndb_socket_create();
  ndb_socket_init_from_native(s, socket(AF_INET6, type, protocol));

  if(! ndb_socket_valid(s))
    return s;

  int on = 0;
  if (ndb_setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &on))
  {
    ndb_socket_close(s);
    ndb_socket_invalidate(&s);
  }
  return s;
}

/* Returns 0 on success, -1 on error
   Use ndb_socket_errno() to retrieve error
*/
static inline
int ndb_bind_inet(ndb_socket_t s, const struct sockaddr_in6 *addr)
{
  int r = bind(s.s, (const struct sockaddr*)addr, sizeof(struct sockaddr_in6));
  return r ? -1 : 0;
}

/* Returns 0 on success, -1 on error
   Use ndb_socket_errno() to retrieve error
*/
static inline
int ndb_listen(ndb_socket_t s, int backlog)
{
  int r = listen(s.s, backlog);
  return r ? -1 : 0;
}

/* Returns 0 on success.
   Use ndb_socket_errno() to retrieve error
*/
static inline
ndb_socket_t ndb_accept(ndb_socket_t s, struct sockaddr *addr,
                        socklen_t *addrlen)
{
  return ndb_socket_create_from_native( accept(s.s, addr, addrlen) );
}

/* Returns 0 on success.
   Use ndb_socket_errno() to retrieve error
*/
static inline
int ndb_connect_inet6(ndb_socket_t s, const struct sockaddr_in6 *addr)
{
  return connect(s.s, (const struct sockaddr*) addr,
                 sizeof(struct sockaddr_in6));
}

// Returns 0 on success, 1 on error
static inline
int ndb_getpeername(ndb_socket_t s, struct sockaddr_in6 *addr)
{
  socklen_t len = sizeof(struct sockaddr_in6);
  if(getpeername(s.s, (struct sockaddr*) addr, &len))
    return 1;

  return 0;
}

// Returns 0 on success, 1 on error
static inline
int ndb_getsockname(ndb_socket_t s, struct sockaddr_in6 *addr)
{
  socklen_t len = sizeof(struct sockaddr_in6);
  if(getsockname(s.s, (struct sockaddr*) addr, &len))
    return 1;

  return 0;
}

// Returns 0 on success or ndb_socket_errno() on failure
static inline
int ndb_socket_connect_address(ndb_socket_t s, struct in6_addr *a)
{
  struct sockaddr_in6 addr;
  if(ndb_getpeername(s, &addr) == -1) return ndb_socket_errno();

  *a= addr.sin6_addr;
  return 0;
}

// Returns 0 on success, 1 on error
static inline
int ndb_socket_get_port(ndb_socket_t s, unsigned short *port)
{
  struct sockaddr_in6 servaddr;
  if(ndb_getsockname(s, &servaddr) < 0) return 1;

  *port= ntohs(servaddr.sin6_port);
  return 0;
}

static inline
void ndb_socket_close_with_reset(ndb_socket_t & sock, bool with_reset = false)
{
  if (with_reset)
  {
    // Force hard reset of the socket by turning on linger with timeout 0
    struct linger hard_reset = {1, 0};
    setsockopt(sock.s, SOL_SOCKET, SO_LINGER,
               (char*)&hard_reset, sizeof(hard_reset));
  }

  ndb_socket_close(sock);
}

/* Create a pair of connected sockets.
   Returns 0 on success.
*/
int ndb_socketpair(ndb_socket_t s[2]);

#endif
