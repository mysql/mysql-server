/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_MOCK_IO_SERVICE_H
#define MYSQLROUTER_MOCK_IO_SERVICE_H

#include <gmock/gmock.h>

#include "mysql/harness/net_ts/io_context.h"

class MockIoService : public net::IoServiceBase {
 public:
  MOCK_METHOD((stdx::expected<void, std::error_code>), open, (), (override));
  MOCK_METHOD((stdx::expected<void, std::error_code>), add_fd_interest,
              (native_handle_type, net::impl::socket::wait_type), (override));
  MOCK_METHOD((stdx::expected<void, std::error_code>), remove_fd,
              (native_handle_type), (override));
  MOCK_METHOD(void, notify, (), (override));

  MOCK_METHOD((stdx::expected<net::fd_event, std::error_code>), poll_one,
              (std::chrono::milliseconds), (override));
};

#endif
