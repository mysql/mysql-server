/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "ngs_common/operations_factory.h"
#include "ngs/memory.h"

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#include <sys/types.h>
#include <signal.h>
#endif


namespace ngs {

namespace details {

class Socket: public Socket_interface {
public:
  Socket(MYSQL_SOCKET mysql_socket)
  : m_mysql_socket(mysql_socket) {
  }

  Socket(PSI_socket_key key, int domain, int type, int protocol)
  : m_mysql_socket(mysql_socket_socket(key, domain, type, protocol)) {
  }

  ~Socket() {
    close();
  }

  virtual int bind(const struct sockaddr *addr, socklen_t len) {
    return mysql_socket_bind(m_mysql_socket, addr, len);
  }

  virtual MYSQL_SOCKET accept(PSI_socket_key key, struct sockaddr *addr, socklen_t *addr_len) {
    return mysql_socket_accept(key, m_mysql_socket, addr, addr_len);
  }

  virtual int listen(int backlog) {
    return mysql_socket_listen(m_mysql_socket, backlog);
  }

  virtual my_socket get_socket_fd() {
    return mysql_socket_getfd(m_mysql_socket);
  }

  virtual MYSQL_SOCKET get_socket_mysql() {
    return m_mysql_socket;
  }

  virtual int set_socket_opt(int level, int optname, const SOCKBUF_T *optval, socklen_t optlen) {
    return mysql_socket_setsockopt(m_mysql_socket, level, optname, optval, optlen);
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

class File: public File_interface {
public:
  File(const char* name, int access, int permission)
  : m_file_descriptor(::open(name, access, permission)) {
  }

  ~File() {
    close();
  }

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
#endif // defined(HAVE_SYS_UN_H)
  }

private:
  int m_file_descriptor;
  static const int INVALID_FILE_DESCRIPTOR;
};

const int File::INVALID_FILE_DESCRIPTOR = -1;


#if defined(HAVE_SYS_UN_H)
#define HAVE_SYS_UN_OR_NOT(U,W) U
#else
#define HAVE_SYS_UN_OR_NOT(U,W) W
#endif // defined(HAVE_SYS_UN_H)

class System: public System_interface {
  virtual int unlink(const char* name) {
    return HAVE_SYS_UN_OR_NOT(::unlink(name), 0);
  }

  virtual int get_errno() {
    return errno;
  }

  virtual int get_ppid() {
    return HAVE_SYS_UN_OR_NOT(::getppid(), 0);
  }

  virtual int get_pid() {
    return HAVE_SYS_UN_OR_NOT(::getpid(), 0);
  }

  virtual int kill(int pid, int signal) {
    return HAVE_SYS_UN_OR_NOT(::kill(pid, signal), 0);
  }

  virtual int get_socket_errno() {
    return socket_errno;
  }

  virtual void get_socket_error_and_message(int& err, std::string& strerr) {
    err = socket_errno;
  #ifdef _WIN32
    char *s = NULL;
    if (0 == FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&s, 0, NULL))
    {
      char text[256];
      my_snprintf(text, sizeof(text), "Error %i", err);
      strerr = text;
    }
    else
    {
      strerr = s;
      LocalFree(s);
    }
  #else
    strerr = strerror(err);
  #endif
  }

  virtual void freeaddrinfo(addrinfo *ai) {
    return ::freeaddrinfo(ai);
  }

  virtual int getaddrinfo(const char *node,
                          const char *service,
                          const addrinfo *hints,
                          addrinfo **res) {
    return ::getaddrinfo(node, service, hints, res);
  }

  virtual void sleep(uint32 seconds) {
    ::sleep(seconds);
  }
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

ngs::shared_ptr<File_interface> Operations_factory::open_file(
    const char* name, int access, int permission) {
  return ngs::allocate_shared<details::File>(name, access, permission);
}

ngs::shared_ptr<System_interface> Operations_factory::create_system_interface() {
  return ngs::allocate_shared<details::System>();
}

} // namespace ngs
