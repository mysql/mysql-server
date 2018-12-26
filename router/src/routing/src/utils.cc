/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "utils.h"
#include "mysqlrouter/utils.h"

#include <assert.h>
#include <stdlib.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

#ifndef _MSC_VER
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

using mysqlrouter::get_socket_errno;

void *get_in_addr(struct sockaddr *addr) {
  if (addr->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)addr)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)addr)->sin6_addr);
}

std::pair<std::string, int> get_peer_name(
    struct sockaddr_storage *addr,
    mysql_harness::SocketOperationsBase *sock_op) {
  char result_addr[105] = {0};  // For IPv4, IPv6 and Unix socket

  const char *res{nullptr};
  int port{0};

  if (addr->ss_family == AF_INET6) {
    // IPv6
    auto *sin6 = (struct sockaddr_in6 *)addr;
    port = ntohs(sin6->sin6_port);
    res = sock_op->inetntop(AF_INET6, &sin6->sin6_addr, result_addr,
                            static_cast<socklen_t>(sizeof result_addr));
  } else if (addr->ss_family == AF_INET) {
    // IPv4
    auto *sin4 = (struct sockaddr_in *)addr;
    port = ntohs(sin4->sin_port);
    res = sock_op->inetntop(AF_INET, &sin4->sin_addr, result_addr,
                            static_cast<socklen_t>(sizeof result_addr));
  } else if (addr->ss_family == AF_UNIX) {
    // Unix socket, no good way to find peer
    return std::make_pair(std::string("unix socket"), 0);
  } else {
    throw std::runtime_error("unknown address family: " +
                             std::to_string(addr->ss_family));
  }

  if (res == nullptr) {
    throw std::runtime_error("inet_ntop() failed, errno: " +
                             std::to_string(get_socket_errno()));
  }

  return std::make_pair(std::string(result_addr), port);
}

std::pair<std::string, int> get_peer_name(
    int sock, mysql_harness::SocketOperationsBase *sock_op) {
  socklen_t sock_len;
  struct sockaddr_storage addr;

  sock_len = static_cast<socklen_t>(sizeof addr);
  if (0 != sock_op->getpeername(sock, (struct sockaddr *)&addr, &sock_len)) {
    throw std::runtime_error("getpeername() failed, errno: " +
                             std::to_string(get_socket_errno()));
  }

  return get_peer_name(&addr, sock_op);
}

std::vector<std::string> split_string(const std::string &data,
                                      const char delimiter, bool allow_empty) {
  std::stringstream ss(data);
  std::string token;
  std::vector<std::string> result;

  if (data.empty()) {
    return {};
  }

  while (std::getline(ss, token, delimiter)) {
    if (token.empty() && !allow_empty) {
      // Skip empty
      continue;
    }
    result.push_back(token);
  }

  // When last character is delimiter, it denotes an empty token
  if (allow_empty && data.back() == delimiter) {
    result.push_back("");
  }

  return result;
}

std::vector<std::string> split_string(const std::string &data,
                                      const char delimiter) {
  return split_string(data, delimiter, true);
}

ClientIpArray in_addr_to_array(const sockaddr_storage &addr) {
  ClientIpArray result{{0}};

  switch (addr.ss_family) {
    case AF_INET6: {
      const sockaddr_in6 *addr_intet6 =
          reinterpret_cast<const sockaddr_in6 *>(&addr);
      std::memcpy(result.data(), &addr_intet6->sin6_addr,
                  sizeof(addr_intet6->sin6_addr));
      break;
    }
    default: {
      const sockaddr_in *addr_intet =
          reinterpret_cast<const sockaddr_in *>(&addr);
      std::memcpy(result.data(), &addr_intet->sin_addr,
                  sizeof(addr_intet->sin_addr));
    }
  }

  return result;
}

std::string get_message_error(int errcode) {
#ifndef _WIN32
  return std::string(strerror(errcode));
#else
  if (errcode == SOCKET_ERROR || errcode == 0) {
    errcode = WSAGetLastError();
  }
  LPTSTR lpMsgBuf;

  if (0 != FormatMessage(
               FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
               NULL, errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
               (LPTSTR)&lpMsgBuf, 0, NULL)) {
    std::string msgerr = "SystemError: ";
    msgerr += lpMsgBuf;
    LocalFree(lpMsgBuf);
    return msgerr;
  } else {
    return "SystemError: " + std::to_string(errcode);
  }
#endif
}
