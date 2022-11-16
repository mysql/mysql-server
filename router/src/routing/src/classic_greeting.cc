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

#include "classic_greeting.h"

#include <cctype>
#include <iostream>
#include <random>  // uniform_int_distribution
#include <system_error>

#include "classic_auth.h"
#include "classic_auth_caching_sha2.h"
#include "classic_auth_cleartext.h"
#include "classic_auth_forwarder.h"
#include "classic_auth_native.h"
#include "classic_auth_sha256_password.h"
#include "classic_change_user.h"
#include "classic_connect.h"
#include "classic_connection.h"
#include "classic_forwarder.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "harness_assert.h"
#include "hexify.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql-server error-codes
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/connection_base.h"
#include "openssl_version.h"
#include "processor.h"
#include "tracer.h"

using namespace std::string_literals;

IMPORT_LOG_FUNCTIONS()

using mysql_harness::hexify;

using namespace std::string_view_literals;

static constexpr const std::array supported_authentication_methods{
    AuthCachingSha2Password::kName,
    AuthNativePassword::kName,
    AuthCleartextPassword::kName,
    AuthSha256Password::kName,
};

static constexpr const bool kCapturePlaintextPassword{true};

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

stdx::expected<Processor::Result, std::error_code> ClientGreetor::process() {
  switch (stage()) {
    case Stage::Init:
      return init();
    case Stage::ServerGreeting:
      return server_greeting();
    case Stage::ServerFirstGreeting:
      return server_first_greeting();
    case Stage::ClientGreeting:
      return client_greeting();
    case Stage::TlsAcceptInit:
      return tls_accept_init();
    case Stage::TlsAccept:
      return tls_accept();
    case Stage::ClientGreetingAfterTls:
      return client_greeting_after_tls();
    case Stage::RequestPlaintextPassword:
      return request_plaintext_password();
    case Stage::PlaintextPassword:
      return plaintext_password();

    case Stage::Accepted:
      return accepted();

    case Stage::Authenticated:
      return authenticated();

      // the two exit-stages:
      // - Error
      // - Ok
    case Stage::Error:
      return error();
    case Stage::Ok:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> ClientGreetor::error() {
  // after the greetings error has been sent to the client.
  trace(Tracer::Event().stage("client::greeting::error"));

  auto &client_conn = connection()->socket_splicer()->client_conn();

  (void)client_conn.cancel();
  (void)client_conn.shutdown(net::socket_base::shutdown_both);

  return Result::Done;
}

stdx::expected<Processor::Result, std::error_code> ClientGreetor::init() {
  trace(Tracer::Event().stage("client::init"));

  if (!connection()->greeting_from_router()) {
    stage(Stage::ServerFirstGreeting);

    connection()->push_processor(
        std::make_unique<ServerFirstConnector>(connection()));
  } else {
    stage(Stage::ServerGreeting);
  }
  return Result::Again;
}

/**
 * client<-router: server::greeting.
 */
stdx::expected<Processor::Result, std::error_code>
ClientGreetor::server_greeting() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = connection()->client_protocol();

  classic_protocol::capabilities::value_type router_capabilities(
      classic_protocol::capabilities::long_password |
      classic_protocol::capabilities::found_rows |
      classic_protocol::capabilities::long_flag |
      classic_protocol::capabilities::connect_with_schema |
      classic_protocol::capabilities::no_schema |
      // compress (not yet)
      classic_protocol::capabilities::odbc |
      classic_protocol::capabilities::local_files |
      // ignore_space (client only)
      classic_protocol::capabilities::protocol_41 |
      classic_protocol::capabilities::interactive |
      // ssl (below)
      // ignore sigpipe (client-only)
      classic_protocol::capabilities::transactions |
      classic_protocol::capabilities::secure_connection |
      classic_protocol::capabilities::multi_statements |
      classic_protocol::capabilities::multi_results |
      classic_protocol::capabilities::ps_multi_results |
      classic_protocol::capabilities::plugin_auth |
      classic_protocol::capabilities::connect_attributes |
      classic_protocol::capabilities::client_auth_method_data_varint |
      classic_protocol::capabilities::expired_passwords |
      classic_protocol::capabilities::session_track |
      classic_protocol::capabilities::text_result_with_session_tracking |
      classic_protocol::capabilities::optional_resultset_metadata
      // compress_zstd (not yet)
  );

  if (connection()->source_ssl_mode() != SslMode::kDisabled) {
    router_capabilities.set(classic_protocol::capabilities::pos::ssl);
  }

  dst_protocol->server_capabilities(router_capabilities);

  auto random_auth_method_data = []() {
    std::random_device rd;
    std::mt19937 gen(rd());
    // 1..255 ... no \0 chars
    std::uniform_int_distribution<> distrib(1, 255);

    std::string scramble;
    scramble.resize(20 + 1);  // 20 random data + [trailing, explicit \0]

    for (size_t n{}; n < scramble.size() - 1; ++n) {
      scramble[n] = distrib(gen);
    }

    return scramble;
  };

  auto server_greeting_version = []() {
    using namespace std::string_literals;

    return MYSQL_ROUTER_VERSION "-router"s;
  };

  classic_protocol::message::server::Greeting server_greeting_msg{
      10,                                           // protocol
      server_greeting_version(),                    // version
      0,                                            // connection-id
      random_auth_method_data(),                    // auth-method-data
      dst_protocol->server_capabilities(),          // server-caps
      255,                                          // 8.0.20 sends 0xff here
      classic_protocol::status::autocommit,         // status-flags
      std::string(AuthCachingSha2Password::kName),  // auth-method-name
  };

  auto send_res =
      ClassicFrame::send_msg(dst_channel, dst_protocol, server_greeting_msg,
                             {/* no shared caps yet */});
  if (!send_res) return send_client_failed(send_res.error());

  trace(Tracer::Event().stage("server::greeting"));

  dst_protocol->auth_method_data(server_greeting_msg.auth_method_data());
  dst_protocol->server_greeting(server_greeting_msg);

  stage(Stage::ClientGreeting);
  return Result::SendToClient;
}

/**
 * client<-router: server::greeting.
 */
stdx::expected<Processor::Result, std::error_code>
ClientGreetor::server_first_greeting() {
  auto *socket_splicer = connection()->socket_splicer();

  // ServerFirstGreetor either
  // - sent the server-greeting to the client and
  //   left the server connection open, or
  // - sent the error to the client and
  //   closed the connection.

  auto &server_conn = socket_splicer->server_conn();

  if (server_conn.is_open()) {
    stage(Stage::ClientGreeting);
  } else {
    stage(Stage::Error);
  }

  return Result::Again;
}

static void adjust_supported_capabilities(
    SslMode source_ssl_mode, SslMode dest_ssl_mode,
    classic_protocol::capabilities::value_type &caps) {
  // don't modify caps on passthrough.
  if (source_ssl_mode == SslMode::kPassthrough) return;

  // disable compression as we don't support it yet.
  caps.reset(classic_protocol::capabilities::pos::compress);
  caps.reset(classic_protocol::capabilities::pos::compress_zstd);
  caps.reset(classic_protocol::capabilities::pos::query_attributes);

  switch (source_ssl_mode) {
    case SslMode::kDisabled:
      // server supports SSL, but client should be forced to be unencrypted.
      //
      // disabling will pretend the server doesn't speak SSL
      //
      // if the client uses SslMode::kPreferred or kDisabled, it will use an
      // unencrypted connection otherwise it will abort the connection.
      caps.reset(classic_protocol::capabilities::pos::ssl);
      break;
    case SslMode::kRequired:
      // config requires: client MUST be encrypted.
      //
      // if the server hasn't set it yet, set it.
      caps.set(classic_protocol::capabilities::pos::ssl);
      break;
    case SslMode::kPreferred:
      // force-set the ssl-cap for the client-side only if we later don't have
      // to use AS_CLIENT when speaking to a non-TLS server.
      if (dest_ssl_mode != SslMode::kAsClient) {
        caps.set(classic_protocol::capabilities::pos::ssl);
      }
      break;
    default:
      break;
  }
}

static bool client_ssl_mode_is_satisfied(
    SslMode client_ssl_mode,
    classic_protocol::capabilities::value_type shared_capabilities) {
  if ((client_ssl_mode == SslMode::kRequired) &&
      !shared_capabilities.test(classic_protocol::capabilities::pos::ssl)) {
    return false;
  }

  return true;
}

static stdx::expected<size_t, std::error_code> send_ssl_connection_error_msg(
    Channel *dst_channel, ClassicProtocolState *dst_protocol,
    const std::string &msg) {
  return ClassicFrame::send_msg(
      dst_channel, dst_protocol,
      classic_protocol::message::server::Error{CR_SSL_CONNECTION_ERROR, msg});
}

/**
 * handle client greeting.
 */
stdx::expected<Processor::Result, std::error_code>
ClientGreetor::client_greeting() {
  auto *src_channel = connection()->socket_splicer()->client_channel();
  auto *src_protocol = connection()->client_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::client::Greeting>(
          src_channel, src_protocol, src_protocol->server_capabilities());
  if (!msg_res) return recv_client_failed(msg_res.error());

  auto msg = std::move(*msg_res);

  if (src_protocol->seq_id() != 1) {
    // client-greeting has seq-id 1
    return recv_client_failed(make_error_code(std::errc::bad_message));
  }

  trace(Tracer::Event().stage("client::greeting"));

  src_protocol->client_greeting(msg);
  src_protocol->client_capabilities(msg.capabilities());
  src_protocol->auth_method_name(msg.auth_method_name());
  src_protocol->username(msg.username());
  src_protocol->schema(msg.schema());
  src_protocol->attributes(msg.attributes());

  if (!client_ssl_mode_is_satisfied(connection()->source_ssl_mode(),
                                    src_protocol->shared_capabilities())) {
    // config says: client->router MUST be encrypted, but client didn't set
    // the SSL cap.
    //
    const auto send_res = send_ssl_connection_error_msg(
        src_channel, src_protocol,
        "SSL connection error: SSL is required from client");
    if (!send_res) return send_client_failed(send_res.error());

    stage(Stage::Error);
    return Result::SendToClient;
  }

  // remove the frame and message from the recv-buffer
  discard_current_msg(src_channel, src_protocol);

  if (!src_protocol->shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    // client wants to stay with plaintext

    if (msg.auth_method_data() == "\x00"sv) {
      // password is empty.
      src_protocol->password("");
    } else {
      const bool client_conn_is_secure =
          connection()->socket_splicer()->client_conn().is_secure_transport();

      if (client_conn_is_secure &&
          src_protocol->auth_method_name() == AuthCachingSha2Password::kName) {
        stage(Stage::RequestPlaintextPassword);
        return Result::Again;
      }
    }

    stage(Stage::Accepted);
    return Result::Again;
  } else if (connection()->source_ssl_mode() == SslMode::kPassthrough) {
    stage(Stage::Accepted);
    return Result::Again;
  } else {
    stage(Stage::TlsAcceptInit);
    return Result::Again;
  }
}

stdx::expected<Processor::Result, std::error_code>
ClientGreetor::tls_accept_init() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();

  src_channel->is_tls(true);

  auto *ssl_ctx = connection()->context().source_ssl_ctx()->get();
  // tls <-> (any)
  if (ssl_ctx == nullptr) {
    // shouldn't happen. But if it does, close the connection.
    log_warning("failed to create SSL_CTX");
    return recv_client_failed(make_error_code(std::errc::invalid_argument));
  }
  src_channel->init_ssl(ssl_ctx);

  stage(Stage::TlsAccept);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> ClientGreetor::tls_accept() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *client_channel = socket_splicer->client_channel();

  if (!client_channel->tls_init_is_finished()) {
    trace(Tracer::Event().stage("tls::accept"));

    auto res = socket_splicer->tls_accept();
    if (!res) {
      const auto ec = res.error();

      // the send-buffer contains an alert message telling the client why the
      // accept failed.
      if (!client_channel->send_buffer().empty()) {
        if (ec != TlsErrc::kWantRead) {
          log_debug("tls-accept failed: %s", ec.message().c_str());

          stage(Stage::Error);
        }
        return Result::SendToClient;
      }

      if (ec == TlsErrc::kWantRead) return Result::RecvFromClient;

      log_fatal_error_code("tls-accept failed", ec);

      return recv_client_failed(ec);
    }
  }

  stage(Stage::ClientGreetingAfterTls);

  // after tls_accept() there may still be data in the send-buffer that must
  // be sent.
  if (!client_channel->send_buffer().empty()) {
    return Result::SendToClient;
  }
  // TLS is accepted, more client greeting should follow.

  return Result::Again;
}

/**
 * check if the authentication method is supported.
 *
 * @see supported_authentication_methods
 *
 * @retval true auth_method_name is supported
 * @retval false auth_method_name is not supported
 */
static bool authentication_method_is_supported(
    const std::string &auth_method_name) {
  auto it = std::find(supported_authentication_methods.begin(),
                      supported_authentication_methods.end(), auth_method_name);
  return it != supported_authentication_methods.end();
}

static bool client_compress_is_satisfied(
    classic_protocol::capabilities::value_type client_capabilities,
    classic_protocol::capabilities::value_type shared_capabilities) {
  // client enabled "zlib-compress" without checking the server's caps.
  //
  // fail the connect.
  if (client_capabilities.test(classic_protocol::capabilities::pos::compress) &&
      !shared_capabilities.test(
          classic_protocol::capabilities::pos::compress)) {
    return false;
  }

  return true;
}

stdx::expected<Processor::Result, std::error_code>
ClientGreetor::client_greeting_after_tls() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::client::Greeting>(
          src_channel, src_protocol, src_protocol->server_capabilities());
  if (!msg_res) return recv_client_failed(msg_res.error());

  auto msg = std::move(*msg_res);

  trace(Tracer::Event().stage("client::greeting"));

  src_protocol->client_greeting(msg);
  src_protocol->auth_method_name(msg.auth_method_name());
  src_protocol->client_capabilities(msg.capabilities());
  src_protocol->username(msg.username());
  src_protocol->schema(msg.schema());
  src_protocol->attributes(msg.attributes());

  discard_current_msg(src_channel, src_protocol);

  if (!authentication_method_is_supported(msg.auth_method_name())) {
    trace(Tracer::Event().stage("client::greeting::error"));

    const auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::Error>(
            src_channel, src_protocol,
            {CR_AUTH_PLUGIN_CANNOT_LOAD,
             "Authentication method " + msg.auth_method_name() +
                 " is not supported",
             "HY000"});
    if (!send_res) return send_client_failed(send_res.error());

    stage(Stage::Error);
    return Result::SendToClient;
  }

  // fail connection from buggy clients that set the compress-cap without
  // checking if the server's capabilities.
  if (!client_compress_is_satisfied(src_protocol->client_capabilities(),
                                    src_protocol->shared_capabilities())) {
    trace(Tracer::Event().stage("client::greeting::error"));
    const auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::Error>(
            src_channel, src_protocol,
            {ER_WRONG_COMPRESSION_ALGORITHM_CLIENT,
             "Compression not supported by router."});
    if (!send_res) return send_client_failed(send_res.error());

    stage(Stage::Error);
    return Result::SendToClient;
  }

  if (src_protocol->client_greeting()->auth_method_data() == "\x00"sv) {
    // special value for 'empty password'. Not scrambled.
    src_protocol->password("");

    stage(Stage::Accepted);
    return Result::Again;
  } else if (kCapturePlaintextPassword && src_protocol->auth_method_name() ==
                                              AuthCachingSha2Password::kName) {
    stage(Stage::RequestPlaintextPassword);
    return Result::Again;
  } else {
    stage(Stage::Accepted);
    return Result::Again;
  }
}

stdx::expected<Processor::Result, std::error_code>
ClientGreetor::request_plaintext_password() {
  auto *socket_splicer = connection()->socket_splicer();

  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = connection()->client_protocol();

  auto send_res = AuthCachingSha2Password::send_plaintext_password_request(
      dst_channel, dst_protocol);
  if (!send_res) return send_client_failed(send_res.error());

  trace(Tracer::Event().stage("server::auth::request::plain"));

  stage(Stage::PlaintextPassword);
  return Result::SendToClient;
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
 * extract the password from auth-method-data.
 *
 * @returns the payload without the trailing NUL-char.
 * @retval false in there is no password.
 */
static std::optional<std::string> password_from_auth_method_data(
    std::string auth_data) {
  if (auth_data.empty() || auth_data.back() != '\0') return std::nullopt;

  // strip the trailing \0
  auth_data.pop_back();

  return auth_data;
}

/**
 * receive the client's plaintext password.
 *
 * after client_send_request_for_plaintext_password()
 */
stdx::expected<Processor::Result, std::error_code>
ClientGreetor::plaintext_password() {
  auto *src_channel = connection()->socket_splicer()->client_channel();
  auto *src_protocol = connection()->client_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::wire::String>(
      src_channel, src_protocol);
  if (!msg_res) return recv_client_failed(msg_res.error());

  trace(Tracer::Event().stage("client::auth::plain"));

  if (auto pwd = password_from_auth_method_data(msg_res->value())) {
    src_protocol->password(*pwd);
  }

  // discard the current frame.
  discard_current_msg(src_channel, src_protocol);

  stage(Stage::Accepted);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> ClientGreetor::accepted() {
  trace(Tracer::Event().stage("client::greeting::client_done"));

  auto *dst_protocol = connection()->server_protocol();

  stage(Stage::Authenticated);

  if (dst_protocol->server_greeting().has_value()) {
    // server-greeting is already present.
    connection()->push_processor(
        std::make_unique<ServerFirstAuthenticator>(connection()));
  } else {
    // server side requires TLS?

    auto dest_ssl_mode = connection()->dest_ssl_mode();
    auto source_ssl_mode = connection()->source_ssl_mode();

    // if a connection is taken from the pool, make sure it matches the TLS
    // requirements.
    connection()->requires_tls(dest_ssl_mode == SslMode::kRequired ||
                               dest_ssl_mode == SslMode::kPreferred ||
                               (dest_ssl_mode == SslMode::kAsClient &&
                                (source_ssl_mode == SslMode::kPreferred ||
                                 source_ssl_mode == SslMode::kRequired)));

    connection()->push_processor(
        std::make_unique<LazyConnector>(connection(), true /* in handshake */));
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ClientGreetor::authenticated() {
  if (connection()->authenticated()) {
    trace(Tracer::Event().stage("greeting::auth::done"));
    stage(Stage::Ok);
  } else {
    trace(Tracer::Event().stage("greeting::error"));
    stage(Stage::Error);
  }
  return Result::Again;
}

static bool server_ssl_mode_is_satisfied(
    SslMode server_ssl_mode,
    classic_protocol::capabilities::value_type server_capabilities) {
  if ((server_ssl_mode == SslMode::kRequired) &&
      !server_capabilities.test(classic_protocol::capabilities::pos::ssl)) {
    return false;
  }

  return true;
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

// server-side handshake

stdx::expected<Processor::Result, std::error_code> ServerGreetor::process() {
  switch (stage()) {
    case Stage::ServerGreeting:
      return server_greeting();
    case Stage::ServerGreetingError:
      return server_greeting_error();
    case Stage::ServerGreetingGreeting:
      return server_greeting_greeting();
    case Stage::ClientGreeting:
      return client_greeting();
    case Stage::ClientGreetingStartTls:
      return client_greeting_start_tls();
    case Stage::ClientGreetingFull:
      return client_greeting_full();
    case Stage::TlsConnectInit:
      return tls_connect_init();
    case Stage::TlsConnect:
      return tls_connect();
    case Stage::ClientGreetingAfterTls:
      return client_greeting_after_tls();
    case Stage::InitialResponse:
      return initial_response();
    case Stage::FinalResponse:
      return final_response();
    case Stage::AuthError:
      return auth_error();
    case Stage::AuthOk:
      return auth_ok();

      // the exit-stages
    case Stage::Error:
      return error();
    case Stage::ServerGreetingSent:
      return Result::Done;
    case Stage::Ok:
      connection()->authenticated(true);
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

// error has been sent to the client.
stdx::expected<Processor::Result, std::error_code> ServerGreetor::error() {
  auto *socket_splicer = connection()->socket_splicer();

  // ConnectProcessor either:
  //
  // - closes the connection and sends an error to the client, or
  // - keeps the connection open.
  auto &server_conn = socket_splicer->server_conn();

  (void)server_conn.close();

  return Result::Done;
}

stdx::expected<Processor::Result, std::error_code>
ServerGreetor::server_greeting() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_server_failed(read_res.error());

  uint8_t msg_type = src_protocol->current_msg_type().value();

  namespace message = classic_protocol::message;

  if (msg_type == ClassicFrame::cmd_byte<message::server::Error>()) {
    stage(Stage::ServerGreetingError);
  } else {
    stage(Stage::ServerGreetingGreeting);
  }
  return Result::Again;
}

/**
 * received an server::error from the server.
 *
 * forward it to the client and close the connection.
 */
stdx::expected<Processor::Result, std::error_code>
ServerGreetor::server_greeting_error() {
  trace(Tracer::Event().stage("server::greeting::error"));

  // don't increment the error-counter
  connection()->client_greeting_sent(true);

  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::Error>(
          src_channel, src_protocol);
  if (!msg_res) return recv_client_failed(msg_res.error());

  auto msg = *msg_res;

  // RouterRoutingTest.RoutingTooManyServerConnections expects this
  // message.
  log_debug(
      "Error from the server while waiting for greetings message: "
      "%u, '%s'",
      msg.error_code(), msg.message().c_str());

  stage(Stage::Error);  // forward the packet and close the connection.

  return forward_server_to_client();
}

// called after server connection is established.
void ServerGreetor::client_greeting_server_adjust_caps(
    ClassicProtocolState *src_protocol, ClassicProtocolState *dst_protocol) {
  auto client_caps = src_protocol->client_capabilities();

  if (!src_protocol->shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    auto attrs_res = classic_proto_decode_and_add_connection_attributes(
        src_protocol->attributes(), connection()
                                        ->socket_splicer()
                                        ->client_conn()
                                        .initial_connection_attributes());

    // client hasn't set the SSL cap, this is the real client greeting
    auto attrs = attrs_res.value_or(src_protocol->attributes());

    dst_protocol->sent_attributes(attrs);
    src_protocol->sent_attributes(attrs);

    auto client_greeting_msg = src_protocol->client_greeting().value();
    client_greeting_msg.attributes(attrs);
    dst_protocol->client_greeting(client_greeting_msg);
  }

  switch (connection()->dest_ssl_mode()) {
    case SslMode::kDisabled:
      // config says: communication to server is unencrypted
      client_caps.reset(classic_protocol::capabilities::pos::ssl);
      break;
    case SslMode::kRequired:
      // config says: communication to server must be encrypted
      client_caps.set(classic_protocol::capabilities::pos::ssl);
      break;
    case SslMode::kPreferred:
      // config says: communication to server should be encrypted if server
      // supports it.
      if (dst_protocol->server_capabilities().test(
              classic_protocol::capabilities::pos::ssl)) {
        client_caps.set(classic_protocol::capabilities::pos::ssl);
      }
      break;
    case SslMode::kAsClient:
      break;
    case SslMode::kPassthrough:
    case SslMode::kDefault:
      harness_assert_this_should_not_execute();
      break;
  }
  dst_protocol->client_capabilities(client_caps);
}

/**
 * received a server::greeting from the server.
 *
 * decode it.
 */
stdx::expected<Processor::Result, std::error_code>
ServerGreetor::server_greeting_greeting() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = connection()->client_protocol();

  const auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::Greeting>(
          src_channel, src_protocol, {/* no shared caps yet */});
  if (!msg_res) return msg_res.get_unexpected();

#if defined(DEBUG_STATE)
  log_debug("client-ssl-mode=%s, server-ssl-mode=%s",
            ssl_mode_to_string(source_ssl_mode()),
            ssl_mode_to_string(dest_ssl_mode()));
#endif

  auto server_greeting_msg = *msg_res;

  auto caps = server_greeting_msg.capabilities();

  src_protocol->server_capabilities(caps);
  src_protocol->server_greeting(server_greeting_msg);

  trace(Tracer::Event().stage("server::greeting::greeting"));

  auto msg = src_protocol->server_greeting().value();

#if 0
  std::cerr << __LINE__ << ": proto-version: " << (int)msg.protocol_version()
            << "\n";
  std::cerr << __LINE__ << ": caps: " << msg.capabilities() << "\n";
  std::cerr << __LINE__ << ": auth-method-name: " << msg.auth_method_name()
            << "\n";
  std::cerr << __LINE__ << ": auth-method-data:\n"
            << hexify(msg.auth_method_data()) << "\n";
  std::cerr << __LINE__ << ": status-flags: " << msg.status_flags() << "\n";
#endif

  if (!server_ssl_mode_is_satisfied(connection()->dest_ssl_mode(),
                                    src_protocol->server_capabilities())) {
    discard_current_msg(src_channel, src_protocol);

    // destination does not support TLS, but config requires encryption.
    log_debug(
        "server_ssl_mode=REQUIRED, but destination doesn't support "
        "encryption.");
    auto send_res = send_ssl_connection_error_msg(
        dst_channel, dst_protocol,
        "SSL connection error: SSL is required by router, but the "
        "server doesn't support it");
    if (!send_res) {
      auto ec = send_res.error();
      log_fatal_error_code("sending error-msg failed", ec);

      return send_client_failed(ec);
    }

    stage(Stage::Error);
    return Result::SendToClient;
  }

  // the server side's auth-method-data
  src_protocol->auth_method_data(msg.auth_method_data());

  if (!dst_protocol->server_greeting()) {
    discard_current_msg(src_channel, src_protocol);
    // client doesn't have server greeting yet, send it the server's.

    auto caps = src_protocol->server_capabilities();

    adjust_supported_capabilities(connection()->source_ssl_mode(),
                                  connection()->dest_ssl_mode(), caps);

    // update the client side's auth-method-data.
    dst_protocol->auth_method_data(msg.auth_method_data());
    dst_protocol->server_capabilities(caps);
    dst_protocol->seq_id(0xff);  // will be incremented by 1

    msg.capabilities(caps);

    auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::Greeting>(
            dst_channel, dst_protocol, msg);
    if (!send_res) return send_client_failed(send_res.error());

    stage(Stage::ServerGreetingSent);  // hand over to the ServerFirstConnector
    return Result::SendToClient;
  } else {
    discard_current_msg(src_channel, src_protocol);

    stage(Stage::ClientGreeting);
    return Result::Again;
  }
}

stdx::expected<Processor::Result, std::error_code>
ServerGreetor::client_greeting() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();
  auto *dst_protocol = connection()->server_protocol();

  bool server_supports_tls = dst_protocol->server_capabilities().test(
      classic_protocol::capabilities::pos::ssl);
  bool client_uses_tls = src_protocol->shared_capabilities().test(
      classic_protocol::capabilities::pos::ssl);

  if (connection()->dest_ssl_mode() == SslMode::kAsClient && client_uses_tls &&
      !server_supports_tls) {
    // config says: do as the client did, and the client did SSL and server
    // doesn't support it -> error

    // send back to the client
    const auto send_res = send_ssl_connection_error_msg(
        src_channel, src_protocol,
        "SSL connection error: Requirements can not be satisfied");
    if (!send_res) return send_client_failed(send_res.error());

    stage(Stage::Error);
    return Result::SendToClient;
  }

  client_greeting_server_adjust_caps(src_protocol, dst_protocol);

  // use the client-side's capabilities to make sure the server encodes
  // the packets according to the client.
  //
  // src_protocol->shared_caps must be used here as the ->client_caps may
  // contain more than what the router advertised.
  auto client_caps = src_protocol->shared_capabilities();

  switch (connection()->dest_ssl_mode()) {
    case SslMode::kDisabled:
      client_caps.reset(classic_protocol::capabilities::pos::ssl);
      break;
    case SslMode::kPreferred:
      client_caps.set(classic_protocol::capabilities::pos::ssl,
                      server_supports_tls);
      break;
    case SslMode::kRequired:
      client_caps.set(classic_protocol::capabilities::pos::ssl);
      break;
    case SslMode::kAsClient:
      client_caps.set(classic_protocol::capabilities::pos::ssl,
                      client_uses_tls);
      break;
    case SslMode::kPassthrough:
      // don't check caps on passthrough.
      break;
    case SslMode::kDefault:
      log_debug("dest_ssl_mode::Default ... should not happen.");

      return recv_client_failed(make_error_code(std::errc::invalid_argument));
  }

  // ensure that "with_schema" cap is set when sending a schema to the server.
  //
  // if the client didn't sent a schema initially, the connect-with-schema cap
  // will not be part of the client's caps.
  const auto with_schema_pos =
      classic_protocol::capabilities::pos::connect_with_schema;
  if (src_protocol->schema().empty()) {
    client_caps.reset(with_schema_pos);
  } else {
    client_caps.set(with_schema_pos);
  }

  dst_protocol->client_capabilities(client_caps);
  dst_protocol->auth_method_name(src_protocol->auth_method_name());
  dst_protocol->username(src_protocol->username());
  dst_protocol->attributes(src_protocol->attributes());

  // the client greeting was received and will be forwarded to the server
  // soon.
  connection()->client_greeting_sent(true);
  connection()->on_handshake_received();

  if (dst_protocol->shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    stage(Stage::ClientGreetingStartTls);
  } else {
    stage(Stage::ClientGreetingFull);
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerGreetor::client_greeting_start_tls() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_protocol = connection()->client_protocol();
  auto *dst_protocol = connection()->server_protocol();
  auto *dst_channel = socket_splicer->server_channel();

  if (!src_protocol->client_greeting()) {
    return send_server_failed(make_error_code(std::errc::invalid_argument));
  }

  auto initial_client_greeting_msg = src_protocol->client_greeting().value();

  // setting username == "" leads to a short, switch-to-ssl
  // client::Greeting.
  auto send_res =
      ClassicFrame::send_msg<classic_protocol::message::client::Greeting>(
          dst_channel, dst_protocol,
          {
              dst_protocol->client_capabilities(),
              initial_client_greeting_msg.max_packet_size(),
              initial_client_greeting_msg.collation(),
              "",  // username
              "",  // auth_method_data
              "",  // schema
              "",  // auth_method_name
              ""   // attributes
          });
  if (!send_res) return send_server_failed(send_res.error());

  trace(Tracer::Event().stage("client::greeting (start-tls)"));

  stage(Stage::TlsConnectInit);

  // leave msg in the send buffer as tls_connect() will flush it.

  // Result::SendToServer
  //       2041 us (      +115 us)      r<-s io::recv
  //       2044 us (        +2 us)           server::greeting?
  //       2049 us (        +5 us)           server::greeting::greeting
  //       2056 us (        +6 us)           client::greeting
  //       2068 us (       +12 us)      r->s io::send  << this one
  //       2233 us (      +164 us)           tls::connect
  //       2249 us (       +16 us)      r->s io::send

  // Result::Again
  //       2005 us (      +138 us)      r<-s io::recv
  //       2008 us (        +2 us)           server::greeting?
  //       2014 us (        +6 us)           server::greeting::greeting
  //       2021 us (        +6 us)           client::greeting
  //       2090 us (       +68 us)           tls::connect
  //       2113 us (       +23 us)      r->s io::send
  return Result::Again;
}

/**
 * send a non-TLS client greeting to the server.
 */
stdx::expected<Processor::Result, std::error_code>
ServerGreetor::client_greeting_full() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();
  auto *dst_channel = socket_splicer->server_channel();
  auto *dst_protocol = connection()->server_protocol();

  auto client_greeting_msg = src_protocol->client_greeting().value();

  const auto attrs_res = classic_proto_decode_and_add_connection_attributes(
      src_protocol->attributes(),
      vector_splice(
          socket_splicer->client_conn().initial_connection_attributes(),
          client_ssl_connection_attributes(src_channel->ssl())));
  if (!attrs_res) {
    auto ec = attrs_res.error();
    // if decode/append fails forward the attributes as is. The server should
    // fail too.
    //
    if (src_protocol->client_capabilities().test(
            classic_protocol::capabilities::pos::connect_attributes)) {
      log_warning("%d: decoding connection attributes failed [ignored]: (%s) ",
                  __LINE__, ec.message().c_str());
    }
  }

  client_greeting_msg.capabilities(dst_protocol->client_capabilities());
  client_greeting_msg.username(src_protocol->username());
  client_greeting_msg.schema(src_protocol->schema());

  auto attrs = attrs_res.value_or(src_protocol->attributes());
  dst_protocol->sent_attributes(attrs);
  src_protocol->sent_attributes(attrs);

  client_greeting_msg.attributes(attrs);

  trace(Tracer::Event().stage("client::greeting::plain"));

  if (src_protocol->password().has_value()) {
    // scramble with the server's auth-data to trigger a fast-auth.

    auto pwd = *(src_protocol->password());

    // if the password set and not empty, rehash it.
    if (!pwd.empty()) {
      if (auto scramble_res = scramble_them_all(
              client_greeting_msg.auth_method_name(),
              strip_trailing_null(
                  dst_protocol->server_greeting()->auth_method_data()),
              pwd)) {
        client_greeting_msg.auth_method_data(*scramble_res);
      }
    }
  } else if (src_protocol->auth_method_name() ==
                 AuthCachingSha2Password::kName &&
             !src_channel->ssl() && connection()->greeting_from_router()) {
    // the client tried the fast-auth path and scrambled it with the router's
    // nonce.
    //
    // That will fail on the server side as it used another scramble.
    //
    // replace the auth-method-method to force a "auth-method-switch" which
    // contains the server's nonce.
    client_greeting_msg.auth_method_name("switch_me_if_you_can");
  } else {
    dst_protocol->auth_method_name(src_protocol->auth_method_name());
  }

  return ClassicFrame::send_msg(dst_channel, dst_protocol, client_greeting_msg)
      .and_then(
          [this](auto /* sent */) -> stdx::expected<Result, std::error_code> {
            stage(Stage::InitialResponse);

            return Result::SendToServer;
          })
      .or_else([this](auto err) { return send_server_failed(err); });
}

static stdx::expected<SSL_CTX *, std::error_code> get_dest_ssl_ctx(
    MySQLRoutingContext &ctx, const std::string &id) {
  return mysql_harness::make_tcp_address(id).and_then(
      [&ctx](const auto &addr) -> stdx::expected<SSL_CTX *, std::error_code> {
        return ctx.dest_ssl_ctx(addr.address())->get();
      });
}

stdx::expected<Processor::Result, std::error_code>
ServerGreetor::tls_connect_init() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *dst_channel = socket_splicer->server_channel();

  auto ssl_ctx_res = get_dest_ssl_ctx(connection()->context(),
                                      connection()->get_destination_id());
  if (!ssl_ctx_res || ssl_ctx_res.value() == nullptr) {
    // shouldn't happen. But if it does, close the connection.
    log_warning("failed to create SSL_CTX");

    return send_server_failed(make_error_code(std::errc::invalid_argument));
  }
  dst_channel->init_ssl(*ssl_ctx_res);

  // when a connection is taken from the pool for this client-connection, make
  // sure it is TLS again.
  connection()->requires_tls(true);

  stage(Stage::TlsConnect);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerGreetor::tls_connect() {
  auto *socket_splicer = connection()->socket_splicer();

  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();
  auto *dst_channel = socket_splicer->server_channel();

  {
    const auto flush_res = dst_channel->flush_from_recv_buf();
    if (!flush_res) {
      auto ec = flush_res.error();
      log_fatal_error_code("tls_connect::recv::flush() failed", ec);

      return recv_server_failed(ec);
    }
  }

  if (!dst_channel->tls_init_is_finished()) {
    const auto res = dst_channel->tls_connect();

    trace(Tracer::Event().stage("tls::connect"));

    if (!res) {
      if (res.error() == TlsErrc::kWantRead) {
        {
          const auto flush_res = dst_channel->flush_to_send_buf();
          if (!flush_res &&
              (flush_res.error() !=
               make_error_condition(std::errc::operation_would_block))) {
            auto ec = flush_res.error();
            log_fatal_error_code("flushing failed", ec);

            return send_server_failed(ec);
          }
        }

        if (!dst_channel->send_buffer().empty()) {
          return Result::SendToServer;
        }

        return Result::RecvFromServer;
      } else {
        // connect may fail fatally if
        //
        // - cert-verification failed.
        // - no shared cipher

        const auto send_res = send_ssl_connection_error_msg(
            src_channel, src_protocol,
            "connecting to destination failed with TLS error: " +
                res.error().message());
        if (!send_res) {
          auto ec = send_res.error();
          log_fatal_error_code("sending error failed", ec);

          return send_client_failed(ec);
        }

        trace(Tracer::Event().stage("server::greeting::error"));

        stage(Stage::Error);
        return Result::SendToClient;
      }
    }
  }

  stage(Stage::ClientGreetingAfterTls);
  // tls is established to the server, send the client::greeting
  return Result::Again;
}

/**
 * a TLS client greeting.
 */
stdx::expected<Processor::Result, std::error_code>
ServerGreetor::client_greeting_after_tls() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();
  auto *dst_channel = socket_splicer->server_channel();
  auto *dst_protocol = connection()->server_protocol();

  auto client_greeting_msg = *src_protocol->client_greeting();

  const auto attrs_res = classic_proto_decode_and_add_connection_attributes(
      src_protocol->attributes(),
      vector_splice(
          socket_splicer->client_conn().initial_connection_attributes(),
          client_ssl_connection_attributes(src_channel->ssl())));
  if (!attrs_res) {
    auto ec = attrs_res.error();
    // if decode/append fails forward the attributes as is. The server should
    // fail too.
    //
    if (src_protocol->client_capabilities().test(
            classic_protocol::capabilities::pos::connect_attributes)) {
      log_warning("%d: decoding connection attributes failed [ignored]: (%s) ",
                  __LINE__, ec.message().c_str());
    }
  }

  dst_protocol->username(src_protocol->username());

  auto attrs = attrs_res.value_or(src_protocol->attributes());
  dst_protocol->sent_attributes(attrs);
  src_protocol->sent_attributes(attrs);

  client_greeting_msg.attributes(attrs);

  client_greeting_msg.username(src_protocol->username());
  client_greeting_msg.schema(src_protocol->schema());
  client_greeting_msg.capabilities(dst_protocol->client_capabilities());

  trace(Tracer::Event().stage("client::greeting (tls)"));

  if (src_protocol->password().has_value()) {
    // scramble with the server's auth-data to trigger a fast-auth.

    auto pwd = *(src_protocol->password());

    // if the password set and not empty, rehash it.
    if (!pwd.empty()) {
      if (auto scramble_res = scramble_them_all(
              client_greeting_msg.auth_method_name(),
              strip_trailing_null(
                  dst_protocol->server_greeting()->auth_method_data()),
              pwd)) {
        client_greeting_msg.auth_method_data(*scramble_res);
      }
    }
  } else if (src_protocol->auth_method_name() ==
                 AuthCachingSha2Password::kName &&
             !src_channel->ssl() && connection()->greeting_from_router()) {
    // the client tried the fast-auth path and scrambled it with the router's
    // nonce.
    //
    // That will fail on the server side as it used another scramble.
    //
    // replace the auth-method-method to force a "auth-method-switch" which
    // contains the server's nonce.
    client_greeting_msg.auth_method_name("switch_me_if_you_can");
  }

  dst_protocol->auth_method_name(src_protocol->auth_method_name());

  return ClassicFrame::send_msg(dst_channel, dst_protocol, client_greeting_msg)
      .and_then(
          [this](auto /* unused */) -> stdx::expected<Result, std::error_code> {
            stage(Stage::InitialResponse);

            return Result::SendToServer;
          })
      .or_else([this](auto err) { return send_server_failed(err); });
}

stdx::expected<Processor::Result, std::error_code>
ServerGreetor::initial_response() {
  connection()->push_processor(std::make_unique<AuthForwarder>(connection()));

  stage(Stage::FinalResponse);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerGreetor::final_response() {
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
      stage(Stage::AuthOk);
      return Result::Again;
    case Msg::Error:
      stage(Stage::AuthError);
      return Result::Again;
  }

  // if there is another packet, dump its payload for now.
  auto &recv_buf = src_channel->recv_plain_buffer();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);

  log_debug(
      "received unexpected message from server after a client::Greeting:\n%s",
      hexify(recv_buf).c_str());

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

/**
 * router<-server: auth error.
 */
stdx::expected<Processor::Result, std::error_code> ServerGreetor::auth_error() {
  trace(Tracer::Event().stage("server::auth::error"));

  stage(Stage::Error);

  if (in_handshake_) {
    return forward_server_to_client();
  }

  return Result::Again;
}

/**
 * server-side: auth is ok.
 */
stdx::expected<Processor::Result, std::error_code> ServerGreetor::auth_ok() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->server_channel();
  auto *src_protocol = connection()->server_protocol();
  auto *dst_protocol = connection()->client_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Ok>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("server::ok"));

  auto msg = std::move(*msg_res);

  if (!msg.session_changes().empty()) {
    (void)connection()->track_session_changes(
        net::buffer(msg.session_changes()),
        src_protocol->shared_capabilities());
  }

  // if the server accepted the schema, track it.
  if (src_protocol->shared_capabilities().test(
          classic_protocol::capabilities::pos::connect_with_schema)) {
    src_protocol->schema(dst_protocol->schema());
  } else {
    src_protocol->schema("");
  }

  stage(Stage::Ok);

  if (in_handshake_) {
    return forward_server_to_client();
  }

  discard_current_msg(src_channel, src_protocol);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstConnector::process() {
  switch (stage()) {
    case Stage::Connect:
      return connect();
    case Stage::ServerGreeting:
      return server_greeting();
    case Stage::ServerGreeted:
      return server_greeted();

      // the two exit-stages:
      // - Error
      // - Ok
    case Stage::Error:
    case Stage::Ok:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstConnector::connect() {
  stage(Stage::ServerGreeting);

  connection()->push_processor(
      std::make_unique<ConnectProcessor>(connection()));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstConnector::server_greeting() {
  auto *socket_splicer = connection()->socket_splicer();

  // ConnectProcessor either:
  //
  // - closes the connection and sends an error to the client, or
  // - keeps the connection open.
  auto &server_conn = socket_splicer->server_conn();

  if (!server_conn.is_open()) {
    trace(Tracer::Event().stage("connect::error"));

    stage(Stage::Error);

    return Result::Again;
  }

  trace(Tracer::Event().stage("server::greeting"));

  stage(Stage::ServerGreeted);

  connection()->push_processor(
      std::make_unique<ServerGreetor>(connection(), false));

  return Result::Again;
}

/**
 * received an server::greeting or server::error from the server.
 */
stdx::expected<Processor::Result, std::error_code>
ServerFirstConnector::server_greeted() {
  auto *socket_splicer = connection()->socket_splicer();

  auto &server_conn = socket_splicer->server_conn();

  if (!server_conn.is_open()) {
    stage(Stage::Error);
  } else {
    stage(Stage::Ok);
  }

  return Result::Again;
}

// server-side authentication

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::process() {
  switch (stage()) {
    case Stage::ClientGreeting:
      return client_greeting();
    case Stage::ClientGreetingStartTls:
      return client_greeting_start_tls();
    case Stage::ClientGreetingFull:
      return client_greeting_full();
    case Stage::TlsForwardInit:
      return tls_forward_init();
    case Stage::TlsForward:
      return tls_forward();
    case Stage::TlsConnectInit:
      return tls_connect_init();
    case Stage::TlsConnect:
      return tls_connect();
    case Stage::ClientGreetingAfterTls:
      return client_greeting_after_tls();
    case Stage::InitialResponse:
      return initial_response();
    case Stage::FinalResponse:
      return final_response();
    case Stage::AuthError:
      return auth_error();
    case Stage::AuthOk:
      return auth_ok();

      // the two exit-stages:
      // - Error
      // - Ok
    case Stage::Error:
      return Result::Done;
    case Stage::Ok:
      connection()->authenticated(true);
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

// called after server connection is established.
void ServerFirstAuthenticator::client_greeting_server_adjust_caps(
    ClassicProtocolState *src_protocol, ClassicProtocolState *dst_protocol) {
  auto client_caps = src_protocol->client_capabilities();

  if (!src_protocol->shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    auto client_greeting_msg = src_protocol->client_greeting().value();

    auto attrs_res = classic_proto_decode_and_add_connection_attributes(
        src_protocol->attributes(), connection()
                                        ->socket_splicer()
                                        ->client_conn()
                                        .initial_connection_attributes());

    auto attrs = attrs_res.value_or(src_protocol->attributes());
    dst_protocol->sent_attributes(attrs);
    src_protocol->sent_attributes(attrs);

    client_greeting_msg.attributes(attrs);

    // client hasn't set the SSL cap, this is the real client greeting
    dst_protocol->client_greeting(client_greeting_msg);
  }

  switch (connection()->dest_ssl_mode()) {
    case SslMode::kDisabled:
      // config says: communication to server is unencrypted
      client_caps.reset(classic_protocol::capabilities::pos::ssl);
      break;
    case SslMode::kRequired:
      // config says: communication to server must be encrypted
      client_caps.set(classic_protocol::capabilities::pos::ssl);
      break;
    case SslMode::kPreferred:
      // config says: communication to server should be encrypted if server
      // supports it.
      if (dst_protocol->server_capabilities().test(
              classic_protocol::capabilities::pos::ssl)) {
        client_caps.set(classic_protocol::capabilities::pos::ssl);
      }
      break;
    case SslMode::kAsClient:
      break;
    case SslMode::kPassthrough:
    case SslMode::kDefault:
      harness_assert_this_should_not_execute();
      break;
  }
  dst_protocol->client_capabilities(client_caps);
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::client_greeting() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();
  auto *dst_protocol = connection()->server_protocol();

  const bool server_supports_tls = dst_protocol->server_capabilities().test(
      classic_protocol::capabilities::pos::ssl);
  const bool client_uses_tls = src_protocol->shared_capabilities().test(
      classic_protocol::capabilities::pos::ssl);
  const bool client_is_secure =
      client_uses_tls || socket_splicer->client_conn().is_secure_transport();

  if (connection()->dest_ssl_mode() == SslMode::kAsClient && client_uses_tls &&
      !server_supports_tls) {
    // config says: do as the client did, and the client did SSL and server
    // doesn't support it -> error

    // send back to the client
    const auto send_res = send_ssl_connection_error_msg(
        src_channel, src_protocol,
        "SSL connection error: Requirements can not be satisfied");
    if (!send_res) return send_client_failed(send_res.error());

    stage(Stage::Error);
    return Result::SendToClient;
  }

  client_greeting_server_adjust_caps(src_protocol, dst_protocol);

  // use the client-side's capabilities to make sure the server encodes
  // the packets according to the client.
  //
  // src_protocol->shared_caps must be used here as the ->client_caps may
  // contain more than what the router advertised.
  auto client_caps = src_protocol->shared_capabilities();

  switch (connection()->dest_ssl_mode()) {
    case SslMode::kDisabled:
      client_caps.reset(classic_protocol::capabilities::pos::ssl);
      break;
    case SslMode::kPreferred:
      client_caps.set(classic_protocol::capabilities::pos::ssl,
                      server_supports_tls);
      break;
    case SslMode::kRequired:
      client_caps.set(classic_protocol::capabilities::pos::ssl);
      break;
    case SslMode::kAsClient:
      if (connection()->source_ssl_mode() != SslMode::kPassthrough) {
        // don't check caps on passthrough.
        client_caps.set(classic_protocol::capabilities::pos::ssl,
                        client_is_secure);
      }
      break;
    case SslMode::kPassthrough:
    case SslMode::kDefault:
      log_debug("dest_ssl_mode::Default ... should not happen.");

      return recv_client_failed(make_error_code(std::errc::invalid_argument));
  }

  dst_protocol->client_capabilities(client_caps);
  dst_protocol->auth_method_name(src_protocol->auth_method_name());
  dst_protocol->username(src_protocol->username());
  dst_protocol->attributes(src_protocol->attributes());

  // the client greeting was received and will be forwarded to the server
  // soon.
  connection()->client_greeting_sent(true);
  connection()->on_handshake_received();

  if (dst_protocol->shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    stage(Stage::ClientGreetingStartTls);

  } else {
    stage(Stage::ClientGreetingFull);
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::client_greeting_start_tls() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_protocol = connection()->client_protocol();
  auto *dst_protocol = connection()->server_protocol();
  auto *dst_channel = socket_splicer->server_channel();

  if (!src_protocol->client_greeting()) {
    return send_server_failed(make_error_code(std::errc::invalid_argument));
  }

  auto initial_client_greeting_msg = src_protocol->client_greeting().value();

  // use the shared capabilities of the client<->router connection as basis
  auto client_caps = src_protocol->shared_capabilities();

  client_caps.set(classic_protocol::capabilities::pos::ssl);

  dst_protocol->client_capabilities(client_caps);

  // setting username == "" leads to a short, switch-to-ssl
  // client::Greeting.
  auto send_res =
      ClassicFrame::send_msg<classic_protocol::message::client::Greeting>(
          dst_channel, dst_protocol,
          {
              client_caps, initial_client_greeting_msg.max_packet_size(),
              initial_client_greeting_msg.collation(),
              "",  // username
              "",  // auth_method_data
              "",  // schema
              "",  // auth_method_name
              ""   // attributes
          });
  if (!send_res) return send_server_failed(send_res.error());

  if (connection()->source_ssl_mode() == SslMode::kPassthrough) {
    trace(Tracer::Event().stage("client::greeting (forward-tls)"));

    stage(Stage::TlsForwardInit);
  } else {
    trace(Tracer::Event().stage("client::greeting (start-tls)"));

    stage(Stage::TlsConnectInit);

    // leave msg in the send buffer as tls_connect() will flush it.

    // Result::SendToServer
    //       2041 us (      +115 us)      r<-s io::recv
    //       2044 us (        +2 us)           server::greeting?
    //       2049 us (        +5 us)           server::greeting::greeting
    //       2056 us (        +6 us)           client::greeting
    //       2068 us (       +12 us)      r->s io::send  << this one
    //       2233 us (      +164 us)           tls::connect
    //       2249 us (       +16 us)      r->s io::send

    // Result::Again
    //       2005 us (      +138 us)      r<-s io::recv
    //       2008 us (        +2 us)           server::greeting?
    //       2014 us (        +6 us)           server::greeting::greeting
    //       2021 us (        +6 us)           client::greeting
    //       2090 us (       +68 us)           tls::connect
    //       2113 us (       +23 us)      r->s io::send
  }
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::client_greeting_full() {
  trace(Tracer::Event().stage("client::greeting (full)"));

  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();
  auto *dst_channel = socket_splicer->server_channel();
  auto *dst_protocol = connection()->server_protocol();

  auto client_greeting_msg = src_protocol->client_greeting().value();

  const auto attrs_res = classic_proto_decode_and_add_connection_attributes(
      src_protocol->attributes(),
      vector_splice(
          socket_splicer->client_conn().initial_connection_attributes(),
          client_ssl_connection_attributes(src_channel->ssl())));
  if (!attrs_res) {
    auto ec = attrs_res.error();
    // if decode/append fails forward the attributes as is. The server should
    // fail too.
    //
    if (src_protocol->client_capabilities().test(
            classic_protocol::capabilities::pos::connect_attributes)) {
      log_warning("%d: decoding connection attributes failed [ignored]: (%s) ",
                  __LINE__, ec.message().c_str());
    }
  }

  auto attrs = attrs_res.value_or(src_protocol->attributes());
  dst_protocol->sent_attributes(attrs);
  src_protocol->sent_attributes(attrs);

  client_greeting_msg.capabilities(dst_protocol->client_capabilities());
  client_greeting_msg.attributes(attrs);

  if (src_protocol->password().has_value()) {
    // scramble with the server's auth-data to trigger a fast-auth.

    auto pwd = *(src_protocol->password());

    // if the password set and not empty, rehash it.
    if (!pwd.empty()) {
      if (auto scramble_res = scramble_them_all(
              client_greeting_msg.auth_method_name(),
              strip_trailing_null(
                  dst_protocol->server_greeting()->auth_method_data()),
              pwd)) {
        client_greeting_msg.auth_method_data(*scramble_res);
      }
    }
  }

  dst_protocol->auth_method_name(src_protocol->auth_method_name());

  return ClassicFrame::send_msg(dst_channel, dst_protocol, client_greeting_msg)
      .and_then(
          [this](auto /* unused */) -> stdx::expected<Result, std::error_code> {
            stage(Stage::InitialResponse);

            return Result::SendToServer;
          })
      .or_else([this](auto err) { return send_server_failed(err); });
}

static TlsErrc forward_tls(Channel *src_channel, Channel *dst_channel) {
  auto &plain = src_channel->recv_plain_buffer();
  src_channel->read_to_plain(5);

  auto plain_buf = net::dynamic_buffer(plain);
  // at least the TLS record header.
  const size_t tls_header_size{5};
  while (plain_buf.size() >= tls_header_size) {
    // plain is TLS traffic.
    const uint8_t tls_content_type = plain[0];
    const uint16_t tls_payload_size = (plain[3] << 8) | plain[4];

#if defined(DEBUG_SSL)
    const uint16_t tls_legacy_version = (plain[1] << 8) | plain[2];

    log_debug("-- ssl: ver=%04x, len=%d, %s", tls_legacy_version,
              tls_payload_size,
              tls_content_type_to_string(
                  static_cast<TlsContentType>(tls_content_type))
                  .c_str());
#endif
    if (plain_buf.size() < tls_header_size + tls_payload_size) {
      src_channel->read_to_plain(tls_header_size + tls_payload_size -
                                 plain_buf.size());
    }

    if (plain_buf.size() < tls_header_size + tls_payload_size) {
      // there isn't the full frame yet.
      return TlsErrc::kWantRead;
    }

    const auto write_res = dst_channel->write(
        plain_buf.data(0, tls_header_size + tls_payload_size));
    if (!write_res) {
      return TlsErrc::kWantWrite;
    }

    // if TlsAlert in handshake, the connection goes back to plain
    if (static_cast<TlsContentType>(tls_content_type) ==
            TlsContentType::kAlert &&
        plain.size() >= 6 && plain[5] == 0x02) {
      src_channel->is_tls(false);
      dst_channel->is_tls(false);
    }
    plain_buf.consume(write_res.value());
  }

  // want more
  return TlsErrc::kWantRead;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::tls_forward() {
  auto *socket_splicer = connection()->socket_splicer();

  auto client_channel = socket_splicer->client_channel();
  auto server_channel = socket_splicer->server_channel();

  bool client_recv_buf_changed =
      client_last_recv_buf_size_ != client_channel->recv_buffer().size();
  bool server_recv_buf_changed =
      server_last_recv_buf_size_ != server_channel->recv_buffer().size();
  bool client_send_buf_changed =
      client_last_send_buf_size_ != client_channel->send_buffer().size();
  bool server_send_buf_changed =
      server_last_send_buf_size_ != server_channel->send_buffer().size();

  if (client_recv_buf_changed || server_send_buf_changed) {
    forward_tls(client_channel, server_channel);

    client_last_recv_buf_size_ = client_channel->recv_buffer().size();
    server_last_send_buf_size_ = server_channel->send_buffer().size();

    if (!server_channel->send_buffer().empty()) {
      return Result::SendToServer;
    }

    return Result::RecvFromClient;

  } else if (server_recv_buf_changed || client_send_buf_changed) {
    forward_tls(server_channel, client_channel);

    server_last_recv_buf_size_ = server_channel->recv_buffer().size();
    client_last_send_buf_size_ = client_channel->send_buffer().size();

    if (!client_channel->send_buffer().empty()) {
      return Result::SendToClient;
    }

    return Result::RecvFromServer;
  }

  return stdx::make_unexpected(make_error_code(std::errc::bad_message));
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::tls_forward_init() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *dst_channel = socket_splicer->server_channel();

  dst_channel->is_tls(true);
  src_channel->is_tls(true);

  // if there is already data in the recv-buffer, forward that.
  forward_tls(src_channel, dst_channel);
  if (!dst_channel->send_buffer().empty()) {
    return Result::SendToServer;
  }

  stage(Stage::TlsForward);
  return Result::RecvFromBoth;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::tls_connect_init() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *dst_channel = socket_splicer->server_channel();

  auto ssl_ctx_res = get_dest_ssl_ctx(connection()->context(),
                                      connection()->get_destination_id());
  if (!ssl_ctx_res || ssl_ctx_res.value() == nullptr) {
    // shouldn't happen. But if it does, close the connection.
    log_warning("failed to create SSL_CTX");

    return send_server_failed(make_error_code(std::errc::invalid_argument));
  }
  dst_channel->init_ssl(*ssl_ctx_res);

  connection()->requires_tls(true);

  stage(Stage::TlsConnect);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::tls_connect() {
  auto *socket_splicer = connection()->socket_splicer();

  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();
  auto *dst_channel = socket_splicer->server_channel();

  {
    const auto flush_res = dst_channel->flush_from_recv_buf();
    if (!flush_res) {
      auto ec = flush_res.error();
      log_fatal_error_code("tls_connect::recv::flush() failed", ec);

      return recv_server_failed(ec);
    }
  }

  if (!dst_channel->tls_init_is_finished()) {
    const auto res = dst_channel->tls_connect();

    trace(Tracer::Event().stage("tls::connect"));

    if (!res) {
      if (res.error() == TlsErrc::kWantRead) {
        {
          const auto flush_res = dst_channel->flush_to_send_buf();
          if (!flush_res &&
              (flush_res.error() !=
               make_error_condition(std::errc::operation_would_block))) {
            auto ec = flush_res.error();
            log_fatal_error_code("flushing failed", ec);

            return send_server_failed(ec);
          }
        }

        if (!dst_channel->send_buffer().empty()) {
          return Result::SendToServer;
        }
        return Result::RecvFromServer;
      } else {
        // connect may fail fatally if
        //
        // - cert-verification failed.
        // - no shared cipher

        const auto send_res = send_ssl_connection_error_msg(
            src_channel, src_protocol,
            "connecting to destination failed with TLS error: " +
                res.error().message());
        if (!send_res) {
          auto ec = send_res.error();
          log_fatal_error_code("sending error failed", ec);

          return send_client_failed(ec);
        }

        trace(Tracer::Event().stage("server::greeting::error"));

        stage(Stage::Error);
        return Result::SendToClient;
      }
    }
  }

  stage(Stage::ClientGreetingAfterTls);
  // tls is established to the server, send the client::greeting
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::client_greeting_after_tls() {
  trace(Tracer::Event().stage("client::greeting(first)"));

  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();
  auto *dst_channel = socket_splicer->server_channel();
  auto *dst_protocol = connection()->server_protocol();

  auto client_greeting_msg = *src_protocol->client_greeting();

  const auto attrs_res = classic_proto_decode_and_add_connection_attributes(
      src_protocol->attributes(),
      vector_splice(
          socket_splicer->client_conn().initial_connection_attributes(),
          client_ssl_connection_attributes(src_channel->ssl())));
  if (!attrs_res) {
    auto ec = attrs_res.error();
    // if decode/append fails forward the attributes as is. The server should
    // fail too.
    //
    if (src_protocol->client_capabilities().test(
            classic_protocol::capabilities::pos::connect_attributes)) {
      log_warning("%d: decoding connection attributes failed [ignored]: (%s) ",
                  __LINE__, ec.message().c_str());
    }
  }

  dst_protocol->username(client_greeting_msg.username());

  auto attrs = attrs_res.value_or(src_protocol->attributes());
  dst_protocol->sent_attributes(attrs);
  src_protocol->sent_attributes(attrs);

  // the client's attributes, as they are sent to the server.

  client_greeting_msg.capabilities(dst_protocol->client_capabilities());
  client_greeting_msg.attributes(attrs);

  if (src_protocol->password().has_value()) {
    // scramble with the server's auth-data to trigger a fast-auth.

    auto pwd = *(src_protocol->password());

    // if the password set and not empty, rehash it.
    if (!pwd.empty()) {
      if (auto scramble_res = scramble_them_all(
              client_greeting_msg.auth_method_name(),
              strip_trailing_null(
                  dst_protocol->server_greeting()->auth_method_data()),
              pwd)) {
        client_greeting_msg.auth_method_data(*scramble_res);
      }
    }
  }

  return ClassicFrame::send_msg(dst_channel, dst_protocol, client_greeting_msg)
      .and_then(
          [this](auto /* unused */) -> stdx::expected<Result, std::error_code> {
            stage(Stage::InitialResponse);

            return Result::SendToServer;
          })
      .or_else([this](auto err) { return send_server_failed(err); });
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::initial_response() {
  connection()->push_processor(std::make_unique<AuthForwarder>(connection()));

  stage(Stage::FinalResponse);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::final_response() {
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
      stage(Stage::AuthOk);
      return Result::Again;
    case Msg::Error:
      stage(Stage::AuthError);
      return Result::Again;
  }

  // if there is another packet, dump its payload for now.
  auto &recv_buf = src_channel->recv_plain_buffer();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ClassicFrame::ensure_has_full_frame(src_channel, src_protocol);

  log_debug(
      "received unexpected message from server after a client::Greeting:\n%s",
      hexify(recv_buf).c_str());

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

/**
 * router<-server: auth error.
 */
stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::auth_error() {
  trace(Tracer::Event().stage("server::auth::error"));

  stage(Stage::Error);  // close the server connection after the Error msg was
                        // sent.

  return forward_server_to_client();
}

/**
 * server-side: auth is ok.
 */
stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::auth_ok() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->server_channel();
  auto *src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Ok>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("server::ok"));

  auto msg = std::move(*msg_res);

  if (!msg.session_changes().empty()) {
    (void)connection()->track_session_changes(
        net::buffer(msg.session_changes()),
        src_protocol->shared_capabilities());
  }

  stage(Stage::Ok);

  return forward_server_to_client();
}
