/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
typedef ULONG nfds_t;
typedef long ssize_t;
#else
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <sys/types.h>
#endif

#include "harness_export.h"
#include "tcp_address.h"

namespace mysql_harness {

/** @class SocketOperationsBase
 * @brief Base class to allow multiple SocketOperations implementations
 *        (at least one "real" and one mock for testing purposes)
 */
class HARNESS_EXPORT SocketOperationsBase {
 public:
  virtual ~SocketOperationsBase() = default;

  // the following functions are thin wrappers around syscalls
  virtual ssize_t write(int fd, void *buffer, size_t nbyte) = 0;
  virtual ssize_t read(int fd, void *buffer, size_t nbyte) = 0;
  virtual void close(int fd) = 0;
  virtual void shutdown(int fd) = 0;
  virtual void freeaddrinfo(addrinfo *ai) = 0;
  virtual int getaddrinfo(const char *node, const char *service,
                          const addrinfo *hints, addrinfo **res) = 0;
  virtual int bind(int fd, const struct sockaddr *addr, socklen_t len) = 0;
  virtual int socket(int domain, int type, int protocol) = 0;
  virtual int setsockopt(int fd, int level, int optname, const void *optval,
                         socklen_t optlen) = 0;
  virtual int listen(int fd, int n) = 0;
  virtual int get_errno() = 0;
  virtual void set_errno(int) = 0;
  virtual int poll(struct pollfd *fds, nfds_t nfds,
                   std::chrono::milliseconds timeout) = 0;
  virtual const char *inetntop(int af, void *cp, char *buf, socklen_t len) = 0;
  virtual int getpeername(int fd, struct sockaddr *addr, socklen_t *len) = 0;

  /** @brief Wrapper around socket library write() with a looping logic
   *         making sure the whole buffer got written
   */
  virtual ssize_t write_all(int fd, void *buffer, size_t nbyte) {
    ssize_t written = 0;
    size_t buffer_offset = 0;
    while (buffer_offset < nbyte) {
      if ((written =
               this->write(fd, reinterpret_cast<char *>(buffer) + buffer_offset,
                           nbyte - buffer_offset)) < 0) {
        return -1;
      }
      buffer_offset += static_cast<size_t>(written);
    }
    return static_cast<ssize_t>(nbyte);
  }

  /**
   * wait for a non-blocking connect() to finish
   *
   * @param sock a connected socket
   * @param timeout time to wait for the connect to complete
   *
   * call connect_non_blocking_status() to get the final result
   */
  virtual int connect_non_blocking_wait(int sock,
                                        std::chrono::milliseconds timeout) = 0;

  /**
   * get the non-blocking connect() status
   *
   * must be called after connect()ed socket became writable.
   *
   * @see connect_non_blocking_wait() and poll()
   */
  virtual int connect_non_blocking_status(int sock, int &so_error) = 0;

  /** @brief Exception thrown by `get_local_hostname()` on error */
  class LocalHostnameResolutionError : public std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  /** @brief return hostname of local host */
  virtual std::string get_local_hostname() = 0;
};

/** @class SocketOperations
 * @brief This class provides a "real" (not mock) implementation
 */
class HARNESS_EXPORT SocketOperations : public SocketOperationsBase {
 public:
  static SocketOperations *instance();

  /** @brief Thin wrapper around socket library write() */
  ssize_t write(int fd, void *buffer, size_t nbyte) override;

  /** @brief Thin wrapper around socket library read() */
  ssize_t read(int fd, void *buffer, size_t nbyte) override;

  /** @brief Thin wrapper around socket library close() */
  void close(int fd) override;

  /** @brief Thin wrapper around socket library shutdown() */
  void shutdown(int fd) override;

  /** @brief Thin wrapper around socket library freeaddrinfo() */
  void freeaddrinfo(addrinfo *ai) override;

  /** @brief Thin wrapper around socket library getaddrinfo() */
  int getaddrinfo(const char *node, const char *service, const addrinfo *hints,
                  addrinfo **res) override;

  /** @brief Thin wrapper around socket library bind() */
  int bind(int fd, const struct sockaddr *addr, socklen_t len) override;

  /** @brief Thin wrapper around socket library socket() */
  int socket(int domain, int type, int protocol) override;

  /** @brief Thin wrapper around socket library setsockopt() */
  int setsockopt(int fd, int level, int optname, const void *optval,
                 socklen_t optlen) override;

  /** @brief Thin wrapper around socket library listen() */
  int listen(int fd, int n) override;

  /**
   * wrapper around poll()/WSAPoll()
   */
  int poll(struct pollfd *fds, nfds_t nfds,
           std::chrono::milliseconds timeout) override;

  /** @brief Wrapper around socket library inet_ntop()
             Can't call it inet_ntop as it is a macro on freebsd and causes
             compilation errors.
  */
  const char *inetntop(int af, void *cp, char *buf, socklen_t len) override;

  /** @brief Wrapper around socket library getpeername() */
  int getpeername(int fd, struct sockaddr *addr, socklen_t *len) override;

  /**
   * wait for a non-blocking connect() to finish
   *
   * call connect_non_blocking_status() to get the final result
   *
   * @param sock a connected socket
   * @param timeout time to wait for the connect to complete
   */
  int connect_non_blocking_wait(int sock,
                                std::chrono::milliseconds timeout) override;

  /**
   * get the non-blocking connect() status
   *
   * must be called after connect()ed socket became writable.
   *
   * @see connect_non_blocking_wait() and poll()
   */
  int connect_non_blocking_status(int sock, int &so_error) override;

  /** @brief return hostname of local host
   *
   * @throws `LocalHostnameResolutionError` (std::runtime_error) on failure
   */
  std::string get_local_hostname() override;

  /**
   * get the error-code of the last (socket) operation
   *
   * @see errno or WSAGetLastError()
   */
  int get_errno() override {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
  }

  /**
   * wrapper around errno/WSAGetLastError()
   */
  void set_errno(int e) override {
#ifdef _WIN32
    WSASetLastError(e);
#else
    errno = e;
#endif
  }

 private:
  SocketOperations(const SocketOperations &) = delete;
  SocketOperations operator=(const SocketOperations &) = delete;
  SocketOperations() = default;
};

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_SOCKETOPERATIONS_INCLUDED
