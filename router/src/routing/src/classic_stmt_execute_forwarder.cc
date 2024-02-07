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

#include "classic_stmt_execute_forwarder.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "hexify.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql-server error-codes
#include "mysqlrouter/classic_protocol_codec_error.h"

stdx::expected<Processor::Result, std::error_code>
StmtExecuteForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Forward:
      return forward();
    case Stage::ForwardDone:
      return forward_done();
    case Stage::Response:
      return response();
    case Stage::ColumnCount:
      return column_count();
    case Stage::Column:
      return column();
    case Stage::EndOfColumns:
      return end_of_columns();
    case Stage::Row:
      return row();
    case Stage::EndOfRows:
      return end_of_rows();
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
StmtExecuteForwarder::command() {
  if (auto &tr = tracer()) {
    auto &src_conn = connection()->client_conn();
    auto &src_channel = src_conn.channel();

    auto msg_res = ClassicFrame::recv_msg<
        classic_protocol::borrowed::message::client::StmtExecute>(src_conn);
    if (!msg_res) {
      auto ec = msg_res.error();

      // parse errors are invalid input.
      if (ec.category() ==
          make_error_code(classic_protocol::codec_errc::invalid_input)
              .category()) {
        auto send_res =
            ClassicFrame::send_msg<classic_protocol::message::server::Error>(
                src_conn, {ER_MALFORMED_PACKET, "Malformed packet", "HY000"});
        if (!send_res) return send_client_failed(send_res.error());

        const auto &recv_buf = src_channel.recv_plain_view();

        tr.trace(Tracer::Event().stage("stmt_execute::command:\n" +
                                       mysql_harness::hexify(recv_buf)));

        discard_current_msg(src_conn);

        stage(Stage::Done);
        return Result::SendToClient;
      }

      return recv_client_failed(msg_res.error());
    }

    const auto &recv_buf = src_channel.recv_plain_view();

    tr.trace(Tracer::Event().stage(
        "stmt_execute::command:\nstmt-id: " +              //
        std::to_string(msg_res->statement_id()) + "\n" +   //
        "flags: " + msg_res->flags().to_string() + "\n" +  //
        "new-params-bound: " + std::to_string(msg_res->new_params_bound()) +
        "\n" +  //
        "types::size(): " + std::to_string(msg_res->types().size()) + "\n" +
        "values::size(): " + std::to_string(msg_res->values().size()) + "\n" +
        mysql_harness::hexify(recv_buf)));
  }

  connection()->execution_context().diagnostics_area().warnings().clear();
  connection()->events().clear();

  trace_event_command_ = trace_command(prefix());

  trace_event_connect_and_forward_command_ =
      trace_connect_and_forward_command(trace_event_command_);

  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    auto &src_conn = connection()->client_conn();

    // take the client::command from the connection.
    auto frame_res = ClassicFrame::ensure_has_full_frame(src_conn);
    if (!frame_res) return recv_client_failed(frame_res.error());

    // discard the recv'ed message as there is ...
    //
    // - no server connection to send it to
    // - and therefore no prepared statement that could be executed on the
    //   server.
    discard_current_msg(src_conn);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("stmt_execute::error"));
    }

    auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Error>(
        src_conn,
        {ER_UNKNOWN_STMT_HANDLER, "Unknown prepared statement id", "HY000"});
    if (!send_res) return send_client_failed(send_res.error());

    trace_span_end(trace_event_connect_and_forward_command_);
    trace_span_end(trace_event_command_);

    stage(Stage::Done);
    return Result::SendToClient;
  }

  trace_event_forward_command_ =
      trace_forward_command(trace_event_connect_and_forward_command_);

  stage(Stage::Forward);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StmtExecuteForwarder::forward() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->server_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto client_caps = src_protocol.shared_capabilities();
  auto server_caps = dst_protocol.shared_capabilities();

  if (client_caps.test(classic_protocol::capabilities::pos::query_attributes) ==
      server_caps.test(classic_protocol::capabilities::pos::query_attributes)) {
    // if caps are the same, forward the message as is
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("stmt_execute::forward"));
    }

    stage(Stage::ForwardDone);

    return forward_client_to_server();
  }

  // ... otherwise: recode the message.

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_execute::forward::recode"));
  }

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::StmtExecute>(src_conn);
  if (!msg_res) {
    if (msg_res.error().category() !=
        make_error_code(classic_protocol::codec_errc::not_enough_input)
            .category()) {
      return recv_client_failed(msg_res.error());
    }

    discard_current_msg(src_conn);

    classic_protocol::borrowed::message::server::Error err_msg{
        ER_MALFORMED_PACKET, "Malformed communication packet", "HY000"};

    if (msg_res.error() ==
        classic_protocol::codec_errc::statement_id_not_found) {
      err_msg = {ER_UNKNOWN_STMT_HANDLER, "Unknown prepared statement id",
                 "HY000"};
    }

    auto send_msg = ClassicFrame::send_msg(src_conn, err_msg);
    if (!send_msg) send_client_failed(send_msg.error());

    trace_span_end(trace_event_forward_command_);
    trace_span_end(trace_event_connect_and_forward_command_);
    trace_command_end(trace_event_command_, TraceEvent::StatusCode::kError);

    stage(Stage::Done);

    return Result::SendToClient;
  }

  // if the msg contains query attributes, but the server doesn't support
  // attributes, ignore them.
  //
  // libmysqlclient behaves the same, if mysql_bind_param() is called with a
  // server which doesn't support query-attributes.

  auto send_res = ClassicFrame::send_msg(dst_conn, *msg_res);
  if (!send_res) return send_server_failed(send_res.error());

  discard_current_msg(src_conn);

  // reset the "param-already-sent" flag for the next time the statement is
  // executed. It will be set by the stmt_param_append
  auto stmt_it =
      src_protocol.prepared_statements().find(msg_res->statement_id());
  if (stmt_it != src_protocol.prepared_statements().end()) {
    for (auto &param : stmt_it->second.parameters) {
      param.param_already_sent = false;
    }
  }

  stage(Stage::ForwardDone);
  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code>
StmtExecuteForwarder::forward_done() {
  stage(Stage::Response);

  trace_span_end(trace_event_forward_command_);
  trace_span_end(trace_event_connect_and_forward_command_);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StmtExecuteForwarder::response() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) {
    return recv_server_failed_and_check_client_socket(read_res.error());
  }

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

  stage(Stage::ColumnCount);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StmtExecuteForwarder::column_count() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto column_count_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::ColumnCount>(src_conn);
  if (!column_count_res) return recv_server_failed(column_count_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_execute::column_count"));
  }

  src_protocol.columns_left = column_count_res->count();

  stage(Stage::Column);

  return forward_server_to_client(true);
}

stdx::expected<Processor::Result, std::error_code>
StmtExecuteForwarder::column() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_execute::column"));
  }

  auto &src_protocol = connection()->server_conn().protocol();

  if (--src_protocol.columns_left == 0) {
    stage(Stage::EndOfColumns);
  }

  return forward_server_to_client(true);
}

stdx::expected<Processor::Result, std::error_code>
StmtExecuteForwarder::end_of_columns() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_execute::end_of_columns"));
  }

  stage(Stage::Row);

  return skip_or_inject_end_of_columns(true);
}

stdx::expected<Processor::Result, std::error_code> StmtExecuteForwarder::row() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) return recv_server_failed(read_res.error());

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    Row = 0x00,
    Eof = ClassicFrame::cmd_byte<classic_protocol::message::server::Eof>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Eof:
      stage(Stage::EndOfRows);
      return Result::Again;
    case Msg::Row:
      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage("stmt_execute::row"));
      }
      return forward_server_to_client(true);
  }

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code>
StmtExecuteForwarder::end_of_rows() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->client_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Eof>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_execute::end_of_rows"));
  }

  auto msg = *msg_res;

  if (msg.status_flags().test(
          classic_protocol::status::pos::more_results_exist)) {
    stage(Stage::Response);
    return forward_server_to_client();
  }

  if (msg.warning_count() > 0) connection()->diagnostic_area_changed(true);

  trace_command_end(trace_event_command_);

  dst_protocol.status_flags(msg.status_flags());

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

stdx::expected<Processor::Result, std::error_code> StmtExecuteForwarder::ok() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->client_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Ok>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto msg = *msg_res;

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_execute::ok"));
  }

  dst_protocol.status_flags(msg.status_flags());

  if (msg.warning_count() > 0) connection()->diagnostic_area_changed(true);

  if (auto *ev = trace_span(trace_event_command_, "mysql/response")) {
    ClassicFrame::trace_set_attributes(ev, src_protocol, msg);

    trace_span_end(ev);
  }

  trace_command_end(trace_event_command_);

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

stdx::expected<Processor::Result, std::error_code>
StmtExecuteForwarder::error() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::Error>(src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto msg = *msg_res;

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_execute::error"));
  }

  connection()->diagnostic_area_changed(true);

  if (auto *ev = trace_span(trace_event_command_, "mysql/response")) {
    ClassicFrame::trace_set_attributes(ev, src_protocol, msg);

    trace_span_end(ev);
  }

  trace_command_end(trace_event_command_);

  stage(Stage::Done);

  return forward_server_to_client();
}
