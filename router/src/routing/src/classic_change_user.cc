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

#include "classic_change_user.h"

#include <optional>

#include <openssl/evp.h>

#include "classic_auth.h"
#include "classic_connect.h"
#include "classic_connection.h"
#include "classic_forwarder.h"
#include "classic_frame.h"
#include "classic_greeting.h"
#include "classic_query.h"
#include "hexify.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql-server error-codes
#include "openssl_version.h"
#include "tracer.h"

IMPORT_LOG_FUNCTIONS()

using mysql_harness::hexify;

using namespace std::string_literals;
using namespace std::string_view_literals;

static bool connection_has_public_key(
    MysqlRoutingClassicConnection *connection) {
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 0, 2)
  if (!connection->context().source_ssl_ctx()) return false;

  SSL_CTX *ssl_ctx = connection->context().source_ssl_ctx()->get();

  return SSL_CTX_get0_certificate(ssl_ctx) != nullptr;
#else
  (void)connection;
  return false;
#endif
}

/**
 * forward the change-user message flow.
 *
 * Expected overall flow:
 *
 * @code
 * c->s: COM_CHANGE_USER
 * alt fast-path
 * alt
 * c<-s: Error
 * else
 * c<-s: Ok
 * end
 * else auth-method-switch
 * c<-s: auth-method-switch
 * c->s: auth-method-data
 * loop more data
 * c<-s: auth-method-data
 * opt
 * c->s: auth-method-data
 * end
 * end
 * alt
 * c<-s: Error
 * else
 * c<-s: Ok
 * end
 * end
 * @endcode
 *
 * If there is no server connection, it is created on demand.
 */
stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
    case Stage::Response:
      return response();
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
ChangeUserForwarder::command() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::client::ChangeUser>(
          src_channel, src_protocol, src_protocol->server_capabilities());
  if (!msg_res) {
    if (msg_res.error().category() ==
        make_error_code(classic_protocol::codec_errc::invalid_input)
            .category()) {
      // a codec error.

      discard_current_msg(src_channel, src_protocol);

      auto send_res =
          ClassicFrame::send_msg<classic_protocol::message::server::Error>(
              src_channel, src_protocol, {1047, "Unknown command", "08S01"});
      if (!send_res) return send_client_failed(send_res.error());

      stage(Stage::Done);
      return Result::SendToClient;
    }
    return recv_client_failed(msg_res.error());
  }

  src_protocol->username(msg_res->username());
  src_protocol->schema(msg_res->schema());
  src_protocol->attributes(msg_res->attributes());
  src_protocol->password(std::nullopt);
  src_protocol->auth_method_name(msg_res->auth_method_name());

  discard_current_msg(src_channel, src_protocol);

  trace(Tracer::Event().stage("change_user::command"));

  auto &server_conn = connection()->socket_splicer()->server_conn();
  if (!server_conn.is_open()) {
    stage(Stage::Connect);
  } else {
    // connection to the server exists, create a new ChangeUser command (don't
    // forward the client's as is) as the attributes need to be modified.
    connection()->push_processor(std::make_unique<ChangeUserSender>(
        connection(), true /* in-handshake */));

    stage(Stage::Response);
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::connect() {
  trace(Tracer::Event().stage("change_user::connect"));

  stage(Stage::Connected);

  // connect or take connection from pool
  //
  // don't use LazyConnector here as it would call authenticate with the old
  // user and then switch to the new one in a 2nd ChangeUser.
  connection()->push_processor(
      std::make_unique<ConnectProcessor>(connection()));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::connected() {
  auto &server_conn = connection()->socket_splicer()->server_conn();
  auto server_protocol = connection()->server_protocol();

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

    trace(Tracer::Event().stage("change_user::connect::error"));

    stage(Stage::Done);
    return Result::Again;
  }

  trace(Tracer::Event().stage("change_user::connected"));

  if (server_protocol->server_greeting()) {
    // from pool.
    connection()->push_processor(
        std::make_unique<ChangeUserSender>(connection(), true));
  } else {
    // connector, but not greeted yet.
    connection()->push_processor(
        std::make_unique<ServerGreetor>(connection(), true));
  }

  stage(Stage::Response);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::response() {
  // ChangeUserSender will set ->authenticated if it succeed.
  if (!connection()->authenticated()) {
    stage(Stage::Error);
  } else {
    stage(Stage::Ok);
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> ChangeUserForwarder::ok() {
  trace(Tracer::Event().stage("change_user::ok"));

  // allow connection sharing again.
  connection()->connection_sharing_allowed_reset();

  // clear the warnings
  connection()->execution_context().diagnostics_area().warnings().clear();

  if (connection()->context().connection_sharing() &&
      connection()->greeting_from_router()) {
    // if connection sharing is enabled in the config, enable the
    // session-tracker.
    connection()->push_processor(std::make_unique<QuerySender>(connection(), R"(
SET @@SESSION.session_track_schema           = 'ON',
    @@SESSION.session_track_system_variables = '*',
    @@SESSION.session_track_transaction_info = 'CHARACTERISTICS',
    @@SESSION.session_track_gtids            = 'OWN_GTID',
    @@SESSION.session_track_state_change     = 'ON')"));

    stage(Stage::Done);
  } else {
    stage(Stage::Done);
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserForwarder::error() {
  trace(Tracer::Event().stage("change_user::error"));

  auto *socket_splicer = connection()->socket_splicer();

  // after the error the server will close the server connection.
  auto &server_conn = socket_splicer->server_conn();

  (void)server_conn.close();

  stage(Stage::Done);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> ChangeUserSender::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Response:
      return response();
    case Stage::AuthMethodSwitch:
      return auth_method_switch();
    case Stage::ClientData:
      return client_data();
    case Stage::AuthResponse:
      return auth_response();
    case Stage::ServerData:
      return server_data();
    case Stage::Ok:
      return ok();
    case Stage::Error:
      return error();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

/**
 * router specific connection attributes.
 *
 * @param[in] ssl pointer to a SSL struct of the client connection. May be
 * nullptr.
 */
static std::vector<std::pair<std::string, std::string>>
client_ssl_connection_attributes(const SSL *ssl) {
  if (ssl == nullptr) return {};

  return {{"_client_ssl_cipher", SSL_get_cipher_name(ssl)},
          {"_client_ssl_version", SSL_get_version(ssl)}};
}

/**
 * splice two vectors together.
 *
 * appends all elements of other to the vector v.
 */
template <class T>
std::vector<T> vector_splice(std::vector<T> v, const std::vector<T> &other) {
  v.insert(v.end(), other.begin(), other.end());
  return v;
}

/**
 * verify connection attributes are sane.
 *
 * connection attributes are a key-value-key-value-...
 *
 * - decodes as var-string
 * - each key must have a value
 */
static stdx::expected<void, std::error_code>
classic_proto_verify_connection_attributes(const std::string &attrs) {
  // track if each key has a matching value.
  bool is_key{true};
  auto attr_buf = net::buffer(attrs);

  while (net::buffer_size(attr_buf) != 0) {
    const auto decode_res =
        classic_protocol::decode<classic_protocol::wire::VarString>(attr_buf,
                                                                    {});
    if (!decode_res) return decode_res.get_unexpected();

    const auto bytes_read = decode_res->first;
    const auto kv = decode_res->second;

    attr_buf += bytes_read;

    // toggle the key/value tracker.
    is_key = !is_key;
  }

  // if the last key doesn't have a value, fail
  if (!is_key || net::buffer_size(attr_buf) != 0) {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  return {};
}

static stdx::expected<size_t, std::error_code> classic_proto_append_attribute(
    std::string &attrs_buf, const std::string &key, const std::string &value) {
  auto encode_res =
      classic_protocol::encode(classic_protocol::wire::VarString(key), {},
                               net::dynamic_buffer(attrs_buf));
  if (!encode_res) {
    return encode_res.get_unexpected();
  }

  size_t encoded_bytes = encode_res.value();

  encode_res =
      classic_protocol::encode(classic_protocol::wire::VarString(value), {},
                               net::dynamic_buffer(attrs_buf));
  if (!encode_res) {
    return encode_res.get_unexpected();
  }

  encoded_bytes += encode_res.value();

  return encoded_bytes;
}

/**
 * remove trailing \0 in a string_view.
 *
 * returns the original string-view, if there is no trailing NUL-char.
 */
static auto strip_trailing_null(std::string_view s) {
  if (s.empty()) return s;

  if (s.back() == '\0') s.remove_suffix(1);

  return s;
}

/**
 * merge connection attributes.
 *
 * - verifies the connection attributes
 * - appends new attributes.
 * - sets attributes back to the client-greeting-msg
 *
 * @returns bytes appended on success, std::error_code on error.
 */
static stdx::expected<std::string, std::error_code>
classic_proto_decode_and_add_connection_attributes(
    std::string attrs,
    const std::vector<std::pair<std::string, std::string>> &extra_attributes) {
  // add attributes if they are sane.
  const auto verify_res = classic_proto_verify_connection_attributes(attrs);
  if (!verify_res) return verify_res.get_unexpected();

  for (const auto &attr : extra_attributes) {
    const auto append_res =
        classic_proto_append_attribute(attrs, attr.first, attr.second);
    if (!append_res) return append_res.get_unexpected();
  }

  return {attrs};
}

static std::optional<std::string> scramble_them_all(
    std::string_view auth_method, std::string_view nonce,
    std::string_view pwd) {
  if (auth_method == AuthCachingSha2Password::kName) {
    return AuthCachingSha2Password::scramble(nonce, pwd);
  } else if (auth_method == AuthNativePassword::kName) {
    return AuthNativePassword::scramble(nonce, pwd);
  } else if (auth_method == AuthSha256Password::kName) {
    return AuthSha256Password::scramble(nonce, pwd);
  } else if (auth_method == AuthCleartextPassword::kName) {
    return AuthCleartextPassword::scramble(nonce, pwd);
  } else {
    return std::nullopt;
  }
}

static classic_protocol::message::client::ChangeUser change_user_for_reuse(
    Channel *src_channel, ClassicProtocolState *src_protocol,
    [[maybe_unused]] ClassicProtocolState *dst_protocol,
    std::vector<std::pair<std::string, std::string>>
        initial_connection_attributes) {
  harness_assert(src_protocol->client_greeting().has_value());
  harness_assert(dst_protocol->server_greeting().has_value());

  const auto append_attrs_res =
      classic_proto_decode_and_add_connection_attributes(
          src_protocol->attributes(),
          vector_splice(initial_connection_attributes,
                        client_ssl_connection_attributes(src_channel->ssl())));
  // if decode/append fails forward the attributes as is. The server should
  // fail too.
  auto attrs = append_attrs_res.value_or(src_protocol->attributes());

  if (src_protocol->password().has_value()) {
    // scramble with the server's auth-data to trigger a fast-auth.

    auto pwd = *(src_protocol->password());

    // if the password set and not empty, rehash it.
    if (auto scramble_res = scramble_them_all(
            src_protocol->auth_method_name(),
            strip_trailing_null(dst_protocol->auth_method_data()), pwd)) {
      return {
          src_protocol->username(),                      // username
          *scramble_res,                                 // auth_method_data
          src_protocol->schema(),                        // schema
          src_protocol->client_greeting()->collation(),  // collation
          src_protocol->auth_method_name(),              // auth_method_name
          attrs,                                         // attributes
      };
    }
  }

  return {
      src_protocol->username(),                      // username
      "",                                            // auth_method_data
      src_protocol->schema(),                        // schema
      src_protocol->client_greeting()->collation(),  // collation
      "switch_me_if_you_can",                        // auth_method_name
      attrs,                                         // attributes
  };
}

stdx::expected<Processor::Result, std::error_code> ChangeUserSender::command() {
  auto *socket_splicer = connection()->socket_splicer();
  auto &src_conn = socket_splicer->client_conn();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection()->client_protocol();

  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = connection()->server_protocol();

  change_user_msg_ =
      change_user_for_reuse(src_channel, src_protocol, dst_protocol,
                            src_conn.initial_connection_attributes());
#if 0
  std::cerr << __LINE__ << ": username: " << change_user_msg_->username()
            << "\n"
            << ".. schema: " << change_user_msg_->schema() << "\n"
            << ".. auth-method-name: " << change_user_msg_->auth_method_name()
            << "\n"
            << ".. auth-method-data: "
            << hexify(change_user_msg_->auth_method_data()) << "\n"
      //
      ;
#endif

  trace(Tracer::Event().stage("change_user::command"));

  dst_protocol->seq_id(0xff);  // reset seq-id

  auto send_res =
      ClassicFrame::send_msg(dst_channel, dst_protocol, *change_user_msg_);
  if (!send_res) return send_server_failed(send_res.error());

  stage(Stage::Response);
  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserSender::response() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_server_failed(read_res.error());

  const uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    AuthMethodSwitch = ClassicFrame::cmd_byte<
        classic_protocol::message::server::AuthMethodSwitch>(),
    Ok = ClassicFrame::cmd_byte<classic_protocol::message::server::Ok>(),
    Error = ClassicFrame::cmd_byte<classic_protocol::message::server::Error>(),
  };

  switch (Msg{msg_type}) {
    case Msg::AuthMethodSwitch:
      stage(Stage::AuthMethodSwitch);
      return Result::Again;
    case Msg::Ok:
      stage(Stage::Ok);
      return Result::Again;
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
  }

  trace(Tracer::Event().stage("change_user::response"));

  return stdx::make_unexpected(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserSender::auth_method_switch() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();
  auto dst_protocol = connection()->client_protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::message::server::AuthMethodSwitch>(src_channel,
                                                           src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto msg = std::move(*msg_res);

  trace(Tracer::Event().stage("change_user::auth_method_switch: " +
                              msg.auth_method()));

  src_protocol->auth_method_name(msg.auth_method());
  src_protocol->auth_method_data(msg.auth_method_data());
  dst_protocol->auth_method_name(msg.auth_method());
  dst_protocol->auth_method_data(msg.auth_method_data());

  if (dst_protocol->password().has_value()) {
    auto pwd = *(dst_protocol->password());

    discard_current_msg(src_channel, src_protocol);

    if (!src_channel->ssl() && !pwd.empty() &&
        dst_protocol->auth_method_name() == AuthSha256Password::kName) {
      // the server channel isn't encrypted, request the public-key.

      auto send_res = AuthSha256Password::send_public_key_request(src_channel,
                                                                  src_protocol);

      if (!send_res) return send_server_failed(send_res.error());
    } else {
      trace(Tracer::Event().stage("change_user::client_data"));

      auto scramble_res = scramble_them_all(
          msg.auth_method(), strip_trailing_null(msg.auth_method_data()), pwd);

      if (!scramble_res) {
        return recv_server_failed(make_error_code(std::errc::bad_message));
      }

      auto send_res = ClassicFrame::send_msg<classic_protocol::wire::String>(
          src_channel, src_protocol, {*scramble_res});
      if (!send_res) return send_server_failed(send_res.error());
    }

    stage(Stage::AuthResponse);
    return Result::SendToServer;
  } else {
    stage(Stage::ClientData);
    return forward_server_to_client();
  }
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserSender::client_data() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection()->client_protocol();
  auto *dst_channel = socket_splicer->server_channel();
  auto *dst_protocol = connection()->server_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::client::AuthMethodData>(
          src_channel, src_protocol);
  if (!msg_res) return recv_client_failed(msg_res.error());

  trace(Tracer::Event().stage("change_user::client_data:\n" +
                              hexify(msg_res->auth_method_data())));

  if (src_protocol->auth_method_name() == AuthCachingSha2Password::kName) {
    using Auth = AuthCachingSha2Password;

    if (Auth::is_public_key_request(msg_res->auth_method_data()) &&
        connection_has_public_key(connection())) {
      discard_current_msg(src_channel, src_protocol);

      trace(Tracer::Event().stage("server::auth::public_key"));

      auto pubkey_res = Auth::public_key_from_ssl_ctx_as_pem(
          connection()->context().source_ssl_ctx()->get());
      if (!pubkey_res) {
        auto ec = pubkey_res.error();

        if (ec != std::errc::function_not_supported) {
          return send_client_failed(ec);
        }

        // couldn't get the public key, fail the auth.
        auto send_res =
            ClassicFrame::send_msg<classic_protocol::message::server::Error>(
                src_channel, src_protocol,
                {ER_ACCESS_DENIED_ERROR, "Access denied", "HY000"});
        if (!send_res) return send_client_failed(send_res.error());
      } else {
        auto send_res =
            Auth::send_public_key(src_channel, src_protocol, *pubkey_res);
        if (!send_res) return send_client_failed(send_res.error());
      }

      return Result::SendToClient;
    } else if (Auth::is_public_key(msg_res->auth_method_data()) &&
               connection_has_public_key(connection())) {
      auto recv_res = Auth::rsa_decrypt_password(
          connection()->context().source_ssl_ctx()->get(),
          msg_res->auth_method_data(), src_protocol->auth_method_data());
      if (!recv_res) return recv_client_failed(recv_res.error());

      src_protocol->password(*recv_res);

      trace(Tracer::Event().stage("change_user::password"));

      discard_current_msg(src_channel, src_protocol);

      if (dst_channel->ssl()) {
        // the server-side is encrypted: send plaintext password
        trace(Tracer::Event().stage("change_user::plaintext_password"));

        auto send_res = Auth::send_plaintext_password(
            dst_channel, dst_protocol, *src_protocol->password());
        if (!send_res) return send_server_failed(send_res.error());
      } else {
        // the server is NOT encrypted: ask for the server's publickey
        trace(Tracer::Event().stage("change_user::request_public_key"));

        auto send_res =
            Auth::send_public_key_request(dst_channel, dst_protocol);
        if (!send_res) return send_server_failed(send_res.error());
      }

      stage(Stage::AuthResponse);
      return Result::SendToServer;
    } else if (src_channel->ssl() && msg_res->auth_method_data().size() == 32) {
      // try fast auth first.

      stage(Stage::AuthResponse);
      return forward_client_to_server();
    } else if (src_channel->ssl()) {
      discard_current_msg(src_channel, src_protocol);

      auto pwd = msg_res->auth_method_data();
      pwd.resize(pwd.size() - 1);

      // the plaintext password.
      src_protocol->password(pwd);

      if (dst_channel->ssl() || pwd.empty()) {
        trace(Tracer::Event().stage("client::auth::plaintext_password"));
        // the server-side is encrypted: send plaintext password with trailing
        // \0
        auto send_res = Auth::send_plaintext_password(
            dst_channel, dst_protocol, *src_protocol->password());
        if (!send_res) return send_server_failed(send_res.error());
      } else {
        trace(Tracer::Event().stage("client::auth::request_public_key"));
        // the server is NOT encrypted: ask for the server's publickey
        auto send_res =
            Auth::send_public_key_request(dst_channel, dst_protocol);
        if (!send_res) return send_server_failed(send_res.error());
      }

      stage(Stage::AuthResponse);
      return Result::SendToServer;
    }
  } else if (src_protocol->auth_method_name() == AuthSha256Password::kName) {
    using Auth = AuthSha256Password;

    if (Auth::is_public_key_request(msg_res->auth_method_data()) &&
        connection_has_public_key(connection())) {
      discard_current_msg(src_channel, src_protocol);

      trace(Tracer::Event().stage("server::auth::public_key"));

      auto pubkey_res = Auth::public_key_from_ssl_ctx_as_pem(
          connection()->context().source_ssl_ctx()->get());
      if (!pubkey_res) return send_client_failed(pubkey_res.error());

      auto send_res =
          Auth::send_public_key(src_channel, src_protocol, *pubkey_res);
      if (!send_res) return send_client_failed(send_res.error());

      return Result::SendToClient;
    } else if (Auth::is_public_key(msg_res->auth_method_data()) &&
               connection()->context().source_ssl_ctx()) {
      auto recv_res = Auth::rsa_decrypt_password(
          connection()->context().source_ssl_ctx()->get(),
          msg_res->auth_method_data(), src_protocol->auth_method_data());
      if (!recv_res) return recv_client_failed(recv_res.error());

      src_protocol->password(*recv_res);

      discard_current_msg(src_channel, src_protocol);

      if (dst_channel->ssl()) {
        trace(Tracer::Event().stage("client::auth_data::password"));
        // the server-side is encrypted: send plaintext password

        auto send_res = Auth::send_plaintext_password(
            dst_channel, dst_protocol, *src_protocol->password());
        if (!send_res) return send_server_failed(send_res.error());
      } else {
        // the server is NOT encrypted: ask for the server's public-key
        trace(Tracer::Event().stage("client::auth::request_public_key"));

        auto send_res =
            Auth::send_public_key_request(dst_channel, dst_protocol);
        if (!send_res) return send_server_failed(send_res.error());
      }

      stage(Stage::AuthResponse);
      return Result::SendToServer;
    } else if (src_channel->ssl()) {
      discard_current_msg(src_channel, src_protocol);
      auto pwd = msg_res->auth_method_data();
      pwd.resize(pwd.size() - 1);

      // the plaintext password.
      src_protocol->password(pwd);

      if (dst_channel->ssl() || pwd.empty()) {
        trace(Tracer::Event().stage("client::auth_data::password"));
        // the server-side is encrypted (or the password is empty)
        auto send_res = Auth::send_plaintext_password(
            dst_channel, dst_protocol, *src_protocol->password());
        if (!send_res) return send_server_failed(send_res.error());
      } else {
        // the server is NOT encrypted: ask for the server's public-key
        trace(Tracer::Event().stage("client::auth::request_public_key"));

        auto send_res =
            Auth::send_public_key_request(dst_channel, dst_protocol);
        if (!send_res) return send_server_failed(send_res.error());
      }

      stage(Stage::AuthResponse);
      return Result::SendToServer;
    }
  }

  stage(Stage::AuthResponse);
  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
ChangeUserSender::auth_response() {
  // ERR|OK|data
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
    AuthData = ClassicFrame::cmd_byte<
        classic_protocol::message::server::AuthMethodData>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
    case Msg::Ok:
      stage(Stage::Ok);
      return Result::Again;
    case Msg::AuthData:
      stage(Stage::ServerData);
      return Result::Again;
  }

  trace(Tracer::Event().stage("change_user::auth::response"));

  return stdx::make_unexpected(make_error_code(std::errc::bad_message));
}

/**
 * receive auth-data from the server handle it.
 */
stdx::expected<Processor::Result, std::error_code>
ChangeUserSender::server_data() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();
  auto dst_protocol = connection()->client_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::AuthMethodData>(
          src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto msg = std::move(*msg_res);

  if (src_protocol->auth_method_name() == AuthCachingSha2Password::kName) {
    using Auth = AuthCachingSha2Password;

    // if ensure_has_full_frame fails, we'll fail later with bad_message.

    if (msg.auth_method_data().size() < 1) {
      return recv_server_failed(make_error_code(std::errc::bad_message));
    }

    switch (msg.auth_method_data()[0]) {
      case Auth::kFastAuthDone:
        trace(Tracer::Event().stage("change_user::auth::fast-auth-ok"));

        stage(Stage::AuthResponse);

        // fast-auth-ok is followed by Ok
        if (dst_protocol->password().has_value()) {
          // as the client provided a password already, it expects a Ok next.
          discard_current_msg(src_channel, src_protocol);

          return Result::Again;
        } else {
          // client provided a hash and expects the 0x03 and OK.
          return forward_server_to_client(true /* noflush */);
        }
      case Auth::kPerformFullAuth:
        trace(Tracer::Event().stage("change_user::auth::full-auth"));

        if (dst_protocol->password().has_value()) {
          auto opt_pwd = dst_protocol->password();

          discard_current_msg(src_channel, src_protocol);

          if (!src_channel->ssl()) {
            // the server is NOT encrypted: ask for the server's publickey
            trace(
                Tracer::Event().stage("change_user::auth::request_public_key"));
            auto send_res =
                Auth::send_public_key_request(src_channel, src_protocol);
            if (!send_res) return send_server_failed(send_res.error());
          } else {
            trace(
                Tracer::Event().stage("change_user::auth::plaintext_password"));
            auto send_res = Auth::send_plaintext_password(
                src_channel, src_protocol, *opt_pwd);
            if (!send_res) return send_server_failed(send_res.error());
          }

          // send it to the server.
          stage(Stage::AuthResponse);
          return Result::SendToServer;
        } else {
          // forward request for full auth to the client.
          stage(Stage::ClientData);

          return forward_server_to_client();
        }
      case '-': {
        trace(Tracer::Event().stage("change_user::auth::public_key"));

        if (dst_protocol->password().has_value()) {
          // the client's password is known: answer the server directly.
          auto pubkey_res = Auth::public_key_from_pem(msg.auth_method_data());
          if (!pubkey_res) return recv_server_failed(pubkey_res.error());

          discard_current_msg(src_channel, src_protocol);

          trace(Tracer::Event().stage("client::auth::password"));

          auto encrypted_res = Auth::rsa_encrypt_password(
              *pubkey_res, *(dst_protocol->password()),
              src_protocol->auth_method_data());
          if (!encrypted_res) return send_server_failed(encrypted_res.error());

          auto send_res = Auth::send_encrypted_password(
              src_channel, src_protocol, *encrypted_res);
          if (!send_res) return send_server_failed(send_res.error());

          stage(Stage::Response);
          return Result::SendToServer;
        } else {
          // ... otherwise forward the public-key to the client and send its
          // encrypted password.
          stage(Stage::ClientData);

          return forward_server_to_client();
        }
      }
    }
  } else if (src_protocol->auth_method_name() == AuthSha256Password::kName) {
    using Auth = AuthSha256Password;
    // public key
    trace(Tracer::Event().stage("server::auth::public-key"));

    if (dst_protocol->password().has_value()) {
      // the client's password is known: answer the server directly.
      auto pubkey_res = Auth::public_key_from_pem(msg.auth_method_data());
      if (!pubkey_res) return recv_server_failed(pubkey_res.error());

      discard_current_msg(src_channel, src_protocol);

      trace(Tracer::Event().stage("client::auth::password"));

      auto encrypted_res =
          Auth::rsa_encrypt_password(*pubkey_res, *(dst_protocol->password()),
                                     src_protocol->auth_method_data());
      if (!encrypted_res) return send_server_failed(encrypted_res.error());

      auto send_res = Auth::send_encrypted_password(src_channel, src_protocol,
                                                    *encrypted_res);
      if (!send_res) return send_server_failed(send_res.error());

      stage(Stage::AuthResponse);
      return Result::SendToServer;
    } else {
      // ... otherwise forward the public-key to the client and send its
      // encrypted password.
      stage(Stage::ClientData);

      return forward_server_to_client();
    }
  }

  log_debug("change_user::auth::data: %s",
            hexify(msg.auth_method_data()).c_str());

  return stdx::make_unexpected(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code> ChangeUserSender::ok() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();
  auto dst_protocol = connection()->client_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Ok>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("change_user::ok"));

  auto msg = std::move(*msg_res);

  if (!msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()),
        src_protocol->shared_capabilities());
  }

  connection()->authenticated(true);

  src_protocol->username(change_user_msg_->username());
  dst_protocol->username(change_user_msg_->username());
  src_protocol->schema(change_user_msg_->schema());
  dst_protocol->schema(change_user_msg_->schema());
  src_protocol->sent_attributes(change_user_msg_->attributes());
  dst_protocol->sent_attributes(change_user_msg_->attributes());

  stage(Stage::Done);
  if (in_handshake_) {
    return forward_server_to_client();
  } else {
    discard_current_msg(src_channel, src_protocol);
    return Result::Again;
  }
}

stdx::expected<Processor::Result, std::error_code> ChangeUserSender::error() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::Error>(
          src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("change_user::error: " + msg_res->message()));

  connection()->authenticated(false);

  stage(Stage::Done);
  if (in_handshake_) {
    // forward the error to the client.
    return forward_server_to_client();
  } else {
    discard_current_msg(src_channel, src_protocol);
    return Result::Again;
  }
}
