/*
  Copyright (c) 2017, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_DESTINATION_ACCEPTOR_INCLUDED
#define MYSQL_HARNESS_DESTINATION_ACCEPTOR_INCLUDED

#include "harness_export.h"

#include <variant>

#include "mysql/harness/destination_endpoint.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/local.h"

namespace mysql_harness {

class HARNESS_EXPORT DestinationAcceptor {
 public:
  using TcpType = net::ip::tcp::acceptor;
  using LocalType = local::stream_protocol::acceptor;

  explicit DestinationAcceptor(net::io_context &io_ctx)
      : acceptor_(TcpType(io_ctx)) {}
  explicit DestinationAcceptor(TcpType sock) : acceptor_(std::move(sock)) {}
  explicit DestinationAcceptor(LocalType sock) : acceptor_(std::move(sock)) {}

  bool is_tcp() const { return std::holds_alternative<TcpType>(acceptor_); }
  bool is_local() const { return !is_tcp(); }

  TcpType &as_tcp() { return std::get<TcpType>(acceptor_); }
  const TcpType &as_tcp() const { return std::get<TcpType>(acceptor_); }

  LocalType &as_local() { return std::get<LocalType>(acceptor_); }
  const LocalType &as_local() const { return std::get<LocalType>(acceptor_); }

  stdx::expected<void, std::error_code> open(
      const mysql_harness::DestinationEndpoint &ep);

  stdx::expected<void, std::error_code> bind(
      const mysql_harness::DestinationEndpoint &ep) {
    if (ep.is_local()) {
      return as_local().bind(ep.as_local());
    }

    return as_tcp().bind(ep.as_tcp());
  }

  stdx::expected<void, std::error_code> listen(int backlog) {
    if (is_local()) {
      return as_local().listen(backlog);
    }

    return as_tcp().listen(backlog);
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

  template <typename SettableSocketOption>
  stdx::expected<void, std::error_code> set_option(
      const SettableSocketOption &option) {
    if (is_local()) {
      return as_local().set_option(option);
    }

    return as_tcp().set_option(option);
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
  auto async_wait(net::socket_base::wait_type wt, CompletionToken &&token) {
    if (is_local()) {
      return as_local().async_wait(wt, std::forward<CompletionToken>(token));
    }

    return as_tcp().async_wait(wt, std::forward<CompletionToken>(token));
  }

  template <class CompletionToken>
  auto async_accept(CompletionToken &&token) {
    if (is_local()) {
      return as_local().async_accept(std::forward<CompletionToken>(token));
    }

    return as_tcp().async_accept(std::forward<CompletionToken>(token));
  }

  template <class CompletionToken>
  auto async_accept(mysql_harness::DestinationEndpoint &ep,
                    CompletionToken &&token) {
    if (is_local()) {
      if (!ep.is_local()) {
        ep = mysql_harness::DestinationEndpoint(
            mysql_harness::DestinationEndpoint::LocalType());
      }
      return as_local().async_accept(ep.as_local(),
                                     std::forward<CompletionToken>(token));
    }

    if (!ep.is_tcp()) {
      ep = mysql_harness::DestinationEndpoint(
          mysql_harness::DestinationEndpoint::TcpType());
    }

    return as_tcp().async_accept(ep.as_tcp(),
                                 std::forward<CompletionToken>(token));
  }

  template <class CompletionToken>
  void async_accept(net::io_context &io_ctx, CompletionToken &&token) {
    if (is_local()) {
      return as_local().async_accept(io_ctx,
                                     std::forward<CompletionToken>(token));
    }

    return as_tcp().async_accept(io_ctx, std::forward<CompletionToken>(token));
  }

 private:
  std::variant<TcpType, LocalType> acceptor_;
};

}  // namespace mysql_harness

#endif
