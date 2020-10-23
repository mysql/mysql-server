/*
  Copyright (c) 2020, Oracle and/or its affiliates.

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

#include "classic_protocol_splicer.h"

#include <system_error>  // error_code

#include <openssl/ssl.h>  // SSL_get_version

#include "my_inttypes.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqlrouter/classic_protocol_codec_base.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/classic_protocol_wire.h"
#include "ssl_mode.h"

IMPORT_LOG_FUNCTIONS()

// enable to get in-depth debug messages of the protocol flow.
#undef DEBUG_STATE

/**
 * log error-msg with error code and set the connection to its FINISH state.
 */
static BasicSplicer::State log_fatal_error_code(const char *msg,
                                                std::error_code ec) {
  log_warning("%s: %s (%s:%d)", msg, ec.message().c_str(), ec.category().name(),
              ec.value());

  return BasicSplicer::State::FINISH;
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
 * decode connection attributes.
 *
 *
 */
static stdx::expected<size_t, std::error_code>
classic_proto_decode_and_add_connection_attributes(
    classic_protocol::message::client::Greeting &client_greeting_msg,
    const std::vector<std::pair<std::string, std::string>> &session_attributes,
    const std::string &client_ssl_cipher,
    const std::string &client_ssl_version) {
  // add attributes if they are sane.
  auto attrs = client_greeting_msg.attributes();

  // track if each key has a matching value.
  bool is_key{true};
  auto attr_buf = net::buffer(attrs);
  do {
    const auto decode_res =
        classic_protocol::decode<classic_protocol::wire::VarString>(attr_buf,
                                                                    {});
    if (!decode_res) {
      return decode_res.get_unexpected();
    }

    const auto bytes_read = decode_res->first;
    const auto kv = decode_res->second;

    attr_buf += bytes_read;

    // toggle the key/value tracker.
    is_key = !is_key;
  } while (net::buffer_size(attr_buf) != 0);

  // if the last key doesn't have a value, don't append our attributes.
  if (!is_key || net::buffer_size(attr_buf) != 0) {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  size_t bytes_appended{};
  for (const auto &attr : session_attributes) {
    const auto append_res =
        classic_proto_append_attribute(attrs, attr.first, attr.second);
    if (!append_res) return append_res.get_unexpected();
    bytes_appended += append_res.value();
  }

  {
    const auto append_res = classic_proto_append_attribute(
        attrs, "_client_ssl_cipher", client_ssl_cipher);
    if (!append_res) return append_res.get_unexpected();
    bytes_appended += append_res.value();
  }

  {
    const auto append_res = classic_proto_append_attribute(
        attrs, "_client_ssl_version", client_ssl_version);
    if (!append_res) return append_res.get_unexpected();
    bytes_appended += append_res.value();
  }

  client_greeting_msg.attributes(attrs);

  return {bytes_appended};
}

/**
 * append router specific connection attributes.
 *
 * @param[in,out] client_greeting_msg a Client Greeting message
 * @param[in] session_attributes session attributes to add.
 * @param[in] ssl pointer to a SSL struct of the client connection. May be
 * nullptr.
 *
 * @return appended bytes on success, std:error_code on failure
 */
static stdx::expected<size_t, std::error_code>
classic_proto_decode_and_add_connection_attributes(
    classic_protocol::message::client::Greeting &client_greeting_msg,
    const std::vector<std::pair<std::string, std::string>> &session_attributes,
    const SSL *ssl) {
  if (ssl != nullptr) {
    return classic_proto_decode_and_add_connection_attributes(
        client_greeting_msg, session_attributes, SSL_get_cipher_name(ssl),
        SSL_get_version(ssl));
  } else {
    return classic_proto_decode_and_add_connection_attributes(
        client_greeting_msg, session_attributes, "", "");
  }
}

BasicSplicer::State ClassicProtocolSplicer::server_greeting() {
  // wait for the server message is complete.
  if (server_channel()->recv_buffer().empty()) {
    server_channel()->want_recv(4);
    return state();
  }

  // decode server-greeting msg from frame.
  const auto decode_res =
      classic_protocol::decode<classic_protocol::frame::Frame<
          classic_protocol::message::server::Greeting>>(
          net::buffer(server_channel()->recv_buffer()), 0);
  if (!decode_res) {
    if (decode_res.error() == classic_protocol::codec_errc::not_enough_input) {
      server_channel()->want_recv(1);
      return state();
    }

    log_debug("decoding server greeting failed: %s",
              decode_res.error().message().c_str());

    return State::FINISH;
  }

  const auto seq_id = decode_res->second.seq_id();
  if (seq_id != 0) {
    // expected seq-id to be 0.
    log_debug(
        "server-greeting's seq-id isn't the expected 0. Dropping connection.");

    return State::FINISH;
  }

#if defined(DEBUG_STATE)
  log_debug("client-ssl-mode=%s, server-ssl-mode=%s",
            ssl_mode_to_string(source_ssl_mode()),
            ssl_mode_to_string(dest_ssl_mode()));
#endif

  auto server_greeting_msg = decode_res->second.payload();
  auto caps = server_greeting_msg.capabilities();

  server_protocol_->seq_id(seq_id);
  server_protocol_->server_capabilities(caps);
  server_protocol_->server_greeting(server_greeting_msg);

  if (source_ssl_mode() != SslMode::kPassthrough) {
    // disable compression as we don't support it yet.
    caps.reset(classic_protocol::capabilities::pos::compress);
    caps.reset(classic_protocol::capabilities::pos::compress_zstd);

    if (source_ssl_mode() == SslMode::kDisabled) {
      // server supports SSL, but client should be forced to be unencrypted.
      //
      // disabling will pretend the server doesn't speak SSL
      //
      // if the client uses SslMode::kPreferred or kDisabled, it will use an
      // unencrypted connection otherwise it will about the connection.
      caps.reset(classic_protocol::capabilities::pos::ssl);
    } else if (source_ssl_mode() == SslMode::kRequired) {
      // config requires: client MUST be encrypted.
      //
      // if the server hasn't set it yet, set it.
      caps.set(classic_protocol::capabilities::pos::ssl);
    } else if (source_ssl_mode() == SslMode::kPreferred) {
      // force-set the ssl-cap for the client-side only if we later don't have
      // to use AS_CLIENT when speaking to a non-TLS server.
      if (dest_ssl_mode() != SslMode::kAsClient) {
        caps.set(classic_protocol::capabilities::pos::ssl);
      }
    }
  }
  client_protocol_->server_capabilities(caps);
  client_protocol_->seq_id(server_protocol_->seq_id());

  if ((dest_ssl_mode() == SslMode::kRequired) &&
      !server_protocol_->server_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    // destination does not support TLS, but config requires encrypted.
    log_debug(
        "server_ssl_mode=REQUIRED, but destination doesn't support "
        "encryption.");

    // encode directly into the send-buffer as connection is still plaintext.
    const auto encode_res = classic_protocol::encode(
        classic_protocol::frame::Frame<
            classic_protocol::message::server::Error>(
            client_protocol_->seq_id(),
            {2026,
             "SSL connection error: SSL is required by router, but the "
             "server doesn't support it"}),
        {}, net::dynamic_buffer(client_channel()->send_buffer()));
    if (!encode_res) {
      return log_fatal_error_code("encoding error failed", encode_res.error());
    }

    return State::FINISH;
  }

#if defined(DEBUG_STATE)
  log_debug("%d: dest::server-caps: %s", __LINE__,
            server_protocol()->server_capabilities().to_string().c_str());
  log_debug("%d: src::server-caps: %s", __LINE__,
            client_protocol()->server_capabilities().to_string().c_str());

  log_debug("%d: .. %s", __LINE__, state_to_string(state()));
#endif
  if (server_protocol_->server_capabilities() !=
      client_protocol_->server_capabilities()) {
    // build a new greeting.
    server_greeting_msg.capabilities(client_protocol_->server_capabilities());

    const auto encode_res = classic_protocol::encode(
        classic_protocol::frame::Frame<
            classic_protocol::message::server::Greeting>(0,
                                                         server_greeting_msg),
        {}, net::dynamic_buffer(client_channel()->send_buffer()));
    if (!encode_res) {
      return log_fatal_error_code("encoding server-greeting failed",
                                  encode_res.error());
    }

    if (client_channel()->send_buffer().empty()) {
      log_debug(
          "encoding server greeting succeeded, but send-buffer is empty.");
      return State::FINISH;
    }

    // reset the server-side recv-buffer.
    server_channel()->recv_buffer().clear();
  } else {
    // forward the packet AS IS
    move_buffer(net::dynamic_buffer(client_channel()->send_buffer()),
                net::dynamic_buffer(server_channel()->recv_buffer()));
  }

#if defined(DEBUG_STATE)
  log_debug("%d: << %s", __LINE__, state_to_string(state()));
#endif
  return State::CLIENT_GREETING;
}

BasicSplicer::State ClassicProtocolSplicer::client_greeting() {
  auto *src_channel = client_channel();
  auto &recv_buf = src_channel->recv_buffer();

  if (recv_buf.empty()) {
    src_channel->want_recv(1);
    return state();
  }

#if defined(DEBUG_STATE)
  log_debug("%d: >> %s", __LINE__, state_to_string(state()));
#endif

  auto header_decode_res =
      classic_protocol::decode<classic_protocol::frame::Header>(
          net::buffer(recv_buf), client_protocol()->server_capabilities());
  if (!header_decode_res) {
    log_debug("%d: >> %s: %s", __LINE__, state_to_string(state()),
              header_decode_res.error().message().c_str());

    if (header_decode_res.error() ==
        classic_protocol::codec_errc::not_enough_input) {
      src_channel->want_recv(1);
      return state();
    }

    log_debug("decoding server greeting failed: %s",
              header_decode_res.error().message().c_str());

    return State::FINISH;
  }

  auto header_size = header_decode_res->first;
  auto hdr = header_decode_res->second;
  const auto payload_size = hdr.payload_size();

  if (payload_size == 0) {
    // invalid packet size.
    return State::FINISH;
  }
  if (hdr.seq_id() != 1) {
    // client-greeting has seq-id 1
    return State::FINISH;
  }

  auto payload_buffer = net::buffer(net::buffer(recv_buf) + header_size);
  if (payload_buffer.size() < hdr.payload_size()) {
    src_channel->want_recv(1);
    return state();
  }

  auto payload_decode_res =
      classic_protocol::decode<classic_protocol::message::client::Greeting>(
          net::buffer(payload_buffer, payload_size),
          client_protocol()->server_capabilities());
  if (!payload_decode_res) {
    if (payload_decode_res.error() ==
        classic_protocol::codec_errc::not_enough_input) {
      src_channel->want_recv(1);
      return state();
    }

    log_debug("decoding server greeting failed: %s",
              payload_decode_res.error().message().c_str());

    return State::FINISH;
  }

#if defined(DEBUG_STATE)
  log_debug("%d: .. %s", __LINE__, state_to_string(state()));
#endif

  auto client_greeting_msg = payload_decode_res->second;
  auto caps = client_greeting_msg.capabilities();

  client_protocol()->client_capabilities(caps);
  client_protocol()->seq_id(1);

  if (!client_protocol()->shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    if (source_ssl_mode() == SslMode::kRequired) {
      // config says: client->router MUST be encrypted, but client didn't set
      // the SSL cap.
      const auto encode_res = classic_protocol::encode(
          classic_protocol::frame::Frame<
              classic_protocol::message::server::Error>(
              ++client_protocol_->seq_id(),
              {2026, "SSL connection error: SSL is required from client"}),
          {}, net::dynamic_buffer(client_channel()->send_buffer()));
      if (!encode_res) {
        return log_fatal_error_code("encoding error failed",
                                    encode_res.error());
      }

      return State::FINISH;
    }
    client_protocol()->client_greeting(client_greeting_msg);

    classic_proto_decode_and_add_connection_attributes(
        client_greeting_msg, session_attributes(), client_channel()->ssl());

    // client hasn't set the SSL cap, this is the real client greeting
    server_protocol()->client_greeting(client_greeting_msg);
  }

  if (dest_ssl_mode() == SslMode::kDisabled) {
    // config says: communication to server is unencrypted
    caps.reset(classic_protocol::capabilities::pos::ssl);
  } else if (dest_ssl_mode() == SslMode::kRequired) {
    // config says: communication to server must be encrypted
    caps.set(classic_protocol::capabilities::pos::ssl);
  } else if (dest_ssl_mode() == SslMode::kPreferred) {
    // config says: communication to server should be encrypted if server
    // supports it.
    if (server_protocol()->server_capabilities().test(
            classic_protocol::capabilities::pos::ssl)) {
      caps.set(classic_protocol::capabilities::pos::ssl);
    }
  }
  server_protocol()->client_capabilities(caps);

#if defined(DEBUG_STATE)
  log_debug("%d: dest::client-caps: %s", __LINE__,
            server_protocol()->client_capabilities().to_string().c_str());
  log_debug("%d: dest::server-caps: %s", __LINE__,
            server_protocol()->server_capabilities().to_string().c_str());
  log_debug("%d: dest::shared-caps: %s", __LINE__,
            server_protocol()->shared_capabilities().to_string().c_str());
#endif

  // client was ok.
  handshake_done(true);

  if (client_protocol()->shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl) &&
      !server_protocol()->shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    // client sent an greeting-tls packet, but the server side is
    // unencrypted.
    //
    // we'll get the real greeting of the TLS-accept stage finished.
#if defined(DEBUG_STATE)
    log_debug("%d: >> %s (skipping client-greeting)", __LINE__,
              state_to_string(state()));
#endif

    // consume the msg from the recv-buffer.
    net::dynamic_buffer(recv_buf).consume(header_size + payload_size);
  } else if ((client_protocol()->client_capabilities() !=
              server_protocol()->client_capabilities()) ||
             (server_protocol()->client_greeting() &&
              (client_protocol()->client_greeting()->attributes() !=
               server_protocol()->client_greeting()->attributes()))) {
    // something changed, encode the greeting packet instead of reusing the one
    // the client sent.
    client_greeting_msg.capabilities(caps);

    const auto encode_res = classic_protocol::encode(
        classic_protocol::frame::Frame<
            classic_protocol::message::client::Greeting>(
            ++server_protocol()->seq_id(), client_greeting_msg),
        server_protocol()->server_capabilities(),
        net::dynamic_buffer(server_channel()->send_buffer()));
    if (!encode_res) {
      return log_fatal_error_code("encoding client-greeting failed",
                                  encode_res.error());
    }

    if (server_channel()->send_buffer().empty()) {
      // encoding succeeded, but no payload?
      log_debug(
          "encoding server greeting succeeded, but send-buffer is empty.");
      return State::FINISH;
    }

    if (!server_protocol()->shared_capabilities().test(
            classic_protocol::capabilities::pos::ssl)) {
      // SSL isn't enabled on the server side. The real client-greeting.
      server_protocol()->client_greeting(client_greeting_msg);
    }

    // consume the msg from the recv-buffer.
    net::dynamic_buffer(recv_buf).consume(header_size + payload_size);
  } else {
    // remember client-greeting
    server_protocol()->client_greeting(client_greeting_msg);
    server_protocol()->seq_id(client_protocol()->seq_id());

    // forward the client greeting to the server-side
    move_buffer(net::dynamic_buffer(server_channel()->send_buffer()),
                net::dynamic_buffer(recv_buf), header_size + payload_size);
  }

  if (client_protocol()->shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    client_channel()->is_tls(true);

    if (source_ssl_mode() == SslMode::kPassthrough) {
      // passthrough, but let the channel know that the frame as TLS now.
      server_channel()->is_tls(true);

      return State::SPLICE_INIT;
    } else {
      // tls <-> (any)
      client_channel()->init_ssl(client_ssl_ctx_getter_());

      return State::TLS_ACCEPT;
    }
  } else if (server_protocol()->shared_capabilities().test(
                 classic_protocol::capabilities::pos::ssl)) {
    // plain <-> tls
    server_channel()->is_tls(true);

    // open an TLS endpoint to the server.
    server_channel()->init_ssl(server_ssl_ctx_getter_());

    return State::TLS_CONNECT;
  } else {
    // plain <-> plain
    return State::SPLICE_INIT;
  }
}

BasicSplicer::State ClassicProtocolSplicer::tls_client_greeting() {
#if defined(DEBUG_STATE)
  log_debug("%d: >> %s", __LINE__, state_to_string(state()));
#endif
  // write socket data to SSL struct
  auto *src_channel = client_channel();

  {
    auto flush_res = src_channel->flush_from_recv_buf();
    if (!flush_res) {
      log_debug("tls_client_greeting::recv::flush() failed: %s (%d)",
                flush_res.error().message().c_str(), flush_res.error().value());

      return State::FINISH;
    }
  }

#if defined(DEBUG_STATE)
  log_debug("%d: << %s", __LINE__, state_to_string(state()));
#endif

  auto &plain_buf = src_channel->recv_plain_buffer();
  auto dyn_buf = net::dynamic_buffer(plain_buf);

  // append to the plain buffer
  const auto read_res = src_channel->read(dyn_buf);
  if (!read_res) {
    if (read_res.error() == std::errc::operation_would_block ||
        read_res.error() == TlsErrc::kWantRead) {
      // want to read some more
      src_channel->want_recv(1);
      return state();
    }

    log_debug("reading client-greeting from TLS failed: %s",
              read_res.error().message().c_str());

    return State::FINISH;
  }

  auto decode_res = classic_protocol::decode<classic_protocol::frame::Frame<
      classic_protocol::message::client::Greeting>>(
      net::buffer(plain_buf), client_protocol()->server_capabilities());
  if (!decode_res) {
    if (decode_res.error() == classic_protocol::codec_errc::not_enough_input) {
      src_channel->want_recv(1);
      return state();
    }

    log_debug("decoding server greeting failed: %s",
              decode_res.error().message().c_str());

    return State::FINISH;
  }

  dyn_buf.consume(decode_res.value().first);

  client_protocol()->seq_id(decode_res->second.seq_id());

  auto client_greeting_msg = decode_res->second.payload();
  auto caps = client_greeting_msg.capabilities();

  client_protocol()->client_greeting(client_greeting_msg);

  if (dest_ssl_mode() == SslMode::kAsClient &&
      !server_protocol()->server_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    // config says: do as the client did, but the client did SSL and server
    // doesn't support it.
    std::vector<uint8_t> error_frame;
    const auto encode_res = classic_protocol::encode(
        classic_protocol::frame::Frame<
            classic_protocol::message::server::Error>(
            ++client_protocol_->seq_id(),
            {2026, "SSL connection error: SSL is required from client"}),
        {}, net::dynamic_buffer(error_frame));
    if (!encode_res) {
      return log_fatal_error_code("encoding error failed", encode_res.error());
    }

    client_channel()->write_plain(net::buffer(error_frame));
    client_channel()->flush_to_send_buf();

    return State::FINISH;
  }

  const auto append_attrs_res =
      classic_proto_decode_and_add_connection_attributes(
          client_greeting_msg, session_attributes(), client_channel()->ssl());
  if (!append_attrs_res) {
    // anything to do in case of failure?
    //
    // if the client
  }

  // client side is TLS encrypted now, check what to do on the server side.
  if (dest_ssl_mode() == SslMode::kDisabled ||
      (dest_ssl_mode() == SslMode::kPreferred &&
       !server_protocol()->server_capabilities().test(
           classic_protocol::capabilities::pos::ssl))) {
#if defined(DEBUG_STATE)
    log_debug("%d: .. %s", __LINE__, state_to_string(state()));
#endif
    // disable the SSL cap on the server side and send updated client greeting
    // to the server.
    caps.reset(classic_protocol::capabilities::pos::ssl);

    server_protocol()->client_capabilities(caps);

    // build client-greeting packet.
    client_greeting_msg.capabilities(caps);
    ++server_protocol()->seq_id();

    const auto encode_res = classic_protocol::encode(
        classic_protocol::frame::Frame<
            classic_protocol::message::client::Greeting>(
            server_protocol()->seq_id(), client_greeting_msg),

        // TODO: shared-caps?
        server_protocol()->server_capabilities(),
        net::dynamic_buffer(server_channel()->send_buffer()));
    if (!encode_res) {
      return log_fatal_error_code("encoding client-greeting failed",
                                  encode_res.error());
    }

    if (server_channel()->send_buffer().empty()) {
      // encoding succeeded, but no payload?
      log_debug("encoding succeeded, but payload is empty?");

      return State::FINISH;
    }

    // SSL isn't enabled on the server side. The real client-greeting.
    server_protocol()->client_greeting(client_greeting_msg);

#if defined(DEBUG_STATE)
    log_debug("%d: << %s", __LINE__, state_to_string(state()));
#endif
    return State::SPLICE_INIT;
  } else {
#if defined(DEBUG_STATE)
    log_debug("%d: << %s", __LINE__, state_to_string(state()));
#endif
    server_protocol()->client_greeting(client_greeting_msg);

    return State::TLS_CLIENT_GREETING_RESPONSE;
  }
}

BasicSplicer::State ClassicProtocolSplicer::tls_client_greeting_response() {
#if defined(DEBUG_STATE)
  log_debug("%d: >> %s", __LINE__, state_to_string(state()));
#endif
  server_channel()->init_ssl(server_ssl_ctx_getter_());
  return State::TLS_CONNECT;
}

stdx::expected<size_t, std::error_code>
ClassicProtocolSplicer::encode_error_packet(
    std::vector<uint8_t> &error_frame, const uint8_t seq_id,
    const classic_protocol::capabilities::value_type caps,
    const uint16_t error_code, const std::string &msg,
    const std::string &sql_state) {
  return classic_protocol::encode(
      classic_protocol::frame::Frame<classic_protocol::message::server::Error>(
          seq_id, {error_code, msg, sql_state}),
      caps, net::dynamic_buffer(error_frame));
}

stdx::expected<size_t, std::error_code>
ClassicProtocolSplicer::encode_error_packet(std::vector<uint8_t> &error_frame,
                                            const uint16_t error_code,
                                            const std::string &msg,
                                            const std::string &sql_state) {
  return encode_error_packet(error_frame, ++client_protocol()->seq_id(),
                             client_protocol()->shared_capabilities(),
                             error_code, msg, sql_state);
}

BasicSplicer::State ClassicProtocolSplicer::tls_connect() {
#if defined(DEBUG_STATE)
  log_debug("%d: >> %s", __LINE__, state_to_string(state()));
#endif
  auto *channel = server_channel_.get();

  {
    const auto flush_res = channel->flush_from_recv_buf();
    if (!flush_res) {
      return log_fatal_error_code("tls_connect::recv::flush() failed",
                                  flush_res.error());
    }
  }

  if (!channel->tls_init_is_finished()) {
#if defined(DEBUG_STATE)
    log_debug("%d: .. %s", __LINE__, state_to_string(state()));
#endif
    const auto res = channel->tls_connect();

    if (!res) {
      if (res.error() == TlsErrc::kWantRead) {
#if defined(DEBUG_STATE)
        log_debug("%d: .. %s", __LINE__, state_to_string(state()));
#endif
        {
          const auto flush_res = channel->flush_to_send_buf();
          if (!flush_res &&
              (flush_res.error() !=
               make_error_condition(std::errc::operation_would_block))) {
            return log_fatal_error_code("tls_connect::send::flush() failed",
                                        flush_res.error());
          }
        }

        // we perhaps one more byte is enough to make SSL_connect() happy?
        channel->want_recv(1);
        return state();
      } else {
#if defined(DEBUG_STATE)
        log_debug("%d: .. %s", __LINE__, state_to_string(state()));
#endif
        // connect may fail fatally if
        //
        // - cert-verification failed.
        // - no shared cipher
        std::vector<uint8_t> error_frame;

        auto encode_res = encode_error_packet(
            error_frame, 2026,
            "connecting to destination failed with TLS error: " +
                res.error().message());

        if (!encode_res) {
          return log_fatal_error_code("encoding error failed",
                                      encode_res.error());
        }

        client_channel()->write_plain(net::buffer(error_frame));
        client_channel()->flush_to_send_buf();

        return State::FINISH;
      }
    }

#if defined(DEBUG_STATE)
    log_debug("%d: .. %s", __LINE__, state_to_string(state()));
#endif
#if defined(DEBUG_STATE)
    log_debug("%d: client can do ssl: %s", __LINE__,
              client_protocol_->shared_capabilities().test(
                  classic_protocol::capabilities::pos::ssl)
                  ? "yes"
                  : "no");
    log_debug("%d: server can do ssl: %s", __LINE__,
              server_protocol_->shared_capabilities().test(
                  classic_protocol::capabilities::pos::ssl)
                  ? "yes"
                  : "no");
#endif

    // the client never sent a full greeting, but we want to upgrade the
    // server connection to TLS
    //
    // client seq-id is 1
    // server seq-id will be one ahead

    // sanity check: if we get here, the client-greeting should be set.
    if (!server_protocol()->client_greeting()) {
      log_error("%d: %s", __LINE__, "expected client-greeting to be set");

      return State::FINISH;
    }

    std::vector<uint8_t> packet;

    const auto encode_res =
        classic_protocol::encode<classic_protocol::frame::Frame<
            classic_protocol::message::client::Greeting>>(
            {++server_protocol_->seq_id(),
             server_protocol()->client_greeting().value()},
            server_protocol()->server_capabilities(),
            net::dynamic_buffer(packet));
    if (!encode_res) {
      return log_fatal_error_code("encoding client-greeting failed",
                                  encode_res.error());
    }

    const auto write_res = channel->write_plain(net::buffer(packet));
    if (!write_res) {
      return log_fatal_error_code("write() to server failed",
                                  write_res.error());
    }

    const auto flush_res = channel->flush_to_send_buf();
    if (!flush_res) {
      return log_fatal_error_code("flush() failed", flush_res.error());

      return State::FINISH;
    }
  }

#if defined(DEBUG_STATE)
  log_debug("%d: << %s", __LINE__, state_to_string(state()));
#endif
  return State::SPLICE_INIT;
}

BasicSplicer::State ClassicProtocolSplicer::splice_int(
    Channel *src_channel, ClassicProtocolState *src_protocol,
    Channel *dst_channel, ClassicProtocolState *dst_protocol) {
  auto &plain = src_channel->recv_plain_buffer();
  read_to_plain(src_channel, plain);

#if defined(DEBUG_STATE)
  bool to_server = src_channel == client_channel();

  log_debug("%d: %s", __LINE__, to_server ? "c->s" : "c<-s");
#endif

  auto plain_buf = net::dynamic_buffer(plain);

  if (source_ssl_mode() == SslMode::kPassthrough && src_channel->is_tls()) {
    // at least the TLS record header.
    const size_t tls_header_size = 5;
    while (plain_buf.size() > tls_header_size) {
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
        return state();
      }

      const auto write_res = dst_channel->write(
          plain_buf.data(0, tls_header_size + tls_payload_size));
      if (!write_res) {
        return State::FINISH;
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
  } else {
    while (plain_buf.size() != 0) {
      // decode the frame and adjust the sequence number as needed.
      const auto decode_res =
          classic_protocol::decode<classic_protocol::frame::Header>(
              plain_buf.data(0, plain_buf.size()), 0);
      if (!decode_res) {
        if (decode_res.error() ==
            classic_protocol::codec_errc::not_enough_input) {
          src_channel->want_recv(1);
          return state();
        }

        log_debug("decoding frame failed: %s",
                  decode_res.error().message().c_str());

        return State::FINISH;
      }

      // rewrite the sequence-id if needed.
      const auto frame_header_res = decode_res.value();
      const auto header_size = frame_header_res.first;
      const auto seq_id = frame_header_res.second.seq_id();
      const auto payload_size = frame_header_res.second.payload_size();

      src_protocol->seq_id(seq_id);

      // if one side starts a new command, reset the sequence-id for the other
      // side too.
      if (seq_id == 0) {
        dst_protocol->seq_id(0);
      } else {
        ++dst_protocol->seq_id();
      }

      if (src_protocol->seq_id() == dst_protocol->seq_id()) {
        // forward the frame as is.
        auto write_res =
            dst_channel->write(plain_buf.data(0, header_size + payload_size));
        if (!write_res) {
          log_debug("write to dst-channel failed: %s",
                    write_res.error().message().c_str());
          return State::FINISH;
        }

        plain_buf.consume(write_res.value());
      } else {
        // build the protocol header, add it to the output.
        std::vector<uint8_t> dest_header;
        const auto encode_res =
            classic_protocol::encode<classic_protocol::frame::Header>(
                {payload_size, dst_protocol->seq_id()}, {},
                net::dynamic_buffer(dest_header));

        if (!encode_res) {
          const auto ec = encode_res.error();
          log_debug("encoding header failed: %s (%s:%d)", ec.message().c_str(),
                    ec.category().name(), ec.value());

          return State::FINISH;
        }

        auto write_res = dst_channel->write(net::buffer(dest_header));
        if (!write_res) {
          log_debug("write to dst-channel failed: %s",
                    write_res.error().message().c_str());
          return State::FINISH;
        }

        // skip the original header, and append the payload as is.
        plain_buf.consume(4);

        write_res = dst_channel->write(plain_buf.data(0, payload_size));
        if (!write_res) {
          log_debug("write to dst-channel failed: %s",
                    write_res.error().message().c_str());
          return State::FINISH;
        }

        plain_buf.consume(write_res.value());
      }

      dst_channel->flush_to_send_buf();
    }
  }

#if defined(DEBUG_STATE)
  log_debug("%d: %s::want-read", __LINE__, to_server ? "client" : "server");
#endif
  src_channel->want_recv(1);

  return state();
}

stdx::expected<size_t, std::error_code>
ClassicProtocolSplicer::on_block_client_host(std::vector<uint8_t> &buf) {
  // the client didn't send a Greeting before closing the connection.
  //
  // Generate a Greeting to be sent to the server, to ensure the router's IP
  // isn't blocked due to the server's max_connect_errors.
  return classic_protocol::encode(
      classic_protocol::frame::Frame<
          classic_protocol::message::client::Greeting>(
          1,
          {
              {},                                            // caps
              16 * 1024 * 1024,                              // max-packet-size
              classic_protocol::collation::Latin1SwedishCi,  // collation
              "ROUTER",                                      // username
              "",                                            // auth data
              "fake_router_login",                           // schema
              "mysql_native_password",                       // auth method
              ""                                             // attributes
          }),
      client_protocol()->shared_capabilities(), net::dynamic_buffer(buf));
}
