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

#ifndef ROUTING_CLASSIC_CONNECTION_INCLUDED
#define ROUTING_CLASSIC_CONNECTION_INCLUDED

#include <functional>
#include <string>
#include <vector>

#include "channel.h"
#include "connection.h"  // MySQLRoutingConnectionBase
#include "mysqlrouter/connection_pool.h"

/**
 * protocol state of a classic protocol connection.
 */
class ClassicProtocolState : public ProtocolStateBase {
 public:
  ClassicProtocolState() = default;

  ClassicProtocolState(classic_protocol::capabilities::value_type server_caps,
                       classic_protocol::capabilities::value_type client_caps)
      : server_caps_{server_caps}, client_caps_{client_caps} {}

  void server_capabilities(classic_protocol::capabilities::value_type caps) {
    server_caps_ = caps;
  }

  void client_capabilities(classic_protocol::capabilities::value_type caps) {
    client_caps_ = caps;
  }

  classic_protocol::capabilities::value_type client_capabilities() const {
    return client_caps_;
  }

  classic_protocol::capabilities::value_type server_capabilities() const {
    return server_caps_;
  }

  classic_protocol::capabilities::value_type shared_capabilities() const {
    return server_caps_ & client_caps_;
  }

  std::optional<classic_protocol::message::client::Greeting> client_greeting()
      const {
    return client_greeting_;
  }

  void client_greeting(
      std::optional<classic_protocol::message::client::Greeting> msg) {
    client_greeting_ = std::move(msg);
  }

  std::optional<classic_protocol::message::server::Greeting> server_greeting()
      const {
    return server_greeting_;
  }

  void server_greeting(
      std::optional<classic_protocol::message::server::Greeting> msg) {
    server_greeting_ = std::move(msg);
  }

  uint8_t &seq_id() { return seq_id_; }
  uint8_t seq_id() const { return seq_id_; }

  void seq_id(uint8_t id) { seq_id_ = id; }

  struct FrameInfo {
    uint8_t seq_id_;               //!< sequence id.
    size_t frame_size_;            //!< size of the whole frame.
    size_t forwarded_frame_size_;  //!< size of the whole frame that's already
                                   //!< forwarded.
  };

  std::optional<FrameInfo> &current_frame() { return current_frame_; }
  std::optional<uint8_t> &current_msg_type() { return msg_type_; }

  uint64_t columns_left{};
  uint32_t params_left{};

  [[nodiscard]] std::string auth_method_name() const {
    return auth_method_name_;
  }

  void auth_method_name(std::string name) {
    auth_method_name_ = std::move(name);
  }

 private:
  classic_protocol::capabilities::value_type server_caps_{};
  classic_protocol::capabilities::value_type client_caps_{};

  std::optional<classic_protocol::message::client::Greeting> client_greeting_{};
  std::optional<classic_protocol::message::server::Greeting> server_greeting_{};

  std::optional<FrameInfo> current_frame_{};
  std::optional<uint8_t> msg_type_{};

  uint8_t seq_id_{255};  // next use will increment to 0

  std::string auth_method_name_;
};

class MysqlRoutingClassicConnection : public MySQLRoutingConnectionBase {
 public:
  using connector_type = PooledConnector<PooledClassicConnection>;

  /**
   * try to pop a connection from the connection pool.
   *
   * called by the Pooled Connector.
   *
   * @param ep endpoint the connection is targeted for.
   *
   * @return if the optional has a value, a classic-connection that is open.
   */

  std::optional<PooledClassicConnection> try_pop_pooled_connection(
      const net::ip::tcp::endpoint &ep);

  MysqlRoutingClassicConnection(
      MySQLRoutingContext &context, RouteDestination *route_destination,
      std::unique_ptr<ConnectionBase> client_connection,
      std::unique_ptr<RoutingConnectionBase> client_routing_connection,
      std::function<void(MySQLRoutingConnectionBase *)> remove_callback)
      : MySQLRoutingConnectionBase{context, std::move(remove_callback)},
        connector_{client_connection->io_ctx(), route_destination,
                   [this](const net::ip::tcp::endpoint &ep) {
                     return try_pop_pooled_connection(ep);
                   }},
        socket_splicer_{std::make_unique<ProtocolSplicerBase>(
            TlsSwitchableConnection{
                std::move(client_connection),
                std::move(client_routing_connection),
                {context.source_ssl_mode(),
                 [this]() -> SSL_CTX * {
                   return this->context().source_ssl_ctx()->get();
                 }},
                std::make_unique<ClassicProtocolState>()},
            TlsSwitchableConnection{
                nullptr,
                nullptr,
                {context.dest_ssl_mode(),
                 [this]() -> SSL_CTX * {
                   auto make_res = mysql_harness::make_tcp_address(
                       this->get_destination_id());
                   if (!make_res) {
                     return nullptr;
                   }

                   return this->context()
                       .dest_ssl_ctx(make_res->address())
                       ->get();
                 }},
                std::make_unique<ClassicProtocolState>()})} {}

  enum class ForwardResult {
    kWantRecvSource,
    kWantSendSource,
    kWantRecvDestination,
    kWantSendDestination,
    kFinished,
  };

  static stdx::expected<size_t, std::error_code> encode_error_packet(
      std::vector<uint8_t> &error_frame, const uint8_t seq_id,
      const classic_protocol::capabilities::value_type caps,
      const uint16_t error_code, const std::string &msg,
      const std::string &sql_state);

  void on_handshake_received();
  void on_handshake_aborted();

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
    std::lock_guard<std::mutex> lk(disconnect_mtx_);
    (void)socket_splicer()->client_conn().cancel();
    (void)socket_splicer()->server_conn().cancel();
    connector().socket().cancel();
    disconnect_ = true;
  }

  void async_run();

 private:
  void send_server_failed(std::error_code ec);

  void recv_server_failed(std::error_code ec);

  void send_client_failed(std::error_code ec);

  void recv_client_failed(std::error_code ec);

  void server_socket_failed(std::error_code ec);

  void client_socket_failed(std::error_code ec);

  enum class Function {
    kServerGreetingFromServer,
    kClientRecvClientGreeting,
    kClientSendServerGreetingFromServer,
    kTlsAccept,

    kConnect,

    kServerRecvChangeUserResponse,
    kServerRecvChangeUserResponseOk,
    kServerRecvChangeUserResponseError,
    kServerRecvChangeUserResponseAuthMethodSwitch,

    kClientRecvSecondClientGreeting,

    kTlsConnectInit,
    kTlsConnect,

    kForwardTlsInit,
    kForwardTlsClientToServer,
    kForwardTlsServerToClient,

    kAuthResponse,
    kAuthResponseOk,
    kAuthResponseError,
    kAuthResponseAuthMethodSwitch,
    kAuthResponseData,
    kAuthClientContinue,

    kClientRecvCmd,

    kCmdPing,
    kCmdPingResponse,

    kCmdQuery,
    kCmdQueryResponse,
    kCmdQueryColumnCount,
    kCmdQueryColumnMeta,
    kCmdQueryColumnMetaForward,
    kCmdQueryColumnMetaForwardLast,
    kCmdQueryEndOfColumnMeta,
    kCmdQueryRow,
    kCmdQueryRowForward,
    kCmdQueryRowForwardLast,
    kCmdQueryRowForwardMoreResultsets,
    kCmdQueryOk,
    kCmdQueryError,
    kCmdQueryLoadData,
    kCmdQueryLoadDataResponse,
    kCmdQueryLoadDataResponseForward,
    kCmdQueryLoadDataResponseForwardLast,

    kCmdQuit,
    kCmdQuitResponse,

    kCmdInitSchema,
    kCmdInitSchemaResponse,

    kCmdResetConnection,
    kCmdResetConnectionResponse,

    kCmdKill,
    kCmdKillResponse,

    kCmdReload,
    kCmdReloadResponse,

    kCmdStatistics,
    kCmdStatisticsResponse,

    kCmdChangeUser,
    kCmdChangeUserResponse,
    kCmdChangeUserResponseOk,
    kCmdChangeUserResponseError,
    kCmdChangeUserResponseSwitchAuth,
    kCmdChangeUserResponseContinue,
    kCmdChangeUserClientAuthContinue,

    kCmdStmtPrepare,
    kCmdStmtPrepareResponse,
    kCmdStmtPrepareResponseOk,
    kCmdStmtPrepareResponseError,
    kCmdStmtPrepareResponseCheckParam,
    kCmdStmtPrepareResponseForwardParam,
    kCmdStmtPrepareResponseForwardParamLast,
    kCmdStmtPrepareResponseForwardEndOfParams,
    kCmdStmtPrepareResponseCheckColumn,
    kCmdStmtPrepareResponseForwardColumn,
    kCmdStmtPrepareResponseForwardColumnLast,
    kCmdStmtPrepareResponseForwardEndOfColumns,

    kCmdStmtExecute,
    kCmdStmtExecuteResponse,
    kCmdStmtExecuteResponseOk,
    kCmdStmtExecuteResponseError,
    kCmdStmtExecuteResponseCheckColumn,
    kCmdStmtExecuteResponseColumnCount,
    kCmdStmtExecuteResponseForwardColumn,
    kCmdStmtExecuteResponseForwardColumnLast,
    kCmdStmtExecuteResponseForwardEndOfColumns,
    kCmdStmtExecuteResponseCheckRow,
    kCmdStmtExecuteResponseForwardRow,
    kCmdStmtExecuteResponseForwardEndOfRows,

    kCmdStmtSetOption,
    kCmdStmtSetOptionResponse,

    kCmdStmtFetch,
    kCmdStmtFetchResponse,

    kCmdStmtClose,

    kCmdStmtReset,
    kCmdStmtResetResponse,

    kCmdStmtParamAppendData,

    kCmdListFields,
    kCmdListFieldsResponse,
    kCmdListFieldsResponseForward,
    kCmdListFieldsResponseForwardLast,

    kCmdClone,
    kCmdCloneResponse,
    kCmdCloneResponseForwardOk,
    kCmdCloneResponseForwardError,
    kClientRecvCloneCmd,
    kCmdCloneInit,
    kCmdCloneInitResponse,
    kCmdCloneInitResponseForward,
    kCmdCloneInitResponseForwardLast,
    kCmdCloneExit,
    kCmdCloneExitResponse,
    kCmdCloneExitResponseForward,
    kCmdCloneExitResponseForwardLast,

    kCmdBinlogDump,
    kCmdBinlogDumpGtid,
    kCmdBinlogDumpResponse,
    kCmdBinlogDumpResponseForward,
    kCmdBinlogDumpResponseForwardLast,
    kCmdRegisterReplica,
    kCmdRegisterReplicaResponse,

    kWaitClientClosed,
    kFinish,
  };

  void call_next_function(Function next) {
    switch (next) {
      case Function::kConnect:
        return connect();
      case Function::kServerGreetingFromServer:
        return server_recv_server_greeting_from_server();
      case Function::kClientSendServerGreetingFromServer:
        return client_send_server_greeting_from_server();
      case Function::kWaitClientClosed:
        return async_wait_client_closed();
      case Function::kClientRecvCmd:
        return client_recv_cmd();

      case Function::kServerRecvChangeUserResponse:
        return server_recv_change_user_response();
      case Function::kServerRecvChangeUserResponseError:
        return server_recv_change_user_response_error();
      case Function::kServerRecvChangeUserResponseOk:
        return server_recv_change_user_response_ok();
      case Function::kServerRecvChangeUserResponseAuthMethodSwitch:
        return server_recv_change_user_response_auth_method_switch();

      case Function::kClientRecvClientGreeting:
        return client_recv_client_greeting();
      case Function::kClientRecvSecondClientGreeting:
        return client_recv_second_client_greeting();
      case Function::kTlsAccept:
        return tls_accept();
      case Function::kTlsConnectInit:
        return tls_connect_init();
      case Function::kTlsConnect:
        return tls_connect();
      case Function::kAuthResponse:
        return auth_response();
      case Function::kAuthResponseOk:
        return auth_response_ok();
      case Function::kAuthResponseError:
        return auth_response_error();
      case Function::kAuthResponseData:
        return auth_response_data();
      case Function::kAuthResponseAuthMethodSwitch:
        return auth_response_auth_method_switch();
      case Function::kAuthClientContinue:
        return auth_client_continue();

      case Function::kCmdQuery:
        return cmd_query();
      case Function::kCmdQueryResponse:
        return cmd_query_response();
      case Function::kCmdQueryColumnCount:
        return cmd_query_column_count();
      case Function::kCmdQueryColumnMeta:
        return cmd_query_column_meta();
      case Function::kCmdQueryColumnMetaForward:
        return cmd_query_column_meta_forward();
      case Function::kCmdQueryColumnMetaForwardLast:
        return cmd_query_column_meta_forward_last();
      case Function::kCmdQueryEndOfColumnMeta:
        return cmd_query_end_of_column_meta();
      case Function::kCmdQueryOk:
        return cmd_query_ok();
      case Function::kCmdQueryError:
        return cmd_query_error();
      case Function::kCmdQueryLoadData:
        return cmd_query_load_data();
      case Function::kCmdQueryLoadDataResponse:
        return cmd_query_load_data_response();
      case Function::kCmdQueryLoadDataResponseForward:
        return cmd_query_load_data_response_forward();
      case Function::kCmdQueryLoadDataResponseForwardLast:
        return cmd_query_load_data_response_forward_last();
      case Function::kCmdQueryRow:
        return cmd_query_row();
      case Function::kCmdQueryRowForward:
        return cmd_query_row_forward();
      case Function::kCmdQueryRowForwardLast:
        return cmd_query_row_forward_last();
      case Function::kCmdQueryRowForwardMoreResultsets:
        return cmd_query_row_forward_more_resultsets();

      case Function::kCmdQuit:
        return cmd_quit();
      case Function::kCmdQuitResponse:
        return cmd_quit_response();

      case Function::kCmdPing:
        return cmd_ping();
      case Function::kCmdPingResponse:
        return cmd_ping_response();

      case Function::kCmdInitSchema:
        return cmd_init_schema();
      case Function::kCmdInitSchemaResponse:
        return cmd_init_schema_response();

      // reset connection
      case Function::kCmdResetConnection:
        return cmd_reset_connection();
      case Function::kCmdResetConnectionResponse:
        return cmd_reset_connection_response();
      case Function::kCmdKill:
        return cmd_kill();
      case Function::kCmdKillResponse:
        return cmd_kill_response();

      case Function::kCmdChangeUser:
        return cmd_change_user();
      case Function::kCmdChangeUserResponse:
        return cmd_change_user_response();
      case Function::kCmdChangeUserResponseOk:
        return cmd_change_user_response_ok();
      case Function::kCmdChangeUserResponseError:
        return cmd_change_user_response_error();
      case Function::kCmdChangeUserResponseSwitchAuth:
        return cmd_change_user_response_switch_auth();
      case Function::kCmdChangeUserResponseContinue:
        return cmd_change_user_response_continue();
      case Function::kCmdChangeUserClientAuthContinue:
        return cmd_change_user_client_auth_continue();

      case Function::kCmdReload:
        return cmd_reload();
      case Function::kCmdReloadResponse:
        return cmd_reload_response();

      case Function::kCmdStatistics:
        return cmd_statistics();
      case Function::kCmdStatisticsResponse:
        return cmd_statistics_response();

      case Function::kCmdListFields:
        return cmd_list_fields();
      case Function::kCmdListFieldsResponse:
        return cmd_list_fields_response();
      case Function::kCmdListFieldsResponseForward:
        return cmd_list_fields_response_forward();
      case Function::kCmdListFieldsResponseForwardLast:
        return cmd_list_fields_response_forward_last();

      case Function::kCmdStmtPrepare:
        return cmd_stmt_prepare();
      case Function::kCmdStmtPrepareResponse:
        return cmd_stmt_prepare_response();
      case Function::kCmdStmtPrepareResponseOk:
        return cmd_stmt_prepare_response_ok();
      case Function::kCmdStmtPrepareResponseError:
        return cmd_stmt_prepare_response_error();
      case Function::kCmdStmtPrepareResponseCheckColumn:
        return cmd_stmt_prepare_response_check_column();
      case Function::kCmdStmtPrepareResponseForwardColumn:
        return cmd_stmt_prepare_response_forward_column();
      case Function::kCmdStmtPrepareResponseForwardColumnLast:
        return cmd_stmt_prepare_response_forward_column_last();
      case Function::kCmdStmtPrepareResponseForwardEndOfColumns:
        return cmd_stmt_prepare_response_forward_end_of_columns();
      case Function::kCmdStmtPrepareResponseCheckParam:
        return cmd_stmt_prepare_response_check_param();
      case Function::kCmdStmtPrepareResponseForwardParam:
        return cmd_stmt_prepare_response_forward_param();
      case Function::kCmdStmtPrepareResponseForwardParamLast:
        return cmd_stmt_prepare_response_forward_param_last();
      case Function::kCmdStmtPrepareResponseForwardEndOfParams:
        return cmd_stmt_prepare_response_forward_end_of_params();

      case Function::kCmdStmtExecute:
        return cmd_stmt_execute();
      case Function::kCmdStmtExecuteResponse:
        return cmd_stmt_execute_response();
      case Function::kCmdStmtExecuteResponseOk:
        return cmd_stmt_execute_response_ok();
      case Function::kCmdStmtExecuteResponseError:
        return cmd_stmt_execute_response_error();
      case Function::kCmdStmtExecuteResponseColumnCount:
        return cmd_stmt_execute_response_column_count();
      case Function::kCmdStmtExecuteResponseCheckColumn:
        return cmd_stmt_execute_response_check_column();
      case Function::kCmdStmtExecuteResponseForwardColumn:
        return cmd_stmt_execute_response_forward_column();
      case Function::kCmdStmtExecuteResponseForwardColumnLast:
        return cmd_stmt_execute_response_forward_column_last();
      case Function::kCmdStmtExecuteResponseForwardEndOfColumns:
        return cmd_stmt_execute_response_forward_end_of_columns();
      case Function::kCmdStmtExecuteResponseCheckRow:
        return cmd_stmt_execute_response_check_row();
      case Function::kCmdStmtExecuteResponseForwardRow:
        return cmd_stmt_execute_response_forward_row();
      case Function::kCmdStmtExecuteResponseForwardEndOfRows:
        return cmd_stmt_execute_response_forward_end_of_rows();

      case Function::kCmdStmtSetOption:
        return cmd_stmt_set_option();
      case Function::kCmdStmtSetOptionResponse:
        return cmd_stmt_set_option_response();

      case Function::kCmdStmtClose:
        return cmd_stmt_close();

      case Function::kCmdStmtFetch:
        return cmd_stmt_fetch();
      case Function::kCmdStmtFetchResponse:
        return cmd_stmt_fetch_response();

      case Function::kCmdStmtReset:
        return cmd_stmt_reset();
      case Function::kCmdStmtResetResponse:
        return cmd_stmt_reset_response();

      case Function::kCmdStmtParamAppendData:
        return cmd_stmt_param_append_data();

      // clone
      case Function::kCmdClone:
        return cmd_clone();
      case Function::kCmdCloneResponse:
        return cmd_clone_response();
      case Function::kCmdCloneResponseForwardError:
        return cmd_clone_response_forward_error();
      case Function::kCmdCloneResponseForwardOk:
        return cmd_clone_response_forward_ok();
      case Function::kClientRecvCloneCmd:
        return client_recv_clone_cmd();
      case Function::kCmdCloneInit:
        return cmd_clone_init();
      case Function::kCmdCloneInitResponse:
        return cmd_clone_init_response();
      case Function::kCmdCloneInitResponseForward:
        return cmd_clone_init_response_forward();
      case Function::kCmdCloneInitResponseForwardLast:
        return cmd_clone_init_response_forward_last();
      case Function::kCmdCloneExit:
        return cmd_clone_exit();
      case Function::kCmdCloneExitResponse:
        return cmd_clone_exit_response();
      case Function::kCmdCloneExitResponseForward:
        return cmd_clone_exit_response_forward();
      case Function::kCmdCloneExitResponseForwardLast:
        return cmd_clone_exit_response_forward_last();

      // binlog
      case Function::kCmdBinlogDump:
        return cmd_binlog_dump();
      case Function::kCmdBinlogDumpGtid:
        return cmd_binlog_dump_gtid();
      case Function::kCmdBinlogDumpResponse:
        return cmd_binlog_dump_response();
      case Function::kCmdBinlogDumpResponseForward:
        return cmd_binlog_dump_response_forward();
      case Function::kCmdBinlogDumpResponseForwardLast:
        return cmd_binlog_dump_response_forward_last();

      case Function::kCmdRegisterReplica:
        return cmd_register_replica();
      case Function::kCmdRegisterReplicaResponse:
        return cmd_register_replica_response();

      case Function::kForwardTlsInit:
        return forward_tls_init();
      case Function::kForwardTlsClientToServer:
        return forward_tls_client_to_server();
      case Function::kForwardTlsServerToClient:
        return forward_tls_server_to_client();
      case Function::kFinish:
        return finish();
    }
  }

  void async_send_client(Function next);

  void async_recv_client(Function next);

  void async_send_server(Function next);

  void async_recv_server(Function next);

  void async_send_client_and_finish();

  void async_wait_client_closed();

  // the client didn't send a Greeting before closing the connection.
  //
  // Generate a Greeting to be sent to the server, to ensure the router's IP
  // isn't blocked due to the server's max_connect_errors.
  void server_side_client_greeting();

  // after a QUIT, we should wait until the client closed the connection.

  // called when the connection should be closed.
  //
  // called multiple times (once per "active_work_").
  void finish();

  // final state.
  //
  // removes the connection from the connection-container.
  void done();

  // the server::Error path of server_recv_server_greeting
  void server_greeting_error();

  // the server::Greeting path of server_recv_server_greeting
  void server_recv_server_greeting_greeting();

  void connect();

  void server_send_change_user();
  void server_recv_change_user_response();
  void server_recv_change_user_response_error();
  void server_recv_change_user_response_ok();
  void server_recv_change_user_response_auth_method_switch();

  /**
   * server-greeting.
   *
   * expects
   *
   * - error-message
   * - server-greeting
   *
   * when a server-greeting is received:
   *
   * - waits for the server greeting to be complete
   * - parses server-greeting message
   * - unsets compress capabilities
   * - tracks capabilities.
   */
  void server_recv_server_greeting_from_server();

  void client_send_server_greeting_from_server();

  void client_send_server_greeting_from_router();

  /**
   * process the Client Greeting packet from the client.
   *
   * - wait for for a full protocol frame
   * - decode client-greeting packet and decide how to proceed based on
   * capabilities and configuration
   *
   * ## client-side connection state
   *
   * ssl-cap::client
   * :  SSL capability the client sends to router
   *
   * ssl-cap::server
   * :  SSL capability the server sends to router
   *
   * ssl-mode::client
   * :  client_ssl_mode used by router
   *
   * ssl-mode::server
   * :  server_ssl_mode used by router
   *
   * | ssl-mode    | ssl-mode | ssl-cap | ssl-cap  | ssl    |
   * | client      | server   | client  | server   | client |
   * | ----------- | -------- | ------- | -------- | ------ |
   * | DISABLED    | any      | any     | any      | PLAIN  |
   * | PREFERRED   | any      | [ ]     | any      | PLAIN  |
   * | PREFERRED   | any      | [x]     | any      | SSL    |
   * | REQUIRED    | any      | [ ]     | any      | FAIL   |
   * | REQUIRED    | any      | [x]     | any      | SSL    |
   * | PASSTHROUGH | any      | [ ]     | any      | PLAIN  |
   * | PASSTHROUGH | any      | [x]     | [x]      | (SSL)  |
   *
   * PLAIN
   * :  client-side connection is plaintext
   *
   * FAIL
   * :  router fails connection with client
   *
   * SSL
   * :  encrypted, client-side TLS endpoint
   *
   * (SSL)
   * :  encrypted, no TLS endpoint
   *
   * ## server-side connection state
   *
   * | ssl-mode    | ssl-mode  | ssl-cap | ssl-cap | ssl    |
   * | client      | server    | client  | server  | server |
   * | ----------- | --------- | ------- | ------- | ------ |
   * | any         | DISABLED  | any     | any     | PLAIN  |
   * | any         | PREFERRED | any     | [ ]     | PLAIN  |
   * | any         | PREFERRED | any     | [x]     | SSL    |
   * | any         | REQUIRED  | any     | [ ]     | FAIL   |
   * | any         | REQUIRED  | any     | [x]     | SSL    |
   * | PASSTHROUGH | AS_CLIENT | [ ]     | any     | PLAIN  |
   * | PASSTHROUGH | AS_CLIENT | [x]     | [x]     | (SSL)  |
   * | other       | AS_CLIENT | [ ]     | any     | PLAIN  |
   * | other       | AS_CLIENT | [x]     | [ ]     | FAIL   |
   * | other       | AS_CLIENT | [x]     | [x]     | SSL    |
   *
   * PLAIN
   * :  server-side connection is plaintext
   *
   * FAIL
   * :  router fails connection with client
   *
   * SSL
   * :  encrypted
   *
   * (SSL)
   * :  encrypted, no TLS endpoint
   *
   */

  stdx::expected<classic_protocol::message::client::Greeting, std::error_code>
  decode_client_greeting(Channel *src_channel,
                         ClassicProtocolState *src_protocol);

  // called after server connection is established.
  void client_greeting_server_adjust_caps(ClassicProtocolState *src_protocol,
                                          ClassicProtocolState *dst_protocol);

  stdx::expected<size_t, std::error_code> encode_client_greeting(
      const classic_protocol::message::client::Greeting &msg,
      ClassicProtocolState *dst_protocol, std::vector<uint8_t> &send_buf);

  void server_send_client_greeting_start_tls();

  /**
   * c<-r: err
   * or
   * r->s: client::greeting
   * or
   * r->s: client::greeting_ssl
   */
  void server_send_first_client_greeting();

  void server_send_client_greeting_full();

  // receive the first client greeting.
  void client_recv_client_greeting();

  void tls_accept_init();

  /**
   * accept a TLS handshake.
   */
  void tls_accept();
  /**
   * after tls-accept expect the full client-greeting.
   */
  void client_recv_second_client_greeting();

  void tls_connect_init();

  /**
   * connect server_channel to a TLS server.
   */
  void tls_connect();

  stdx::expected<void, std::error_code> forward_tls(Channel *src_channel,
                                                    Channel *dst_channel);

  void forward_tls_client_to_server();

  void forward_tls_server_to_client();

  void forward_tls_init();

  stdx::expected<ForwardResult, std::error_code>
  forward_frame_sequence_from_client_to_server();

  void forward_client_to_server(Function this_func, Function next_func);

  stdx::expected<ForwardResult, std::error_code>
  forward_frame_sequence_from_server_to_client();

  void forward_server_to_client(Function this_func, Function next_func,
                                bool flush_before_next_func_optional = false);

  void auth_client_continue();

  void auth_response_auth_method_switch();

  void auth_response();
  void auth_response_ok();
  void auth_response_error();
  void auth_response_data();

  void cmd_query();
  void cmd_query_response();
  void cmd_query_ok();
  void cmd_query_error();
  void cmd_query_load_data();
  void cmd_query_load_data_response();
  void cmd_query_load_data_response_forward();
  void cmd_query_load_data_response_forward_last();
  void cmd_query_column_count();
  void cmd_query_column_meta_forward();
  void cmd_query_column_meta_forward_last();
  void cmd_query_column_meta();
  void cmd_query_end_of_column_meta();
  void cmd_query_row();
  void cmd_query_row_forward_last();
  void cmd_query_row_forward();
  void cmd_query_row_forward_more_resultsets();

  void cmd_ping();
  void cmd_ping_response();

  void cmd_quit();
  void cmd_quit_response();

  void cmd_init_schema();
  void cmd_init_schema_response();

  void cmd_reset_connection();
  void cmd_reset_connection_response();

  void cmd_kill();
  void cmd_kill_response();

  void cmd_change_user();
  void cmd_change_user_response();
  void cmd_change_user_response_ok();
  void cmd_change_user_response_error();
  void cmd_change_user_response_switch_auth();
  void cmd_change_user_response_continue();
  void cmd_change_user_client_auth_continue();

  void cmd_reload();
  void cmd_reload_response();

  void cmd_statistics();
  void cmd_statistics_response();

  void cmd_stmt_prepare();
  void cmd_stmt_prepare_response();
  void cmd_stmt_prepare_response_ok();
  void cmd_stmt_prepare_response_check_param();
  void cmd_stmt_prepare_response_forward_param();
  void cmd_stmt_prepare_response_forward_param_last();
  void cmd_stmt_prepare_response_forward_end_of_params();
  void cmd_stmt_prepare_response_check_column();
  void cmd_stmt_prepare_response_forward_column();
  void cmd_stmt_prepare_response_forward_column_last();
  void cmd_stmt_prepare_response_forward_end_of_columns();
  void cmd_stmt_prepare_response_error();

  void cmd_stmt_execute();
  void cmd_stmt_execute_response();
  void cmd_stmt_execute_response_ok();
  void cmd_stmt_execute_response_error();
  void cmd_stmt_execute_response_column_count();
  void cmd_stmt_execute_response_check_column();
  void cmd_stmt_execute_response_forward_column();
  void cmd_stmt_execute_response_forward_column_last();
  void cmd_stmt_execute_response_forward_end_of_columns();
  void cmd_stmt_execute_response_check_row();
  void cmd_stmt_execute_response_forward_row();
  void cmd_stmt_execute_response_forward_end_of_rows();

  void cmd_stmt_set_option();
  void cmd_stmt_set_option_response();

  void cmd_stmt_reset();
  void cmd_stmt_reset_response();

  void cmd_stmt_close();

  void cmd_stmt_param_append_data();

  void cmd_stmt_fetch();
  void cmd_stmt_fetch_response();

  void cmd_list_fields();
  void cmd_list_fields_response();
  void cmd_list_fields_response_forward();
  void cmd_list_fields_response_forward_last();

  void cmd_clone();
  void cmd_clone_response();
  void cmd_clone_response_forward_ok();
  void cmd_clone_response_forward_error();
  void client_recv_clone_cmd();

  void cmd_clone_init();
  void cmd_clone_init_response();
  void cmd_clone_init_response_forward();
  void cmd_clone_init_response_forward_last();
  void cmd_clone_exit();
  void cmd_clone_exit_response();
  void cmd_clone_exit_response_forward();
  void cmd_clone_exit_response_forward_last();

  void cmd_binlog_dump();
  void cmd_binlog_dump_gtid();
  void cmd_binlog_dump_response();
  void cmd_binlog_dump_response_forward();
  void cmd_binlog_dump_response_forward_last();
  void cmd_register_replica();
  void cmd_register_replica_response();

  // something was received on the client channel.
  void client_recv_cmd();

  ClassicProtocolState *client_protocol() {
    return dynamic_cast<ClassicProtocolState *>(
        socket_splicer()->client_conn().protocol());
  }

  ClassicProtocolState *server_protocol() {
    return dynamic_cast<ClassicProtocolState *>(
        socket_splicer()->server_conn().protocol());
  }

  const ProtocolSplicerBase *socket_splicer() const {
    return socket_splicer_.get();
  }

  ProtocolSplicerBase *socket_splicer() { return socket_splicer_.get(); }

  std::string get_destination_id() const override {
    return connector().destination_id();
  }

  int active_work_{0};

  bool client_greeting_sent_{false};

  /**
   * if the router is sending the initial server-greeting.
   *
   * if true, the router sends the initial greeting to the client,
   * if false, the server is sending the initial greeting and router is forward
   * it.
   */
  bool greeting_from_router_{false};

  connector_type &connector() { return connector_; }
  const connector_type &connector() const { return connector_; }

  connector_type connector_;

  std::unique_ptr<ProtocolSplicerBase> socket_splicer_;
};

#endif
