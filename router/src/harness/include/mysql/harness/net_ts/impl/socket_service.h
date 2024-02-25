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

#ifndef MYSQL_HARNESS_NET_TS_IMPL_SOCKET_SERVICE_H_
#define MYSQL_HARNESS_NET_TS_IMPL_SOCKET_SERVICE_H_

#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/impl/socket_service_base.h"
#include "mysql/harness/stdx/expected.h"

namespace net {
namespace impl {
namespace socket {

class SocketService : public SocketServiceBase {
 public:
  stdx::expected<native_handle_type, error_type> socket(
      int family, int sock_type, int protocol) const override {
    return socket::socket(family, sock_type, protocol);
  }

  stdx::expected<std::pair<native_handle_type, native_handle_type>, error_type>
  socketpair(int family, int sock_type, int protocol) const override {
    return socket::socketpair(family, sock_type, protocol);
  }

  stdx::expected<void, std::error_code> close(
      native_handle_type native_handle) const override {
    return socket::close(native_handle);
  }

  stdx::expected<void, error_type> ioctl(native_handle_type native_handle,
                                         unsigned long cmd,
                                         void *data) const override {
    return socket::ioctl(native_handle, cmd, data);
  }

  stdx::expected<bool, error_type> native_non_blocking(
      native_handle_type native_handle) const override {
    return socket::native_non_blocking(native_handle);
  }

  stdx::expected<void, error_type> native_non_blocking(
      native_handle_type native_handle, bool on) const override {
    return socket::native_non_blocking(native_handle, on);
  }

  stdx::expected<void, error_type> listen(native_handle_type native_handle,
                                          int backlog) const override {
    return socket::listen(native_handle, backlog);
  }

  stdx::expected<void, error_type> setsockopt(native_handle_type native_handle,
                                              int level, int optname,
                                              const void *optval,
                                              socklen_t optlen) const override {
    return socket::setsockopt(native_handle, level, optname, optval, optlen);
  }

  stdx::expected<void, error_type> getsockopt(
      native_handle_type native_handle, int level, int optname, void *optval,
      socklen_t *optlen) const override {
    return socket::getsockopt(native_handle, level, optname, optval, optlen);
  }

  stdx::expected<size_t, error_type> recvmsg(
      native_handle_type native_handle, msghdr_base &msg,
      message_flags flags) const override {
    return socket::recvmsg(native_handle, msg, flags);
  }

  stdx::expected<size_t, error_type> sendmsg(
      native_handle_type native_handle, msghdr_base &msg,
      message_flags flags) const override {
    return socket::sendmsg(native_handle, msg, flags);
  }

  stdx::expected<void, error_type> bind(native_handle_type native_handle,
                                        const struct sockaddr *addr,
                                        size_t addr_len) const override {
    return socket::bind(native_handle, addr, addr_len);
  }

  stdx::expected<void, error_type> connect(native_handle_type native_handle,
                                           const struct sockaddr *addr,
                                           size_t addr_len) const override {
    return socket::connect(native_handle, addr, addr_len);
  }

  stdx::expected<native_handle_type, error_type> accept(
      native_handle_type native_handle, struct sockaddr *addr,
      socklen_t *addr_len) const override {
    return socket::accept(native_handle, addr, addr_len);
  }

  // freebsd and linux have accept4()
  // solaris and windows don't
  stdx::expected<native_handle_type, error_type> accept4(
      native_handle_type native_handle, struct sockaddr *addr,
      socklen_t *addr_len, int flags = 0) const override {
    return socket::accept4(native_handle, addr, addr_len, flags);
  }

  stdx::expected<void, error_type> getsockname(
      native_handle_type native_handle, struct sockaddr *addr,
      size_t *addr_len) const override {
    return socket::getsockname(native_handle, addr, addr_len);
  }

  stdx::expected<void, error_type> getpeername(
      native_handle_type native_handle, struct sockaddr *addr,
      size_t *addr_len) const override {
    return socket::getpeername(native_handle, addr, addr_len);
  }

#ifdef __linux__
  stdx::expected<size_t, error_type> splice(native_handle_type fd_in,
                                            native_handle_type fd_out,
                                            size_t len,
                                            int flags) const override {
    return socket::splice(fd_in, fd_out, len, flags);
  }
#endif

  stdx::expected<size_t, error_type> splice_to_pipe(
      native_handle_type fd_in, impl::file::file_handle_type fd_out, size_t len,
      int flags) const override {
    return socket::splice_to_pipe(fd_in, fd_out, len, flags);
  }

  stdx::expected<size_t, error_type> splice_from_pipe(
      impl::file::file_handle_type fd_in, native_handle_type fd_out, size_t len,
      int flags) const override {
    return socket::splice_from_pipe(fd_in, fd_out, len, flags);
  }

  stdx::expected<void, error_type> wait(native_handle_type fd,
                                        wait_type wt) const override {
    return socket::wait(fd, wt);
  }

  stdx::expected<void, error_type> shutdown(native_handle_type fd,
                                            int how) const override {
    return socket::shutdown(fd, how);
  }

  stdx::expected<std::unique_ptr<struct addrinfo, void (*)(struct addrinfo *)>,
                 std::error_code>
  getaddrinfo(const char *node, const char *service,
              const addrinfo *hints) const override {
    return resolver::getaddrinfo(node, service, hints);
  }
};

}  // namespace socket
}  // namespace impl
}  // namespace net

#endif
