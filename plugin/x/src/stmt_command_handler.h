/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_STMT_COMMAND_HANDLER_H_
#define PLUGIN_X_SRC_STMT_COMMAND_HANDLER_H_

#include "plugin/x/ngs/include/ngs/error_code.h"
#include "plugin/x/ngs/include/ngs/interface/sql_session_interface.h"
#include "plugin/x/ngs/include/ngs/protocol_fwd.h"
#include "plugin/x/src/admin_cmd_handler.h"
#include "plugin/x/src/query_string_builder.h"

namespace xpl {

class Stmt_command_handler {
 public:
  explicit Stmt_command_handler(ngs::Session_interface *session)
      : m_session{session} {}

  ngs::Error_code execute(const Mysqlx::Sql::StmtExecute &msg);

 private:
  ngs::Error_code sql_stmt_execute(const Mysqlx::Sql::StmtExecute &msg);
  ngs::Error_code deprecated_admin_stmt_execute(
      const Mysqlx::Sql::StmtExecute &msg);
  ngs::Error_code admin_stmt_execute(const Mysqlx::Sql::StmtExecute &msg);

  Query_string_builder m_qb{1024};
  ngs::Session_interface *m_session;
  Admin_command_handler m_admin_handler{m_session};
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_STMT_COMMAND_HANDLER_H_
