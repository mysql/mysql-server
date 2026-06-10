/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_DESTINATION_SOCKET_INCLUDED
#define MYSQL_HARNESS_DESTINATION_SOCKET_INCLUDED

#include "harness_export.h"

#include <variant>

#include "mysql/harness/destination_endpoint.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/local.h"

namespace mysql_harness {

class HARNESS_EXPORT DestinationSocket {
 public:
  using TcpType = net::ip::tcp::socket;
  using LocalType = local::stream_protocol::socket;

  DestinationSocket(TcpType sock) : sock_(std::move(sock)) {}
  DestinationSocket(LocalType sock) : sock_(std::move(sock)) {}

  bool is_tcp() const { return std::holds_alternative<TcpType>(sock_); }
  bool is_local() const { return !is_tcp(); }

  TcpType &as_tcp() { return std::get<TcpType>(sock_); }
  const TcpType &as_tcp() const { return std::get<TcpType>(sock_); }

  LocalType &as_local() { return std::get<LocalType>(sock_); }
  const LocalType &as_local() const { return std::get<LocalType>(sock_); }

  stdx::expected<void, std::error_code> open(
      const mysql_harness::DestinationEndpoint &ep, int flags = 0);

  bool is_open() const {
    if (is_local()) {
      return as_local().is_open();
    }

    return as_tcp().is_open();
  }

  stdx::expected<void, std::error_code> native_non_blocking(bool val) {
    if (is_local()) {
      return as_local().native_non_blocking(val);
    }

    return as_tcp().native_non_blocking(val);
  }

  net::impl::socket::native_handle_type native_handle() const {
    if (is_local()) {
      return as_local().native_handle();
    }

    return as_tcp().native_handle();
  }

  stdx::expected<void, std::error_code> connect(
      const mysql_harness::DestinationEndpoint &ep);

  template <typename SettableSocketOption>
  stdx::expected<void, std::error_code> set_option(
      const SettableSocketOption &option) {
    if (is_local()) {
      return as_local().set_option(option);
    }

    return as_tcp().set_option(option);
  }

  template <typename GettableSocketOption>
  stdx::expected<void, std::error_code> get_option(
      GettableSocketOption &option) const {
    if (is_local()) {
      return as_local().get_option(option);
    }

    return as_tcp().get_option(option);
  }

  net::io_context::executor_type get_executor() {
    if (is_local()) {
      return as_local().get_executor();
    }

    return as_tcp().get_executor();
  }

  net::io_context &io_context() { return get_executor().context(); }

  stdx::expected<void, std::error_code> cancel() {
    if (is_local()) {
      return as_local().cancel();
    }

    return as_tcp().cancel();
  }

  stdx::expected<void, std::error_code> close() {
    if (is_local()) {
      return as_local().close();
    }

    return as_tcp().close();
  }

  template <class CompletionToken>
  void async_wait(net::socket_base::wait_type wt, CompletionToken &&token) {
    if (is_local()) {
      return as_local().async_wait(wt, std::forward<CompletionToken>(token));
    }

    return as_tcp().async_wait(wt, std::forward<CompletionToken>(token));
  }

  template <class DynamicBuffer, class CompletionToken>
  void async_send(DynamicBuffer &&dyn_buf, CompletionToken &&token)
    requires(net::is_dynamic_buffer_v<DynamicBuffer>)
  {
    if (is_local()) {
      return net::async_write(as_local(), std::forward<DynamicBuffer>(dyn_buf),
                              std::forward<CompletionToken>(token));
    }

    return net::async_write(as_tcp(), std::forward<DynamicBuffer>(dyn_buf),
                            std::forward<CompletionToken>(token));
  }

  template <class DynamicBuffer, class CompletionToken>
  void async_recv(DynamicBuffer &&dyn_buf, CompletionToken &&token)
    requires(net::is_dynamic_buffer_v<DynamicBuffer>)
  {
    if (is_local()) {
      return net::async_read(as_local(), std::forward<DynamicBuffer>(dyn_buf),
                             std::forward<CompletionToken>(token));
    }

    return net::async_read(as_tcp(), std::forward<DynamicBuffer>(dyn_buf),
                           std::forward<CompletionToken>(token));
  }

 private:
  std::variant<TcpType, LocalType> sock_;
};

}  // namespace mysql_harness

#endif
