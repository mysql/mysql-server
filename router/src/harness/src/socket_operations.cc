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

#include "socket_operations.h"
#include "common.h"

#include <memory>
#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#ifndef __APPLE__
#include <ifaddrs.h>
#include <net/if.h>
#endif
#else
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace mysql_harness {

SocketOperations *SocketOperations::instance() {
  static SocketOperations instance_;
  return &instance_;
}

int SocketOperations::poll(struct pollfd *fds, nfds_t nfds,
                           std::chrono::milliseconds timeout_ms) {
#ifdef _WIN32
  return ::WSAPoll(fds, nfds, timeout_ms.count());
#else
  return ::poll(fds, nfds, static_cast<int>(timeout_ms.count()));
#endif
}

int SocketOperations::connect_non_blocking_wait(
    socket_t sock, std::chrono::milliseconds timeout_ms) {
  struct pollfd fds[] = {
      {sock, POLLOUT, 0},
  };

  int res = poll(fds, sizeof(fds) / sizeof(fds[0]), timeout_ms);

  if (0 == res) {
    // timeout
    this->set_errno(ETIMEDOUT);
    return -1;
  } else if (res < 0) {
    // some error
    return -1;
  }

  bool connect_writable = (fds[0].revents & POLLOUT) != 0;

  if (!connect_writable) {
    // this should not happen
    this->set_errno(EINVAL);
    return -1;
  }

  return 0;
}

int SocketOperations::connect_non_blocking_status(int sock, int &so_error) {
  socklen_t error_len = static_cast<socklen_t>(sizeof(so_error));

  if (getsockopt(sock, SOL_SOCKET, SO_ERROR,
                 reinterpret_cast<char *>(&so_error), &error_len) == -1) {
    so_error = get_errno();
    return -1;
  }

  if (so_error) {
    return -1;
  }

  return 0;
}

ssize_t SocketOperations::write(int fd, void *buffer, size_t nbyte) {
#ifndef _WIN32
  return ::write(fd, buffer, nbyte);
#else
  return ::send(fd, reinterpret_cast<const char *>(buffer), nbyte, 0);
#endif
}

ssize_t SocketOperations::read(int fd, void *buffer, size_t nbyte) {
#ifndef _WIN32
  return ::read(fd, buffer, nbyte);
#else
  return ::recv(fd, reinterpret_cast<char *>(buffer), nbyte, 0);
#endif
}

void SocketOperations::close(int fd) {
#ifndef _WIN32
  ::close(fd);
#else
  ::closesocket(fd);
#endif
}

void SocketOperations::shutdown(int fd) {
#ifndef _WIN32
  ::shutdown(fd, SHUT_RDWR);
#else
  ::shutdown(fd, SD_BOTH);
#endif
}

void SocketOperations::freeaddrinfo(addrinfo *ai) { return ::freeaddrinfo(ai); }

int SocketOperations::getaddrinfo(const char *node, const char *service,
                                  const addrinfo *hints, addrinfo **res) {
  return ::getaddrinfo(node, service, hints, res);
}

int SocketOperations::bind(int fd, const struct sockaddr *addr, socklen_t len) {
  return ::bind(fd, addr, len);
}

int SocketOperations::socket(int domain, int type, int protocol) {
  return ::socket(domain, type, protocol);
}

int SocketOperations::setsockopt(int fd, int level, int optname,
                                 const void *optval, socklen_t optlen) {
#ifndef _WIN32
  return ::setsockopt(fd, level, optname, optval, optlen);
#else
  return ::setsockopt(fd, level, optname,
                      reinterpret_cast<const char *>(optval), optlen);
#endif
}

int SocketOperations::listen(int fd, int n) { return ::listen(fd, n); }

const char *SocketOperations::inetntop(int af, void *cp, char *buf,
                                       socklen_t len) {
  return ::inet_ntop(af, cp, buf, len);
}

int SocketOperations::getpeername(int fd, struct sockaddr *addr,
                                  socklen_t *len) {
  return ::getpeername(fd, addr, len);
}

std::string SocketOperations::get_local_hostname() {
  char buf[1024] = {0};
#if defined(_WIN32) || defined(__APPLE__) || defined(__FreeBSD__)
  int ret = gethostname(buf, sizeof(buf));
  if (ret < 0) {
    int err = get_errno();
    throw LocalHostnameResolutionError(
        "Could not get local hostname: " + mysql_harness::get_strerror(err) +
        " (ret: " + std::to_string(ret) + ", error: " + std::to_string(err) +
        ")");
  }
#else
  struct ifaddrs *ifa = nullptr, *ifap;
  int ret = -1, family;
  socklen_t addrlen;

  std::shared_ptr<ifaddrs> ifa_deleter(nullptr, [&](void *) {
    if (ifa) freeifaddrs(ifa);
  });
  if ((ret = getifaddrs(&ifa)) != 0 || !ifa) {
    throw LocalHostnameResolutionError(
        "Could not get local host address: " +
        mysql_harness::get_strerror(errno) + " (ret: " + std::to_string(ret) +
        ", errno: " + std::to_string(errno) + ")");
  }
  for (ifap = ifa; ifap != NULL; ifap = ifap->ifa_next) {
    if ((ifap->ifa_addr == NULL) || (ifap->ifa_flags & IFF_LOOPBACK) ||
        (!(ifap->ifa_flags & IFF_UP)))
      continue;
    family = ifap->ifa_addr->sa_family;
    if (family != AF_INET && family != AF_INET6) continue;
    if (family == AF_INET6) {
      struct sockaddr_in6 *sin6;

      sin6 = (struct sockaddr_in6 *)ifap->ifa_addr;
      if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) ||
          IN6_IS_ADDR_MC_LINKLOCAL(&sin6->sin6_addr))
        continue;
    }
    addrlen = static_cast<socklen_t>((family == AF_INET)
                                         ? sizeof(struct sockaddr_in)
                                         : sizeof(struct sockaddr_in6));
    ret =
        getnameinfo(ifap->ifa_addr, addrlen, buf,
                    static_cast<socklen_t>(sizeof(buf)), NULL, 0, NI_NAMEREQD);
  }
  if (ret != EAI_NONAME && ret != 0) {
    throw LocalHostnameResolutionError(
        "Could not get local hostname: " + std::string(gai_strerror(ret)) +
        " (ret: " + std::to_string(ret) + ", errno: " + std::to_string(errno) +
        ")");
  }
#endif
  return buf;
}

}  // namespace mysql_harness
