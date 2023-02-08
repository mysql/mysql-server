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

#include "classic_auth_native_sender.h"

#include "auth_digest.h"
#include "classic_auth.h"
#include "classic_frame.h"
#include "hexify.h"
#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

using mysql_harness::hexify;

stdx::expected<Processor::Result, std::error_code> AuthNativeSender::process() {
  switch (stage()) {
    case Stage::Init:
      return init();
    case Stage::Response:
      return response();
    case Stage::Error:
      return error();
    case Stage::Ok:
      return ok();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> AuthNativeSender::init() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = connection()->server_protocol();

  auto scramble_res = Auth::scramble(
      AuthBase::strip_trailing_null(initial_server_auth_data_), password_);
  if (!scramble_res) {
    return send_server_failed(make_error_code(std::errc::invalid_argument));
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("native::sender::scrambled_password"));
  }

  auto send_res = ClassicFrame::send_msg(
      dst_channel, dst_protocol,
      classic_protocol::borrowed::message::client::AuthMethodData{
          *scramble_res});
  if (!send_res) return send_server_failed(send_res.error());

  stage(Stage::Response);

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code>
AuthNativeSender::response() {
  // ERR|OK|EOF|other
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  // ensure the recv_buf has at last frame-header (+ msg-byte)
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
    tr.trace(Tracer::Event().stage("native::sender::response"));
  }

  // if there is another packet, dump its payload for now.
  auto &recv_buf = src_channel->recv_plain_view();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);

  log_debug("received unexpected message from server in native-auth:\n%s",
            hexify(recv_buf).c_str());

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code> AuthNativeSender::ok() {
  stage(Stage::Done);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("native::sender::ok"));
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> AuthNativeSender::error() {
  stage(Stage::Done);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("native::sender::error"));
  }

  return Result::Again;
}
