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

#include "classic_auth_native_forwarder.h"

#include "classic_auth.h"
#include "classic_auth_caching_sha2.h"
#include "classic_frame.h"
#include "hexify.h"
#include "mysql/harness/logging/logging.h"
#include "mysqld_error.h"  // mysql-server error-codes

IMPORT_LOG_FUNCTIONS()

using mysql_harness::hexify;

stdx::expected<Processor::Result, std::error_code>
AuthNativeForwarder::process() {
  switch (stage()) {
    case Stage::Init:
      return init();
    case Stage::ClientData:
      return client_data();
    case Stage::CachingSha2Scrambled:
      return caching_sha2_scrambled();
    case Stage::CachingSha2Encrypted:
      return caching_sha2_encrypted();
    case Stage::CachingSha2Plaintext:
      return caching_sha2_plaintext();
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

/*
 * server switched to mysql_native_password
 *
 * - if the client supports caching-sha2-password, use it to fetch the client's
 *   password.
 * - otherwise forward the switch message to the client.
 */
stdx::expected<Processor::Result, std::error_code> AuthNativeForwarder::init() {
  auto &dst_conn = connection()->client_conn();
  auto &dst_protocol = dst_conn.protocol();

  if (connection()->context().connection_sharing() &&
      dst_protocol.auth_method_name() == "caching_sha2_password" &&
      (AuthCachingSha2Password::connection_has_public_key(connection()) ||
       dst_conn.is_secure_transport())) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("native::forward::switch_for_plaintext"));
    }

    // speak caching-sha2-password with the client to get the plaintext-password
    auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::AuthMethodSwitch>(
        dst_conn, {dst_protocol.auth_method_name(), initial_server_auth_data_});
    if (!send_res) return send_client_failed(send_res.error());

    stage(Stage::CachingSha2Scrambled);
    return Result::SendToClient;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("native::forward::switch"));
  }

  auto send_res = ClassicFrame::send_msg<
      classic_protocol::borrowed::message::server::AuthMethodSwitch>(
      dst_conn, {Auth::kName, initial_server_auth_data_});
  if (!send_res) return send_client_failed(send_res.error());

  stage(Stage::ClientData);
  return Result::SendToClient;
}

stdx::expected<Processor::Result, std::error_code>
AuthNativeForwarder::client_data() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::AuthMethodData>(src_conn);
  if (!msg_res) return recv_client_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("native::forward::scrambled_password"));
  }

  if (msg_res->auth_method_data().empty()) {
    src_protocol.password("");
  }

  stage(Stage::Response);

  return forward_client_to_server();
}

/*
 * - receive the caching_sha2_password's scrambled fast-auth packet
 * - if it is empty, remember the empty-password and forward it.
 * - otherwise, discard it and ask the client for the plaintext-password.
 */
stdx::expected<Processor::Result, std::error_code>
AuthNativeForwarder::caching_sha2_scrambled() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->server_conn();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::AuthMethodData>(src_conn);
  if (!msg_res) return recv_client_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("native::forward::scrambled"));
  }

  // caching-sha2-password sends a "\x00" for empty-password.
  if (msg_res->auth_method_data() == std::string_view("\x00", 1) ||
      msg_res->auth_method_data().empty()) {
    src_protocol.password("");

    discard_current_msg(src_conn);

    stage(Stage::Response);

    // native password expected a empty packet for empty-password.
    auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::client::AuthMethodData>(dst_conn,
                                                                     {{}});
    if (!send_res) return send_server_failed(send_res.error());

    return Result::SendToServer;
  }

  discard_current_msg(src_conn);

  // request the plaintext password.
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("native::forward::request_plaintext"));
  }

  stage(Stage::CachingSha2Plaintext);

  auto send_res = ClassicFrame::send_msg<
      classic_protocol::borrowed::message::server::AuthMethodData>(src_conn,
                                                                   {"\x04"});
  if (!send_res) return send_client_failed(send_res.error());

  return Result::SendToClient;
}

/*
 * - receive the client's plaintext password via caching-sha2-password
 * - scramble according to mysql-native-password.
 */
stdx::expected<Processor::Result, std::error_code>
AuthNativeForwarder::caching_sha2_plaintext() {
  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->server_conn();

  // receive plaintext password.
  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::AuthMethodData>(src_conn);
  if (!msg_res) return recv_client_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("native::forward::caching_full_auth"));
  }

  if (AuthCachingSha2Password::is_public_key_request(
          msg_res->auth_method_data()) &&
      !src_conn.is_secure_transport()) {
    // send the router's public-key to be able to decrypt the client's
    // password.
    discard_current_msg(src_conn);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("caching_sha2::forward::public_key"));
    }

    auto pubkey_res = AuthCachingSha2Password::public_key_from_ssl_ctx_as_pem(
        connection()->context().source_ssl_ctx()->get());
    if (!pubkey_res) {
      auto ec = pubkey_res.error();

      if (ec != std::errc::function_not_supported) {
        return send_client_failed(ec);
      }

      stage(Stage::Done);

      // couldn't get the public key, fail the auth.
      auto send_res = ClassicFrame::send_msg<
          classic_protocol::borrowed::message::server::Error>(
          src_conn, {ER_ACCESS_DENIED_ERROR, "Access denied", "HY000"});
      if (!send_res) return send_client_failed(send_res.error());
    } else {
      // send the router's public key to the client.
      stage(Stage::CachingSha2Encrypted);

      auto send_res = AuthCachingSha2Password::send_public_key(
          src_channel, src_protocol, *pubkey_res);
      if (!send_res) return send_client_failed(send_res.error());
    }

    return Result::SendToClient;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("native::forward::plaintext"));
  }

  src_protocol.password(
      std::string(AuthBase::strip_trailing_null(msg_res->auth_method_data())));

  discard_current_msg(src_conn);

  // scramble according the mysql_native_password.
  auto scramble_res =
      Auth::scramble(AuthBase::strip_trailing_null(initial_server_auth_data_),
                     *src_protocol.password());
  if (!scramble_res) {
    return recv_client_failed(make_error_code(std::errc::bad_message));
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("native::forward::scrambled"));
  }

  // send scrambled native-password to the server.
  stage(Stage::Response);

  auto send_res =
      ClassicFrame::send_msg<classic_protocol::message::client::AuthMethodData>(
          dst_conn, {*scramble_res});
  if (!send_res) return send_server_failed(send_res.error());

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code>
AuthNativeForwarder::caching_sha2_encrypted() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->server_conn();

  // receive plaintext password.
  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::AuthMethodData>(src_conn);
  if (!msg_res) return recv_client_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("native::forward::encrypted"));
  }

  harness_assert(AuthBase::connection_has_public_key(connection()));

  auto nonce = initial_server_auth_data_;

  // if there is a trailing zero, strip it.
  if (nonce.size() == AuthCachingSha2Password::kNonceLength + 1 &&
      nonce[AuthCachingSha2Password::kNonceLength] == 0x00) {
    nonce = nonce.substr(0, AuthCachingSha2Password::kNonceLength);
  }

  auto recv_res = AuthCachingSha2Password::rsa_decrypt_password(
      connection()->context().source_ssl_ctx()->get(),
      msg_res->auth_method_data(), nonce);
  if (!recv_res) return recv_client_failed(recv_res.error());

  src_protocol.password(*recv_res);

  discard_current_msg(src_conn);

  auto scramble_res =
      Auth::scramble(AuthBase::strip_trailing_null(initial_server_auth_data_),
                     *src_protocol.password());
  if (!scramble_res) {
    return send_server_failed(make_error_code(std::errc::bad_message));
  }

  stage(Stage::Response);

  auto send_res =
      ClassicFrame::send_msg<classic_protocol::message::client::AuthMethodData>(
          dst_conn, {*scramble_res});
  if (!send_res) return send_server_failed(send_res.error());

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code>
AuthNativeForwarder::response() {
  // ERR|OK|EOF|other
  auto &src_conn = connection()->server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  // ensure the recv_buf has at last frame-header (+ msg-byte)
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

  // if there is another packet, dump its payload for now.
  const auto &recv_buf = src_channel.recv_plain_view();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ClassicFrame::ensure_has_full_frame(src_conn);

  log_debug("received unexpected message from server in native-auth:\n%s",
            hexify(recv_buf).c_str());

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code> AuthNativeForwarder::ok() {
  stage(Stage::Done);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("native::forward::ok"));
  }

  // leave the message in the queue for the AuthForwarder.
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
AuthNativeForwarder::error() {
  stage(Stage::Done);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("native::forward::error"));
  }

  // leave the message in the queue for the AuthForwarder.
  return Result::Again;
}
