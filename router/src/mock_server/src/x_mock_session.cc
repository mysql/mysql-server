/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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
#include "x_mock_session.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysqlrouter/classic_protocol_codec_base.h"
#include "mysqlrouter/classic_protocol_codec_error.h"
#include "mysqlrouter/classic_protocol_wire.h"
#include "router/src/mock_server/src/statement_reader.h"

#include <exception>
#include <memory>
#include <system_error>
#include <thread>
#include <tuple>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>

#include "config.h"
#include "harness_assert.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol_codec_wire.h"
#include "mysqlx_error.h"
#include "mysqlxclient/xprotocol.h"

IMPORT_LOG_FUNCTIONS()

namespace server_mock {

template <class Rep, class Period>
static std::string duration_to_us_string(
    const std::chrono::duration<Rep, Period> &dur) {
  return std::to_string(
             std::chrono::duration_cast<std::chrono::microseconds>(dur)
                 .count()) +
         " us";
}

stdx::expected<size_t, std::error_code> MySQLXProtocol::decode_frame(
    std::vector<uint8_t> &payload) {
  auto buf = net::buffer(recv_buffer_);
  auto decode_res =
      classic_protocol::decode<classic_protocol::wire::FixedInt<4>>(buf, {});
  if (!decode_res) return stdx::unexpected(decode_res.error());

  auto hdr_size = decode_res->first;
  auto payload_size = decode_res->second.value();

  // skip header.
  buf += hdr_size;

  if (buf.size() < payload_size) {
    // not enough data.
    return stdx::unexpected(make_error_code(std::errc::operation_would_block));
  }

  payload.resize(payload_size);
  net::buffer_copy(net::buffer(payload), buf, payload_size);

  // remove the bytes from the recv-buffer
  net::dynamic_buffer(recv_buffer_).consume(hdr_size + payload_size);

  return payload_size;
}

stdx::expected<std::pair<xcl::XProtocol::Client_message_type_id,
                         std::unique_ptr<xcl::XProtocol::Message>>,
               std::error_code>
MySQLXProtocol::decode_single_message(const std::vector<uint8_t> &payload) {
  using ret_type =
      stdx::expected<std::pair<xcl::XProtocol::Client_message_type_id,
                               std::unique_ptr<xcl::XProtocol::Message>>,
                     std::error_code>;
  if (payload.empty())
    return stdx::unexpected(make_error_code(std::errc::bad_message));

  uint8_t header_msg_id = payload[0];

  auto msg_id =
      static_cast<xcl::XProtocol::Client_message_type_id>(header_msg_id);

  auto buf = net::buffer(payload) + 1;

  try {
    return ret_type{std::in_place, msg_id,
                    protocol_decoder_.decode_message(
                        header_msg_id, static_cast<const uint8_t *>(buf.data()),
                        buf.size())};
  } catch (...) {
    return stdx::unexpected(make_error_code(std::errc::bad_message));
  }
}

void MySQLXProtocol::encode_message(
    const xcl::XProtocol::Server_message_type_id msg_id,
    const xcl::XProtocol::Message &msg) {
#if (defined(GOOGLE_PROTOBUF_VERSION) && GOOGLE_PROTOBUF_VERSION > 3000000)
  const std::size_t msg_size = msg.ByteSizeLong();
#else
  const std::size_t msg_size = msg.ByteSize();
#endif

  auto payload_size = 1 + msg_size;

  classic_protocol::encode(classic_protocol::wire::FixedInt<4>(payload_size),
                           {}, net::dynamic_buffer(send_buffer_));
  auto dyn_buf = net::dynamic_buffer(send_buffer_);

  auto orig_size = dyn_buf.size();
  auto grow_size = payload_size;
  dyn_buf.grow(grow_size);

  auto buf = dyn_buf.data(orig_size, grow_size);

  static_cast<uint8_t *>(buf.data())[0] = msg_id;
  buf += 1;

  if (!msg.SerializeToArray(buf.data(), buf.size())) {
    throw std::runtime_error("Failed to serialize the message");
  }
}

MySQLServerMockSessionX::clock_type::time_point
MySQLServerMockSessionX::notice_expiry() const {
  clock_type::time_point earliest{clock_type::time_point::max()};

  for (const auto &notice : async_notices_) {
    auto offset = start_time_ + notice.send_offset_ms;

    if (offset < earliest) {
      earliest = offset;
    }
  }

  return earliest;
}

bool MySQLServerMockSessionX::encode_due_async_notices(
    const MySQLServerMockSessionX::clock_type::time_point &start_time) {
  if (async_notices_.empty()) return false;

  const auto current_time = clock_type::now();
  auto ms_passed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       current_time - start_time)
                       .count();

  bool some{false};
  for (auto it = async_notices_.begin(); it != async_notices_.end();) {
    if (it->send_offset_ms.count() <= ms_passed) {
      protocol_.encode_async_notice(*it);
      some = true;
      it = async_notices_.erase(it);
    } else {
      ++it;
    }
  }

  return some;
}

stdx::expected<std::unique_ptr<xcl::XProtocol::Message>, std::string>
MySQLXProtocol::gr_state_changed_from_json(const std::string &json_string) {
  rapidjson::Document json_doc;
  auto result{std::make_unique<Mysqlx::Notice::GroupReplicationStateChanged>()};
  json_doc.Parse(json_string.data(), json_string.size());

  {
    const auto it = json_doc.FindMember("type");
    if (it != json_doc.MemberEnd()) {
      if (it->value.IsUint()) {
        result->set_type(it->value.GetUint());
      } else {
        return stdx::unexpected(
            "Invalid json type for field 'type', expected 'uint' got " +
            std::to_string(it->value.GetType()));
      }
    }
  }

  {
    const auto it = json_doc.FindMember("view-id");
    if (it != json_doc.MemberEnd()) {
      if (it->value.IsString()) {
        result->set_view_id(it->value.GetString());
      } else {
        return stdx::unexpected(
            "Invalid json type for field 'view_id', expected 'string' got " +
            std::to_string(it->value.GetType()));
      }
    }
  }

  return std::unique_ptr<xcl::XProtocol::Message>(std::move(result));
}

stdx::expected<std::unique_ptr<xcl::XProtocol::Message>, std::string>
MySQLXProtocol::get_notice_message(const unsigned id,
                                   const std::string &payload) {
  switch (id) {
    case Mysqlx::Notice::Frame_Type_GROUP_REPLICATION_STATE_CHANGED: {
      return gr_state_changed_from_json(payload);
    }
    case Mysqlx::Notice::Frame_Type_SERVER_HELLO:
      return std::unique_ptr<xcl::XProtocol::Message>(
          std::make_unique<Mysqlx::Notice::ServerHello>());
    // those we currently not use, if needed add a function encoding json
    // string to the selected message type
    case Mysqlx::Notice::Frame_Type_WARNING:
    case Mysqlx::Notice::Frame_Type_SESSION_VARIABLE_CHANGED:
    case Mysqlx::Notice::Frame_Type_SESSION_STATE_CHANGED:
    default:
      return stdx::unexpected("Unsupported notice id: " + std::to_string(id));
  }
}

void MySQLXProtocol::encode_async_notice(const AsyncNotice &async_notice) {
  Mysqlx::Notice::Frame notice_frame;
  notice_frame.set_type(async_notice.type);
  notice_frame.set_scope(async_notice.is_local
                             ? Mysqlx::Notice::Frame_Scope_LOCAL
                             : Mysqlx::Notice::Frame_Scope_GLOBAL);

  auto notice_msg = get_notice_message(async_notice.type, async_notice.payload);

  if (!notice_msg)
    throw std::runtime_error("Failed encoding notice message: " +
                             notice_msg.error());

  notice_frame.set_payload(notice_msg.value()->SerializeAsString());

  encode_message(
      xcl::XProtocol::Server_message_type_id::ServerMessages_Type_NOTICE,
      notice_frame);
}

MySQLServerMockSessionX::MySQLServerMockSessionX(
    MySQLXProtocol protocol,
    std::unique_ptr<StatementReaderBase> statement_processor,
    const bool debug_mode, bool with_tls)
    : MySQLServerMockSession(std::move(statement_processor), debug_mode),
      async_notices_(this->json_reader_->get_async_notices()),
      protocol_{std::move(protocol)},
      with_tls_{with_tls} {}

void MySQLServerMockSessionX::send_response_then_handshake() {
  protocol_.async_send([&, to_send = protocol_.send_buffer().size()](
                           std::error_code ec, size_t transferred) {
    if (ec) {
      disconnect();
      return;
    };

    if (transferred < to_send) {
      send_response_then_handshake();
    } else {
      handshake();
    }
  });
}

void MySQLServerMockSessionX::greeting() {
  protocol_.encode_async_notice(
      {{},                                       // send_offset_ms
       Mysqlx::Notice::Frame_Type_SERVER_HELLO,  // type
       false,                                    // is_local
       {}});                                     // payload

  send_response_then_handshake();
}

void MySQLServerMockSessionX::handshake() {
  std::vector<uint8_t> payload;

  auto decode_frame_res = protocol_.decode_frame(payload);
  if (!decode_frame_res) {
    auto ec = decode_frame_res.error();
    if (ec == classic_protocol::codec_errc::not_enough_input) {
      protocol_.async_receive(
          [&](std::error_code ec, size_t /* transferred */) {
            if (ec) {
              if (ec != std::errc::operation_canceled) {
                log_warning("receive handshake-frame failed: %s",
                            ec.message().c_str());
              }

              disconnect();
              return;
            }

            // call handshake again to process the message.
            handshake();
          });
      return;
    } else {
      log_warning("decoding handshake-frame failed: %s", ec.message().c_str());

      protocol_.encode_error({ER_X_BAD_MESSAGE, "Bad Message", "HY000"});

      send_response_then_disconnect();

      return;
    }
  }

  auto decode_res = protocol_.decode_single_message(payload);
  if (!decode_res) {
    auto ec = decode_res.error();
    log_warning("decoding handshake-message failed: %s", ec.message().c_str());

    protocol_.encode_error({ER_X_BAD_MESSAGE, "Bad Message", "HY000"});

    send_response_then_disconnect();

    return;
  }

  auto msg_id = decode_res->first;
  auto msg = std::move(decode_res->second);

  switch (msg_id) {
    case Mysqlx::ClientMessages::CON_CAPABILITIES_SET: {
      auto *capab_msg =
          dynamic_cast<Mysqlx::Connection::CapabilitiesSet *>(msg.get());
      harness_assert(capab_msg != nullptr);
      bool tls_request = false;
      bool compression_request = false;
      const auto capabilities = capab_msg->capabilities();
      for (int i = 0; i < capabilities.capabilities_size(); ++i) {
        const auto capability = capabilities.capabilities(i);
        if (capability.name() == "tls") tls_request = true;
        if (capability.name() == "compression") compression_request = true;
      }

      if (tls_request) {
        if (with_tls_) {
          Mysqlx::Ok ok_msg;
          protocol_.encode_message(Mysqlx::ServerMessages::OK, ok_msg);

          protocol_.async_send(
              [&](std::error_code ec, size_t /* transferred */) {
                if (ec) {
                  disconnect();
                  return;
                }

                protocol_.init_tls();

                protocol_.async_tls_accept([&](std::error_code ec) {
                  if (ec) {
                    if (ec != std::errc::operation_canceled) {
                      log_warning("async_tls_accept failed: %s",
                                  ec.message().c_str());
                    }

                    disconnect();
                    return;
                  }

                  auto *ssl = protocol_.ssl();
                  json_reader_->set_session_ssl_info(ssl);

                  // read the next message via SSL
                  handshake();
                });
              });

          return;
        } else {
          protocol_.encode_error({ER_X_CAPABILITIES_PREPARE_FAILED,
                                  "Capability prepare failed for tls",
                                  "HY000"});

          send_response_then_handshake();

          return;
        }
      } else if (compression_request) {
        protocol_.encode_error(
            {ER_X_CAPABILITY_COMPRESSION_INVALID_ALGORITHM,
             "Invalid or unsupported value for 'compression.algorithm'",
             "HY000"});

        send_response_then_handshake();

        return;
      } else {
        Mysqlx::Ok ok_msg;
        protocol_.encode_message(Mysqlx::ServerMessages::OK, ok_msg);

        send_response_then_handshake();
      }
      return;
    }
    case Mysqlx::ClientMessages::CON_CAPABILITIES_GET: {
      Mysqlx::Connection::Capabilities msg_capab;

      if (with_tls_) {
        auto scalar = new Mysqlx::Datatypes::Scalar;
        scalar->set_type(Mysqlx::Datatypes::Scalar_Type::Scalar_Type_V_BOOL);
        scalar->set_v_bool(true);

        auto any = new Mysqlx::Datatypes::Any;
        any->set_type(Mysqlx::Datatypes::Any_Type::Any_Type_SCALAR);
        any->set_allocated_scalar(scalar);

        auto *tls_cap = msg_capab.add_capabilities();
        tls_cap->set_name("tls");
        tls_cap->set_allocated_value(any);
      }

      protocol_.encode_message(Mysqlx::ServerMessages::CONN_CAPABILITIES,
                               msg_capab);

      send_response_then_handshake();
      return;
    }
    case Mysqlx::ClientMessages::SESS_AUTHENTICATE_START: {
      Mysqlx::Session::AuthenticateContinue msg_auth_cont;
      msg_auth_cont.set_auth_data("01234567890123456789");
      protocol_.encode_message(
          Mysqlx::ServerMessages::SESS_AUTHENTICATE_CONTINUE, msg_auth_cont);

      send_response_then_handshake();
      return;
    }
    case Mysqlx::ClientMessages::SESS_AUTHENTICATE_CONTINUE: {
      Mysqlx::Session::AuthenticateOk msg_auth_ok;
      protocol_.encode_message(Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK,
                               msg_auth_ok);

      send_response_then_first_idle();

      return;
    }
    case Mysqlx::ClientMessages::CON_CLOSE: {
      // wait until the client closed the connection.
      protocol_.async_receive([&](std::error_code ec, size_t transferred) {
        if (ec) {
          // EOF is expected, don't log it.
          if (ec != net::stream_errc::eof &&
              ec != std::errc::operation_canceled) {
            log_warning("receive connection-close failed: %s",
                        ec.message().c_str());
          }
        } else {
          // something _was_ sent? log it.
          log_debug("data after QUIT: %zu", transferred);
        }
        disconnect();
      });

      return;
    }
    default:
      disconnect();
      return;
  }
}

void MySQLServerMockSessionX::send_response_then_disconnect() {
  protocol_.async_send([&, to_send = protocol_.send_buffer().size()](
                           std::error_code ec, size_t transferred) {
    if (ec) {
      if (ec != std::errc::operation_canceled) {
        log_warning("sending response failed: %s", ec.message().c_str());
      }
      disconnect();
      return;
    }

    if (transferred < to_send) {
      // still some data to send.

      send_response_then_disconnect();
    } else {
      disconnect();
    }
  });
}

void MySQLServerMockSessionX::send_response_then_first_idle() {
  protocol_.async_send([&, to_send = protocol_.send_buffer().size()](
                           std::error_code ec, size_t transferred) {
    if (ec) {
      if (ec != std::errc::operation_canceled) {
        log_warning("sending response failed: %s", ec.message().c_str());
      }

      disconnect();
      return;
    }

    if (transferred < to_send) {
      // still some data to send.

      send_response_then_first_idle();
    } else {
      start_time_ = clock_type::now();

      // fetch the first statement.
      notices();
      idle();
    }
  });
}

void MySQLServerMockSessionX::send_response_then_idle() {
  protocol_.async_send([&, to_send = protocol_.send_buffer().size()](
                           std::error_code ec, size_t transferred) {
    if (ec) {
      if (ec != std::errc::operation_canceled) {
        log_warning("sending response failed: %s", ec.message().c_str());
      }

      disconnect();
      return;
    }

    if (transferred < to_send) {
      // still some data to send.

      send_response_then_idle();
    } else {
      // fetch the next statement.
      idle();
    }
  });
}

void MySQLServerMockSessionX::send_notice_then_notices() {
  protocol_.async_send([&, to_send = protocol_.send_buffer().size()](
                           std::error_code ec, size_t transferred) {
    if (ec) {
      if (ec != std::errc::operation_canceled) {
        log_warning("sending notice failed: %s", ec.message().c_str());
      }
      return;
    };

    if (transferred < to_send) {
      // still some data to send.

      send_notice_then_notices();
    } else {
      notices();
    }
  });
}

// set the timer for the notices.
void MySQLServerMockSessionX::notices() {
  const auto notice_ts = notice_expiry();

  if (notice_ts == clock_type::time_point::max()) return;

  // there is at least a notice.
  notice_timer_.expires_at(notice_ts);

  notice_timer_.async_wait([&](std::error_code ec) {
    if (ec) {
      if (ec != std::errc::operation_canceled) {
        log_warning("waiting for notice timer failed: %s",
                    ec.message().c_str());
      }

      return;
    }

    if (encode_due_async_notices(start_time_)) {
      send_notice_then_notices();
    }
  });
}

void MySQLServerMockSessionX::idle() {
  if (start_time_ == clock_type::time_point{}) {
    throw std::logic_error("start_time_ isn't set.");
  }

  std::vector<uint8_t> payload;

  auto decode_frame_res = protocol_.decode_frame(payload);
  if (!decode_frame_res) {
    protocol_.async_receive([&](std::error_code ec, size_t /* transferred */) {
      if (ec) {
        if (ec != std::errc::operation_canceled) {
          log_warning("receiving frame failed: %s", ec.message().c_str());
        }

        disconnect();
        return;
      }

      // call handshake again to process the message.
      idle();
    });
    return;
  }

  auto decode_res = protocol_.decode_single_message(payload);
  if (!decode_res) {
    auto ec = decode_res.error();

    log_warning("decoding message failed: %s", ec.message().c_str());

    disconnect();
    return;
  }

  const auto msg_id = decode_res->first;
  const auto msg = std::move(decode_res->second);

  switch (msg_id) {
    case Mysqlx::ClientMessages::SQL_STMT_EXECUTE: {
      auto *msg_stmt_execute =
          dynamic_cast<Mysqlx::Sql::StmtExecute *>(msg.get());
      harness_assert(msg_stmt_execute != nullptr);
      const auto statement_received = msg_stmt_execute->stmt();

      try {
        const auto started = std::chrono::steady_clock::now();

        json_reader_->handle_statement(statement_received, &protocol_);

        // handle_statement will set the exec-timer.
        protocol_.exec_timer().async_wait(
            [this, started,
             statement = statement_received](std::error_code ec) {
              // wait until exec-time passed.
              if (ec) {
                if (ec != std::errc::operation_canceled) {
                  log_warning("waiting for exec-timer failed: %s",
                              ec.message().c_str());
                }
                disconnect();
                return;
              }

              const auto now = std::chrono::steady_clock::now();
              log_info("(%s)> %s", duration_to_us_string(now - started).c_str(),
                       statement.c_str());

              send_response_then_idle();
            });

      } catch (const std::exception &e) {
        // handling statement failed. Return the error to the client
        protocol_.encode_error(
            {1064, std::string("executing statement failed: ") + e.what(),
             "HY000"});

        send_response_then_idle();
      }
      return;

    } break;

    case Mysqlx::ClientMessages::CON_CLOSE: {
      protocol_.async_receive([&](std::error_code ec, size_t transferred) {
        if (ec) {
          // EOF is expected, don't log it.
          if (ec != net::stream_errc::eof) {
            log_warning("receive connection-close failed: %s",
                        ec.message().c_str());
          }
        } else {
          log_debug("data after QUIT: %zu", transferred);
        }
        disconnect();
      });
      return;
    }

    default:
      log_error("received unsupported message from the x-client: %d",
                static_cast<int>(msg_id));

      protocol_.encode_error(
          {1064, "Unsupported command: " + std::to_string(msg_id), "HY000"});

      send_response_then_disconnect();

      return;
  }
}

void MySQLServerMockSessionX::run() { greeting(); }

void MySQLXProtocol::encode_error(const ErrorResponse &err) {
  Mysqlx::Error err_msg;
  protocol_encoder_.encode_error(err_msg, err.error_code(), err.message(),
                                 err.sql_state());

  encode_message(Mysqlx::ServerMessages::ERROR, err_msg);
}

void MySQLXProtocol::encode_ok(const OkResponse & /* msg */) {
  Mysqlx::Sql::StmtExecuteOk ok_msg;
  encode_message(Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK, ok_msg);
}

void MySQLXProtocol::encode_resultset(const ResultsetResponse &response) {
  for (const auto &column : response.columns) {
    Mysqlx::Resultset::ColumnMetaData metadata_msg;
    protocol_encoder_.encode_metadata(metadata_msg, column);
    encode_message(Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA,
                   metadata_msg);
  }

  for (const auto &row : response.rows) {
    if (response.columns.size() != row.size()) {
      throw std::runtime_error(
          std::string("columns_info.size() != row_values.size() ") +
          std::to_string(response.columns.size()) + std::string("!=") +
          std::to_string(row.size()));
    }
    Mysqlx::Resultset::Row row_msg;

    size_t col_ndx{};
    for (const auto &field : row) {
      const bool is_null = !field;
      protocol_encoder_.encode_row_field(
          row_msg,
          protocol_encoder_.column_type_to_x(
              response.columns[col_ndx++].type()),
          field.value(), is_null);
    }
    encode_message(Mysqlx::ServerMessages::RESULTSET_ROW, row_msg);
  }

  Mysqlx::Resultset::FetchDone fetch_done_msg;
  encode_message(Mysqlx::ServerMessages::RESULTSET_FETCH_DONE, fetch_done_msg);
  encode_ok({});
}

}  // namespace server_mock
