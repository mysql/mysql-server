/*
  Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/logging/logging.h"
IMPORT_LOG_FUNCTIONS()

#include "config.h"
#include "mysql_protocol_utils.h"
#include "mysqlxclient/xprotocol.h"

namespace server_mock {

struct MySQLServerMockSessionX::Impl {
  Impl(socket_t client_socket, const XProtocolDecoder &protocol_decoder)
      : client_socket_(client_socket), protocol_decoder_(protocol_decoder) {}

  bool recv_header(uint8_t *out_msg_id, std::size_t *out_buffer_size) {
    union {
      uint8_t header_buffer[5];
      uint32_t payload_size;
    };

    read_packet(client_socket_, &header_buffer[0], 5);

#ifdef WORDS_BIGENDIAN
    std::swap(header_buffer[0], header_buffer[3]);
    std::swap(header_buffer[1], header_buffer[2]);
#endif

    *out_buffer_size = payload_size - 1;
    *out_msg_id = header_buffer[4];

    return true;
  }

  std::unique_ptr<xcl::XProtocol::Message> recv_single_message(
      xcl::XProtocol::Client_message_type_id *out_msg_id) {
    std::size_t payload_size = 0;
    uint8_t header_msg_id;

    const bool res = recv_header(&header_msg_id, &payload_size);
    if (!res) return nullptr;

    std::unique_ptr<std::uint8_t[]> allocated_payload_buffer;
    std::uint8_t *payload = nullptr;
    if (payload_size > 0) {
      allocated_payload_buffer.reset(new uint8_t[payload_size]);
      payload = allocated_payload_buffer.get();
      read_packet(client_socket_, payload, payload_size);
    }

    *out_msg_id =
        static_cast<xcl::XProtocol::Client_message_type_id>(header_msg_id);

    return protocol_decoder_.decode_message(header_msg_id, payload,
                                            payload_size);
  }

  xcl::XError send(const xcl::XProtocol::Server_message_type_id msg_id,
                   const xcl::XProtocol::Message &msg) {
    std::string msg_buffer;
    const std::uint8_t header_size = 5;
    const std::size_t msg_size = msg.ByteSize();
    msg_buffer.resize(msg_size + header_size);

    if (!msg.SerializeToArray(&msg_buffer[0] + header_size, msg_size)) {
      return xcl::XError{CR_MALFORMED_PACKET,
                         "Failed to serialize the message"};
    }

    const auto msg_size_to_buffer = static_cast<std::uint32_t>(msg_size + 1);

    memcpy(&msg_buffer[0], &msg_size_to_buffer, sizeof(std::uint32_t));
#ifdef WORDS_BIGENDIAN
    std::swap(msg_buffer[0], msg_buffer[3]);
    std::swap(msg_buffer[1], msg_buffer[2]);
#endif
    msg_buffer[4] = msg_id;

    send_packet(client_socket_,
                reinterpret_cast<const std::uint8_t *>(msg_buffer.data()),
                msg_buffer.size());

    return xcl::XError();
  }

 private:
  socket_t client_socket_;
  const XProtocolDecoder &protocol_decoder_;
};

MySQLServerMockSessionX::MySQLServerMockSessionX(
    const socket_t client_sock,
    std::unique_ptr<StatementReaderBase> statement_processor,
    const bool debug_mode)
    : MySQLServerMockSession(client_sock, std::move(statement_processor),
                             debug_mode),
      impl_(new MySQLServerMockSessionX::Impl(client_sock, protocol_decoder_)) {
}

MySQLServerMockSessionX::~MySQLServerMockSessionX() = default;

bool MySQLServerMockSessionX::process_handshake() {
  xcl::XProtocol::Client_message_type_id out_msg_id;
  bool done = false;
  while (!done) {
    auto msg = impl_->recv_single_message(&out_msg_id);
    switch (out_msg_id) {
      case Mysqlx::ClientMessages::CON_CAPABILITIES_SET: {
        Mysqlx::Ok ok_msg;
        impl_->send(Mysqlx::ServerMessages::OK, ok_msg);
        break;
      }
      case Mysqlx::ClientMessages::SESS_AUTHENTICATE_START: {
        Mysqlx::Session::AuthenticateContinue msg_auth_cont;
        msg_auth_cont.set_auth_data("abcd");
        impl_->send(Mysqlx::ServerMessages::SESS_AUTHENTICATE_CONTINUE,
                    msg_auth_cont);
        break;
      }
      case Mysqlx::ClientMessages::SESS_AUTHENTICATE_CONTINUE: {
        Mysqlx::Session::AuthenticateOk msg_auth_ok;
        impl_->send(Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK, msg_auth_ok);
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
  while (!killed_) {
    xcl::XProtocol::Client_message_type_id out_msg_id;
    auto msg = impl_->recv_single_message(&out_msg_id);
    switch (out_msg_id) {
      case Mysqlx::ClientMessages::SQL_STMT_EXECUTE: {
        Mysqlx::Sql::StmtExecute *msg_stmt_execute =
            dynamic_cast<Mysqlx::Sql::StmtExecute *>(msg.get());
        const auto statement_received = msg_stmt_execute->stmt();

        try {
          handle_statement(json_reader_->handle_statement(statement_received));
        } catch (const std::exception &e) {
          // handling statement failed. Return the error to the client
          std::this_thread::sleep_for(json_reader_->get_default_exec_time());
          send_error(1064,
                     std::string("executing statement failed: ") + e.what());

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
        send_error(1064, "Unsupported command: " + std::to_string(out_msg_id));
    }
  }

  return true;
}

void MySQLServerMockSessionX::send_error(uint16_t error_code,
                                         const std::string &error_msg,
                                         const std::string &sql_state) {
  Mysqlx::Error err_msg;
  protocol_encoder_.encode_error(err_msg, error_code, error_msg, sql_state);

  impl_->send(Mysqlx::ServerMessages::ERROR, err_msg);
}

void MySQLServerMockSessionX::send_ok(
    const uint64_t /*affected_rows*/,  // TODO: notice with this data?
    const uint64_t /*last_insert_id*/, const uint16_t /*server_status*/,
    const uint16_t /*warning_count*/) {
  Mysqlx::Ok ok_msg;
  impl_->send(Mysqlx::ServerMessages::OK, ok_msg);
}

void MySQLServerMockSessionX::send_resultset(
    const ResultsetResponse &response,
    const std::chrono::microseconds delay_ms) {
  std::this_thread::sleep_for(delay_ms);

  for (const auto &column : response.columns) {
    Mysqlx::Resultset::ColumnMetaData metadata_msg;
    protocol_encoder_.encode_metadata(metadata_msg, column);
    impl_->send(Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA,
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
      const bool is_null = !field.first;
      protocol_encoder_.encode_row_field(
          row_msg, protocol_encoder_.column_type_to_x(response.columns[i].type),
          field.second, is_null);
    }
    impl_->send(Mysqlx::ServerMessages::RESULTSET_ROW, row_msg);
  }

  Mysqlx::Resultset::FetchDone fetch_done_msg;
  impl_->send(Mysqlx::ServerMessages::RESULTSET_FETCH_DONE, fetch_done_msg);
  Mysqlx::Sql::StmtExecuteOk sql_ok_msg;
  impl_->send(Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK, sql_ok_msg);
}

}  // namespace server_mock
