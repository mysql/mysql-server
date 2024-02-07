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

#include "classic_set_option_forwarder.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "hexify.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql-server error-codes
#include "mysqlrouter/classic_protocol_codec_error.h"

stdx::expected<Processor::Result, std::error_code>
SetOptionForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
    case Stage::Forward:
      return forward();
    case Stage::ForwardDone:
      return forward_done();
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
SetOptionForwarder::command() {
  auto &src_conn = connection()->client_conn();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::SetOption>(src_conn);
  if (!msg_res) {
    // all codec-errors should result in a Malformed Packet error..
    if (msg_res.error().category() !=
        make_error_code(classic_protocol::codec_errc::not_enough_input)
            .category()) {
      return recv_client_failed(msg_res.error());
    }

    discard_current_msg(src_conn);

    auto send_msg =
        ClassicFrame::send_msg<classic_protocol::message::server::Error>(
            src_conn,
            {ER_MALFORMED_PACKET, "Malformed communication packet", "HY000"});
    if (!send_msg) send_client_failed(send_msg.error());

    stage(Stage::Done);

    return Result::SendToClient;
  }

  option_value_ = msg_res->option();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("set_option::command: " +
                                   std::to_string(option_value_)));
  }

  // reset the warnings from the previous statements.
  connection()->execution_context().diagnostics_area().warnings().clear();

  trace_event_command_ = trace_command(prefix());

  trace_event_connect_and_forward_command_ =
      trace_connect_and_forward_command(trace_event_command_);

  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    trace_event_connect_ =
        trace_connect(trace_event_connect_and_forward_command_);

    stage(Stage::Connect);
  } else {
    trace_event_forward_command_ =
        trace_forward_command(trace_event_connect_and_forward_command_);

    stage(Stage::Forward);
  }
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
SetOptionForwarder::connect() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("set_option::connect"));
  }

  stage(Stage::Connected);
  return mysql_reconnect_start(trace_event_connect_);
}

stdx::expected<Processor::Result, std::error_code>
SetOptionForwarder::connected() {
  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    auto &src_conn = connection()->client_conn();

    // take the client::command from the connection.
    auto recv_res = ClassicFrame::ensure_has_full_frame(src_conn);
    if (!recv_res) return recv_client_failed(recv_res.error());

    discard_current_msg(src_conn);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("set_option::connect::error"));
    }

    trace_span_end(trace_event_connect_);
    trace_span_end(trace_event_connect_and_forward_command_);
    trace_command_end(trace_event_command_);

    stage(Stage::Done);
    return reconnect_send_error_msg(src_conn);
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("set_option::connected"));
  }

  trace_span_end(trace_event_connect_);
  trace_forward_command(trace_event_connect_and_forward_command_);

  stage(Stage::Forward);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
SetOptionForwarder::forward() {
  stage(Stage::ForwardDone);
  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
SetOptionForwarder::forward_done() {
  stage(Stage::Response);

  trace_span_end(trace_event_forward_command_);
  trace_span_end(trace_event_connect_and_forward_command_);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
SetOptionForwarder::response() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) return recv_server_failed(read_res.error());

  const uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    Eof = ClassicFrame::cmd_byte<classic_protocol::message::server::Eof>(),
    Error = ClassicFrame::cmd_byte<classic_protocol::message::server::Error>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Eof:
      stage(Stage::Ok);
      return Result::Again;
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("set_option::response"));
  }

  return stdx::unexpected(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code> SetOptionForwarder::ok() {
  auto &src_conn = connection()->server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->client_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Eof>(
          src_conn);
  if (!msg_res) {
    auto ec = msg_res.error();
    if (ec.category() ==
        make_error_code(classic_protocol::codec_errc::invalid_input)
            .category()) {
      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage(
            "set_option::eof failed\n" +
            mysql_harness::hexify(src_channel.recv_plain_view())));
      }
    }

    return recv_server_failed(msg_res.error());
  }

  auto msg = *msg_res;

  auto cap = classic_protocol::capabilities::pos::multi_statements;

  switch (option_value_) {
    case MYSQL_OPTION_MULTI_STATEMENTS_OFF:

      src_protocol.client_capabilities(
          src_protocol.client_capabilities().reset(cap));

      dst_protocol.client_capabilities(
          dst_protocol.client_capabilities().reset(cap));
      break;
    case MYSQL_OPTION_MULTI_STATEMENTS_ON:

      src_protocol.client_capabilities(
          src_protocol.client_capabilities().set(cap));

      dst_protocol.client_capabilities(
          dst_protocol.client_capabilities().set(cap));
      break;
  }

  dst_protocol.status_flags(msg.status_flags());

  trace_command_end(trace_event_command_);

  if (msg.warning_count() > 0) connection()->diagnostic_area_changed(true);

  stage(Stage::Done);

  if (!connection()->events().empty()) {
    msg.warning_count(msg.warning_count() + 1);
  }

  if (!connection()->events().empty() ||
      !message_can_be_forwarded_as_is(src_protocol, dst_protocol, msg)) {
    auto send_res = ClassicFrame::send_msg(dst_conn, msg);
    if (!send_res) return stdx::unexpected(send_res.error());

    discard_current_msg(src_conn);

    return Result::SendToClient;
  }

  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code> SetOptionForwarder::error() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("set_option::error"));
  }

  trace_command_end(trace_event_command_, TraceEvent::StatusCode::kError);

  connection()->diagnostic_area_changed(true);

  stage(Stage::Done);

  return forward_server_to_client();
}
