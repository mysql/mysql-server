/*
  Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

/*
 * include climits first to ensure _POSIX_C_SOURCE will be set
 * correctly early.
 *
 * On solaris sys/feature_set.h must be included before errno.h
 * to ensure the right version of 'errno' is provided:
 *
 * - without _POSIX_C_SOURCE >= 199509L: 'extern int errno'
 * - with, '*(___errno())', the thread-local-storage version.
 *
 * including climits is safe on all platforms AND includes
 * sys/feature_set.h on solaris.
 */
#include <climits>
#include <stdexcept>
#include <system_error>

#ifndef _WIN32
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#else
#include <windows.h>
#include <winsock2.h>
typedef long ssize_t;
#endif

#include "mysql_protocol_decoder.h"
#include "mysql_protocol_encoder.h"
#include "mysql_protocol_utils.h"

std::error_code last_socket_error_code() {
  return {
#ifndef _WIN32
      errno, std::generic_category()
#else
      WSAGetLastError(), std::system_category()
#endif
  };
}

void send_packet(socket_t client_socket, const uint8_t *data, size_t size,
                 int flags) {
  ssize_t sent = 0;
  size_t buffer_offset = 0;
  while (buffer_offset < size) {
    if ((sent = send(client_socket,
                     reinterpret_cast<const char *>(data) + buffer_offset,
                     size - buffer_offset, flags)) < 0) {
      throw std::system_error(last_socket_error_code(), "send() failed");
    }
    buffer_offset += static_cast<size_t>(sent);
  }
}

void send_packet(socket_t client_socket,
                 const server_mock::MySQLProtocolEncoder::MsgBuffer &buffer,
                 int flags) {
  send_packet(client_socket, buffer.data(), buffer.size(), flags);
}

bool socket_has_data(socket_t sock, int timeout_ms) {
// check if the current socket is readable/open
//
// allow interrupting the read() by closing the socket in another thread
#ifdef _WIN32
  WSAPOLLFD
#else
  struct pollfd
#endif
  fds[1];
  memset(fds, 0, sizeof(fds));

  fds[0].fd = sock;
#ifdef _WIN32
  fds[0].events = POLLRDNORM;
#else
  fds[0].events = POLLIN | POLLHUP;
#endif

// check if someone closed our socket externally
#ifdef _WIN32
  int r = ::WSAPoll(fds, 1, timeout_ms);
#else
  int r = ::poll(fds, 1, timeout_ms);
#endif

  if (r > 0) return true;
  if (r < 0) throw std::system_error(last_socket_error_code(), "poll() failed");

  if (fds[0].revents & POLLNVAL) {
    // another thread may have closed the socket
    throw std::runtime_error("poll() reported: invalid socket");
  }

  return false;
}

void read_packet(socket_t client_socket, uint8_t *data, size_t size,
                 int flags) {
  ssize_t received = 0;
  size_t buffer_offset = 0;
  while (buffer_offset < size) {
    while (true) {
      if (socket_has_data(client_socket, 100)) break;
    }

    received =
        recv(client_socket, reinterpret_cast<char *>(data) + buffer_offset,
             size - buffer_offset, flags);
    if (received < 0) {
      throw std::system_error(last_socket_error_code(), "recv() failed");
    } else if (received == 0) {
      // connection closed by client
      throw std::runtime_error("recv() failed: Connection Closed");
    }
    buffer_offset += static_cast<size_t>(received);
  }
}

int close_socket(socket_t sock) {
#ifndef _WIN32
  return close(sock);
#else
  return closesocket(sock);
#endif
}
