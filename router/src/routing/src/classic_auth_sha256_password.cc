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

#include "classic_auth_sha256_password.h"

#include "auth_digest.h"
#include "classic_frame.h"
#include "hexify.h"
#include "mysql/harness/logging/logging.h"
#include "mysqld_error.h"  // mysql-server error-codes

IMPORT_LOG_FUNCTIONS()

using mysql_harness::hexify;

// AuthSha256Password

std::optional<std::string> AuthSha256Password::scramble(
    [[maybe_unused]] std::string_view nonce, std::string_view pwd) {
  std::string s(pwd);

  s.push_back('\0');

  return s;
}

stdx::expected<size_t, std::error_code>
AuthSha256Password::send_public_key_request(
    Channel *dst_channel, ClassicProtocolState *dst_protocol) {
  return ClassicFrame::send_msg<
      classic_protocol::message::client::AuthMethodData>(
      dst_channel, dst_protocol, {std::string(kPublicKeyRequest)});
}

stdx::expected<size_t, std::error_code> AuthSha256Password::send_public_key(
    Channel *dst_channel, ClassicProtocolState *dst_protocol,
    const std::string &public_key) {
  return ClassicFrame::send_msg<
      classic_protocol::message::client::AuthMethodData>(
      dst_channel, dst_protocol, {public_key});
}

stdx::expected<size_t, std::error_code>
AuthSha256Password::send_plaintext_password(Channel *dst_channel,
                                            ClassicProtocolState *dst_protocol,
                                            const std::string &password) {
  return ClassicFrame::send_msg<
      classic_protocol::message::client::AuthMethodData>(
      dst_channel, dst_protocol, {password + '\0'});
}

stdx::expected<size_t, std::error_code>
AuthSha256Password::send_encrypted_password(Channel *dst_channel,
                                            ClassicProtocolState *dst_protocol,
                                            const std::string &encrypted) {
  return ClassicFrame::send_msg<
      classic_protocol::message::client::AuthMethodData>(
      dst_channel, dst_protocol, {encrypted});
}

bool AuthSha256Password::is_public_key_request(const std::string_view &data) {
  return data == kPublicKeyRequest;
}

bool AuthSha256Password::is_public_key(const std::string_view &data) {
  return data.size() == 256;
}

// sender

stdx::expected<Processor::Result, std::error_code> AuthSha256Sender::process() {
  switch (stage()) {
    case Stage::Init:
      return init();
    case Stage::Response:
      return response();
    case Stage::PublicKey:
      return public_key();
    case Stage::Error:
      return error();
    case Stage::Ok:
      return ok();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> AuthSha256Sender::init() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = connection()->server_protocol();

  if (dst_channel->ssl() || password_.empty()) {
    trace(Tracer::Event().stage("sha256_password::sender::plaintext_password"));

    auto send_res = ClassicFrame::send_msg<
        classic_protocol::message::client::AuthMethodData>(
        dst_channel, dst_protocol, {password_ + '\0'});
    if (!send_res) return send_server_failed(send_res.error());

    stage(Stage::Response);
  } else {
    trace(Tracer::Event().stage("sha256_password::sender::public_key_request"));
    auto send_res = Auth::send_public_key_request(dst_channel, dst_protocol);
    if (!send_res) return send_server_failed(send_res.error());

    stage(Stage::PublicKey);
  }

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code>
AuthSha256Sender::response() {
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

  // if there is another packet, dump its payload for now.
  auto &recv_buf = src_channel->recv_plain_buffer();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);

  log_debug("received unexpected message from server in sha256-auth:\n%s",
            mysql_harness::hexify(recv_buf).c_str());
  return recv_server_failed(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code>
AuthSha256Sender::public_key() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = connection()->server_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::AuthMethodData>(
          dst_channel, dst_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto msg = std::move(*msg_res);

  discard_current_msg(dst_channel, dst_protocol);

  auto pubkey_res = Auth::public_key_from_pem(msg.auth_method_data());
  if (!pubkey_res) return recv_server_failed(pubkey_res.error());

  auto encrypted_res = Auth::rsa_encrypt_password(*pubkey_res, password_,
                                                  initial_server_auth_data_);
  if (!encrypted_res) return send_server_failed(encrypted_res.error());

  auto send_res =
      Auth::send_encrypted_password(dst_channel, dst_protocol, *encrypted_res);
  if (!send_res) return send_server_failed(send_res.error());

  stage(Stage::Response);

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code> AuthSha256Sender::ok() {
  stage(Stage::Done);

  trace(Tracer::Event().stage("sha256_password::sender::ok"));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> AuthSha256Sender::error() {
  stage(Stage::Done);

  trace(Tracer::Event().stage("sha256_password::sender::error"));

  return Result::Again;
}

// forwarder

stdx::expected<Processor::Result, std::error_code>
AuthSha256Forwarder::process() {
  switch (stage()) {
    case Stage::Init:
      return init();
    case Stage::ClientData:
      return client_data();
    case Stage::EncryptedPassword:
      return encrypted_password();
    case Stage::Response:
      return response();
    case Stage::PublicKeyResponse:
      return public_key_response();
    case Stage::PublicKey:
      return public_key();
    case Stage::Error:
      return error();
    case Stage::Ok:
      return ok();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> AuthSha256Forwarder::init() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = connection()->client_protocol();

  trace(Tracer::Event().stage("sha256_password::forward::switch"));

  auto send_res = ClassicFrame::send_msg<
      classic_protocol::message::server::AuthMethodSwitch>(
      dst_channel, dst_protocol,
      {std::string(Auth::kName), initial_server_auth_data_});
  if (!send_res) return send_client_failed(send_res.error());

  stage(Stage::ClientData);

  return Result::SendToClient;
}

stdx::expected<Processor::Result, std::error_code>
AuthSha256Forwarder::client_data() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection()->client_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::client::AuthMethodData>(
          src_channel, src_protocol);
  if (!msg_res) return recv_client_failed(msg_res.error());

  trace(Tracer::Event().stage("sha256_password::forward::client_data:\n" +
                              hexify(msg_res->auth_method_data())));

  if (src_channel->ssl() ||
      msg_res->auth_method_data() == Auth::kEmptyPassword) {
    // password is null-terminated, remove it.
    src_protocol->password(std::string(
        AuthBase::strip_trailing_null(msg_res->auth_method_data())));

    trace(Tracer::Event().stage("sha256_password::forward::password:\n" +
                                hexify(*src_protocol->password())));

    discard_current_msg(src_channel, src_protocol);

    return send_password();
  } else if (Auth::is_public_key_request(msg_res->auth_method_data())) {
    trace(
        Tracer::Event().stage("sha256_password::forward::public_key_request"));

    if (AuthBase::connection_has_public_key(connection())) {
      // send the router's public-key to be able to decrypt the client's
      // password.
      discard_current_msg(src_channel, src_protocol);

      trace(Tracer::Event().stage("sha256_password::forward::public_key"));

      auto pubkey_res = Auth::public_key_from_ssl_ctx_as_pem(
          connection()->context().source_ssl_ctx()->get());
      if (!pubkey_res) {
        auto ec = pubkey_res.error();

        if (ec != std::errc::function_not_supported) {
          return send_client_failed(ec);
        }

        stage(Stage::Done);

        // couldn't get the public key, fail the auth.
        auto send_res =
            ClassicFrame::send_msg<classic_protocol::message::server::Error>(
                src_channel, src_protocol,
                {ER_ACCESS_DENIED_ERROR, "Access denied", "HY000"});
        if (!send_res) return send_client_failed(send_res.error());
      } else {
        // send the router's public key to the client.
        stage(Stage::EncryptedPassword);

        auto send_res =
            Auth::send_public_key(src_channel, src_protocol, *pubkey_res);
        if (!send_res) return send_client_failed(send_res.error());
      }

      return Result::SendToClient;
    } else {
      // client requested a public key, but router has no ssl-ctx
      // (client-ssl-mode is DISABLED|PASSTHROUGH)
      //
      // If the server-connection is encrypted, the server will treat the
      // public-key-request as an invalid password
      // (as it isn't terminated by \0)
      stage(Stage::PublicKeyResponse);

      return forward_client_to_server();
    }
  } else {
    discard_current_msg(src_channel, src_protocol);

    Tracer::Event().stage("sha256_password::forward::bad_message:\n" +
                          hexify(msg_res->auth_method_data()));

    return recv_client_failed(make_error_code(std::errc::bad_message));
  }
}

// encrypted password from client to server.
stdx::expected<Processor::Result, std::error_code>
AuthSha256Forwarder::encrypted_password() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection()->client_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::client::AuthMethodData>(
          src_channel, src_protocol);
  if (!msg_res) return recv_client_failed(msg_res.error());

  if (AuthBase::connection_has_public_key(connection())) {
    auto recv_res = Auth::rsa_decrypt_password(
        connection()->context().source_ssl_ctx()->get(),
        msg_res->auth_method_data(), src_protocol->auth_method_data());
    if (!recv_res) {
      trace(Tracer::Event().stage("sha256_password::forward::decrypt:\n" +
                                  recv_res.error().message()));
      return recv_client_failed(recv_res.error());
    }

    src_protocol->password(*recv_res);

    trace(Tracer::Event().stage("sha256_password::forward::password:\n" +
                                hexify(*src_protocol->password())));

    discard_current_msg(src_channel, src_protocol);

    return send_password();
  } else {
    trace(Tracer::Event().stage("sha256_password::forward::encrypted"));

    stage(Stage::Response);

    return forward_client_to_server();
  }
}

// encrypted password from client to server.
stdx::expected<Processor::Result, std::error_code>
AuthSha256Forwarder::send_password() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_protocol = connection()->client_protocol();

  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = connection()->server_protocol();

  if (dst_channel->ssl() || src_protocol->password()->empty()) {
    // the server-side is encrypted (or the password is empty):
    //
    // send plaintext password
    trace(
        Tracer::Event().stage("sha256_password::forward::plaintext_password"));

    stage(Stage::Response);

    auto send_res = Auth::send_plaintext_password(
        dst_channel, dst_protocol, src_protocol->password().value());
    if (!send_res) return send_server_failed(send_res.error());
  } else {
    // the server is NOT encrypted: ask for the server's publickey
    trace(
        Tracer::Event().stage("sha256_password::forward::public_key_request"));

    stage(Stage::PublicKeyResponse);

    auto send_res = Auth::send_public_key_request(dst_channel, dst_protocol);
    if (!send_res) return send_server_failed(send_res.error());
  }

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code>
AuthSha256Forwarder::response() {
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

  trace(Tracer::Event().stage("sha256_password::forward::response"));

  // if there is another packet, dump its payload for now.
  auto &recv_buf = src_channel->recv_plain_buffer();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);

  log_debug(
      "received unexpected message from server in sha256-password-auth:\n%s",
      hexify(recv_buf).c_str());

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code>
AuthSha256Forwarder::public_key_response() {
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
    AuthData = ClassicFrame::cmd_byte<
        classic_protocol::message::server::AuthMethodData>(),
    Error = ClassicFrame::cmd_byte<classic_protocol::message::server::Error>(),
  };

  switch (Msg{msg_type}) {
    case Msg::AuthData:
      stage(Stage::PublicKey);
      return Result::Again;
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
  }

  trace(Tracer::Event().stage("sha256_password::forward::response"));

  // if there is another packet, dump its payload for now.
  auto &recv_buf = src_channel->recv_plain_buffer();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);

  log_debug(
      "received unexpected message from server in sha256-password-auth:\n%s",
      hexify(recv_buf).c_str());

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

/*
 * @pre
 * a public-key request was sent to the server.
 */
stdx::expected<Processor::Result, std::error_code>
AuthSha256Forwarder::public_key() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = connection()->server_protocol();
  auto src_protocol = connection()->client_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::AuthMethodData>(
          dst_channel, dst_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("sha256_password::forward::public_key"));

  if (!src_protocol->password().has_value()) {
    stage(Stage::EncryptedPassword);

    return forward_server_to_client();
  }

  auto password = *src_protocol->password();
  auto msg = std::move(*msg_res);

  discard_current_msg(dst_channel, dst_protocol);

  auto pubkey_res = Auth::public_key_from_pem(msg.auth_method_data());
  if (!pubkey_res) return recv_server_failed(pubkey_res.error());

  auto encrypted_res = Auth::rsa_encrypt_password(*pubkey_res, password,
                                                  initial_server_auth_data_);
  if (!encrypted_res) return send_server_failed(encrypted_res.error());

  trace(Tracer::Event().stage("sha256_password::forward::encrypted_password"));

  auto send_res =
      Auth::send_encrypted_password(dst_channel, dst_protocol, *encrypted_res);
  if (!send_res) return send_server_failed(send_res.error());

  stage(Stage::Response);

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code> AuthSha256Forwarder::ok() {
  stage(Stage::Done);

  trace(Tracer::Event().stage("sha256_password::forward::ok"));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
AuthSha256Forwarder::error() {
  stage(Stage::Done);

  trace(Tracer::Event().stage("sha256_password::forward::error"));

  return Result::Again;
}
