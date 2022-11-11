/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_NET_TS_IMPL_SOCKET_H_
#define MYSQL_HARNESS_NET_TS_IMPL_SOCKET_H_

#include <algorithm>  // max
#include <array>
#include <bitset>
#include <cinttypes>  // PRIx64
#include <limits>
#include <system_error>

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#ifdef AF_UNIX
#include <afunix.h>
#endif
#else
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>  // close
#endif

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/file.h"
#include "mysql/harness/net_ts/impl/poll.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/stdx/expected.h"

#include "scope_guard.h"

namespace net {
namespace impl {
namespace socket {

inline stdx::expected<native_handle_type, error_type> socket(int family,
                                                             int sock_type,
                                                             int protocol) {
  native_handle_type sock = ::socket(family, sock_type, protocol);

  if (sock == kInvalidSocket) {
    return stdx::make_unexpected(last_error_code());
  }

  return sock;
}

inline stdx::expected<void, std::error_code> close(
    native_handle_type native_handle) {
#ifdef _WIN32
  if (SOCKET_ERROR == ::closesocket(native_handle)) {
    return stdx::make_unexpected(last_error_code());
  }
#else
  if (0 != ::close(native_handle)) {
    return stdx::make_unexpected(last_error_code());
  }
#endif
  return {};
}

inline stdx::expected<void, error_type> ioctl(native_handle_type native_handle,
                                              unsigned long cmd, void *data) {
#ifdef _WIN32
  // use WSAIoctl() instead ?
  if (kSocketError == ::ioctlsocket(native_handle, static_cast<long>(cmd),
                                    reinterpret_cast<unsigned long *>(data))) {
    return stdx::make_unexpected(last_error_code());
  }
#else
  if (kSocketError == ::ioctl(native_handle, cmd, data)) {
    return stdx::make_unexpected(last_error_code());
  }
#endif

  return {};
}

inline stdx::expected<bool, error_type> native_non_blocking(
    native_handle_type native_handle) {
#ifdef _WIN32
  (void)native_handle;
  // windows has no way to query to the blocking state
  return stdx::make_unexpected(
      std::make_error_code(std::errc::function_not_supported));
#else
  auto res = impl::file::fcntl(native_handle, impl::file::get_file_status());
  if (!res) return stdx::make_unexpected(res.error());

  return (*res & O_NONBLOCK) != 0;
#endif
}

inline stdx::expected<void, error_type> native_non_blocking(
    native_handle_type native_handle, bool on) {
#ifdef _WIN32
  unsigned long nonblocking{on};

  return ioctl(native_handle, FIONBIO, &nonblocking);
#else
  auto res = impl::file::fcntl(native_handle, impl::file::get_file_status());
  if (!res) return stdx::make_unexpected(res.error());

  int flags = *res;

  if (on) {
    if (flags & O_NONBLOCK) return {};
    flags |= O_NONBLOCK;
  } else {
    if (!(flags & O_NONBLOCK)) return {};
    flags &= ~O_NONBLOCK;
  }

  auto set_res =
      impl::file::fcntl(native_handle, impl::file::set_file_status(flags));
  if (!set_res) return stdx::make_unexpected(set_res.error());

  return {};
#endif
}

inline stdx::expected<void, error_type> listen(native_handle_type native_handle,
                                               int backlog) {
  if (kSocketError == ::listen(native_handle, backlog)) {
    return stdx::make_unexpected(last_error_code());
  }

  return {};
}

inline stdx::expected<void, error_type> setsockopt(
    native_handle_type native_handle, int level, int optname,
    const void *optval, socklen_t optlen) {
#ifdef _WIN32
  int res = ::setsockopt(native_handle, level, optname,
                         reinterpret_cast<const char *>(optval), optlen);
#else
  int res = ::setsockopt(native_handle, level, optname, optval, optlen);
#endif
  if (kSocketError == res) {
    return stdx::make_unexpected(last_error_code());
  }

  return {};
}

inline stdx::expected<void, error_type> getsockopt(
    native_handle_type native_handle, int level, int optname, void *optval,
    socklen_t *optlen) {
#ifdef _WIN32
  int res = ::getsockopt(native_handle, level, optname,
                         reinterpret_cast<char *>(optval), optlen);
#else
  int res = ::getsockopt(native_handle, level, optname, optval, optlen);
#endif
  if (kSocketError == res) {
    return stdx::make_unexpected(last_error_code());
  }

  return {};
}

/**
 * wrap ::recv() in a portable way.
 *
 * @param native_handle socket handle
 * @param buf pointer to a mutable buffer of size 'buf_len'
 * @param buf_len size of 'buf'
 * @param flags message flags
 * @returns bytes transferred on success, std::error_code otherwise
 */
inline stdx::expected<size_t, error_type> recv(native_handle_type native_handle,
                                               void *buf, size_t buf_len,
                                               message_flags flags) {
#if defined(_WIN32)
  // recv() expects an 'int' instead of a 'size_t'.
  // Ensure, buf_len is properly narrowed instead of wrapped around
  auto bytes_transferred = ::recv(
      native_handle, static_cast<char *>(buf),
      std::min(static_cast<size_t>(std::numeric_limits<int>::max()), buf_len),
      flags.to_ulong());
#else
  auto bytes_transferred =
      ::recv(native_handle, buf, buf_len, static_cast<int>(flags.to_ulong()));
#endif
  if (kSocketError == bytes_transferred) {
    return stdx::make_unexpected(last_error_code());
  }

  return {static_cast<size_t>(bytes_transferred)};
}

inline stdx::expected<size_t, error_type> read(native_handle_type native_handle,
                                               void *data, size_t data_len) {
#ifdef _WIN32
  // fallback to recv()
  return recv(native_handle, data, data_len, 0);
#else
  auto bytes_transferred = ::read(native_handle, data, data_len);
  if (kSocketError == bytes_transferred) {
    return stdx::make_unexpected(last_error_code());
  }

  return {static_cast<size_t>(bytes_transferred)};
#endif
}

inline stdx::expected<size_t, error_type> recvmsg(
    native_handle_type native_handle, msghdr_base &msg, message_flags flags) {
#ifdef _WIN32
  DWORD bytes_transferred;
  DWORD _flags = flags.to_ulong();

  // WSARecvMsg() also exists, but is less flexible and is only reachable
  // via a function-pointer-lookup via WSAID_WSARECVMSG
  int err = ::WSARecvFrom(native_handle, msg.lpBuffers, msg.dwBufferCount,
                          &bytes_transferred, &_flags,
                          msg.name,      // from
                          &msg.namelen,  // from_len
                          nullptr,       // overlapped
                          nullptr        // completor
  );
  if (kSocketError == err) {
    return stdx::make_unexpected(last_error_code());
  }
#else
  ssize_t bytes_transferred =
      ::recvmsg(native_handle, &msg, static_cast<int>(flags.to_ulong()));
  if (kSocketError == bytes_transferred) {
    return stdx::make_unexpected(last_error_code());
  }
#endif

  return {static_cast<size_t>(bytes_transferred)};
}

/**
 * wrap ::send() in a portable way.
 *
 * @param native_handle socket handle
 * @param buf pointer to a const buffer of size 'buf_len'
 * @param buf_len size of 'buf'
 * @param flags message flags
 * @returns bytes transferred on success, std::error_code otherwise
 */
inline stdx::expected<size_t, error_type> send(native_handle_type native_handle,
                                               const void *buf, size_t buf_len,
                                               message_flags flags) {
#if defined(_WIN32)
  // send() expects an 'int' instead of a 'size_t'.
  // Ensure, buf_len is properly narrowed instead of wrapped around
  auto bytes_transferred = ::send(
      native_handle, static_cast<const char *>(buf),
      std::min(static_cast<size_t>(std::numeric_limits<int>::max()), buf_len),
      flags.to_ulong());
#else
  // ssize_t
  auto bytes_transferred =
      ::send(native_handle, buf, buf_len, static_cast<int>(flags.to_ulong()));
#endif
  if (kSocketError == bytes_transferred) {
    return stdx::make_unexpected(last_error_code());
  }

  return {static_cast<size_t>(bytes_transferred)};
}

inline stdx::expected<size_t, error_type> write(
    native_handle_type native_handle, const void *data, size_t data_len) {
#ifdef _WIN32
  // fallback to send()
  return send(native_handle, data, data_len, 0);
#else
  auto bytes_transferred = ::write(native_handle, data, data_len);
  if (kSocketError == bytes_transferred) {
    return stdx::make_unexpected(last_error_code());
  }

  return {static_cast<size_t>(bytes_transferred)};
#endif
}

inline stdx::expected<size_t, error_type> sendmsg(
    native_handle_type native_handle, msghdr_base &msg, message_flags flags) {
#ifdef _WIN32
  DWORD bytes_transferred;
  DWORD _flags = flags.to_ulong();
  int err = ::WSASendTo(native_handle, msg.lpBuffers, msg.dwBufferCount,
                        &bytes_transferred, _flags,
                        msg.name,     // to
                        msg.namelen,  // to_len
                        nullptr,      // overlapped
                        nullptr       // completor
  );
  if (kSocketError == err) {
    return stdx::make_unexpected(last_error_code());
  }
#else
  ssize_t bytes_transferred =
      ::sendmsg(native_handle, &msg, static_cast<int>(flags.to_ulong()));
  if (kSocketError == bytes_transferred) {
    return stdx::make_unexpected(last_error_code());
  }
#endif

  return {static_cast<size_t>(bytes_transferred)};
}

/**
 * wrap ::bind() in a portable way.
 */
inline stdx::expected<void, error_type> bind(native_handle_type native_handle,
                                             const struct sockaddr *addr,
                                             size_t addr_len) {
  if (kSocketError ==
      ::bind(native_handle, addr, static_cast<socklen_t>(addr_len))) {
    return stdx::make_unexpected(last_error_code());
  }

  return {};
}

/**
 * wrap ::connect() in a portable way.
 */
inline stdx::expected<void, error_type> connect(
    native_handle_type native_handle, const struct sockaddr *addr,
    size_t addr_len) {
  if (kSocketError ==
      ::connect(native_handle, addr, static_cast<socklen_t>(addr_len))) {
    return stdx::make_unexpected(last_error_code());
  }

  return {};
}

/**
 * wrap ::accept() in a portable way.
 */
inline stdx::expected<native_handle_type, error_type> accept(
    native_handle_type native_handle, struct sockaddr *addr,
    socklen_t *addr_len) {
  native_handle_type fd = ::accept(native_handle, addr, addr_len);
  if (kInvalidSocket == fd) {
    return stdx::make_unexpected(last_error_code());
  }

  return fd;
}

// freebsd and linux have accept4()
// solaris and windows don't
inline stdx::expected<native_handle_type, error_type> accept4(
    native_handle_type native_handle, struct sockaddr *addr,
    socklen_t *addr_len, int flags = 0) {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
  native_handle_type fd = ::accept4(native_handle, addr, addr_len, flags);
  if (kInvalidSocket == fd) {
    return stdx::make_unexpected(last_error_code());
  }

  return fd;
#else
  // static_assert(false, "operation not supported");
  (void)native_handle;
  (void)addr;
  (void)addr_len;
  (void)flags;

  return stdx::make_unexpected(
      make_error_code(std::errc::operation_not_supported));
#endif
}

inline stdx::expected<void, error_type> getsockname(
    native_handle_type native_handle, struct sockaddr *addr, size_t *addr_len) {
#ifdef _WIN32
  socklen_t len = static_cast<socklen_t>(*addr_len);
#else
  socklen_t len = *addr_len;
#endif

  if (kSocketError == ::getsockname(native_handle, addr, &len)) {
    return stdx::make_unexpected(last_error_code());
  }

#ifdef _WIN32
  *addr_len = static_cast<size_t>(len);
#else
  *addr_len = len;
#endif

  return {};
}

inline stdx::expected<void, error_type> getpeername(
    native_handle_type native_handle, struct sockaddr *addr, size_t *addr_len) {
#ifdef _WIN32
  socklen_t len = static_cast<socklen_t>(*addr_len);
#else
  socklen_t len = *addr_len;
#endif

  if (kSocketError == ::getpeername(native_handle, addr, &len)) {
    return stdx::make_unexpected(last_error_code());
  }

#ifdef _WIN32
  *addr_len = static_cast<size_t>(len);
#else
  *addr_len = len;
#endif

  return {};
}

/**
 * socketpair().
 *
 * - wraps socketpair() on POSIX
 * - emulates socketpair() on windows as winsock2() provides no socketpair.
 */
inline stdx::expected<std::pair<native_handle_type, native_handle_type>,
                      error_type>
socketpair(int family, int sock_type, int protocol) {
#if !defined(_WIN32)
  std::array<native_handle_type, 2> fds;

  if (0 != ::socketpair(family, sock_type, protocol, fds.data())) {
    return stdx::make_unexpected(last_error_code());
  }

  return std::make_pair(fds[0], fds[1]);
#else
  auto listener_res = impl::socket::socket(family, sock_type, protocol);
  if (!listener_res) return stdx::make_unexpected(listener_res.error());

  auto listener = listener_res.value();

  Scope_guard listener_guard([listener]() {
#if defined(AF_UNIX)
    struct sockaddr_storage ss {};
    size_t ss_len = sizeof(ss);

    auto name_res = impl::socket::getsockname(
        listener, reinterpret_cast<sockaddr *>(&ss), &ss_len);
    if (name_res) {
      if (ss.ss_family == AF_UNIX) {
        struct sockaddr_un *su = reinterpret_cast<sockaddr_un *>(&ss);

        // delete the named socket
        DeleteFile(su->sun_path);
      }
    }
#endif

    impl::socket::close(listener);
  });

  stdx::expected<void, std::error_code> bind_res;

  switch (family) {
    case AF_INET: {
      int reuse = 1;
      impl::socket::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse,
                               sizeof(reuse));

      struct sockaddr_in sa {};
      size_t sa_len = sizeof(sa);

      sa.sin_family = family;
      sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      sa.sin_port = 0;  // pick a random port

      bind_res = impl::socket::bind(listener, reinterpret_cast<sockaddr *>(&sa),
                                    sa_len);
    } break;
#if defined(AF_UNIX)
    case AF_UNIX: {
      struct sockaddr_un sa {};
      size_t sa_len = sizeof(sa);

      sa.sun_family = family;

      DWORD path_len = GetTempPath(UNIX_PATH_MAX, sa.sun_path);

      // use the current dir if the tmppath is too long.
      if (path_len >= UNIX_PATH_MAX - 9) path_len = 0;

      LARGE_INTEGER ticks;
      QueryPerformanceCounter(&ticks);

      snprintf(sa.sun_path + path_len, UNIX_PATH_MAX - path_len,
               "%" PRIx64 "-%lu.sok", ticks.QuadPart, GetCurrentProcessId());

      bind_res = impl::socket::bind(listener, reinterpret_cast<sockaddr *>(&sa),
                                    sa_len);
    } break;
#endif
    default:
      bind_res = stdx::make_unexpected(
          make_error_code(std::errc::address_family_not_supported));
      break;
  }

  if (!bind_res) return stdx::make_unexpected(bind_res.error());

  auto listen_res = impl::socket::listen(listener, 128);
  if (!listen_res) return stdx::make_unexpected(listen_res.error());

  auto first_res = impl::socket::socket(family, sock_type, protocol);
  if (!first_res) return stdx::make_unexpected(first_res.error());

  auto first_fd = first_res.value();

  Scope_guard first_fd_guard([first_fd]() { impl::socket::close(first_fd); });

  auto remote_sa_res = [](auto sock_handle)
      -> stdx::expected<sockaddr_storage, std::error_code> {
    struct sockaddr_storage ss {};
    size_t ss_len = sizeof(ss);

    const auto name_res = impl::socket::getsockname(
        sock_handle, reinterpret_cast<sockaddr *>(&ss), &ss_len);
    if (!name_res) return stdx::make_unexpected(name_res.error());

    // overwrite the address.
    if (ss.ss_family == AF_INET) {
      auto *sa = reinterpret_cast<sockaddr_in *>(&ss);

      sa->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }

    return ss;
  }(listener);

  if (!remote_sa_res) return stdx::make_unexpected(remote_sa_res.error());
  const auto remote_sa = *remote_sa_res;

  const auto connect_res = impl::socket::connect(
      first_fd, reinterpret_cast<const sockaddr *>(&remote_sa),
      sizeof(remote_sa));
  if (!connect_res) return stdx::make_unexpected(connect_res.error());

  const auto second_res = impl::socket::accept(listener, nullptr, nullptr);
  if (!second_res) return stdx::make_unexpected(second_res.error());

  first_fd_guard.commit();

  auto second_fd = second_res.value();

  return std::make_pair(first_fd, second_fd);
#endif
}

#ifdef __linux__
inline stdx::expected<size_t, error_type> splice(native_handle_type fd_in,
                                                 native_handle_type fd_out,
                                                 size_t len, int flags) {
  ssize_t written = ::splice(fd_in, nullptr, fd_out, nullptr, len, flags);

  if (written == -1) {
    return stdx::make_unexpected(last_error_code());
  }

  // post-condition to ensure that we can safely convert to size_t
  if (written < 0) {
    return stdx::make_unexpected(
        make_error_code(std::errc::result_out_of_range));
  }

  return written;
}
#endif

inline stdx::expected<size_t, error_type> splice_to_pipe(
    native_handle_type fd_in, impl::file::file_handle_type fd_out, size_t len,
    int flags) {
#ifdef __linux__
  return splice(fd_in, fd_out, len, flags);
#else
  (void)fd_in;
  (void)fd_out;
  (void)len;
  (void)flags;

  return stdx::make_unexpected(
      make_error_code(std::errc::operation_not_supported));
#endif
}

inline stdx::expected<size_t, error_type> splice_from_pipe(
    impl::file::file_handle_type fd_in, native_handle_type fd_out, size_t len,
    int flags) {
#ifdef __linux__
  return splice(fd_in, fd_out, len, flags);
#else
  (void)fd_in;
  (void)fd_out;
  (void)len;
  (void)flags;

  return stdx::make_unexpected(
      make_error_code(std::errc::operation_not_supported));
#endif
}

inline stdx::expected<void, error_type> wait(native_handle_type fd,
                                             wait_type wt) {
  short events{};

  switch (wt) {
    case wait_type::wait_read:
      events |= POLLIN;
      break;
    case wait_type::wait_write:
      events |= POLLOUT;
      break;
    case wait_type::wait_error:
      events |= POLLERR;
      break;
  }
  std::array<impl::poll::poll_fd, 1> fds{{
      {fd, events, 0},
  }};

  const auto res =
      impl::poll::poll(fds.data(), fds.size(), std::chrono::milliseconds{-1});

  if (!res) return stdx::make_unexpected(res.error());

  return {};
}

inline stdx::expected<void, error_type> shutdown(native_handle_type fd,
                                                 int how) {
  const auto res = ::shutdown(fd, how);
  if (kSocketError == res) {
    return stdx::make_unexpected(last_error_code());
  }

  return {};
}

inline stdx::expected<void, std::error_code> init() {
#ifdef _WIN32
  WORD wVersionRequested = MAKEWORD(2, 2);
  WSADATA wsaData;
  if (int err = WSAStartup(wVersionRequested, &wsaData)) {
    return stdx::make_unexpected(impl::socket::last_error_code());
  }
#endif
  return {};
}

}  // namespace socket
}  // namespace impl

}  // namespace net
#endif
