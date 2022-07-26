/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "classic_connection.h"

#include <chrono>
#include <cinttypes>  // PRIu64
#include <iostream>
#include <random>  // uniform_int_distribution
#include <sstream>
#include <string>

#include "basic_protocol_splicer.h"
#include "channel.h"  // Channel, ClassicProtocolState
#include "errmsg.h"   // mysql-client error-codes
#include "harness_assert.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql-server error-codes
#include "mysqlrouter/classic_protocol_clone.h"
#include "mysqlrouter/classic_protocol_codec_base.h"
#include "mysqlrouter/classic_protocol_codec_clone.h"
#include "mysqlrouter/classic_protocol_codec_error.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_frame.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/connection_pool.h"
#include "mysqlrouter/connection_pool_component.h"
#include "mysqlrouter/routing_component.h"
#include "mysqlrouter/utils.h"  // to_string
#include "router/src/routing/src/basic_protocol_splicer.h"
#include "ssl_mode.h"

IMPORT_LOG_FUNCTIONS()

#undef DEBUG_IO
#undef DEBUG_SSL

// enable to add tracing for the clone-cmd-stream
#undef DEBUG_CMD_CLONE

using namespace std::string_literals;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

static constexpr const std::string_view kCachingSha2Password{
    "caching_sha2_password"sv};

static constexpr const std::string_view kMysqlNativePassword{
    "mysql_native_password"sv};

static constexpr const std::string_view kMysqlClearPassword{
    "mysql_clear_password"sv};

static constexpr const std::array supported_authentication_methods{
    kCachingSha2Password,
    kMysqlNativePassword,
    kMysqlClearPassword,
};

/**
 * hexdump into a string.
 */
template <class T>
static std::string hexify(const T &buf) {
  std::string out;
  size_t col{};

  auto *start = reinterpret_cast<const uint8_t *>(buf.data());
  const auto *end = start + buf.size();

  for (auto cur = start; cur != end; ++cur) {
    std::array<char, 3> hexchar{};
    snprintf(hexchar.data(), hexchar.size(), "%02x", *cur);

    out.append(hexchar.data());

    if (++col >= 16) {
      col = 0;
      out.append("\n");
    } else {
      out.append(" ");
    }
  }

  if (col != 0) out += "\n";

  return out;
}

template <class T>
static constexpr uint8_t cmd_byte() {
  return classic_protocol::Codec<T>::cmd_byte();
}

/*
 * discard the current message.
 *
 * @post after success, the current msg is reset.
 *
 * - succeeds if there is no current-msg
 * - succeeds if the whole message is in the receive buffer.
 * - fails with bad_message if recv-buffer isn't complete.
 * - fails with invalid_argument frame has been partially forwarded already.
 */
static stdx::expected<void, std::error_code> discard_current_msg(
    Channel *src_channel, ClassicProtocolState *src_protocol) {
  auto &opt_current_frame = src_protocol->current_frame();
  if (!opt_current_frame) return {};

  auto &current_frame = *opt_current_frame;

  auto &recv_buf = src_channel->recv_plain_buffer();

  if (recv_buf.size() < current_frame.frame_size_) {
    // received message is incomplete.
    return stdx::make_unexpected(make_error_code(std::errc::bad_message));
  }
  if (current_frame.forwarded_frame_size_ != 0) {
    // partially forwarded already.
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  net::dynamic_buffer(recv_buf).consume(current_frame.frame_size_);

  // unset current frame and also current-msg
  src_protocol->current_frame().reset();
  src_protocol->current_msg_type().reset();

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
static stdx::expected<size_t, std::error_code>
classic_proto_decode_and_add_connection_attributes(
    classic_protocol::message::client::Greeting &client_greeting_msg,
    const std::vector<std::pair<std::string, std::string>> &attributes) {
  // add attributes if they are sane.
  auto attrs = client_greeting_msg.attributes();

  const auto verify_res = classic_proto_verify_connection_attributes(attrs);
  if (!verify_res) return verify_res.get_unexpected();

  size_t bytes_appended{};
  for (const auto &attr : attributes) {
    const auto append_res =
        classic_proto_append_attribute(attrs, attr.first, attr.second);
    if (!append_res) return append_res.get_unexpected();
    bytes_appended += append_res.value();
  }

  client_greeting_msg.attributes(attrs);

  return {bytes_appended};
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

static void adjust_supported_capabilities(
    SslMode source_ssl_mode, SslMode dest_ssl_mode,
    classic_protocol::capabilities::value_type &caps) {
  // don't modify caps on passthrough.
  if (source_ssl_mode == SslMode::kPassthrough) return;

  // disable compression as we don't support it yet.
  caps.reset(classic_protocol::capabilities::pos::compress);
  caps.reset(classic_protocol::capabilities::pos::compress_zstd);

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

static bool server_ssl_mode_is_satisfied(
    SslMode server_ssl_mode,
    classic_protocol::capabilities::value_type server_capabilities) {
  if ((server_ssl_mode == SslMode::kRequired) &&
      !server_capabilities.test(classic_protocol::capabilities::pos::ssl)) {
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
  if (client_capabilities.test(classic_protocol::capabilities::pos::compress) &&
      !shared_capabilities.test(
          classic_protocol::capabilities::pos::compress)) {
    return false;
  }

  return true;
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

static stdx::expected<size_t, std::error_code> encode_error_msg(
    std::vector<uint8_t> &send_buf, uint8_t seq_id,
    const classic_protocol::message::server::Error &msg) {
  return classic_protocol::encode(
      classic_protocol::frame::Frame<classic_protocol::message::server::Error>(
          seq_id, msg),
      {}, net::dynamic_buffer(send_buf));
}

static bool has_frame_header(ClassicProtocolState *src_protocol) {
  return src_protocol->current_frame().has_value();
}

static bool has_msg_type(ClassicProtocolState *src_protocol) {
  return src_protocol->current_msg_type().has_value();
}

static stdx::expected<std::pair<size_t, ClassicProtocolState::FrameInfo>,
                      std::error_code>
decode_frame_header(const net::const_buffer &recv_buf) {
  const auto decode_res =
      classic_protocol::decode<classic_protocol::frame::Header>(
          net::buffer(recv_buf), 0);
  if (!decode_res) {
    const auto ec = decode_res.error();

    if (ec == classic_protocol::codec_errc::not_enough_input) {
      return stdx::make_unexpected(make_error_code(TlsErrc::kWantRead));
    }
    return decode_res.get_unexpected();
  }

  const auto frame_header_res = decode_res.value();
  const auto header_size = frame_header_res.first;
  const auto seq_id = frame_header_res.second.seq_id();
  const auto payload_size = frame_header_res.second.payload_size();

  const auto frame_size = header_size + payload_size;

  return {std::in_place, header_size,
          ClassicProtocolState::FrameInfo{seq_id, frame_size, 0u}};
}

/**
 * ensure current_frame() has a current frame-info.
 *
 * @post after success returned, src_protocol->current_frame() has a frame
 * decoded.
 */
static stdx::expected<void, std::error_code> ensure_frame_header(
    Channel *src_channel, ClassicProtocolState *src_protocol) {
  auto &recv_buf = src_channel->recv_plain_buffer();

  const size_t min_size{4};
  const auto cur_size = recv_buf.size();
  if (cur_size < min_size) {
    // read the rest of the header.
    auto read_res = src_channel->read_to_plain(min_size - cur_size);
    if (!read_res) return read_res.get_unexpected();

    if (recv_buf.size() < min_size) {
      return stdx::make_unexpected(make_error_code(TlsErrc::kWantRead));
    }
  }

  const auto decode_frame_res = decode_frame_header(net::buffer(recv_buf));
  if (!decode_frame_res) return decode_frame_res.get_unexpected();

  src_protocol->current_frame() = std::move(decode_frame_res.value()).second;

  return {};
}

/**
 * ensure the recv_plain_buffer() has a full frame.
 *
 * if the frame is complete, returns immediately with success.
 * otherwise tries to read the rest of the frame from the network buffers into
 * the plain-buffer.
 *
 * @pre there must be current-frame set.
 */
[[nodiscard]] static stdx::expected<void, std::error_code>
ensure_has_full_frame(Channel *src_channel,
                      ClassicProtocolState *src_protocol) {
  harness_assert(src_protocol->current_frame());

  auto &current_frame = src_protocol->current_frame().value();
  auto &recv_buf = src_channel->recv_plain_buffer();

  const auto min_size = current_frame.frame_size_;
  const auto cur_size = recv_buf.size();
  if (cur_size >= min_size) return {};

  auto read_res = src_channel->read_to_plain(min_size - cur_size);
  if (!read_res) return read_res.get_unexpected();

  return {};
}

/**
 * ensure message has a frame-header and msg-type.
 *
 * @retval true if src-protocol's recv-buffer has frame-header and msg-type.
 */
static stdx::expected<void, std::error_code> ensure_has_msg_prefix(
    Channel *src_channel, ClassicProtocolState *src_protocol) {
  if (has_frame_header(src_protocol) && has_msg_type(src_protocol)) return {};

  if (!has_frame_header(src_protocol)) {
    auto decode_frame_res = ensure_frame_header(src_channel, src_protocol);
    if (!decode_frame_res) {
      return decode_frame_res.get_unexpected();
    }
  }

  if (!has_msg_type(src_protocol)) {
    auto &current_frame = src_protocol->current_frame().value();

    if (current_frame.frame_size_ < 5) {
      // expected a frame with at least one msg-type-byte
      return stdx::make_unexpected(make_error_code(std::errc::bad_message));
    }

    if (current_frame.forwarded_frame_size_ >= 4) {
      return stdx::make_unexpected(make_error_code(std::errc::bad_message));
    }

    const size_t msg_type_pos = 4 - current_frame.forwarded_frame_size_;

    auto &recv_buf = src_channel->recv_plain_buffer();
    if (msg_type_pos >= recv_buf.size()) {
      // read some more data.
      auto read_res = src_channel->read_to_plain(1);
      if (!read_res) return read_res.get_unexpected();

      if (msg_type_pos >= recv_buf.size()) {
        return stdx::make_unexpected(make_error_code(TlsErrc::kWantRead));
      }
    }

    src_protocol->current_msg_type() = recv_buf[msg_type_pos];
  }

#if defined(DEBUG_IO)
  std::cerr << __LINE__ << ": "
            << "seq-id: " << (int)src_protocol->current_frame()->seq_id_
            << ", frame-size: "
            << (int)src_protocol->current_frame()->frame_size_
            << ", msg-type: " << (int)src_protocol->current_msg_type().value()
            << "\n";
#endif

  return {};
}

static stdx::expected<void, std::error_code> ensure_server_greeting(
    Channel *src_channel, ClassicProtocolState *src_protocol) {
  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return read_res.get_unexpected();

  // if it fails, the next function will fail with not-enough-input
  (void)ensure_has_full_frame(src_channel, src_protocol);

  auto &recv_buf = src_channel->recv_plain_buffer();
  // decode server-greeting msg from frame.
  const auto decode_res =
      classic_protocol::decode<classic_protocol::frame::Frame<
          classic_protocol::message::server::Greeting>>(net::buffer(recv_buf),
                                                        0);
  if (!decode_res) return decode_res.get_unexpected();

#if defined(DEBUG_STATE)
  log_debug("client-ssl-mode=%s, server-ssl-mode=%s",
            ssl_mode_to_string(source_ssl_mode()),
            ssl_mode_to_string(dest_ssl_mode()));
#endif

  auto server_greeting_msg = decode_res->second.payload();

  auto caps = server_greeting_msg.capabilities();

  auto seq_id = src_protocol->current_frame()->seq_id_;

  src_protocol->seq_id(seq_id);
  src_protocol->server_capabilities(caps);
  src_protocol->server_greeting(server_greeting_msg);

  return {};
}

// encode an error-msg and flush it to the send-buffers.
static stdx::expected<void, std::error_code> send_error_packet(
    Channel *dst_channel, ClassicProtocolState *dst_protocol,
    const classic_protocol::message::server::Error &msg) {
  std::vector<uint8_t> encode_buf;
  const auto encode_res =
      encode_error_msg(encode_buf, dst_protocol->seq_id(), msg);
  if (!encode_res) return encode_res.get_unexpected();

  auto write_res = dst_channel->write(net::buffer(encode_buf));
  if (!write_res) return write_res.get_unexpected();

  auto flush_res = dst_channel->flush_to_send_buf();
  if (!flush_res) return flush_res.get_unexpected();

  return {};
}

static stdx::expected<void, std::error_code> send_ssl_connection_error_msg(
    Channel *dst_channel, ClassicProtocolState *dst_protocol,
    const std::string &msg) {
  return send_error_packet(dst_channel, dst_protocol,
                           {CR_SSL_CONNECTION_ERROR, msg});
}

stdx::expected<size_t, std::error_code>
MysqlRoutingClassicConnection::encode_error_packet(
    std::vector<uint8_t> &error_frame, const uint8_t seq_id,
    const classic_protocol::capabilities::value_type caps,
    const uint16_t error_code, const std::string &msg,
    const std::string &sql_state) {
  return classic_protocol::encode(
      classic_protocol::frame::Frame<classic_protocol::message::server::Error>(
          seq_id, {error_code, msg, sql_state}),
      caps, net::dynamic_buffer(error_frame));
}

static stdx::expected<size_t, std::error_code> forward_frame_header_as_is(
    Channel *src_channel, Channel *dst_channel, size_t header_size) {
  auto &recv_buf = src_channel->recv_plain_buffer();

  return dst_channel->write(net::buffer(recv_buf, header_size));
}

static stdx::expected<size_t, std::error_code> write_frame_header(
    Channel *dst_channel, classic_protocol::frame::Header frame_header) {
  std::vector<uint8_t> dest_header;
  const auto encode_res =
      classic_protocol::encode<classic_protocol::frame::Header>(
          frame_header, {}, net::dynamic_buffer(dest_header));
  if (!encode_res) {
    return encode_res.get_unexpected();
  }

  return dst_channel->write(net::buffer(dest_header));
}

static stdx::expected<size_t, std::error_code> forward_header(
    Channel *src_channel, ClassicProtocolState *src_protocol,
    Channel *dst_channel, ClassicProtocolState *dst_protocol,
    size_t header_size, size_t payload_size) {
  if (src_protocol->seq_id() == dst_protocol->seq_id()) {
    return forward_frame_header_as_is(src_channel, dst_channel, header_size);
  } else {
    auto write_res =
        write_frame_header(dst_channel, {payload_size, dst_protocol->seq_id()});
    if (!write_res) return write_res.get_unexpected();

    // return the bytes that were skipped from the recv_buffer.
    return header_size;
  }
}

/**
 * @returns frame-is-done on success and std::error_code on error.
 */
static stdx::expected<bool, std::error_code> forward_frame_from_channel(
    Channel *src_channel, ClassicProtocolState *src_protocol,
    Channel *dst_channel, ClassicProtocolState *dst_protocol) {
  if (!has_frame_header(src_protocol)) {
    auto read_res = ensure_frame_header(src_channel, src_protocol);
    if (!read_res) return read_res.get_unexpected();
  }

  auto &current_frame = src_protocol->current_frame().value();

  auto &recv_buf = src_channel->recv_plain_buffer();
  if (current_frame.forwarded_frame_size_ == 0) {
    const size_t header_size{4};

    const uint8_t seq_id = current_frame.seq_id_;
    const size_t payload_size = current_frame.frame_size_ - header_size;

    src_protocol->seq_id(seq_id);

    // if one side starts a new command, reset the sequence-id for the other
    // side too.
    if (seq_id == 0) {
      dst_protocol->seq_id(0);
    } else {
      ++dst_protocol->seq_id();
    }

    auto forward_res = forward_header(src_channel, src_protocol, dst_channel,
                                      dst_protocol, header_size, payload_size);
    if (!forward_res) return forward_res.get_unexpected();

    const size_t transferred = forward_res.value();

    current_frame.forwarded_frame_size_ = transferred;

    // skip the original header
    net::dynamic_buffer(recv_buf).consume(transferred);
  }

#if 0
  std::cerr << __LINE__ << ": "
            << "seq-id: " << (int)current_frame.seq_id_ << ", "
            << "frame-size: " << current_frame.frame_size_ << ", "
            << "forwarded so far: " << current_frame.forwarded_frame_size_
            << "\n";
#endif

  // forward the (rest of the) payload.

  const size_t rest_of_frame_size =
      current_frame.frame_size_ - current_frame.forwarded_frame_size_;

  if (rest_of_frame_size > 0) {
    // try to fill the recv-buf up to the end of the frame
    if (rest_of_frame_size > recv_buf.size()) {
      // ... not more than 16k to avoid reading all 16M at once.
      auto read_res = src_channel->read_to_plain(
          std::min(rest_of_frame_size - recv_buf.size(), size_t{16 * 1024}));
      if (!read_res) return read_res.get_unexpected();
    }

    if (recv_buf.empty()) {
      return stdx::make_unexpected(make_error_code(TlsErrc::kWantRead));
    }

    const auto write_res =
        dst_channel->write(net::buffer(recv_buf, rest_of_frame_size));
    if (!write_res) return write_res.get_unexpected();

    size_t transferred = write_res.value();
    current_frame.forwarded_frame_size_ += transferred;

    net::dynamic_buffer(recv_buf).consume(transferred);
  }

  bool src_side_is_done{false};
  if (current_frame.forwarded_frame_size_ == current_frame.frame_size_) {
#if 0
    std::cerr << __LINE__ << ": "
              << "seq-id: " << (int)(current_frame.seq_id_) << ", done"
              << "\n";
#endif
    bool is_overlong_packet = current_frame.frame_size_ == 0xffffff;

    // frame is forwarded, reset for the next one.
    src_protocol->current_frame().reset();

    if (!is_overlong_packet) {
      src_side_is_done = true;
      src_protocol->current_msg_type().reset();
    }
  } else {
#if 0
    std::cerr << __LINE__ << ": partial-frame: "
              << "seq-id: " << (int)(current_frame.seq_id_) << ", "
              << "rest-of-frame: "
              << (current_frame.frame_size_ -
                  current_frame.forwarded_frame_size_)
              << "\n";
#endif
  }

  dst_channel->flush_to_send_buf();

  return src_side_is_done;
}

static stdx::expected<uint64_t, std::error_code> decode_column_count(
    const net::const_buffer &recv_buf) {
  const auto decode_res = classic_protocol::decode<
      classic_protocol::frame::Frame<classic_protocol::wire::VarInt>>(
      net::buffer(recv_buf), 0);
  if (!decode_res) {
    return decode_res.get_unexpected();
  }

  // the var-int's value.
  return decode_res->second.payload().value();
}

static stdx::expected<MysqlRoutingClassicConnection::ForwardResult,
                      std::error_code>
forward_frame_sequence(Channel *src_channel, ClassicProtocolState *src_protocol,
                       Channel *dst_channel,
                       ClassicProtocolState *dst_protocol) {
  const auto forward_res = forward_frame_from_channel(
      src_channel, src_protocol, dst_channel, dst_protocol);
  if (!forward_res) {
    auto ec = forward_res.error();

    if (ec == TlsErrc::kWantRead) {
      if (!dst_channel->send_buffer().empty()) {
        return MysqlRoutingClassicConnection::ForwardResult::
            kWantSendDestination;
      }

      return MysqlRoutingClassicConnection::ForwardResult::kWantRecvSource;
    }

    return forward_res.get_unexpected();
  }

  // if forward-frame succeeded, then the send-buffer should be all sent.
  if (dst_channel->send_buffer().empty()) {
    log_debug("%d: %s", __LINE__, "send-buffer is empty.");

    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  const auto src_is_done = forward_res.value();
  if (src_is_done) {
    return MysqlRoutingClassicConnection::ForwardResult::kFinished;
  } else {
    return MysqlRoutingClassicConnection::ForwardResult::kWantSendDestination;
  }
}

static stdx::expected<size_t, std::error_code>
encode_server_side_client_greeting(
    Channel::recv_buffer_type &send_buf, uint8_t seq_id,
    const classic_protocol::capabilities::value_type &shared_capabilities) {
  return classic_protocol::encode(
      classic_protocol::frame::Frame<
          classic_protocol::message::client::Greeting>(
          seq_id,
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
      shared_capabilities, net::dynamic_buffer(send_buf));
}

static stdx::expected<size_t, std::error_code> encode_server_greeting(
    Channel::recv_buffer_type &send_buf, uint8_t seq_id,
    const classic_protocol::message::server::Greeting &msg) {
  return classic_protocol::encode(
      classic_protocol::frame::Frame<
          classic_protocol::message::server::Greeting>(seq_id, msg),
      {}, net::dynamic_buffer(send_buf));
}

static void log_fatal_error_code(const char *msg, std::error_code ec) {
  log_error("%s: %s (%s:%d)", msg, ec.message().c_str(), ec.category().name(),
            ec.value());
}

static stdx::expected<void, std::error_code> handle_server_greeting_error(
    Channel *src_channel, Channel *dst_channel,
    ClassicProtocolState *dst_protocol) {
  auto &recv_buf = src_channel->recv_plain_buffer();

  // decode the server's initial error-message.
  const auto decode_res = classic_protocol::decode<
      classic_protocol::frame::Frame<classic_protocol::message::server::Error>>(
      net::buffer(recv_buf), {});
  if (!decode_res) return decode_res.get_unexpected();

  const auto error_msg = decode_res->second.payload();

  // RouterRoutingTest.RoutingTooManyServerConnections expects this
  // message.
  log_debug(
      "Error from the server while waiting for greetings message: "
      "%u, '%s'",
      error_msg.error_code(), error_msg.message().c_str());

  // we got an error from the server, let's encode it again and send it to
  // the client.
  //
  // As the client may already be in the later in the
  // handshake/encrypted/has-other-caps.

  std::vector<uint8_t> frame;
  const auto encode_res =
      encode_error_msg(frame, ++dst_protocol->seq_id(), error_msg);
  if (!encode_res) return encode_res.get_unexpected();

  dst_channel->write_plain(net::buffer(frame));
  dst_channel->flush_to_send_buf();

  return {};
}

/**
 * forward server::Greeting from the src_channel if possible.
 */
static stdx::expected<bool, std::error_code> forward_server_greeting(
    Channel *src_channel, ClassicProtocolState *src_protocol,
    Channel *dst_channel, ClassicProtocolState *dst_protocol) {
  // what to do next depends on the ssl-modes and capabilities.

  if (src_protocol->server_capabilities() !=
      dst_protocol->server_capabilities()) {
    auto &send_buf = dst_channel->send_buffer();

    // use the server's greeting as is, but use the client-side's
    // "server-capabilities".
    auto server_greeting_msg = src_protocol->server_greeting().value();

    // build a new greeting.
    server_greeting_msg.capabilities(dst_protocol->server_capabilities());

    const auto encode_res = encode_server_greeting(
        send_buf, dst_protocol->seq_id(), server_greeting_msg);
    if (!encode_res) return encode_res.get_unexpected();

    // reset the server-side recv-buffer.
    (void)discard_current_msg(src_channel, src_protocol);

    return {true};  // done, no overlong packet.
  } else {
    return forward_frame_from_channel(src_channel, src_protocol, dst_channel,
                                      dst_protocol);
  }
}

void MysqlRoutingClassicConnection::on_handshake_received() {
  auto &blocked_endpoints = this->context().blocked_endpoints();
  auto &client_conn = this->socket_splicer()->client_conn();

  const uint64_t old_value = client_conn.reset_error_count(blocked_endpoints);

  if (old_value != 0) {
    log_info("[%s] resetting error counter for %s (was %" PRIu64 ")",
             this->context().get_name().c_str(), client_conn.endpoint().c_str(),
             old_value);
  }
}

void MysqlRoutingClassicConnection::on_handshake_aborted() {
  auto &blocked_endpoints = this->context().blocked_endpoints();
  auto &client_conn = this->socket_splicer()->client_conn();
  const uint64_t new_value =
      client_conn.increment_error_count(blocked_endpoints);

  if (new_value >= blocked_endpoints.max_connect_errors()) {
    log_warning("[%s] blocking client host for %s",
                this->context().get_name().c_str(),
                client_conn.endpoint().c_str());
  } else {
    log_info("[%s] incrementing error counter for host of %s (now %" PRIu64 ")",
             this->context().get_name().c_str(), client_conn.endpoint().c_str(),
             new_value);
  }
}

void MysqlRoutingClassicConnection::async_run() {
  this->accepted();

  connector().on_connect_failure(
      [&](std::string hostname, uint16_t port, const std::error_code last_ec) {
        if (last_ec == std::error_code{}) return;  // no failure.

        log_debug("[%s] add destination '%s:%d' to quarantine",
                  context().get_name().c_str(), hostname.c_str(), port);
        context().shared_quarantine().update({hostname, port});
      });

  connector().on_is_destination_good(
      [&](const std::string &hostname, uint16_t port) {
        const auto is_quarantined =
            context().shared_quarantine().is_quarantined({hostname, port});
        if (is_quarantined) {
          log_debug("[%s] skip quarantined destination '%s:%d'",
                    context().get_name().c_str(), hostname.c_str(), port);

          return false;
        }

        return true;
      });

  // the server's greeting if:
  //
  // passthrough + as_client
  // preferred   + as_client
  greeting_from_router_ = !((source_ssl_mode() == SslMode::kPassthrough) ||
                            (source_ssl_mode() == SslMode::kPreferred &&
                             dest_ssl_mode() == SslMode::kAsClient));

  if (greeting_from_router_) {
    client_send_server_greeting_from_router();
  } else {
    server_recv_server_greeting_from_server();
  }
}

void MysqlRoutingClassicConnection::send_server_failed(std::error_code ec) {
#if defined(DEBUG_IO)
  std::cerr << __LINE__ << ": r->s: " << ec.message() << ", next: finish\n";
#endif

  server_socket_failed(ec);
}

void MysqlRoutingClassicConnection::recv_server_failed(std::error_code ec) {
#if defined(DEBUG_IO)
  std::cerr << __LINE__ << ": r<-s: " << ec.message() << ", next: finish\n";
#endif

  server_socket_failed(ec);
}

void MysqlRoutingClassicConnection::send_client_failed(std::error_code ec) {
#if defined(DEBUG_IO)
  std::cerr << __LINE__ << ": c<-r: " << ec.message() << ", next: finish\n";
#endif

  client_socket_failed(ec);
}

void MysqlRoutingClassicConnection::recv_client_failed(std::error_code ec) {
#if defined(DEBUG_IO)
  std::cerr << __LINE__ << ": c->r: " << ec.message() << ", next: finish\n";
#endif

  client_socket_failed(ec);
}

void MysqlRoutingClassicConnection::server_socket_failed(std::error_code ec) {
  auto &server_conn = this->socket_splicer()->server_conn();

  if (server_conn.is_open()) {
    auto &client_conn = this->socket_splicer()->client_conn();

    log_debug("[%s] fd=%d -- %d: connection closed (up: %zub; down: %zub)",
              this->context().get_name().c_str(), client_conn.native_handle(),
              server_conn.native_handle(), this->get_bytes_up(),
              this->get_bytes_down());

    if (ec != net::stream_errc::eof) {
      (void)server_conn.shutdown(net::socket_base::shutdown_send);
    }
    (void)server_conn.close();
  }

  finish();
}

void MysqlRoutingClassicConnection::client_socket_failed(std::error_code ec) {
  auto &client_conn = this->socket_splicer()->client_conn();

  if (client_conn.is_open()) {
    if (!client_greeting_sent_) {
      log_info("[%s] %s closed connection before finishing handshake",
               this->context().get_name().c_str(),
               client_conn.endpoint().c_str());

      on_handshake_aborted();
    }

    auto &server_conn = this->socket_splicer()->server_conn();

    if (server_conn.is_open()) {
      log_debug("[%s] fd=%d -- %d: connection closed (up: %zub; down: %zub)",
                this->context().get_name().c_str(), client_conn.native_handle(),
                server_conn.native_handle(), this->get_bytes_up(),
                this->get_bytes_down());
    } else {
      log_debug(
          "[%s] fd=%d -- (not connected): connection closed (up: %zub; down: "
          "%zub)",
          this->context().get_name().c_str(), client_conn.native_handle(),
          this->get_bytes_up(), this->get_bytes_down());
    }

    if (ec != net::stream_errc::eof) {
      // the other side hasn't closed yet, shutdown our send-side.
      (void)client_conn.shutdown(net::socket_base::shutdown_send);
    }
    (void)client_conn.close();
  }

  finish();
}

void MysqlRoutingClassicConnection::async_send_client(Function next) {
  auto socket_splicer = this->socket_splicer();
  auto dst_channel = socket_splicer->client_channel();

  ++active_work_;
  socket_splicer->async_send_client(
      [this, next, to_transfer = dst_channel->send_buffer().size()](
          std::error_code ec, size_t transferred) {
        --active_work_;
        if (ec) return send_client_failed(ec);

        this->transfered_to_client(transferred);

        if (transferred < to_transfer) {
          // send the rest
          return async_send_client(next);
        }

        return call_next_function(next);
      });
}

void MysqlRoutingClassicConnection::async_recv_client(Function next) {
  ++active_work_;
  this->socket_splicer()->async_recv_client(
      [this, next](std::error_code ec, size_t transferred) {
        (void)transferred;

        --active_work_;
        if (ec) return recv_client_failed(ec);

        return call_next_function(next);
      });
}

void MysqlRoutingClassicConnection::async_send_server(Function next) {
  auto socket_splicer = this->socket_splicer();
  auto dst_channel = socket_splicer->server_channel();

  ++active_work_;
  socket_splicer->async_send_server(
      [this, next, to_transfer = dst_channel->send_buffer().size()](
          std::error_code ec, size_t transferred) {
        --active_work_;
        if (ec) return send_server_failed(ec);

        this->transfered_to_server(transferred);

        if (transferred < to_transfer) {
          // send the rest
          return async_send_server(next);
        }

        return call_next_function(next);
      });
}

void MysqlRoutingClassicConnection::async_recv_server(Function next) {
  ++active_work_;
  this->socket_splicer()->async_recv_server(
      [this, next](std::error_code ec, size_t transferred) {
        (void)transferred;

        --active_work_;
        if (ec) return recv_server_failed(ec);

        return call_next_function(next);
      });
}

void MysqlRoutingClassicConnection::async_send_client_and_finish() {
  return async_send_client(Function::kWaitClientClosed);
}

void MysqlRoutingClassicConnection::async_wait_client_closed() {
  return async_recv_client(Function::kWaitClientClosed);
}

// the client didn't send a Greeting before closing the connection.
//
// Generate a Greeting to be sent to the server, to ensure the router's IP
// isn't blocked due to the server's max_connect_errors.
void MysqlRoutingClassicConnection::server_side_client_greeting() {
  auto encode_res = encode_server_side_client_greeting(
      this->socket_splicer()->server_channel()->send_buffer(), 1,
      client_protocol()->shared_capabilities());
  if (!encode_res) return send_server_failed(encode_res.error());

  return async_send_server(Function::kFinish);
}

// after a QUIT, we should wait until the client closed the connection.

// called when the connection should be closed.
//
// called multiple times (once per "active_work_").
void MysqlRoutingClassicConnection::finish() {
  auto &client_socket = this->socket_splicer()->client_conn();
  auto &server_socket = this->socket_splicer()->server_conn();

  if (server_socket.is_open() && !client_socket.is_open()) {
    // client side closed while server side is still open ...
    if (!client_greeting_sent_) {
      // client hasn't sent a greeting to the server. The server would track
      // this as "connection error" and block the router. Better send our own
      // client-greeting.
      client_greeting_sent_ = true;
      return server_side_client_greeting();
    } else {
      // if the server is waiting on something, as client is already gone.
      (void)server_socket.cancel();
    }
  } else if (!server_socket.is_open() && client_socket.is_open()) {
    // if the client is waiting on something, as server is already gone.
    (void)client_socket.cancel();
  }

  if (active_work_ == 0) {
    if (server_socket.is_open()) {
      (void)server_socket.shutdown(net::socket_base::shutdown_send);
      (void)server_socket.close();
    }
    if (client_socket.is_open()) {
      (void)client_socket.shutdown(net::socket_base::shutdown_send);
      (void)client_socket.close();
    }

    done();
  }
}

// final state.
//
// removes the connection from the connection-container.
void MysqlRoutingClassicConnection::done() { this->disassociate(); }

// the server::Error path of server_recv_server_greeting
void MysqlRoutingClassicConnection::server_greeting_error() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();
  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = client_protocol();

  // if it fails, the next function will fail with not-enough-input
  (void)ensure_has_full_frame(src_channel, src_protocol);

  auto handle_res =
      handle_server_greeting_error(src_channel, dst_channel, dst_protocol);
  if (!handle_res) {
    auto ec = handle_res.error();

    if (ec == classic_protocol::codec_errc::not_enough_input) {
      return async_recv_server(Function::kServerGreetingFromServer);
    }

    return recv_server_failed(ec);
  }

  // the server sent an error-packet and doesn't expect a client-greeting.
  client_greeting_sent_ = true;

  // try to close the server side socket before the server does.
  //
  // whoever closes first, will enter TIME_WAIT.
  (void)socket_splicer->server_conn().close();

  return async_send_client(Function::kWaitClientClosed);
}

// the server::Greeting path of server_recv_server_greeting
void MysqlRoutingClassicConnection::server_recv_server_greeting_greeting() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();
  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = client_protocol();

  auto ensure_res = ensure_server_greeting(src_channel, src_protocol);
  if (!ensure_res) {
    const auto ec = ensure_res.error();
    if (ec == classic_protocol::codec_errc::not_enough_input ||
        ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerGreetingFromServer);
    }

    log_fatal_error_code("decoding server greeting failed", ec);

    return recv_server_failed(ec);
  }

  auto msg = src_protocol->server_greeting().value();

#if 0
  std::cerr << __LINE__ << ": proto-version: " << (int)msg.protocol_version()
            << "\n";
  std::cerr << __LINE__ << ": caps: " << msg.capabilities() << "\n";
  std::cerr << __LINE__ << ": auth-method-name: " << msg.auth_method_name()
            << "\n";
  std::cerr << __LINE__
            << ": auth-method-data: " << hexify(msg.auth_method_data()) << "\n";
  std::cerr << __LINE__ << ": status-flags: " << msg.status_flags() << "\n";
#endif

  if (!server_ssl_mode_is_satisfied(dest_ssl_mode(),
                                    src_protocol->server_capabilities())) {
    discard_current_msg(src_channel, src_protocol);

    // destination does not support TLS, but config requires encryption.
    log_debug(
        "server_ssl_mode=REQUIRED, but destination doesn't support "
        "encryption.");

    ++dst_protocol->seq_id();

    auto send_res = send_ssl_connection_error_msg(
        dst_channel, dst_protocol,
        "SSL connection error: SSL is required by router, but the "
        "server doesn't support it");
    if (!send_res) {
      auto ec = send_res.error();
      log_fatal_error_code("sending error-msg failed", ec);

      return recv_server_failed(ec);
    }

    return async_send_client(Function::kWaitClientClosed);
  }

  if (!dst_protocol->server_greeting()) {
    // client doesn't have server greeting yet, send it the server's.

    auto caps = src_protocol->server_capabilities();

    adjust_supported_capabilities(source_ssl_mode(), dest_ssl_mode(), caps);

    dst_protocol->server_capabilities(caps);
    dst_protocol->seq_id(0);

    // keep the server-greeting in the recv-buffer, it will be used for
    // forwarding.
    return client_send_server_greeting_from_server();
  } else {
    discard_current_msg(src_channel, src_protocol);

    return server_send_first_client_greeting();
  }
}

void MysqlRoutingClassicConnection::client_send_server_greeting_from_server() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();
  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = client_protocol();

  auto forward_res = forward_server_greeting(src_channel, src_protocol,
                                             dst_channel, dst_protocol);
  if (!forward_res) {
    auto ec = forward_res.error();

    return recv_server_failed(ec);
  }

  const auto src_is_done = forward_res.value();
  if (src_is_done) {
    return async_send_client(Function::kClientRecvClientGreeting);
  } else {
    return async_send_client(Function::kClientSendServerGreetingFromServer);
  }
}

/**
 * receive the server's response to change_user_for_resue().
 *
 * expects response:
 * - AuthMethodSwitch
 * - Ok
 * - Error
 *
 * similar to ::auth_response()
 */
void MysqlRoutingClassicConnection::server_recv_change_user_response() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvChangeUserResponse);
    }

    return recv_server_failed(ec);
  }

  const uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    AuthMethodSwitch =
        cmd_byte<classic_protocol::message::server::AuthMethodSwitch>(),
    Error = cmd_byte<classic_protocol::message::server::Error>(),
    Ok = cmd_byte<classic_protocol::message::server::Ok>(),
  };

  switch (Msg{msg_type}) {
    case Msg::AuthMethodSwitch:
      return server_recv_change_user_response_auth_method_switch();
    case Msg::Error:
      return server_recv_change_user_response_error();
    case Msg::Ok:
      return server_recv_change_user_response_ok();
  }

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ensure_has_full_frame(src_channel, src_protocol);

  // if there is another packet, dump its payload for now.
  auto &recv_buf = src_channel->recv_plain_buffer();

  log_debug(
      "received unexpected message from server after a client::ChangeUser: "
      "%s",
      hexify(recv_buf).c_str());

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

void MysqlRoutingClassicConnection::
    server_recv_change_user_response_auth_method_switch() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  // if it fails, the next function will fail with not-enough-input
  (void)ensure_has_full_frame(src_channel, src_protocol);

  auto &recv_buf = src_channel->recv_plain_buffer();

  auto decode_res = classic_protocol::decode<classic_protocol::frame::Frame<
      classic_protocol::message::server::AuthMethodSwitch>>(
      net::buffer(recv_buf), src_protocol->server_capabilities());
  if (!decode_res) {
    log_debug(
        "decoding message from server failed after a client::ChangeUser: "
        "%s\n%s",
        hexify(recv_buf).c_str(), decode_res.error().message().c_str());

    return recv_server_failed(make_error_code(std::errc::bad_message));
  }

  auto msg = decode_res.value().second.payload();

  src_protocol->auth_method_name(msg.auth_method());

  return forward_server_to_client(
      Function::kServerRecvChangeUserResponseAuthMethodSwitch,
      Function::kAuthClientContinue);
}

void MysqlRoutingClassicConnection::server_recv_change_user_response_ok() {
  // change user succeeded right away.
  return forward_server_to_client(Function::kServerRecvChangeUserResponseOk,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::server_recv_change_user_response_error() {
  // change user fail.
  return forward_server_to_client(Function::kServerRecvChangeUserResponseError,
                                  Function::kClientRecvCmd);
}

static classic_protocol::message::client::ChangeUser change_user_for_reuse(
    Channel *src_channel, ClassicProtocolState *src_protocol,
    [[maybe_unused]] ClassicProtocolState *dst_protocol,
    std::vector<std::pair<std::string, std::string>>
        initial_connection_attributes) {
  harness_assert(src_protocol->client_greeting().has_value());

  auto client_greeting = src_protocol->client_greeting().value();

  const auto append_attrs_res =
      classic_proto_decode_and_add_connection_attributes(
          client_greeting,
          vector_splice(initial_connection_attributes,
                        client_ssl_connection_attributes(src_channel->ssl())));
  if (!append_attrs_res) {
    auto ec = append_attrs_res.error();
    // if decode/append fails forward the attributes as is. The server should
    // fail too.
    //
    log_warning("%d: decoding connection attributes failed [ignored]: (%s) ",
                __LINE__, ec.message().c_str());
  }

#if 0
  std::cerr << __LINE__ << ": "
            << src_protocol->server_greeting().value().auth_method_name()
            << "\n";
  std::cerr << __LINE__ << ": "
            << dst_protocol->server_greeting().value().auth_method_name()
            << "\n";
  std::cerr << __LINE__ << ": " << client_greeting.auth_method_name() << "\n";
  std::cerr << __LINE__ << ": " << hexify(client_greeting.auth_method_data())
            << "\n";
#endif

  if (src_protocol->server_greeting().has_value()) {
    auto server_greeting = src_protocol->server_greeting().value();

    // the client sent an empty password. Can be reused.
    if (server_greeting.auth_method_name() == kCachingSha2Password &&
        client_greeting.auth_method_name() == kCachingSha2Password &&
        client_greeting.auth_method_data() == "\x00"s) {
      return {
          client_greeting.username(),          // username
          client_greeting.auth_method_data(),  // auth_method_data
          client_greeting.schema(),            // schema
          client_greeting.collation(),         // collation
          client_greeting.auth_method_name(),  // auth_method_name
          client_greeting.attributes(),        // attributes
      };
    }
  }

  return {
      client_greeting.username(),    // username
      "",                            // auth_method_data
      client_greeting.schema(),      // schema
      client_greeting.collation(),   // collation
      "switch_me_if_you_can",        // auth_method_name
      client_greeting.attributes(),  // attributes
  };
}

void MysqlRoutingClassicConnection::server_send_change_user() {
  auto *socket_splicer = this->socket_splicer();
  auto &src_conn = socket_splicer->client_conn();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = client_protocol();

  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = server_protocol();

  auto change_user_msg =
      change_user_for_reuse(src_channel, src_protocol, dst_protocol,
                            src_conn.initial_connection_attributes());
#if 0
  std::cerr << __LINE__ << ": username: " << change_user_msg.username() << "\n"
            << ".. schema: " << change_user_msg.schema() << "\n"
            << ".. auth-method-name: " << change_user_msg.auth_method_name()
            << "\n"
            << ".. auth-method-data: "
            << hexify(change_user_msg.auth_method_data()) << "\n"
      //
      ;
#endif

  std::vector<uint8_t> frame;
  auto encode_res = classic_protocol::encode<classic_protocol::frame::Frame<
      classic_protocol::message::client::ChangeUser>>(
      {0, change_user_msg},                 // frame
      dst_protocol->shared_capabilities(),  // caps
      net::dynamic_buffer(frame));

  if (!encode_res) {
    const auto ec = encode_res.error();

    log_debug("encoding client::ChangeUser failed: %s", ec.message().c_str());

    return recv_client_failed(ec);
  }

  dst_channel->write_plain(net::buffer(frame));
  dst_channel->flush_to_send_buf();

  return async_send_server(Function::kServerRecvChangeUserResponse);
}

static TlsSwitchableConnection make_connection_from_pooled(
    PooledClassicConnection &&other) {
  return {std::move(other.connection()),
          nullptr,  // routing_conn
          {SslMode::kPreferred, {}},
          std::make_unique<Channel>(std::move(other.ssl())),
          std::make_unique<ClassicProtocolState>(other.server_capabilities(),
                                                 other.client_capabilities())};
}

std::optional<PooledClassicConnection>
MysqlRoutingClassicConnection::try_pop_pooled_connection(
    const net::ip::tcp::endpoint &ep) {
  if (!greeting_from_router_) return {};

  auto &pools = ConnectionPoolComponent::get_instance();

  if (auto pool = pools.get(ConnectionPoolComponent::default_pool_name())) {
    // pop the first connection from the pool that matches our requirements
    //
    // - endpoint
    // - capabilities

    auto client_caps = client_protocol()->shared_capabilities();

    client_caps.reset(classic_protocol::capabilities::pos::ssl)
        .reset(classic_protocol::capabilities::pos::compress)
        .reset(classic_protocol::capabilities::pos::compress_zstd);

    return pool->pop_if([client_caps, ep = mysqlrouter::to_string(ep),
                         my_executor = connector().socket().get_executor()](
                            const auto &pooled_conn) {
      auto pooled_caps = pooled_conn.shared_capabilities();

      pooled_caps.reset(classic_protocol::capabilities::pos::ssl)
          .reset(classic_protocol::capabilities::pos::compress)
          .reset(classic_protocol::capabilities::pos::compress_zstd);

      return (pooled_conn.endpoint() == ep && client_caps == pooled_caps &&
              pooled_conn.connection()->io_ctx().get_executor() == my_executor);
    });
  }

  return {};
}

void MysqlRoutingClassicConnection::connect() {
  auto &connector = this->connector();

  auto connect_res = connector.connect();
  if (!connect_res) {
    const auto ec = connect_res.error();

    // We need to keep the disconnect_mtx_ while the async handlers are being
    // set up in order not to miss the disconnect request. Otherwise we could
    // end up blocking for the whole 'destination_connect_timeout' duration
    // before giving up the connection.
    std::lock_guard<std::mutex> lk(disconnect_mtx_);
    if ((!disconnect_) &&
        (ec == make_error_condition(std::errc::operation_in_progress) ||
         ec == make_error_condition(std::errc::operation_would_block))) {
      auto &t = connector.timer();

      t.expires_after(context().get_destination_connect_timeout());

      t.async_wait([this](std::error_code ec) {
        if (ec) {
          return;
        }

        this->connector().connect_timed_out(true);
        this->connector().socket().cancel();
      });

      connector.socket().async_wait(
          net::socket_base::wait_write, [this](std::error_code ec) {
            if (ec) {
              if (this->connector().connect_timed_out()) {
                // the connector will handle this.
                return call_next_function(Function::kConnect);
              } else {
                return call_next_function(Function::kFinish);
              }
            }
            this->connector().timer().cancel();

            return call_next_function(Function::kConnect);
          });

      return;
    }

    // close the server side.
    this->connector().socket().close();

    if (ec == std::errc::no_such_file_or_directory) {
      MySQLRoutingComponent::get_instance()
          .api(context().get_id())
          .stop_socket_acceptors();
    } else if (ec == make_error_condition(std::errc::too_many_files_open) ||
               ec == make_error_condition(
                         std::errc::too_many_files_open_in_system)) {
      // release file-descriptors on the connection pool when out-of-fds is
      // noticed.
      //
      // don't retry as router may run into an infinite loop.
      ConnectionPoolComponent::get_instance().clear();
    }

    log_fatal_error_code("connecting to backend failed", ec);

    // don't increment the max-connect-error
    // counter is it is the server that failed to connect().
    client_greeting_sent_ = true;

    auto *socket_splicer = this->socket_splicer();
    auto dst_channel = socket_splicer->client_channel();
    auto dst_protocol = client_protocol();

    std::vector<uint8_t> error_frame;

    const auto encode_res = encode_error_msg(
        error_frame, ++dst_protocol->seq_id(),  // 0 or 2/3
        {2003, "Can't connect to remote MySQL server for client connected to " +
                   get_client_address()});
    if (!encode_res) {
      auto ec = encode_res.error();
      log_fatal_error_code("encoding error failed", ec);

      return send_client_failed(ec);
    }

    // send back to the client
    dst_channel->write_plain(net::buffer(error_frame));
    dst_channel->flush_to_send_buf();

    return async_send_client_and_finish();
  }

  auto server_connection = std::move(connect_res.value());

  if (server_connection.is_authenticated()) {
    // connection is from pool.
    this->socket_splicer()->server_conn() =
        make_connection_from_pooled(std::move(server_connection));

    this->connected();

    this->socket_splicer()->server_channel()->recv_buffer().reserve(
        context().get_net_buffer_length());

    // the server side is already authenticated. Avoid sending the fake
    // handshake.
    client_greeting_sent_ = true;

    return server_send_change_user();
  } else {
    // connection is fresh.
    this->socket_splicer()->server_conn().assign_connection(
        std::move(server_connection.connection()));

    this->connected();

    this->socket_splicer()->server_channel()->recv_buffer().reserve(
        context().get_net_buffer_length());

    return server_recv_server_greeting_from_server();
  }
}

/**
 * server-greeting.
 *
 * expects
 *
 * - error-message
 * - server-greeting
 *
 * when a server-greeting is received:
 *
 * - waits for the server greeting to be complete
 * - parses server-greeting message
 * - unsets compress capabilities
 * - tracks capabilities.
 */
void MysqlRoutingClassicConnection::server_recv_server_greeting_from_server() {
  auto *socket_splicer = this->socket_splicer();
  auto &src_conn = socket_splicer->server_conn();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  if (!src_conn.is_open()) {
    return connect();
  }

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerGreetingFromServer);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol->current_msg_type().value();

  if (msg_type == cmd_byte<classic_protocol::message::server::Error>()) {
    return server_greeting_error();
  } else {
    return server_recv_server_greeting_greeting();
  }
}

void MysqlRoutingClassicConnection::client_send_server_greeting_from_router() {
  auto *socket_splicer = this->socket_splicer();
  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = client_protocol();

  auto &send_buf = dst_channel->send_buffer();

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
      // ps_multi_results (to-be-done)
      classic_protocol::capabilities::plugin_auth |
      classic_protocol::capabilities::connect_attributes |
      classic_protocol::capabilities::client_auth_method_data_varint |
      classic_protocol::capabilities::expired_passwords |
      classic_protocol::capabilities::session_track |
      classic_protocol::capabilities::text_result_with_session_tracking |
      classic_protocol::capabilities::optional_resultset_metadata
      // compress_zstd (not yet)
  );

  if (source_ssl_mode() != SslMode::kDisabled) {
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
    return MYSQL_ROUTER_VERSION "-router"s;
  };

  // wanna sync the auto-commit flag?
  classic_protocol::message::server::Greeting server_greeting_msg{
      10,                                    // protocol
      server_greeting_version(),             // version
      0,                                     // connection-id
      random_auth_method_data(),             // auth-method-data
      dst_protocol->server_capabilities(),   // server-caps
      255,                                   // 8.0.20 sends 0xff here
      classic_protocol::status::autocommit,  // status-flags
      std::string(kCachingSha2Password),     // auth-method-name
  };

  const auto encode_res = encode_server_greeting(
      send_buf, ++dst_protocol->seq_id(), server_greeting_msg);
  if (!encode_res) {
    auto ec = encode_res.error();

    return send_client_failed(ec);
  }

  dst_protocol->server_greeting(server_greeting_msg);

  return async_send_client(Function::kClientRecvClientGreeting);
}

/**
 * process the Client Greeting packet from the client.
 *
 * - wait for for a full protocol frame
 * - decode client-greeting packet and decide how to proceed based on
 * capabilities and configuration
 *
 * ## client-side connection state
 *
 * ssl-cap::client
 * :  SSL capability the client sends to router
 *
 * ssl-cap::server
 * :  SSL capability the server sends to router
 *
 * ssl-mode::client
 * :  client_ssl_mode used by router
 *
 * ssl-mode::server
 * :  server_ssl_mode used by router
 *
 * | ssl-mode    | ssl-mode | ssl-cap | ssl-cap  | ssl    |
 * | client      | server   | client  | server   | client |
 * | ----------- | -------- | ------- | -------- | ------ |
 * | DISABLED    | any      | any     | any      | PLAIN  |
 * | PREFERRED   | any      | [ ]     | any      | PLAIN  |
 * | PREFERRED   | any      | [x]     | any      | SSL    |
 * | REQUIRED    | any      | [ ]     | any      | FAIL   |
 * | REQUIRED    | any      | [x]     | any      | SSL    |
 * | PASSTHROUGH | any      | [ ]     | any      | PLAIN  |
 * | PASSTHROUGH | any      | [x]     | [x]      | (SSL)  |
 *
 * PLAIN
 * :  client-side connection is plaintext
 *
 * FAIL
 * :  router fails connection with client
 *
 * SSL
 * :  encrypted, client-side TLS endpoint
 *
 * (SSL)
 * :  encrypted, no TLS endpoint
 *
 * ## server-side connection state
 *
 * | ssl-mode    | ssl-mode  | ssl-cap | ssl-cap | ssl    |
 * | client      | server    | client  | server  | server |
 * | ----------- | --------- | ------- | ------- | ------ |
 * | any         | DISABLED  | any     | any     | PLAIN  |
 * | any         | PREFERRED | any     | [ ]     | PLAIN  |
 * | any         | PREFERRED | any     | [x]     | SSL    |
 * | any         | REQUIRED  | any     | [ ]     | FAIL   |
 * | any         | REQUIRED  | any     | [x]     | SSL    |
 * | PASSTHROUGH | AS_CLIENT | [ ]     | any     | PLAIN  |
 * | PASSTHROUGH | AS_CLIENT | [x]     | [x]     | (SSL)  |
 * | other       | AS_CLIENT | [ ]     | any     | PLAIN  |
 * | other       | AS_CLIENT | [x]     | [ ]     | FAIL   |
 * | other       | AS_CLIENT | [x]     | [x]     | SSL    |
 *
 * PLAIN
 * :  client-side connection is plaintext
 *
 * FAIL
 * :  router fails connection with client
 *
 * SSL
 * :  encrypted, client-side TLS endpoint
 *
 * (SSL)
 * :  encrypted, no TLS endpoint
 *
 */

stdx::expected<classic_protocol::message::client::Greeting, std::error_code>
MysqlRoutingClassicConnection::decode_client_greeting(
    Channel *src_channel, ClassicProtocolState *src_protocol) {
  auto &recv_buf = src_channel->recv_plain_buffer();

  auto payload_decode_res =
      classic_protocol::decode<classic_protocol::frame::Frame<
          classic_protocol::message::client::Greeting>>(
          net::buffer(recv_buf), src_protocol->server_capabilities());
  if (!payload_decode_res) return payload_decode_res.get_unexpected();

  return payload_decode_res->second.payload();
}

// called after server connection is established.
void MysqlRoutingClassicConnection::client_greeting_server_adjust_caps(
    ClassicProtocolState *src_protocol, ClassicProtocolState *dst_protocol) {
  auto client_caps = src_protocol->client_capabilities();

  if (!src_protocol->shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    auto client_greeting_msg = src_protocol->client_greeting().value();

    classic_proto_decode_and_add_connection_attributes(
        client_greeting_msg,
        this->socket_splicer()->client_conn().initial_connection_attributes());

    // client hasn't set the SSL cap, this is the real client greeting
    dst_protocol->client_greeting(client_greeting_msg);
  }

  switch (dest_ssl_mode()) {
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

stdx::expected<size_t, std::error_code>
MysqlRoutingClassicConnection::encode_client_greeting(
    const classic_protocol::message::client::Greeting &msg,
    ClassicProtocolState *dst_protocol, std::vector<uint8_t> &send_buf) {
  return classic_protocol::encode(
      classic_protocol::frame::Frame<
          classic_protocol::message::client::Greeting>(++dst_protocol->seq_id(),
                                                       msg),
      dst_protocol->server_capabilities(), net::dynamic_buffer(send_buf));
}

void MysqlRoutingClassicConnection::server_send_client_greeting_start_tls() {
  auto *socket_splicer = this->socket_splicer();
  auto *src_protocol = client_protocol();
  auto *dst_protocol = server_protocol();
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
  std::vector<uint8_t> frame_buf;
  auto encode_res = encode_client_greeting(
      {
          client_caps, initial_client_greeting_msg.max_packet_size(),
          initial_client_greeting_msg.collation(),
          "",  // username
          "",  // auth_method_data
          "",  // schema
          "",  // auth_method_name
          ""   // attributes
      },
      dst_protocol, frame_buf);
  if (!encode_res) {
    auto ec = encode_res.error();
    return send_server_failed(ec);
  }

  dst_channel->write_plain(net::buffer(frame_buf));
  dst_channel->flush_to_send_buf();

  if (source_ssl_mode() == SslMode::kPassthrough) {
    // the client's start-tls is forwarded. The client will send a
    // TlsHandshake next.
    //
    return async_send_server(Function::kForwardTlsInit);
  } else {
    return async_send_server(Function::kTlsConnectInit);
  }
}

/**
 * c<-r: err
 * or
 * r->s: client::greeting
 * or
 * r->s: client::greeting_ssl
 */
void MysqlRoutingClassicConnection::server_send_first_client_greeting() {
  auto *socket_splicer = this->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = client_protocol();
  auto *dst_protocol = server_protocol();

  bool server_supports_tls = dst_protocol->server_capabilities().test(
      classic_protocol::capabilities::pos::ssl);
  bool client_uses_tls = src_protocol->shared_capabilities().test(
      classic_protocol::capabilities::pos::ssl);

  if (dest_ssl_mode() == SslMode::kAsClient && client_uses_tls &&
      !server_supports_tls) {
    // config says: do as the client did, and the client did SSL and server
    // doesn't support it -> error

    ++src_protocol->seq_id();

    // send back to the client
    const auto send_res = send_ssl_connection_error_msg(
        src_channel, src_protocol,
        "SSL connection error: Requirements can not be satisfied");
    if (!send_res) {
      auto ec = send_res.error();

      log_fatal_error_code("encoding error failed", ec);

      return send_client_failed(ec);
    }

    return async_send_client_and_finish();
  }

  client_greeting_server_adjust_caps(src_protocol, dst_protocol);

  // use the client-side's capabilities to make sure the server encodes
  // the packets according to the client.
  //
  // src_protocol->shared_caps must be used here as the ->client_caps may
  // contain more than what the router advertised.
  auto client_caps = src_protocol->shared_capabilities();

  switch (dest_ssl_mode()) {
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

  dst_protocol->client_capabilities(client_caps);
  dst_protocol->auth_method_name(src_protocol->auth_method_name());

  // the client greeting was received and will be forwarded to the server
  // soon.
  client_greeting_sent_ = true;
  on_handshake_received();

  if (dst_protocol->shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl)) {
    return server_send_client_greeting_start_tls();
  } else {
    return server_send_client_greeting_full();
  }
}

void MysqlRoutingClassicConnection::server_send_client_greeting_full() {
  auto *socket_splicer = this->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = client_protocol();
  auto *dst_channel = socket_splicer->server_channel();
  auto *dst_protocol = server_protocol();

  auto client_greeting_msg = src_protocol->client_greeting().value();

  const auto append_attrs_res =
      classic_proto_decode_and_add_connection_attributes(
          client_greeting_msg,
          vector_splice(this->socket_splicer()
                            ->client_conn()
                            .initial_connection_attributes(),
                        client_ssl_connection_attributes(src_channel->ssl())));
  if (!append_attrs_res) {
    auto ec = append_attrs_res.error();
    // if decode/append fails forward the attributes as is. The server should
    // fail too.
    //
    log_warning("%d: decoding connection attributes failed [ignored]: (%s) ",
                __LINE__, ec.message().c_str());
  }

  client_greeting_msg.capabilities(dst_protocol->client_capabilities());

  std::vector<uint8_t> frame_buf;
  auto encode_res =
      encode_client_greeting(client_greeting_msg, dst_protocol, frame_buf);
  if (!encode_res) {
    auto ec = encode_res.error();
    return send_server_failed(ec);
  }

  const auto write_res = dst_channel->write_plain(net::buffer(frame_buf));
  if (!write_res) {
    auto ec = write_res.error();
    log_fatal_error_code("server::write() failed", ec);

    return send_server_failed(ec);
  }

  const auto flush_res = dst_channel->flush_to_send_buf();
  if (!flush_res) {
    auto ec = flush_res.error();

    log_fatal_error_code("server::flush() failed", ec);

    return send_server_failed(ec);
  }

  return async_send_server(Function::kAuthResponse);
}

// receive the first client greeting.
void MysqlRoutingClassicConnection::client_recv_client_greeting() {
  auto *src_channel = this->socket_splicer()->client_channel();
  auto *src_protocol = client_protocol();
  auto *dst_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_client(Function::kClientRecvClientGreeting);
    }

    log_fatal_error_code("decoding client greeting failed", ec);
    return recv_client_failed(ec);
  }

  auto &current_frame = src_protocol->current_frame().value();

  if (current_frame.seq_id_ != 1) {
    // client-greeting has seq-id 1
    return recv_client_failed(make_error_code(std::errc::bad_message));
  }

#if defined(DEBUG_IO)
  std::ostringstream oss;
  oss << __LINE__ << ": c->r: " << hexify(src_channel->recv_plain_buffer())
      << "\n";
  std::cerr << oss.str();
#endif

  // if it fails, the next function will fail with not-enough-input
  (void)ensure_has_full_frame(src_channel, src_protocol);

  auto client_greeting_res = decode_client_greeting(src_channel, src_protocol);
  if (!client_greeting_res) {
    auto ec = client_greeting_res.error();

    if (ec == classic_protocol::codec_errc::not_enough_input) {
      return async_recv_client(Function::kClientRecvClientGreeting);
    }

    log_fatal_error_code("decoding client greeting failed", ec);
    return recv_client_failed(ec);
  }

  auto client_greeting_msg = client_greeting_res.value();
  auto caps = client_greeting_msg.capabilities();

  src_protocol->client_capabilities(caps);
  src_protocol->seq_id(1);
  src_protocol->client_greeting(client_greeting_msg);
  src_protocol->auth_method_name(client_greeting_msg.auth_method_name());

  if (!client_ssl_mode_is_satisfied(source_ssl_mode(),
                                    src_protocol->shared_capabilities())) {
    // config says: client->router MUST be encrypted, but client didn't set
    // the SSL cap.
    //
    ++src_protocol->seq_id();

    const auto send_res = send_ssl_connection_error_msg(
        src_channel, src_protocol,
        "SSL connection error: SSL is required from client");
    if (!send_res) {
      auto ec = send_res.error();

      log_fatal_error_code("sending error failed", ec);

      return send_client_failed(ec);
    }

    return async_send_client_and_finish();
  }

  // remove the frame and message from the recv-buffer
  discard_current_msg(src_channel, src_protocol);

  // client wants to switch to tls
  if (src_protocol->shared_capabilities().test(
          classic_protocol::capabilities::pos::ssl) &&
      source_ssl_mode() != SslMode::kPassthrough) {
    return tls_accept_init();
  }

  if (dst_protocol->server_greeting().has_value()) {
    // server-greeting is already present, continue with the client
    // greeting.
    return server_send_first_client_greeting();
  } else {
    return server_recv_server_greeting_from_server();
  }
}

void MysqlRoutingClassicConnection::tls_accept_init() {
  auto *socket_splicer = this->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();

  src_channel->is_tls(true);

  auto *ssl_ctx = socket_splicer->client_conn().get_ssl_ctx();
  // tls <-> (any)
  if (ssl_ctx == nullptr) {
    // shouldn't happen. But if it does, close the connection.
    log_warning("failed to create SSL_CTX");
    return recv_client_failed(make_error_code(std::errc::invalid_argument));
  }
  src_channel->init_ssl(ssl_ctx);

  return tls_accept();
}

/**
 * accept a TLS handshake.
 */
void MysqlRoutingClassicConnection::tls_accept() {
  auto *socket_splicer = this->socket_splicer();
  auto *client_channel = socket_splicer->client_channel();

  if (!client_channel->tls_init_is_finished()) {
    auto res = socket_splicer->tls_accept();
    if (!res) {
      const auto ec = res.error();

      // if there is something in the send_buffer, send it.
      if (!client_channel->send_buffer().empty()) {
        return async_send_client(Function::kTlsAccept);
      }

      if (ec == TlsErrc::kWantRead) {
        return async_recv_client(Function::kTlsAccept);
      }

      log_fatal_error_code("tls-accept failed", ec);

      return recv_client_failed(ec);
    }
  }

  // after tls_accept() there may still be data in the send-buffer that must
  // be sent.
  if (!client_channel->send_buffer().empty()) {
    return async_send_client(Function::kClientRecvSecondClientGreeting);
  }
  // TLS is accepted, more client greeting should follow.

  return client_recv_second_client_greeting();
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

/**
 * after tls-accept expect the full client-greeting.
 */
void MysqlRoutingClassicConnection::client_recv_second_client_greeting() {
  auto *socket_splicer = this->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = client_protocol();
  auto *dst_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_client(Function::kClientRecvSecondClientGreeting);
    }

    log_fatal_error_code("decoding client greeting failed", ec);
    return recv_client_failed(ec);
  }

  // if it fails, the next function will fail with not-enough-input
  (void)ensure_has_full_frame(src_channel, src_protocol);

  auto &recv_buf = src_channel->recv_plain_buffer();

  auto decode_res = classic_protocol::decode<classic_protocol::frame::Frame<
      classic_protocol::message::client::Greeting>>(
      net::buffer(recv_buf), src_protocol->server_capabilities());
  if (!decode_res) {
    if (decode_res.error() == classic_protocol::codec_errc::not_enough_input) {
      return async_recv_client(Function::kClientRecvSecondClientGreeting);
    }

    log_fatal_error_code("decoding client greeting failed", decode_res.error());

    return recv_client_failed(decode_res.error());
  }

  discard_current_msg(src_channel, src_protocol);

  auto client_greeting_msg = decode_res->second.payload();

  src_protocol->seq_id(decode_res->second.seq_id());
  src_protocol->client_greeting(client_greeting_msg);
  src_protocol->auth_method_name(client_greeting_msg.auth_method_name());

  if (!authentication_method_is_supported(
          client_greeting_msg.auth_method_name())) {
    discard_current_msg(src_channel, src_protocol);

    ++src_protocol->seq_id();

    const auto send_res = send_error_packet(
        src_channel, src_protocol,
        {CR_AUTH_PLUGIN_CANNOT_LOAD,
         "Authentication method " + client_greeting_msg.auth_method_name() +
             " is not supported",
         "HY000"});

    if (!send_res) {
      auto ec = send_res.error();

      log_fatal_error_code("sending error failed", ec);

      return send_client_failed(ec);
    }

    return async_send_client(Function::kFinish);
  }

  // fail connection from buggy clients that set the compress-cap without
  // checking if the server's capabilities.
  if (!client_compress_is_satisfied(src_protocol->client_capabilities(),
                                    src_protocol->shared_capabilities())) {
    discard_current_msg(src_channel, src_protocol);

    ++src_protocol->seq_id();

    const auto send_res =
        send_error_packet(src_channel, src_protocol,
                          {ER_WRONG_COMPRESSION_ALGORITHM_CLIENT,
                           "Compression not supported by router."});
    if (!send_res) {
      auto ec = send_res.error();

      log_fatal_error_code("sending error failed", ec);

      return send_client_failed(ec);
    }

    return async_send_client_and_finish();
  }

  if (dst_protocol->server_greeting().has_value()) {
    // server-greeting is already present, continue with the client
    // greeting.
    return server_send_first_client_greeting();
  } else {
    return server_recv_server_greeting_from_server();
  }
}

void MysqlRoutingClassicConnection::tls_connect_init() {
  auto *socket_splicer = this->socket_splicer();
  auto *dst_channel = socket_splicer->server_channel();

  auto *ssl_ctx = socket_splicer->server_conn().get_ssl_ctx();
  if (ssl_ctx == nullptr) {
    // shouldn't happen. But if it does, close the connection.
    log_warning("failed to create SSL_CTX");

    return send_server_failed(make_error_code(std::errc::invalid_argument));
  }
  dst_channel->init_ssl(ssl_ctx);

  return tls_connect();
}

/**
 * connect server_channel to a TLS server.
 */
void MysqlRoutingClassicConnection::tls_connect() {
  auto *socket_splicer = this->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = client_protocol();
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
          return async_send_server(Function::kTlsConnect);
        }
        return async_recv_server(Function::kTlsConnect);
      } else {
        // connect may fail fatally if
        //
        // - cert-verification failed.
        // - no shared cipher

        ++src_protocol->seq_id();

        const auto send_res = send_ssl_connection_error_msg(
            src_channel, src_protocol,
            "connecting to destination failed with TLS error: " +
                res.error().message());
        if (!send_res) {
          auto ec = send_res.error();
          log_fatal_error_code("sending error failed", ec);

          return send_server_failed(ec);
        }

        return async_send_client_and_finish();
      }
    }
  }

  // tls is established to the server, send the client::greeting
  return server_send_client_greeting_full();
}

stdx::expected<void, std::error_code>
MysqlRoutingClassicConnection::forward_tls(Channel *src_channel,
                                           Channel *dst_channel) {
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
      return stdx::make_unexpected(make_error_code(TlsErrc::kWantRead));
    }

    const auto write_res = dst_channel->write(
        plain_buf.data(0, tls_header_size + tls_payload_size));
    if (!write_res) {
      return stdx::make_unexpected(make_error_code(TlsErrc::kWantWrite));
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
  return stdx::make_unexpected(make_error_code(TlsErrc::kWantRead));
}

void MysqlRoutingClassicConnection::forward_tls_client_to_server() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto dst_channel = socket_splicer->server_channel();

  auto forward_res = forward_tls(src_channel, dst_channel);

  if (!dst_channel->send_buffer().empty()) {
    return async_send_server(Function::kForwardTlsClientToServer);
  }

  if (!forward_res) {
    return async_recv_client(Function::kForwardTlsClientToServer);
  }
}

void MysqlRoutingClassicConnection::forward_tls_server_to_client() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto dst_channel = socket_splicer->client_channel();

  auto forward_res = forward_tls(src_channel, dst_channel);

  if (!dst_channel->send_buffer().empty()) {
    return async_send_client(Function::kForwardTlsServerToClient);
  }

  if (!forward_res) {
    return async_recv_server(Function::kForwardTlsServerToClient);
  }
}

void MysqlRoutingClassicConnection::forward_tls_init() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto dst_channel = socket_splicer->server_channel();

  src_channel->is_tls(true);
  dst_channel->is_tls(true);

  forward_tls_client_to_server();
  forward_tls_server_to_client();
}

stdx::expected<MysqlRoutingClassicConnection::ForwardResult, std::error_code>
MysqlRoutingClassicConnection::forward_frame_sequence_from_client_to_server() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = client_protocol();
  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = server_protocol();

  return forward_frame_sequence(src_channel, src_protocol, dst_channel,
                                dst_protocol);
}

void MysqlRoutingClassicConnection::forward_client_to_server(
    Function this_func, Function next_func) {
  auto forward_res = forward_frame_sequence_from_client_to_server();
  if (!forward_res) {
    return recv_client_failed(forward_res.error());
  }

  switch (forward_res.value()) {
    case ForwardResult::kWantRecvSource:
      return async_recv_client(this_func);
    case ForwardResult::kWantSendSource:
      return async_send_client(this_func);
    case ForwardResult::kWantRecvDestination:
      return async_recv_server(this_func);
    case ForwardResult::kWantSendDestination:
      return async_send_server(this_func);
    case ForwardResult::kFinished:
      return async_send_server(next_func);
  }
}

stdx::expected<MysqlRoutingClassicConnection::ForwardResult, std::error_code>
MysqlRoutingClassicConnection::forward_frame_sequence_from_server_to_client() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();
  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = client_protocol();

  return forward_frame_sequence(src_channel, src_protocol, dst_channel,
                                dst_protocol);
}

void MysqlRoutingClassicConnection::forward_server_to_client(
    Function this_func, Function next_func,
    bool flush_before_next_func_optional) {
  auto forward_res = forward_frame_sequence_from_server_to_client();
  if (!forward_res) {
    return recv_server_failed(forward_res.error());
  }

  switch (forward_res.value()) {
    case ForwardResult::kWantRecvDestination:
      return async_recv_client(this_func);
    case ForwardResult::kWantSendDestination:
      return async_send_client(this_func);
    case ForwardResult::kWantRecvSource:
      return async_recv_server(this_func);
    case ForwardResult::kWantSendSource:
      return async_send_server(this_func);
    case ForwardResult::kFinished: {
      auto *socket_splicer = this->socket_splicer();
      auto dst_channel = socket_splicer->client_channel();

      // if flush is optional and send-buffer is not too full, skip the flush.
      //
      // force-send-buffer-size is a trade-off between latency,
      // syscall-latency and memory usage:
      //
      // - buffering more: less send()-syscalls which helps with small
      // resultset.
      // - buffering less: faster forwarding of smaller packets if the server
      // is send to generate packets.
      constexpr const size_t kForceFlushAfterBytes{16 * 1024};

      if (flush_before_next_func_optional &&
          dst_channel->send_buffer().size() < kForceFlushAfterBytes) {
        return call_next_function(next_func);
      } else {
        return async_send_client(next_func);
      }
    }
  }
}

void MysqlRoutingClassicConnection::auth_client_continue() {
  auto *src_channel = this->socket_splicer()->client_channel();
  auto *src_protocol = client_protocol();

  auto read_res = ensure_frame_header(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_client(Function::kAuthClientContinue);
    }

    log_fatal_error_code("decoding auth-client-continue failed", ec);
    return recv_client_failed(ec);
  }

  forward_client_to_server(Function::kAuthClientContinue,
                           Function::kAuthResponse);
}

void MysqlRoutingClassicConnection::auth_response_auth_method_switch() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();
    return recv_server_failed(ec);
  }

  // if it fails, the next function will fail with not-enough-input
  (void)ensure_has_full_frame(src_channel, src_protocol);

  auto &recv_buf = src_channel->recv_plain_buffer();

  const auto decode_res =
      classic_protocol::decode<classic_protocol::frame::Frame<
          classic_protocol::message::server::AuthMethodSwitch>>(
          net::buffer(recv_buf), src_protocol->shared_capabilities());
  if (!decode_res) {
    auto ec = decode_res.error();
    return recv_server_failed(ec);
  }

  auto switch_auth_msg = decode_res->second.payload();

  // remember the auth_method_name.
  src_protocol->auth_method_name(switch_auth_msg.auth_method());

  forward_server_to_client(Function::kAuthResponseAuthMethodSwitch,
                           Function::kAuthClientContinue);
}

void MysqlRoutingClassicConnection::auth_response_ok() {
  forward_server_to_client(Function::kAuthResponseOk, Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::auth_response_error() {
  forward_server_to_client(Function::kAuthResponseError, Function::kFinish);
}

void MysqlRoutingClassicConnection::auth_response_data() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  if (src_protocol->auth_method_name() == kCachingSha2Password) {
    // if it fails, the next function will fail with bad-message
    (void)ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel->recv_plain_buffer();

    if (recv_buf.size() < 6) {
      return recv_server_failed(make_error_code(std::errc::bad_message));
    }

    switch (recv_buf[5]) {
      case 0x03:
        // fast-auth-ok is followed by Ok
        return forward_server_to_client(Function::kAuthResponseData,
                                        Function::kAuthResponse);
    }
  }

  // followed by a client-packet
  return forward_server_to_client(Function::kAuthResponseData,
                                  Function::kAuthClientContinue);
}

void MysqlRoutingClassicConnection::auth_response() {
  // ERR|OK|EOF|other
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  // ensure the recv_buf has at last frame-header (+ msg-byte)
  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kAuthResponse);
    }

    return recv_server_failed(ec);
  }

  const uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    AuthMethodSwitch =
        cmd_byte<classic_protocol::message::server::AuthMethodSwitch>(),
    Ok = cmd_byte<classic_protocol::message::server::Ok>(),
    Error = cmd_byte<classic_protocol::message::server::Error>(),
    AuthMethodData =
        cmd_byte<classic_protocol::message::server::AuthMethodData>(),
  };

  switch (Msg{msg_type}) {
    case Msg::AuthMethodSwitch:
      return auth_response_auth_method_switch();
    case Msg::Ok:
      return auth_response_ok();
    case Msg::Error:
      return auth_response_error();
    case Msg::AuthMethodData:
      return auth_response_data();
  }

  // if there is another packet, dump its payload for now.
  auto &recv_buf = src_channel->recv_plain_buffer();

  // get as much data of the current frame from the recv-buffers to log it.
  (void)ensure_has_full_frame(src_channel, src_protocol);

  log_debug(
      "received unexpected message from server after a client::Greeting: %s",
      hexify(recv_buf).c_str());

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

void MysqlRoutingClassicConnection::cmd_query_ok() {
  return forward_server_to_client(Function::kCmdQueryOk,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_query_error() {
  return forward_server_to_client(Function::kCmdQueryError,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_query_load_data() {
  return forward_server_to_client(Function::kCmdQueryLoadData,
                                  Function::kCmdQueryLoadDataResponse);
}

void MysqlRoutingClassicConnection::cmd_query_load_data_response_forward() {
  return forward_client_to_server(Function::kCmdQueryLoadDataResponseForward,
                                  Function::kCmdQueryLoadDataResponse);
}

void MysqlRoutingClassicConnection::
    cmd_query_load_data_response_forward_last() {
  return forward_client_to_server(Function::kCmdQueryLoadDataResponseForward,
                                  Function::kCmdQueryResponse);
}

/*
 * loop
 * c->s: payload
 * until payload.is_empty()
 * c<-s: cmd-query-response
 *
 */
void MysqlRoutingClassicConnection::cmd_query_load_data_response() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = client_protocol();

  auto read_res = ensure_frame_header(src_channel, src_protocol);
  if (!read_res) {
    const auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_client(Function::kCmdQueryLoadDataResponse);
    }

    log_fatal_error_code("decoding load-data-response failed", ec);
    return recv_client_failed(ec);
  }

  if (src_protocol->current_frame()->frame_size_ == 4) {
    cmd_query_load_data_response_forward_last();
  } else {
    cmd_query_load_data_response_forward();
  }
}

void MysqlRoutingClassicConnection::cmd_query_column_count() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  // if it fails, the next function will fail with not-enough-input
  (void)ensure_has_full_frame(src_channel, src_protocol);

  auto &recv_buf = src_channel->recv_plain_buffer();

  auto column_count_res = decode_column_count(net::buffer(recv_buf));
  if (!column_count_res) {
    auto ec = column_count_res.error();

    if (ec == classic_protocol::codec_errc::not_enough_input) {
      return async_recv_server(Function::kCmdQueryColumnCount);
    }
  }

  src_protocol->columns_left = column_count_res.value();

  forward_server_to_client(Function::kCmdQueryColumnCount,
                           Function::kCmdQueryColumnMeta, true);
}

void MysqlRoutingClassicConnection::cmd_query_column_meta_forward() {
  return forward_server_to_client(Function::kCmdQueryColumnMetaForward,
                                  Function::kCmdQueryColumnMeta, true);
}

void MysqlRoutingClassicConnection::cmd_query_column_meta_forward_last() {
  return forward_server_to_client(Function::kCmdQueryColumnMetaForwardLast,
                                  Function::kCmdQueryEndOfColumnMeta, true);
}

void MysqlRoutingClassicConnection::cmd_query_row_forward_more_resultsets() {
  return forward_server_to_client(Function::kCmdQueryRowForwardMoreResultsets,
                                  Function::kCmdQueryResponse, true);
}

void MysqlRoutingClassicConnection::cmd_query_column_meta() {
  auto src_protocol = server_protocol();

  if (--src_protocol->columns_left > 0) {
    cmd_query_column_meta_forward();
  } else {
    cmd_query_column_meta_forward_last();
  }
}

void MysqlRoutingClassicConnection::cmd_query_end_of_column_meta() {
  auto src_protocol = server_protocol();
  auto dst_protocol = client_protocol();

  auto skips_eof_pos =
      classic_protocol::capabilities::pos::text_result_with_session_tracking;

  bool server_skips_end_of_columns{
      src_protocol->shared_capabilities().test(skips_eof_pos)};

  bool router_skips_end_of_columns{
      dst_protocol->shared_capabilities().test(skips_eof_pos)};

  if (server_skips_end_of_columns && router_skips_end_of_columns) {
    // this is a Row, not a EOF packet.
    return cmd_query_row();
  } else if (!server_skips_end_of_columns && !router_skips_end_of_columns) {
    return forward_server_to_client(Function::kCmdQueryEndOfColumnMeta,
                                    Function::kCmdQueryRow);
  } else {
    return finish();
  }
}

void MysqlRoutingClassicConnection::cmd_query_row_forward_last() {
  return forward_server_to_client(Function::kCmdQueryRowForwardLast,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_query_row_forward() {
  return forward_server_to_client(Function::kCmdQueryRowForward,
                                  Function::kCmdQueryRow, true);
}

void MysqlRoutingClassicConnection::cmd_query_row() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kCmdQueryRow);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Error = cmd_byte<classic_protocol::message::server::Error>(),
    Eof = cmd_byte<classic_protocol::message::server::Eof>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Eof: {
      // if it fails, the next function will fail with not-enough-input
      (void)ensure_has_full_frame(src_channel, src_protocol);

      auto &recv_buf = src_channel->recv_plain_buffer();
      const auto decode_res =
          classic_protocol::decode<classic_protocol::frame::Frame<
              classic_protocol::message::server::Eof>>(
              net::buffer(recv_buf), src_protocol->shared_capabilities());
      if (!decode_res) {
        auto ec = decode_res.error();

        if (ec == classic_protocol::codec_errc::not_enough_input) {
          return async_recv_server(Function::kCmdQueryRow);
        }

        return recv_server_failed(ec);
      }

      auto eof_msg = decode_res->second.payload();

      if (eof_msg.status_flags().test(
              classic_protocol::status::pos::more_results_exist)) {
        return cmd_query_row_forward_more_resultsets();
      } else {
        return cmd_query_row_forward_last();
      }
    }
    case Msg::Error:
      return cmd_query_row_forward_last();
    default:
      return cmd_query_row_forward();
  }
}

void MysqlRoutingClassicConnection::cmd_query_response() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kCmdQueryResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Error = cmd_byte<classic_protocol::message::server::Error>(),
    Ok = cmd_byte<classic_protocol::message::server::Ok>(),
    LoadData = 0xfb,
  };

  switch (Msg{msg_type}) {
    case Msg::Error:
      return cmd_query_error();
    case Msg::Ok: {
      // if it fails, the next function will fail with not-enough-input
      (void)ensure_has_full_frame(src_channel, src_protocol);

      auto &recv_buf = src_channel->recv_plain_buffer();
      const auto decode_res =
          classic_protocol::decode<classic_protocol::frame::Frame<
              classic_protocol::message::server::Ok>>(
              net::buffer(recv_buf), src_protocol->shared_capabilities());
      if (!decode_res) {
        auto ec = decode_res.error();

        if (ec == classic_protocol::codec_errc::not_enough_input) {
          return async_recv_server(Function::kCmdQueryResponse);
        }

        return recv_server_failed(ec);
      }

      auto ok_msg = decode_res->second.payload();

      if (ok_msg.status_flags().test(
              classic_protocol::status::pos::more_results_exist)) {
        return cmd_query_row_forward_more_resultsets();
      } else {
        return cmd_query_ok();
      }
    }
    case Msg::LoadData:
      return cmd_query_load_data();
  }

  return cmd_query_column_count();
}

void MysqlRoutingClassicConnection::cmd_query() {
  return forward_client_to_server(Function::kCmdQuery,
                                  Function::kCmdQueryResponse);
}

void MysqlRoutingClassicConnection::cmd_ping_response() {
  return forward_server_to_client(Function::kCmdPingResponse,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_ping() {
  return forward_client_to_server(Function::kCmdPing,
                                  Function::kCmdPingResponse);
}

void MysqlRoutingClassicConnection::cmd_quit_response() { finish(); }

PooledClassicConnection make_pooled_connection(
    TlsSwitchableConnection &&other) {
  auto *classic_protocol_state =
      dynamic_cast<ClassicProtocolState *>(other.protocol());
  return {std::move(other.connection()), other.channel()->release_ssl(),
          classic_protocol_state->server_capabilities(),
          classic_protocol_state->client_capabilities()

  };
}

void MysqlRoutingClassicConnection::cmd_quit() {
  // move the connection to the pool.
  //
  // the pool will either close it or keep it alive.
  auto &pools = ConnectionPoolComponent::get_instance();

  if (auto pool = pools.get(ConnectionPoolComponent::default_pool_name())) {
    pool->add(make_pooled_connection(std::exchange(
        socket_splicer()->server_conn(),
        TlsSwitchableConnection{
            nullptr, nullptr, socket_splicer()->server_conn().tls_switchable(),
            std::make_unique<ClassicProtocolState>()})));

    // client's expect the server to close first.
    //
    // close the sending side and wait until the client closed its side too.
    (void)socket_splicer()->client_conn().shutdown(
        net::socket_base::shutdown_send);

    async_wait_client_closed();
  } else {
    return forward_client_to_server(Function::kCmdQuit,
                                    Function::kCmdQuitResponse);
  }
}

void MysqlRoutingClassicConnection::cmd_init_schema_response() {
  return forward_server_to_client(Function::kCmdInitSchemaResponse,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_init_schema() {
  return forward_client_to_server(Function::kCmdInitSchema,
                                  Function::kCmdInitSchemaResponse);
}

void MysqlRoutingClassicConnection::cmd_reset_connection_response() {
  return forward_server_to_client(Function::kCmdResetConnectionResponse,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_reset_connection() {
  return forward_client_to_server(Function::kCmdResetConnection,
                                  Function::kCmdResetConnectionResponse);
}

void MysqlRoutingClassicConnection::cmd_kill_response() {
  return forward_server_to_client(Function::kCmdKillResponse,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_kill() {
  return forward_client_to_server(Function::kCmdKill,
                                  Function::kCmdKillResponse);
}

void MysqlRoutingClassicConnection::cmd_change_user() {
  return forward_client_to_server(Function::kCmdChangeUser,
                                  Function::kCmdChangeUserResponse);
}

void MysqlRoutingClassicConnection::cmd_change_user_response() {
  // ERR|OK|EOF|other
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kCmdChangeUserResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Error = cmd_byte<classic_protocol::message::server::Error>(),
    Ok = cmd_byte<classic_protocol::message::server::Ok>(),
    AuthSwitchUser =
        cmd_byte<classic_protocol::message::server::AuthMethodSwitch>(),
    AuthContinue =
        cmd_byte<classic_protocol::message::server::AuthMethodData>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Error:
      return cmd_change_user_response_error();
    case Msg::Ok:
      return cmd_change_user_response_ok();
    case Msg::AuthSwitchUser:
      return cmd_change_user_response_switch_auth();
    case Msg::AuthContinue:
      return cmd_change_user_response_continue();
    default: {
      const auto &recv_buf = src_channel->recv_plain_buffer();

      log_debug(
          "received unexpected message from server after a "
          "client::ChangeUser: %s",
          hexify(recv_buf).c_str());

      return recv_server_failed(make_error_code(std::errc::bad_message));
    }
  }
}

void MysqlRoutingClassicConnection::cmd_change_user_response_error() {
  return forward_server_to_client(Function::kCmdChangeUserResponseError,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_change_user_response_ok() {
  return forward_server_to_client(Function::kCmdChangeUserResponseOk,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_change_user_response_switch_auth() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();
    return recv_server_failed(ec);
  }

  // if it fails, the next function will fail with not-enough-input
  (void)ensure_has_full_frame(src_channel, src_protocol);

  auto &recv_buf = src_channel->recv_plain_buffer();

  const auto decode_res =
      classic_protocol::decode<classic_protocol::frame::Frame<
          classic_protocol::message::server::AuthMethodSwitch>>(
          net::buffer(recv_buf), src_protocol->shared_capabilities());
  if (!decode_res) {
    auto ec = decode_res.error();

    if (ec == classic_protocol::codec_errc::not_enough_input) {
      return async_recv_server(Function::kCmdChangeUserResponseSwitchAuth);
    }

    return recv_server_failed(ec);
  }

  auto switch_auth_msg = decode_res->second.payload();

#if 0
  std::cerr << __LINE__ << ": .. switching to " << switch_auth_msg.auth_method()
            << "\n";
  std::cerr << __LINE__ << ": .. auth-data "
            << hexify(switch_auth_msg.auth_method_data()) << "\n";
#endif

  src_protocol->auth_method_name(switch_auth_msg.auth_method());

  return forward_server_to_client(Function::kCmdChangeUserResponseSwitchAuth,
                                  Function::kCmdChangeUserClientAuthContinue);
}

void MysqlRoutingClassicConnection::cmd_change_user_response_continue() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  if (src_protocol->auth_method_name() == kCachingSha2Password) {
    // if ensure_has_full_frame fails, we'll fail later with bad_message.
    (void)ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel->recv_plain_buffer();

    if (recv_buf.size() < 6) {
      return recv_server_failed(make_error_code(std::errc::bad_message));
    }

    switch (recv_buf[5]) {
      case 0x03:
        // fast-auth-ok is followed by Ok
        return forward_server_to_client(
            Function::kCmdChangeUserResponseContinue,
            Function::kCmdChangeUserResponse);
    }
  }

  return forward_server_to_client(Function::kCmdChangeUserResponseContinue,
                                  Function::kCmdChangeUserClientAuthContinue);
}

void MysqlRoutingClassicConnection::cmd_change_user_client_auth_continue() {
  return forward_client_to_server(Function::kCmdChangeUserClientAuthContinue,
                                  Function::kCmdChangeUserResponse);
}

void MysqlRoutingClassicConnection::cmd_reload_response() {
  return forward_server_to_client(Function::kCmdReloadResponse,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_reload() {
  return forward_client_to_server(Function::kCmdReload,
                                  Function::kCmdReloadResponse);
}

void MysqlRoutingClassicConnection::cmd_statistics() {
  return forward_client_to_server(Function::kCmdStatistics,
                                  Function::kCmdStatisticsResponse);
}

void MysqlRoutingClassicConnection::cmd_statistics_response() {
  return forward_server_to_client(Function::kCmdStatisticsResponse,
                                  Function::kClientRecvCmd);
}

// ListFields

void MysqlRoutingClassicConnection::cmd_list_fields() {
  return forward_client_to_server(Function::kCmdListFields,
                                  Function::kCmdListFieldsResponse);
}

void MysqlRoutingClassicConnection::cmd_list_fields_response_forward() {
  return forward_server_to_client(Function::kCmdListFieldsResponseForward,
                                  Function::kCmdListFieldsResponse);
}

void MysqlRoutingClassicConnection::cmd_list_fields_response_forward_last() {
  return forward_server_to_client(Function::kCmdListFieldsResponseForwardLast,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_list_fields_response() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kCmdListFieldsResponse);
    }

    return recv_server_failed(ec);
  }

  const uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Error = cmd_byte<classic_protocol::message::server::Error>(),
    Eof = cmd_byte<classic_protocol::message::server::Eof>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Eof:
    case Msg::Error:
      return cmd_list_fields_response_forward_last();
  }

  return cmd_list_fields_response_forward();
}

void MysqlRoutingClassicConnection::cmd_stmt_prepare_response_ok() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();
    return recv_server_failed(ec);
  }

  // if it fails, the next function will fail with not-enough-input
  (void)ensure_has_full_frame(src_channel, src_protocol);

  auto &recv_buf = src_channel->recv_plain_buffer();

  const auto decode_res =
      classic_protocol::decode<classic_protocol::frame::Frame<
          classic_protocol::message::server::StmtPrepareOk>>(
          net::buffer(recv_buf), src_protocol->shared_capabilities());
  if (!decode_res) {
    auto ec = decode_res.error();

    if (ec == classic_protocol::codec_errc::not_enough_input) {
      return async_recv_server(Function::kCmdStmtPrepareResponseOk);
    }

    return recv_server_failed(ec);
  }

  auto stmt_prep_ok = decode_res->second.payload();

  if (stmt_prep_ok.with_metadata()) {
    src_protocol->columns_left = stmt_prep_ok.column_count();
    src_protocol->params_left = stmt_prep_ok.param_count();
  } else {
    src_protocol->columns_left = 0;
    src_protocol->params_left = 0;
  }

  return forward_server_to_client(Function::kCmdStmtPrepareResponseOk,
                                  Function::kCmdStmtPrepareResponseCheckParam);
}

void MysqlRoutingClassicConnection::cmd_stmt_prepare_response_check_param() {
  auto src_protocol = server_protocol();

  if (src_protocol->params_left > 0) {
    if (--src_protocol->params_left == 0) {
      return cmd_stmt_prepare_response_forward_param_last();
    } else {
      return cmd_stmt_prepare_response_forward_param();
    }
  }

  return cmd_stmt_prepare_response_check_column();
}

void MysqlRoutingClassicConnection::cmd_stmt_prepare_response_forward_param() {
  return forward_server_to_client(Function::kCmdStmtPrepareResponseForwardParam,
                                  Function::kCmdStmtPrepareResponseCheckParam);
}

void MysqlRoutingClassicConnection::
    cmd_stmt_prepare_response_forward_param_last() {
  return forward_server_to_client(
      Function::kCmdStmtPrepareResponseForwardParamLast,
      Function::kCmdStmtPrepareResponseForwardEndOfParams);
}

void MysqlRoutingClassicConnection::
    cmd_stmt_prepare_response_forward_end_of_params() {
  auto src_protocol = server_protocol();

  if (src_protocol->shared_capabilities().test(
          classic_protocol::capabilities::pos::
              text_result_with_session_tracking)) {
    // no end-of-columns packet.
    return cmd_stmt_prepare_response_check_column();
  }

  return forward_server_to_client(
      Function::kCmdStmtPrepareResponseForwardEndOfParams,
      Function::kCmdStmtPrepareResponseCheckColumn);
}

void MysqlRoutingClassicConnection::cmd_stmt_prepare_response_check_column() {
  auto src_protocol = server_protocol();

  if (src_protocol->columns_left > 0) {
    if (--src_protocol->columns_left == 0) {
      return cmd_stmt_prepare_response_forward_column_last();
    } else {
      return cmd_stmt_prepare_response_forward_column();
    }
  }

  // nothing to forward, wait for the client again.
  client_recv_cmd();
}

void MysqlRoutingClassicConnection::cmd_stmt_prepare_response_forward_column() {
  return forward_server_to_client(
      Function::kCmdStmtPrepareResponseForwardColumn,
      Function::kCmdStmtPrepareResponseCheckColumn);
}

void MysqlRoutingClassicConnection::
    cmd_stmt_prepare_response_forward_column_last() {
  return forward_server_to_client(
      Function::kCmdStmtPrepareResponseForwardColumnLast,
      Function::kCmdStmtPrepareResponseForwardEndOfColumns);
}

void MysqlRoutingClassicConnection::
    cmd_stmt_prepare_response_forward_end_of_columns() {
  auto src_protocol = server_protocol();

  if (src_protocol->shared_capabilities().test(
          classic_protocol::capabilities::pos::
              text_result_with_session_tracking)) {
    // no end-of-columns packet.
    return call_next_function(Function::kClientRecvCmd);
  }

  return forward_server_to_client(
      Function::kCmdStmtPrepareResponseForwardEndOfColumns,
      Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_stmt_prepare_response_error() {
  return forward_server_to_client(Function::kCmdStmtPrepareResponseError,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_stmt_prepare_response() {
  // ERR|OK|EOF|other
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kCmdStmtPrepareResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Error = cmd_byte<classic_protocol::message::server::Error>(),
    Ok = cmd_byte<classic_protocol::message::server::StmtPrepareOk>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Error:
      return cmd_stmt_prepare_response_error();
    case Msg::Ok:
      return cmd_stmt_prepare_response_ok();
  }

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

void MysqlRoutingClassicConnection::cmd_stmt_prepare() {
  return forward_client_to_server(Function::kCmdStmtPrepare,
                                  Function::kCmdStmtPrepareResponse);
}

// StmtExecute
//
void MysqlRoutingClassicConnection::cmd_stmt_execute_response_ok() {
  return forward_server_to_client(Function::kCmdStmtExecuteResponseOk,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_stmt_execute_response_error() {
  return forward_server_to_client(Function::kCmdStmtExecuteResponseError,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_stmt_execute_response_column_count() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  // if it fails, the next function will fail with not-enough-input
  (void)ensure_has_full_frame(src_channel, src_protocol);

  auto &recv_buf = src_channel->recv_plain_buffer();

  auto column_count_res = decode_column_count(net::buffer(recv_buf));
  if (!column_count_res) {
    auto ec = column_count_res.error();

    if (ec == classic_protocol::codec_errc::not_enough_input) {
      return async_recv_server(Function::kCmdStmtExecuteResponseColumnCount);
    }

    return recv_server_failed(ec);
  }

  src_protocol->columns_left = column_count_res.value();

  forward_server_to_client(Function::kCmdStmtExecuteResponseColumnCount,
                           Function::kCmdStmtExecuteResponseCheckColumn);
}

void MysqlRoutingClassicConnection::cmd_stmt_execute_response_check_column() {
  auto src_protocol = server_protocol();

  if (--src_protocol->columns_left == 0) {
    cmd_stmt_execute_response_forward_column_last();
  } else {
    cmd_stmt_execute_response_forward_column();
  }
}

void MysqlRoutingClassicConnection::cmd_stmt_execute_response_forward_column() {
  return forward_server_to_client(
      Function::kCmdStmtExecuteResponseForwardColumn,
      Function::kCmdStmtExecuteResponseCheckColumn);
}

void MysqlRoutingClassicConnection::
    cmd_stmt_execute_response_forward_column_last() {
  return forward_server_to_client(
      Function::kCmdStmtExecuteResponseForwardColumnLast,
      Function::kCmdStmtExecuteResponseForwardEndOfColumns);
}

void MysqlRoutingClassicConnection::
    cmd_stmt_execute_response_forward_end_of_columns() {
  auto src_protocol = server_protocol();

  if (src_protocol->shared_capabilities().test(
          classic_protocol::capabilities::pos::
              text_result_with_session_tracking)) {
    // no end-of-columns packet.
    return call_next_function(Function::kCmdStmtExecuteResponseCheckRow);
  }

  return forward_server_to_client(
      Function::kCmdStmtPrepareResponseForwardEndOfColumns,
      Function::kCmdStmtExecuteResponseCheckRow);
}

void MysqlRoutingClassicConnection::
    cmd_stmt_execute_response_forward_end_of_rows() {
  return forward_server_to_client(
      Function::kCmdStmtExecuteResponseForwardEndOfRows,
      Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_stmt_execute_response_forward_row() {
  return forward_server_to_client(Function::kCmdStmtExecuteResponseForwardRow,
                                  Function::kCmdStmtExecuteResponseCheckRow);
}

void MysqlRoutingClassicConnection::cmd_stmt_execute_response_check_row() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kCmdStmtExecuteResponseCheckRow);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Row = 0x00,
    Eof = cmd_byte<classic_protocol::message::server::Eof>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Eof:
      return cmd_stmt_execute_response_forward_end_of_rows();
    case Msg::Row:
      return cmd_stmt_execute_response_forward_row();
  }

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

void MysqlRoutingClassicConnection::cmd_stmt_execute_response() {
  // ERR|OK|EOF|other
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kCmdStmtExecuteResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Error = cmd_byte<classic_protocol::message::server::Error>(),
    Ok = cmd_byte<classic_protocol::message::server::Ok>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Error:
      return cmd_stmt_execute_response_error();
    case Msg::Ok:
      return cmd_stmt_execute_response_ok();
  }

  return cmd_stmt_execute_response_column_count();
}

void MysqlRoutingClassicConnection::cmd_stmt_execute() {
  return forward_client_to_server(Function::kCmdStmtExecute,
                                  Function::kCmdStmtExecuteResponse);
}

void MysqlRoutingClassicConnection::cmd_stmt_set_option_response() {
  return forward_server_to_client(Function::kCmdStmtSetOptionResponse,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_stmt_set_option() {
  return forward_client_to_server(Function::kCmdStmtSetOption,
                                  Function::kCmdStmtSetOptionResponse);
}

void MysqlRoutingClassicConnection::cmd_stmt_reset_response() {
  return forward_server_to_client(Function::kCmdStmtResetResponse,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_stmt_reset() {
  return forward_client_to_server(Function::kCmdStmtReset,
                                  Function::kCmdStmtResetResponse);
}

void MysqlRoutingClassicConnection::cmd_stmt_close() {
  return forward_client_to_server(Function::kCmdStmtClose,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_stmt_param_append_data() {
  return forward_client_to_server(Function::kCmdStmtParamAppendData,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_stmt_fetch_response() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kCmdStmtExecuteResponseCheckRow);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Row = 0x00,
    Eof = cmd_byte<classic_protocol::message::server::Eof>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Eof:
      return cmd_stmt_execute_response_forward_end_of_rows();
    case Msg::Row:
      return cmd_stmt_execute_response_forward_row();
  }

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

void MysqlRoutingClassicConnection::cmd_stmt_fetch() {
  return forward_client_to_server(Function::kCmdStmtFetch,
                                  Function::kCmdStmtFetchResponse);
}

void MysqlRoutingClassicConnection::cmd_clone() {
  return forward_client_to_server(Function::kCmdClone,
                                  Function::kCmdCloneResponse);
}

void MysqlRoutingClassicConnection::cmd_clone_response() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kCmdCloneResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Ok = cmd_byte<classic_protocol::message::server::Ok>(),
    Err = cmd_byte<classic_protocol::message::server::Error>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Ok:
      return cmd_clone_response_forward_ok();
    case Msg::Err:
      return cmd_clone_response_forward_error();
  }

  return recv_server_failed(make_error_code(std::errc::bad_message));
}

void MysqlRoutingClassicConnection::cmd_clone_response_forward_error() {
  return forward_server_to_client(Function::kCmdCloneResponseForwardError,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_clone_response_forward_ok() {
  return forward_server_to_client(Function::kCmdCloneResponseForwardError,
                                  Function::kClientRecvCloneCmd);
}

#if defined(DEBUG_CMD_CLONE)
static std::string_view clone_client_command_byte_to_string(uint8_t msg_type) {
  using namespace classic_protocol::clone::client;

  switch (CommandByte{msg_type}) {
    case CommandByte::Init:
      return "Init";
    case CommandByte::Execute:
      return "Execute";
    case CommandByte::Exit:
      return "Exit";
    case CommandByte::Reinit:
      return "Reinit";
    case CommandByte::Ack:
      return "Ack";
    case CommandByte::Attach:
      return "Attach";
  }

  return "Unknown Cmd";
}
#endif

void MysqlRoutingClassicConnection::client_recv_clone_cmd() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = client_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_client(Function::kClientRecvCloneCmd);
    }

    return recv_client_failed(ec);
  }

  const uint8_t msg_type = src_protocol->current_msg_type().value();

#if defined(DEBUG_CMD_CLONE)
  std::cerr << "c->s: clone::" << clone_client_command_byte_to_string(msg_type)
            << " (" << (int)msg_type << ")\n";
#endif

  enum class Msg {
    Init = cmd_byte<classic_protocol::clone::client::Init>(),
    Attach = cmd_byte<classic_protocol::clone::client::Attach>(),
    Reinit = cmd_byte<classic_protocol::clone::client::Reinit>(),
    Execute = cmd_byte<classic_protocol::clone::client::Execute>(),
    Ack = cmd_byte<classic_protocol::clone::client::Ack>(),
    Exit = cmd_byte<classic_protocol::clone::client::Exit>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Init:
    case Msg::Attach:
    case Msg::Reinit:
    case Msg::Execute:
    case Msg::Ack:
      return cmd_clone_init();
    case Msg::Exit:
      return cmd_clone_exit();
  }

  std::cerr << __LINE__ << ": Unknown clone-cmd: " << (int)msg_type << "\n";

  return recv_client_failed(make_error_code(std::errc::bad_message));
}

void MysqlRoutingClassicConnection::cmd_clone_init() {
  return forward_client_to_server(Function::kCmdCloneInit,
                                  Function::kCmdCloneInitResponse);
}

#if defined(DEBUG_CMD_CLONE)
static std::string_view clone_server_command_byte_to_string(uint8_t msg_type) {
  using namespace classic_protocol::clone::server;

  switch (CommandByte{msg_type}) {
    case CommandByte::Locators:
      return "Locators";
    case CommandByte::DataDescriptor:
      return "DataDescriptor";
    case CommandByte::Data:
      return "Data";
    case CommandByte::Config:
      return "Config";
    case CommandByte::Collation:
      return "Collation";
    case CommandByte::Plugin:
      return "Plugin";
    case CommandByte::PluginV2:
      return "PluginV2";
    case CommandByte::ConfigV3:
      return "ConfigV3";
    case CommandByte::Complete:
      return "Complete";
    case CommandByte::Error:
      return "Error";
  }

  return "Unknown Cmd";
}
#endif

void MysqlRoutingClassicConnection::cmd_clone_init_response() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    const auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kCmdCloneInitResponse);
    }

    return recv_server_failed(ec);
  }

  const uint8_t msg_type = src_protocol->current_msg_type().value();

#if defined(DEBUG_CMD_CLONE)
  std::cerr << "c<-s: clone::" << clone_server_command_byte_to_string(msg_type)
            << " (" << (int)msg_type << ")\n";
#endif

  enum class Msg {
    Complete = cmd_byte<classic_protocol::clone::server::Complete>(),
    Error = cmd_byte<classic_protocol::clone::server::Error>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Complete:
    case Msg::Error:
      return cmd_clone_init_response_forward_last();
    default:
      return cmd_clone_init_response_forward();
  }
}

void MysqlRoutingClassicConnection::cmd_clone_init_response_forward() {
  return forward_server_to_client(Function::kCmdCloneInitResponseForward,
                                  Function::kCmdCloneInitResponse);
}

void MysqlRoutingClassicConnection::cmd_clone_init_response_forward_last() {
  return forward_server_to_client(Function::kCmdCloneInitResponseForwardLast,
                                  Function::kClientRecvCloneCmd);
}

void MysqlRoutingClassicConnection::cmd_clone_exit() {
  return forward_client_to_server(Function::kCmdCloneExit,
                                  Function::kCmdCloneExitResponse);
}

void MysqlRoutingClassicConnection::cmd_clone_exit_response() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kCmdCloneExitResponse);
    }

    return recv_server_failed(ec);
  }

  const uint8_t msg_type = src_protocol->current_msg_type().value();

#if defined(DEBUG_CMD_CLONE)
  std::cerr << "c<-s: clone::" << clone_server_command_byte_to_string(msg_type)
            << " (" << (int)msg_type << ")\n";
#endif

  enum class Msg {
    Complete = cmd_byte<classic_protocol::clone::server::Complete>(),
    Error = cmd_byte<classic_protocol::clone::server::Error>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Complete:
    case Msg::Error:
      return cmd_clone_exit_response_forward_last();
    default:
      return cmd_clone_exit_response_forward();
  }
}

void MysqlRoutingClassicConnection::cmd_clone_exit_response_forward() {
  return forward_server_to_client(Function::kCmdCloneExitResponseForward,
                                  Function::kCmdCloneExitResponse);
}

void MysqlRoutingClassicConnection::cmd_clone_exit_response_forward_last() {
  return forward_server_to_client(Function::kCmdCloneExitResponseForwardLast,
                                  Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_binlog_dump() {
  forward_client_to_server(Function::kCmdBinlogDump,
                           Function::kCmdBinlogDumpResponse);
}

void MysqlRoutingClassicConnection::cmd_binlog_dump_gtid() {
  forward_client_to_server(Function::kCmdBinlogDumpGtid,
                           Function::kCmdBinlogDumpResponse);
}

void MysqlRoutingClassicConnection::cmd_register_replica() {
  forward_client_to_server(Function::kCmdRegisterReplica,
                           Function::kCmdRegisterReplicaResponse);
}

void MysqlRoutingClassicConnection::cmd_register_replica_response() {
  forward_server_to_client(Function::kCmdRegisterReplicaResponse,
                           Function::kClientRecvCmd);
}

void MysqlRoutingClassicConnection::cmd_binlog_dump_response() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = server_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kCmdBinlogDumpResponse);
    }

    return recv_server_failed(ec);
  }

  const uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Err = cmd_byte<classic_protocol::message::server::Error>(),
    Eof = cmd_byte<classic_protocol::message::server::Eof>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Err:
    case Msg::Eof:
      return cmd_binlog_dump_response_forward_last();
    default:
      return cmd_binlog_dump_response_forward();
  }
}

void MysqlRoutingClassicConnection::cmd_binlog_dump_response_forward() {
  forward_server_to_client(Function::kCmdBinlogDumpResponseForward,
                           Function::kCmdBinlogDumpResponse);
}

void MysqlRoutingClassicConnection::cmd_binlog_dump_response_forward_last() {
  forward_server_to_client(Function::kCmdBinlogDumpResponseForwardLast,
                           Function::kClientRecvCmd);
}

// something was received on the client channel.
void MysqlRoutingClassicConnection::client_recv_cmd() {
  auto *socket_splicer = this->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = client_protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_client(Function::kClientRecvCmd);
    }

    return recv_client_failed(ec);
  }

  const uint8_t msg_type = src_protocol->current_msg_type().value();

  enum class Msg {
    Quit = cmd_byte<classic_protocol::message::client::Quit>(),
    InitSchema = cmd_byte<classic_protocol::message::client::InitSchema>(),
    Query = cmd_byte<classic_protocol::message::client::Query>(),
    ListFields = cmd_byte<classic_protocol::message::client::ListFields>(),
    Reload = cmd_byte<classic_protocol::message::client::Reload>(),
    Statistics = cmd_byte<classic_protocol::message::client::Statistics>(),
    // ProcessInfo =
    // cmd_byte<classic_protocol::message::client::ProcessInfo>(),
    Kill = cmd_byte<classic_protocol::message::client::Kill>(),
    Ping = cmd_byte<classic_protocol::message::client::Ping>(),
    ChangeUser = cmd_byte<classic_protocol::message::client::ChangeUser>(),
    BinlogDump = cmd_byte<classic_protocol::message::client::BinlogDump>(),
    RegisterReplica =
        cmd_byte<classic_protocol::message::client::RegisterReplica>(),
    StmtPrepare = cmd_byte<classic_protocol::message::client::StmtPrepare>(),
    StmtExecute = cmd_byte<classic_protocol::message::client::StmtExecute>(),
    StmtParamAppendData =
        cmd_byte<classic_protocol::message::client::StmtParamAppendData>(),
    StmtClose = cmd_byte<classic_protocol::message::client::StmtClose>(),
    StmtReset = cmd_byte<classic_protocol::message::client::StmtReset>(),
    StmtSetOption =
        cmd_byte<classic_protocol::message::client::StmtSetOption>(),
    StmtFetch = cmd_byte<classic_protocol::message::client::StmtFetch>(),
    BinlogDumpGtid =
        cmd_byte<classic_protocol::message::client::BinlogDumpGtid>(),
    ResetConnection =
        cmd_byte<classic_protocol::message::client::ResetConnection>(),
    Clone = cmd_byte<classic_protocol::message::client::Clone>(),
    // SubscribeGroupReplicationStream = cmd_byte<
    //     classic_protocol::message::client::SubscribeGroupReplicationStream>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Quit:
      return cmd_quit();
    case Msg::InitSchema:
      return cmd_init_schema();
    case Msg::Query:
      return cmd_query();
    case Msg::ListFields:
      return cmd_list_fields();
    case Msg::ChangeUser:
      return cmd_change_user();
    case Msg::Ping:
      return cmd_ping();
    case Msg::ResetConnection:
      return cmd_reset_connection();
    case Msg::Kill:
      return cmd_kill();
    case Msg::Reload:
      return cmd_reload();
    case Msg::Statistics:
      return cmd_statistics();
    case Msg::StmtPrepare:
      return cmd_stmt_prepare();
    case Msg::StmtExecute:
      return cmd_stmt_execute();
    case Msg::StmtClose:
      return cmd_stmt_close();
    case Msg::StmtFetch:
      return cmd_stmt_fetch();
    case Msg::StmtSetOption:
      return cmd_stmt_set_option();
    case Msg::StmtReset:
      return cmd_stmt_reset();
    case Msg::StmtParamAppendData:
      return cmd_stmt_param_append_data();
    case Msg::Clone:
      return cmd_clone();
    case Msg::BinlogDump:
      return cmd_binlog_dump();
    case Msg::BinlogDumpGtid:
      return cmd_binlog_dump_gtid();
    case Msg::RegisterReplica:
      return cmd_register_replica();
  }

  // unknown command

  auto send_res = send_error_packet(
      src_channel, src_protocol,
      {ER_UNKNOWN_COM_ERROR, "Unknown command " + std::to_string(msg_type),
       "HY000"});
  if (!send_res) {
    return async_send_client_and_finish();
  }

  // drain the current command from the recv-buffers.
  (void)ensure_has_full_frame(src_channel, src_protocol);

  // try to discard the current message.
  //
  // if the current message isn't received completely yet, drop the connection
  // after sending the error-message.
  auto discard_res = discard_current_msg(src_channel, src_protocol);

  if (!discard_res) {
    return async_send_client_and_finish();
  } else {
    return async_send_client(Function::kClientRecvCmd);
  }
}
