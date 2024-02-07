/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTING_CLASSIC_GREETING_FORWARDER_INCLUDED
#define ROUTING_CLASSIC_GREETING_FORWARDER_INCLUDED

#include "forwarding_processor.h"

#include "router_require.h"

/**
 * classic protocol handshake between client<->router and router<->server.
 */
class ServerGreetor : public ForwardingProcessor {
 public:
  /**
   * construct a server::greeting processor.
   *
   *     c->r   : ...
   *        r->s: connect()
   *        r<-s: server::greeting
   *
   * a server greeting may be sent as part of the initial connection
   * setup between client<->router<->server (in_handshake=true) or
   * when router starts a connection on its own.
   *
   * If `in_handshake` is true, the ServerGreetor expects it can send:
   *
   * - server::AuthMethodSwitch and
   * - server::Ok
   *
   * to the client connection.
   *
   * @param conn the connection the greeting will be transferred on.
   * @param in_handshake true if the greeting is part of the initial
   * handshake.
   * @param on_error callback called on failure.
   * @param parent_event parent span for the TraceEvents
   */
  ServerGreetor(
      MysqlRoutingClassicConnectionBase *conn, bool in_handshake,
      std::function<void(const classic_protocol::message::server::Error &)>
          on_error,
      TraceEvent *parent_event)
      : ForwardingProcessor(conn),
        in_handshake_{in_handshake},
        on_error_(std::move(on_error)),
        parent_event_(parent_event) {}

  /**
   * stages of the handshake flow.
   *
   * - Client stages are on the client<->router side.
   * - Server stages are on the router<->server side.
   */
  enum class Stage {
    ServerGreeting,
    ServerGreetingError,
    ServerGreetingGreeting,
    ClientGreeting,
    ClientGreetingStartTls,
    ClientGreetingFull,
    TlsConnectInit,
    TlsConnect,
    ClientGreetingAfterTls,
    InitialResponse,
    FinalResponse,
    AuthOk,
    AuthError,

    ServerGreetingSent,
    Error,
    Ok,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

  void failed(
      const std::optional<classic_protocol::message::server::Error> &err) {
    failed_ = err;
  }

  std::optional<classic_protocol::message::server::Error> failed() const {
    return failed_;
  }

 private:
  stdx::expected<Result, std::error_code> server_greeting();
  stdx::expected<Result, std::error_code> server_greeting_greeting();
  stdx::expected<Result, std::error_code> server_greeting_error();
  stdx::expected<Result, std::error_code> client_greeting();
  stdx::expected<Result, std::error_code> client_greeting_start_tls();
  stdx::expected<Result, std::error_code> client_greeting_full();
  stdx::expected<Result, std::error_code> tls_connect_init();
  stdx::expected<Result, std::error_code> tls_connect();
  stdx::expected<Result, std::error_code> client_greeting_after_tls();
  stdx::expected<Result, std::error_code> initial_response();
  stdx::expected<Result, std::error_code> final_response();
  stdx::expected<Result, std::error_code> auth_error();
  stdx::expected<Result, std::error_code> auth_ok();
  stdx::expected<Result, std::error_code> error();

  void client_greeting_server_adjust_caps(ClassicProtocolState &src_protocol,
                                          ClassicProtocolState &dst_protocol);

  bool in_handshake_;

  Stage stage_{Stage::ServerGreeting};

  std::optional<classic_protocol::message::server::Error> failed_;

  std::function<void(const classic_protocol::message::server::Error &err)>
      on_error_;

  TraceEvent *parent_event_{nullptr};
  TraceEvent *trace_event_greeting_{nullptr};
  TraceEvent *trace_event_server_greeting_{nullptr};
  TraceEvent *trace_event_client_greeting_{nullptr};
  TraceEvent *trace_event_tls_connect_{nullptr};
};

/**
 * classic protocol handshake between router<->server and client<->router.
 *
 * a server::greeting processor which fetches a server::greeting
 * to send it to the client.
 *
 *     c->r   : accept()
 *        r->s: connect()
 *        r<-s: server::greeting
 *     c<-r   : ...
 *
 */
class ServerFirstConnector : public ForwardingProcessor {
 public:
  using ForwardingProcessor::ForwardingProcessor;

  /**
   * stages of the handshake flow.
   */
  enum class Stage {
    Connect,
    ServerGreeting,
    ServerGreeted,

    Error,
    Ok,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

 private:
  stdx::expected<Result, std::error_code> connect();
  stdx::expected<Result, std::error_code> server_greeting();
  stdx::expected<Result, std::error_code> server_greeted();

  Stage stage_{Stage::Connect};

  // start timepoint to calculate the connect-retry-timeout.
  std::chrono::steady_clock::time_point started_{
      std::chrono::steady_clock::now()};
};

/**
 * authenticates a server connection.
 *
 * Assumes the server
 *
 * 1. sent a server::greeting already
 * 2. expects to receive a client::greeting
 */
class ServerFirstAuthenticator : public ForwardingProcessor {
 public:
  ServerFirstAuthenticator(
      MysqlRoutingClassicConnectionBase *conn,
      std::function<void(const classic_protocol::message::server::Error &)>
          on_error)
      : ForwardingProcessor(conn), on_error_(std::move(on_error)) {}

  /**
   * stages of the handshake flow.
   */
  enum class Stage {
    ClientGreeting,
    ClientGreetingStartTls,
    ClientGreetingFull,
    TlsForwardInit,
    TlsForward,
    TlsConnectInit,
    TlsConnect,
    ClientGreetingAfterTls,
    InitialResponse,
    FinalResponse,
    AuthOk,
    AuthError,
    FetchUserAttrs,
    FetchUserAttrsDone,

    Error,
    Ok,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

  void failed(
      const std::optional<classic_protocol::message::server::Error> &err) {
    failed_ = err;
  }

  std::optional<classic_protocol::message::server::Error> failed() const {
    return failed_;
  }

 private:
  stdx::expected<Result, std::error_code> client_greeting();
  stdx::expected<Result, std::error_code> client_greeting_start_tls();
  stdx::expected<Result, std::error_code> client_greeting_full();
  stdx::expected<Result, std::error_code> tls_forward_init();
  stdx::expected<Result, std::error_code> tls_forward();
  stdx::expected<Result, std::error_code> tls_connect_init();
  stdx::expected<Result, std::error_code> tls_connect();
  stdx::expected<Result, std::error_code> client_greeting_after_tls();
  stdx::expected<Result, std::error_code> initial_response();
  stdx::expected<Result, std::error_code> final_response();
  stdx::expected<Result, std::error_code> auth_error();
  stdx::expected<Result, std::error_code> auth_ok();
  stdx::expected<Result, std::error_code> fetch_user_attrs();
  stdx::expected<Result, std::error_code> fetch_user_attrs_done();

  void client_greeting_server_adjust_caps(ClassicProtocolState &rc_protocol,
                                          ClassicProtocolState &st_protocol);

  size_t client_last_recv_buf_size_{};
  size_t client_last_send_buf_size_{};
  size_t server_last_recv_buf_size_{};
  size_t server_last_send_buf_size_{};

  std::optional<classic_protocol::message::server::Error> failed_;

  RouterRequireFetcher::Result required_connection_attributes_fetcher_result_;

  Stage stage_{Stage::ClientGreeting};

  std::function<void(const classic_protocol::message::server::Error &err)>
      on_error_;
};

#endif
