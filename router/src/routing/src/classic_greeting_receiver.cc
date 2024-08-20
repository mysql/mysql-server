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

#include "classic_greeting_receiver.h"

#include <cctype>
#include <random>  // uniform_int_distribution
#include <sstream>
#include <system_error>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "classic_auth.h"
#include "classic_auth_caching_sha2.h"
#include "classic_auth_cleartext.h"
#include "classic_auth_forwarder.h"
#include "classic_auth_native.h"
#include "classic_auth_openid_connect.h"
#include "classic_auth_sha256_password.h"
#include "classic_connection_base.h"
#include "classic_frame.h"
#include "classic_greeting_forwarder.h"
#include "classic_lazy_connect.h"
#include "harness_assert.h"
#include "hexify.h"
#include "mysql/harness/logging/logger.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql-server error-codes
#include "mysqlrouter/classic_protocol_codec_error.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/connection_base.h"
#include "mysqlrouter/routing.h"
#include "openssl_msg.h"
#include "openssl_version.h"
#include "processor.h"
#include "router_config.h"  // MYSQL_ROUTER_VERSION
#include "tracer.h"

IMPORT_LOG_FUNCTIONS()

using namespace std::string_literals;
using namespace std::string_view_literals;

static constexpr const std::array supported_authentication_methods{
    AuthCachingSha2Password::kName,  //
    AuthNativePassword::kName,       //
    AuthCleartextPassword::kName,    //
    AuthSha256Password::kName,       //
    AuthOpenidConnect::kName,        //
};

static constexpr const bool kCapturePlaintextPassword{true};

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
    case Stage::DecryptPassword:
      return decrypt_password();

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
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::greeting::error"));
  }

  auto &client_conn = connection()->client_conn();

  if (client_conn.protocol().handshake_state() ==
      ClassicProtocolState::HandshakeState::kClientGreeting) {
    // reached the error, but still are in the initial ClientGreeting state.
    connection()->on_handshake_aborted();
  }

  (void)client_conn.cancel();
  (void)client_conn.shutdown(net::socket_base::shutdown_both);

  return Result::Done;
}

stdx::expected<Processor::Result, std::error_code> ClientGreetor::init() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::init"));
  }

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
  auto &dst_conn = connection()->client_conn();
  auto &dst_channel = dst_conn.channel();
  auto &dst_protocol = dst_conn.protocol();

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
      classic_protocol::capabilities::optional_resultset_metadata |
      classic_protocol::capabilities::query_attributes
      // compress_zstd (not yet)
  );

  if (connection()->source_ssl_mode() != SslMode::kDisabled) {
    router_capabilities.set(classic_protocol::capabilities::pos::ssl);
  }

  dst_protocol.server_capabilities(router_capabilities);

  auto random_auth_method_data = []() {
    std::random_device rd;
    std::mt19937 gen(rd());
    // Scrambles defined as 7-bit: 1..127 ... no \0 chars
    std::uniform_int_distribution<> distrib(1, 127);

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
      dst_protocol.server_capabilities(),           // server-caps
      255,                                          // 8.0.20 sends 0xff here
      classic_protocol::status::autocommit,         // status-flags
      std::string(AuthCachingSha2Password::kName),  // auth-method-name
  };

  auto send_res =
      ClassicFrame::send_msg(dst_channel, dst_protocol, server_greeting_msg,
                             {/* no shared caps yet */});
  if (!send_res) return send_client_failed(send_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("server::greeting"));
  }

  dst_protocol.auth_method_data(server_greeting_msg.auth_method_data());
  dst_protocol.server_greeting(server_greeting_msg);

  // ServerGreeting is sent, expecting a ClientGreeting next.
  connection()->client_conn().protocol().handshake_state(
      ClassicProtocolState::HandshakeState::kServerGreeting);

  stage(Stage::ClientGreeting);
  return Result::SendToClient;
}

/**
 * client<-router: server::greeting.
 */
stdx::expected<Processor::Result, std::error_code>
ClientGreetor::server_first_greeting() {
  // ServerFirstGreetor either
  // - sent the server-greeting to the client and
  //   left the server connection open, or
  // - sent the error to the client and
  //   closed the connection.

  auto &server_conn = connection()->server_conn();

  if (server_conn.is_open()) {
    stage(Stage::ClientGreeting);
  } else {
    stage(Stage::Error);
  }

  return Result::Again;
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

static bool client_compress_is_satisfied(
    classic_protocol::capabilities::value_type client_capabilities,
    classic_protocol::capabilities::value_type shared_capabilities) {
  // client enabled "zlib-compress" without checking the server's caps.
  //
  // fail the connect.
  return !(
      client_capabilities.test(classic_protocol::capabilities::pos::compress) &&
      !shared_capabilities.test(classic_protocol::capabilities::pos::compress));
}

static stdx::expected<size_t, std::error_code> send_ssl_connection_error_msg(
    Channel &dst_channel, ClassicProtocolState &dst_protocol,
    const std::string &msg) {
  return ClassicFrame::send_msg<
      classic_protocol::borrowed::message::server::Error>(
      dst_channel, dst_protocol, {CR_SSL_CONNECTION_ERROR, msg});
}

/**
 * handle client greeting.
 */
stdx::expected<Processor::Result, std::error_code>
ClientGreetor::client_greeting() {
  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::client::Greeting>(
          src_channel, src_protocol, src_protocol.server_capabilities());
  if (!msg_res) {
    const auto ec = msg_res.error();

    if (!src_channel.recv_plain_view().empty()) {
      // something was received, but it failed to decode.
      connection()->client_conn().protocol().handshake_state(
          ClassicProtocolState::HandshakeState::kClientGreeting);
    }

    if (ec.category() != classic_protocol::codec_category()) {
      return recv_client_failed(ec);
    }

    discard_current_msg(src_conn);

    // server sends Bad Handshake instead of Malformed message.
    const auto send_msg = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Error>(
        src_conn, {ER_HANDSHAKE_ERROR, "Bad handshake", "08S01"});
    if (!send_msg) send_client_failed(send_msg.error());

    stage(Stage::Error);

    return Result::SendToClient;
  }

  // got a greeting, treat all errors that abort the connection
  // in abnormal way as "connect-errors".
  connection()->client_conn().protocol().handshake_state(
      ClassicProtocolState::HandshakeState::kClientGreeting);

  if (src_protocol.seq_id() != 1) {
    discard_current_msg(src_conn);

    const auto send_msg = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Error>(
        src_conn,
        {ER_NET_PACKETS_OUT_OF_ORDER, "Got packets out of order", "08S01"});
    if (!send_msg) send_client_failed(send_msg.error());

    stage(Stage::Error);

    return Result::SendToClient;
  }

  auto msg = std::move(*msg_res);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::greeting"));
  }

  src_protocol.client_greeting(msg);
  src_protocol.client_capabilities(msg.capabilities());
  src_protocol.auth_method_name(msg.auth_method_name());
  src_protocol.username(msg.username());
  src_protocol.schema(msg.schema());
  src_protocol.attributes(msg.attributes());

  if (!client_ssl_mode_is_satisfied(connection()->source_ssl_mode(),
                                    src_protocol.shared_capabilities())) {
    // do NOT treat ssl-mode-errors as "connect-error"
    connection()->client_conn().protocol().handshake_state(
        ClassicProtocolState::HandshakeState::kFinished);

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

  // fail connection from buggy clients that set the compress-cap without
  // checking the server's capabilities.
  if (!client_compress_is_satisfied(src_protocol.client_capabilities(),
                                    src_protocol.shared_capabilities())) {
    // do NOT treat compress-mode-errors as "connect-error"
    connection()->client_conn().protocol().handshake_state(
        ClassicProtocolState::HandshakeState::kFinished);

    const auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Error>(
        src_conn, {ER_WRONG_COMPRESSION_ALGORITHM_CLIENT,
                   "Compression not supported by router."});
    if (!send_res) return send_client_failed(send_res.error());

    stage(Stage::Error);
    return Result::SendToClient;
  }

  // block pre-5.6-like clients that don't support CLIENT_PLUGIN_AUTH.
  //
  // CLIENT_PLUGIN_AUTH is later needed to switch mysql-native-password
  // from the router's nonce to the server's nonce.
  if (connection()->greeting_from_router() &&
      !src_protocol.client_capabilities().test(
          classic_protocol::capabilities::pos::plugin_auth) &&
      src_protocol.server_capabilities().test(
          classic_protocol::capabilities::pos::plugin_auth)) {
    // do NOT treat this error as "connect-error"
    connection()->client_conn().protocol().handshake_state(
        ClassicProtocolState::HandshakeState::kFinished);

    const auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Error>(
        src_conn, {ER_NOT_SUPPORTED_AUTH_MODE,
                   "Client does not support authentication protocol requested "
                   "by server; consider upgrading MySQL client",
                   "08004"});
    if (!send_res) return send_client_failed(send_res.error());

    stage(Stage::Error);
    return Result::SendToClient;
  }

  // remove the frame and message from the recv-buffer
  discard_current_msg(src_conn);

  if (!src_protocol.shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    // client wants to stay with plaintext

    // libmysqlclient sends auth-data: \x00 for empty password
    // php sends auth-data: <empty> for empty password.
    //
    // check that the auth-method-name matches the auth method sent in the
    // server-greeting the client received.
    if (src_protocol.server_greeting()->auth_method_name() ==
            AuthCachingSha2Password::kName &&
        src_protocol.auth_method_name() == AuthCachingSha2Password::kName &&
        (msg.auth_method_data() == "\x00"sv ||
         msg.auth_method_data().empty())) {
      // password is empty.
      src_protocol.credentials().emplace(src_protocol.auth_method_name(), "");
    } else if (connection()->source_ssl_mode() != SslMode::kPassthrough) {
      const bool client_conn_is_secure =
          connection()->client_conn().is_secure_transport();

      if ((client_conn_is_secure ||
           connection()->context().source_ssl_ctx() != nullptr) &&
          connection()->context().connection_sharing() &&
          src_protocol.auth_method_name() == AuthCachingSha2Password::kName) {
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

static void ssl_msg_cb(int write_p, int version, int content_type,
                       const void *buf, size_t len, SSL *ssl [[maybe_unused]],
                       void *arg) {
  if (arg == nullptr) return;

  auto *conn = static_cast<MysqlRoutingClassicConnectionBase *>(arg);

  auto &tr = conn->tracer();
  if (!tr) return;

  if (content_type == SSL3_RT_HEADER) return;
#ifdef SSL3_RT_INNER_CONTENT_TYPE
  if (content_type == SSL3_RT_INNER_CONTENT_TYPE) return;
#endif

  tr.trace(Tracer::Event().stage(
      "tls::" + std::string(write_p == 0 ? "client" : "server") +
      "::msg: " + openssl_msg_version_to_string(version).value_or("") + " " +
      openssl_msg_content_type_to_string(content_type).value_or("") + "::" +
      openssl_msg_content_to_string(
          content_type, static_cast<const unsigned char *>(buf), len)
          .value_or("")
#if 0
      +
      "\n" +
      mysql_harness::hexify(
          std::string_view(static_cast<const char *>(buf), len))
#endif
          ));
}

stdx::expected<Processor::Result, std::error_code>
ClientGreetor::tls_accept_init() {
  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();

  src_channel.is_tls(true);

  auto *ssl_ctx = connection()->context().source_ssl_ctx()->get();
  // tls <-> (any)
  if (ssl_ctx == nullptr) {
    // shouldn't happen. But if it does, close the connection.
    log_warning("failed to create SSL_CTX");
    return recv_client_failed(make_error_code(std::errc::invalid_argument));
  }
  src_channel.init_ssl(ssl_ctx);

  SSL_set_app_data(src_channel.ssl(), connection());

  SSL_set_msg_callback(src_channel.ssl(), ssl_msg_cb);
  SSL_set_msg_callback_arg(src_channel.ssl(), connection());

  stage(Stage::TlsAccept);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> ClientGreetor::tls_accept() {
  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();

  if (!src_channel.tls_init_is_finished()) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("tls::accept"));
    }

    {
      const auto flush_res = src_channel.flush_from_recv_buf();
      if (!flush_res) return stdx::unexpected(flush_res.error());
    }

    auto res = src_channel.tls_accept();

    // flush the TLS message to the send-buffer.
    {
      const auto flush_res = src_channel.flush_to_send_buf();
      if (!flush_res) {
        const auto ec = flush_res.error();
        if (ec != make_error_code(std::errc::operation_would_block)) {
          return stdx::unexpected(flush_res.error());
        }
      }
    }

    if (!res) {
      const auto ec = res.error();

      // the send-buffer contains an alert message telling the client why the
      // accept failed.
      if (!src_channel.send_buffer().empty()) {
        if (ec != TlsErrc::kWantRead) {
          // do NOT treat tls-handshake-errors that are returned
          // to the client as "connect-error"
          connection()->client_conn().protocol().handshake_state(
              ClassicProtocolState::HandshakeState::kFinished);

          log_debug("tls-accept failed: %s", ec.message().c_str());

          stage(Stage::Error);
        }
        return Result::SendToClient;
      }

      if (ec == TlsErrc::kWantRead) return Result::RecvFromClient;

      log_info("accepting TLS connection from %s failed: %s",
               connection()->get_client_address().c_str(),
               ec.message().c_str());

      stage(Stage::Error);
      return Result::Again;
    }
  }

  if (auto &tr = tracer()) {
    auto *ssl = src_channel.ssl();

    std::ostringstream oss;
    oss << "tls::accept::ok: " << SSL_get_version(ssl);
    oss << " using " << SSL_get_cipher_name(ssl);
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(3, 0, 0)
    oss << " and " << OBJ_nid2ln(SSL_get_negotiated_group(ssl));
#endif

    if (SSL_session_reused(ssl) != 0) {
      oss << ", session_reused";
    }

    tr.trace(Tracer::Event().stage(oss.str()));
  }

  stage(Stage::ClientGreetingAfterTls);

  // after tls_accept() there may still be data in the send-buffer that must
  // be sent.
  if (!src_channel.send_buffer().empty()) {
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

stdx::expected<Processor::Result, std::error_code>
ClientGreetor::client_greeting_after_tls() {
  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::client::Greeting>(
          src_channel, src_protocol, src_protocol.server_capabilities());
  if (!msg_res) return recv_client_failed(msg_res.error());

  auto msg = *msg_res;

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::greeting"));
  }

  src_protocol.client_greeting(msg);
  src_protocol.auth_method_name(msg.auth_method_name());
  src_protocol.client_capabilities(msg.capabilities());
  src_protocol.username(msg.username());
  src_protocol.schema(msg.schema());
  src_protocol.attributes(msg.attributes());

  discard_current_msg(src_conn);

  if (!authentication_method_is_supported(msg.auth_method_name())) {
    // do NOT treat auth-errors as "connect-error"
    connection()->client_conn().protocol().handshake_state(
        ClassicProtocolState::HandshakeState::kFinished);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("client::greeting::error"));
    }

    const auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Error>(
        src_conn, {CR_AUTH_PLUGIN_CANNOT_LOAD,
                   "Authentication method " + msg.auth_method_name() +
                       " is not supported",
                   "HY000"});
    if (!send_res) return send_client_failed(send_res.error());

    stage(Stage::Error);
    return Result::SendToClient;
  }

  // fail connection from buggy clients that set the compress-cap without
  // checking if the server's capabilities.
  if (!client_compress_is_satisfied(src_protocol.client_capabilities(),
                                    src_protocol.shared_capabilities())) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("client::greeting::error"));
    }

    // do NOT treat compress-mode-errors as "connect-error"
    connection()->client_conn().protocol().handshake_state(
        ClassicProtocolState::HandshakeState::kFinished);

    const auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Error>(
        src_conn, {ER_WRONG_COMPRESSION_ALGORITHM_CLIENT,
                   "Compression not supported by router."});
    if (!send_res) return send_client_failed(send_res.error());

    stage(Stage::Error);
    return Result::SendToClient;
  }

  // if the client and server use the same auth-method-name,
  // then a empty auth-method-data means "empty-password".
  //
  // - server: --default-auth=caching-sha2-password
  // - client: --default-auth=caching-sha2-password
  //
  // Otherwise its value is bogus:
  //
  // - server: --default-auth=caching-sha2-password
  // - client: --default-auth=mysql_native_password
  //
  if ((src_protocol.auth_method_name() ==
       src_protocol.server_greeting()->auth_method_name()) &&
      (src_protocol.client_greeting()->auth_method_data() == "\x00"sv ||
       src_protocol.client_greeting()->auth_method_data().empty())) {
    // special value for 'empty password'. Not scrambled.
    //
    // - php sends no trailing '\0'
    // - libmysqlclient sends a trailing '\0'
    src_protocol.credentials().emplace(src_protocol.auth_method_name(), "");

    stage(Stage::Accepted);
    return Result::Again;
  } else if (kCapturePlaintextPassword && src_protocol.auth_method_name() ==
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
  auto &dst_conn = connection()->client_conn();
  auto &dst_channel = dst_conn.channel();
  auto &dst_protocol = dst_conn.protocol();

  auto send_res = AuthCachingSha2Password::send_plaintext_password_request(
      dst_channel, dst_protocol);
  if (!send_res) return send_client_failed(send_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("server::auth::request::plain"));
  }

  stage(Stage::PlaintextPassword);
  return Result::SendToClient;
}

/**
 * extract the password from auth-method-data.
 *
 * @returns the payload without the trailing NUL-char.
 * @retval false in there is no password.
 */
static std::optional<std::string_view> password_from_auth_method_data(
    std::string_view auth_data) {
  if (auth_data.empty() || auth_data.back() != '\0') return std::nullopt;

  return auth_data.substr(0, auth_data.size() - 1);
}

/**
 * receive the client's plaintext password.
 *
 * after client_send_request_for_plaintext_password()
 */
stdx::expected<Processor::Result, std::error_code>
ClientGreetor::plaintext_password() {
  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::AuthMethodData>(src_conn);
  if (!msg_res) return recv_client_failed(msg_res.error());

  const bool client_conn_is_secure = src_conn.is_secure_transport();

  if (client_conn_is_secure) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("client::auth::plain"));
    }

    if (auto pwd =
            password_from_auth_method_data(msg_res->auth_method_data())) {
      src_protocol.credentials().emplace(src_protocol.auth_method_name(), *pwd);
    }
    // discard the current frame.
    discard_current_msg(src_conn);

    stage(Stage::Accepted);
    return Result::Again;
  }

  if (AuthCachingSha2Password::is_public_key_request(
          msg_res->auth_method_data())) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("client::auth::public_key_request"));
    }

    auto pubkey_res = AuthCachingSha2Password::public_key_from_ssl_ctx_as_pem(
        connection()->context().source_ssl_ctx()->get());
    if (!pubkey_res) {
      // couldn't get the public key, fail the auth.
      auto send_res = ClassicFrame::send_msg<
          classic_protocol::borrowed::message::server::Error>(
          src_conn, {ER_ACCESS_DENIED_ERROR, "Access denied", "HY000"});
      if (!send_res) return send_client_failed(send_res.error());

      discard_current_msg(src_conn);

      stage(Stage::Error);
      return Result::SendToClient;
    }

    auto send_res = AuthCachingSha2Password::send_public_key(
        src_channel, src_protocol, *pubkey_res);
    if (!send_res) return send_client_failed(send_res.error());

    discard_current_msg(src_conn);

    stage(Stage::DecryptPassword);
    return Result::SendToClient;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::auth::???"));
  }

  const auto send_res = ClassicFrame::send_msg<
      classic_protocol::borrowed::message::server::Error>(
      src_conn,
      {CR_AUTH_PLUGIN_CANNOT_LOAD, "Unexpected message ...", "HY000"});
  if (!send_res) return send_client_failed(send_res.error());

  discard_current_msg(src_conn);

  stage(Stage::Error);
  return Result::SendToClient;
}

stdx::expected<Processor::Result, std::error_code>
ClientGreetor::decrypt_password() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::client::AuthMethodData>(src_conn);
  if (!msg_res) return recv_client_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::auth::encrypted"));
  }

  auto nonce = src_protocol.server_greeting()->auth_method_data();

  // if there is a trailing zero, strip it.
  if (nonce.size() == AuthCachingSha2Password::kNonceLength + 1 &&
      nonce[AuthCachingSha2Password::kNonceLength] == 0x00) {
    nonce = nonce.substr(0, AuthCachingSha2Password::kNonceLength);
  }

  auto recv_res = AuthCachingSha2Password::rsa_decrypt_password(
      connection()->context().source_ssl_ctx()->get(),
      msg_res->auth_method_data(), nonce);
  if (!recv_res) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("client::auth::encrypted::failed: " +
                                     recv_res.error().message()));
    }

    return recv_client_failed(recv_res.error());
  }

  src_protocol.credentials().emplace(src_protocol.auth_method_name(),
                                     *recv_res);

  // discard the current frame.
  discard_current_msg(src_conn);

  stage(Stage::Accepted);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> ClientGreetor::accepted() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::greeting::client_done"));
  }

  // treat the client-handshake as finished. No further tracking of
  // max-connect-errors.
  connection()->client_conn().protocol().handshake_state(
      ClassicProtocolState::HandshakeState::kFinished);

  auto &src_protocol = connection()->client_conn().protocol();
  auto &dst_protocol = connection()->server_conn().protocol();

  stage(Stage::Authenticated);

  if (dst_protocol.server_greeting().has_value()) {
    // server-greeting is already present.
    connection()->push_processor(std::make_unique<ServerFirstAuthenticator>(
        connection(),
        [this](const classic_protocol::message::server::Error &err) {
          connect_err_ = err;
        }));
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

    if (connection()->requires_tls() &&
        !connection()->context().dest_ssl_cert().empty()) {
      connection()->requires_client_cert(true);
    }

    if (connection()->context().access_mode() == routing::AccessMode::kAuto &&
        !src_protocol.credentials().get(AuthCachingSha2Password::kName)) {
      // by default, authentication can be done on any server if rw-splitting is
      // enabled.
      //
      // But if there is no password yet, router may also not get it in the
      // authentication round, which would mean that the connection can't be
      // shared and switched to the read-write server when needed.
      //
      // Therefore, force authentication on a read-write server
      connection()->expected_server_mode(mysqlrouter::ServerMode::ReadWrite);
    }

    connection()->push_processor(std::make_unique<LazyConnector>(
        connection(), true /* in handshake */,
        [this](const classic_protocol::message::server::Error &err) {
          connect_err_ = err;
        },
        nullptr));
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ClientGreetor::authenticated() {
  if (!connection()->authenticated()) {
    auto &src_conn = connection()->client_conn();

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("greeting::error"));
    }

    stage(Stage::Error);

    auto send_res = ClassicFrame::send_msg(src_conn, connect_err_);
    if (!send_res) return send_client_failed(send_res.error());

    return Result::SendToClient;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("greeting::auth::done"));
  }

  stage(Stage::Ok);
  return Result::Again;
}
