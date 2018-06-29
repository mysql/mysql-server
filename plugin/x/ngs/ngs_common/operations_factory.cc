/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/ngs/include/ngs_common/operations_factory.h"

#include "my_config.h"
#include "my_psi_config.h"

#include <errno.h>
#include <fcntl.h>
#ifndef _WIN32
#include <netdb.h>
#endif

#include "my_inttypes.h"
#include "my_io.h"
#include "plugin/x/ngs/include/ngs/memory.h"

#ifdef HAVE_SYS_UN_H
#include <signal.h>
#include <sys/types.h>
#include <sys/un.h>
#endif
#include "plugin/x/ngs/include/ngs_common/config.h"

namespace ngs {

namespace details {

class Socket : public Socket_interface {
 public:
  Socket(MYSQL_SOCKET mysql_socket) : m_mysql_socket(mysql_socket) {}

  Socket(PSI_socket_key key MY_ATTRIBUTE((unused)), int domain, int type,
         int protocol)
      : m_mysql_socket(mysql_socket_socket(key, domain, type, protocol)) {}

  ~Socket() { close(); }

  virtual int bind(const struct sockaddr *addr, socklen_t len) {
    return mysql_socket_bind(m_mysql_socket, addr, len);
  }

  virtual MYSQL_SOCKET accept(PSI_socket_key key MY_ATTRIBUTE((unused)),
                              struct sockaddr *addr, socklen_t *addr_len) {
    return mysql_socket_accept(key, m_mysql_socket, addr, addr_len);
  }

  virtual int listen(int backlog) {
    return mysql_socket_listen(m_mysql_socket, backlog);
  }

  virtual my_socket get_socket_fd() {
    return mysql_socket_getfd(m_mysql_socket);
  }

  virtual MYSQL_SOCKET get_socket_mysql() { return m_mysql_socket; }

  virtual int set_socket_opt(int level, int optname, const SOCKBUF_T *optval,
                             socklen_t optlen) {
    return mysql_socket_setsockopt(m_mysql_socket, level, optname, optval,
                                   optlen);
  }

  virtual void close() {
    if (INVALID_SOCKET != get_socket_fd()) {
      mysql_socket_close(m_mysql_socket);
      m_mysql_socket = MYSQL_INVALID_SOCKET;
    }
  }

  void set_socket_thread_owner() {
    mysql_socket_set_thread_owner(m_mysql_socket);
  }

 private:
  MYSQL_SOCKET m_mysql_socket;
};

class File : public File_interface {
 public:
  File(const char *name, int access, int permission)
      : m_file_descriptor(::open(name, access, permission)) {}

  ~File() { close(); }

  virtual int close() {
    if (INVALID_FILE_DESCRIPTOR != m_file_descriptor) {
      const int result = ::close(m_file_descriptor);

      m_file_descriptor = INVALID_FILE_DESCRIPTOR;

      return result;
    }

    return 0;
  }

  virtual int read(void *buffer, int nbyte) {
    return ::read(m_file_descriptor, buffer, nbyte);
  }

  virtual int write(void *buffer, int nbyte) {
    return ::write(m_file_descriptor, buffer, nbyte);
  }

  virtual bool is_valid() {
    return INVALID_FILE_DESCRIPTOR != m_file_descriptor;
  }

  virtual int fsync() {
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

class System : public System_interface {
  int unlink(const char *name) override {
    return HAVE_UNIX_SOCKET(::unlink(name), 0);
  }

  int get_errno() override { return errno; }

  int get_ppid() override { return HAVE_UNIX_SOCKET(::getppid(), 0); }

  int get_pid() override { return HAVE_UNIX_SOCKET(::getpid(), 0); }

  int kill(int pid, int signal) override {
    return HAVE_UNIX_SOCKET(::kill(pid, signal), 0);
  }

  int get_socket_errno() override { return socket_errno; }

  void set_socket_errno(const int err) override {
#if defined(_WIN32)
    // socket_errno resolved on windows to WASGetLastError which can't be set.
    WSASetLastError(err);
#else
    socket_errno = err;
#endif  // defined(_WIN32)
  }

  void get_socket_error_and_message(int &out_err,
                                    std::string &out_strerr) override {
    out_err = socket_errno;
#ifdef _WIN32
    char *s = nullptr;
    if (0 == FormatMessage(
                 FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, out_err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 reinterpret_cast<LPSTR>(&s), 0, nullptr)) {
      char text[256];
      snprintf(text, sizeof(text), "Error %i", out_err);
      out_strerr = text;
    } else {
      out_strerr = s;
      LocalFree(s);
    }
#else
    out_strerr = strerror(out_err);
#endif
  }

  void freeaddrinfo(addrinfo *ai) override { return ::freeaddrinfo(ai); }

  int getaddrinfo(const char *node, const char *service, const addrinfo *hints,
                  addrinfo **res) override {
    return ::getaddrinfo(node, service, hints, res);
  }

  void sleep(uint32 seconds) override { ::sleep(seconds); }
};

}  // namespace details

ngs::shared_ptr<Socket_interface> Operations_factory::create_socket(
    PSI_socket_key key, int domain, int type, int protocol) {
  return ngs::allocate_shared<details::Socket>(key, domain, type, protocol);
}

ngs::shared_ptr<Socket_interface> Operations_factory::create_socket(
    MYSQL_SOCKET mysql_socket) {
  return ngs::allocate_shared<details::Socket>(mysql_socket);
}

ngs::shared_ptr<File_interface> Operations_factory::open_file(const char *name,
                                                              int access,
                                                              int permission) {
  return ngs::allocate_shared<details::File>(name, access, permission);
}

ngs::shared_ptr<System_interface>
Operations_factory::create_system_interface() {
  return ngs::allocate_shared<details::System>();
}

}  // namespace ngs
