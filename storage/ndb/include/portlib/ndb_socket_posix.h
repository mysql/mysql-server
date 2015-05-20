/*
   Copyright (c) 2008, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MY_SOCKET_FORMAT "%d"
#define MY_SOCKET_FORMAT_VALUE(x) (x.fd)

typedef int ndb_native_socket_t;
typedef struct { int fd; } ndb_socket_t;

static inline ndb_native_socket_t
ndb_socket_get_native(ndb_socket_t s)
{
  return s.fd;
}

static inline int my_socket_valid(ndb_socket_t s)
{
  return (s.fd != -1);
}

static inline ndb_socket_t* my_socket_invalidate(ndb_socket_t *s)
{
  s->fd= -1;
  return s;
}

static inline ndb_socket_t my_socket_create_invalid()
{
  ndb_socket_t s;
  my_socket_invalidate(&s);
  return s;
}

static inline int my_socket_get_fd(ndb_socket_t s)
{
  return s.fd;
}

/* implemented in ndb_socket.cpp */
extern int my_socket_close(ndb_socket_t s);

static inline int my_socket_errno()
{
  return errno;
}

static inline void my_socket_set_errno(int error)
{
  errno= error;
}

static inline ndb_socket_t my_socket_create(int domain, int type, int protocol)
{
  ndb_socket_t s;
  s.fd= socket(domain, type, protocol);

  return s;
}

static inline ssize_t my_recv(ndb_socket_t s, char* buf, size_t len, int flags)
{
  return recv(s.fd, buf, len, flags);
}

static inline
ssize_t my_send(ndb_socket_t s, const char* buf, size_t len, int flags)
{
  return send(s.fd, buf, len, flags);
}

static inline int my_socket_reuseaddr(ndb_socket_t s, int enable)
{
  const int on = enable;
  return setsockopt(s.fd, SOL_SOCKET, SO_REUSEADDR,
                    (const void*)&on, sizeof(on));
}

static inline int my_socket_nonblock(ndb_socket_t s, int enable)
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
    return my_socket_errno();

  return 0;
#undef NONBLOCKFLAG
}

static inline int my_bind(ndb_socket_t s, const struct sockaddr *my_addr,
                          SOCKET_SIZE_TYPE len)
{
  return bind(s.fd, my_addr, len);
}

static inline int my_bind_inet(ndb_socket_t s, const struct sockaddr_in *my_addr)
{
  return bind(s.fd, (struct sockaddr*)my_addr, sizeof(struct sockaddr_in));
}

static inline int my_socket_get_port(ndb_socket_t s, unsigned short *port)
{
  struct sockaddr_in servaddr;
  SOCKET_SIZE_TYPE sock_len = sizeof(servaddr);
  if(getsockname(s.fd, (struct sockaddr*)&servaddr, &sock_len) < 0) {
    return 1;
  }

  *port= ntohs(servaddr.sin_port);
  return 0;
}

static inline int my_listen(ndb_socket_t s, int backlog)
{
  return listen(s.fd, backlog);
}

static inline
ndb_socket_t my_accept(ndb_socket_t s, struct sockaddr *addr,
                    SOCKET_SIZE_TYPE *addrlen)
{
  ndb_socket_t r;
  r.fd= accept(s.fd, addr, addrlen);
  return r;
}

static inline int my_connect_inet(ndb_socket_t s, const struct sockaddr_in *addr)
{
  return connect(s.fd, (const struct sockaddr*)addr,
                 sizeof(struct sockaddr_in));
}

static inline
int my_getsockopt(ndb_socket_t s, int level, int optname,
                  void *optval, SOCKET_SIZE_TYPE *optlen)
{
  return getsockopt(s.fd, level, optname, optval, optlen);
}

static inline
int my_setsockopt(ndb_socket_t s, int level, int optname,
                  void *optval, SOCKET_SIZE_TYPE optlen)
{
  return setsockopt(s.fd, level, optname, optval, optlen);
}

static inline int my_socket_connect_address(ndb_socket_t s, struct in_addr *a)
{
  struct sockaddr_in addr;
  SOCKET_SIZE_TYPE addrlen= sizeof(addr);
  if(getpeername(s.fd, (struct sockaddr*)&addr, &addrlen))
    return my_socket_errno();

  *a= addr.sin_addr;
  return 0;
}

static inline int my_getpeername(ndb_socket_t s, struct sockaddr *a, SOCKET_SIZE_TYPE *addrlen)
{
  if(getpeername(s.fd, a, addrlen))
    return my_socket_errno();

  return 0;
}

static inline int my_shutdown(ndb_socket_t s, int how)
{
  return shutdown(s.fd, how);
}

static inline int my_socket_equal(ndb_socket_t s1, ndb_socket_t s2)
{
  return s1.fd==s2.fd;
}

static inline ssize_t my_socket_readv(ndb_socket_t s, const struct iovec *iov,
                                      int iovcnt)
{
  return readv(s.fd, iov, iovcnt);
}
static inline ssize_t my_socket_writev(ndb_socket_t s, const struct iovec *iov,
                                       int iovcnt)
{
  return writev(s.fd, iov, iovcnt);
}
