/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/operations_factory.h"

#include <errno.h>
#include <fcntl.h>
#ifndef _WIN32
#include <netdb.h>
#endif

#include <cinttypes>
#include <cstdint>
#include <string>

#include "my_io.h"       // NOLINT(build/include_subdir)
#include "my_systime.h"  // NOLINT(build/include_subdir)

#ifdef HAVE_SYS_UN_H
#include <signal.h>
#include <sys/types.h>
#include <sys/un.h>
#endif

#include "plugin/x/src/config/config.h"

namespace xpl {

namespace details {

class Socket : public iface::Socket {
 public:
  explicit Socket(MYSQL_SOCKET mysql_socket) : m_mysql_socket(mysql_socket) {}

  Socket(PSI_socket_key key [[maybe_unused]], int domain, int type,
         int protocol)
      : m_mysql_socket(mysql_socket_socket(key, domain, type, protocol)) {}

  ~Socket() override { close(); }

  int bind(const struct sockaddr *addr, socklen_t len) override {
    return mysql_socket_bind(m_mysql_socket, addr, len);
  }

  MYSQL_SOCKET accept(PSI_socket_key key [[maybe_unused]],
                      struct sockaddr *addr, socklen_t *addr_len) override {
    return mysql_socket_accept(key, m_mysql_socket, addr, addr_len);
  }

  int listen(int backlog) override {
    return mysql_socket_listen(m_mysql_socket, backlog);
  }

  my_socket get_socket_fd() override {
    return mysql_socket_getfd(m_mysql_socket);
  }

  MYSQL_SOCKET get_socket_mysql() override { return m_mysql_socket; }

  int set_socket_opt(int level, int optname, const SOCKBUF_T *optval,
                     socklen_t optlen) override {
    return mysql_socket_setsockopt(m_mysql_socket, level, optname, optval,
                                   optlen);
  }

  void close() override {
    if (INVALID_SOCKET != get_socket_fd()) {
      mysql_socket_close(m_mysql_socket);
      m_mysql_socket = MYSQL_INVALID_SOCKET;
    }
  }

  void set_socket_thread_owner() override {
    mysql_socket_set_thread_owner(m_mysql_socket);
  }

 private:
  MYSQL_SOCKET m_mysql_socket;
};

class File : public iface::File {
 public:
  File(const char *name, int access, int permission)
      : m_file_descriptor(::open(name, access, permission)) {}

  ~File() override { close(); }

  int close() override {
    if (INVALID_FILE_DESCRIPTOR != m_file_descriptor) {
      const int result = ::close(m_file_descriptor);

      m_file_descriptor = INVALID_FILE_DESCRIPTOR;

      return result;
    }

    return 0;
  }

  int read(void *buffer, int nbyte) override {
    return ::read(m_file_descriptor, buffer, nbyte);
  }

  int write(void *buffer, int nbyte) override {
    return ::write(m_file_descriptor, buffer, nbyte);
  }

  bool is_valid() override {
    return INVALID_FILE_DESCRIPTOR != m_file_descriptor;
  }

  int fsync() override {
#if defined(HAVE_SYS_UN_H)
    return ::fsync(m_file_descriptor);
#else
    return 0;
#endif  // defined(HAVE_SYS_UN_H)
  }

 private:
  int m_file_descriptor;
  static const int INVALID_FILE_DESCRIPTOR;
};

const int File::INVALID_FILE_DESCRIPTOR = -1;

class System : public iface::System {
  int32_t unlink(const char *name) override {
    return HAVE_UNIX_SOCKET(::unlink(name), 0);
  }

  int32_t get_errno() override { return errno; }

  int32_t get_ppid() override { return HAVE_UNIX_SOCKET(::getppid(), 0); }

  int32_t get_pid() override { return HAVE_UNIX_SOCKET(::getpid(), 0); }

  int32_t kill(int32_t pid, int32_t signal) override {
    return HAVE_UNIX_SOCKET(::kill(pid, signal), 0);
  }

  int32_t get_socket_errno() override { return socket_errno; }

  void set_socket_errno(const int32_t err) override {
#if defined(_WIN32)
    // socket_errno resolved on windows to WASGetLastError which can't be set.
    WSASetLastError(err);
#else
    socket_errno = err;
#endif  // defined(_WIN32)
  }

  void get_socket_error_and_message(int32_t *out_err,
                                    std::string *out_strerr) override {
    *out_err = socket_errno;
#ifdef _WIN32
    char *s = nullptr;
    if (0 == FormatMessage(
                 FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, *out_err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 reinterpret_cast<LPSTR>(&s), 0, nullptr)) {
      char text[256];
      snprintf(text, sizeof(text), "Error %" PRIi32, *out_err);
      *out_strerr = text;
    } else {
      *out_strerr = s;
      LocalFree(s);
    }
#else
    *out_strerr = strerror(*out_err);
#endif
  }

  void freeaddrinfo(addrinfo *ai) override { return ::freeaddrinfo(ai); }

  int32_t getaddrinfo(const char *node, const char *service,
                      const addrinfo *hints, addrinfo **res) override {
    return ::getaddrinfo(node, service, hints, res);
  }

  void sleep(uint32_t seconds) override { ::sleep(seconds); }
};

}  // namespace details

std::shared_ptr<iface::Socket> Operations_factory::create_socket(
    PSI_socket_key key, int domain, int type, int protocol) {
  return std::make_shared<details::Socket>(key, domain, type, protocol);
}

std::shared_ptr<iface::Socket> Operations_factory::create_socket(
    MYSQL_SOCKET mysql_socket) {
  return std::make_shared<details::Socket>(mysql_socket);
}

std::shared_ptr<iface::File> Operations_factory::open_file(const char *name,
                                                           int access,
                                                           int permission) {
  return std::make_shared<details::File>(name, access, permission);
}

std::shared_ptr<iface::System> Operations_factory::create_system_interface() {
  return std::make_shared<details::System>();
}

}  // namespace xpl
