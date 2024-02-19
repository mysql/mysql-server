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

#ifndef ROUTING_BASIC_PROTOCOL_SPLICER_INCLUDED
#define ROUTING_BASIC_PROTOCOL_SPLICER_INCLUDED

#include <cstdint>  // size_t
#include <cstdio>
#include <functional>  // function
#include <optional>
#include <sstream>
#include <string>

#include "blocked_endpoints.h"
#include "harness_assert.h"
#include "initial_connection_attributes.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/net_ts/internet.h"  // net::ip::tcp
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/local.h"  // local::stream_protocol
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/channel.h"
#include "mysqlrouter/connection_base.h"

class RoutingConnectionBase {
 public:
  virtual ~RoutingConnectionBase() = default;

  [[nodiscard]] virtual std::vector<std::pair<std::string, std::string>>
  initial_connection_attributes() const = 0;

  virtual uint64_t reset_error_count(BlockedEndpoints &blocked_endpoints) = 0;
  virtual uint64_t increment_error_count(
      BlockedEndpoints &blocked_endpoints) = 0;
};

template <class Protocol>
class RoutingConnection : public RoutingConnectionBase {
 public:
  using protocol_type = Protocol;
  using endpoint_type = typename protocol_type::endpoint;

  RoutingConnection(endpoint_type ep) : ep_{std::move(ep)} {}

  [[nodiscard]] std::vector<std::pair<std::string, std::string>>
  initial_connection_attributes() const override {
    return ::initial_connection_attributes(ep_);
  }

  uint64_t reset_error_count(BlockedEndpoints &blocked_endpoints) override {
    return blocked_endpoints.reset_error_count(ep_);
  }

  uint64_t increment_error_count(BlockedEndpoints &blocked_endpoints) override {
    return blocked_endpoints.increment_error_count(ep_);
  }

 private:
  endpoint_type ep_;
};

template <class T>
class TlsSwitchableClientConnection : public TlsSwitchableConnection<T> {
  using base_class_ = TlsSwitchableConnection<T>;

 public:
  using protocol_state_type = typename base_class_::protocol_state_type;

  TlsSwitchableClientConnection(
      std::unique_ptr<ConnectionBase> conn,
      std::unique_ptr<RoutingConnectionBase> routing_conn, SslMode ssl_mode,
      protocol_state_type state)
      : base_class_(std::move(conn), ssl_mode, std::move(state)),
        routing_conn_(std::move(routing_conn)) {}

  TlsSwitchableClientConnection(
      std::unique_ptr<ConnectionBase> conn,
      std::unique_ptr<RoutingConnectionBase> routing_conn, SslMode ssl_mode,
      Channel channel, protocol_state_type state)
      : base_class_(std::move(conn), ssl_mode, std::move(channel),
                    std::move(state)),
        routing_conn_(std::move(routing_conn)) {}

  [[nodiscard]] std::vector<std::pair<std::string, std::string>>
  initial_connection_attributes() const {
    return routing_conn_->initial_connection_attributes();
  }

  [[nodiscard]] uint64_t reset_error_count(
      BlockedEndpoints &blocked_endpoints) {
    return routing_conn_->reset_error_count(blocked_endpoints);
  }

  [[nodiscard]] uint64_t increment_error_count(
      BlockedEndpoints &blocked_endpoints) {
    return routing_conn_->increment_error_count(blocked_endpoints);
  }

 private:
  std::unique_ptr<RoutingConnectionBase> routing_conn_;
};

#endif
