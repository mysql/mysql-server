/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "forwarding_processor.h"

#include <memory>  // make_unique

#include "classic_connect.h"
#include "classic_connection_base.h"
#include "classic_forwarder.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "mysqlrouter/connection_pool_component.h"

stdx::expected<Processor::Result, std::error_code>
ForwardingProcessor::forward_server_to_client(bool noflush) {
  connection()->push_processor(
      std::make_unique<ServerToClientForwarder>(connection(), noflush));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ForwardingProcessor::forward_client_to_server(bool noflush) {
  connection()->push_processor(
      std::make_unique<ClientToServerForwarder>(connection(), noflush));

  return Result::Again;
}

static PooledClassicConnection make_pooled_connection(
    TlsSwitchableConnection &&other) {
  auto *classic_protocol_state =
      dynamic_cast<ClassicProtocolState *>(other.protocol());
  return {std::move(other.connection()),
          other.channel()->release_ssl(),
          classic_protocol_state->server_capabilities(),
          classic_protocol_state->client_capabilities(),
          classic_protocol_state->server_greeting(),
          other.ssl_mode(),
          classic_protocol_state->username(),
          classic_protocol_state->schema(),
          classic_protocol_state->sent_attributes()};
}

static TlsSwitchableConnection make_connection_from_pooled(
    PooledClassicConnection &&other) {
  return {std::move(other.connection()),
          nullptr,  // routing_conn
          other.ssl_mode(), std::make_unique<Channel>(std::move(other.ssl())),
          std::make_unique<ClassicProtocolState>(
              other.server_capabilities(), other.client_capabilities(),
              other.server_greeting(), other.username(), other.schema(),
              other.attributes())};
}

stdx::expected<bool, std::error_code>
ForwardingProcessor::pool_server_connection() {
  auto *socket_splicer = connection()->socket_splicer();
  auto &server_conn = socket_splicer->server_conn();

  if (server_conn.is_open()) {
    // move the connection to the pool.
    //
    // the pool will either close it or keep it alive.
    auto &pools = ConnectionPoolComponent::get_instance();

    if (auto pool = pools.get(ConnectionPoolComponent::default_pool_name())) {
      auto ssl_mode = server_conn.ssl_mode();

      auto is_full_res = pool->add_if_not_full(make_pooled_connection(
          std::exchange(socket_splicer->server_conn(),
                        TlsSwitchableConnection{
                            nullptr,   // connection
                            nullptr,   // routing-connection
                            ssl_mode,  //
                            std::make_unique<ClassicProtocolState>()})));

      if (is_full_res) {
        // pool is full, restore the connection.
        server_conn = make_connection_from_pooled(std::move(*is_full_res));
        return {false};
      }
    }
  }

  return {true};
}

stdx::expected<Processor::Result, std::error_code>
ForwardingProcessor::socket_reconnect_start() {
  connection()->push_processor(std::make_unique<ConnectProcessor>(
      connection(), [this](classic_protocol::message::server::Error err) {
        reconnect_error_ = std::move(err);
      }));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ForwardingProcessor::mysql_reconnect_start() {
  connection()->push_processor(std::make_unique<LazyConnector>(
      connection(), false /* not in handshake */,
      [this](classic_protocol::message::server::Error err) {
        reconnect_error_ = std::move(err);
      }));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ForwardingProcessor::reconnect_send_error_msg(
    Channel *src_channel, ClassicProtocolState *src_protocol) {
  auto err = reconnect_error_.error_code() != 0
                 ? reconnect_error_
                 : classic_protocol::message::server::Error{
                       2003, "Connect to backend failed.", "HY000"};

  if (err.error_code() == 1045) {
    // rewrite the auth-fail error.
    err.message("Access denied for user '" +
                connection()->client_protocol()->username() +
                "' for router while reauthenticating");
  }

  auto send_res = ClassicFrame::send_msg(src_channel, src_protocol, err);
  if (!send_res) return send_client_failed(send_res.error());

  return Result::SendToClient;
}
