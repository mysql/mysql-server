/*
   Copyright (c) 2008, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/* Link with winsock library */
#pragma comment(lib, "ws2_32")

#include <Winsock2.h>  // INVALID_SOCKET
#include <ws2tcpip.h>

#include <cstring>
#include <string>

#include <ndb_global.h>
#include "my_stacktrace.h"

using posix_poll_fd = WSAPOLLFD;

using socklen_t = int;

using socket_t = SOCKET;

struct ndb_socket_t {
  socket_t s = INVALID_SOCKET;
};

static inline int ndb_setsockopt(ndb_socket_t, int, int, const int *);

static inline std::string ndb_socket_to_string(ndb_socket_t s) {
  char buff[20];
  std::string str;
  snprintf(buff, sizeof(buff), "%p", (void *)s.s);
  str.assign(buff);
  return str;
}

static inline int ndb_socket_errno() { return WSAGetLastError(); }

static inline std::string ndb_socket_err_message(int error_code) {
  LPTSTR tmp_str = nullptr;
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                    FORMAT_MESSAGE_IGNORE_INSERTS |
                    FORMAT_MESSAGE_MAX_WIDTH_MASK,
                nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&tmp_str, 0, nullptr);
  std::string err_str(tmp_str);
  LocalFree(tmp_str);
  return err_str;
}

static inline int ndb_socket_configure_reuseaddr(ndb_socket_t s, int enable) {
  const int on = enable;
  return ndb_setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, &on);
}

static inline int ndb_socket_shutdown_both(ndb_socket_t s) {
  return shutdown(s.s, SD_BOTH);
}

static inline int ndb_socket_close(ndb_socket_t s) { return closesocket(s.s); }

static inline int ndb_socket_nonblock(ndb_socket_t s, int enable) {
  unsigned long ul = enable;

  if (ioctlsocket(s.s, FIONBIO, &ul)) return ndb_socket_errno();

  return 0;
}

static inline ssize_t ndb_recv(ndb_socket_t s, char *buf, size_t len,
                               int flags) {
  int ret = recv(s.s, buf, (int)len, flags);
  if (ret == SOCKET_ERROR) return -1;
  return ret;
}

static inline ssize_t ndb_send(ndb_socket_t s, const char *buf, size_t len,
                               int flags) {
  int ret = send(s.s, buf, (int)len, flags);
  if (ret == SOCKET_ERROR) return -1;
  return ret;
}

/*
 * NOTE: the order of len and base are *DIFFERENT* on Linux and Win32.
 * casting our iovec to a WSABUF is fine as it's the same structure,
 * just with different names for the members.
 */
struct iovec {
  u_long iov_len; /* 'u_long len' in WSABUF */
  void *iov_base; /* 'char*  buf' in WSABUF */
};

static inline ssize_t ndb_socket_writev(ndb_socket_t s, const struct iovec *iov,
                                        int iovcnt) {
  DWORD rv = 0;
  if (WSASend(s.s, reinterpret_cast<LPWSABUF>(const_cast<struct iovec *>(iov)),
              iovcnt, &rv, 0, nullptr, nullptr) == SOCKET_ERROR)
    return -1;
  return rv;
}

static inline int ndb_poll_sockets(posix_poll_fd *fdarray, unsigned long nfds,
                                   int timeout) {
  if (nfds == 0) {
    Sleep(timeout);
    return 0;  // "timeout occurred"
  }
  int r = WSAPoll(fdarray, nfds, timeout);
  if (r == SOCKET_ERROR) {
    return -1;
  }
  return r;
}
