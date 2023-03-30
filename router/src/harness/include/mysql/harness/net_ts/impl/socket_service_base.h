/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_NET_TS_IMPL_SOCKET_SERVICE_BASE_H_
#define MYSQL_HARNESS_NET_TS_IMPL_SOCKET_SERVICE_BASE_H_

#include <array>
#include <memory>
#include <system_error>

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>  // addrinfo
#else
#include <netdb.h>  // addrinfo
#endif

#include "mysql/harness/net_ts/impl/file.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/stdx/expected.h"

namespace net {
namespace impl {
namespace socket {

class SocketServiceBase {
 public:
  virtual ~SocketServiceBase() = default;

  virtual stdx::expected<native_handle_type, error_type> socket(
      int family, int sock_type, int protocol) const = 0;

  virtual stdx::expected<std::pair<native_handle_type, native_handle_type>,
                         error_type>
  socketpair(int family, int sock_type, int protocol) const = 0;

  virtual stdx::expected<void, error_type> close(
      native_handle_type native_handle) const = 0;

  virtual stdx::expected<void, error_type> ioctl(
      native_handle_type native_handle, unsigned long cmd,
      void *data) const = 0;

  virtual stdx::expected<bool, error_type> native_non_blocking(
      native_handle_type native_handle) const = 0;

  virtual stdx::expected<void, error_type> native_non_blocking(
      native_handle_type native_handle, bool on) const = 0;

  virtual stdx::expected<void, error_type> listen(
      native_handle_type native_handle, int backlog) const = 0;

  virtual stdx::expected<void, error_type> setsockopt(
      native_handle_type native_handle, int level, int optname,
      const void *optval, socklen_t optlen) const = 0;

  virtual stdx::expected<void, error_type> getsockopt(
      native_handle_type native_handle, int level, int optname, void *optval,
      socklen_t *optlen) const = 0;

  virtual stdx::expected<size_t, error_type> recvmsg(
      native_handle_type native_handle, msghdr_base &msg,
      message_flags flags) const = 0;

  virtual stdx::expected<size_t, error_type> sendmsg(
      native_handle_type native_handle, msghdr_base &msg,
      message_flags flags) const = 0;

  virtual stdx::expected<void, error_type> bind(
      native_handle_type native_handle, const struct sockaddr *addr,
      size_t addr_len) const = 0;

  virtual stdx::expected<void, error_type> connect(
      native_handle_type native_handle, const struct sockaddr *addr,
      size_t addr_len) const = 0;

  virtual stdx::expected<native_handle_type, error_type> accept(
      native_handle_type native_handle, struct sockaddr *addr,
      socklen_t *addr_len) const = 0;

  // freebsd and linux have accept4()
  // solaris and windows don't
  virtual stdx::expected<native_handle_type, error_type> accept4(
      native_handle_type native_handle, struct sockaddr *addr,
      socklen_t *addr_len, int flags = 0) const = 0;

  virtual stdx::expected<void, error_type> getsockname(
      native_handle_type native_handle, struct sockaddr *addr,
      size_t *addr_len) const = 0;
  virtual stdx::expected<void, error_type> getpeername(
      native_handle_type native_handle, struct sockaddr *addr,
      size_t *addr_len) const = 0;

#ifdef __linux__
  virtual stdx::expected<size_t, error_type> splice(native_handle_type fd_in,
                                                    native_handle_type fd_out,
                                                    size_t len,
                                                    int flags) const = 0;
#endif

  virtual stdx::expected<size_t, error_type> splice_to_pipe(
      native_handle_type fd_in, impl::file::file_handle_type fd_out, size_t len,
      int flags) const = 0;

  virtual stdx::expected<size_t, error_type> splice_from_pipe(
      impl::file::file_handle_type fd_in, native_handle_type fd_out, size_t len,
      int flags) const = 0;

  virtual stdx::expected<void, error_type> wait(native_handle_type fd,
                                                wait_type wt) const = 0;

  virtual stdx::expected<void, error_type> shutdown(native_handle_type fd,
                                                    int how) const = 0;

  virtual stdx::expected<
      std::unique_ptr<struct addrinfo, void (*)(struct addrinfo *)>,
      std::error_code>
  getaddrinfo(const char *node, const char *service,
              const addrinfo *hints) const = 0;
};

}  // namespace socket
}  // namespace impl
}  // namespace net

#endif
