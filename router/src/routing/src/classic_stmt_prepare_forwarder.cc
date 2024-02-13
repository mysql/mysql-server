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

#include "classic_stmt_prepare_forwarder.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "classic_quit_sender.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql errors
#include "mysqlrouter/client_error_code.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/routing.h"
#include "sql_parser_state.h"
#include "sql_splitting_allowed.h"

namespace {

stdx::expected<SplittingAllowedParser::Allowed, std::string> splitting_allowed(
    std::string_view stmt) {
  SqlParserState sql_parser_state;

  sql_parser_state.statement(stmt);

  auto lexer = sql_parser_state.lexer();

  return SplittingAllowedParser(lexer.begin(), lexer.end()).parse();
}

}  // namespace

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::ForbidCommand:
      return forbid_command();
    case Stage::PoolBackend:
      return pool_backend();
    case Stage::SwitchBackend:
      return switch_backend();
    case Stage::PrepareBackend:
      return prepare_backend();
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
    case Stage::Column:
      return column();
    case Stage::EndOfColumns:
      return end_of_columns();
    case Stage::Param:
      return param();
    case Stage::EndOfParams:
      return end_of_params();
    case Stage::Ok:
      return ok();
    case Stage::OkDone:
      return ok_done();
    case Stage::Error:
      return error();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::command() {
  auto &src_conn = connection()->client_conn();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::client::StmtPrepare>(
          src_conn);
  if (!msg_res) {
    // all codec-errors should result in a Malformed Packet error..
    if (msg_res.error().category() == classic_protocol::codec_category()) {
      discard_current_msg(src_conn);

      const auto send_msg = ClassicFrame::send_msg<
          classic_protocol::borrowed::message::server::Error>(
          src_conn,
          {ER_MALFORMED_PACKET, "Malformed communication packet", "HY000"});
      if (!send_msg) send_client_failed(send_msg.error());

      stage(Stage::Done);

      return Result::SendToClient;
    }

    return recv_client_failed(msg_res.error());
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_prepare::command"));
  }

  // reset the command-related state
  connection()->execution_context().diagnostics_area().warnings().clear();
  connection()->events().clear();

  trace_event_command_ = trace_command(prefix());

  trace_event_connect_and_forward_command_ =
      trace_connect_and_forward_command(trace_event_command_);

  stage(Stage::PrepareBackend);

  if (connection()->context().access_mode() == routing::AccessMode::kAuto) {
    const auto allowed_res = splitting_allowed(msg_res->statement());
    if (!allowed_res) {
      auto send_res = ClassicFrame::send_msg<
          classic_protocol::borrowed::message::server::Error>(
          src_conn, {ER_ROUTER_NOT_ALLOWED_WITH_CONNECTION_SHARING,
                     allowed_res.error(), "HY000"});
      if (!send_res) return send_client_failed(send_res.error());

      discard_current_msg(src_conn);

      stage(Stage::Done);
      return Result::SendToClient;
    }

    switch (*allowed_res) {
      case SplittingAllowedParser::Allowed::Always:
        break;
      case SplittingAllowedParser::Allowed::Never: {
        auto send_res = ClassicFrame::send_msg<
            classic_protocol::borrowed::message::server::Error>(
            src_conn,
            {ER_ROUTER_NOT_ALLOWED_WITH_CONNECTION_SHARING,
             "Statement not allowed if access_mode is 'auto'", "HY000"});
        if (!send_res) return send_client_failed(send_res.error());

        discard_current_msg(src_conn);

        stage(Stage::Done);
        return Result::SendToClient;
      }
      case SplittingAllowedParser::Allowed::OnlyReadOnly:
      case SplittingAllowedParser::Allowed::OnlyReadWrite:
      case SplittingAllowedParser::Allowed::InTransaction:
        if (!connection()->trx_state() ||
            connection()->trx_state()->trx_type() == '_') {
          auto send_res = ClassicFrame::send_msg<
              classic_protocol::borrowed::message::server::Error>(
              src_conn,
              {ER_ROUTER_NOT_ALLOWED_WITH_CONNECTION_SHARING,
               "Statement not allowed outside a transaction if access_mode "
               "is 'auto'",
               "HY000"});
          if (!send_res) return send_client_failed(send_res.error());

          discard_current_msg(src_conn);

          stage(Stage::Done);
          return Result::SendToClient;
        }
        break;
    }
    // prepare statements on the PRIMARY to ensure all statements can be
    // prepared even if the connection can't be shared anymore.
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("stmt_prepare::command::auto"));
    }

    if (!connection()->client_protocol().access_mode().has_value()) {
      // session's access-mode is 'auto'
      if (connection()->expected_server_mode() ==
          mysqlrouter::ServerMode::ReadWrite) {
        if (auto &tr = tracer()) {
          tr.trace(Tracer::Event().stage(
              "stmt_prepare::command::expect_read_write"));
        }

        // ok.
      } else if (connection()->connection_sharing_allowed()) {
        if (auto &tr = tracer()) {
          tr.trace(Tracer::Event().stage(
              "stmt_prepare::command::expect_read_only_and_sharing_allowed"));
        }

        // read-only, but can be switched.
        connection()->expected_server_mode(mysqlrouter::ServerMode::ReadWrite);

        if (connection()->server_conn().is_open()) {
          // as the connection will be switched, get rid of this connection.
          stage(Stage::PoolBackend);
        }
      } else {
        // read-only, but can't be switched.
        stage(Stage::ForbidCommand);
      }
    } else {
      auto session_access_mode = *connection()->client_protocol().access_mode();

      if (session_access_mode ==
              ClientSideClassicProtocolState::AccessMode::ReadOnly &&
          connection()->expected_server_mode() !=
              mysqlrouter::ServerMode::ReadOnly) {
        connection()->expected_server_mode(mysqlrouter::ServerMode::ReadOnly);

        if (connection()->server_conn().is_open()) {
          // as the connection will be switched, get rid of this connection.
          stage(Stage::PoolBackend);
        }
      } else if (session_access_mode ==
                     ClientSideClassicProtocolState::AccessMode::ReadWrite &&
                 connection()->expected_server_mode() !=
                     mysqlrouter::ServerMode::ReadWrite) {
        connection()->expected_server_mode(mysqlrouter::ServerMode::ReadWrite);

        if (connection()->server_conn().is_open()) {
          // as the connection will be switched, get rid of this connection.
          stage(Stage::PoolBackend);
        }
      }
    }
  }

  return Result::Again;
}

// drain the current command and return an error-msg.
stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::forbid_command() {
  auto &src_conn = connection()->client_conn();

  // take the client::command from the connection.
  auto recv_res = ClassicFrame::ensure_has_full_frame(src_conn);
  if (!recv_res) return recv_client_failed(recv_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_prepare::command::forbid"));
  }

  discard_current_msg(src_conn);

  stage(Stage::Done);

  auto send_res = ClassicFrame::send_msg<
      classic_protocol::borrowed::message::server::Error>(
      src_conn,
      {1064, "prepared statements not allowed with access_mode = 'auto'",
       "42000"});
  if (!send_res) return stdx::unexpected(send_res.error());

  return Result::SendToClient;
}

// pool the current server connection.
stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::pool_backend() {
  stage(Stage::SwitchBackend);

  auto pooled_res = pool_server_connection();
  if (!pooled_res) return send_server_failed(pooled_res.error());

  const auto pooled = *pooled_res;

  if (pooled) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("stmt_prepare::switch_backend::pooled"));
    }
  } else {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("stmt_prepare::switch_backend::full"));
    }

    // as the pool is full, close the server connection nicely.
    connection()->push_processor(std::make_unique<QuitSender>(connection()));
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::switch_backend() {
  // toggle the read-only state.
  // and connect to the backend again.
  stage(Stage::PrepareBackend);

  auto &server_conn = connection()->server_conn();

  // server socket is closed, reset its state.
  auto ssl_mode = server_conn.ssl_mode();
  server_conn =
      TlsSwitchableConnection{nullptr,   // connection
                              ssl_mode,  //
                              MysqlRoutingClassicConnectionBase::
                                  ServerSideConnection::protocol_state_type()};

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::prepare_backend() {
  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    stage(Stage::Connect);
  } else {
    trace_event_forward_command_ =
        trace_forward_command(trace_event_connect_and_forward_command_);
    stage(Stage::Forward);
  }
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::connect() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_prepare::connect"));
  }

  stage(Stage::Connected);
  return mysql_reconnect_start(trace_event_connect_and_forward_command_);
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::connected() {
  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    auto &src_conn = connection()->client_conn();

    // take the client::command from the connection.
    auto recv_res = ClassicFrame::ensure_has_full_frame(src_conn);
    if (!recv_res) return recv_client_failed(recv_res.error());

    discard_current_msg(src_conn);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("stmt_prepare::connect::error"));
    }

    trace_span_end(trace_event_connect_and_forward_command_);
    trace_command_end(trace_event_command_);

    stage(Stage::Done);
    return reconnect_send_error_msg(src_conn);
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_prepare::connected"));
  }

  trace_forward_command(trace_event_connect_and_forward_command_);

  stage(Stage::Forward);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::forward() {
  stage(Stage::ForwardDone);
  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::forward_done() {
  stage(Stage::Response);

  trace_span_end(trace_event_forward_command_);
  trace_span_end(trace_event_connect_and_forward_command_);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::response() {
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
    tr.trace(Tracer::Event().stage("stmt_prepare::response"));
  }

  return stdx::unexpected(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code> StmtPrepareForwarder::ok() {
  auto &src_conn = connection()->server_conn();
  auto &dst_conn = connection()->client_conn();

  const auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::StmtPrepareOk>(src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto msg = *msg_res;

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage(
        "stmt_prepare::ok: stmt-id: " +
        std::to_string(msg_res->statement_id()) +
        ", param-count: " + std::to_string(msg_res->param_count()) +
        ", column-count: " + std::to_string(msg_res->column_count())));
  }

  auto stmt_prep_ok = *msg_res;

  if (stmt_prep_ok.with_metadata() != 0) {
    columns_left_ = stmt_prep_ok.column_count();
    params_left_ = stmt_prep_ok.param_count();
  }

  prep_stmt_.parameters.reserve(stmt_prep_ok.param_count());
  stmt_id_ = stmt_prep_ok.statement_id();

  connection()->some_state_changed(true);

  if (msg.warning_count() > 0) connection()->diagnostic_area_changed(true);

  stage(Stage::Param);

  if (!connection()->events().empty()) {
    auto msg = *msg_res;

    msg.warning_count(msg.warning_count() + 1);

    auto send_res = ClassicFrame::send_msg(dst_conn, msg);
    if (!send_res) return stdx::unexpected(send_res.error());

    discard_current_msg(src_conn);

    return has_more_messages() ? Result::Again : Result::SendToClient;
  }

  return forward_server_to_client(has_more_messages());
}

bool StmtPrepareForwarder::has_more_messages() const {
  return columns_left_ != 0 || params_left_ != 0;
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::param() {
  if (params_left_ == 0) {
    // if there are no params, then there is no end-of-params either.
    stage(Stage::Column);
    return Result::Again;
  }

  auto &src_conn = connection()->server_conn();

  auto &dst_conn = connection()->client_conn();
  auto &dst_protocol = dst_conn.protocol();

  const auto skips_eof =
      classic_protocol::capabilities::pos::text_result_with_session_tracking;

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::ColumnMeta>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  bool is_unsigned =
      msg_res->flags().test(classic_protocol::column_def::pos::is_unsigned);

  // 0x8000 is the unsigned-flag.
  prep_stmt_.parameters.emplace_back(msg_res->type() |
                                     (is_unsigned ? 1 << 15 : 0));

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_prepare::param"));
  }

  if (--params_left_ == 0) {
    stage(Stage::EndOfParams);
  }

  return forward_server_to_client(
      has_more_messages() ||
      // there will be EOF, no need to flush the column already.
      !dst_protocol.shared_capabilities().test(skips_eof));
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::end_of_params() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->client_conn();
  auto &dst_protocol = dst_conn.protocol();

  stage(Stage::Column);

  const auto skips_eof =
      classic_protocol::capabilities::pos::text_result_with_session_tracking;
  const auto server_skips = src_protocol.shared_capabilities().test(skips_eof);
  const auto router_skips = dst_protocol.shared_capabilities().test(skips_eof);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_prepare::end_of_params"));
  }

  if (server_skips) {
    // server does not send a EOF

    // no end-of-params packet.
    if (router_skips) return Result::Again;

    // ... but client expects a EOF packet.
    auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Eof>(dst_conn, {});
    if (!send_res) return stdx::unexpected(send_res.error());

    return has_more_messages() ? Result::Again : Result::SendToClient;
  }

  if (router_skips) {
    // drop the Eof packet the server sent as the client does not want it.
    auto msg_res = ClassicFrame::recv_msg<
        classic_protocol::borrowed::message::server::Eof>(src_conn);
    if (!msg_res) return stdx::unexpected(msg_res.error());

    discard_current_msg(src_conn);

    return Result::Again;
  }

  // forward the end-of-params
  return forward_server_to_client(has_more_messages());
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::column() {
  auto &dst_protocol = connection()->client_conn().protocol();

  if (columns_left_ > 0) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("stmt_prepare::column"));
    }
    if (--columns_left_ == 0) {
      stage(Stage::EndOfColumns);
    }
    auto skips_eof =
        classic_protocol::capabilities::pos::text_result_with_session_tracking;

    return forward_server_to_client(
        has_more_messages() ||
        // there will be EOF, no need to flush the column already.
        !dst_protocol.shared_capabilities().test(skips_eof));
  }

  stage(Stage::OkDone);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::end_of_columns() {
  stage(Stage::OkDone);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_prepare::end_of_columns"));
  }

  return skip_or_inject_end_of_columns();
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::ok_done() {
  auto &dst_protocol = connection()->client_conn().protocol();

  // remember the stmt.
  dst_protocol.prepared_statements().emplace(stmt_id_, prep_stmt_);

  trace_command_end(trace_event_command_);

  stage(Stage::Done);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::error() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::Error>(src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto msg = *msg_res;

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("stmt_prepare::error"));
  }

  if (auto *ev = trace_span(trace_event_command_, "mysql/response")) {
    ClassicFrame::trace_set_attributes(ev, src_protocol, msg);

    trace_span_end(ev);
  }

  trace_command_end(trace_event_command_, TraceEvent::StatusCode::kError);

  connection()->diagnostic_area_changed(true);

  stage(Stage::Done);

  return forward_server_to_client();
}
