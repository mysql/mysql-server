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

#include "classic_quit_forwarder.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqlrouter/connection_pool.h"
#include "mysqlrouter/connection_pool_component.h"

IMPORT_LOG_FUNCTIONS()

/**
 * forward the quit message flow.
 *
 * Expected overall flow:
 *
 * @code
 * c->r: COM_QUIT
 * alt can not be pooled
 *    r->s: COM_QUIT
 * else
 *    r->s: (add to pool)
 * end
 * c<-r: (close)
 * @endcode
 *
 * It is no error, if there is no server-connection.
 */
stdx::expected<Processor::Result, std::error_code> QuitForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::ServerTlsShutdownFirst:
      return server_tls_shutdown_first();
    case Stage::ServerShutdownSend:
      return server_shutdown_send();
    case Stage::ServerTlsShutdownResponse:
      return server_tls_shutdown_response();
    case Stage::ClientTlsShutdownFirst:
      return client_tls_shutdown_first();
    case Stage::ClientShutdownSend:
      return client_shutdown_send();
    case Stage::ClientTlsShutdownResponse:
      return client_tls_shutdown_response();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> QuitForwarder::command() {
  auto &src_conn = connection()->client_conn();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::client::Quit>(
          src_conn);
  if (!msg_res) return recv_client_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("quit::command"));
  }

  if (!connection()->server_conn().is_open()) {
    discard_current_msg(src_conn);

    stage(Stage::ServerTlsShutdownFirst);

    return Result::Again;
  }

  // move the connection to the pool.
  //
  // the pool will either close it or keep it alive.
  auto pooled_res = pool_server_connection();
  if (!pooled_res) return recv_client_failed(pooled_res.error());

  const bool connection_was_pooled = *pooled_res;
  if (connection_was_pooled) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("quit::pooled"));
    }

    // the connect was pooled, discard the Quit message.
    discard_current_msg(src_conn);

    stage(Stage::ServerTlsShutdownFirst);

    return Result::Again;
  }

  stage(Stage::ServerTlsShutdownFirst);

  // Merge the COM_QUIT and the TLS shutdown into one write() by not flushing
  // to the socket yet.
  const bool delay_com_quit_for_tls_shutdown{
      connection()->server_conn().channel().ssl() != nullptr};

  return forward_client_to_server(delay_com_quit_for_tls_shutdown);
}

stdx::expected<Processor::Result, std::error_code>
QuitForwarder::server_tls_shutdown_first() {
  auto &dst_conn = connection()->server_conn();
  auto &dst_channel = dst_conn.channel();

  if (dst_channel.ssl() == nullptr || !dst_conn.is_open()) {
    // no TLS or no socket, continue with the client.
    stage(Stage::ServerShutdownSend);
    return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("tls_shutdown::server::first"));
  }

  // the COM_QUIT is not encrypted yet, flush it to the send-buffer.
  dst_channel.flush_to_send_buf();

  const auto res = dst_channel.tls_shutdown();
  if (!res) {
    const auto ec = res.error();

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("tls_shutdown::server::err::" +
                                     res.error().message()));
    }

    if (!dst_channel.send_buffer().empty()) {
      assert(ec == TlsErrc::kWantRead);

      if (ec != TlsErrc::kWantRead) {
        stage(Stage::Done);
      }
      return Result::SendToServer;
    }

    if (ec == TlsErrc::kWantRead) return Result::RecvFromServer;

    log_fatal_error_code("tls_shutdown::server failed", ec);

    return recv_server_failed(ec);
  }

  // shutdown shouldn't be finished yet.
  assert(!*res);

  // send the message and shutdown the send-part of the socket.
  stage(Stage::ServerShutdownSend);

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code>
QuitForwarder::server_shutdown_send() {
  auto &server_conn = connection()->server_conn();

  if (!server_conn.is_open()) {
    // no socket, continue with the client.
    stage(Stage::ClientTlsShutdownFirst);
    return Result::Again;
  }

  auto shutdown_res = server_conn.shutdown(net::socket_base::shutdown_send);
  if (!shutdown_res) {
    auto ec = shutdown_res.error();

    // if the server has already closed the connection, shutdown() will fail and
    // the socket can be closed right away.
    if (ec == make_error_condition(std::errc::not_connected)) {
      // server already closed the socket.
      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event()
                     .stage("close::server")
                     .direction(Tracer::Event::Direction::kServerClose));
      }

      (void)server_conn.close();
    } else {
      log_debug("Quit::server_shutdown_send: shutdown() failed: %s",
                ec.message().c_str());
    }
  } else {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("server::shutdown::send"));
    }
  }

  stage(Stage::ClientTlsShutdownFirst);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
QuitForwarder::client_tls_shutdown_first() {
  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();

  if (src_channel.ssl() == nullptr || !src_conn.is_open()) {
    // client isn't TLS or the socket is already closed.
    stage(Stage::ClientShutdownSend);

    return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("tls_shutdown::client::first"));
  }

  const auto res = src_channel.tls_shutdown();
  if (!res) {
    const auto ec = res.error();

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("tls_shutdown::client::err::" +
                                     res.error().message()));
    }

    if (!src_channel.send_buffer().empty()) {
      if (ec != TlsErrc::kWantRead) {
        stage(Stage::Done);
      }
      return Result::SendToClient;
    }

    if (ec == TlsErrc::kWantRead) return Result::RecvFromClient;

    log_fatal_error_code("tls_shutdown::client failed", ec);

    return recv_client_failed(ec);
  }

  // shutdown started, not finished yet.
  assert(!*res);

  // send TLS shutdown Alert to client and wait for the server's shutdown
  // response.
  stage(Stage::ClientShutdownSend);

  return Result::SendToClient;
}

stdx::expected<Processor::Result, std::error_code>
QuitForwarder::client_shutdown_send() {
  auto &client_conn = connection()->client_conn();

  if (!client_conn.is_open()) {
    // no socket, continue with the server.
    stage(Stage::ServerTlsShutdownResponse);
    return Result::Again;
  }

  auto shutdown_res = client_conn.shutdown(net::socket_base::shutdown_send);
  if (!shutdown_res) {
    auto ec = shutdown_res.error();

    if (ec == make_error_condition(std::errc::not_connected)) {
      // client already closed the socket.
      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event()
                     .stage("close::client")
                     .direction(Tracer::Event::Direction::kClientClose));
      }

      (void)client_conn.close();
    }
  } else {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("client::shutdown::send"));
    }
  }

  // wait for the server side.
  stage(Stage::ServerTlsShutdownResponse);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
QuitForwarder::server_tls_shutdown_response() {
  auto &server_conn = connection()->server_conn();
  // SSL_shutdown() could be called a 2nd time, but the server won't send a TLS
  // alert anyway. Close the socket right away.
  if (server_conn.is_open()) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event()
                   .stage("close::server")
                   .direction(Tracer::Event::Direction::kServerClose));
    }
    (void)server_conn.close();
  }

  stage(Stage::ClientTlsShutdownResponse);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
QuitForwarder::client_tls_shutdown_response() {
  auto &client_conn = connection()->client_conn();

  if (client_conn.is_open()) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event()
                   .stage("close::client")
                   .direction(Tracer::Event::Direction::kClientClose));
    }
    (void)client_conn.close();
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("quit::ok"));
  }

  stage(Stage::Done);
  return Result::Again;
}
