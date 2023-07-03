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

#ifndef MYSQL_HARNESS_NET_TS_IMPL_POLL_H_
#define MYSQL_HARNESS_NET_TS_IMPL_POLL_H_

#include <array>
#include <chrono>
#include <system_error>

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#else
#include <poll.h>  // poll
#endif

#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/stdx/expected.h"

namespace net {
namespace impl {
namespace poll {

#ifdef _WIN32
using poll_fd = WSAPOLLFD;
#else
using poll_fd = pollfd;
#endif

inline stdx::expected<size_t, std::error_code> poll(
    poll_fd *fds, size_t num_fds, std::chrono::milliseconds timeout) {
#ifdef _WIN32
  constexpr const auto err_res{SOCKET_ERROR};
  auto res = ::WSAPoll(fds, num_fds, timeout.count());
#else
  constexpr const auto err_res{-1};
  auto res = ::poll(fds, num_fds, timeout.count());
#endif

  if (res == err_res) {
    return stdx::make_unexpected(impl::socket::last_error_code());
  }
  if (0 == res) {
    return stdx::make_unexpected(make_error_code(std::errc::timed_out));
  }

  return res;
}
}  // namespace poll
}  // namespace impl
}  // namespace net

#endif
