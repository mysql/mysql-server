/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "classic_list_fields_forwarder.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"

/**
 * forward the list-fields message flow.
 *
 * Expected overall flow:
 *
 * @code
 * c->s: COM_LIST_FIELDS
 * alt table exists
 * loop
 * c<-s: Column
 * endloop
 * c<-s: Eof
 * else
 * c<-s: Error
 * end
 * @endcode
 *
 * If there is no server connection, it is created on demand.
 */
stdx::expected<Processor::Result, std::error_code>
ListFieldsForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
    case Stage::Response:
      return response();
    case Stage::Eof:
      return eof();
    case Stage::Error:
      return error();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
ListFieldsForwarder::command() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("list_fields::command"));
  }

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
ListFieldsForwarder::connect() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("list_fields::connect"));
  }

  stage(Stage::Connected);
  return mysql_reconnect_start();
}

stdx::expected<Processor::Result, std::error_code>
ListFieldsForwarder::connected() {
  auto &server_conn = connection()->socket_splicer()->server_conn();
  if (!server_conn.is_open()) {
    auto *socket_splicer = connection()->socket_splicer();
    auto *src_channel = socket_splicer->client_channel();
    auto *src_protocol = connection()->client_protocol();

    // take the client::command from the connection.
    auto recv_res =
        ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);
    if (!recv_res) return recv_client_failed(recv_res.error());

    discard_current_msg(src_channel, src_protocol);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("list_fields::connect::error"));
    }

    stage(Stage::Done);
    return reconnect_send_error_msg(src_channel, src_protocol);
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("list_fields::connected"));
  }

  stage(Stage::Response);
  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
ListFieldsForwarder::response() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_server_failed(read_res.error());

  const uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Eof = ClassicFrame::cmd_byte<classic_protocol::message::server::Eof>(),
    Error = ClassicFrame::cmd_byte<classic_protocol::message::server::Error>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Eof:
      stage(Stage::Eof);
      return Result::Again;
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("list_fields::column"));
  }

  // don't force the flush to the client as more messages from the server
  // follow.
  return forward_server_to_client(true /* noflush */);
}

stdx::expected<Processor::Result, std::error_code> ListFieldsForwarder::eof() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->server_channel();
  auto *src_protocol = connection()->server_protocol();
  auto *dst_protocol = connection()->client_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Eof>(
          src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto msg = *msg_res;

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("list_fields::end_of_columns"));
  }

  dst_protocol->status_flags(msg.status_flags());

  stage(Stage::Done);

  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code>
ListFieldsForwarder::error() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("list_fields::error"));
  }

  stage(Stage::Done);

  return forward_server_to_client();
}
