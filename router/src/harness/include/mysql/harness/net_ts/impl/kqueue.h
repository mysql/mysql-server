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

#ifndef MYSQL_HARNESS_NET_TS_IMPL_KQUEUE_H_
#define MYSQL_HARNESS_NET_TS_IMPL_KQUEUE_H_

#include "my_config.h"  // HAVE_KQUEUE

#ifdef HAVE_KQUEUE
#include <chrono>
#include <iostream>
#include <system_error>

#include <sys/event.h>  // kqueue

#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/stdx/expected.h"

namespace net {
namespace impl {
namespace kqueue {

inline stdx::expected<int, std::error_code> create() {
  int res = ::kqueue();

  if (res == -1) {
    return stdx::unexpected(impl::socket::last_error_code());
  }

  return {res};
}

inline stdx::expected<int, std::error_code> kevent(
    int kedf, const struct kevent *changelist, int nchanges,
    struct kevent *eventlist, int nevents, const struct timespec *timeout) {
  int res = ::kevent(kedf, changelist, nchanges, eventlist, nevents, timeout);

  if (res == -1) {
    return stdx::unexpected(impl::socket::last_error_code());
  }

  return {res};
}

inline stdx::expected<int, std::error_code> ctl(int kedf,
                                                const struct kevent *changelist,
                                                int nchanges) {
  return kqueue::kevent(kedf, changelist, nchanges, nullptr, 0, nullptr);
}

inline stdx::expected<int, std::error_code> wait(
    int kedf, struct kevent *eventlist, int nevents,
    const struct timespec *timeout) {
  return kqueue::kevent(kedf, nullptr, 0, eventlist, nevents, timeout);
}

}  // namespace kqueue
}  // namespace impl
}  // namespace net
#endif

#endif
