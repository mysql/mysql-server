/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLD_MOCK_DUKTAPE_STATEMENT_READER_INCLUDED
#define MYSQLD_MOCK_DUKTAPE_STATEMENT_READER_INCLUDED

#include <map>
#include <string>

#include "mysqlrouter/mock_server_global_scope.h"
#include "statement_reader.h"

namespace server_mock {
class DuktapeStatementReader : public StatementReaderBase {
 public:
  DuktapeStatementReader(const std::string &filename,
                         const std::string &module_prefix,
                         std::map<std::string, std::string> session_data,
                         std::shared_ptr<MockServerGlobalScope> shared_globals);

  StatementAndResponse handle_statement(const std::string &statement) override;

  std::chrono::microseconds get_default_exec_time() override;

  ~DuktapeStatementReader();

 private:
  struct Pimpl;
  std::unique_ptr<Pimpl> pimpl_;
  std::shared_ptr<MockServerGlobalScope> shared_;
};
}  // namespace server_mock

#endif
