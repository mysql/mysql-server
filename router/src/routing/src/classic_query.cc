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

#include "classic_query.h"

#include <memory>
#include <system_error>

#include "classic_connection.h"
#include "classic_forwarder.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "processor.h"

stdx::expected<Processor::Result, std::error_code> QueryForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
    case Stage::Response:
      return response();
    case Stage::ColumnCount:
      return column_count();
    case Stage::LoadData:
      return load_data();
    case Stage::Data:
      return data();
    case Stage::Column:
      return column();
    case Stage::ColumnEnd:
      return column_end();
    case Stage::RowOrEnd:
      return row_or_end();
    case Stage::Row:
      return row();
    case Stage::RowEnd:
      return row_end();
    case Stage::Ok:
      return ok();
    case Stage::Error:
      return error();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::command() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection()->client_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::client::Query>(
          src_channel, src_protocol);
  if (!msg_res) return recv_client_failed(msg_res.error());

  trace(Tracer::Event().stage("query::command: " +
                              msg_res->statement().substr(0, 1024)));

  auto &server_conn = connection()->socket_splicer()->server_conn();
  if (!server_conn.is_open()) {
    stage(Stage::Connect);
    return Result::Again;
  } else {
    stage(Stage::Response);
    return forward_client_to_server();
  }
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::connect() {
  trace(Tracer::Event().stage("query::connect"));

  stage(Stage::Connected);

  connection()->push_processor(std::make_unique<LazyConnector>(
      connection(), false /* not in handshake */));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::connected() {
  auto &server_conn = connection()->socket_splicer()->server_conn();
  if (!server_conn.is_open()) {
    // Connector sent an server::Error already.
    auto *socket_splicer = connection()->socket_splicer();
    auto src_channel = socket_splicer->client_channel();
    auto src_protocol = connection()->client_protocol();

    // take the client::command from the connection.
    auto msg_res = ClassicFrame::recv_msg<classic_protocol::wire::String>(
        src_channel, src_protocol);
    if (!msg_res) return recv_client_failed(msg_res.error());

    discard_current_msg(src_channel, src_protocol);

    trace(Tracer::Event().stage("query::error"));

    stage(Stage::Done);
    return Result::Again;
  }

  trace(Tracer::Event().stage("query::connected"));
  stage(Stage::Response);
  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::response() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_server_failed(read_res.error());

  uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Error = ClassicFrame::cmd_byte<classic_protocol::message::server::Error>(),
    Ok = ClassicFrame::cmd_byte<classic_protocol::message::server::Ok>(),
    LoadData = 0xfb,
  };

  switch (Msg{msg_type}) {
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
    case Msg::Ok:
      stage(Stage::Ok);
      return Result::Again;
    case Msg::LoadData:
      stage(Stage::LoadData);
      return Result::Again;
  }

  stage(Stage::ColumnCount);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::load_data() {
  trace(Tracer::Event().stage("query::load_data"));

  stage(Stage::Data);
  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::data() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection()->client_protocol();

  auto read_res = ClassicFrame::ensure_frame_header(src_channel, src_protocol);
  if (!read_res) return recv_client_failed(read_res.error());

  trace(Tracer::Event().stage("query::data"));

  // local-data is finished with an empty packet.
  if (src_protocol->current_frame()->frame_size_ == 4) {
    stage(Stage::Response);
  }

  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
QueryForwarder::column_count() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::wire::VarInt>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::column_count"));

  columns_left_ = msg_res->value();

  stage(Stage::Column);

  return forward_server_to_client(true);
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::column() {
  const auto trace_event = Tracer::Event().stage("query::column");

  trace(trace_event);

  if (--columns_left_ == 0) {
    stage(Stage::ColumnEnd);
  }

  return forward_server_to_client(true);
}

stdx::expected<Processor::Result, std::error_code>
QueryForwarder::column_end() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();
  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = connection()->client_protocol();

  auto skips_eof_pos =
      classic_protocol::capabilities::pos::text_result_with_session_tracking;

  bool server_skips_end_of_columns{
      src_protocol->shared_capabilities().test(skips_eof_pos)};

  bool router_skips_end_of_columns{
      dst_protocol->shared_capabilities().test(skips_eof_pos)};

  if (server_skips_end_of_columns && router_skips_end_of_columns) {
    // this is a Row, not a EOF packet.
    stage(Stage::RowOrEnd);
    return Result::Again;
  } else if (!server_skips_end_of_columns && !router_skips_end_of_columns) {
    trace(Tracer::Event().stage("query::column_end::eof"));
    stage(Stage::RowOrEnd);
    return forward_server_to_client(true);
  } else if (!server_skips_end_of_columns && router_skips_end_of_columns) {
    // client is new, server is old: drop the server's EOF.
    trace(Tracer::Event().stage("query::column_end::skip_eof"));

    auto msg_res =
        ClassicFrame::recv_msg<classic_protocol::message::server::Eof>(
            src_channel, src_protocol);
    if (!msg_res) return recv_server_failed(msg_res.error());

    discard_current_msg(src_channel, src_protocol);

    stage(Stage::RowOrEnd);
    return Result::Again;
  } else {
    // client is old, server is new: inject an EOF between column-meta and rows.
    trace(Tracer::Event().stage("query::column_end::add_eof"));

    auto msg_res =
        ClassicFrame::send_msg<classic_protocol::message::server::Eof>(
            dst_channel, dst_protocol, {});
    if (!msg_res) return recv_server_failed(msg_res.error());

    stage(Stage::RowOrEnd);
    return Result::SendToServer;
  }
}

stdx::expected<Processor::Result, std::error_code>
QueryForwarder::row_or_end() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_server_failed(read_res.error());

  uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Error = ClassicFrame::cmd_byte<classic_protocol::message::server::Error>(),
    EndOfResult =
        ClassicFrame::cmd_byte<classic_protocol::message::server::Eof>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
    case Msg::EndOfResult:
      // 0xfe is used for:
      //
      // - end-of-rows packet
      // - fields in a row > 16MByte.
      if (src_protocol->current_frame()->frame_size_ < 1024) {
        stage(Stage::RowEnd);
        return Result::Again;
      }
      [[fallthrough]];
    default:
      stage(Stage::Row);
      return Result::Again;
  }
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::row() {
  trace(Tracer::Event().stage("query::row"));

  stage(Stage::RowOrEnd);
  return forward_server_to_client(true /* noflush */);
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::row_end() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Eof>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::row_end"));

  auto msg = std::move(*msg_res);

  if (!msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()),
        src_protocol->shared_capabilities());
  }

  if (msg.status_flags().test(
          classic_protocol::status::pos::more_results_exist)) {
    stage(Stage::Response);  // another resultset is coming

    trace(Tracer::Event().stage("query::more_resultsets"));
    return forward_server_to_client(true);
  } else {
    stage(Stage::Done);  // once the message is forwarded, we are done.
    return forward_server_to_client();
  }
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::ok() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Ok>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::ok"));

  auto msg = std::move(*msg_res);

  if (!msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()),
        src_protocol->shared_capabilities());
  }

  if (msg.status_flags().test(
          classic_protocol::status::pos::more_results_exist)) {
    stage(Stage::Response);  // another resultset is coming

    trace(Tracer::Event().stage("query::more_resultsets"));
    return forward_server_to_client(true);
  } else {
    stage(Stage::Done);  // once the message is forwarded, we are done.
    return forward_server_to_client();
  }
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::error() {
  trace(Tracer::Event().stage("query::error"));

  stage(Stage::Done);
  return forward_server_to_client();
}

// Sender

stdx::expected<Processor::Result, std::error_code> QuerySender::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Response:
      return response();
    case Stage::ColumnCount:
      return column_count();
    case Stage::LoadData:
      return load_data();
    case Stage::Data:
      return data();
    case Stage::Column:
      return column();
    case Stage::ColumnEnd:
      return column_end();
    case Stage::RowOrEnd:
      return row_or_end();
    case Stage::Row:
      return row();
    case Stage::RowEnd:
      return row_end();
    case Stage::Ok:
      return ok();
    case Stage::Error:
      return error();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> QuerySender::command() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = connection()->server_protocol();

  trace(Tracer::Event().stage("query::command"));
  trace(Tracer::Event().stage(">> " + stmt_));

  dst_protocol->seq_id(0xff);

  auto send_res =
      ClassicFrame::send_msg<classic_protocol::message::client::Query>(
          dst_channel, dst_protocol, stmt_);
  if (!send_res) return send_server_failed(send_res.error());

  stage(Stage::Response);

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::response() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_server_failed(read_res.error());

  uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Error = ClassicFrame::cmd_byte<classic_protocol::message::server::Error>(),
    Ok = ClassicFrame::cmd_byte<classic_protocol::message::server::Ok>(),
    LoadData = 0xfb,
  };

  switch (Msg{msg_type}) {
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
    case Msg::Ok:
      stage(Stage::Ok);
      return Result::Again;
    case Msg::LoadData:
      stage(Stage::LoadData);
      return Result::Again;
  }

  stage(Stage::ColumnCount);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::load_data() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::wire::String>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::load_data"));

  // we could decode the filename here.

  discard_current_msg(src_channel, src_protocol);

  stage(Stage::Data);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::data() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = connection()->server_protocol();

  trace(Tracer::Event().stage("query::data"));

  // an empty packet.
  auto send_res = ClassicFrame::send_msg<classic_protocol::wire::String>(
      dst_channel, dst_protocol, {});
  if (!send_res) return send_server_failed(send_res.error());

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::column_count() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::ColumnCount>(
          src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::column_count"));

  if (handler_) handler_->on_column_count(msg_res->count());

  columns_left_ = msg_res->count();

  discard_current_msg(src_channel, src_protocol);

  stage(Stage::Column);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::column() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::ColumnMeta>(
          src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::column"));

  discard_current_msg(src_channel, src_protocol);

  if (handler_) handler_->on_column(*msg_res);

  if (--columns_left_ == 0) {
    const auto skips_eof_pos =
        classic_protocol::capabilities::pos::text_result_with_session_tracking;

    const bool server_skips_end_of_columns{
        src_protocol->shared_capabilities().test(skips_eof_pos)};

    if (server_skips_end_of_columns) {
      // next is a Row, not a EOF packet.
      stage(Stage::RowOrEnd);
    } else {
      stage(Stage::ColumnEnd);
    }
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::column_end() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Eof>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::column_end"));

  discard_current_msg(src_channel, src_protocol);

  stage(Stage::RowOrEnd);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::row_or_end() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_server_failed(read_res.error());

  uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Error = ClassicFrame::cmd_byte<classic_protocol::message::server::Error>(),
    EndOfResult =
        ClassicFrame::cmd_byte<classic_protocol::message::server::Eof>(),
  };

  switch (Msg{msg_type}) {
    case Msg::EndOfResult:
      stage(Stage::RowEnd);
      return Result::Again;
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
  }

  stage(Stage::Row);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::row() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Row>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::row"));

  discard_current_msg(src_channel, src_protocol);

  if (handler_) handler_->on_row(*msg_res);

  stage(Stage::RowOrEnd);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::row_end() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  trace(Tracer::Event().stage("query::row_end"));

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Eof>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto eof_msg = std::move(*msg_res);

  if (handler_) handler_->on_row_end(eof_msg);

  if (!eof_msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(eof_msg.session_changes()),
        src_protocol->shared_capabilities());
  }

  discard_current_msg(src_channel, src_protocol);

  if (eof_msg.status_flags().test(
          classic_protocol::status::pos::more_results_exist)) {
    trace(Tracer::Event().stage("query::more_resultsets"));
    stage(Stage::Response);

    return Result::Again;
  } else {
    trace(Tracer::Event().stage("query::row_end"));
    stage(Stage::Done);
    return Result::Again;
  }
}

stdx::expected<Processor::Result, std::error_code> QuerySender::ok() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Ok>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  discard_current_msg(src_channel, src_protocol);

  auto msg = std::move(*msg_res);

  if (handler_) handler_->on_ok(msg);

  if (!msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()),
        src_protocol->shared_capabilities());
  }

  if (msg.status_flags().test(
          classic_protocol::status::pos::more_results_exist)) {
    trace(Tracer::Event().stage("query::ok::more"));
    stage(Stage::Response);
  } else {
    trace(Tracer::Event().stage("query::ok::done"));
    stage(Stage::Done);
  }
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::error() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::Error>(
          src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::error"));

  discard_current_msg(src_channel, src_protocol);

  if (handler_) handler_->on_error(*msg_res);

  stage(Stage::Done);
  return Result::Again;
}
