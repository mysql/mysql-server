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

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "ndb_config.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#include <string>
#include <string.h>   // strerror()

#define INVALID_SOCKET -1

using posix_poll_fd = struct pollfd;

static inline int ndb_socket_reuseaddr(ndb_socket_t, int);

static inline
std::string ndb_socket_to_string(ndb_socket_t s)
{
  return std::to_string(s.s);
}

static inline int ndb_socket_errno()
{
  return errno;
}

static inline
std::string ndb_socket_err_message(int error_code)
{
  std::string err_str(strerror(error_code));
  return err_str;
}

static inline
int ndb_socket_configure_reuseaddr(ndb_socket_t s, int enable)
{
  return ndb_socket_reuseaddr(s, enable);
}

static inline
int ndb_socket_close(ndb_socket_t s)
{
  return close(s.s);
}

static inline
int ndb_socket_nonblock(ndb_socket_t s, int enable)
{
  int flags;
  flags = fcntl(s.s, F_GETFL, 0);
  if (flags < 0)
    return flags;

  if(enable)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;

  if (fcntl(s.s, F_SETFL, flags) == -1)
    return ndb_socket_errno();

  return 0;
}

static inline
ssize_t ndb_recv(ndb_socket_t s, char* buf, size_t len, int flags)
{
  return recv(s.s, buf, len, flags);
}

static inline
ssize_t ndb_send(ndb_socket_t s, const char* buf, size_t len, int flags)
{
  return send(s.s, buf, len, flags);
}

static inline
ssize_t ndb_socket_writev(ndb_socket_t s, const struct iovec *iov, int iovcnt)
{
  return writev(s.s, iov, iovcnt);
}


static inline
int ndb_poll_sockets(posix_poll_fd *fdarray, unsigned long nfds, int timeout)
{
  return poll(fdarray, nfds, timeout);
}
