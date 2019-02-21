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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Implements the interface defined in
#include "sql/ndb_ddl_transaction_ctx.h"

void Ndb_DDL_transaction_ctx::log_create_table(const std::string &path_name) {
  log_ddl_stmt(Ndb_DDL_stmt::CREATE_TABLE, path_name);
}

void Ndb_DDL_transaction_ctx::log_rename_table(
    const std::string &old_db_name, const std::string &old_table_name,
    const std::string &new_db_name, const std::string &new_table_name,
    const std::string &from, const std::string &to) {
  log_ddl_stmt(Ndb_DDL_stmt::RENAME_TABLE, old_db_name, old_table_name,
               new_db_name, new_table_name, from, to);
}

void Ndb_DDL_transaction_ctx::commit() {
  DBUG_TRACE;
  DBUG_ASSERT(m_ddl_status == DDL_IN_PROGRESS);
  /* The schema changes would have been already committed internally to the NDB
     by the respective handler functions that made the change. So just update
     the status of the DDL and make note of the latest stmt on which the
     Server has requested a commit. */
  m_ddl_status = DDL_COMMITED;
  m_latest_committed_stmt = m_executed_ddl_stmts.size();
}

bool Ndb_DDL_transaction_ctx::rollback() {
  DBUG_TRACE;
  DBUG_ASSERT(m_ddl_status == DDL_IN_PROGRESS);

  bool result = true;
  m_ddl_status = DDL_ROLLED_BACK;
  /* Rollback all the uncommitted DDL statements in reverse order */
  for (auto it = m_executed_ddl_stmts.rbegin();
       it != (m_executed_ddl_stmts.rend() - m_latest_committed_stmt); ++it) {
    const Ndb_DDL_stmt &ddl_stmt = *it;
    switch (ddl_stmt.get_ddl_type()) {
      case Ndb_DDL_stmt::CREATE_TABLE:
        break;
      case Ndb_DDL_stmt::RENAME_TABLE:
        break;
      default:
        result = false;
        DBUG_ASSERT(false);
        break;
    }
  }

  return result;
}
