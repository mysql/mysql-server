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

#include "classic_auth_cleartext_forwarder.h"

#include <array>
#include <memory>  // unique_ptr
#include <system_error>

#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "auth_digest.h"
#include "classic_frame.h"
#include "harness_assert.h"
#include "hexify.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqld_error.h"  // mysql-server error-codes
#include "mysqlrouter/classic_protocol_wire.h"

IMPORT_LOG_FUNCTIONS()

using mysql_harness::hexify;

stdx::expected<Processor::Result, std::error_code>
AuthCleartextForwarder::process() {
  switch (stage()) {
    case Stage::Init:
      return init();
    case Stage::ClientData:
      return client_data();
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

stdx::expected<Processor::Result, std::error_code>
AuthCleartextForwarder::init() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = connection()->client_protocol();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("cleartext::forward::switch"));
  }

  auto send_res = ClassicFrame::send_msg<
      classic_protocol::borrowed::message::server::AuthMethodSwitch>(
      dst_channel, dst_protocol, {Auth::kName, initial_server_auth_data_});
  if (!send_res) return send_client_failed(send_res.error());

  stage(Stage::ClientData);

  return Result::SendToClient;
}

stdx::expected<Processor::Result, std::error_code>
AuthCleartextForwarder::client_data() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection()->client_protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::AuthMethodData>(
      src_channel, src_protocol);
  if (!msg_res) return recv_client_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("cleartext::forward::plaintext_password"));
  }

  stage(Stage::Response);

  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
AuthCleartextForwarder::response() {
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
    tr.trace(Tracer::Event().stage("cleartext::forward::response"));
  }

  // if there is another packet, dump its payload for now.
  auto &recv_buf = src_channel->recv_plain_view();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);

  log_debug("received unexpected message from server in cleartext-auth:\n%s",
            hexify(recv_buf).c_str());

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code>
AuthCleartextForwarder::ok() {
  stage(Stage::Done);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("cleartext::forward::ok"));
  }

  // leave the message in the queue for the AuthForwarder.
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
AuthCleartextForwarder::error() {
  stage(Stage::Done);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("cleartext::forward::error"));
  }

  // leave the message in the queue for the AuthForwarder.
  return Result::Again;
}
