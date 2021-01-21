/*
  Copyright (c) 2017, 2021, Oracle and/or its affiliates.

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

#ifndef MYSQLD_MOCK_X_MOCK_SESSION_INCLUDED
#define MYSQLD_MOCK_X_MOCK_SESSION_INCLUDED

#include <memory>
#include <mutex>
#include <set>

#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql_server_mock.h"
#include "router/src/mock_server/src/mock_session.h"
#include "x_protocol_decoder.h"
#include "x_protocol_encoder.h"

namespace server_mock {

class MySQLXProtocol : public ProtocolBase {
 public:
  using ProtocolBase::ProtocolBase;

  stdx::expected<std::tuple<uint8_t, size_t>, std::error_code> recv_header(
      const net::const_buffer &buf);

  std::unique_ptr<xcl::XProtocol::Message> recv_single_message(
      xcl::XProtocol::Client_message_type_id *out_msg_id);

  // throws std::system_error
  void send_error(const uint16_t error_code, const std::string &error_msg,
                  const std::string &sql_state = "HY000") override;

  // throws std::system_error
  void send_ok(const uint64_t affected_rows = 0,
               const uint64_t last_insert_id = 0,
               const uint16_t server_status = 0,
               const uint16_t warning_count = 0) override;

  // throws std::system_error
  void send_resultset(const ResultsetResponse &response,
                      const std::chrono::microseconds delay_ms) override;

  void send_message(const xcl::XProtocol::Server_message_type_id msg_id,
                    const xcl::XProtocol::Message &msg);

  void send_async_notice(const AsyncNotice &async_notice);

  Mysqlx::Notice::Frame notice_frame;
  stdx::expected<std::unique_ptr<xcl::XProtocol::Message>, std::string>
  gr_state_changed_from_json(const std::string &json_string);

  stdx::expected<std::unique_ptr<xcl::XProtocol::Message>, std::string>
  get_notice_message(const unsigned id, const std::string &payload);

 private:
  XProtocolEncoder protocol_encoder_;
  XProtocolDecoder protocol_decoder_;
};

class MySQLServerMockSessionX : public MySQLServerMockSession {
 public:
  MySQLServerMockSessionX(
      MySQLXProtocol *protocol,
      std::unique_ptr<StatementReaderBase> statement_processor,
      const bool debug_mode, bool with_tls);

  /**
   * process the handshake of the current connection.
   *
   * @throws std::system_error
   * @returns handshake-success
   * @retval true handshake succeeded
   * @retval false handshake failed, close connection
   */
  bool process_handshake() override;

  /**
   * process the statements of the current connection.
   *
   * @pre connection must be authenticated with process_handshake() first
   *
   * @throws std::system_error, std::runtime_error
   * @returns handshake-success
   * @retval true handshake succeeded
   * @retval false handshake failed, close connection
   */
  bool process_statements() override;

  void send_due_async_notices(
      const std::chrono::time_point<std::chrono::system_clock> &start_time);

 private:
  std::vector<AsyncNotice> async_notices_;
  MySQLXProtocol *protocol_;

  bool with_tls_{false};
};

}  // namespace server_mock

#endif  // MYSQLD_MOCK_X_MOCK_SESSION_INCLUDED
