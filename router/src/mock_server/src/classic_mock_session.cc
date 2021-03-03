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

#include "classic_mock_session.h"

#include <array>
#include <chrono>
#include <memory>
#include <system_error>
#include <thread>

#include "harness_assert.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"
#include "mysqlrouter/classic_protocol.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_message.h"

IMPORT_LOG_FUNCTIONS()

namespace server_mock {

stdx::expected<size_t, std::error_code> MySQLClassicProtocol::read_packet(
    std::vector<uint8_t> &payload) {
  std::array<char, 4> hdr_buf_storage;

  net::mutable_buffer hdr_buf = net::buffer(hdr_buf_storage);

  read_buffer(hdr_buf);

  auto decode_res = classic_protocol::decode<classic_protocol::frame::Header>(
      net::buffer(hdr_buf_storage), {});
  if (!decode_res) {
    return decode_res.get_unexpected();
  }

  auto hdr_frame = decode_res.value();

  auto hdr = hdr_frame.second;

  if (hdr.payload_size() == 0xffffff) {
    return stdx::make_unexpected(
        make_error_code(std::errc::operation_not_supported));
  }

  seq_no_ = hdr.seq_id() + 1;

  payload.resize(hdr.payload_size());
  net::mutable_buffer payload_buf = net::buffer(payload);

  read_buffer(payload_buf);

  return payload.size();
}

void MySQLClassicProtocol::send_packet(const std::vector<uint8_t> &payload) {
  send_buffer(net::buffer(payload));
}

bool MySQLServerMockSessionClassic::process_handshake() {
  bool is_first_packet = true;

  while (!killed()) {
    std::vector<uint8_t> payload;
    if (!is_first_packet) {
      auto read_res = protocol_->read_packet(payload);
      if (!read_res) {
        throw std::system_error(read_res.error());
      }
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
  while (!killed()) {
    std::vector<uint8_t> payload;

    auto read_res = protocol_->read_packet(payload);
    if (!read_res) {
      throw std::system_error(read_res.error());
    }

    if (payload.size() == 0) {
      throw std::system_error(make_error_code(std::errc::bad_message));
    }

    auto cmd = payload[0];
    switch (cmd) {
      case classic_protocol::Codec<
          classic_protocol::message::client::Query>::cmd_byte(): {
        // skip the first
        std::string statement_received(std::next(payload.begin()),
                                       payload.end());

        try {
          handle_statement(json_reader_->handle_statement(statement_received));
        } catch (const std::exception &e) {
          // handling statement failed. Return the error to the client
          std::this_thread::sleep_for(json_reader_->get_default_exec_time());
          log_error("executing statement failed: %s", e.what());
          protocol_->send_error(
              ER_PARSE_ERROR,
              std::string("executing statement failed: ") + e.what());

          // assume the connection is broken
          return true;
        }
      } break;
      case classic_protocol::Codec<
          classic_protocol::message::client::Quit>::cmd_byte():
        log_info("received QUIT command from the client");
        return true;
      default:
        log_info("received unsupported command from the client: %d", cmd);
        std::this_thread::sleep_for(json_reader_->get_default_exec_time());
        protocol_->send_error(ER_PARSE_ERROR,
                              "Unsupported command: " + std::to_string(cmd));
    }
  }

  return true;
}

void MySQLClassicProtocol::send_auth_fast_message() {
  std::vector<uint8_t> buf;

  classic_protocol::capabilities::value_type shared_caps{
      classic_protocol::capabilities::protocol_41};

  auto encode_res = classic_protocol::encode<
      classic_protocol::frame::Frame<classic_protocol::wire::FixedInt<1>>>(
      {seq_no_++, {3}}, shared_caps, net::dynamic_buffer(buf));

  send_packet(buf);
}

void MySQLClassicProtocol::send_auth_switch_message(
    const AuthSwitch *auth_switch_resp) {
  std::vector<uint8_t> buf;

  classic_protocol::capabilities::value_type shared_caps{
      classic_protocol::capabilities::protocol_41 |
      classic_protocol::capabilities::plugin_auth};

  auto encode_res = classic_protocol::encode<classic_protocol::frame::Frame<
      classic_protocol::message::server::AuthMethodSwitch>>(
      {seq_no_++,
       {auth_switch_resp->method(), auth_switch_resp->data() + '\0'}},
      shared_caps, net::dynamic_buffer(buf));

  send_packet(buf);
}

void MySQLClassicProtocol::send_greeting(const Greeting *greeting_resp) {
  std::vector<uint8_t> buf;

  classic_protocol::capabilities::value_type shared_caps{
      classic_protocol::capabilities::protocol_41};

  auto encode_res = classic_protocol::encode<classic_protocol::frame::Frame<
      classic_protocol::message::server::Greeting>>(
      {seq_no_++,
       {0x0a, greeting_resp->server_version(), greeting_resp->connection_id(),
        greeting_resp->auth_data(), greeting_resp->capabilities(),
        greeting_resp->character_set(), greeting_resp->status_flags(),
        greeting_resp->auth_method()}},
      shared_caps, net::dynamic_buffer(buf));

  send_packet(buf);
}

bool MySQLServerMockSessionClassic::handle_handshake(
    const HandshakeResponse &response) {
  using ResponseType = HandshakeResponse::ResponseType;

  std::this_thread::sleep_for(response.exec_time);

  switch (response.response_type) {
    case ResponseType::GREETING: {
      auto *greeting_resp = dynamic_cast<Greeting *>(response.response.get());
      harness_assert(greeting_resp);

      protocol_->send_greeting(greeting_resp);
    } break;
    case ResponseType::AUTH_SWITCH: {
      auto *auth_switch_resp =
          dynamic_cast<AuthSwitch *>(response.response.get());
      harness_assert(auth_switch_resp);

      protocol_->send_auth_switch_message(auth_switch_resp);
    } break;
    case ResponseType::AUTH_FAST: {
      // sha256-fast-auth is
      // - 0x03
      // - ok
      protocol_->send_auth_fast_message();

      protocol_->send_ok(0, 0, 0, 0);

      return true;
    }
    case ResponseType::OK: {
      auto *ok_resp = dynamic_cast<OkResponse *>(response.response.get());
      harness_assert(ok_resp);

      protocol_->send_ok(0, ok_resp->last_insert_id, 0, ok_resp->warning_count);

      return true;
    }
    case ResponseType::ERROR: {
      auto *err_resp = dynamic_cast<ErrorResponse *>(response.response.get());
      harness_assert(err_resp);
      protocol_->send_error(err_resp->code, err_resp->msg);

      return true;
    }
    default:
      throw std::runtime_error(
          "Unsupported command in handle_handshake(): " +
          std::to_string(static_cast<int>(response.response_type)));
  }

  return false;
}

void MySQLClassicProtocol::send_error(const uint16_t error_code,
                                      const std::string &error_msg,
                                      const std::string &sql_state) {
  std::vector<uint8_t> buf;
  auto encode_res = classic_protocol::encode<
      classic_protocol::frame::Frame<classic_protocol::message::server::Error>>(
      {seq_no_++, {error_code, error_msg, sql_state}},
      {classic_protocol::capabilities::protocol_41}, net::dynamic_buffer(buf));

  send_packet(buf);
}

void MySQLClassicProtocol::send_ok(const uint64_t affected_rows,
                                   const uint64_t last_insert_id,
                                   const uint16_t server_status,
                                   const uint16_t warning_count) {
  std::vector<uint8_t> buf;
  auto encode_res = classic_protocol::encode<
      classic_protocol::frame::Frame<classic_protocol::message::server::Ok>>(
      {seq_no_++,
       {affected_rows, last_insert_id, server_status, warning_count}},
      {classic_protocol::capabilities::protocol_41}, net::dynamic_buffer(buf));

  send_packet(buf);
}

stdx::expected<std::string, void> make_field(
    std::pair<bool, std::string> const &v) {
  if (v.first) {
    return v.second;
  } else {
    return stdx::make_unexpected();
  }
}

void MySQLClassicProtocol::send_resultset(
    const ResultsetResponse &response,
    const std::chrono::microseconds delay_ms) {
  std::vector<uint8_t> buf;

  classic_protocol::capabilities::value_type shared_caps{
      classic_protocol::capabilities::protocol_41};

  auto encode_res = classic_protocol::encode<
      classic_protocol::frame::Frame<classic_protocol::wire::VarInt>>(
      {seq_no_++, {static_cast<long>(response.columns.size())}}, shared_caps,
      net::dynamic_buffer(buf));

  for (const auto &column : response.columns) {
    encode_res = classic_protocol::encode<classic_protocol::frame::Frame<
        classic_protocol::message::server::ColumnMeta>>(
        {seq_no_++,
         {column.catalog, column.schema, column.table, column.orig_table,
          column.name, column.orig_name, column.character_set, column.length,
          static_cast<uint8_t>(column.type), column.flags, column.decimals}},
        shared_caps, net::dynamic_buffer(buf));
  }

  encode_res = classic_protocol::encode<
      classic_protocol::frame::Frame<classic_protocol::message::server::Eof>>(
      {seq_no_++, {}}, shared_caps, net::dynamic_buffer(buf));

  std::this_thread::sleep_for(delay_ms);
  send_packet(buf);
  buf.clear();

  for (size_t i = 0; i < response.rows.size(); ++i) {
    std::vector<stdx::expected<std::string, void>> fields;

    auto const &row = response.rows[i];

    for (size_t f{}; f < response.columns.size(); ++f) {
      fields.push_back(make_field(row[f]));
    }

    encode_res = classic_protocol::encode<
        classic_protocol::frame::Frame<classic_protocol::message::server::Row>>(
        {seq_no_++, {fields}}, shared_caps, net::dynamic_buffer(buf));

    send_packet(buf);
    buf.clear();
  }

  encode_res = classic_protocol::encode<
      classic_protocol::frame::Frame<classic_protocol::message::server::Eof>>(
      {seq_no_++, {}}, shared_caps, net::dynamic_buffer(buf));

  send_packet(buf);
  buf.clear();
}

}  // namespace server_mock
