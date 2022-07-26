/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_NET_TS_IMPL_LINUX_EPOLL_H_
#define MYSQL_HARNESS_NET_TS_IMPL_LINUX_EPOLL_H_

#include "my_config.h"  // HAVE_EPOLL

#ifdef HAVE_EPOLL
#include <chrono>
#include <system_error>

#include <sys/epoll.h>

#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/stdx/expected.h"

namespace net {
namespace impl {

namespace epoll {

enum class Cmd {
  add = EPOLL_CTL_ADD,
  del = EPOLL_CTL_DEL,
  mod = EPOLL_CTL_MOD,
};

// restarted syscalls automatically after EINTR
template <class Func>
inline auto uninterruptable(Func &&f) {
  do {
    auto res = f();
    if (res || (res.error() != std::errc::interrupted)) return res;
  } while (true);
}

inline stdx::expected<int, std::error_code> create() {
  return uninterruptable([&]() -> stdx::expected<int, std::error_code> {
    int epfd = ::epoll_create1(EPOLL_CLOEXEC);

    if (-1 == epfd) {
      return stdx::make_unexpected(
          std::error_code{errno, std::generic_category()});
    }

    return epfd;
  });
}
inline stdx::expected<void, std::error_code> ctl(int epfd, Cmd cmd, int fd,
                                                 epoll_event *ev) {
  return uninterruptable([&]() -> stdx::expected<void, std::error_code> {
    if (-1 == ::epoll_ctl(epfd, static_cast<int>(cmd), fd, ev)) {
      return stdx::make_unexpected(
          std::error_code{errno, std::generic_category()});
    }

    return {};
  });
}
inline stdx::expected<size_t, std::error_code> wait(
    int epfd, epoll_event *fd_events, size_t num_fd_events,
    std::chrono::milliseconds timeout) {
  // all are processed. fetch the next batch
  int res = ::epoll_wait(epfd, fd_events, num_fd_events, timeout.count());

  if (res < 0) {
    return stdx::make_unexpected(impl::socket::last_error_code());
  } else if (res == 0) {
    // timed out
    return stdx::make_unexpected(make_error_code(std::errc::timed_out));
  }

  return res;
}

}  // namespace epoll

}  // namespace impl
}  // namespace net
#endif

#endif
