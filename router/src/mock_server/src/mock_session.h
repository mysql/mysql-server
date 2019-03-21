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

#ifndef MYSQLD_MOCK_MOCK_SESSION_INCLUDED
#define MYSQLD_MOCK_MOCK_SESSION_INCLUDED

#include "statement_reader.h"

namespace server_mock {

class MySQLServerMockSession {
 public:
  MySQLServerMockSession(
      const socket_t client_sock,
      std::unique_ptr<StatementReaderBase> statement_processor,
      const bool debug_mode);

  // factory method
  // throws std::runtime_error
  static std::unique_ptr<MySQLServerMockSession> create_session(
      const std::string &protocol, const socket_t client_socket,
      std::unique_ptr<StatementReaderBase> statement_processor,
      bool debug_mode);

  virtual ~MySQLServerMockSession();

  /**
   * process the handshake of the current connection.
   *
   * @throws std::system_error
   * @returns handshake-success
   * @retval true handshake succeeded
   * @retval false handshake failed, close connection
   */
  virtual bool process_handshake() = 0;

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
  virtual bool process_statements() = 0;

  // throws std::system_error
  virtual void send_error(const uint16_t error_code,
                          const std::string &error_msg,
                          const std::string &sql_state = "HY000") = 0;

  // throws std::system_error
  virtual void send_ok(const uint64_t affected_rows = 0,
                       const uint64_t last_insert_id = 0,
                       const uint16_t server_status = 0,
                       const uint16_t warning_count = 0) = 0;

  // throws std::system_error
  virtual void send_resultset(const ResultsetResponse &response,
                              const std::chrono::microseconds delay_ms) = 0;

  // throws std::system_error, std::runtime_error
  void run();

  void kill() noexcept { killed_ = true; }

 protected:
  // throws std::system_error, std::runtime_error
  virtual void handle_statement(const StatementResponse &statement);

  bool killed_{false};
  socket_t client_socket_;
  std::unique_ptr<StatementReaderBase> json_reader_;
  bool debug_mode_;
};

}  // namespace server_mock

#endif  // MYSQLD_MOCK_MOCK_SESSION_INCLUDED
