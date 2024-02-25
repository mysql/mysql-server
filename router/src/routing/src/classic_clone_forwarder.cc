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

#include "classic_clone_forwarder.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "harness_assert.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqlrouter/classic_protocol_clone.h"
#include "mysqlrouter/classic_protocol_codec_clone.h"

stdx::expected<Processor::Result, std::error_code> CloneForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
    case Stage::Response:
      return response();
    case Stage::CloneCommand:
      return clone_command();
    case Stage::CloneInit:
      return clone_init();
    case Stage::CloneAttach:
      return clone_attach();
    case Stage::CloneReinit:
      return clone_reinit();
    case Stage::CloneExecute:
      return clone_execute();
    case Stage::CloneAck:
      return clone_ack();
    case Stage::CloneExit:
      return clone_exit();
    case Stage::CloneResponse:
      return clone_response();
    case Stage::CloneData:
      return clone_data();
    case Stage::CloneComplete:
      return clone_complete();
    case Stage::CloneError:
      return clone_error();
    case Stage::Ok:
      return ok();
    case Stage::Error:
      return error();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> CloneForwarder::command() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::switch"));
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

stdx::expected<Processor::Result, std::error_code> CloneForwarder::connect() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::connect"));
  }

  stage(Stage::Connected);
  return mysql_reconnect_start();
}

stdx::expected<Processor::Result, std::error_code> CloneForwarder::connected() {
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
      tr.trace(Tracer::Event().stage("clone::connect::error"));
    }

    stage(Stage::Done);
    return reconnect_send_error_msg(src_channel, src_protocol);
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::connected"));
  }
  stage(Stage::Response);
  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code> CloneForwarder::response() {
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

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::response"));
  }

  return stdx::make_unexpected(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code> CloneForwarder::ok() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::switched"));
  }

  stage(Stage::CloneCommand);

  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code> CloneForwarder::error() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::error"));
  }

  stage(Stage::Done);

  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code>
CloneForwarder::clone_command() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection()->client_protocol();

  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_client_failed(read_res.error());

  const uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Init = ClassicFrame::cmd_byte<classic_protocol::clone::client::Init>(),
    Attach = ClassicFrame::cmd_byte<classic_protocol::clone::client::Attach>(),
    Reinit = ClassicFrame::cmd_byte<classic_protocol::clone::client::Reinit>(),
    Execute =
        ClassicFrame::cmd_byte<classic_protocol::clone::client::Execute>(),
    Ack = ClassicFrame::cmd_byte<classic_protocol::clone::client::Ack>(),
    Exit = ClassicFrame::cmd_byte<classic_protocol::clone::client::Exit>(),
  };

  clone_cmd_ = msg_type;

  switch (Msg{msg_type}) {
    case Msg::Init:
      stage(Stage::CloneInit);
      return Result::Again;
    case Msg::Attach:
      stage(Stage::CloneAttach);
      return Result::Again;
    case Msg::Reinit:
      stage(Stage::CloneReinit);
      return Result::Again;
    case Msg::Execute:
      stage(Stage::CloneExecute);
      return Result::Again;
    case Msg::Ack:
      stage(Stage::CloneAck);
      return Result::Again;
    case Msg::Exit:
      stage(Stage::CloneExit);
      return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::clone::*"));
  }

  return stdx::make_unexpected(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code>
CloneForwarder::clone_init() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::cmd::init"));
  }

  stage(Stage::CloneResponse);

  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
CloneForwarder::clone_attach() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::cmd::attach"));
  }

  stage(Stage::CloneResponse);

  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
CloneForwarder::clone_reinit() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::cmd::reinit"));
  }

  stage(Stage::CloneResponse);

  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
CloneForwarder::clone_execute() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::cmd::execute"));
  }

  stage(Stage::CloneResponse);

  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code> CloneForwarder::clone_ack() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::cmd::ack"));
  }

  stage(Stage::CloneResponse);

  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
CloneForwarder::clone_exit() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::cmd::exit"));
  }

  stage(Stage::CloneResponse);

  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
CloneForwarder::clone_response() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_server_failed(read_res.error());

  const uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Complete =
        ClassicFrame::cmd_byte<classic_protocol::clone::server::Complete>(),
    Error = ClassicFrame::cmd_byte<classic_protocol::clone::server::Error>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Error:
      stage(Stage::CloneError);
      return Result::Again;
    case Msg::Complete:
      stage(Stage::CloneComplete);
      return Result::Again;
    default:
      stage(Stage::CloneData);
      return Result::Again;
  }
}

stdx::expected<Processor::Result, std::error_code>
CloneForwarder::clone_data() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::data"));
  }

  stage(Stage::CloneResponse);

  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code>
CloneForwarder::clone_complete() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::complete"));
  }

  if (clone_cmd_ ==
      ClassicFrame::cmd_byte<classic_protocol::clone::client::Exit>()) {
    stage(Stage::Done);
  } else {
    stage(Stage::CloneCommand);
  }

  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code>
CloneForwarder::clone_error() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("clone::error"));
  }

  if (clone_cmd_ ==
      ClassicFrame::cmd_byte<classic_protocol::clone::client::Exit>()) {
    stage(Stage::Done);
  } else {
    stage(Stage::CloneCommand);
  }

  return forward_server_to_client();
}
