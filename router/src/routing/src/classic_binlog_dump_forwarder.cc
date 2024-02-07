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

#include "classic_binlog_dump_forwarder.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqlrouter/client_error_code.h"

IMPORT_LOG_FUNCTIONS()

stdx::expected<Processor::Result, std::error_code>
BinlogDumpForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::ForbidCommand:
      return forbid_command();
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
    case Stage::Response:
      return response();
    case Stage::Event:
      return event();
    case Stage::EndOfStream:
      return end_of_stream();
    case Stage::Error:
      return error();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
BinlogDumpForwarder::command() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("binlog_dump::command"));
  }

  if (connection()->context().access_mode() == routing::AccessMode::kAuto) {
    stage(Stage::ForbidCommand);

    return Result::Again;
  }

  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    stage(Stage::Connect);
    return Result::Again;
  } else {
    stage(Stage::Response);

    return forward_client_to_server();
  }
}

stdx::expected<Processor::Result, std::error_code>
BinlogDumpForwarder::forbid_command() {
  auto &src_conn = connection()->client_conn();

  // take the client::command from the connection.
  auto recv_res = ClassicFrame::ensure_has_full_frame(src_conn);
  if (!recv_res) return recv_client_failed(recv_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("binlog_dump::command::forbid"));
  }

  discard_current_msg(src_conn);

  stage(Stage::Done);

  auto send_res = ClassicFrame::send_msg<
      classic_protocol::borrowed::message::server::Error>(
      src_conn,
      {ER_ROUTER_NOT_ALLOWED_WITH_CONNECTION_SHARING,
       "binlog dump is not allowed with access_mode = 'auto'", "42000"});
  if (!send_res) return stdx::unexpected(send_res.error());

  return Result::SendToClient;
}

stdx::expected<Processor::Result, std::error_code>
BinlogDumpForwarder::connect() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("binlog_dump::connect"));
  }

  stage(Stage::Connected);
  return mysql_reconnect_start(nullptr);
}

stdx::expected<Processor::Result, std::error_code>
BinlogDumpForwarder::connected() {
  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    auto &src_conn = connection()->client_conn();

    // take the client::command from the connection.
    auto recv_res = ClassicFrame::ensure_has_full_frame(src_conn);
    if (!recv_res) return recv_client_failed(recv_res.error());

    discard_current_msg(src_conn);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("binlog_dump::connect::error"));
    }

    stage(Stage::Done);
    return reconnect_send_error_msg(src_conn);
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("binlog_dump::connected"));
  }
  stage(Stage::Response);
  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
BinlogDumpForwarder::response() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) return recv_server_failed(read_res.error());

  const uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    Event = 0x00,
    EndOfStream =
        ClassicFrame::cmd_byte<classic_protocol::message::server::Eof>(),
    Error = ClassicFrame::cmd_byte<classic_protocol::message::server::Error>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Event:
      stage(Stage::Event);
      return Result::Again;
    case Msg::EndOfStream:
      stage(Stage::EndOfStream);
      return Result::Again;
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
  }

  log_debug("binlog_dump::response: unexpected msg-type '%02x'", msg_type);

  return stdx::unexpected(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code>
BinlogDumpForwarder::event() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("binlog_dump::event"));
  }

  stage(Stage::Response);

  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code>
BinlogDumpForwarder::end_of_stream() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("binlog_dump::end_of_stream"));
  }

  // avoid reuse of the connection as the server will close it.
  connection()->some_state_changed(true);

  stage(Stage::Done);

  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code>
BinlogDumpForwarder::error() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("binlog_dump::error"));
  }

  // avoid reuse of the connection as the server will close it.
  connection()->some_state_changed(true);

  stage(Stage::Done);

  return forward_server_to_client();
}
