/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "x_connection.h"

#include <exception>
#include <mutex>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/message_lite.h>
#include <mysqld_error.h>  // ER_SECURE_TRANSPORT_REQUIRED
#include <mysqlx.pb.h>
#include <mysqlx_connection.pb.h>
#include <mysqlx_datatypes.pb.h>
#include <mysqlx_error.h>  // ER_X_BAD_MESSAGE

#include "hexify.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqlrouter/classic_protocol_wire.h"
#include "mysqlrouter/connection_pool_component.h"
#include "mysqlrouter/routing_component.h"
#include "mysqlrouter/ssl_mode.h"
#include "tls_content_type.h"

IMPORT_LOG_FUNCTIONS()

using mysql_harness::hexify;

static void log_fatal_error_code(const char *msg, std::error_code ec) {
  log_warning("%s: %s (%s:%d)", msg, ec.message().c_str(), ec.category().name(),
              ec.value());
}

static size_t message_byte_size(const google::protobuf::MessageLite &msg) {
#if (defined(GOOGLE_PROTOBUF_VERSION) && GOOGLE_PROTOBUF_VERSION > 3000000)
  return msg.ByteSizeLong();
#else
  return msg.ByteSize();
#endif
}

// map a c++-type to a msg-type
static constexpr uint8_t xproto_frame_msg_type(const Mysqlx::Error &) {
  return Mysqlx::ServerMessages::ERROR;
}

static constexpr uint8_t xproto_frame_msg_type(const Mysqlx::Ok &) {
  return Mysqlx::ServerMessages::OK;
}

static constexpr uint8_t xproto_frame_msg_type(
    const Mysqlx::Connection::Capabilities &) {
  return Mysqlx::ServerMessages::CONN_CAPABILITIES;
}

static constexpr uint8_t xproto_frame_msg_type(
    const Mysqlx::Connection::CapabilitiesSet &) {
  return Mysqlx::ClientMessages::CON_CAPABILITIES_SET;
}

static constexpr uint8_t xproto_frame_msg_type(
    const Mysqlx::Connection::CapabilitiesGet &) {
  return Mysqlx::ClientMessages::CON_CAPABILITIES_GET;
}

static bool has_frame_header(XProtocolState &src_protocol) {
  return src_protocol.current_frame().has_value();
}

static bool has_msg_type(XProtocolState &src_protocol) {
  return src_protocol.current_msg_type().has_value();
}

static stdx::expected<std::pair<size_t, XProtocolState::FrameInfo>,
                      std::error_code>
decode_frame_header(const net::const_buffer &recv_buf) {
  // decode the frame and adjust the sequence number as needed.
  const auto decode_res =
      classic_protocol::decode<classic_protocol::wire::FixedInt<4>>(
          net::buffer(recv_buf), 0);
  if (!decode_res) {
    auto ec = decode_res.error();

    if (ec == classic_protocol::codec_errc::not_enough_input) {
      return stdx::unexpected(make_error_code(TlsErrc::kWantRead));
    }
    return stdx::unexpected(decode_res.error());
  }

  const auto frame_header_res = decode_res.value();
  const auto header_size = frame_header_res.first;
  const auto payload_size = frame_header_res.second.value();

  const auto frame_size = header_size + payload_size;

  return std::make_pair(header_size, XProtocolState::FrameInfo{frame_size, 0u});
}

static stdx::expected<size_t, std::error_code> ensure_frame_header(
    Channel &src_channel, XProtocolState &src_protocol) {
  auto &recv_buf = src_channel.recv_plain_view();

  size_t min_size{4};
  auto cur_size = recv_buf.size();
  if (cur_size < min_size) {
    // read the rest of the header.
    auto read_res = src_channel.read_to_plain(min_size - cur_size);
    if (!read_res) return stdx::unexpected(read_res.error());

    if (recv_buf.size() < min_size) {
      return stdx::unexpected(make_error_code(TlsErrc::kWantRead));
    }
  }

  auto decode_frame_res = decode_frame_header(net::buffer(recv_buf));
  if (!decode_frame_res) return stdx::unexpected(decode_frame_res.error());

  const size_t header_size = decode_frame_res.value().first;
  src_protocol.current_frame() = decode_frame_res.value().second;

  return header_size;
}

/**
 * ensure recv-channel contains a frame+msg-header.
 *
 * frame-header is: @c len
 * msg-header is:   @c msg-type
 *
 * @retval true if src_protocol's msg_type() is valid()
 * @retval false .error() will contain the reason for error.
 *
 * - std::errc::bad_message frame is too small.
 * - TlsErrc::kWantRead for more data is needed.
 */
static stdx::expected<void, std::error_code> ensure_has_msg_prefix(
    Channel &src_channel, XProtocolState &src_protocol) {
  if (has_frame_header(src_protocol) && has_msg_type(src_protocol)) return {};

  if (!has_frame_header(src_protocol)) {
    const auto decode_frame_res =
        ensure_frame_header(src_channel, src_protocol);
    if (!decode_frame_res) return stdx::unexpected(decode_frame_res.error());
  }

  if (!has_msg_type(src_protocol)) {
    auto &current_frame = src_protocol.current_frame().value();

    if (current_frame.frame_size_ < 5) {
      // expected a frame with at least one msg-type-byte
      return stdx::unexpected(make_error_code(std::errc::bad_message));
    }

    if (current_frame.forwarded_frame_size_ >= 4) {
      return stdx::unexpected(make_error_code(std::errc::bad_message));
    }

    const size_t msg_type_pos = 4 - current_frame.forwarded_frame_size_;

    auto &recv_buf = src_channel.recv_plain_view();
    if (msg_type_pos >= recv_buf.size()) {
      // read some more data.
      auto read_res = src_channel.read_to_plain(1);
      if (!read_res) return stdx::unexpected(read_res.error());

      if (msg_type_pos >= recv_buf.size()) {
        return stdx::unexpected(make_error_code(TlsErrc::kWantRead));
      }
    }

    src_protocol.current_msg_type() = recv_buf[msg_type_pos];
  }

#if defined(DEBUG_IO)
  std::cerr << __LINE__ << ": "
            << "seq-id: " << (int)src_protocol.current_frame()->seq_id_
            << ", frame-size: "
            << (int)src_protocol.current_frame()->frame_size_
            << ", msg-type: " << (int)src_protocol.current_msg_type().value()
            << "\n";
#endif

  return {};
}

static stdx::expected<void, std::error_code> ensure_has_full_frame(
    Channel &src_channel, XProtocolState &src_protocol) {
  auto &current_frame = src_protocol.current_frame().value();
  auto &recv_buf = src_channel.recv_plain_view();

  const auto min_size = current_frame.frame_size_;
  const auto cur_size = recv_buf.size();
  if (cur_size >= min_size) return {};

  auto read_res = src_channel.read_to_plain(min_size - cur_size);

  if (!read_res) return stdx::unexpected(read_res.error());

  return {};
}

static void discard_current_msg(Channel &src_channel,
                                XProtocolState &src_protocol) {
  auto &opt_current_frame = src_protocol.current_frame();
  if (!opt_current_frame) return;

  auto &current_frame = *opt_current_frame;

  auto &recv_buf = src_channel.recv_plain_view();

  harness_assert(current_frame.frame_size_ <= recv_buf.size());
  harness_assert(current_frame.forwarded_frame_size_ == 0);

  src_channel.consume_plain(current_frame.frame_size_);

  // unset current frame and also current-msg
  src_protocol.current_frame().reset();
  src_protocol.current_msg_type().reset();
}

void MysqlRoutingXConnection::disconnect() {
  disconnect_request([this](auto &req) {
    auto &io_ctx = client_conn().connection()->io_ctx();

    if (io_ctx.stopped()) abort();

    req = true;

    net::dispatch(io_ctx, [this, self = shared_from_this()]() {
      (void)client_conn().cancel();
      (void)server_conn().cancel();

      connector().socket().cancel();
    });
  });
}

/**
 * encode a message into a xproto frame.
 *
 * - 4-byte length (msg-type + payload)
 * - 1-byte msg-type
 * - payload
 */
template <class T>
static size_t xproto_frame_encode(const T &msg, std::vector<uint8_t> &out_buf) {
  using google::protobuf::io::ArrayOutputStream;
  using google::protobuf::io::CodedOutputStream;

  const auto out_payload_size = message_byte_size(msg);
  out_buf.resize(5 + out_payload_size);
  ArrayOutputStream outs(out_buf.data(), out_buf.size());
  CodedOutputStream codecouts(&outs);

  codecouts.WriteLittleEndian32(out_payload_size + 1);
  uint8_t msg_type = xproto_frame_msg_type(msg);
  codecouts.WriteRaw(&msg_type, 1);
  return msg.SerializeToCodedStream(&codecouts);
}

stdx::expected<size_t, std::error_code>
MysqlRoutingXConnection::encode_error_packet(std::vector<uint8_t> &error_frame,
                                             uint16_t error_code,
                                             const std::string &msg,
                                             const std::string &sql_state,
                                             Mysqlx::Error::Severity severity) {
  auto err_msg = Mysqlx::Error();

  err_msg.set_severity(severity);
  err_msg.set_msg(msg);
  err_msg.set_code(error_code);
  err_msg.set_sql_state(sql_state);

  return xproto_frame_encode(err_msg, error_frame);
}

void MysqlRoutingXConnection::client_con_close() {
  Mysqlx::Ok msg_ok;
  msg_ok.set_msg("bye!");
  std::vector<uint8_t> out_buf;
  xproto_frame_encode(msg_ok, out_buf);

  return async_send_client_buffer(net::buffer(out_buf),
                                  Function::kWaitClientClose);
}

void MysqlRoutingXConnection::async_run() {
  this->accepted();

  // the server's greeting if:
  //
  // passthrough + as_client
  // preferred   + as_client
  greeting_from_router_ = !(source_ssl_mode() == SslMode::kPassthrough);

  if (greeting_from_router_) {
    client_send_server_greeting_from_router();
  } else {
    server_recv_server_greeting_from_server();
  }
}

void MysqlRoutingXConnection::send_server_failed(std::error_code ec) {
#if defined(DEBUG_IO)
  std::cerr << __LINE__ << ": r->s: " << ec.message() << ", next: finish\n";
#endif

  server_socket_failed(ec);
}

void MysqlRoutingXConnection::recv_server_failed(std::error_code ec) {
#if defined(DEBUG_IO)
  std::cerr << __LINE__ << ": r<-s: " << ec.message() << ", next: finish\n";
#endif

  server_socket_failed(ec);
}

void MysqlRoutingXConnection::send_client_failed(std::error_code ec) {
#if defined(DEBUG_IO)
  std::cerr << __LINE__ << ": c<-r: " << ec.message() << ", next: finish\n";
#endif

  client_socket_failed(ec);
}

void MysqlRoutingXConnection::recv_client_failed(std::error_code ec) {
#if defined(DEBUG_IO)
  std::cerr << __LINE__ << ": c->r: " << ec.message() << ", next: finish\n";
#endif

  client_socket_failed(ec);
}

void MysqlRoutingXConnection::server_socket_failed(std::error_code ec) {
  auto &server_conn = this->server_conn();

  if (server_conn.is_open()) {
    log_connection_summary();

    if (ec != net::stream_errc::eof) {
      (void)server_conn.shutdown(net::socket_base::shutdown_send);
    }
    (void)server_conn.close();
  }

  finish();
}

void MysqlRoutingXConnection::client_socket_failed(std::error_code ec) {
  auto &client_conn = this->client_conn();

  if (client_conn.is_open()) {
    log_connection_summary();

    if (ec != net::stream_errc::eof) {
      // the other side hasn't closed yet, shutdown our send-side.
      (void)client_conn.shutdown(net::socket_base::shutdown_send);
    }
    (void)client_conn.close();
  }

  finish();
}

void MysqlRoutingXConnection::async_send_client(Function next) {
  auto &dst_channel = client_conn().channel();

  ++active_work_;
  client_conn().async_send(
      [this, next, to_transfer = dst_channel.send_buffer().size()](
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

void MysqlRoutingXConnection::async_recv_client(Function next) {
  ++active_work_;
  client_conn().async_recv(
      [this, next](std::error_code ec, size_t transferred) {
        (void)transferred;

        --active_work_;
        if (ec) return recv_client_failed(ec);

        return call_next_function(next);
      });
}

void MysqlRoutingXConnection::async_send_server(Function next) {
  auto &dst_channel = server_conn().channel();

  ++active_work_;
  server_conn().async_send(
      [this, next, to_transfer = dst_channel.send_buffer().size()](
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

void MysqlRoutingXConnection::async_recv_server(Function next) {
  ++active_work_;
  server_conn().async_recv(
      [this, next](std::error_code ec, size_t transferred) {
        (void)transferred;

        --active_work_;
        if (ec) return recv_server_failed(ec);

        return call_next_function(next);
      });
}

void MysqlRoutingXConnection::client_send_server_greeting_from_router() {
  return async_recv_client(Function::kClientRecvCmd);
}

void MysqlRoutingXConnection::client_recv_cmd() {
  auto &src_channel = client_conn().channel();
  auto &src_protocol = client_conn().protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_client(Function::kClientRecvCmd);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kConCapGet = Mysqlx::ClientMessages::CON_CAPABILITIES_GET,
    kConCapSet = Mysqlx::ClientMessages::CON_CAPABILITIES_SET,
    kConClose = Mysqlx::ClientMessages::CON_CLOSE,
    kSessAuthStart = Mysqlx::ClientMessages::SESS_AUTHENTICATE_START,
    kSessionClose = Mysqlx::ClientMessages::SESS_CLOSE,
    kSessionReset = Mysqlx::ClientMessages::SESS_RESET,
    kStmtExecute = Mysqlx::ClientMessages::SQL_STMT_EXECUTE,
    kCrudFind = Mysqlx::ClientMessages::CRUD_FIND,
    kCrudDelete = Mysqlx::ClientMessages::CRUD_DELETE,
    kCrudInsert = Mysqlx::ClientMessages::CRUD_INSERT,
    kCrudUpdate = Mysqlx::ClientMessages::CRUD_UPDATE,
    kPreparePrepare = Mysqlx::ClientMessages::PREPARE_PREPARE,
    kPrepareDeallocate = Mysqlx::ClientMessages::PREPARE_DEALLOCATE,
    kPrepareExecute = Mysqlx::ClientMessages::PREPARE_EXECUTE,
    kExpectOpen = Mysqlx::ClientMessages::EXPECT_OPEN,
    kExpectClose = Mysqlx::ClientMessages::EXPECT_CLOSE,
    kCrudCreateView = Mysqlx::ClientMessages::CRUD_CREATE_VIEW,
    kCrudModifyView = Mysqlx::ClientMessages::CRUD_MODIFY_VIEW,
    kCrudDropView = Mysqlx::ClientMessages::CRUD_DROP_VIEW,
    kCursorOpen = Mysqlx::ClientMessages::CURSOR_OPEN,
    kCursorFetch = Mysqlx::ClientMessages::CURSOR_FETCH,
    kCursorClose = Mysqlx::ClientMessages::CURSOR_CLOSE,
  };

  // we need to check if the server connection is properly initialized if the
  // message we are handling is not one from the session setup stage. This may
  // be the case if the client is not following the protocol properly.
  bool server_connection_state_ok{true};
  const auto msg = Msg{msg_type};
  switch (msg) {
    case Msg::kConCapGet:
    case Msg::kConCapSet:
    case Msg::kSessAuthStart:
      break;
    default: {
      if (!server_conn().connection()) {
        server_connection_state_ok = false;
      }
    }
  }

  if (server_connection_state_ok) {
    switch (msg) {
      case Msg::kConClose:
        return client_con_close();
      case Msg::kConCapGet:
        return client_cap_get();
      case Msg::kConCapSet:
        return client_cap_set();
      case Msg::kSessAuthStart:
        return client_sess_auth_start();
      case Msg::kSessionReset:
        return client_session_reset();
      case Msg::kSessionClose:
        return client_session_close();
      case Msg::kStmtExecute:
        return client_stmt_execute();
      case Msg::kCrudFind:
        return client_crud_find();
      case Msg::kCrudDelete:
        return client_crud_delete();
      case Msg::kCrudInsert:
        return client_crud_insert();
      case Msg::kCrudUpdate:
        return client_crud_update();
      case Msg::kPreparePrepare:
        return client_prepare_prepare();
      case Msg::kPrepareDeallocate:
        return client_prepare_deallocate();
      case Msg::kPrepareExecute:
        return client_prepare_execute();
      case Msg::kExpectOpen:
        return client_expect_open();
      case Msg::kExpectClose:
        return client_expect_close();
      case Msg::kCrudCreateView:
        return client_crud_create_view();
      case Msg::kCrudModifyView:
        return client_crud_modify_view();
      case Msg::kCrudDropView:
        return client_crud_drop_view();
      case Msg::kCursorOpen:
        return client_cursor_open();
      case Msg::kCursorFetch:
        return client_cursor_fetch();
      case Msg::kCursorClose:
        return client_cursor_close();
    }
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

/**
 * @returns frame-is-done on success and std::error_code on error.
 */
static stdx::expected<bool, std::error_code> forward_frame_from_channel(
    Channel &src_channel, XProtocolState &src_protocol, Channel &dst_channel,
    [[maybe_unused]] XProtocolState &dst_protocol) {
  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return stdx::unexpected(read_res.error());

  auto &current_frame = src_protocol.current_frame().value();

  auto &recv_buf = src_channel.recv_plain_view();
  // forward the (rest of the) payload.

  const size_t rest_of_frame_size =
      current_frame.frame_size_ - current_frame.forwarded_frame_size_;

  if (rest_of_frame_size > 0) {
    // try to full the recv-buf up to the end of the frame
    if (rest_of_frame_size > recv_buf.size()) {
      // ... not more than 16k to avoid reading all 16M at once.
      auto read_res = src_channel.read_to_plain(
          std::min(rest_of_frame_size - recv_buf.size(), size_t{16 * 1024}));
      if (!read_res) return stdx::unexpected(read_res.error());
    }

    if (recv_buf.empty()) {
      return stdx::unexpected(make_error_code(TlsErrc::kWantRead));
    }

    const auto write_res =
        dst_channel.write(net::buffer(recv_buf, rest_of_frame_size));
    if (!write_res) return stdx::unexpected(write_res.error());

    size_t transferred = write_res.value();
    current_frame.forwarded_frame_size_ += transferred;

    src_channel.consume_plain(transferred);
  }

  dst_channel.flush_to_send_buf();

  if (current_frame.forwarded_frame_size_ == current_frame.frame_size_) {
    // frame is forwarded, reset for the next one.
    src_protocol.current_frame().reset();
    src_protocol.current_msg_type().reset();

    return true;
  } else {
    return false;
  }
}

static stdx::expected<MysqlRoutingXConnection::ForwardResult, std::error_code>
forward_frame(Channel &src_channel, XProtocolState &src_protocol,
              Channel &dst_channel, XProtocolState &dst_protocol) {
  const auto forward_res = forward_frame_from_channel(
      src_channel, src_protocol, dst_channel, dst_protocol);

  if (!forward_res) {
    auto ec = forward_res.error();

    if (ec == TlsErrc::kWantRead) {
      if (!dst_channel.send_buffer().empty()) {
        return MysqlRoutingXConnection::ForwardResult::kWantSendDestination;
      }

      return MysqlRoutingXConnection::ForwardResult::kWantRecvSource;
    }

    return stdx::unexpected(forward_res.error());
  }

  const auto src_is_done = forward_res.value();
  if (!dst_channel.send_buffer().empty()) {
    if (src_is_done) {
      return MysqlRoutingXConnection::ForwardResult::kFinished;
    } else {
      return MysqlRoutingXConnection::ForwardResult::kWantSendDestination;
    }
  }

  // shouldn't happen.
  std::cerr << __LINE__ << ": " << __FUNCTION__
            << ": famous last words: should not happen." << std::endl;
  std::terminate();
}

stdx::expected<MysqlRoutingXConnection::ForwardResult, std::error_code>
MysqlRoutingXConnection::forward_frame_from_client_to_server() {
  auto &src_conn = client_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = server_conn();
  auto &dst_channel = dst_conn.channel();
  auto &dst_protocol = dst_conn.protocol();

  return forward_frame(src_channel, src_protocol, dst_channel, dst_protocol);
}

void MysqlRoutingXConnection::forward_client_to_server(Function this_func,
                                                       Function next_func) {
  auto forward_res = forward_frame_from_client_to_server();
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

stdx::expected<MysqlRoutingXConnection::ForwardResult, std::error_code>
MysqlRoutingXConnection::forward_frame_from_server_to_client() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = client_conn();
  auto &dst_channel = dst_conn.channel();
  auto &dst_protocol = dst_conn.protocol();

  return forward_frame(src_channel, src_protocol, dst_channel, dst_protocol);
}

void MysqlRoutingXConnection::forward_server_to_client(Function this_func,
                                                       Function next_func) {
  auto forward_res = forward_frame_from_server_to_client();
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
    case ForwardResult::kFinished:
      return async_send_client(next_func);
  }
}

void MysqlRoutingXConnection::connect() {
  auto &connector = this->connector();

  auto connect_res = connector.connect();
  if (!connect_res) {
    const auto ec = connect_res.error();

    // We need to keep the disconnect_request's mtx while the async handlers are
    // being set up in order not to miss the disconnect request. Otherwise we
    // could end up blocking for the whole 'destination_connect_timeout'
    // duration before giving up the connection.
    auto handled = disconnect_request([this, &connector, ec](auto requested) {
      if ((!requested) &&
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

        return true;
      }
      return false;
    });

    if (handled) return;

    // close the server side.
    this->connector().socket().close();

    if (ec == DestinationsErrc::kNoDestinations) {
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

    auto &dst_channel = client_conn().channel();

    std::vector<uint8_t> error_frame;
    const auto encode_res = encode_error_packet(
        error_frame, 2026, "connecting to backend failed", "HY000");
    if (!encode_res) {
      auto ec = encode_res.error();
      log_fatal_error_code("encoding error failed", ec);

      return send_client_failed(ec);
    }

    // send back to the client
    dst_channel.write_plain(net::buffer(error_frame));
    dst_channel.flush_to_send_buf();

    return async_send_client(Function::kFinish);
  }

  auto server_connection = std::move(connect_res.value());

  server_conn().assign_connection(std::move(server_connection));

  this->connected();

  return server_init_tls();
}

static void set_capability_tls(Mysqlx::Connection::Capability *cap,
                               bool value) {
  cap->set_name("tls");

  auto scalar = new Mysqlx::Datatypes::Scalar;
  scalar->set_v_bool(value);
  scalar->set_type(Mysqlx::Datatypes::Scalar_Type::Scalar_Type_V_BOOL);

  auto any = new Mysqlx::Datatypes::Any;
  any->set_type(Mysqlx::Datatypes::Any_Type::Any_Type_SCALAR);
  any->set_allocated_scalar(scalar);

  cap->set_allocated_value(any);
}

/**
 * client wants to get the capabilities of the server.
 *
 * send back the router's caps.
 */
void MysqlRoutingXConnection::client_cap_get() {
  auto &src_conn = client_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto res = ensure_has_full_frame(src_channel, src_protocol);
  if (!res) {
    return async_recv_client(Function::kClientCapGet);
  }

  const auto &recv_buf = src_channel.recv_plain_view();

  auto msg_payload =
      net::buffer(recv_buf, src_protocol.current_frame()->frame_size_) + 5;
  {
    auto msg = std::make_unique<Mysqlx::Connection::CapabilitiesGet>();
    if (!msg->ParseFromArray(msg_payload.data(), msg_payload.size())) {
      std::vector<uint8_t> out_buf;

      encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                          Mysqlx::Error::FATAL);

      return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
    }
  }

  if (source_ssl_mode() == SslMode::kPassthrough) {
    return forward_client_to_server(Function::kClientCapGet,
                                    Function::kServerRecvCapGetResponse);
  }

  {
    Mysqlx::Connection::Capabilities msg;

    switch (source_ssl_mode()) {
      case SslMode::kDisabled:
        break;
      case SslMode::kPreferred:
      case SslMode::kRequired:
        set_capability_tls(msg.add_capabilities(), true);
        break;
      case SslMode::kPassthrough:
      case SslMode::kDefault:
      case SslMode::kAsClient:
        // unreachable.
        std::terminate();
        break;
    }

    discard_current_msg(src_channel, src_protocol);

    std::vector<uint8_t> out_buf;

    xproto_frame_encode(msg, out_buf);

    return async_send_client_buffer(net::buffer(out_buf),
                                    Function::kClientRecvCmd);
  }
}

void MysqlRoutingXConnection::server_recv_switch_tls_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvSwitchTlsResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kError = Mysqlx::ServerMessages::ERROR,
    kOk = Mysqlx::ServerMessages::OK,
    kClassicProto = 10,
  };

  ensure_has_full_frame(src_channel, src_protocol);

  const auto &recv_buf = src_channel.recv_plain_view();

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      discard_current_msg(src_channel, src_protocol);

      return server_recv_switch_tls_response();
    case Msg::kError: {
      auto msg_payload =
          net::buffer(recv_buf, src_protocol.current_frame()->frame_size_) + 5;

      auto msg = std::make_unique<Mysqlx::Error>();
      if (!msg->ParseFromArray(msg_payload.data(), msg_payload.size())) {
        std::vector<uint8_t> out_buf;

        encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                            Mysqlx::Error::FATAL);

        return async_send_client_buffer(net::buffer(out_buf),
                                        Function::kFinish);
      }

      discard_current_msg(src_channel, src_protocol);

      switch (dest_ssl_mode()) {
        case SslMode::kPreferred:
          // enabling TLS failed, that's ok.
          return client_recv_cmd();
        case SslMode::kAsClient:
        case SslMode::kRequired: {
          // enabling TLS failed, not ok.
          //
          std::vector<uint8_t> out_buf;

          encode_error_packet(out_buf, ER_SECURE_TRANSPORT_REQUIRED,
                              "Server needs TLS", "HY000",
                              Mysqlx::Error::FATAL);

          return async_send_client_buffer(net::buffer(out_buf),
                                          Function::kFinish);
        }

        case SslMode::kDisabled:
        case SslMode::kPassthrough:
        case SslMode::kDefault:
          // unreachable.
          std::terminate();
          break;
      }
    } break;
    case Msg::kOk: {
      // server side connection succeeded.
      discard_current_msg(src_channel, src_protocol);

      return tls_connect_init();
    } break;
    case Msg::kClassicProto:
      // fallthrough for now.
      break;
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_switch_tls_response_passthrough() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(
          Function::kServerRecvSwitchTlsResponsePassthrough);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kError = Mysqlx::ServerMessages::ERROR,
    kOk = Mysqlx::ServerMessages::OK,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_switch_tls_response_passthrough_forward();
    case Msg::kError:
      return server_recv_switch_tls_response_passthrough_forward_last();
    case Msg::kOk:
      // server side connection succeeded.
      return server_recv_switch_tls_response_passthrough_forward_ok();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::
    server_recv_switch_tls_response_passthrough_forward() {
  forward_server_to_client(
      Function::kServerRecvSwitchTlsResponsePassthroughForward,
      Function::kServerRecvSwitchTlsResponsePassthrough);
}

void MysqlRoutingXConnection::
    server_recv_switch_tls_response_passthrough_forward_last() {
  forward_server_to_client(
      Function::kServerRecvSwitchTlsResponsePassthroughForwardLast,
      Function::kClientRecvCmd);
}

void MysqlRoutingXConnection::
    server_recv_switch_tls_response_passthrough_forward_ok() {
  forward_server_to_client(
      Function::kServerRecvSwitchTlsResponsePassthroughForwardOk,
      Function::kForwardTlsInit);
}

stdx::expected<void, std::error_code> MysqlRoutingXConnection::forward_tls(
    Channel &src_channel, Channel &dst_channel) {
  const auto &plain = src_channel.recv_plain_view();
  src_channel.read_to_plain(5);

  // at least the TLS record header.
  const size_t tls_header_size{5};
  while (plain.size() >= tls_header_size) {
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
    if (plain.size() < tls_header_size + tls_payload_size) {
      src_channel.read_to_plain(tls_header_size + tls_payload_size -
                                plain.size());
    }

    if (plain.size() < tls_header_size + tls_payload_size) {
      // there isn't the full frame yet.
      return stdx::unexpected(make_error_code(TlsErrc::kWantRead));
    }

    const auto write_res = dst_channel.write(
        net::buffer(plain.subspan(0, tls_header_size + tls_payload_size)));
    if (!write_res) {
      return stdx::unexpected(make_error_code(TlsErrc::kWantWrite));
    }

    // if TlsAlert in handshake, the connection goes back to plain
    if (static_cast<TlsContentType>(tls_content_type) ==
            TlsContentType::kAlert &&
        plain.size() >= 6 && plain[5] == 0x02) {
      src_channel.is_tls(false);
      dst_channel.is_tls(false);
    }

    src_channel.consume_plain(*write_res);
  }

  dst_channel.flush_to_send_buf();

  // want more
  return stdx::unexpected(make_error_code(TlsErrc::kWantRead));
}

void MysqlRoutingXConnection::forward_tls_client_to_server() {
  auto &src_channel = client_conn().channel();
  auto &dst_channel = server_conn().channel();

  auto forward_res = forward_tls(src_channel, dst_channel);

  if (!dst_channel.send_buffer().empty()) {
    return async_send_server(Function::kForwardTlsClientToServer);
  }

  if (!forward_res) {
    return async_recv_client(Function::kForwardTlsClientToServer);
  }
}

void MysqlRoutingXConnection::forward_tls_server_to_client() {
  auto &src_channel = server_conn().channel();
  auto &dst_channel = client_conn().channel();

  auto forward_res = forward_tls(src_channel, dst_channel);

  if (!dst_channel.send_buffer().empty()) {
    return async_send_client(Function::kForwardTlsServerToClient);
  }

  if (!forward_res) {
    return async_recv_server(Function::kForwardTlsServerToClient);
  }
}

void MysqlRoutingXConnection::forward_tls_init() {
  auto &src_channel = client_conn().channel();
  auto &dst_channel = server_conn().channel();

  src_channel.is_tls(true);
  dst_channel.is_tls(true);

  forward_tls_client_to_server();
  forward_tls_server_to_client();
}

static stdx::expected<TlsClientContext *, std::error_code> get_dest_ssl_ctx(
    MySQLRoutingContext &ctx, const std::string &id) {
  return mysql_harness::make_tcp_address(id).and_then(
      [&ctx, &id](const auto &addr)
          -> stdx::expected<TlsClientContext *, std::error_code> {
        return ctx.dest_ssl_ctx(id, addr.address());
      });
}

void MysqlRoutingXConnection::tls_connect_init() {
  auto &dst_channel = server_conn().channel();

  auto tls_client_ctx_res = get_dest_ssl_ctx(context(), get_destination_id());
  if (!tls_client_ctx_res || tls_client_ctx_res.value() == nullptr ||
      (*tls_client_ctx_res)->get() == nullptr) {
    // shouldn't happen. But if it does, close the connection.
    log_warning("failed to create SSL_CTX");

    return send_server_failed(make_error_code(std::errc::invalid_argument));
  }

  auto *tls_client_ctx = *tls_client_ctx_res;
  auto *ssl_ctx = tls_client_ctx->get();

  dst_channel.init_ssl(ssl_ctx);

  tls_client_ctx->get_session().and_then(
      [&](auto *sess) -> stdx::expected<void, std::error_code> {
        SSL_set_session(dst_channel.ssl(), sess);
        return {};
      });

  return tls_connect();
}

/**
 * connect server_channel to a TLS server.
 */
void MysqlRoutingXConnection::tls_connect() {
  auto &dst_channel = server_conn().channel();

  {
    const auto flush_res = dst_channel.flush_from_recv_buf();
    if (!flush_res) {
      auto ec = flush_res.error();
      log_fatal_error_code("tls_connect::recv::flush() failed", ec);

      return recv_server_failed(ec);
    }
  }

  if (!dst_channel.tls_init_is_finished()) {
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
          return async_send_server(Function::kTlsConnect);
        }
        return async_recv_server(Function::kTlsConnect);
      } else {
        // connect may fail fatally if
        //
        // - cert-verification failed.
        // - no shared cipher
        //
        std::vector<uint8_t> error_frame;

        encode_error_packet(
            error_frame, 2026,
            "connecting to destination failed with TLS error: " +
                res.error().message(),
            "HY000", Mysqlx::Error::FATAL);

        return async_send_client_buffer(net::buffer(error_frame),
                                        Function::kFinish);
      }
    }
  }

  // tls is established to the server
  return client_recv_cmd();
}

void MysqlRoutingXConnection::server_recv_cap_get_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvCapGetResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kCapabilities = Mysqlx::ServerMessages::CONN_CAPABILITIES,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_cap_get_response_forward();
    case Msg::kCapabilities:
      return server_recv_cap_get_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_cap_get_response_forward() {
  forward_server_to_client(Function::kServerRecvCapGetResponseForward,
                           Function::kServerRecvCapGetResponse);
}

void MysqlRoutingXConnection::server_recv_cap_get_response_forward_last() {
  forward_server_to_client(Function::kServerRecvCapGetResponseForwardLast,
                           Function::kClientRecvCmd);
}

void MysqlRoutingXConnection::async_send_client_buffer(
    net::const_buffer send_buf, Function next) {
  auto &dst_conn = client_conn();
  auto &dst_channel = dst_conn.channel();

  auto write_res = dst_channel.write(send_buf);
  if (!write_res) {
    auto ec = write_res.error();
    log_fatal_error_code("write() to client failed", ec);

    return send_client_failed(ec);
  }

  dst_channel.flush_to_send_buf();

  return async_send_client(next);
}

void MysqlRoutingXConnection::async_send_server_buffer(
    net::const_buffer send_buf, Function next) {
  auto &dst_conn = server_conn();
  auto &dst_channel = dst_conn.channel();

  auto write_res = dst_channel.write(send_buf);
  if (!write_res) {
    auto ec = write_res.error();
    log_fatal_error_code("write() to server failed", ec);

    return send_server_failed(ec);
  }

  dst_channel.flush_to_send_buf();

  return async_send_server(next);
}

/**
 * client wants to set the capabilities.
 *
 * send back the router's caps.
 */
void MysqlRoutingXConnection::client_cap_set() {
  auto &src_conn = client_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = server_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto res = ensure_has_full_frame(src_channel, src_protocol);
  if (!res) {
    return async_recv_client(Function::kClientCapSet);
  }

  const auto &recv_buf = src_channel.recv_plain_view();

  auto msg_payload =
      net::buffer(recv_buf, src_protocol.current_frame()->frame_size_) + 5;

  auto msg = std::make_unique<Mysqlx::Connection::CapabilitiesSet>();
  if (!msg->ParseFromArray(msg_payload.data(), msg_payload.size())) {
    std::vector<uint8_t> out_buf;

    encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                        Mysqlx::Error::FATAL);

    return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
  }

  bool msg_is_broken{false};
  bool switch_to_tls{false};
  bool has_cap_compression{false};
  if (!msg->has_capabilities()) {
    msg_is_broken = true;
  } else {
    for (auto const &cap : msg->capabilities().capabilities()) {
      if (!(cap.has_name() && cap.has_value() && cap.value().has_type())) {
        msg_is_broken = true;
        break;
      }

      if (cap.name() == "tls") {
        if (!(cap.value().type() == Mysqlx::Datatypes::Any_Type_SCALAR &&
              cap.value().has_scalar() && cap.value().scalar().has_type() &&
              cap.value().scalar().type() ==
                  Mysqlx::Datatypes::Scalar::V_BOOL)) {
          msg_is_broken = true;
          break;
        } else {
          switch_to_tls = cap.value().scalar().v_bool();
        }
      } else if (cap.name() == "compression") {
        has_cap_compression = true;
      } else {
#ifdef DEBUG_IO
        std::cerr << __LINE__ << ": " << cap.name() << "\n";
#endif
        // not "tls"
      }
    }
  }

  if (msg_is_broken) {
    discard_current_msg(src_channel, src_protocol);

    std::vector<uint8_t> out_buf;

    encode_error_packet(out_buf, 5001, "Capability prepare failed for \'tls\'",
                        "HY000", Mysqlx::Error::ERROR);

    return async_send_client_buffer(net::buffer(out_buf),
                                    Function::kClientRecvCmd);
  }

  if (has_cap_compression) {
    discard_current_msg(src_channel, src_protocol);

    std::vector<uint8_t> out_buf;

    encode_error_packet(
        out_buf, ER_X_CAPABILITY_COMPRESSION_INVALID_ALGORITHM,
        "Invalid or unsupported value for \'compression.algorithm\'", "HY000",
        Mysqlx::Error::ERROR);

    return async_send_client_buffer(net::buffer(out_buf),
                                    Function::kClientRecvCmd);
  }

  if (switch_to_tls) {
    bool continue_with_tls{false};
    switch (source_ssl_mode()) {
      case SslMode::kDisabled: {
        continue_with_tls = false;
        break;
      }
      case SslMode::kRequired: {
        continue_with_tls = true;
        break;
      }
      case SslMode::kPreferred: {
        switch (dest_ssl_mode()) {
          case SslMode::kAsClient:
            if (!server_conn().is_open()) {
              // leave the client message in place and connect to the backend.
              //
              // connect() will eventually call this function again and the same
              // message will be processed in the 2nd round.
              return connect();
            }

            // check if the server supports Tls
            for (auto const &caps : dst_protocol.caps()->capabilities()) {
              if (!caps.has_name()) continue;
#ifdef DEBUG_IO
              std::cerr << __LINE__ << ": " << caps.name() << "\n";
#endif

              if (caps.name() == "tls") {
                continue_with_tls = true;
                break;
              }
            }
            break;

          default:
            continue_with_tls = true;
            break;
        }
        break;
      }
      case SslMode::kPassthrough:
        return forward_client_to_server(
            Function::kClientCapSet,
            Function::kServerRecvSwitchTlsResponsePassthrough);
      case SslMode::kDefault:
      case SslMode::kAsClient:
        // unreachable.
        std::terminate();
        return;
    }

    discard_current_msg(src_channel, src_protocol);
    std::vector<uint8_t> out_buf;

    if (!continue_with_tls) {
#ifdef DEBUG_IO
      std::cerr << __LINE__ << ": "
                << "no tls cap"
                << "\n";
#endif
      encode_error_packet(out_buf, 5001,
                          "Capability prepare failed for \'tls\'", "HY000",
                          Mysqlx::Error::ERROR);

      return async_send_client_buffer(net::buffer(out_buf),
                                      Function::kClientRecvCmd);
    }

    xproto_frame_encode(Mysqlx::Ok{}, out_buf);

    return async_send_client_buffer(net::buffer(out_buf),
                                    Function::kTlsAcceptInit);
  } else {
    discard_current_msg(src_channel, src_protocol);

    std::vector<uint8_t> out_buf;

    xproto_frame_encode(Mysqlx::Ok{}, out_buf);

    return async_send_client_buffer(net::buffer(out_buf),
                                    Function::kClientRecvCmd);
  }
}

void MysqlRoutingXConnection::tls_accept_init() {
  auto &src_channel = client_conn().channel();

  src_channel.is_tls(true);

  auto *ssl_ctx = context().source_ssl_ctx()->get();
  // tls <-> (any)
  if (ssl_ctx == nullptr) {
    // shouldn't happen. But if it does, close the connection.
    log_warning("failed to create SSL_CTX");
    return recv_client_failed(make_error_code(std::errc::invalid_argument));
  }
  src_channel.init_ssl(ssl_ctx);

  return tls_accept();
}

/**
 * accept a TLS handshake.
 */
void MysqlRoutingXConnection::tls_accept() {
  auto &src_channel = client_conn().channel();

  if (!src_channel.tls_init_is_finished()) {
    {
      const auto flush_res = src_channel.flush_from_recv_buf();
      if (!flush_res) return recv_client_failed(flush_res.error());
    }

    auto res = src_channel.tls_accept();

    // flush the TLS message to the send-buffer.
    {
      const auto flush_res = src_channel.flush_to_send_buf();
      if (!flush_res) {
        const auto ec = flush_res.error();
        if (ec != make_error_code(std::errc::operation_would_block)) {
          return recv_client_failed(flush_res.error());
        }
      }
    }

    if (!res) {
      const auto ec = res.error();

      // if there is something in the send_buffer, send it.
      if (!src_channel.send_buffer().empty()) {
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
  if (!src_channel.send_buffer().empty()) {
    return async_send_client(Function::kTlsAcceptFinalize);
  }
  // TLS is accepted, more client greeting should follow.

  return tls_accept_finalize();
}

void MysqlRoutingXConnection::tls_accept_finalize() {
  auto &src_channel = client_conn().channel();

  if (!server_conn().is_open()) {
    return connect();
  } else if (source_ssl_mode() == SslMode::kPreferred &&
             dest_ssl_mode() == SslMode::kAsClient && src_channel.ssl()) {
    // pre-conditions.
    if (!server_conn().is_open()) {
      throw std::logic_error("server-conn is not opened, but should be.");
    }
    if (server_conn().channel().ssl()) {
      throw std::logic_error(
          "server-conn is already with TLS, but should not be.");
    }

    return server_init_tls();
  } else {
    return client_recv_cmd();
  }
}

void MysqlRoutingXConnection::server_init_tls() {
  auto &src_channel = client_conn().channel();
  auto &dst_channel = server_conn().channel();

  switch (dest_ssl_mode()) {
    case SslMode::kAsClient:
      switch (source_ssl_mode()) {
        case SslMode::kPreferred:
          // called twice:
          //
          // 1. at server-side connect().
          // 2. by tls_accept_finalize to open the server-side TLS connection
          // after the client asked to enable the client side.
          if (src_channel.ssl()) {
            return server_send_switch_to_tls();
          } else {
            return server_send_check_caps();
          }
        case SslMode::kPassthrough:
        case SslMode::kDisabled:
          // nothing to do.
          return client_recv_cmd();
        case SslMode::kRequired:
          if (!dst_channel.ssl()) {
            return server_send_switch_to_tls();
          } else {
            return client_recv_cmd();
          }
        default:
          std::cerr << __LINE__ << ": expected dest-ssl-mode: "
                    << static_cast<int>(dest_ssl_mode()) << "\n";
          std::terminate();
          break;
      }
    case SslMode::kRequired:
    case SslMode::kPreferred:
      if (!dst_channel.ssl()) {
        return server_send_switch_to_tls();
      } else {
        return client_recv_cmd();
      }
    case SslMode::kDisabled:
      // nothing to do, back to the client.
      return client_recv_cmd();
    case SslMode::kPassthrough:
    case SslMode::kDefault:
      std::cerr << __LINE__ << ": expected dest-ssl-mode: "
                << static_cast<int>(dest_ssl_mode()) << "\n";
      std::terminate();
      break;
  }

  std::terminate();
}

void MysqlRoutingXConnection::server_send_switch_to_tls() {
  std::vector<uint8_t> out_buf;

  {
    Mysqlx::Connection::CapabilitiesSet msg;

    set_capability_tls(msg.mutable_capabilities()->add_capabilities(), true);

    xproto_frame_encode(msg, out_buf);
  }

  return async_send_server_buffer(net::buffer(out_buf),
                                  Function::kServerRecvSwitchTlsResponse);
}

void MysqlRoutingXConnection::server_send_check_caps() {
  std::vector<uint8_t> out_buf;

  {
    Mysqlx::Connection::CapabilitiesGet msg;

    xproto_frame_encode(msg, out_buf);
  }

  return async_send_server_buffer(net::buffer(out_buf),
                                  Function::kServerRecvCheckCapsResponse);
}

void MysqlRoutingXConnection::server_recv_check_caps_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvCheckCapsResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kCaps = Mysqlx::ServerMessages::CONN_CAPABILITIES,
  };

  ensure_has_full_frame(src_channel, src_protocol);

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      discard_current_msg(src_channel, src_protocol);

      return server_recv_check_caps_response();
    case Msg::kCaps: {
      const auto &recv_buf = src_channel.recv_plain_view();

      auto msg_payload =
          net::buffer(recv_buf, src_protocol.current_frame()->frame_size_) + 5;
      {
        auto msg = std::make_unique<Mysqlx::Connection::Capabilities>();
        if (!msg->ParseFromArray(msg_payload.data(), msg_payload.size())) {
          std::vector<uint8_t> out_buf;

          encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                              Mysqlx::Error::FATAL);

          return async_send_client_buffer(net::buffer(out_buf),
                                          Function::kFinish);
        }

        src_protocol.caps(std::move(msg));
      }

      discard_current_msg(src_channel, src_protocol);

      return client_recv_cmd();
    }
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_cap_set_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvCapSetResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kOk = Mysqlx::ServerMessages::OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_cap_set_response_forward();
    case Msg::kError:
    case Msg::kOk:
      return server_recv_cap_set_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_cap_set_response_forward() {
  forward_server_to_client(Function::kServerRecvCapSetResponseForward,
                           Function::kServerRecvCapSetResponse);
}

void MysqlRoutingXConnection::server_recv_cap_set_response_forward_last() {
  forward_server_to_client(Function::kServerRecvCapSetResponseForwardLast,
                           Function::kClientRecvCmd);
}

// session auth start

void MysqlRoutingXConnection::client_sess_auth_start() {
  auto &src_channel = client_conn().channel();

  // require TLS before authentication is started.
  if (source_ssl_mode() == SslMode::kRequired && !src_channel.ssl()) {
    std::vector<uint8_t> out_buf;

    encode_error_packet(out_buf, 5001, "Client requires TLS", "HY000",
                        Mysqlx::Error::FATAL);

    return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
  }

  if (!server_conn().is_open()) {
    // leave the client message in place and connect to the backend.
    return connect();
  }

  forward_client_to_server(Function::kClientSessAuthStart,
                           Function::kServerRecvAuthResponse);
}

void MysqlRoutingXConnection::server_recv_auth_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvAuthResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kAuthContinue = Mysqlx::ServerMessages::SESS_AUTHENTICATE_CONTINUE,
    kAuthOk = Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_auth_response_forward();
    case Msg::kAuthContinue:
      return server_recv_auth_response_continue();
    case Msg::kError:
    case Msg::kAuthOk:
      return server_recv_auth_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_auth_response_forward() {
  forward_server_to_client(Function::kServerRecvAuthResponseForward,
                           Function::kServerRecvAuthResponse);
}

void MysqlRoutingXConnection::server_recv_auth_response_continue() {
  forward_server_to_client(Function::kServerRecvAuthResponseContinue,
                           Function::kClientRecvAuthContinue);
}

void MysqlRoutingXConnection::client_recv_auth_continue() {
  forward_client_to_server(Function::kClientRecvAuthContinue,
                           Function::kServerRecvAuthResponse);
}

void MysqlRoutingXConnection::server_recv_auth_response_forward_last() {
  forward_server_to_client(Function::kServerRecvAuthResponseForwardLast,
                           Function::kClientRecvCmd);
}

// stmt execute

void MysqlRoutingXConnection::client_stmt_execute() {
  forward_client_to_server(Function::kClientStmtExecute,
                           Function::kServerRecvStmtExecuteResponse);
}

void MysqlRoutingXConnection::server_recv_stmt_execute_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvStmtExecuteResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kColumnMeta = Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA,
    kRow = Mysqlx::ServerMessages::RESULTSET_ROW,
    kFetchDone = Mysqlx::ServerMessages::RESULTSET_FETCH_DONE,
    kStmtOk = Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kRow:
    case Msg::kColumnMeta:
    case Msg::kNotice:
    case Msg::kFetchDone:
      return server_recv_stmt_execute_response_forward();
    case Msg::kError:
    case Msg::kStmtOk:
      return server_recv_stmt_execute_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_stmt_execute_response_forward() {
  forward_server_to_client(Function::kServerRecvStmtExecuteResponseForward,
                           Function::kServerRecvStmtExecuteResponse);
}

void MysqlRoutingXConnection::server_recv_stmt_execute_response_forward_last() {
  forward_server_to_client(Function::kServerRecvStmtExecuteResponseForwardLast,
                           Function::kClientRecvCmd);
}

// crud::find

void MysqlRoutingXConnection::client_crud_find() {
  forward_client_to_server(Function::kClientCrudFind,
                           Function::kServerRecvCrudFindResponse);
}

void MysqlRoutingXConnection::server_recv_crud_find_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvCrudFindResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kColumnMeta = Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA,
    kRow = Mysqlx::ServerMessages::RESULTSET_ROW,
    kFetchDone = Mysqlx::ServerMessages::RESULTSET_FETCH_DONE,
    kStmtOk = Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kRow:
    case Msg::kColumnMeta:
    case Msg::kNotice:
    case Msg::kFetchDone:
      return server_recv_crud_find_response_forward();
    case Msg::kError:
    case Msg::kStmtOk:
      return server_recv_crud_find_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_crud_find_response_forward() {
  forward_server_to_client(Function::kServerRecvCrudFindResponseForward,
                           Function::kServerRecvCrudFindResponse);
}

void MysqlRoutingXConnection::server_recv_crud_find_response_forward_last() {
  forward_server_to_client(Function::kServerRecvCrudFindResponseForwardLast,
                           Function::kClientRecvCmd);
}

// crud::delete

void MysqlRoutingXConnection::client_crud_delete() {
  forward_client_to_server(Function::kClientCrudDelete,
                           Function::kServerRecvCrudDeleteResponse);
}

void MysqlRoutingXConnection::server_recv_crud_delete_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvCrudDeleteResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kStmtOk = Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK,
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_crud_delete_response_forward();
    case Msg::kStmtOk:
    case Msg::kError:
      return server_recv_crud_delete_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_crud_delete_response_forward() {
  forward_server_to_client(Function::kServerRecvCrudDeleteResponseForward,
                           Function::kServerRecvCrudDeleteResponse);
}

void MysqlRoutingXConnection::server_recv_crud_delete_response_forward_last() {
  forward_server_to_client(Function::kServerRecvCrudDeleteResponseForwardLast,
                           Function::kClientRecvCmd);
}

// crud::insert

void MysqlRoutingXConnection::client_crud_insert() {
  forward_client_to_server(Function::kClientCrudInsert,
                           Function::kServerRecvCrudInsertResponse);
}

void MysqlRoutingXConnection::server_recv_crud_insert_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvCrudInsertResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kStmtOk = Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK,
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_crud_insert_response_forward();
    case Msg::kStmtOk:
    case Msg::kError:
      return server_recv_crud_insert_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_crud_insert_response_forward() {
  forward_server_to_client(Function::kServerRecvCrudInsertResponseForward,
                           Function::kServerRecvCrudInsertResponse);
}

void MysqlRoutingXConnection::server_recv_crud_insert_response_forward_last() {
  forward_server_to_client(Function::kServerRecvCrudInsertResponseForwardLast,
                           Function::kClientRecvCmd);
}

// crud::update

void MysqlRoutingXConnection::client_crud_update() {
  forward_client_to_server(Function::kClientCrudUpdate,
                           Function::kServerRecvCrudUpdateResponse);
}

void MysqlRoutingXConnection::server_recv_crud_update_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvCrudUpdateResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kStmtOk = Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK,
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_crud_update_response_forward();
    case Msg::kStmtOk:
    case Msg::kError:
      return server_recv_crud_update_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_crud_update_response_forward() {
  forward_server_to_client(Function::kServerRecvCrudUpdateResponseForward,
                           Function::kServerRecvCrudUpdateResponse);
}

void MysqlRoutingXConnection::server_recv_crud_update_response_forward_last() {
  forward_server_to_client(Function::kServerRecvCrudUpdateResponseForwardLast,
                           Function::kClientRecvCmd);
}

// prepare::prepare

void MysqlRoutingXConnection::client_prepare_prepare() {
  forward_client_to_server(Function::kClientPreparePrepare,
                           Function::kServerRecvPreparePrepareResponse);
}

void MysqlRoutingXConnection::server_recv_prepare_prepare_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvPreparePrepareResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kOk = Mysqlx::ServerMessages::OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_prepare_prepare_response_forward();
    case Msg::kError:
    case Msg::kOk:
      return server_recv_prepare_prepare_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_prepare_prepare_response_forward() {
  forward_server_to_client(Function::kServerRecvPreparePrepareResponseForward,
                           Function::kServerRecvPreparePrepareResponse);
}

void MysqlRoutingXConnection::
    server_recv_prepare_prepare_response_forward_last() {
  forward_server_to_client(
      Function::kServerRecvPreparePrepareResponseForwardLast,
      Function::kClientRecvCmd);
}

// prepare::deallocate

void MysqlRoutingXConnection::client_prepare_deallocate() {
  forward_client_to_server(Function::kClientPrepareDeallocate,
                           Function::kServerRecvPrepareDeallocateResponse);
}

void MysqlRoutingXConnection::server_recv_prepare_deallocate_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvPrepareDeallocateResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kOk = Mysqlx::ServerMessages::OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_prepare_deallocate_response_forward();
    case Msg::kError:
    case Msg::kOk:
      return server_recv_prepare_deallocate_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::
    server_recv_prepare_deallocate_response_forward() {
  forward_server_to_client(
      Function::kServerRecvPrepareDeallocateResponseForward,
      Function::kServerRecvPrepareDeallocateResponse);
}

void MysqlRoutingXConnection::
    server_recv_prepare_deallocate_response_forward_last() {
  forward_server_to_client(
      Function::kServerRecvPrepareDeallocateResponseForwardLast,
      Function::kClientRecvCmd);
}

// prepare::execute

void MysqlRoutingXConnection::client_prepare_execute() {
  forward_client_to_server(Function::kClientPrepareExecute,
                           Function::kServerRecvPrepareExecuteResponse);
}

void MysqlRoutingXConnection::server_recv_prepare_execute_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvPrepareExecuteResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kColumnMeta = Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA,
    kRow = Mysqlx::ServerMessages::RESULTSET_ROW,
    kFetchDone = Mysqlx::ServerMessages::RESULTSET_FETCH_DONE,
    kStmtOk = Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kRow:
    case Msg::kColumnMeta:
    case Msg::kNotice:
    case Msg::kFetchDone:
      return server_recv_prepare_execute_response_forward();
    case Msg::kError:
    case Msg::kStmtOk:
      return server_recv_prepare_execute_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_prepare_execute_response_forward() {
  forward_server_to_client(Function::kServerRecvPrepareExecuteResponseForward,
                           Function::kServerRecvPrepareExecuteResponse);
}

void MysqlRoutingXConnection::
    server_recv_prepare_execute_response_forward_last() {
  forward_server_to_client(
      Function::kServerRecvPrepareExecuteResponseForwardLast,
      Function::kClientRecvCmd);
}

// expect::open

void MysqlRoutingXConnection::client_expect_open() {
  forward_client_to_server(Function::kClientExpectOpen,
                           Function::kServerRecvExpectOpenResponse);
}

void MysqlRoutingXConnection::server_recv_expect_open_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvExpectOpenResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kOk = Mysqlx::ServerMessages::OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_expect_open_response_forward();
    case Msg::kError:
    case Msg::kOk:
      return server_recv_expect_open_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_expect_open_response_forward() {
  forward_server_to_client(Function::kServerRecvExpectOpenResponseForward,
                           Function::kServerRecvExpectOpenResponse);
}

void MysqlRoutingXConnection::server_recv_expect_open_response_forward_last() {
  forward_server_to_client(Function::kServerRecvExpectOpenResponseForwardLast,
                           Function::kClientRecvCmd);
}

// expect::close

void MysqlRoutingXConnection::client_expect_close() {
  forward_client_to_server(Function::kClientExpectClose,
                           Function::kServerRecvExpectCloseResponse);
}

void MysqlRoutingXConnection::server_recv_expect_close_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvExpectCloseResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kOk = Mysqlx::ServerMessages::OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_expect_close_response_forward();
    case Msg::kError:
    case Msg::kOk:
      return server_recv_expect_close_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_expect_close_response_forward() {
  forward_server_to_client(Function::kServerRecvExpectCloseResponseForward,
                           Function::kServerRecvExpectCloseResponse);
}

void MysqlRoutingXConnection::server_recv_expect_close_response_forward_last() {
  forward_server_to_client(Function::kServerRecvExpectCloseResponseForwardLast,
                           Function::kClientRecvCmd);
}

// crud::create_view

void MysqlRoutingXConnection::client_crud_create_view() {
  forward_client_to_server(Function::kClientCrudCreateView,
                           Function::kServerRecvCrudCreateViewResponse);
}

void MysqlRoutingXConnection::server_recv_crud_create_view_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvCrudCreateViewResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kOk = Mysqlx::ServerMessages::OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_crud_create_view_response_forward();
    case Msg::kError:
    case Msg::kOk:
      return server_recv_crud_create_view_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_crud_create_view_response_forward() {
  forward_server_to_client(Function::kServerRecvCrudCreateViewResponseForward,
                           Function::kServerRecvCrudCreateViewResponse);
}

void MysqlRoutingXConnection::
    server_recv_crud_create_view_response_forward_last() {
  forward_server_to_client(
      Function::kServerRecvCrudCreateViewResponseForwardLast,
      Function::kClientRecvCmd);
}

// crud::modify_view

void MysqlRoutingXConnection::client_crud_modify_view() {
  forward_client_to_server(Function::kClientCrudModifyView,
                           Function::kServerRecvCrudModifyViewResponse);
}

void MysqlRoutingXConnection::server_recv_crud_modify_view_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvCrudModifyViewResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kOk = Mysqlx::ServerMessages::OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_crud_modify_view_response_forward();
    case Msg::kError:
    case Msg::kOk:
      return server_recv_crud_modify_view_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_crud_modify_view_response_forward() {
  forward_server_to_client(Function::kServerRecvCrudModifyViewResponseForward,
                           Function::kServerRecvCrudModifyViewResponse);
}

void MysqlRoutingXConnection::
    server_recv_crud_modify_view_response_forward_last() {
  forward_server_to_client(
      Function::kServerRecvCrudModifyViewResponseForwardLast,
      Function::kClientRecvCmd);
}

// crud::drop_view

void MysqlRoutingXConnection::client_crud_drop_view() {
  forward_client_to_server(Function::kClientCrudDropView,
                           Function::kServerRecvCrudDropViewResponse);
}

void MysqlRoutingXConnection::server_recv_crud_drop_view_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvCrudDropViewResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kOk = Mysqlx::ServerMessages::OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_crud_drop_view_response_forward();
    case Msg::kError:
    case Msg::kOk:
      return server_recv_crud_drop_view_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_crud_drop_view_response_forward() {
  forward_server_to_client(Function::kServerRecvCrudDropViewResponseForward,
                           Function::kServerRecvCrudDropViewResponse);
}

void MysqlRoutingXConnection::
    server_recv_crud_drop_view_response_forward_last() {
  forward_server_to_client(Function::kServerRecvCrudDropViewResponseForwardLast,
                           Function::kClientRecvCmd);
}

// cursor::open

void MysqlRoutingXConnection::client_cursor_open() {
  forward_client_to_server(Function::kClientCursorOpen,
                           Function::kServerRecvCursorOpenResponse);
}

void MysqlRoutingXConnection::server_recv_cursor_open_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvCursorOpenResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kColumnMeta = Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA,
    kFetchSuspended = Mysqlx::ServerMessages::RESULTSET_FETCH_SUSPENDED,
    kStmtOk = Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
    case Msg::kColumnMeta:
    case Msg::kFetchSuspended:
      return server_recv_cursor_open_response_forward();
    case Msg::kError:
    case Msg::kStmtOk:
      return server_recv_cursor_open_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_cursor_open_response_forward() {
  forward_server_to_client(Function::kServerRecvCursorOpenResponseForward,
                           Function::kServerRecvCursorOpenResponse);
}

void MysqlRoutingXConnection::server_recv_cursor_open_response_forward_last() {
  forward_server_to_client(Function::kServerRecvCursorOpenResponseForwardLast,
                           Function::kClientRecvCmd);
}

// cursor::fetch

void MysqlRoutingXConnection::client_cursor_fetch() {
  forward_client_to_server(Function::kClientCursorFetch,
                           Function::kServerRecvCursorFetchResponse);
}

void MysqlRoutingXConnection::server_recv_cursor_fetch_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvCursorFetchResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kFetchSuspended = Mysqlx::ServerMessages::RESULTSET_FETCH_SUSPENDED,
    kFetchDone = Mysqlx::ServerMessages::RESULTSET_FETCH_DONE,
    kRow = Mysqlx::ServerMessages::RESULTSET_ROW,
    kStmtOk = Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
    case Msg::kRow:
    case Msg::kFetchSuspended:
    case Msg::kFetchDone:
      return server_recv_cursor_fetch_response_forward();
    case Msg::kStmtOk:
    case Msg::kError:
      return server_recv_cursor_fetch_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_cursor_fetch_response_forward() {
  forward_server_to_client(Function::kServerRecvCursorFetchResponseForward,
                           Function::kServerRecvCursorFetchResponse);
}

void MysqlRoutingXConnection::server_recv_cursor_fetch_response_forward_last() {
  forward_server_to_client(Function::kServerRecvCursorFetchResponseForwardLast,
                           Function::kClientRecvCmd);
}

// cursor::close

void MysqlRoutingXConnection::client_cursor_close() {
  forward_client_to_server(Function::kClientCursorClose,
                           Function::kServerRecvCursorCloseResponse);
}

void MysqlRoutingXConnection::server_recv_cursor_close_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvCursorCloseResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kOk = Mysqlx::ServerMessages::OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_cursor_close_response_forward();
    case Msg::kError:
    case Msg::kOk:
      return server_recv_cursor_close_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_cursor_close_response_forward() {
  forward_server_to_client(Function::kServerRecvCursorCloseResponseForward,
                           Function::kServerRecvCursorCloseResponse);
}

void MysqlRoutingXConnection::server_recv_cursor_close_response_forward_last() {
  forward_server_to_client(Function::kServerRecvCursorCloseResponseForwardLast,
                           Function::kClientRecvCmd);
}

// session::close

void MysqlRoutingXConnection::client_session_close() {
  forward_client_to_server(Function::kClientSessionClose,
                           Function::kServerRecvSessionCloseResponse);
}

void MysqlRoutingXConnection::server_recv_session_close_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvSessionCloseResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kOk = Mysqlx::ServerMessages::OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_session_close_response_forward();
    case Msg::kError:
    case Msg::kOk:
      return server_recv_session_close_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_session_close_response_forward() {
  forward_server_to_client(Function::kServerRecvSessionCloseResponseForward,
                           Function::kServerRecvSessionCloseResponse);
}

void MysqlRoutingXConnection::
    server_recv_session_close_response_forward_last() {
  forward_server_to_client(Function::kServerRecvSessionCloseResponseForwardLast,
                           Function::kClientRecvCmd);
}

// session::reset

void MysqlRoutingXConnection::client_session_reset() {
  forward_client_to_server(Function::kClientSessionReset,
                           Function::kServerRecvSessionResetResponse);
}

void MysqlRoutingXConnection::server_recv_session_reset_response() {
  auto &src_conn = server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) {
    auto ec = read_res.error();

    if (ec == TlsErrc::kWantRead) {
      return async_recv_server(Function::kServerRecvSessionResetResponse);
    }

    return recv_server_failed(ec);
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    kNotice = Mysqlx::ServerMessages::NOTICE,
    kOk = Mysqlx::ServerMessages::OK,
    kError = Mysqlx::ServerMessages::ERROR,
  };

  switch (Msg{msg_type}) {
    case Msg::kNotice:
      return server_recv_session_reset_response_forward();
    case Msg::kError:
    case Msg::kOk:
      return server_recv_session_reset_response_forward_last();
  }

  {
    ensure_has_full_frame(src_channel, src_protocol);

    auto &recv_buf = src_channel.recv_plain_view();
    log_debug("%s: %s", __FUNCTION__, hexify(recv_buf).c_str());
  }

  std::vector<uint8_t> out_buf;
  encode_error_packet(out_buf, ER_X_BAD_MESSAGE, "Bad Message", "HY000",
                      Mysqlx::Error::FATAL);

  return async_send_client_buffer(net::buffer(out_buf), Function::kFinish);
}

void MysqlRoutingXConnection::server_recv_session_reset_response_forward() {
  forward_server_to_client(Function::kServerRecvSessionResetResponseForward,
                           Function::kServerRecvSessionResetResponse);
}

void MysqlRoutingXConnection::
    server_recv_session_reset_response_forward_last() {
  forward_server_to_client(Function::kServerRecvSessionResetResponseForwardLast,
                           Function::kClientRecvCmd);
}

// get server greeting

void MysqlRoutingXConnection::server_recv_server_greeting_from_server() {
  return connect();
}

void MysqlRoutingXConnection::finish() {
  auto &client_socket = client_conn();
  auto &server_socket = server_conn();

  if (server_socket.is_open() && !client_socket.is_open()) {
#if 0
    if (!client_greeting_sent_) {
      // client hasn't sent a greeting to the server. The server would track
      // this as "connection error" and block the router. Better send our own
      // client-greeting.
      client_greeting_sent_ = true;
      return server_side_client_greeting();
    } else {
#endif
    // if the server is waiting on something, as client is already gone.
    (void)server_socket.cancel();
  } else if (!server_socket.is_open() && client_socket.is_open()) {
    // if the client is waiting on something, as server is already gone.
    (void)client_socket.cancel();
  }
  if (active_work_ == 0) {
    if (server_socket.is_open()) {
      server_tls_shutdown();
      (void)server_socket.shutdown(net::socket_base::shutdown_send);
      (void)server_socket.close();
    }
    if (client_socket.is_open()) {
      client_tls_shutdown();
      (void)client_socket.shutdown(net::socket_base::shutdown_send);
      (void)client_socket.close();
    }

    done();
  }
}

void MysqlRoutingXConnection::wait_client_close() { finish(); }

// final state.
//
// removes the connection from the connection-container.
void MysqlRoutingXConnection::done() { this->disassociate(); }

void MysqlRoutingXConnection::server_tls_shutdown() {
  auto &channel = server_conn().channel();
  if (channel.ssl()) {
    (void)channel.tls_shutdown();
  }
}

void MysqlRoutingXConnection::client_tls_shutdown() {
  auto &channel = client_conn().channel();
  if (channel.ssl()) {
    (void)channel.tls_shutdown();
  }
}
