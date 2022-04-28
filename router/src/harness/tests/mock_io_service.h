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

#ifndef MYSQLROUTER_MOCK_IO_SERVICE_H
#define MYSQLROUTER_MOCK_IO_SERVICE_H

#include <gmock/gmock.h>

#include "mysql/harness/net_ts/io_context.h"

class MockIoService : public net::IoServiceBase {
 public:
  using void_ret = stdx::expected<void, std::error_code>;
  MOCK_METHOD0(open, void_ret());
  MOCK_METHOD2(add_fd_interest,
               void_ret(native_handle_type, net::impl::socket::wait_type));
  MOCK_METHOD1(remove_fd, void_ret(native_handle_type));
  MOCK_METHOD0(notify, void());

  using fdevent_ret = stdx::expected<net::fd_event, std::error_code>;
  MOCK_METHOD1(poll_one, fdevent_ret(std::chrono::milliseconds));
};

#endif
