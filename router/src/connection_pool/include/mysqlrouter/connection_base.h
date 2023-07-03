/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef ROUTER_CONNECTION_POOL_CONNECTION_BASE_INCLUDED
#define ROUTER_CONNECTION_POOL_CONNECTION_BASE_INCLUDED

#include <cstdint>  // size_t
#include <functional>
#include <system_error>  // error_code
#include <vector>

#include "mysql/harness/default_init_allocator.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"

/**
 * virtual base-class of BasicConnection.
 */
class ConnectionBase {
 public:
  virtual ~ConnectionBase() = default;

  using recv_buffer_type =
      std::vector<uint8_t, default_init_allocator<uint8_t>>;

  virtual net::io_context &io_ctx() = 0;

  virtual void async_recv(
      recv_buffer_type &,
      std::function<void(std::error_code ec, size_t transferred)>) = 0;

  virtual void async_send(
      recv_buffer_type &,
      std::function<void(std::error_code ec, size_t transferred)>) = 0;

  virtual void async_wait_send(std::function<void(std::error_code ec)>) = 0;
  virtual void async_wait_recv(std::function<void(std::error_code ec)>) = 0;

  [[nodiscard]] virtual bool is_open() const = 0;

  [[nodiscard]] virtual net::impl::socket::native_handle_type native_handle()
      const = 0;

  [[nodiscard]] virtual stdx::expected<void, std::error_code> close() = 0;
  [[nodiscard]] virtual stdx::expected<void, std::error_code> shutdown(
      net::socket_base::shutdown_type st) = 0;

  [[nodiscard]] virtual std::string endpoint() const = 0;

  [[nodiscard]] virtual stdx::expected<void, std::error_code> cancel() = 0;

  [[nodiscard]] virtual bool is_secure_transport() const = 0;

  [[nodiscard]] virtual stdx::expected<void, std::error_code> set_io_context(
      net::io_context &new_ctx) = 0;
};

#endif
