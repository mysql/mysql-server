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
#include "classic_auth_sha256_password.h"
#include "classic_connection_base.h"
#include "classic_frame.h"
#include "classic_greeting_forwarder.h"
#include "classic_lazy_connect.h"
#include "harness_assert.h"
#include "mysql/harness/logging/logger.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql-server error-codes
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/connection_base.h"
#include "processor.h"
#include "sql/server_component/mysql_command_services_imp.h"
#include "tracer.h"

IMPORT_LOG_FUNCTIONS()

using namespace std::string_literals;
using namespace std::string_view_literals;

static constexpr const std::array supported_authentication_methods{
    AuthCachingSha2Password::kName,
    AuthNativePassword::kName,
    AuthCleartextPassword::kName,
    AuthSha256Password::kName,
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

static void ssl_info_cb(const SSL *ssl, int where, int ret) {
  auto *conn = reinterpret_cast<MysqlRoutingClassicConnectionBase *>(
      SSL_get_app_data(ssl));

  auto &tr = conn->tracer();
  if (!tr) return;

  if ((where & SSL_CB_LOOP) != 0) {
    tr.trace(
        Tracer::Event().stage("tls::state: "s + SSL_state_string_long(ssl)));
  } else if ((where & SSL_CB_ALERT) != 0) {
    tr.trace(Tracer::Event().stage("tls::alert: "s +
                                   SSL_alert_type_string_long(ret) + "::"s +
                                   SSL_alert_desc_string_long(ret)));
  } else if ((where & SSL_CB_EXIT) != 0) {
    if (ret == 0) {
      tr.trace(Tracer::Event().stage(
          "tls::state: "s + SSL_state_string_long(ssl) + " <failed>"s));
    } else if (ret < 0) {
      switch (SSL_get_error(ssl, ret)) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
          break;
        default:
          tr.trace(Tracer::Event().stage("tls"s + SSL_state_string_long(ssl) +
                                         " <error>"s));
      }
    }
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
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::greeting::error"));
  }

  auto &client_conn = connection()->socket_splicer()->client_conn();

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
      dst_protocol->server_capabilities(),          // server-caps
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
    Channel *dst_channel, ClassicProtocolState *dst_protocol,
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

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::greeting"));
  }

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

  // fail connection from buggy clients that set the compress-cap without
  // checking the server's capabilities.
  if (!client_compress_is_satisfied(src_protocol->client_capabilities(),
                                    src_protocol->shared_capabilities())) {
    const auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Error>(
        src_channel, src_protocol,
        {ER_WRONG_COMPRESSION_ALGORITHM_CLIENT,
         "Compression not supported by router."});
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

  SSL_set_app_data(src_channel->ssl(), connection());
  SSL_set_info_callback(src_channel->ssl(), ssl_info_cb);

  stage(Stage::TlsAccept);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> ClientGreetor::tls_accept() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *client_channel = socket_splicer->client_channel();

  if (!client_channel->tls_init_is_finished()) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("tls::accept"));
    }

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

  if (auto &tr = tracer()) {
    auto *ssl = client_channel->ssl();
    std::ostringstream oss;
    oss << "tls::accept::ok: " << SSL_get_version(ssl);
    oss << " using " << SSL_get_cipher_name(ssl);

    if (SSL_session_reused(ssl) != 0) {
      oss << ", session_reused";
    }

    tr.trace(Tracer::Event().stage(oss.str()));
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

stdx::expected<Processor::Result, std::error_code>
ClientGreetor::client_greeting_after_tls() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::client::Greeting>(
          src_channel, src_protocol, src_protocol->server_capabilities());
  if (!msg_res) return recv_client_failed(msg_res.error());

  auto msg = *msg_res;

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::greeting"));
  }

  src_protocol->client_greeting(msg);
  src_protocol->auth_method_name(msg.auth_method_name());
  src_protocol->client_capabilities(msg.capabilities());
  src_protocol->username(msg.username());
  src_protocol->schema(msg.schema());
  src_protocol->attributes(msg.attributes());

  discard_current_msg(src_channel, src_protocol);

  if (!authentication_method_is_supported(msg.auth_method_name())) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("client::greeting::error"));
    }

    const auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Error>(
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
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("client::greeting::error"));
    }
    const auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Error>(
        src_channel, src_protocol,
        {ER_WRONG_COMPRESSION_ALGORITHM_CLIENT,
         "Compression not supported by router."});
    if (!send_res) return send_client_failed(send_res.error());

    stage(Stage::Error);
    return Result::SendToClient;
  }

  if (src_protocol->client_greeting()->auth_method_data() == "\x00"sv ||
      src_protocol->client_greeting()->auth_method_data().empty()) {
    // special value for 'empty password'. Not scrambled.
    //
    // - php sends no trailing '\0'
    // - libmysqlclient sends a trailing '\0'
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

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::auth::plain"));
  }

  if (auto pwd = password_from_auth_method_data(msg_res->value())) {
    src_protocol->password(*pwd);
  }

  // discard the current frame.
  discard_current_msg(src_channel, src_protocol);

  stage(Stage::Accepted);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> ClientGreetor::accepted() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::greeting::client_done"));
  }

  auto *dst_protocol = connection()->server_protocol();

  stage(Stage::Authenticated);

  if (dst_protocol->server_greeting().has_value()) {
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

    connection()->push_processor(std::make_unique<LazyConnector>(
        connection(), true /* in handshake */,
        [this](const classic_protocol::message::server::Error &err) {
          connect_err_ = err;
        }));
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ClientGreetor::authenticated() {
  if (!connection()->authenticated()) {
    auto *src_channel = connection()->socket_splicer()->client_channel();
    auto *src_protocol = connection()->client_protocol();

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("greeting::error"));
    }

    stage(Stage::Error);

    auto send_res =
        ClassicFrame::send_msg(src_channel, src_protocol, connect_err_);
    if (!send_res) return send_client_failed(send_res.error());

    return Result::SendToClient;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("greeting::auth::done"));
  }

  stage(Stage::Ok);
  return Result::Again;
}
