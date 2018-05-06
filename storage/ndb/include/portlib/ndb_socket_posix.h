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

#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>

typedef socklen_t ndb_socket_len_t;

#define MY_SOCKET_FORMAT "%d"
#define MY_SOCKET_FORMAT_VALUE(x) (x.fd)

typedef int ndb_native_socket_t;
typedef struct { int fd; } ndb_socket_t;

static inline ndb_native_socket_t
ndb_socket_get_native(ndb_socket_t s)
{
  return s.fd;
}

static inline
int ndb_socket_valid(ndb_socket_t s)
{
  return (s.fd != -1);
}

static inline
ndb_socket_t* ndb_socket_invalidate(ndb_socket_t *s)
{
  s->fd= -1;
  return s;
}

static inline
ndb_socket_t ndb_socket_create_invalid()
{
  ndb_socket_t s;
  ndb_socket_invalidate(&s);
  return s;
}

static inline
int ndb_socket_close(ndb_socket_t s)
{
  struct stat sb;
  if (fstat(s.fd, &sb) == 0)
  {
    if ((sb.st_mode & S_IFMT) != S_IFSOCK)
    {
      fprintf(stderr, "fd=%d: not socket: mode=%o",
              s.fd, sb.st_mode);
      abort();
    }
  }
  return close(s.fd);
}

static inline int ndb_socket_errno()
{
  return errno;
}

static inline
ndb_socket_t ndb_socket_create(int domain, int type, int protocol)
{
  ndb_socket_t s;
  s.fd= socket(domain, type, protocol);

  return s;
}

static inline
ssize_t ndb_recv(ndb_socket_t s, char* buf, size_t len, int flags)
{
  return recv(s.fd, buf, len, flags);
}

static inline
ssize_t ndb_send(ndb_socket_t s, const char* buf, size_t len, int flags)
{
  return send(s.fd, buf, len, flags);
}

static inline
int ndb_socket_reuseaddr(ndb_socket_t s, int enable)
{
  const int on = enable;
  return setsockopt(s.fd, SOL_SOCKET, SO_REUSEADDR,
                    (const void*)&on, sizeof(on));
}

static inline
int ndb_socket_nonblock(ndb_socket_t s, int enable)
{
  int flags;
  flags = fcntl(s.fd, F_GETFL, 0);
  if (flags < 0)
    return flags;

#if defined(O_NONBLOCK)
#define NONBLOCKFLAG O_NONBLOCK
#elif defined(O_NDELAY)
#define NONBLOCKFLAG O_NDELAY
#endif

  if(enable)
    flags |= NONBLOCKFLAG;
  else
    flags &= ~NONBLOCKFLAG;

  if (fcntl(s.fd, F_SETFL, flags) == -1)
    return ndb_socket_errno();

  return 0;
#undef NONBLOCKFLAG
}

static inline
int ndb_bind_inet(ndb_socket_t s, const struct sockaddr_in *addr)
{
  return bind(s.fd, (struct sockaddr*)addr, sizeof(struct sockaddr_in));
}

static inline
int ndb_socket_get_port(ndb_socket_t s, unsigned short *port)
{
  struct sockaddr_in servaddr;
  ndb_socket_len_t sock_len = sizeof(servaddr);
  if(getsockname(s.fd, (struct sockaddr*)&servaddr, &sock_len) < 0) {
    return 1;
  }

  *port= ntohs(servaddr.sin_port);
  return 0;
}

static inline
int ndb_listen(ndb_socket_t s, int backlog)
{
  return listen(s.fd, backlog);
}

static inline
ndb_socket_t ndb_accept(ndb_socket_t s, struct sockaddr *addr,
                       ndb_socket_len_t *addrlen)
{
  ndb_socket_t r;
  r.fd= accept(s.fd, addr, addrlen);
  return r;
}

static inline
int ndb_connect_inet(ndb_socket_t s, const struct sockaddr_in *addr)
{
  return connect(s.fd, (const struct sockaddr*)addr,
                 sizeof(struct sockaddr_in));
}

static inline
int ndb_getsockopt(ndb_socket_t s, int level, int optname,
                   void *optval, ndb_socket_len_t *optlen)
{
  return getsockopt(s.fd, level, optname, optval, optlen);
}

static inline
int ndb_setsockopt(ndb_socket_t s, int level, int optname,
                  void *optval, ndb_socket_len_t optlen)
{
  return setsockopt(s.fd, level, optname, optval, optlen);
}

static inline
int ndb_socket_connect_address(ndb_socket_t s, struct in_addr *a)
{
  struct sockaddr_in addr;
  ndb_socket_len_t addrlen= sizeof(addr);
  if(getpeername(s.fd, (struct sockaddr*)&addr, &addrlen))
    return ndb_socket_errno();

  *a= addr.sin_addr;
  return 0;
}

static inline
int ndb_getpeername(ndb_socket_t s, struct sockaddr *a, ndb_socket_len_t *addrlen)
{
  if(getpeername(s.fd, a, addrlen))
    return ndb_socket_errno();

  return 0;
}

static inline
int ndb_getsockname(ndb_socket_t s, struct sockaddr *a, ndb_socket_len_t *addrlen)
{
  if(getsockname(s.fd, a, addrlen))
    return 1;

  return 0;
}

static inline
ssize_t ndb_socket_readv(ndb_socket_t s, const struct iovec *iov,
                         int iovcnt)
{
  return readv(s.fd, iov, iovcnt);
}

static inline
ssize_t ndb_socket_writev(ndb_socket_t s, const struct iovec *iov,
                          int iovcnt)
{
  return writev(s.fd, iov, iovcnt);
}
