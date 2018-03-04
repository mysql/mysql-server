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

#include <winsock2.h>
#include <ws2tcpip.h>

/* Link with winsock library */
#pragma comment(lib, "ws2_32")

#include <ndb_global.h>

#define MY_SOCKET_FORMAT "%p"
#define MY_SOCKET_FORMAT_VALUE(x) (x.s)

typedef int ndb_socket_len_t;

typedef SOCKET ndb_native_socket_t;
typedef struct { SOCKET s; } ndb_socket_t;

static inline ndb_native_socket_t
ndb_socket_get_native(ndb_socket_t s)
{
  return s.s;
}

static inline
int ndb_socket_valid(ndb_socket_t s)
{
  return (s.s != INVALID_SOCKET);
}

static inline
ndb_socket_t* ndb_socket_invalidate(ndb_socket_t *s)
{
  s->s= INVALID_SOCKET;
  return s;
}

static inline ndb_socket_t
ndb_socket_create_invalid()
{
  ndb_socket_t s;
  ndb_socket_invalidate(&s);
  return s;
}

static inline
int ndb_socket_close(ndb_socket_t s)
{
  return closesocket(s.s);
}

static inline int ndb_socket_errno()
{
  return WSAGetLastError();
}

static inline
ndb_socket_t ndb_socket_create(int domain, int type,int protocol)
{
  ndb_socket_t s;
  s.s= socket(domain, type, protocol);

  return s;
}

static inline
ssize_t ndb_recv(ndb_socket_t s, char* buf, size_t len, int flags)
{
  int ret= recv(s.s, buf, (int)len, flags);
  if (ret == SOCKET_ERROR)
    return -1;
  return ret;
}

static inline
ssize_t ndb_send(ndb_socket_t s, const char* buf, size_t len, int flags)
{
  int ret= send(s.s, buf, (int)len, flags);
  if (ret == SOCKET_ERROR)
    return -1;
  return ret;
}

static inline
int ndb_socket_reuseaddr(ndb_socket_t s, int enable)
{
  const int on = enable;
  return setsockopt(s.s, SOL_SOCKET, SO_REUSEADDR,
                    (const char*)&on, sizeof(on));
}

static inline
int ndb_socket_nonblock(ndb_socket_t s, int enable)
{
  unsigned long  ul = enable;

  if(ioctlsocket(s.s, FIONBIO, &ul))
    return ndb_socket_errno();

  return 0;
}

static inline
int ndb_bind_inet(ndb_socket_t s, const struct sockaddr_in *addr)
{
  return bind(s.s, (const struct sockaddr*)addr, sizeof(struct sockaddr_in));
}

static inline
int ndb_socket_get_port(ndb_socket_t s, unsigned short *port)
{
  struct sockaddr_in servaddr;
  ndb_socket_len_t sock_len = sizeof(servaddr);
  if(getsockname(s.s, (struct sockaddr*)&servaddr, &sock_len) < 0) {
    return 1;
  }

  *port= ntohs(servaddr.sin_port);
  return 0;
}

static inline
int ndb_listen(ndb_socket_t s, int backlog)
{
  return listen(s.s, backlog);
}

static inline
ndb_socket_t ndb_accept(ndb_socket_t s, struct sockaddr *addr,
                       ndb_socket_len_t *addrlen)
{
  ndb_socket_t r;
  r.s= accept(s.s, addr, addrlen);
  return r;
}

static inline
int ndb_connect_inet(ndb_socket_t s, const struct sockaddr_in *addr)
{
  return connect(s.s, (const struct sockaddr*) addr,
                 sizeof(struct sockaddr_in));
}

static inline
int ndb_getsockopt(ndb_socket_t s, int level, int optname,
                   void *optval, ndb_socket_len_t *optlen)
{
  return getsockopt(s.s, level, optname, (char*)optval, optlen);
}

static inline
int ndb_setsockopt(ndb_socket_t s, int level, int optname,
                   void *optval, ndb_socket_len_t optlen)
{
  return setsockopt(s.s, level, optname, (char*)optval, optlen);
}

static inline
int ndb_socket_connect_address(ndb_socket_t s, struct in_addr *a)
{
  struct sockaddr_in addr;
  ndb_socket_len_t addrlen= sizeof(addr);
  if(getpeername(s.s, (struct sockaddr*)&addr, &addrlen)==SOCKET_ERROR)
    return ndb_socket_errno();

  *a= addr.sin_addr;
  return 0;
}

static inline
int ndb_getpeername(ndb_socket_t s, struct sockaddr *a,
                    ndb_socket_len_t *addrlen)
{
  if(getpeername(s.s, a, addrlen))
    return ndb_socket_errno();

  return 0;
}

static inline
int ndb_getsockname(ndb_socket_t s, struct sockaddr *a,
                                 ndb_socket_len_t *addrlen)
{
  if(getsockname(s.s, a, addrlen))
    return 1;

  return 0;
}

/*
 * NOTE: the order of len and base are *DIFFERENT* on Linux and Win32.
 * casting our iovec to a WSABUF is fine as it's the same structure,
 * just with different names for the members.
 */
struct iovec {
  u_long iov_len;   /* 'u_long len' in WSABUF */
  void*  iov_base;  /* 'char*  buf' in WSABUF */
};

static inline
ssize_t ndb_socket_readv(ndb_socket_t s, const struct iovec *iov,
                         int iovcnt)
{
  DWORD rv=0;
  if (WSARecv(s.s,(LPWSABUF)iov,iovcnt,&rv,0,0,0) == SOCKET_ERROR)
    return -1;
  return rv;
}

static inline
ssize_t ndb_socket_writev(ndb_socket_t s, const struct iovec *iov,
                                       int iovcnt)
{
  DWORD rv=0;
  if (WSASend(s.s,(LPWSABUF)iov,iovcnt,&rv,0,0,0) == SOCKET_ERROR)
    return -1;
  return rv;
}
