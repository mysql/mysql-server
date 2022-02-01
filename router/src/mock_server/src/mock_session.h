/*
  Copyright (c) 2019, 2021, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/net_ts/internet.h"
#include "statement_reader.h"

namespace server_mock {

class MySQLServerMockSession {
 public:
  MySQLServerMockSession(
      ProtocolBase *protocol,
      std::unique_ptr<StatementReaderBase> statement_processor,
      const bool debug_mode);

  virtual ~MySQLServerMockSession() = default;

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

  // throws std::system_error, std::runtime_error
  void run();

  void kill() noexcept { killed_ = true; }

  bool killed() const { return killed_; }

  bool debug_mode() const { return debug_mode_; }

 protected:
  std::unique_ptr<StatementReaderBase> json_reader_;

 private:
  bool killed_{false};
  ProtocolBase *protocol_;
  bool debug_mode_;
};

}  // namespace server_mock

#endif  // MYSQLD_MOCK_MOCK_SESSION_INCLUDED
