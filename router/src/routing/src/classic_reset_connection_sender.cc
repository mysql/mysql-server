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

#include "classic_reset_connection_sender.h"

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "hexify.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "tracer.h"

IMPORT_LOG_FUNCTIONS()

stdx::expected<Processor::Result, std::error_code>
ResetConnectionSender::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Response:
      return response();
    case Stage::Ok:
      return ok();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
ResetConnectionSender::command() {
  auto &dst_conn = connection()->server_conn();
  auto &dst_protocol = dst_conn.protocol();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("reset_connection::command"));
  }

  trace_event_command_ = trace_span(parent_event_, "mysql/reset_connection");

  dst_protocol.seq_id(0xff);  // reset seq-id

  dst_protocol.system_variables().clear();

  const auto send_res = ClassicFrame::send_msg<
      classic_protocol::borrowed::message::client::ResetConnection>(dst_conn,
                                                                    {});
  if (!send_res) return send_server_failed(send_res.error());

  stage(Stage::Response);

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code>
ResetConnectionSender::response() {
  auto &src_conn = connection()->server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) return recv_server_failed(read_res.error());

  const uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    Ok = ClassicFrame::cmd_byte<classic_protocol::message::server::Ok>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Ok:
      stage(Stage::Ok);
      return Result::Again;
  }

  (void)ClassicFrame::ensure_has_full_frame(src_conn);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage(
        "reset_connection::response\n" +
        mysql_harness::hexify(src_channel.recv_plain_view())));
  }

  log_debug("reset_connection::response: unexpected msg-type '%02x'", msg_type);

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code> ResetConnectionSender::ok() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Ok>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("reset_connection::ok"));
  }

  trace_span_end(trace_event_command_);

  auto msg = *msg_res;

  if (!msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()), src_protocol.shared_capabilities());
    if (!track_res) {
      // ignore
    }
  }

  discard_current_msg(src_conn);

  connection()->connection_sharing_allowed_reset();

  stage(Stage::Done);
  return Result::Again;
}
