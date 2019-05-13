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

#include "sql/ndb_ddl_definitions.h"
#include "sql/ndb_name_util.h"
#include "sql/ndb_schema_dist.h"
#include "sql/ndb_thd_ndb.h"

void Ndb_DDL_transaction_ctx::log_create_table(const std::string &path_name) {
  log_ddl_stmt(Ndb_DDL_stmt::CREATE_TABLE, path_name);
}

bool Ndb_DDL_transaction_ctx::rollback_create_table(
    const Ndb_DDL_stmt &ddl_stmt) {
  DBUG_TRACE;

  /* extract info from ddl_info */
  const std::vector<std::string> &ddl_info = ddl_stmt.get_info();
  DBUG_ASSERT(ddl_info.size() == 1);
  const char *path_name = ddl_info[0].c_str();
  char db_name[FN_HEADLEN];
  char table_name[FN_HEADLEN];
  ndb_set_dbname(path_name, db_name);
  ndb_set_tabname(path_name, table_name);

  /* Prepare schema client for rollback if required */
  Thd_ndb *thd_ndb = get_thd_ndb(m_thd);
  Ndb_schema_dist_client schema_dist_client(m_thd);
  bool schema_dist_prepared = false;
  if (ddl_stmt.has_been_distributed()) {
    /* The stmt was distributed.
       So rollback should be distributed too.
       Prepare the schema client */
    schema_dist_prepared = schema_dist_client.prepare(db_name, table_name);
    if (!schema_dist_prepared) {
      /* Report the error and just drop it locally */
      thd_ndb->push_warning(
          "Failed to distribute rollback to connected servers.");
    }
  }

  DBUG_PRINT("info",
             ("Rollback : Dropping table '%s.%s'", db_name, table_name));

  /* Drop the table created during this DDL execution */
  Ndb *ndb = thd_ndb->ndb;
  if (drop_table_impl(m_thd, ndb,
                      schema_dist_prepared ? &schema_dist_client : nullptr,
                      path_name, db_name, table_name)) {
    thd_ndb->push_warning("Failed to rollback after CREATE TABLE failure.");
    return false;
  }

  return true;
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
        result &= rollback_create_table(ddl_stmt);
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
