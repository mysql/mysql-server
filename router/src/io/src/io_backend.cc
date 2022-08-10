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

#include "mysqlrouter/io_backend.h"

#include <memory>
#include <string>

#include "mysql/harness/net_ts/impl/linux_epoll_io_service.h"
#include "mysql/harness/net_ts/impl/poll_io_service.h"

#ifdef HAVE_EPOLL
static constexpr const char kLinuxEpoll[]{"linux_epoll"};
#endif
static constexpr const char kPoll[]{"poll"};

std::string IoBackend::preferred() {
  return
#if defined(HAVE_EPOLL)
      kLinuxEpoll
#else
      kPoll
#endif
      ;
}

std::set<std::string> IoBackend::supported() {
  return {
      kPoll,
#ifdef HAVE_EPOLL
      kLinuxEpoll,
#endif
  };
}

std::unique_ptr<net::IoServiceBase> IoBackend::backend(
    const std::string &name) {
  if (name == kPoll) {
    return std::make_unique<net::poll_io_service>();
#ifdef HAVE_EPOLL
  } else if (name == kLinuxEpoll) {
    return std::make_unique<net::linux_epoll_io_service>();
#endif
  } else {
    return {};
  }
}
