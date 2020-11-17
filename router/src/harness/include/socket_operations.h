/*
  Copyright (c) 2018, 2020, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_SOCKETOPERATIONS_INCLUDED
#define MYSQL_HARNESS_SOCKETOPERATIONS_INCLUDED

#include <chrono>
#include <memory>  // unique_ptr
#include <stdexcept>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>       // addrinfo
#include <sys/socket.h>  // sockaddr
#endif

#include "harness_export.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/stdx/expected.h"

namespace mysql_harness {

using socket_t = net::impl::socket::native_handle_type;
constexpr socket_t kInvalidSocket = net::impl::socket::kInvalidSocket;

/** @class SocketOperationsBase
 * @brief Base class to allow multiple SocketOperations implementations
 *        (at least one "real" and one mock for testing purposes)
 */
class HARNESS_EXPORT SocketOperationsBase {
 public:
  template <class T>
  using result = stdx::expected<T, std::error_code>;

  using addrinfo_result =
      result<std::unique_ptr<struct ::addrinfo, void (*)(struct ::addrinfo *)>>;

  explicit SocketOperationsBase() = default;
  explicit SocketOperationsBase(const SocketOperationsBase &) = default;
  SocketOperationsBase &operator=(const SocketOperationsBase &) = default;
  virtual ~SocketOperationsBase() = default;

  // the following functions are thin wrappers around syscalls
  virtual result<size_t> write(socket_t fd, const void *buffer,
                               size_t nbyte) = 0;
  virtual result<size_t> read(socket_t fd, void *buffer, size_t nbyte) = 0;
  virtual result<void> close(socket_t fd) = 0;
  virtual result<void> shutdown(socket_t fd) = 0;
  virtual addrinfo_result getaddrinfo(const char *node, const char *service,
                                      const addrinfo *hints) = 0;
  virtual result<void> connect(socket_t fd, const struct sockaddr *addr,
                               size_t len) = 0;
  virtual result<void> bind(socket_t fd, const struct sockaddr *addr,
                            size_t len) = 0;
  virtual result<socket_t> socket(int domain, int type, int protocol) = 0;
  virtual result<void> setsockopt(socket_t fd, int level, int optname,
                                  const void *optval, size_t optlen) = 0;
  virtual result<void> listen(socket_t fd, int n) = 0;
  virtual result<size_t> poll(struct pollfd *fds, size_t nfds,
                              std::chrono::milliseconds timeout) = 0;
  virtual result<const char *> inetntop(int af, const void *cp, char *out,
                                        size_t out_len) = 0;
  virtual result<void> getpeername(socket_t fd, struct sockaddr *addr,
                                   size_t *len) = 0;

  /** @brief Wrapper around socket library write() with a looping logic
   *         making sure the whole buffer got written
   */
  virtual result<size_t> write_all(socket_t fd, const void *buffer,
                                   size_t nbyte) {
    size_t buffer_offset = 0;
    while (buffer_offset < nbyte) {
      const auto write_res = this->write(
          fd, reinterpret_cast<const char *>(buffer) + buffer_offset,
          nbyte - buffer_offset);

      if (!write_res) {
        return write_res;
      }
      buffer_offset += write_res.value();
    }
    return buffer_offset;
  }

  /**
   * wait for a non-blocking connect() to finish
   *
   * @param sock a connected socket
   * @param timeout time to wait for the connect to complete
   *
   * call connect_non_blocking_status() to get the final result
   */
  virtual result<void> connect_non_blocking_wait(
      socket_t sock, std::chrono::milliseconds timeout) = 0;

  /**
   * Sets blocking flag for given socket
   *
   * @param sock a socket file descriptor
   * @param blocking whether to set blocking off (false) or on (true)
   */
  virtual result<void> set_socket_blocking(socket_t sock, bool blocking) = 0;

  /**
   * get the non-blocking connect() status
   *
   * must be called after connect()ed socket became writable.
   *
   * @see connect_non_blocking_wait() and poll()
   */
  virtual result<void> connect_non_blocking_status(socket_t sock) = 0;

  /** @brief Exception thrown by `get_local_hostname()` on error */
  class LocalHostnameResolutionError : public std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  /** @brief return hostname of local host */
  virtual std::string get_local_hostname() = 0;

  /** @brief return true if there is data to read from the socket passed as a
   * parameter */
  virtual result<bool> has_data(socket_t sock,
                                std::chrono::milliseconds timeout) = 0;
};

/** @class SocketOperations
 * @brief This class provides a "real" (not mock) implementation
 */
class HARNESS_EXPORT SocketOperations : public SocketOperationsBase {
 public:
  static SocketOperations *instance();

  SocketOperations(const SocketOperations &) = delete;
  SocketOperations operator=(const SocketOperations &) = delete;

  /** @brief Thin wrapper around socket library write() */
  result<size_t> write(socket_t fd, const void *buffer, size_t nbyte) override;

  /** @brief Thin wrapper around socket library read() */
  result<size_t> read(socket_t fd, void *buffer, size_t nbyte) override;

  /** @brief Thin wrapper around socket library close() */
  result<void> close(socket_t fd) override;

  /** @brief Thin wrapper around socket library shutdown() */
  result<void> shutdown(socket_t fd) override;

  /** @brief Thin wrapper around socket library getaddrinfo() */
  result<std::unique_ptr<addrinfo, void (*)(addrinfo *)>> getaddrinfo(
      const char *node, const char *service, const addrinfo *hints) override;

  /** @brief Thin wrapper around socket library connect() */
  result<void> connect(socket_t fd, const struct sockaddr *addr,
                       size_t len) override;

  /** @brief Thin wrapper around socket library bind() */
  result<void> bind(socket_t fd, const struct sockaddr *addr,
                    size_t len) override;

  /** @brief Thin wrapper around socket library socket() */
  result<socket_t> socket(int domain, int type, int protocol) override;

  /** @brief Thin wrapper around socket library setsockopt() */
  result<void> setsockopt(socket_t fd, int level, int optname,
                          const void *optval, size_t optlen) override;

  /** @brief Thin wrapper around socket library listen() */
  result<void> listen(socket_t fd, int n) override;

  /**
   * wrapper around poll()/WSAPoll()
   */
  result<size_t> poll(struct pollfd *fds, size_t nfds,
                      std::chrono::milliseconds timeout) override;

  /** @brief Wrapper around socket library inet_ntop()
             Can't call it inet_ntop as it is a macro on freebsd and causes
             compilation errors.
  */
  result<const char *> inetntop(int af, const void *cp, char *out,
                                size_t out_len) override;

  /** @brief Wrapper around socket library getpeername() */
  result<void> getpeername(socket_t fd, struct sockaddr *addr,
                           size_t *len) override;

  /**
   * wait for a non-blocking connect() to finish
   *
   * call connect_non_blocking_status() to get the final result
   *
   * @param sock a connected socket
   * @param timeout time to wait for the connect to complete
   */
  result<void> connect_non_blocking_wait(
      socket_t sock, std::chrono::milliseconds timeout) override;

  /**
   * get the non-blocking connect() status
   *
   * must be called after connect()ed socket became writable.
   *
   * @see connect_non_blocking_wait() and poll()
   */
  result<void> connect_non_blocking_status(socket_t sock) override;

  /**
   * Sets blocking flag for given socket
   *
   * @param sock a socket file descriptor
   * @param blocking whether to set blocking off (false) or on (true)
   */
  result<void> set_socket_blocking(socket_t sock, bool blocking) override;

  /** @brief return hostname of local host
   *
   * @throws `LocalHostnameResolutionError` (std::runtime_error) on failure
   */
  std::string get_local_hostname() override;

  /** @brief return true if there is data to read from the socket passed as a
   * parameter */
  result<bool> has_data(socket_t sock,
                        std::chrono::milliseconds timeout) override;

 private:
  SocketOperations() = default;
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_SOCKETOPERATIONS_INCLUDED
