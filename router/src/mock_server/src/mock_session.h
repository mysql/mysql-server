/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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
      std::unique_ptr<StatementReaderBase> statement_processor,
      const bool debug_mode)
      : json_reader_{std::move(statement_processor)}, debug_mode_{debug_mode} {}

  virtual ~MySQLServerMockSession() = default;

  virtual void run() = 0;

  virtual void cancel() = 0;

  bool debug_mode() const { return debug_mode_; }

  void disconnector(std::function<void()> func) {
    disconnector_ = std::move(func);
  }

  void disconnect() {
    if (disconnector_) disconnector_();
  }

 protected:
  std::unique_ptr<StatementReaderBase> json_reader_;

 private:
  bool debug_mode_;

  std::function<void()> disconnector_;
};

}  // namespace server_mock

#endif  // MYSQLD_MOCK_MOCK_SESSION_INCLUDED
