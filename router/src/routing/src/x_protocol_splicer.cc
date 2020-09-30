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

#include "x_protocol_splicer.h"

#include <vector>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/message_lite.h>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/tls_error.h"
#include "mysqlx.pb.h"
#include "mysqlx_connection.pb.h"
#include "mysqlx_datatypes.pb.h"
#include "mysqlx_notice.pb.h"
#include "mysqlx_session.pb.h"

IMPORT_LOG_FUNCTIONS()

/**
 * hexdump into a string.
 */
template <class T>
static std::string dump(const T &plain_buf) {
  std::string out;
  size_t i{};
  for (auto const &c : plain_buf) {
    std::array<char, 3> hexchar{};
    snprintf(hexchar.data(), hexchar.size(), "%02x", c);

    out.append(hexchar.data());

    ++i;
    if (i >= 16) {
      i = 0;
      out.append("\n");
    } else {
      out.append(" ");
    }
  }
  if (i != 0) out += "\n";

  return out;
}

/**
 * log error-msg with error code and set the connection to its FINISH state.
 */
static BasicSplicer::State log_fatal_error_code(const char *msg,
                                                std::error_code ec) {
  log_warning("%s: %s (%s:%d)", msg, ec.message().c_str(), ec.category().name(),
              ec.value());

  return BasicSplicer::State::FINISH;
}

// map a c++-type to a msg-type
template <class T>
uint8_t xproto_frame_msg_type();

template <>
constexpr uint8_t xproto_frame_msg_type<Mysqlx::Error>() {
  return Mysqlx::ServerMessages::ERROR;
}

template <>
constexpr uint8_t xproto_frame_msg_type<Mysqlx::Ok>() {
  return Mysqlx::ServerMessages::OK;
}

template <>
constexpr uint8_t xproto_frame_msg_type<Mysqlx::Connection::Capabilities>() {
  return Mysqlx::ServerMessages::CONN_CAPABILITIES;
}

template <>
constexpr uint8_t xproto_frame_msg_type<Mysqlx::Connection::CapabilitiesSet>() {
  return Mysqlx::ClientMessages::CON_CAPABILITIES_SET;
}

template <>
constexpr uint8_t xproto_frame_msg_type<Mysqlx::Connection::CapabilitiesGet>() {
  return Mysqlx::ClientMessages::CON_CAPABILITIES_GET;
}

static size_t message_byte_size(const google::protobuf::MessageLite &msg) {
#if (defined(GOOGLE_PROTOBUF_VERSION) && GOOGLE_PROTOBUF_VERSION > 3000000)
  return msg.ByteSizeLong();
#else
  return msg.ByteSize();
#endif
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
  uint8_t msg_type = xproto_frame_msg_type<T>();
  codecouts.WriteRaw(&msg_type, 1);
  return msg.SerializeToCodedStream(&codecouts);
}

#if 0
// requires protobuf 3.x
//
// only used if debugging is enabled.
static const char *xproto_client_message_to_string(uint8_t message_type) {
  return Mysqlx::ClientMessages_Type_Name(message_type).c_str();
}

static const char *xproto_server_message_to_string(uint8_t message_type) {
  return Mysqlx::ServerMessages_Type_Name(message_type).c_str();
}
#endif

BasicSplicer::State XProtocolSplicer::tls_client_greeting() {
#if 0
  log_debug("%d: >> %s", __LINE__, state_to_string(state()));
#endif
  if (source_ssl_mode() == SslMode::kPreferred &&
      dest_ssl_mode() == SslMode::kAsClient) {
    // client-side as an established TLS session and server-side is expecting a
    // Tls Client Hello now.
    server_channel()->is_tls(true);
    server_channel()->init_ssl(server_ssl_ctx_getter_());

    return State::TLS_CONNECT;
  } else if (dest_ssl_mode() != SslMode::kDisabled) {
    // remember that we tried at least once to initiate a server side TLS
    // connection.
    tls_handshake_tried_ = true;

    // try to enable TLS
    Mysqlx::Connection::CapabilitiesSet msg;

    auto cap = msg.mutable_capabilities()->add_capabilities();
    cap->set_name("tls");

    auto scalar = new Mysqlx::Datatypes::Scalar;
    scalar->set_v_bool(true);
    scalar->set_type(Mysqlx::Datatypes::Scalar_Type::Scalar_Type_V_BOOL);

    auto any = new Mysqlx::Datatypes::Any;
    any->set_type(Mysqlx::Datatypes::Any_Type::Any_Type_SCALAR);
    any->set_allocated_scalar(scalar);

    cap->set_allocated_value(any);
    std::vector<uint8_t> out_buf;
    xproto_frame_encode(msg, out_buf);

    // dump(out_buf);

    server_channel()->write(net::buffer(out_buf));

#if 0
    log_debug("%d: << %s (switch-to-tls sent)", __LINE__,
              state_to_string(state()));
#endif
    return State::TLS_CLIENT_GREETING_RESPONSE;
  } else {
#if 0
    log_debug("%d: << %s", __LINE__, state_to_string(state()));
#endif
    return State::SPLICE_INIT;
  }
}

BasicSplicer::State XProtocolSplicer::tls_client_greeting_response() {
#if 0
  log_debug("%d: >> %s", __LINE__, state_to_string(state()));
#endif
  /**
   * we sent the server a cap-set: tls.
   *
   * check if the server likes it.
   */
  using google::protobuf::io::CodedInputStream;

  if (!client_waiting() && server_waiting()) {
    // the client woke us up, we are actually waiting for the server.
    client_channel()->want_recv(1);
    return state();
  }

  const uint32_t header_size{4};

  auto *src_channel = server_channel();
  auto &recv_buf = src_channel->recv_buffer();

  if (recv_buf.size() < header_size) {
#if 0
      log_debug("%d: << (%s) [want more]", __LINE__, state_to_string(state()));
#endif
    src_channel->want_recv(1);
    return state();
  }

  while (recv_buf.size() != 0) {
    if (recv_buf.size() < header_size) {
      src_channel->want_recv(1);
      return state();
    }
    uint32_t payload_size;

    CodedInputStream::ReadLittleEndian32FromArray(recv_buf.data(),
                                                  &payload_size);

    if (recv_buf.size() < header_size + payload_size) {
      src_channel->want_recv(1);
      return state();
    }

    if (payload_size == 0) {
      // payload should not be empty.
      return State::FINISH;
    }

    const uint8_t message_type = recv_buf[header_size + 0];

#if 0
      log_debug("%d: .. %s -> %s", __LINE__, state_to_string(state()),
                xproto_server_message_to_string(message_type));
#endif

    if (message_type == Mysqlx::ServerMessages::OK) {
      net::dynamic_buffer(recv_buf).consume(header_size + payload_size);

      server_channel()->is_tls(true);
      server_channel()->init_ssl(server_ssl_ctx_getter_());
#if 0
        log_debug("%d: << %s", __LINE__, state_to_string(state()));
#endif
      return State::TLS_CONNECT;
    } else if (message_type == Mysqlx::ServerMessages::ERROR) {
      // switch to TLS failed. ... if it is required, send error and drop
      // connection.
      net::dynamic_buffer(recv_buf).consume(header_size + payload_size);

      if (dest_ssl_mode() == SslMode::kRequired) {
        std::vector<uint8_t> error_frame;
        auto err_msg = Mysqlx::Error();

        // make sure the client didn't retry, but aborts the
        // auth-session-start cycle by marking it FATAL.
        err_msg.set_severity(Mysqlx::Error::FATAL);
        err_msg.set_msg("Server needs TLS");
        err_msg.set_code(3159);  // ER_SECURE_TRANSPORT_REQUIRED
        err_msg.set_sql_state("HY000");

        xproto_frame_encode(err_msg, error_frame);

        client_channel()->write_plain(net::buffer(error_frame));
        client_channel()->flush_to_send_buf();

        return State::FINISH;
      } else if (dest_ssl_mode() == SslMode::kPreferred) {
        // it is ok that it failed.
#if 0
          log_debug("%d: << %s (plain-size: %zu)", __LINE__,
                    state_to_string(state()),
                    client_channel()->recv_plain_buffer().size());
#endif

        auto &plain = client_channel()->recv_plain_buffer();
        auto plain_buf = net::dynamic_buffer(plain);

        read_to_plain(client_channel(), plain);

        if (!client_channel()->recv_plain_buffer().empty()) {
          // if there is already some data in the plain buffers, send it to
          // the backends.
          // forward the frame as is.
          const auto write_res = server_channel()->write(
              plain_buf.data(0, header_size + payload_size));
          if (!write_res) {
            log_debug("write to dst-channel failed: %s",
                      write_res.error().message().c_str());
            return State::FINISH;
          }

          plain_buf.consume(write_res.value());
        } else {
          client_channel()->want_recv(1);
        }

        return State::SPLICE;
      } else if (dest_ssl_mode() == SslMode::kAsClient) {
        // client side has TLS established, but opening server side failed.
        std::vector<uint8_t> error_frame;

        auto err_msg = Mysqlx::Error();

        // make sure the client didn't retry, but aborts the
        // auth-session-start cycle by marking it FATAL.
        err_msg.set_severity(Mysqlx::Error::FATAL);
        err_msg.set_msg("Router failed to open TLS connection to server");
        err_msg.set_code(3159);  // ER_SECURE_TRANSPORT_REQUIRED
        err_msg.set_sql_state("HY000");

        xproto_frame_encode(err_msg, error_frame);

        client_channel()->write_plain(net::buffer(error_frame));
        client_channel()->flush_to_send_buf();

        return State::FINISH;
      } else {
        std::terminate();
      }
    } else {
      net::dynamic_buffer(recv_buf).consume(header_size + payload_size);
    }
  }

#if 0
    log_debug("%d: << %s", __LINE__, state_to_string(state()));
#endif
  return state();
}

stdx::expected<size_t, std::error_code> XProtocolSplicer::write_error_packet(
    std::vector<uint8_t> &error_frame, uint16_t error_code,
    const std::string &msg, const std::string &sql_state) {
  auto err_msg = Mysqlx::Error();

  err_msg.set_severity(Mysqlx::Error::ERROR);
  err_msg.set_msg(msg);
  err_msg.set_code(error_code);
  err_msg.set_sql_state(sql_state);

  return xproto_frame_encode(err_msg, error_frame);
}

BasicSplicer::State XProtocolSplicer::tls_connect() {
#if 0
  log_debug("%d: >> %s", __LINE__, state_to_string(state()));
#endif
  auto *channel = server_channel();

  {
    const auto flush_res = channel->flush_from_recv_buf();
    if (!flush_res) {
      return log_fatal_error_code("tls_connect::recv::flush() failed",
                                  flush_res.error());
    }
  }

  if (tls_connect_sent_ && server_waiting() && !client_waiting()) {
    // the TLS connect has already been sent and we are waiting for the servers
    // response.
    //
    // Looks like we got called by data from the client side as the client isn't
    // waiting.
    client_channel()->want_recv(1);

    return state();
  }

  if (!channel->tls_init_is_finished()) {
#if 0
    log_debug("%d: .. %s", __LINE__, state_to_string(state()));
#endif

    tls_connect_sent_ = true;

    const auto res = channel->tls_connect();

    if (!res) {
      if (res.error() == TlsErrc::kWantRead) {
#if 0
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
#if 0
        log_debug("%d: .. %s", __LINE__, state_to_string(state()));
#endif
        // connect may fail fatally if
        //
        // - cert-verification failed.
        // - no shared cipher
        std::vector<uint8_t> error_frame;

        auto encode_res = write_error_packet(
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

#if 0
    log_debug("%d: .. %s", __LINE__, state_to_string(state()));
#endif
  }

#if 0
  log_debug("%d: << %s", __LINE__, state_to_string(state()));
#endif
  return State::SPLICE_INIT;
}

std::unique_ptr<google::protobuf::MessageLite> make_client_message(
    uint8_t message_type) {
  switch (message_type) {
    case Mysqlx::ClientMessages::SESS_AUTHENTICATE_START:
      return std::make_unique<Mysqlx::Session::AuthenticateStart>();
    case Mysqlx::ClientMessages::CON_CAPABILITIES_GET:
      return std::make_unique<Mysqlx::Connection::CapabilitiesGet>();
    case Mysqlx::ClientMessages::CON_CAPABILITIES_SET:
      return std::make_unique<Mysqlx::Connection::CapabilitiesSet>();
    case Mysqlx::ClientMessages::CON_CLOSE:
      return std::make_unique<Mysqlx::Connection::Close>();
  }

  return {};
}

std::unique_ptr<google::protobuf::MessageLite> make_server_message(
    uint8_t message_type) {
  switch (message_type) {
    case Mysqlx::ServerMessages::CONN_CAPABILITIES:
      return std::make_unique<Mysqlx::Connection::Capabilities>();
    case Mysqlx::ServerMessages::NOTICE:
      return std::make_unique<Mysqlx::Notice::Frame>();
  }

  return {};
}

BasicSplicer::State XProtocolSplicer::xproto_splice_int(
    Channel *src_channel, XProtocolState * /* src_protocol */,
    Channel *dst_channel, XProtocolState *
    /* dst_protocol */) {
  const bool to_server = src_channel == client_channel();

  auto &plain = src_channel->recv_plain_buffer();
  read_to_plain(src_channel, plain);

#if 0
  if (plain.empty()) {
    log_debug("%d", __LINE__);
    src_channel->want_recv(1);
    return state();
  }
#endif

#if 0
  log_debug("%d: %s", __LINE__, to_server ? "c->s" : "c<-s");

  log_debug("%d:\n%s", __LINE__, dump(plain).c_str());
#endif

  auto plain_buf = net::dynamic_buffer(plain);

  if (source_ssl_mode() == SslMode::kPassthrough && src_channel->is_tls()) {
#if 0
    log_debug("%d: >> %s", __LINE__, state_to_string(state()));
#endif
    // at least the TLS record header.
    const size_t tls_header_size = 5;
    while (plain_buf.size() > tls_header_size) {
      // plain is TLS traffic.
      const uint8_t tls_content_type = plain[0];
      const uint16_t tls_payload_size = (plain[3] << 8) | plain[4];

#if defined(DEBUG_SSL)
      const uint16_t tls_legacy_version = (plain[1] << 8) | plain[2];

      printf("-- ssl: ver=%04x, len=%d, %s\n", tls_legacy_version,
             tls_payload_size,
             tls_content_type_to_string(
                 static_cast<TlsContentType>(tls_content_type))
                 .c_str());
#endif

      if (plain_buf.size() < tls_header_size + tls_payload_size) {
        return state();
      }

      auto write_res = dst_channel->write(
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
#if 0
    log_debug("%d: >> %s: %zu", __LINE__, state_to_string(state()),
              plain_buf.size());
#endif
    using google::protobuf::io::CodedInputStream;

    while (plain_buf.size() != 0) {
      const uint32_t header_size{4};
      if (plain_buf.size() < header_size) {
        src_channel->want_recv(1);
        return state();
      }
      uint32_t payload_size;

      CodedInputStream::ReadLittleEndian32FromArray(plain.data(),
                                                    &payload_size);

      if (plain_buf.size() < header_size + payload_size) {
        src_channel->want_recv(1);
        return state();
      }

      bool forward_as_is{true};

      if (payload_size > 0) {
        uint8_t message_type = plain[header_size + 0];

        // check if the message is finishes the handshake part that needs to
        // be tracked for connection-error-tracking.
        if (!handshake_done()) {
          if (to_server) {
            switch (message_type) {
              case Mysqlx::ClientMessages::SESS_AUTHENTICATE_START:
              case Mysqlx::ClientMessages::CON_CAPABILITIES_GET:
              case Mysqlx::ClientMessages::CON_CAPABILITIES_SET:
              case Mysqlx::ClientMessages::CON_CLOSE: {
                auto msg = make_client_message(message_type);
                if (!msg->ParseFromArray(plain.data() + header_size + 1,
                                         payload_size - 1)) {
                  log_warning("failed to parse message of type: %hhu",
                              message_type);
                  return State::FINISH;
                }

                handshake_done(true);
              } break;
              default:
                log_warning(
                    "Received incorrect message type from the client while "
                    "handshaking (was %hhu)",
                    message_type);
                return State::FINISH;
            }
          } else {
            switch (message_type) {
              case Mysqlx::ServerMessages::ERROR:
                handshake_done(true);
                break;
              default:
                break;
            }
          }
        }

        // - disable SSL if requested.
        // - start TLS if requested.
        if (to_server) {
          // c->r:
#if 0
          log_debug("%d: .. %s -> %s", __LINE__, state_to_string(state()),
                    xproto_client_message_to_string(message_type));
#endif

          // TODO: we check if we really want to do two rounds here.
          xproto_client_msg_type_.push_back(message_type);

          if (message_type == Mysqlx::ClientMessages::SESS_AUTHENTICATE_START) {
            if (source_ssl_mode() == SslMode::kRequired &&
                !client_channel()->is_tls()) {
#if 0
              log_debug("%d: .. %s -> %s (require-tls, but no tls) -> ERROR",
                        __LINE__, state_to_string(state()),
                        xproto_client_message_to_string(message_type));
#endif
              // client wants to authenticate. Ensure TLS on the client side
              // is enabled
              forward_as_is = false;

              // fail.
              auto err_msg = Mysqlx::Error();

              // write the message the server would use.
              err_msg.set_severity(Mysqlx::Error::ERROR);
              err_msg.set_msg("Router requires SSL");
              err_msg.set_code(5001);
              err_msg.set_sql_state("HY000");

              std::vector<uint8_t> out_buf;
              xproto_frame_encode(err_msg, out_buf);

              // dump(out_buf);

              auto write_res = src_channel->write(net::buffer(out_buf));

              // return State::FINISH;
            } else if ((dest_ssl_mode() == SslMode::kRequired ||
                        dest_ssl_mode() == SslMode::kPreferred) &&
                       !server_channel()->is_tls()) {
              if (!tls_handshake_tried_) {
#if 0
                log_debug("%d: .. %s -> %s (trying to switch to TLS)", __LINE__,
                          state_to_string(state()),
                          xproto_client_message_to_string(message_type));
#endif

                // initiate a TLS handshake on the server-side.
                //
                // once it is done, we'll be called again with the same client
                // payload can decide what to do next (tls_handshake_tried_
                // will be true).
                return State::TLS_CLIENT_GREETING;
              } else if (dest_ssl_mode() == SslMode::kRequired) {
                // TLS was attempted, failed ... but config says it is
                // required.

                // don't forward the client packet.
                forward_as_is = false;

                // fail.
                auto err_msg = Mysqlx::Error();

                // write the message the server would use.
                err_msg.set_severity(Mysqlx::Error::ERROR);
                err_msg.set_msg("Server requires SSL");
                err_msg.set_code(5001);
                err_msg.set_sql_state("HY000");

                std::vector<uint8_t> out_buf;
                xproto_frame_encode(err_msg, out_buf);

                // dump(out_buf);

                auto write_res = src_channel->write(net::buffer(out_buf));

                return State::FINISH;
              }
            } else {
#if 0
              log_debug("%d: .. %s -> %s (forwarding ...)", __LINE__,
                        state_to_string(state()),
                        xproto_client_message_to_string(message_type));
#endif
              // otherwise forward as is.
            }
          } else if (message_type ==
                     Mysqlx::ClientMessages::CON_CAPABILITIES_SET) {
            // if config says, that SSL shouldn't be passed through
            //
            // - parse cap-set.
            auto msg = make_client_message(message_type);
            if (!msg->ParseFromArray(plain.data() + header_size + 1,
                                     payload_size - 1)) {
              log_warning("failed to parse message of type: %hhu",
                          message_type);
              return State::FINISH;
            }

            // handle cap-set SSL
            //
            // - if client-ssl-mode is DISABLED, FAIL

            auto *cap_set =
                dynamic_cast<Mysqlx::Connection::CapabilitiesSet *>(msg.get());
            if (cap_set->has_capabilities()) {
              auto &caps = cap_set->capabilities();
              for (auto const &cap : caps.capabilities()) {
                if (cap.has_name()) {
#if 0
                  log_debug("%d: .. %s -> cap:%s", __LINE__,
                            state_to_string(state()), cap.name().c_str());
#endif
                  if (cap.name() == "tls") {
                    if (source_ssl_mode() == SslMode::kPassthrough ||
                        (source_ssl_mode() == SslMode::kPreferred &&
                         dest_ssl_mode() == SslMode::kAsClient)) {
                      // switching the TLS.
                      //
                      // next state should be a "wait for Ok".
                      is_switch_to_tls_ = true;
                    } else if (source_ssl_mode() == SslMode::kDisabled) {
                      forward_as_is = false;
                      // fail.
                      auto err_msg = Mysqlx::Error();

                      // write the message the server would use.
                      err_msg.set_severity(Mysqlx::Error::ERROR);
                      err_msg.set_msg("Capability prepare failed for \'tls\'");
                      err_msg.set_code(5001);
                      err_msg.set_sql_state("HY000");

                      std::vector<uint8_t> out_buf;
                      xproto_frame_encode(err_msg, out_buf);

                      // dump(out_buf);

                      auto write_res = src_channel->write(net::buffer(out_buf));
                    } else if (source_ssl_mode() == SslMode::kPreferred ||
                               source_ssl_mode() == SslMode::kRequired) {
                      forward_as_is = false;
                      // take the packet from the receive-buffer.
                      plain_buf.consume(header_size + payload_size);

                      // send ok and switch to TLS.
                      auto msg = Mysqlx::Ok();

                      std::vector<uint8_t> out_buf;
                      xproto_frame_encode(msg, out_buf);

                      // dump(out_buf);

                      auto write_res = src_channel->write(net::buffer(out_buf));

                      client_channel()->is_tls(true);
                      client_channel()->init_ssl(client_ssl_ctx_getter_());

                      return State::TLS_ACCEPT;
                    } else {
                      std::terminate();
                    }

                    break;
                  }
                }
              }
            }
          }
        } else {
          // r<-s:
#if 0
          log_debug("%d: .. %s <- %s", __LINE__, state_to_string(state()),
                    xproto_server_message_to_string(message_type));
#endif

          const uint8_t client_message_type = xproto_client_msg_type_[0];

          if (message_type == Mysqlx::ServerMessages::OK ||
              message_type == Mysqlx::ServerMessages::ERROR ||
              message_type == Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK) {
            // client command is finished, remove it from the FIFO.
            xproto_client_msg_type_.erase(xproto_client_msg_type_.begin());

            if (client_message_type ==
                    Mysqlx::ClientMessages::CON_CAPABILITIES_SET &&
                is_switch_to_tls_) {
              if (message_type == Mysqlx::ServerMessages::OK) {
                is_switch_to_tls_ = false;

                if (source_ssl_mode() == SslMode::kPassthrough) {
                  // the server side switched to TLS, and the Ok will be
                  // forwarded to the client. Both channel are then expecting
                  // TLS afterwards.
                  src_channel->is_tls(true);
                  dst_channel->is_tls(true);
                } else if (source_ssl_mode() == SslMode::kPreferred &&
                           dest_ssl_mode() == SslMode::kAsClient) {
                  // server agreed to switch to TLS.
                  //
                  // forward the Ok packet as is to the client and expect the
                  // Tls Client Hello afterwards.
                  const auto write_res = dst_channel->write(
                      plain_buf.data(0, header_size + payload_size));
                  if (!write_res) {
                    log_debug("write to dst-channel failed: %s (%d)",
                              write_res.error().message().c_str(),
                              write_res.error().value());
                    return State::FINISH;
                  }

                  plain_buf.consume(write_res.value());

                  client_channel()->is_tls(true);
                  client_channel()->init_ssl(client_ssl_ctx_getter_());

                  return State::TLS_ACCEPT;
                }
              } else if (message_type == Mysqlx::ServerMessages::ERROR) {
                is_switch_to_tls_ = false;
              } else {
                // should be a Notice. Ignore it.
              }
            }
          }

          // - hide compression from the client.
          // - hide TLS from the client.
          if (message_type == Mysqlx::ServerMessages::CONN_CAPABILITIES) {
            auto msg = make_server_message(message_type);
            if (!msg->ParseFromArray(plain.data() + header_size + 1,
                                     payload_size - 1)) {
              log_warning("failed to parse message of type: %hhu",
                          message_type);
              return State::FINISH;
            }

            switch (message_type) {
              case Mysqlx::ServerMessages::CONN_CAPABILITIES: {
                bool has_changed{false};
                auto *caps =
                    dynamic_cast<Mysqlx::Connection::Capabilities *>(msg.get());
                for (auto cur = caps->capabilities().begin();
                     cur != caps->capabilities().end();) {
                  auto &cap = *cur;

                  if (cap.has_name()) {
                    if (cap.name() == "compression" || cap.name() == "tls") {
#if 0
                      log_debug("%d: .. %s <- --cap:%s", __LINE__,
                                state_to_string(state()), cap.name().c_str());
#endif
                      cur = caps->mutable_capabilities()->erase(cur);
                      has_changed = true;
                    } else {
                      ++cur;
                    }
                  }
                }

                if (has_changed) {
                  forward_as_is = false;

                  std::vector<uint8_t> out_buf;

                  xproto_frame_encode(*caps, out_buf);

                  // dump(out_buf);

                  auto write_res = dst_channel->write(net::buffer(out_buf));
                }
              } break;
            }
          }
        }
      }

      if (forward_as_is) {
        // forward the frame as is.
        const auto write_res =
            dst_channel->write(plain_buf.data(0, header_size + payload_size));
        if (!write_res) {
          log_debug("write to dst-channel failed: %s (%d)",
                    write_res.error().message().c_str(),
                    write_res.error().value());
          return State::FINISH;
        }

        plain_buf.consume(write_res.value());
      } else {
#if 0
        log_debug("%d: xx %s", __LINE__, state_to_string(state()));
#endif
        // skip the packet
        plain_buf.consume(header_size + payload_size);
      }

      const auto flush_res = dst_channel->flush_to_send_buf();
      if (!flush_res) {
        log_debug("%s: flush to dst failed: %s (%d)", state_to_string(state()),
                  flush_res.error().message().c_str(),
                  flush_res.error().value());

        return State::FINISH;
      }
    }
  }

#if defined(DEBUG_SSL)
  std::cerr << __LINE__ << ": " << from << "::want-read" << std::endl;
#endif

  src_channel->want_recv(1);
#if 0
  log_debug("%d: << %s", __LINE__, state_to_string(state()));
#endif
  return state();
}
