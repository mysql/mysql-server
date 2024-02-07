/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_NET_TS_IMPL_SOCKET_CONSTANTS_H_
#define MYSQL_HARNESS_NET_TS_IMPL_SOCKET_CONSTANTS_H_

#include <bitset>
#include <system_error>

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#else
#include <poll.h>        // POLLIN, ...
#include <sys/socket.h>  // MSG_...
#endif

namespace net {
namespace impl {
namespace socket {

#ifdef _WIN32
constexpr const int kSocketError{SOCKET_ERROR};
using native_handle_type = SOCKET;
constexpr const native_handle_type kInvalidSocket = INVALID_SOCKET;
using socklen_t = int;
#else
constexpr const int kSocketError{-1};
using native_handle_type = int;
constexpr const native_handle_type kInvalidSocket = -1;
#endif

using error_type = std::error_code;
#ifdef _WIN32
using msghdr_base = ::WSAMSG;
using iovec_base = ::WSABUF;
#else
using msghdr_base = ::msghdr;
using iovec_base = ::iovec;
#endif

using message_flags = std::bitset<31>;

static constexpr message_flags message_peek = MSG_PEEK;
static constexpr message_flags message_out_of_band = MSG_OOB;
static constexpr message_flags message_do_not_route = MSG_DONTROUTE;
#ifdef MSG_FASTOPEN
// linux
static constexpr message_flags message_fast_open = MSG_FASTOPEN;
#endif
#ifdef MSG_ZEROCOPY
// linux
static constexpr message_flags message_zero_copy = MSG_ZEROCOPY;
#endif
#ifdef MSG_ERRQUEUE
// linux
static constexpr message_flags message_error_queue = MSG_ERRQUEUE;
#endif
#ifdef _WIN32
static constexpr message_flags message_partial = MSG_PARTIAL;
static constexpr message_flags message_waitall = MSG_WAITALL;
#endif

enum class wait_type {
  wait_read = POLLIN,
  wait_write = POLLOUT,
  wait_error = POLLERR | POLLHUP,
};

}  // namespace socket
}  // namespace impl
}  // namespace net

#endif
