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

#include "classic_stmt_prepare.h"

#include "classic_connection.h"
#include "classic_forwarder.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "hexify.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "tracer.h"

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
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
    case Stage::Error:
      return error();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::command() {
  trace(Tracer::Event().stage("stmt_prepare::command"));

  auto &server_conn = connection()->socket_splicer()->server_conn();
  if (!server_conn.is_open()) {
    stage(Stage::Connect);
    return Result::Again;
  } else {
    stage(Stage::Response);
    return forward_client_to_server();
  }
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::connect() {
  trace(Tracer::Event().stage("stmt_prepare::connect"));

  stage(Stage::Connected);

  connection()->push_processor(
      std::make_unique<LazyConnector>(connection(), false /* in-handshake */));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::connected() {
  auto &server_conn = connection()->socket_splicer()->server_conn();
  if (!server_conn.is_open()) {
    // Connector sent an server::Error already.
    auto *socket_splicer = connection()->socket_splicer();
    auto src_channel = socket_splicer->client_channel();
    auto src_protocol = connection()->client_protocol();

    // take the client::command from the connection.
    auto recv_res =
        ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);
    if (!recv_res) return recv_client_failed(recv_res.error());

    discard_current_msg(src_channel, src_protocol);

    trace(Tracer::Event().stage("stmt_prepare::error"));

    stage(Stage::Done);
    return Result::Again;
  }

  trace(Tracer::Event().stage("stmt_prepare::connected"));

  stage(Stage::Response);
  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::response() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_server_failed(read_res.error());

  const uint8_t msg_type = src_protocol->current_msg_type().value();

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

  trace(Tracer::Event().stage("stmt_prepare::response"));

  return stdx::make_unexpected(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code> StmtPrepareForwarder::ok() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  const auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::StmtPrepareOk>(
          src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("stmt_prepare::ok"));

  auto stmt_prep_ok = *msg_res;

  if (stmt_prep_ok.with_metadata()) {
    columns_left_ = stmt_prep_ok.column_count();
    params_left_ = stmt_prep_ok.param_count();
  }

  connection()->some_state_changed(true);

  stage(Stage::Param);

  return forward_server_to_client(has_more_messages());
}

bool StmtPrepareForwarder::has_more_messages() const {
  return columns_left_ != 0 || params_left_ != 0;
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::param() {
  if (params_left_ > 0) {
    trace(Tracer::Event().stage("stmt_prepare::param"));
    if (--params_left_ == 0) {
      stage(Stage::EndOfParams);
    }
    return forward_server_to_client(has_more_messages());
  }

  stage(Stage::Column);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::end_of_params() {
  auto src_protocol = connection()->server_protocol();

  stage(Stage::Column);

  if (src_protocol->shared_capabilities().test(
          classic_protocol::capabilities::pos::
              text_result_with_session_tracking)) {
    // no end-of-params packet.
    return Result::Again;
  }

  trace(Tracer::Event().stage("stmt_prepare::end_of_params"));
  return forward_server_to_client(has_more_messages());
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::column() {
  if (columns_left_ > 0) {
    trace(Tracer::Event().stage("stmt_prepare::column"));
    if (--columns_left_ == 0) {
      stage(Stage::EndOfColumns);
    }
    return forward_server_to_client(has_more_messages());
  }

  stage(Stage::Done);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::end_of_columns() {
  auto src_protocol = connection()->server_protocol();

  stage(Stage::Done);

  if (src_protocol->shared_capabilities().test(
          classic_protocol::capabilities::pos::
              text_result_with_session_tracking)) {
    // no end-of-columns packet.
    return Result::Again;
  }

  trace(Tracer::Event().stage("stmt_prepare::end_of_columns"));
  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code>
StmtPrepareForwarder::error() {
  trace(Tracer::Event().stage("stmt_prepare::error"));

  stage(Stage::Done);

  return forward_server_to_client();
}
