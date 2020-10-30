/*
  Copyright (c) 2019, 2020, Oracle and/or its affiliates.

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
#include "x_mock_session.h"

#include <thread>
#include <tuple>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>

#include "config.h"
#include "harness_assert.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlx_error.h"
#include "mysqlxclient/xprotocol.h"
IMPORT_LOG_FUNCTIONS()

namespace server_mock {

stdx::expected<std::tuple<uint8_t, size_t>, std::error_code>
MySQLXProtocol::recv_header(const net::const_buffer &buf) {
  union {
    std::array<uint8_t, 5> header_buffer;
    uint32_t payload_size;
  };

  if (buf.size() < 5) {
    // too small
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }

  net::buffer_copy(net::buffer(header_buffer), buf);

#ifdef WORDS_BIGENDIAN
  std::swap(header_buffer[0], header_buffer[3]);
  std::swap(header_buffer[1], header_buffer[2]);
#endif

  size_t buffer_size = payload_size - 1;
  uint8_t msg_id = header_buffer[4];

  return {std::make_tuple(msg_id, buffer_size)};
}

std::unique_ptr<xcl::XProtocol::Message> MySQLXProtocol::recv_single_message(
    xcl::XProtocol::Client_message_type_id *out_msg_id) {
  std::array<uint8_t, 5> header_buf;
  net::mutable_buffer recv_buf = net::buffer(header_buf);

  read_buffer(recv_buf);

  const auto recv_res = recv_header(net::buffer(header_buf));
  if (!recv_res) return nullptr;

  uint8_t header_msg_id = std::get<0>(recv_res.value());
  std::size_t payload_size = std::get<1>(recv_res.value());

  std::vector<std::uint8_t> payload;
  if (payload_size > 0) {
    payload.resize(payload_size);
    auto b = net::buffer(payload);
    read_buffer(b);
  }

  *out_msg_id =
      static_cast<xcl::XProtocol::Client_message_type_id>(header_msg_id);

  return protocol_decoder_.decode_message(header_msg_id, payload.data(),
                                          payload.size());
}

void MySQLXProtocol::send_message(
    const xcl::XProtocol::Server_message_type_id msg_id,
    const xcl::XProtocol::Message &msg) {
  std::string msg_buffer;
  const std::uint8_t header_size = 5;

#if (defined(GOOGLE_PROTOBUF_VERSION) && GOOGLE_PROTOBUF_VERSION > 3000000)
  const std::size_t msg_size = msg.ByteSizeLong();
#else
  const std::size_t msg_size = msg.ByteSize();
#endif

  msg_buffer.resize(msg_size + header_size);

  if (!msg.SerializeToArray(&msg_buffer[0] + header_size, msg_size)) {
    throw std::runtime_error("Failed to serialize the message");
  }

  const auto msg_size_to_buffer = static_cast<std::uint32_t>(msg_size + 1);

  memcpy(&msg_buffer[0], &msg_size_to_buffer, sizeof(std::uint32_t));
#ifdef WORDS_BIGENDIAN
  std::swap(msg_buffer[0], msg_buffer[3]);
  std::swap(msg_buffer[1], msg_buffer[2]);
#endif
  msg_buffer[4] = msg_id;

  send_buffer(net::buffer(msg_buffer));
}

void MySQLServerMockSessionX::send_due_async_notices(
    const std::chrono::time_point<std::chrono::system_clock> &start_time) {
  if (async_notices_.empty()) return;

  const auto current_time = std::chrono::system_clock::now();
  auto ms_passed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       current_time - start_time)
                       .count();
  for (auto it = async_notices_.begin(); it != async_notices_.end();) {
    if (it->send_offset_ms.count() <= ms_passed) {
      protocol_->send_async_notice(*it);
      it = async_notices_.erase(it);
    } else {
      ++it;
    }
  }
}

stdx::expected<std::unique_ptr<xcl::XProtocol::Message>, std::string>
MySQLXProtocol::gr_state_changed_from_json(const std::string &json_string) {
  rapidjson::Document json_doc;
  auto result{std::make_unique<Mysqlx::Notice::GroupReplicationStateChanged>()};
  json_doc.Parse(json_string.c_str());
  if (json_doc.HasMember("type")) {
    if (json_doc["type"].IsUint()) {
      result->set_type(json_doc["type"].GetUint());
    } else {
      return stdx::make_unexpected(
          "Invalid json type for field 'type', expected 'uint' got " +
          std::to_string(json_doc["type"].GetType()));
    }
  }

  if (json_doc.HasMember("view_id")) {
    if (json_doc["view_id"].IsString()) {
      result->set_view_id(json_doc["view_id"].GetString());
    } else {
      return stdx::make_unexpected(
          "Invalid json type for field 'view_id', expected 'string' got " +
          std::to_string(json_doc["view_id"].GetType()));
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
    // those we currently not use, if needed add a function encoding json
    // string to the selected message type
    case Mysqlx::Notice::Frame_Type_WARNING:
    case Mysqlx::Notice::Frame_Type_SESSION_VARIABLE_CHANGED:
    case Mysqlx::Notice::Frame_Type_SESSION_STATE_CHANGED:
    default:
      return stdx::make_unexpected("Unsupported notice id: " +
                                   std::to_string(id));
  }
}

void MySQLXProtocol::send_async_notice(const AsyncNotice &async_notice) {
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

  send_message(
      xcl::XProtocol::Server_message_type_id::ServerMessages_Type_NOTICE,
      notice_frame);
}

MySQLServerMockSessionX::MySQLServerMockSessionX(
    MySQLXProtocol *protocol,
    std::unique_ptr<StatementReaderBase> statement_processor,
    const bool debug_mode)
    : MySQLServerMockSession(protocol, std::move(statement_processor),
                             debug_mode),
      async_notices_(this->json_reader_->get_async_notices()),
      protocol_{protocol} {}

bool MySQLServerMockSessionX::process_handshake() {
  xcl::XProtocol::Client_message_type_id out_msg_id;
  bool done = false;

  while (!done) {
    auto msg = protocol_->recv_single_message(&out_msg_id);
    switch (out_msg_id) {
      case Mysqlx::ClientMessages::CON_CAPABILITIES_SET: {
        auto *capab_msg =
            dynamic_cast<Mysqlx::Connection::CapabilitiesSet *>(msg.get());
        harness_assert(capab_msg != nullptr);
        bool tls_request = false;
        const auto capabilities = capab_msg->capabilities();
        for (int i = 0; i < capabilities.capabilities_size(); ++i) {
          const auto capability = capabilities.capabilities(i);
          if (capability.name() == "tls") tls_request = true;
        }

        // we do not support TLS so if the client requested it
        // we need to reject it
        if (tls_request) {
          protocol_->send_error(ER_X_CAPABILITIES_PREPARE_FAILED,
                                "Capability prepare failed for tls");
        } else {
          Mysqlx::Ok ok_msg;
          protocol_->send_message(Mysqlx::ServerMessages::OK, ok_msg);
        }
        break;
      }
      case Mysqlx::ClientMessages::CON_CAPABILITIES_GET: {
        Mysqlx::Connection::Capabilities msg_capab;
        protocol_->send_message(Mysqlx::ServerMessages::CONN_CAPABILITIES,
                                msg_capab);
        break;
      }
      case Mysqlx::ClientMessages::SESS_AUTHENTICATE_START: {
        Mysqlx::Session::AuthenticateContinue msg_auth_cont;
        msg_auth_cont.set_auth_data("01234567890123456789");
        protocol_->send_message(
            Mysqlx::ServerMessages::SESS_AUTHENTICATE_CONTINUE, msg_auth_cont);
        break;
      }
      case Mysqlx::ClientMessages::SESS_AUTHENTICATE_CONTINUE: {
        Mysqlx::Session::AuthenticateOk msg_auth_ok;
        protocol_->send_message(Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK,
                                msg_auth_ok);
        done = true;
        break;
      }
      case Mysqlx::ClientMessages::CON_CLOSE: {
        return false;
      }
      default:
        done = true;
        break;
    }
  }

  return true;
}

bool MySQLServerMockSessionX::process_statements() {
  const auto kTimerResolution = std::chrono::milliseconds(10);
  const auto start_time = std::chrono::system_clock::now();

  while (!killed()) {
    send_due_async_notices(start_time);
    auto readable_res = protocol_->socket_has_data(kTimerResolution);

    if (!readable_res) {
      // got terminated by the mainloop.
      kill();
      continue;
    } else if (readable_res.value() == false) {
      // no data yet, wait a bit more
      continue;
    }

    xcl::XProtocol::Client_message_type_id out_msg_id;
    auto msg = protocol_->recv_single_message(&out_msg_id);
    switch (out_msg_id) {
      case Mysqlx::ClientMessages::SQL_STMT_EXECUTE: {
        auto *msg_stmt_execute =
            dynamic_cast<Mysqlx::Sql::StmtExecute *>(msg.get());
        harness_assert(msg_stmt_execute != nullptr);
        const auto statement_received = msg_stmt_execute->stmt();
        try {
          json_reader_->handle_statement(statement_received, protocol_);
        } catch (const std::exception &e) {
          // handling statement failed. Return the error to the client
          std::this_thread::sleep_for(json_reader_->get_default_exec_time());
          protocol_->send_error(
              1064, std::string("executing statement failed: ") + e.what());

          // assume the connection is broken
          return true;
        }

      } break;

      case Mysqlx::ClientMessages::CON_CLOSE: {
        log_info("received QUIT command from the client");
        return true;
      }

      default:
        log_error("received unsupported message from the x-client: %d",
                  static_cast<int>(out_msg_id));

        std::this_thread::sleep_for(json_reader_->get_default_exec_time());
        protocol_->send_error(
            1064, "Unsupported command: " + std::to_string(out_msg_id));
    }
  }

  return true;
}

void MySQLXProtocol::send_error(uint16_t error_code,
                                const std::string &error_msg,
                                const std::string &sql_state) {
  Mysqlx::Error err_msg;
  protocol_encoder_.encode_error(err_msg, error_code, error_msg, sql_state);

  send_message(Mysqlx::ServerMessages::ERROR, err_msg);
}

void MySQLXProtocol::send_ok(
    const uint64_t /*affected_rows*/,  // TODO: notice with this data?
    const uint64_t /*last_insert_id*/, const uint16_t /*server_status*/,
    const uint16_t /*warning_count*/) {
  Mysqlx::Sql::StmtExecuteOk ok_msg;
  send_message(Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK, ok_msg);
}

void MySQLXProtocol::send_resultset(const ResultsetResponse &response,
                                    const std::chrono::microseconds delay_ms) {
  std::this_thread::sleep_for(delay_ms);

  for (const auto &column : response.columns) {
    Mysqlx::Resultset::ColumnMetaData metadata_msg;
    protocol_encoder_.encode_metadata(metadata_msg, column);
    send_message(Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA,
                 metadata_msg);
  }

  for (size_t i = 0; i < response.rows.size(); ++i) {
    auto row = response.rows[i];
    if (response.columns.size() != row.size()) {
      throw std::runtime_error(
          std::string("columns_info.size() != row_values.size() ") +
          std::to_string(response.columns.size()) + std::string("!=") +
          std::to_string(row.size()));
    }
    Mysqlx::Resultset::Row row_msg;
    for (const auto &field : row) {
      const bool is_null = !field;
      protocol_encoder_.encode_row_field(
          row_msg,
          protocol_encoder_.column_type_to_x(response.columns[i].type()),
          field.value(), is_null);
    }
    send_message(Mysqlx::ServerMessages::RESULTSET_ROW, row_msg);
  }

  Mysqlx::Resultset::FetchDone fetch_done_msg;
  send_message(Mysqlx::ServerMessages::RESULTSET_FETCH_DONE, fetch_done_msg);
  send_ok();
}

}  // namespace server_mock
