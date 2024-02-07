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

#include "classic_stmt_reset_forwarder.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql-server error-codes

stdx::expected<Processor::Result, std::error_code>
StmtResetForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Response:
      return response();
    case Stage::Ok:
      return ok();
    case Stage::Error:
      return error();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
StmtResetForwarder::command() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_reset::command"));
  }

  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    auto &src_conn = connection()->client_conn();

    // take the client::command from the connection.
    auto recv_res = ClassicFrame::ensure_has_full_frame(src_conn);
    if (!recv_res) return recv_client_failed(recv_res.error());

    discard_current_msg(src_conn);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("stmt_reset::error"));
    }

    auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Error>(
        src_conn, {1243, "Unknown prepared statement handler", "HY000"});
    if (!send_res) return send_client_failed(send_res.error());

    stage(Stage::Done);
    return Result::SendToClient;
  } else {
    stage(Stage::Response);

    return forward_client_to_server();
  }
}

stdx::expected<Processor::Result, std::error_code>
StmtResetForwarder::response() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) return recv_server_failed(read_res.error());

  const uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    Ok = ClassicFrame::cmd_byte<classic_protocol::message::server::Ok>(),
    Error = ClassicFrame::cmd_byte<classic_protocol::message::server::Error>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Ok:
      stage(Stage::Ok);
      return Result::Again;
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_reset::response"));
  }

  return stdx::unexpected(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code> StmtResetForwarder::ok() {
  auto &src_conn = connection()->server_conn();

  auto &dst_conn = connection()->client_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Ok>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto msg = *msg_res;

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_reset::ok"));
  }

  dst_protocol.status_flags(msg.status_flags());

  stage(Stage::Done);

  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code> StmtResetForwarder::error() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_reset::error"));
  }

  stage(Stage::Done);

  return forward_server_to_client();
}
