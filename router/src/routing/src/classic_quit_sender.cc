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

#include "classic_quit_sender.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqlrouter/connection_pool.h"
#include "mysqlrouter/connection_pool_component.h"
#include "tracer.h"

IMPORT_LOG_FUNCTIONS()

// sender

stdx::expected<Processor::Result, std::error_code> QuitSender::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::TlsShutdown:
      return tls_shutdown();
    case Stage::CloseSocket:
      return close_socket();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> QuitSender::command() {
  auto &dst_conn = connection()->server_conn();
  auto &dst_channel = dst_conn.channel();
  auto &dst_protocol = dst_conn.protocol();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("quit::command"));
  }

  dst_protocol.seq_id(0xff);

  auto msg_res =
      ClassicFrame::send_msg<classic_protocol::borrowed::message::client::Quit>(
          dst_channel, dst_protocol, {});
  if (!msg_res) return send_server_failed(msg_res.error());

  // the COM_QUIT is not encrypted yet, flush it to the send-buffer.
  dst_channel.flush_to_send_buf();

  if (dst_channel.ssl() == nullptr) {
    // no TLS, close the socket.
    stage(Stage::CloseSocket);
    return Result::SendToServer;
  }

  stage(Stage::TlsShutdown);
  return Result::Again;
}

// called twice
stdx::expected<Processor::Result, std::error_code> QuitSender::tls_shutdown() {
  auto &dst_conn = connection()->server_conn();
  auto &dst_channel = dst_conn.channel();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event()
                 .stage("quit::tls_shutdown")
                 .direction(Tracer::Event::Direction::kServerClose));
  }

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
        stage(Stage::CloseSocket);
      }
      return Result::RecvFromServer;
    }

    if (ec == TlsErrc::kWantRead) return Result::RecvFromServer;

    log_fatal_error_code("tls_shutdown::server failed", ec);

    return recv_server_failed(ec);
  }

  stage(Stage::CloseSocket);
  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code> QuitSender::close_socket() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event()
                 .stage("quit::close")
                 .direction(Tracer::Event::Direction::kServerClose));
  }

  (void)connection()->server_conn().close();

  stage(Stage::Done);
  return Result::Again;
}
