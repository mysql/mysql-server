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

#ifndef ROUTING_X_CONNECTION_INCLUDED
#define ROUTING_X_CONNECTION_INCLUDED

#include <memory>
#include <optional>
#include <string>

#include <mysqlx.pb.h>
#include <mysqlx_connection.pb.h>

#include "connection.h"  // MySQLRoutingConnectionBase

class XProtocolState : public ProtocolStateBase {
 public:
  struct FrameInfo {
    size_t frame_size_;            //!< size of the whole frame
    size_t forwarded_frame_size_;  //!< size of the forwarded part of frame
  };

  std::optional<FrameInfo> &current_frame() { return current_frame_; }
  std::optional<uint8_t> &current_msg_type() { return msg_type_; }

  Mysqlx::Connection::Capabilities *caps() { return caps_.get(); }
  void caps(std::unique_ptr<Mysqlx::Connection::Capabilities> caps) {
    caps_ = std::move(caps);
  }

 private:
  std::optional<FrameInfo> current_frame_{};
  std::optional<uint8_t> msg_type_{};

  std::unique_ptr<Mysqlx::Connection::Capabilities> caps_;
};

class MysqlRoutingXConnection : public MySQLRoutingConnectionBase {
 public:
  using connector_type = Connector<std::unique_ptr<ConnectionBase>>;

  MysqlRoutingXConnection(
      MySQLRoutingContext &context, RouteDestination *route_destination,
      std::unique_ptr<ConnectionBase> client_connection,
      std::unique_ptr<RoutingConnectionBase> client_routing_connection,
      std::function<void(MySQLRoutingConnectionBase *)> remove_callback)
      : MySQLRoutingConnectionBase{context, std::move(remove_callback)},
        connector_{client_connection->io_ctx(), route_destination},
        socket_splicer_{std::make_unique<ProtocolSplicerBase>(
            TlsSwitchableConnection{
                std::move(client_connection),
                std::move(client_routing_connection),
                {context.source_ssl_mode(),
                 [this]() -> SSL_CTX * {
                   return this->context().source_ssl_ctx()->get();
                 }},
                std::make_unique<XProtocolState>()},
            TlsSwitchableConnection{nullptr,
                                    nullptr,
                                    {context.dest_ssl_mode(),
                                     [this]() -> SSL_CTX * {
                                       auto make_res =
                                           mysql_harness::make_tcp_address(
                                               this->get_destination_id());
                                       if (!make_res) {
                                         return nullptr;
                                       }

                                       return this->context()
                                           .dest_ssl_ctx(make_res->address())
                                           ->get();
                                     }},
                                    std::make_unique<XProtocolState>()})} {}

  static stdx::expected<size_t, std::error_code> encode_error_packet(
      std::vector<uint8_t> &error_frame, uint16_t error_code,
      const std::string &msg, const std::string &sql_state = "HY000",
      Mysqlx::Error::Severity severity = Mysqlx::Error::ERROR);

  SslMode source_ssl_mode() const {
    return this->socket_splicer()->source_ssl_mode();
  }

  SslMode dest_ssl_mode() const {
    return this->socket_splicer()->dest_ssl_mode();
  }

  std::string get_client_address() const override {
    return socket_splicer()->client_conn().endpoint();
  }

  std::string get_server_address() const override {
    return socket_splicer()->server_conn().endpoint();
  }

  void disconnect() override {
    (void)socket_splicer()->client_conn().cancel();
    (void)socket_splicer()->server_conn().cancel();
  }

  enum class Function {
    kClientRecvCmd,

    // tls-accept
    kTlsAcceptInit,
    kTlsAccept,
    kTlsAcceptFinalize,

    kServerInitTls,
    kServerRecvSwitchTlsResponse,

    kTlsConnectInit,
    kTlsConnect,

    kForwardTlsInit,
    kForwardTlsClientToServer,
    kForwardTlsServerToClient,

    kServerSendCheckCaps,
    kServerRecvCheckCapsResponse,

    // cap-get
    kClientCapGet,
    kServerRecvCapGetResponse,
    kServerRecvCapGetResponseForward,
    kServerRecvCapGetResponseForwardLast,
    kServerRecvSwitchTlsResponsePassthrough,
    kServerRecvSwitchTlsResponsePassthroughForward,
    kServerRecvSwitchTlsResponsePassthroughForwardLast,
    kServerRecvSwitchTlsResponsePassthroughForwardOk,

    // cap-set
    kClientCapSet,
    kServerRecvCapSetResponse,
    kServerRecvCapSetResponseForward,
    kServerRecvCapSetResponseForwardLast,

    // sess-auth
    kClientSessAuthStart,
    kServerRecvAuthResponse,
    kServerRecvAuthResponseForward,
    kServerRecvAuthResponseContinue,
    kServerRecvAuthResponseForwardLast,
    kClientRecvAuthContinue,

    // stmt-exec
    kClientStmtExecute,
    kServerRecvStmtExecuteResponse,
    kServerRecvStmtExecuteResponseForward,
    kServerRecvStmtExecuteResponseForwardLast,

    kConnect,
    kWaitClientClose,
    kFinish,
  };

  void call_next_function(Function next) {
    switch (next) {
      case Function::kClientRecvCmd:
        return client_recv_cmd();

      case Function::kTlsAcceptInit:
        return tls_accept_init();
      case Function::kTlsAccept:
        return tls_accept();
      case Function::kTlsAcceptFinalize:
        return tls_accept_finalize();

      case Function::kServerInitTls:
        return server_init_tls();
      case Function::kServerRecvSwitchTlsResponse:
        return server_recv_switch_tls_response();

      case Function::kTlsConnectInit:
        return tls_connect_init();
      case Function::kTlsConnect:
        return tls_connect();

      case Function::kServerSendCheckCaps:
        return server_send_check_caps();
      case Function::kServerRecvCheckCapsResponse:
        return server_recv_check_caps_response();

      case Function::kClientCapGet:
        return client_cap_get();
      case Function::kServerRecvCapGetResponse:
        return server_recv_cap_get_response();
      case Function::kServerRecvCapGetResponseForward:
        return server_recv_cap_get_response_forward();
      case Function::kServerRecvCapGetResponseForwardLast:
        return server_recv_cap_get_response_forward_last();

      case Function::kClientCapSet:
        return client_cap_set();
      case Function::kServerRecvCapSetResponse:
        return server_recv_cap_set_response();
      case Function::kServerRecvCapSetResponseForward:
        return server_recv_cap_set_response_forward();
      case Function::kServerRecvCapSetResponseForwardLast:
        return server_recv_cap_set_response_forward_last();
      case Function::kServerRecvSwitchTlsResponsePassthrough:
        return server_recv_switch_tls_response_passthrough();
      case Function::kServerRecvSwitchTlsResponsePassthroughForward:
        return server_recv_switch_tls_response_passthrough_forward();
      case Function::kServerRecvSwitchTlsResponsePassthroughForwardLast:
        return server_recv_switch_tls_response_passthrough_forward_last();
      case Function::kServerRecvSwitchTlsResponsePassthroughForwardOk:
        return server_recv_switch_tls_response_passthrough_forward_ok();

      case Function::kForwardTlsInit:
        return forward_tls_init();
      case Function::kForwardTlsClientToServer:
        return forward_tls_client_to_server();
      case Function::kForwardTlsServerToClient:
        return forward_tls_server_to_client();

      case Function::kClientSessAuthStart:
        return client_sess_auth_start();
      case Function::kServerRecvAuthResponse:
        return server_recv_auth_response();
      case Function::kServerRecvAuthResponseForward:
        return server_recv_auth_response_forward();
      case Function::kServerRecvAuthResponseContinue:
        return server_recv_auth_response_continue();
      case Function::kClientRecvAuthContinue:
        return client_recv_auth_continue();
      case Function::kServerRecvAuthResponseForwardLast:
        return server_recv_auth_response_forward_last();

      case Function::kClientStmtExecute:
        return client_stmt_execute();
      case Function::kServerRecvStmtExecuteResponse:
        return server_recv_stmt_execute_response();
      case Function::kServerRecvStmtExecuteResponseForward:
        return server_recv_stmt_execute_response_forward();
      case Function::kServerRecvStmtExecuteResponseForwardLast:
        return server_recv_stmt_execute_response_forward_last();

      case Function::kConnect:
        return connect();
      case Function::kWaitClientClose:
        return wait_client_close();
      case Function::kFinish:
        return finish();
    }
  }

  void send_server_failed(std::error_code ec);

  void recv_server_failed(std::error_code ec);

  void send_client_failed(std::error_code ec);

  void recv_client_failed(std::error_code ec);

  void client_socket_failed(std::error_code ec);

  void server_socket_failed(std::error_code ec);

  void async_send_client(Function next);

  void async_recv_client(Function next);

  void async_send_server(Function next);

  void async_recv_server(Function next);

  void async_run();

  void wait_client_close();
  void finish();

  void done();

  stdx::expected<void, std::error_code> forward_tls(Channel *src_channel,
                                                    Channel *dst_channel);

  void forward_tls_client_to_server();

  void forward_tls_server_to_client();

  void forward_tls_init();

  enum class ForwardResult {
    kWantRecvSource,
    kWantSendSource,
    kWantRecvDestination,
    kWantSendDestination,
    kFinished,
  };

  stdx::expected<ForwardResult, std::error_code>
  forward_frame_from_client_to_server();

  stdx::expected<ForwardResult, std::error_code>
  forward_frame_from_server_to_client();

  void forward_client_to_server(Function this_func, Function next_func);

  void forward_server_to_client(Function this_func, Function next_func);

  void async_send_client_buffer(net::const_buffer send_buf, Function next);
  void async_send_server_buffer(net::const_buffer send_buf, Function next);

  void client_send_server_greeting_from_router();
  void server_recv_server_greeting_from_server();
  void connect();

  void server_send_switch_to_tls();
  void server_send_check_caps();
  void server_recv_check_caps_response();

  void tls_accept_init();
  void tls_accept();
  void tls_accept_finalize();

  void server_init_tls();
  void server_recv_switch_tls_response();
  void tls_connect_init();
  void tls_connect();

  void client_recv_cmd();

  void client_cap_get();
  void server_recv_cap_get_response();
  void server_recv_cap_get_response_forward();
  void server_recv_cap_get_response_forward_last();

  void client_cap_set();
  void server_recv_cap_set_response();
  void server_recv_cap_set_response_forward();
  void server_recv_cap_set_response_forward_last();
  void server_recv_switch_tls_response_passthrough();
  void server_recv_switch_tls_response_passthrough_forward();
  void server_recv_switch_tls_response_passthrough_forward_last();
  void server_recv_switch_tls_response_passthrough_forward_ok();

  void passthrough_tls_init();

  void client_sess_auth_start();
  void client_recv_auth_continue();
  void server_recv_auth_response();
  void server_recv_auth_response_forward();
  void server_recv_auth_response_continue();
  void server_recv_auth_response_forward_last();

  void client_stmt_execute();
  void server_recv_stmt_execute_response();
  void server_recv_stmt_execute_response_forward();
  void server_recv_stmt_execute_response_forward_last();

  XProtocolState *client_protocol() {
    return dynamic_cast<XProtocolState *>(
        socket_splicer()->client_conn().protocol());
  }

  XProtocolState *server_protocol() {
    return dynamic_cast<XProtocolState *>(
        socket_splicer()->server_conn().protocol());
  }

  const ProtocolSplicerBase *socket_splicer() const {
    return socket_splicer_.get();
  }

  ProtocolSplicerBase *socket_splicer() { return socket_splicer_.get(); }

  std::string get_destination_id() const override {
    return connector().destination_id();
  }

 private:
  int active_work_{0};

  using clock_type = std::chrono::steady_clock;

  clock_type::time_point started_{clock_type::now()};
  clock_type::time_point last_trace_{clock_type::now()};

  bool greeting_from_router_{true};

  connector_type &connector() { return connector_; }
  const connector_type &connector() const { return connector_; }

  connector_type connector_;

  std::unique_ptr<ProtocolSplicerBase> socket_splicer_;
};

#endif
