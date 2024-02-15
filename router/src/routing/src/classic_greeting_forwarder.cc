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

#include "classic_greeting_forwarder.h"

#include <cctype>
#include <iostream>
#include <memory>
#include <optional>
#include <random>  // uniform_int_distribution
#include <sstream>
#include <system_error>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "await_client_or_server.h"
#include "classic_auth.h"
#include "classic_auth_caching_sha2.h"
#include "classic_auth_cleartext.h"
#include "classic_auth_forwarder.h"
#include "classic_auth_native.h"
#include "classic_auth_sha256_password.h"
#include "classic_change_user_forwarder.h"
#include "classic_connect.h"
#include "classic_connection_base.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "classic_query_sender.h"
#include "harness_assert.h"
#include "hexify.h"
#include "mysql/harness/logging/logger.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql-server error-codes
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/connection_base.h"
#include "openssl_msg.h"
#include "openssl_version.h"
#include "processor.h"
#include "router_require.h"
#include "tls_content_type.h"
#include "tracer.h"

IMPORT_LOG_FUNCTIONS()

using mysql_harness::hexify;

using namespace std::string_literals;
using namespace std::string_view_literals;

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

static stdx::expected<size_t, std::error_code> send_ssl_connection_error_msg(
    MysqlRoutingClassicConnectionBase::ClientSideConnection &conn,
    std::string_view msg) {
  return ClassicFrame::send_msg<
      classic_protocol::borrowed::message::server::Error>(
      conn, {CR_SSL_CONNECTION_ERROR, msg});
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
    return stdx::unexpected(encode_res.error());
  }

  size_t encoded_bytes = encode_res.value();

  encode_res =
      classic_protocol::encode(classic_protocol::wire::VarString(value), {},
                               net::dynamic_buffer(attrs_buf));
  if (!encode_res) {
    return stdx::unexpected(encode_res.error());
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
    if (!decode_res) return stdx::unexpected(decode_res.error());

    const auto bytes_read = decode_res->first;
    const auto kv = decode_res->second;

    attr_buf += bytes_read;

    // toggle the key/value tracker.
    is_key = !is_key;
  }

  // if the last key doesn't have a value, fail
  if (!is_key || net::buffer_size(attr_buf) != 0) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
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
  if (!verify_res) return stdx::unexpected(verify_res.error());

  for (const auto &attr : extra_attributes) {
    const auto append_res =
        classic_proto_append_attribute(attrs, attr.first, attr.second);
    if (!append_res) return stdx::unexpected(append_res.error());
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
      trace_span_end(trace_event_greeting_);

      connection()->authenticated(true);
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> ServerGreetor::error() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event()
                 .stage("close::server")
                 .direction(Tracer::Event::Direction::kServerClose));
  }

  trace_span_end(trace_event_greeting_, TraceEvent::StatusCode::kError);

  // reset the server connection.
  //
  // - close the connection
  // - reset all protocol state.
  // - reset all channel state

  connection()->server_conn() = {
      nullptr, connection()->context().dest_ssl_mode(),
      MysqlRoutingClassicConnectionBase::ServerSideConnection::
          protocol_state_type()};

  // force a connection close after the error-msg was sent.
  connection()->authenticated(false);

  return Result::Done;
}

stdx::expected<Processor::Result, std::error_code>
ServerGreetor::server_greeting() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  if (trace_event_greeting_ == nullptr) {
    trace_event_greeting_ = trace_span(parent_event_, "mysql/greeting");

    trace_event_server_greeting_ =
        trace_span(trace_event_greeting_, "mysql/server_greeting");
  }

  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) return recv_server_failed(read_res.error());

  uint8_t msg_type = src_protocol.current_msg_type().value();

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
  connection()->server_conn().protocol().handshake_state(
      ClassicProtocolState::HandshakeState::kFinished);

  auto &src_conn = connection()->server_conn();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::Error>(src_conn);
  if (!msg_res) return recv_client_failed(msg_res.error());

  auto msg = *msg_res;

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("server::greeting::error: " +
                                   std::to_string(msg.error_code())));
  }

  if (auto *ev = trace_event_server_greeting_) {
    trace_span_end(ev, TraceEvent::StatusCode::kError);
  }

  trace_span_end(trace_event_greeting_, TraceEvent::StatusCode::kError);

  stage(Stage::Error);

  // the message arrived before the handshake started and is therefore in
  // in 3.21 format which has no "sql-state".
  //
  // 08004 is 'server rejected connection'
  on_error_(
      {msg.error_code(), std::string(msg.message()), std::string("08004")});

  discard_current_msg(src_conn);

  return Result::Again;
}

// called after server connection is established.
void ServerGreetor::client_greeting_server_adjust_caps(
    ClassicProtocolState &src_protocol, ClassicProtocolState &dst_protocol) {
  auto client_caps = src_protocol.client_capabilities();

  if (!src_protocol.shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    auto attrs_res = classic_proto_decode_and_add_connection_attributes(
        src_protocol.attributes(),
        connection()->client_conn().initial_connection_attributes());

    // client hasn't set the SSL cap, this is the real client greeting
    auto attrs = attrs_res.value_or(src_protocol.attributes());

    dst_protocol.sent_attributes(attrs);
    src_protocol.sent_attributes(attrs);

    auto client_greeting_msg = src_protocol.client_greeting().value();
    client_greeting_msg.attributes(attrs);
    dst_protocol.client_greeting(client_greeting_msg);
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
      if (dst_protocol.server_capabilities().test(
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
  dst_protocol.client_capabilities(client_caps);
}

/**
 * received a server::greeting from the server.
 *
 * decode it.
 */
stdx::expected<Processor::Result, std::error_code>
ServerGreetor::server_greeting_greeting() {
  auto &src_conn = connection()->server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->client_conn();
  auto &dst_protocol = dst_conn.protocol();

  const auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::Greeting>(
          src_channel, src_protocol, {/* no shared caps yet */});
  if (!msg_res) return stdx::unexpected(msg_res.error());

#if defined(DEBUG_STATE)
  log_debug("client-ssl-mode=%s, server-ssl-mode=%s",
            ssl_mode_to_string(source_ssl_mode()),
            ssl_mode_to_string(dest_ssl_mode()));
#endif

  auto server_greeting_msg = *msg_res;

  // engage the "send-client-greeting-on-early-client-abort"
  connection()->server_conn().protocol().handshake_state(
      ClassicProtocolState::HandshakeState::kServerGreeting);

  auto caps = server_greeting_msg.capabilities();

  src_protocol.server_capabilities(caps);
  src_protocol.server_greeting(server_greeting_msg);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("server::greeting::greeting"));
  }

  if (auto *ev = trace_event_server_greeting_) {
    ev->attrs.emplace_back(
        "mysql.remote.connection_id",
        static_cast<int64_t>(server_greeting_msg.connection_id()));
  }

  auto msg = src_protocol.server_greeting().value();
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
                                    src_protocol.server_capabilities())) {
    discard_current_msg(src_conn);

    // destination does not support TLS, but config requires encryption.
    log_debug(
        "server_ssl_mode=REQUIRED, but destination doesn't support "
        "encryption.");

    stage(Stage::Error);
    if (!in_handshake_) {
      on_error_({CR_SSL_CONNECTION_ERROR,
                 "SSL connection error: SSL is required by router, but the "
                 "server doesn't support it"});

      return Result::Again;
    }

    auto send_res = send_ssl_connection_error_msg(
        dst_conn,
        "SSL connection error: SSL is required by router, but the "
        "server doesn't support it");
    if (!send_res) {
      auto ec = send_res.error();
      log_fatal_error_code("sending error-msg failed", ec);

      return send_client_failed(ec);
    }

    return Result::SendToClient;
  }

  // the server side's auth-method-data
  src_protocol.auth_method_data(msg.auth_method_data());

  if (!dst_protocol.server_greeting()) {
    discard_current_msg(src_conn);
    // client doesn't have server greeting yet, send it the server's.

    auto caps = src_protocol.server_capabilities();

    adjust_supported_capabilities(connection()->source_ssl_mode(),
                                  connection()->dest_ssl_mode(), caps);

    // update the client side's auth-method-data.
    dst_protocol.auth_method_data(msg.auth_method_data());
    dst_protocol.server_capabilities(caps);
    dst_protocol.seq_id(0xff);  // will be incremented by 1

    msg.capabilities(caps);

    auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::Greeting>(
            dst_conn, msg);
    if (!send_res) return send_client_failed(send_res.error());

    dst_protocol.server_greeting(msg);

    trace_span_end(trace_event_greeting_, TraceEvent::StatusCode::kOk);

    stage(Stage::ServerGreetingSent);  // hand over to the ServerFirstConnector
    return Result::SendToClient;
  } else {
    discard_current_msg(src_conn);

    stage(Stage::ClientGreeting);
    return Result::Again;
  }
}

stdx::expected<Processor::Result, std::error_code>
ServerGreetor::client_greeting() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->server_conn();
  auto &dst_protocol = dst_conn.protocol();

  bool server_supports_tls = dst_protocol.server_capabilities().test(
      classic_protocol::capabilities::pos::ssl);
  bool client_uses_tls = src_protocol.shared_capabilities().test(
      classic_protocol::capabilities::pos::ssl);

  if (connection()->dest_ssl_mode() == SslMode::kAsClient && client_uses_tls &&
      !server_supports_tls) {
    // config says: do as the client did, and the client did SSL and server
    // doesn't support it -> error

    stage(Stage::Error);

    if (!in_handshake_) {
      on_error_({CR_SSL_CONNECTION_ERROR,
                 "SSL connection error: Requirements can not be satisfied"});

      return Result::Again;
    }

    // send back to the client
    const auto send_res = send_ssl_connection_error_msg(
        src_conn, "SSL connection error: Requirements can not be satisfied");
    if (!send_res) return send_client_failed(send_res.error());

    return Result::SendToClient;
  }

  client_greeting_server_adjust_caps(src_protocol, dst_protocol);

  // use the client-side's capabilities to make sure the server encodes
  // the packets according to the client.
  //
  // src_protocol->shared_caps must be used here as the ->client_caps may
  // contain more than what the router advertised.
  auto client_caps = src_protocol.shared_capabilities();

  if (connection()->context().connection_sharing()) {
    client_caps.set(classic_protocol::capabilities::pos::session_track)
        .set(classic_protocol::capabilities::pos::
                 text_result_with_session_tracking);
  }

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
  if (src_protocol.schema().empty()) {
    client_caps.reset(with_schema_pos);
  } else {
    client_caps.set(with_schema_pos);
  }

  dst_protocol.client_capabilities(client_caps);
  dst_protocol.auth_method_name(src_protocol.auth_method_name());
  dst_protocol.username(src_protocol.username());
  dst_protocol.attributes(src_protocol.attributes());

  connection()->on_handshake_received();

  trace_event_client_greeting_ =
      trace_span(trace_event_greeting_, "mysql/client_greeting");

  if (dst_protocol.shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    stage(Stage::ClientGreetingStartTls);
  } else {
    stage(Stage::ClientGreetingFull);
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerGreetor::client_greeting_start_tls() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->server_conn();
  auto &dst_protocol = dst_conn.protocol();

  if (!src_protocol.client_greeting()) {
    return send_server_failed(make_error_code(std::errc::invalid_argument));
  }

  auto initial_client_greeting_msg = src_protocol.client_greeting().value();

  // setting username == "" leads to a short, switch-to-ssl
  // client::Greeting.
  auto send_res = ClassicFrame::send_msg<
      classic_protocol::borrowed::message::client::Greeting>(
      dst_conn, {
                    dst_protocol.client_capabilities(),
                    initial_client_greeting_msg.max_packet_size(),
                    initial_client_greeting_msg.collation(),
                    "",  // username
                    "",  // auth_method_data
                    "",  // schema
                    "",  // auth_method_name
                    ""   // attributes
                });
  if (!send_res) return send_server_failed(send_res.error());

  connection()->server_conn().protocol().handshake_state(
      ClassicProtocolState::HandshakeState::kClientGreeting);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::greeting (start-tls)"));
  }

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
  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->server_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto client_greeting_msg = src_protocol.client_greeting().value();

  const auto attrs_res = classic_proto_decode_and_add_connection_attributes(
      src_protocol.attributes(),
      vector_splice(src_conn.initial_connection_attributes(),
                    client_ssl_connection_attributes(src_channel.ssl())));
  if (!attrs_res) {
    auto ec = attrs_res.error();
    // if decode/append fails forward the attributes as is. The server should
    // fail too.
    //
    if (src_protocol.client_capabilities().test(
            classic_protocol::capabilities::pos::connect_attributes)) {
      log_warning("%d: decoding connection attributes failed [ignored]: (%s) ",
                  __LINE__, ec.message().c_str());
    }
  }

  client_greeting_msg.capabilities(dst_protocol.client_capabilities());
  client_greeting_msg.username(src_protocol.username());
  client_greeting_msg.schema(src_protocol.schema());

  auto attrs = attrs_res.value_or(src_protocol.attributes());
  dst_protocol.sent_attributes(attrs);
  src_protocol.sent_attributes(attrs);

  client_greeting_msg.attributes(attrs);

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::greeting::plain"));
  }

  if (src_protocol.password().has_value()) {
    // scramble with the server's auth-data to trigger a fast-auth.

    auto pwd = *(src_protocol.password());

    // if the password set and not empty, rehash it.
    if (!pwd.empty()) {
      if (auto scramble_res = scramble_them_all(
              client_greeting_msg.auth_method_name(),
              strip_trailing_null(
                  dst_protocol.server_greeting()->auth_method_data()),
              pwd)) {
        client_greeting_msg.auth_method_data(*scramble_res);
      }
    }
  } else if (src_protocol.auth_method_name() ==
                 AuthCachingSha2Password::kName &&
             !src_channel.ssl() && connection()->greeting_from_router()) {
    // the client tried the fast-auth path and scrambled it with the router's
    // nonce.
    //
    // That will fail on the server side as it used another scramble.
    //
    // replace the auth-method-method to force a "auth-method-switch" which
    // contains the server's nonce.
    client_greeting_msg.auth_method_name("switch_me_if_you_can");
  } else {
    dst_protocol.auth_method_name(src_protocol.auth_method_name());
  }

  return ClassicFrame::send_msg(dst_conn, client_greeting_msg)
      .and_then(
          [this](auto /* sent */) -> stdx::expected<Result, std::error_code> {
            connection()->server_conn().protocol().handshake_state(
                ClassicProtocolState::HandshakeState::kClientGreeting);

            stage(Stage::InitialResponse);

            return Result::SendToServer;
          })
      .or_else([this](auto err) { return send_server_failed(err); });
}

static stdx::expected<TlsClientContext *, std::error_code> get_dest_ssl_ctx(
    MySQLRoutingContext &ctx, const std::string &id) {
  return mysql_harness::make_tcp_address(id).and_then(
      [&ctx, &id](const auto &addr)
          -> stdx::expected<TlsClientContext *, std::error_code> {
        return ctx.dest_ssl_ctx(id, addr.address());
      });
}

stdx::expected<Processor::Result, std::error_code>
ServerGreetor::tls_connect_init() {
  auto &dst_conn = connection()->server_conn();
  auto &dst_channel = dst_conn.channel();
  // auto &dst_protocol = dst_conn.protocol();

  auto tls_client_ctx_res = get_dest_ssl_ctx(
      connection()->context(), connection()->get_destination_id());
  if (!tls_client_ctx_res || tls_client_ctx_res.value() == nullptr ||
      (*tls_client_ctx_res)->get() == nullptr) {
    // shouldn't happen. But if it does, close the connection.
    log_warning("failed to create SSL_CTX");

    return send_server_failed(make_error_code(std::errc::invalid_argument));
  }

  auto *tls_client_ctx = *tls_client_ctx_res;
  auto *ssl_ctx = tls_client_ctx->get();

  dst_channel.init_ssl(ssl_ctx);

  SSL_set_app_data(dst_channel.ssl(), connection());

  SSL_set_msg_callback(dst_channel.ssl(), ssl_msg_cb);
  SSL_set_msg_callback_arg(dst_channel.ssl(), connection());

  // when a connection is taken from the pool for this client-connection ...

  // ... ensure it is TLS again.
  connection()->requires_tls(true);

  // ... ensure it has/hasn't a client cert.
  connection()->requires_client_cert(SSL_get_certificate(dst_channel.ssl()) !=
                                     nullptr);

  trace_event_tls_connect_ =
      trace_span(trace_event_client_greeting_, "mysql/tls_connect");

  tls_client_ctx->get_session().and_then(
      [&](auto *sess) -> stdx::expected<void, std::error_code> {
        SSL_set_session(dst_channel.ssl(), sess);
        return {};
      });

  stage(Stage::TlsConnect);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerGreetor::tls_connect() {
  auto &src_conn = connection()->client_conn();

  auto &dst_conn = connection()->server_conn();
  auto &dst_channel = dst_conn.channel();

  {
    const auto flush_res = dst_channel.flush_from_recv_buf();
    if (!flush_res) {
      auto ec = flush_res.error();
      log_fatal_error_code("tls_connect::recv::flush() failed", ec);

      return recv_server_failed(ec);
    }
  }

  if (!dst_channel.tls_init_is_finished()) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("tls::connect"));
    }

    const auto res = dst_channel.tls_connect();
    if (!res) {
      if (res.error() == TlsErrc::kWantRead) {
        {
          const auto flush_res = dst_channel.flush_to_send_buf();
          if (!flush_res &&
              (flush_res.error() !=
               make_error_condition(std::errc::operation_would_block))) {
            auto ec = flush_res.error();
            log_fatal_error_code("flushing failed", ec);

            return send_server_failed(ec);
          }
        }

        if (!dst_channel.send_buffer().empty()) {
          return Result::SendToServer;
        }

        return Result::RecvFromServer;
      } else {
        // connect may fail fatally if
        //
        // - cert-verification failed.
        // - no shared cipher
        stage(Stage::Error);

        if (!in_handshake_) {
          on_error_({CR_SSL_CONNECTION_ERROR,
                     "connecting to destination failed with TLS error: " +
                         res.error().message()});

          return Result::Again;
        }

        const auto send_res = send_ssl_connection_error_msg(
            src_conn, "connecting to destination failed with TLS error: " +
                          res.error().message());
        if (!send_res) {
          auto ec = send_res.error();
          log_fatal_error_code("sending error failed", ec);

          return send_client_failed(ec);
        }

        if (auto &tr = tracer()) {
          tr.trace(Tracer::Event().stage("server::greeting::error"));
        }

        // close the server-socket as no futher communication is expected.

        return Result::SendToClient;
      }
    }
  }

  if (auto &tr = tracer()) {
    auto *ssl = dst_channel.ssl();
    std::ostringstream oss;
    oss << "tls::connect::ok: " << SSL_get_version(ssl);
    oss << " using " << SSL_get_cipher_name(ssl);
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(3, 0, 0)
    oss << " and " << OBJ_nid2ln(SSL_get_negotiated_group(ssl));
#endif

    if (SSL_session_reused(ssl) != 0) {
      oss << ", session_reused";
    }
    tr.trace(Tracer::Event().stage(oss.str()));
  }

  if (auto *ev = trace_event_tls_connect_) {
    auto *ssl = dst_channel.ssl();
    ev->attrs.emplace_back("tls.version", SSL_get_version(ssl));
    ev->attrs.emplace_back("tls.cipher", SSL_get_cipher_name(ssl));
    ev->attrs.emplace_back("tls.session_reused", SSL_session_reused(ssl) != 0);
    trace_span_end(trace_event_tls_connect_);
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
  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->server_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto client_greeting_msg = *src_protocol.client_greeting();

  const auto attrs_res = classic_proto_decode_and_add_connection_attributes(
      src_protocol.attributes(),
      vector_splice(src_conn.initial_connection_attributes(),
                    client_ssl_connection_attributes(src_channel.ssl())));
  if (!attrs_res) {
    auto ec = attrs_res.error();
    // if decode/append fails forward the attributes as is. The server should
    // fail too.
    //
    if (src_protocol.client_capabilities().test(
            classic_protocol::capabilities::pos::connect_attributes)) {
      log_warning("%d: decoding connection attributes failed [ignored]: (%s) ",
                  __LINE__, ec.message().c_str());
    }
  }

  dst_protocol.username(src_protocol.username());

  auto attrs = attrs_res.value_or(src_protocol.attributes());
  dst_protocol.sent_attributes(attrs);
  src_protocol.sent_attributes(attrs);

  client_greeting_msg.attributes(attrs);

  client_greeting_msg.username(src_protocol.username());
  client_greeting_msg.schema(src_protocol.schema());
  client_greeting_msg.capabilities(dst_protocol.client_capabilities());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::greeting (tls)"));
  }

  if (auto *ev = trace_event_client_greeting_) {
    ev->attrs.emplace_back("db.name", client_greeting_msg.schema());
  }

  if (src_protocol.password().has_value()) {
    // scramble with the server's auth-data to trigger a fast-auth.

    auto pwd = *(src_protocol.password());

    // if the password set and not empty, rehash it.
    if (!pwd.empty()) {
      if (auto scramble_res = scramble_them_all(
              client_greeting_msg.auth_method_name(),
              strip_trailing_null(
                  dst_protocol.server_greeting()->auth_method_data()),
              pwd)) {
        client_greeting_msg.auth_method_data(*scramble_res);
      }
    }
  } else if (src_protocol.auth_method_name() ==
                 AuthCachingSha2Password::kName &&
             !src_channel.ssl() && connection()->greeting_from_router()) {
    // the client tried the fast-auth path and scrambled it with the router's
    // nonce.
    //
    // That will fail on the server side as it used another scramble.
    //
    // replace the auth-method-method to force a "auth-method-switch" which
    // contains the server's nonce.
    client_greeting_msg.auth_method_name("switch_me_if_you_can");
  }

  dst_protocol.auth_method_name(src_protocol.auth_method_name());

  return ClassicFrame::send_msg(dst_conn, client_greeting_msg)
      .and_then(
          [this](auto /* unused */) -> stdx::expected<Result, std::error_code> {
            stage(Stage::InitialResponse);

            return Result::SendToServer;
          })
      .or_else([this](auto err) { return send_server_failed(err); });
}

stdx::expected<Processor::Result, std::error_code>
ServerGreetor::initial_response() {
  trace_span_end(trace_event_client_greeting_);

  auto &src_conn = connection()->client_conn();
  // auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  connection()->push_processor(std::make_unique<AuthForwarder>(
      connection(),
      // password was requested already.
      src_protocol.password() && !src_protocol.password()->empty()));

  stage(Stage::FinalResponse);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerGreetor::final_response() {
  // ERR|OK|EOF|other
  auto &src_conn = connection()->server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  // ensure the recv_buf has at last frame-header (+ msg-byte)
  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) return recv_server_failed(read_res.error());

  connection()->server_conn().protocol().handshake_state(
      ClassicProtocolState::HandshakeState::kFinished);

  const uint8_t msg_type = src_protocol.current_msg_type().value();

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
  const auto &recv_buf = src_channel.recv_plain_view();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ClassicFrame::ensure_has_full_frame(src_conn);

  log_debug(
      "received unexpected message from server after a client::Greeting:\n%s",
      hexify(recv_buf).c_str());

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

/**
 * router<-server: auth error.
 */
stdx::expected<Processor::Result, std::error_code> ServerGreetor::auth_error() {
  auto &src_conn = connection()->server_conn();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::Error>(src_conn);
  if (!msg_res) return recv_client_failed(msg_res.error());

  auto msg = *msg_res;

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("server::auth::error: " +
                                   std::to_string(msg.error_code())));
  }

  trace_span_end(trace_event_greeting_, TraceEvent::StatusCode::kError);

  stage(Stage::Error);

  on_error_({msg.error_code(), std::string(msg.message()),
             std::string(msg.sql_state())});

  discard_current_msg(src_conn);

  if (auto *ssl = connection()->server_conn().channel().ssl()) {
    // shutdown the ssl-session to allow tls-resumption of the session.
    //
    // The socket will be closed in ::error().
    SSL_shutdown(ssl);
  }

  return Result::Again;
}

/**
 * server-side: auth is ok.
 */
stdx::expected<Processor::Result, std::error_code> ServerGreetor::auth_ok() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->client_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Ok>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("server::ok"));
  }

  auto msg = *msg_res;

  if (!msg.session_changes().empty()) {
    (void)connection()->track_session_changes(
        net::buffer(msg.session_changes()), src_protocol.shared_capabilities());
  }

  dst_protocol.status_flags(msg.status_flags());

  // if the server accepted the schema, track it.
  if (src_protocol.shared_capabilities().test(
          classic_protocol::capabilities::pos::connect_with_schema)) {
    src_protocol.schema(dst_protocol.schema());
  } else {
    src_protocol.schema("");
  }

  stage(Stage::Ok);

  discard_current_msg(src_conn);
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

  return socket_reconnect_start(nullptr);
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstConnector::server_greeting() {
  // ConnectProcessor either:
  //
  // - closes the connection and sends an error to the client, or
  // - keeps the connection open.
  auto &server_conn = connection()->server_conn();

  if (!server_conn.is_open()) {
    auto &src_conn = connection()->client_conn();

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("connect::error"));
    }

    stage(Stage::Error);

    return reconnect_send_error_msg(src_conn);
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("server::greeting"));
  }

  stage(Stage::ServerGreeted);

  // the client hasn't started the handshake yet, therefore it isn't
  // "in_handshake"
  connection()->push_processor(std::make_unique<ServerGreetor>(
      connection(), false,
      [this](const classic_protocol::message::server::Error &err) {
        this->reconnect_error(err);
      },
      nullptr));

  return Result::Again;
}

/**
 * received an server::greeting or server::error from the server.
 */
stdx::expected<Processor::Result, std::error_code>
ServerFirstConnector::server_greeted() {
  auto &server_conn = connection()->server_conn();

  if (!server_conn.is_open()) {
    auto &src_conn = connection()->client_conn();

    auto ec = reconnect_error();

    if (connect_error_is_transient(ec) &&
        std::chrono::steady_clock::now() <
            started_ + connection()->context().connect_retry_timeout()) {
      stage(Stage::Connect);

      connection()->connect_timer().expires_after(kConnectRetryInterval);
      connection()->connect_timer().async_wait([this](std::error_code ec) {
        if (ec) return;

        connection()->resume();
      });

      return Result::Suspend;
    }

    stage(Stage::Error);

    if (log_level_is_handled(mysql_harness::logging::LogLevel::kDebug)) {
      // RouterRoutingTest.RoutingTooManyServerConnections expects this
      // message.
      log_debug(
          "Error from the server while waiting for greetings message: "
          "%u, '%s'",
          ec.error_code(), ec.message().c_str());
    }

    return reconnect_send_error_msg(src_conn);
  }

  stage(Stage::Ok);
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
    case Stage::FetchUserAttrs:
      return fetch_user_attrs();
    case Stage::FetchUserAttrsDone:
      return fetch_user_attrs_done();

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
    ClassicProtocolState &src_protocol, ClassicProtocolState &dst_protocol) {
  auto client_caps = src_protocol.client_capabilities();

  if (!src_protocol.shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    auto client_greeting_msg = src_protocol.client_greeting().value();

    auto attrs_res = classic_proto_decode_and_add_connection_attributes(
        src_protocol.attributes(),
        connection()->client_conn().initial_connection_attributes());

    auto attrs = attrs_res.value_or(src_protocol.attributes());
    dst_protocol.sent_attributes(attrs);
    src_protocol.sent_attributes(attrs);

    client_greeting_msg.attributes(attrs);

    // client hasn't set the SSL cap, this is the real client greeting
    dst_protocol.client_greeting(client_greeting_msg);
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
      if (dst_protocol.server_capabilities().test(
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
  dst_protocol.client_capabilities(client_caps);
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::client_greeting() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->server_conn();
  auto &dst_protocol = dst_conn.protocol();

  const bool server_supports_tls = dst_protocol.server_capabilities().test(
      classic_protocol::capabilities::pos::ssl);
  const bool client_uses_tls = src_protocol.shared_capabilities().test(
      classic_protocol::capabilities::pos::ssl);
  const bool client_is_secure =
      client_uses_tls || src_conn.is_secure_transport();

  if (connection()->dest_ssl_mode() == SslMode::kAsClient && client_uses_tls &&
      !server_supports_tls) {
    // config says: do as the client did, and the client did SSL and server
    // doesn't support it -> error

    // send back to the client
    const auto send_res = send_ssl_connection_error_msg(
        src_conn, "SSL connection error: Requirements can not be satisfied");
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
  auto client_caps = src_protocol.shared_capabilities();

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

  dst_protocol.client_capabilities(client_caps);
  dst_protocol.auth_method_name(src_protocol.auth_method_name());
  dst_protocol.username(src_protocol.username());
  dst_protocol.attributes(src_protocol.attributes());

  // the client greeting was received and will be forwarded to the server
  // soon.
  connection()->on_handshake_received();

  if (dst_protocol.shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    stage(Stage::ClientGreetingStartTls);

  } else {
    stage(Stage::ClientGreetingFull);
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::client_greeting_start_tls() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->server_conn();
  auto &dst_protocol = dst_conn.protocol();

  if (!src_protocol.client_greeting()) {
    return send_server_failed(make_error_code(std::errc::invalid_argument));
  }

  auto initial_client_greeting_msg = src_protocol.client_greeting().value();

  // use the shared capabilities of the client<->router connection as basis
  auto client_caps = src_protocol.shared_capabilities();

  client_caps.set(classic_protocol::capabilities::pos::ssl);

  dst_protocol.client_capabilities(client_caps);

  // setting username == "" leads to a short, switch-to-ssl
  // client::Greeting.
  auto send_res = ClassicFrame::send_msg<
      classic_protocol::borrowed::message::client::Greeting>(
      dst_conn, {
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
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("client::greeting (forward-tls)"));
    }

    // whatever happens next is encrypted and will not be treated as
    // connection-error.
    connection()->client_conn().protocol().handshake_state(
        ClassicProtocolState::HandshakeState::kFinished);
    connection()->server_conn().protocol().handshake_state(
        ClassicProtocolState::HandshakeState::kFinished);

    stage(Stage::TlsForwardInit);
  } else {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("client::greeting (start-tls)"));
    }

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
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::greeting (full)"));
  }

  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->server_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto client_greeting_msg = src_protocol.client_greeting().value();

  const auto attrs_res = classic_proto_decode_and_add_connection_attributes(
      src_protocol.attributes(),
      vector_splice(src_conn.initial_connection_attributes(),
                    client_ssl_connection_attributes(src_channel.ssl())));
  if (!attrs_res) {
    auto ec = attrs_res.error();
    // if decode/append fails forward the attributes as is. The server should
    // fail too.
    //
    if (src_protocol.client_capabilities().test(
            classic_protocol::capabilities::pos::connect_attributes)) {
      log_warning("%d: decoding connection attributes failed [ignored]: (%s) ",
                  __LINE__, ec.message().c_str());
    }
  }

  auto attrs = attrs_res.value_or(src_protocol.attributes());
  dst_protocol.sent_attributes(attrs);
  src_protocol.sent_attributes(attrs);

  client_greeting_msg.capabilities(dst_protocol.client_capabilities());
  client_greeting_msg.attributes(attrs);

  if (src_protocol.password().has_value()) {
    // scramble with the server's auth-data to trigger a fast-auth.

    auto pwd = *(src_protocol.password());

    // if the password set and not empty, rehash it.
    if (!pwd.empty()) {
      if (auto scramble_res = scramble_them_all(
              client_greeting_msg.auth_method_name(),
              strip_trailing_null(
                  dst_protocol.server_greeting()->auth_method_data()),
              pwd)) {
        client_greeting_msg.auth_method_data(*scramble_res);
      }
    }
  }

  dst_protocol.auth_method_name(src_protocol.auth_method_name());

  return ClassicFrame::send_msg(dst_conn, client_greeting_msg)
      .and_then(
          [this](auto /* unused */) -> stdx::expected<Result, std::error_code> {
            stage(Stage::InitialResponse);

            return Result::SendToServer;
          })
      .or_else([this](auto err) { return send_server_failed(err); });
}

static TlsErrc forward_tls(Channel &src_channel, Channel &dst_channel) {
  // at least the TLS record header.
  const size_t tls_header_size{5};
  const size_t tls_type_offset{5};

  src_channel.read_to_plain(tls_header_size);

  const auto &plain = src_channel.recv_plain_view();
  while (plain.size() >= tls_header_size) {
    // plain is TLS traffic.
    const uint8_t tls_content_type = plain[0];
    const uint16_t tls_payload_size = (plain[3] << 8) | plain[4];

    if (plain.size() < tls_header_size + tls_payload_size) {
      src_channel.read_to_plain(tls_header_size + tls_payload_size -
                                plain.size());
    }

    if (plain.size() < tls_header_size + tls_payload_size) {
      // there isn't the full frame yet.
      return TlsErrc::kWantRead;
    }

    const auto write_res = dst_channel.write(
        net::buffer(plain.subspan(0, tls_header_size + tls_payload_size)));
    if (!write_res) return TlsErrc::kWantWrite;

    // if TlsAlert in handshake, the connection goes back to plain
    if (static_cast<TlsContentType>(tls_content_type) ==
            TlsContentType::kAlert &&
        plain.size() > tls_type_offset && plain[tls_type_offset] == 0x02) {
      src_channel.is_tls(false);
      dst_channel.is_tls(false);
    }

    src_channel.consume_plain(*write_res);
  }

  // want more
  return TlsErrc::kWantRead;
}

template <bool toServer>
class SendProcessor : public Processor {
 public:
  explicit SendProcessor(MysqlRoutingClassicConnectionBase *conn)
      : Processor(conn) {}

  stdx::expected<Result, std::error_code> process() override {
    auto &dst_channel = toServer ? connection()->server_conn().channel()
                                 : connection()->client_conn().channel();

    if (dst_channel.send_buffer().empty()) return Result::Done;

    return toServer ? Result::SendToServer : Result::SendToClient;
  }
};

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::tls_forward() {
  connection()->push_processor(std::make_unique<AwaitClientOrServerProcessor>(
      connection(), [this](auto result) {
        if (!result) return;

        switch (*result) {
          case AwaitClientOrServerProcessor::AwaitResult::ClientReadable: {
            auto &src_conn = connection()->client_conn();
            auto &src_channel = src_conn.channel();

            auto &dst_conn = connection()->server_conn();
            auto &dst_channel = dst_conn.channel();

            forward_tls(src_channel, dst_channel);

            if (!dst_channel.send_buffer().empty()) {
              connection()->push_processor(
                  std::make_unique<SendProcessor<true>>(connection()));
            }
            return;
          }
          case AwaitClientOrServerProcessor::AwaitResult::ServerReadable: {
            auto &src_conn = connection()->server_conn();
            auto &src_channel = src_conn.channel();

            auto &dst_conn = connection()->client_conn();
            auto &dst_channel = dst_conn.channel();

            forward_tls(src_channel, dst_channel);

            if (!dst_channel.send_buffer().empty()) {
              connection()->push_processor(
                  std::make_unique<SendProcessor<false>>(connection()));
            }

            return;
          }
        }

        harness_assert_this_should_not_execute();
      }));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::tls_forward_init() {
  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();

  auto &dst_conn = connection()->server_conn();
  auto &dst_channel = dst_conn.channel();

  dst_channel.is_tls(true);
  src_channel.is_tls(true);

  // if there is already data in the recv-buffer, forward that.
  forward_tls(src_channel, dst_channel);
  if (!dst_channel.send_buffer().empty()) {
    return Result::SendToServer;
  }

  stage(Stage::TlsForward);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::tls_connect_init() {
  auto &dst_conn = connection()->server_conn();
  auto &dst_channel = dst_conn.channel();

  auto tls_client_ctx_res = get_dest_ssl_ctx(
      connection()->context(), connection()->get_destination_id());
  if (!tls_client_ctx_res || tls_client_ctx_res.value() == nullptr ||
      (*tls_client_ctx_res)->get() == nullptr) {
    // shouldn't happen. But if it does, close the connection.
    log_warning("failed to create SSL_CTX");

    return send_server_failed(make_error_code(std::errc::invalid_argument));
  }

  auto *tls_client_ctx = *tls_client_ctx_res;
  auto *ssl_ctx = tls_client_ctx->get();

  dst_channel.init_ssl(ssl_ctx);

  SSL_set_app_data(dst_channel.ssl(), connection());

  SSL_set_msg_callback(dst_channel.ssl(), ssl_msg_cb);
  SSL_set_msg_callback_arg(dst_channel.ssl(), connection());

  // when a connection is taken from the pool for this client-connection ...

  // ... ensure it is TLS again.
  connection()->requires_tls(true);

  // ... ensure it has/hasn't a client cert.
  connection()->requires_client_cert(SSL_get_certificate(dst_channel.ssl()) !=
                                     nullptr);

  tls_client_ctx->get_session().and_then(
      [&](auto *sess) -> stdx::expected<void, std::error_code> {
        SSL_set_session(dst_channel.ssl(), sess);
        return {};
      });

  stage(Stage::TlsConnect);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::tls_connect() {
  auto &src_conn = connection()->client_conn();

  auto &dst_conn = connection()->server_conn();
  auto &dst_channel = dst_conn.channel();

  {
    const auto flush_res = dst_channel.flush_from_recv_buf();
    if (!flush_res) {
      auto ec = flush_res.error();
      log_fatal_error_code("tls_connect::recv::flush() failed", ec);

      return recv_server_failed(ec);
    }
  }

  if (!dst_channel.tls_init_is_finished()) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("tls::connect"));
    }

    const auto res = dst_channel.tls_connect();

    if (!res) {
      if (res.error() == TlsErrc::kWantRead) {
        {
          const auto flush_res = dst_channel.flush_to_send_buf();
          if (!flush_res &&
              (flush_res.error() !=
               make_error_condition(std::errc::operation_would_block))) {
            auto ec = flush_res.error();
            log_fatal_error_code("flushing failed", ec);

            return send_server_failed(ec);
          }
        }

        if (!dst_channel.send_buffer().empty()) {
          return Result::SendToServer;
        }
        return Result::RecvFromServer;
      } else {
        // connect may fail fatally if
        //
        // - cert-verification failed.
        // - no shared cipher

        const auto send_res = send_ssl_connection_error_msg(
            src_conn, "connecting to destination failed with TLS error: " +
                          res.error().message());
        if (!send_res) {
          auto ec = send_res.error();
          log_fatal_error_code("sending error failed", ec);

          return send_client_failed(ec);
        }

        if (auto &tr = tracer()) {
          tr.trace(Tracer::Event().stage("server::greeting::error"));
        }

        // close the server-socket as no futher communication is expected.
        (void)connection()->server_conn().close();

        stage(Stage::Error);
        return Result::SendToClient;
      }
    }
  }

  if (auto &tr = tracer()) {
    auto *ssl = dst_channel.ssl();
    std::ostringstream oss;
    oss << "tls::connect::ok: " << SSL_get_version(ssl);
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
  // tls is established to the server, send the client::greeting
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::client_greeting_after_tls() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("client::greeting(first)"));
  }

  auto &src_conn = connection()->client_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->server_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto client_greeting_msg = *src_protocol.client_greeting();

  const auto attrs_res = classic_proto_decode_and_add_connection_attributes(
      src_protocol.attributes(),
      vector_splice(src_conn.initial_connection_attributes(),
                    client_ssl_connection_attributes(src_channel.ssl())));
  if (!attrs_res) {
    auto ec = attrs_res.error();
    // if decode/append fails forward the attributes as is. The server should
    // fail too.
    //
    if (src_protocol.client_capabilities().test(
            classic_protocol::capabilities::pos::connect_attributes)) {
      log_warning("%d: decoding connection attributes failed [ignored]: (%s) ",
                  __LINE__, ec.message().c_str());
    }
  }

  dst_protocol.username(client_greeting_msg.username());

  auto attrs = attrs_res.value_or(src_protocol.attributes());
  dst_protocol.sent_attributes(attrs);
  src_protocol.sent_attributes(attrs);

  // the client's attributes, as they are sent to the server.

  client_greeting_msg.capabilities(dst_protocol.client_capabilities());
  client_greeting_msg.attributes(attrs);

  if (src_protocol.password().has_value()) {
    // scramble with the server's auth-data to trigger a fast-auth.

    auto pwd = *(src_protocol.password());

    // if the password set and not empty, rehash it.
    if (!pwd.empty()) {
      if (auto scramble_res = scramble_them_all(
              client_greeting_msg.auth_method_name(),
              strip_trailing_null(
                  dst_protocol.server_greeting()->auth_method_data()),
              pwd)) {
        client_greeting_msg.auth_method_data(*scramble_res);
      }
    }
  }

  return ClassicFrame::send_msg(dst_conn, client_greeting_msg)
      .and_then(
          [this](auto /* unused */) -> stdx::expected<Result, std::error_code> {
            stage(Stage::InitialResponse);

            return Result::SendToServer;
          })
      .or_else([this](auto err) { return send_server_failed(err); });
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::initial_response() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  connection()->push_processor(std::make_unique<AuthForwarder>(
      connection(),
      // password was requested already.
      src_protocol.password() && !src_protocol.password()->empty()));

  stage(Stage::FinalResponse);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::final_response() {
  // ERR|OK|EOF|other
  auto &src_conn = connection()->server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  // ensure the recv_buf has at last frame-header (+ msg-byte)
  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) return recv_server_failed(read_res.error());

  connection()->server_conn().protocol().handshake_state(
      ClassicProtocolState::HandshakeState::kFinished);

  const uint8_t msg_type = src_protocol.current_msg_type().value();

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
  const auto &recv_buf = src_channel.recv_plain_view();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ClassicFrame::ensure_has_full_frame(src_conn);

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
  auto &src_conn = connection()->server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::Error>(src_channel,
                                                          src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto msg = *msg_res;

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("server::auth::error: " +
                                   std::to_string(msg.error_code())));
  }

  stage(Stage::Error);  // close the server connection after the Error msg was
                        // sent.

  on_error_({msg.error_code(), std::string(msg.message()),
             std::string(msg.sql_state())});

  return Result::Again;
}

/**
 * server-side: auth is ok.
 */
stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::auth_ok() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Ok>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("server::ok"));
  }

  auto msg = *msg_res;

  if (!msg.session_changes().empty()) {
    (void)connection()->track_session_changes(
        net::buffer(msg.session_changes()), src_protocol.shared_capabilities());
  }

  if (connection()->context().router_require_enforce()) {
    discard_current_msg(src_conn);

    // fetch the user-vars.

    stage(Stage::FetchUserAttrs);

    return Result::Again;
  }

  stage(Stage::Ok);

  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::fetch_user_attrs() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("server::fetch_user_attrs"));
  }

  RouterRequireFetcher::push_processor(
      connection(), required_connection_attributes_fetcher_result_);

  stage(Stage::FetchUserAttrsDone);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ServerFirstAuthenticator::fetch_user_attrs_done() {
  auto &dst_conn = connection()->client_conn();
  auto &dst_channel = dst_conn.channel();
  auto &dst_protocol = dst_conn.protocol();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("server::fetch_user_attrs::done"));
  }

  if (!required_connection_attributes_fetcher_result_) {
    auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Error>(
        dst_conn, {1045, "Access denied", "28000"});
    if (!send_res) return stdx::unexpected(send_res.error());

    stage(Stage::Error);
    return Result::SendToClient;
  }

  auto enforce_res = RouterRequire::enforce(
      dst_channel, *required_connection_attributes_fetcher_result_);
  if (!enforce_res) {
    auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Error>(
        dst_conn, {1045, "Access denied", "28000"});
    if (!send_res) return stdx::unexpected(send_res.error());

    stage(Stage::Error);
    return Result::SendToClient;
  }

  auto send_res =
      ClassicFrame::send_msg<classic_protocol::borrowed::message::server::Ok>(
          dst_conn, {0, 0, dst_protocol.status_flags(), 0});
  if (!send_res) return stdx::unexpected(send_res.error());

  stage(Stage::Ok);
  return Result::SendToClient;
}
