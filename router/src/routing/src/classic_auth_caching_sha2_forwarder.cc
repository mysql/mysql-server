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

#include "classic_auth_caching_sha2_forwarder.h"

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

// Forwarder

stdx::expected<Processor::Result, std::error_code>
AuthCachingSha2Forwarder::process() {
  switch (stage()) {
    case Stage::Init:
      return init();
    case Stage::ClientData:
      return client_data();
    case Stage::EncryptedPassword:
      return encrypted_password();
    case Stage::PlaintextPassword:
      return plaintext_password();
    case Stage::Response:
      return response();
    case Stage::PublicKeyResponse:
      return public_key_response();
    case Stage::PublicKey:
      return public_key();
    case Stage::AuthData:
      return auth_data();
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
AuthCachingSha2Forwarder::init() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = connection()->client_protocol();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("caching_sha2::forward::switch"));
  }

  auto send_res = ClassicFrame::send_msg<
      classic_protocol::borrowed::message::server::AuthMethodSwitch>(
      dst_channel, dst_protocol, {Auth::kName, initial_server_auth_data_});
  if (!send_res) return send_client_failed(send_res.error());

  stage(Stage::ClientData);
  return Result::SendToClient;
}

stdx::expected<Processor::Result, std::error_code>
AuthCachingSha2Forwarder::client_data() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::AuthMethodData>(
      src_channel, src_protocol);
  if (!msg_res) return recv_client_failed(msg_res.error());

  if (Auth::is_public_key_request(msg_res->auth_method_data())) {
    if (auto &tr = tracer()) {
      tr.trace(
          Tracer::Event().stage("caching_sha2::forward::public_key_request"));
    }

    if (AuthBase::connection_has_public_key(connection())) {
      // send the router's public-key to be able to decrypt the client's
      // password.
      discard_current_msg(src_channel, src_protocol);

      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage("caching_sha2::forward::public_key"));
      }

      auto pubkey_res = Auth::public_key_from_ssl_ctx_as_pem(
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
      // If the server-connection is secure, the server will treat the
      // public-key-request as an invalid password
      // (as it isn't terminated by \0)
      stage(Stage::PublicKeyResponse);

      return forward_client_to_server();
    }
  } else if (msg_res->auth_method_data() == std::string_view("\x00", 1)) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("caching_sha2::forward::empty_password"));
    }

    src_protocol->password("");

    stage(Stage::Response);

    return forward_client_to_server();
  } else if (connection()->context().connection_sharing() &&
             socket_splicer->client_conn().is_secure_transport()) {
    // while it is possible to request the plain-text password over
    // plaintext-connections via "public-key", the router doesn't know how the
    // client would react to that request.
    //
    // By default clients don't use the public-key auth and would close the
    // connection with "caching-sha2-password requires a SSL connection".
    //
    // Long story short: only request the public-key via secure connections.

    discard_current_msg(src_channel, src_protocol);

    if (auto &tr = tracer()) {
      tr.trace(
          Tracer::Event().stage("caching_sha2::forward::scrambled_password"));
    }

    // ask the client for a plaintext password.
    auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::AuthMethodData>(
        src_channel, src_protocol, {"\x04"});
    if (!send_res) return send_client_failed(send_res.error());

    client_requested_full_auth_ = true;

    stage(Stage::PlaintextPassword);

    return Result::SendToClient;
  } else {
    // if it isn't a public-key request, it is a fast-auth.
    if (auto &tr = tracer()) {
      tr.trace(
          Tracer::Event().stage("caching_sha2::forward::scrambled_password"));
    }

    stage(Stage::Response);

    return forward_client_to_server();
  }
}

// encrypted password from client to server.
stdx::expected<Processor::Result, std::error_code>
AuthCachingSha2Forwarder::encrypted_password() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection()->client_protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::AuthMethodData>(
      src_channel, src_protocol);
  if (!msg_res) return recv_client_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("caching_sha2::forward::encrypted"));
  }

  if (AuthBase::connection_has_public_key(connection())) {
    auto nonce = initial_server_auth_data_;

    // if there is a trailing zero, strip it.
    if (nonce.size() == Auth::kNonceLength + 1 &&
        nonce[Auth::kNonceLength] == 0x00) {
      nonce = nonce.substr(0, Auth::kNonceLength);
    }

    auto recv_res = Auth::rsa_decrypt_password(
        connection()->context().source_ssl_ctx()->get(),
        msg_res->auth_method_data(), nonce);
    if (!recv_res) return recv_client_failed(recv_res.error());

    src_protocol->password(*recv_res);

    discard_current_msg(src_channel, src_protocol);

    return send_password();
  } else {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("caching_sha2::forward::encrypted"));
    }

    stage(Stage::Response);

    return forward_client_to_server();
  }
}

// plaintext password from client to server.
stdx::expected<Processor::Result, std::error_code>
AuthCachingSha2Forwarder::plaintext_password() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::AuthMethodData>(
      src_channel, src_protocol);
  if (!msg_res) return recv_client_failed(msg_res.error());

  if (socket_splicer->client_conn().is_secure_transport()) {
    if (auto &tr = tracer()) {
      tr.trace(
          Tracer::Event().stage("caching_sha2::forward::plaintext_password"));
    }

    // remove trailing null
    src_protocol->password(std::string(
        AuthBase::strip_trailing_null(msg_res->auth_method_data())));

    discard_current_msg(src_channel, src_protocol);

    return send_password();
  } else if (Auth::is_public_key_request(msg_res->auth_method_data())) {
    if (auto &tr = tracer()) {
      tr.trace(
          Tracer::Event().stage("caching_sha2::forward::public_key_request"));
    }

    if (AuthBase::connection_has_public_key(connection())) {
      // send the router's public-key to be able to decrypt the client's
      // password.
      discard_current_msg(src_channel, src_protocol);

      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage("caching_sha2::forward::public_key"));
      }

      auto pubkey_res = Auth::public_key_from_ssl_ctx_as_pem(
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

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("caching_sha2::forward::bad_message"));
    }

    return recv_client_failed(make_error_code(std::errc::bad_message));
  }
}

// encrypted password from client to server.
stdx::expected<Processor::Result, std::error_code>
AuthCachingSha2Forwarder::send_password() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_protocol = connection()->client_protocol();

  auto *dst_channel = socket_splicer->server_channel();
  auto *dst_protocol = connection()->server_protocol();

  if (!server_requested_full_auth_) {
    // server hasn't requested full auth yet, it expects a scrambled password.
    auto scramble_res =
        Auth::scramble(AuthBase::strip_trailing_null(initial_server_auth_data_),
                       *src_protocol->password());
    if (!scramble_res) {
      return send_server_failed(make_error_code(std::errc::bad_message));
    }

    stage(Stage::Response);

    auto msg_res = ClassicFrame::send_msg<
        classic_protocol::message::client::AuthMethodData>(
        dst_channel, dst_protocol, {*scramble_res});
    if (!msg_res) return send_server_failed(msg_res.error());

    return Result::SendToServer;
  }

  if (socket_splicer->server_conn().is_secure_transport()) {
    // the server-side is secure: send plaintext password
    if (auto &tr = tracer()) {
      tr.trace(
          Tracer::Event().stage("caching_sha2::forward::plaintext_password"));
    }

    stage(Stage::Response);

    auto send_res = Auth::send_plaintext_password(
        dst_channel, dst_protocol, src_protocol->password().value());
    if (!send_res) return send_server_failed(send_res.error());
    return Result::SendToServer;
  }

  // the server is NOT secure: ask for the server's publickey
  if (auto &tr = tracer()) {
    tr.trace(
        Tracer::Event().stage("caching_sha2::forward::public_key_request"));
  }

  stage(Stage::PublicKeyResponse);

  auto send_res = Auth::send_public_key_request(dst_channel, dst_protocol);
  if (!send_res) return send_server_failed(send_res.error());

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code>
AuthCachingSha2Forwarder::response() {
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
    AuthData = ClassicFrame::cmd_byte<
        classic_protocol::message::server::AuthMethodData>(),
  };

  switch (Msg{msg_type}) {
    case Msg::AuthData:
      stage(Stage::AuthData);
      return Result::Again;
    case Msg::Ok:
      stage(Stage::Ok);
      return Result::Again;
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
  }

  // if there is another packet, dump its payload for now.
  auto &recv_buf = src_channel->recv_plain_view();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);

  log_debug("received unexpected message from server in caching-sha2-auth:\n%s",
            hexify(recv_buf).c_str());

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code>
AuthCachingSha2Forwarder::public_key_response() {
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

  if (auto &tr = tracer()) {
    tr.trace(
        Tracer::Event().stage("caching_sha2::forward::public_key_response"));
  }

  // if there is another packet, dump its payload for now.
  auto &recv_buf = src_channel->recv_plain_view();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);

  log_debug(
      "received unexpected message from server in sha256-password-auth:\n%s",
      hexify(recv_buf).c_str());

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

// receive a public-key from the server.
stdx::expected<Processor::Result, std::error_code>
AuthCachingSha2Forwarder::public_key() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = connection()->server_protocol();
  auto src_protocol = connection()->client_protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::AuthMethodData>(
      dst_channel, dst_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("caching_sha2::forward::public-key"));
  }

  if (!src_protocol->password().has_value()) {
    // the client's password isn't known.
    //
    // Forward the server's public-key to the client
    stage(Stage::EncryptedPassword);

    return forward_server_to_client();
  }

  // as the plaintext password is known, encrypt it with the server's
  // public-key.
  auto pubkey_res = Auth::public_key_from_pem(msg_res->auth_method_data());
  if (!pubkey_res) return recv_server_failed(pubkey_res.error());

  discard_current_msg(dst_channel, dst_protocol);

  auto password = *src_protocol->password();

  auto nonce = initial_server_auth_data_;

  // if there is a trailing zero, strip it.
  if (nonce.size() == Auth::kNonceLength + 1 &&
      nonce[Auth::kNonceLength] == 0x00) {
    nonce = nonce.substr(0, Auth::kNonceLength);
  }

  auto encrypted_res = Auth::rsa_encrypt_password(*pubkey_res, password, nonce);
  if (!encrypted_res) return send_server_failed(encrypted_res.error());

  auto send_res =
      Auth::send_encrypted_password(dst_channel, dst_protocol, *encrypted_res);
  if (!send_res) return send_server_failed(send_res.error());

  stage(Stage::Response);

  return Result::SendToServer;
}

// after fast-auth failed.
stdx::expected<Processor::Result, std::error_code>
AuthCachingSha2Forwarder::auth_data() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = connection()->server_protocol();
  auto src_protocol = connection()->client_protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::AuthMethodData>(
      dst_channel, dst_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (msg_res->auth_method_data() == std::string_view("\x04")) {
    server_requested_full_auth_ = true;

    if (auto &tr = tracer()) {
      tr.trace(
          Tracer::Event().stage("caching_sha2::forward::request_full_auth"));
    }

    if (src_protocol->password().has_value()) {
      discard_current_msg(dst_channel, dst_protocol);

      return send_password();
    } else {
      client_requested_full_auth_ = true;

      stage(Stage::PlaintextPassword);

      return forward_server_to_client();
    }
  } else if (msg_res->auth_method_data() == std::string_view("\x03")) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("caching_sha2::forward::fast_auth_ok"));
    }

    // next is a Ok packet.
    stage(Stage::Response);

    if (client_requested_full_auth_) {
      // 0x03 means the client-greeting provided the right scrambled
      // password that matches the cached entry.

      // as there is a password provided by the client already
      // the client side expects either server::Ok or server::Error now.
      //
      // c<-r: server::greeting (from router)
      // c->r: client::greeting (with tls handshake)
      // c<-r: 0x01 0x04
      // c->r: password
      //    r->s: connect()
      //    r<-s: server::greeting
      //    r->s: client::greeting (with tls handshake, rehashed pwd)
      //    r<-s: 0x01 0x03   // current message
      //    r<-s: server::Ok
      // c<-r: server::Ok
      discard_current_msg(dst_channel, dst_protocol);

      // skip this message.
      return Result::Again;
    } else {
      return forward_server_to_client(true);
    }
  } else {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("caching_sha2::forward::??\n" +
                                     hexify(msg_res->auth_method_data())));
    }
    stage(Stage::Response);

    return forward_server_to_client();
  }
}

stdx::expected<Processor::Result, std::error_code>
AuthCachingSha2Forwarder::ok() {
  stage(Stage::Done);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("caching_sha2::forward::ok"));
  }

  // leave the message in the queue for the AuthForwarder.
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
AuthCachingSha2Forwarder::error() {
  stage(Stage::Done);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("caching_sha2::forward::error"));
  }

  // leave the message in the queue for the AuthForwarder.
  return Result::Again;
}
