/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#ifndef ROUTING_CLASSIC_GREETING_INCLUDED
#define ROUTING_CLASSIC_GREETING_INCLUDED

#include "processor.h"

/**
 * classic protocol handshake between client<->router (and router<->server).
 *
 *
 */
class ClientGreetor : public Processor {
 public:
  using Processor::Processor;

  /**
   * stages of the handshake flow.
   */
  enum class Stage {
    Init,
    ServerGreeting,
    ServerFirstGreeting,
    ClientGreeting,
    TlsAcceptInit,
    TlsAccept,
    ClientGreetingAfterTls,
    RequestPlaintextPassword,
    PlaintextPassword,
    Accepted,
    Authenticated,

    Error,
    Ok,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

 private:
  stdx::expected<Result, std::error_code> init();
  stdx::expected<Result, std::error_code> server_greeting();
  stdx::expected<Result, std::error_code> server_first_greeting();
  stdx::expected<Result, std::error_code> client_greeting();
  stdx::expected<Result, std::error_code> tls_accept_init();
  stdx::expected<Result, std::error_code> tls_accept();
  stdx::expected<Result, std::error_code> client_greeting_after_tls();
  stdx::expected<Result, std::error_code> request_plaintext_password();
  stdx::expected<Result, std::error_code> plaintext_password();
  stdx::expected<Result, std::error_code> accepted();
  stdx::expected<Result, std::error_code> authenticated();
  stdx::expected<Result, std::error_code> error();

  Stage stage_{Stage::Init};
};

/**
 * classic protocol handshake between client<->router and router<->server.
 */
class ServerGreetor : public Processor {
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
   * If `in_handshake` is true, a auth-method switch request by the
   * server can be sent to the client.
   *
   * The ServerGreetor expects it can send
   *
   * - server::Error
   * - server::AuthMethodSwitch and server::Ok (if in_handshake==true)
   *
   * to the client connection.
   *
   * @param conn the connection the greeting will be transferred on.
   * @param in_handshake true if the greeting is part of the initial
   * handshake.
   */
  ServerGreetor(MysqlRoutingClassicConnection *conn, bool in_handshake)
      : Processor(conn), in_handshake_{in_handshake} {}

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

  void client_greeting_server_adjust_caps(ClassicProtocolState *src_protocol,
                                          ClassicProtocolState *dst_protocol);

  bool in_handshake_;

  Stage stage_{Stage::ServerGreeting};
};

/**
 * classic protocol handshake between router<->server and client<->router.
 */
class ServerFirstConnector : public Processor {
 public:
  /**
   * construct a server::greeting processor fetches a server::greeting
   * to send it to the client.
   *
   *     c->r   : accept()
   *        r->s: connect()
   *        r<-s: server::greeting
   *     c<-r   : ...
   *
   * @param conn the connection the greeting will be transferred on.
   */
  ServerFirstConnector(MysqlRoutingClassicConnection *conn) : Processor(conn) {}

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
};

/**
 * authenticates a server connection.
 *
 * Assumes the server
 *
 * 1. sent a server::greeting already
 * 2. expects to receive a client::greeting
 */
class ServerFirstAuthenticator : public Processor {
 public:
  ServerFirstAuthenticator(MysqlRoutingClassicConnection *conn)
      : Processor(conn) {}

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

    Error,
    Ok,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

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

  void client_greeting_server_adjust_caps(ClassicProtocolState *src_protocol,
                                          ClassicProtocolState *dst_protocol);

  size_t client_last_recv_buf_size_{};
  size_t client_last_send_buf_size_{};
  size_t server_last_recv_buf_size_{};
  size_t server_last_send_buf_size_{};

  Stage stage_{Stage::ClientGreeting};
};

#endif
