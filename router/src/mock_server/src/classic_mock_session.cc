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

#include "classic_mock_session.h"

#include <thread>
#include "mysql_protocol_utils.h"

#include "mysql/harness/logging/logging.h"
#include "mysqld_error.h"
IMPORT_LOG_FUNCTIONS()

namespace server_mock {

bool MySQLServerMockSessionClassic::process_handshake() {
  using namespace mysql_protocol;

  bool is_first_packet = true;

  while (!killed_) {
    std::vector<uint8_t> payload;
    if (!is_first_packet) {
      protocol_decoder_.read_message(client_socket_);
      seq_no_ = protocol_decoder_.packet_seq() + 1;
      payload = protocol_decoder_.get_payload();
    }
    is_first_packet = false;
    if (true == handle_handshake(json_reader_->handle_handshake(payload))) {
      // handshake is done
      return true;
    }
  }

  return false;
}

bool MySQLServerMockSessionClassic::process_statements() {
  using mysql_protocol::Command;

  while (!killed_) {
    protocol_decoder_.read_message(client_socket_);
    seq_no_ = protocol_decoder_.packet_seq() + 1;
    auto cmd = protocol_decoder_.get_command_type();
    switch (cmd) {
      case Command::QUERY: {
        std::string statement_received = protocol_decoder_.get_statement();

        try {
          handle_statement(json_reader_->handle_statement(statement_received));
        } catch (const std::exception &e) {
          // handling statement failed. Return the error to the client
          std::this_thread::sleep_for(json_reader_->get_default_exec_time());
          send_error(ER_PARSE_ERROR,
                     std::string("executing statement failed: ") + e.what());

          // assume the connection is broken
          return true;
        }
      } break;
      case Command::QUIT:
        log_info("received QUIT command from the client");
        return true;
      default:
        std::cerr << "received unsupported command from the client: "
                  << static_cast<int>(cmd) << "\n";
        std::this_thread::sleep_for(json_reader_->get_default_exec_time());
        send_error(ER_PARSE_ERROR,
                   "Unsupported command: " + std::to_string(cmd));
    }
  }

  return true;
}

bool MySQLServerMockSessionClassic::handle_handshake(
    const HandshakeResponse &response) {
  using ResponseType = HandshakeResponse::ResponseType;

  std::this_thread::sleep_for(response.exec_time);

  switch (response.response_type) {
    case ResponseType::GREETING: {
      Greeting *greeting_resp =
          dynamic_cast<Greeting *>(response.response.get());
      harness_assert(greeting_resp);

      send_packet(
          client_socket_,
          protocol_encoder_.encode_greetings_message(
              seq_no_++, greeting_resp->server_version(),
              greeting_resp->connection_id(), greeting_resp->auth_data(),
              greeting_resp->capabilities(), greeting_resp->auth_method(),
              greeting_resp->character_set(), greeting_resp->status_flags()));
    } break;
    case ResponseType::AUTH_SWITCH: {
      AuthSwitch *auth_switch_resp =
          dynamic_cast<AuthSwitch *>(response.response.get());
      harness_assert(auth_switch_resp);

      send_packet(client_socket_, protocol_encoder_.encode_auth_switch_message(
                                      seq_no_++, auth_switch_resp->method(),
                                      auth_switch_resp->data()));
    } break;
    case ResponseType::AUTH_FAST: {
      // sha256-fast-auth is
      // - 0x03
      // - ok
      send_packet(client_socket_,
                  protocol_encoder_.encode_auth_fast_message(seq_no_++));

      send_ok(0, 0, 0, 0);

      return true;
    }
    case ResponseType::OK: {
      OkResponse *ok_resp = dynamic_cast<OkResponse *>(response.response.get());
      harness_assert(ok_resp);

      send_ok(0, ok_resp->last_insert_id, 0, ok_resp->warning_count);

      return true;
    }
    case ResponseType::ERROR: {
      ErrorResponse *err_resp =
          dynamic_cast<ErrorResponse *>(response.response.get());
      harness_assert(err_resp);
      send_error(err_resp->code, err_resp->msg);

      return true;
    }
    default:
      throw std::runtime_error(
          "Unsupported command in handle_handshake(): " +
          std::to_string(static_cast<int>(response.response_type)));
  }

  return false;
}

void MySQLServerMockSessionClassic::send_error(const uint16_t error_code,
                                               const std::string &error_msg,
                                               const std::string &sql_state) {
  auto buf = protocol_encoder_.encode_error_message(seq_no_++, error_code,
                                                    sql_state, error_msg);
  send_packet(client_socket_, buf);
}

void MySQLServerMockSessionClassic::send_ok(const uint64_t affected_rows,
                                            const uint64_t last_insert_id,
                                            const uint16_t server_status,
                                            const uint16_t warning_count) {
  auto buf = protocol_encoder_.encode_ok_message(
      seq_no_++, affected_rows, last_insert_id, server_status, warning_count);
  send_packet(client_socket_, buf);
}

void MySQLServerMockSessionClassic::send_resultset(
    const ResultsetResponse &response,
    const std::chrono::microseconds delay_ms) {
  auto buf = protocol_encoder_.encode_columns_number_message(
      seq_no_++, response.columns.size());
  std::this_thread::sleep_for(delay_ms);
  send_packet(client_socket_, buf);
  for (const auto &column : response.columns) {
    auto col_buf =
        protocol_encoder_.encode_column_meta_message(seq_no_++, column);
    send_packet(client_socket_, col_buf);
  }
  buf = protocol_encoder_.encode_eof_message(seq_no_++);
  send_packet(client_socket_, buf);

  for (size_t i = 0; i < response.rows.size(); ++i) {
    auto res_buf = protocol_encoder_.encode_row_message(
        seq_no_++, response.columns, response.rows[i]);
    send_packet(client_socket_, res_buf);
  }
  buf = protocol_encoder_.encode_eof_message(seq_no_++);
  send_packet(client_socket_, buf);
}

MySQLServerMockSessionClassic::MySQLServerMockSessionClassic(
    const socket_t client_sock,
    std::unique_ptr<StatementReaderBase> statement_processor,
    const bool debug_mode)
    : MySQLServerMockSession(client_sock, std::move(statement_processor),
                             debug_mode),
      protocol_decoder_{&read_packet} {}

}  // namespace server_mock
