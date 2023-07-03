/*
   Copyright (c) 2011, 2022, Oracle and/or its affiliates.

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

#include "storage/ndb/plugin/ndb_local_connection.h"
#include "storage/ndb/plugin/ndb_anyvalue.h"

#include "sql/mysqld.h"  // next_query_id()
#include "sql/sql_class.h"
#include "sql/sql_prepare.h"
#include "storage/ndb/plugin/ndb_log.h"

class Ndb_local_connection::Impl {
 public:
  explicit Impl(THD *thd_arg) : connection(thd_arg) {}
  Ed_connection connection;
};

Ndb_local_connection::Ndb_local_connection(THD *thd_arg)
    : saved_thd_server_id(thd_arg->server_id),
      saved_thd_options(thd_arg->variables.option_bits),
      m_thd(thd_arg),
      impl(std::make_unique<Impl>(thd_arg)) {
  assert(thd_arg);
  /*
    System(or daemon) threads report error to log file
    all other threads use push_warning
  */
  m_push_warnings = (thd_arg->get_command() != COM_DAEMON);
}

Ndb_local_connection::~Ndb_local_connection() {
  m_thd->server_id = saved_thd_server_id;
  m_thd->variables.option_bits = saved_thd_options;
}

static inline bool should_ignore_error(const uint *ignore_error_list,
                                       uint error) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("error: %u", error));
  const uint *ignore_error = ignore_error_list;
  while (*ignore_error != 0) {
    DBUG_PRINT("info", ("ignore_error: %u", *ignore_error));
    if (*ignore_error == error) {
      return true;
    }
    ignore_error++;
  }
  DBUG_PRINT("info", ("Don't ignore error"));
  return false;
}

void Ndb_local_connection::set_binlog_options(bool log_replica_updates,
                                              unsigned int op_anyvalue) {
  bool disable_binlog = false;

  if (ndbcluster_anyvalue_is_reserved(op_anyvalue)) {
    if (ndbcluster_anyvalue_is_nologging(op_anyvalue)) disable_binlog = true;
  } else {
    unsigned int req_server_id = ndbcluster_anyvalue_get_serverid(op_anyvalue);
    if (req_server_id != 0) {
      m_thd->server_id = req_server_id;
      if (!log_replica_updates) disable_binlog = true;
    }
  }

  if (disable_binlog) m_thd->variables.option_bits &= ~OPTION_BIN_LOG;
}

uint Ndb_local_connection::execute_query(const std::string &sql_query) {
  DBUG_TRACE;

  uint result = 0;
  const LEX_STRING sql_text{const_cast<char *>(sql_query.c_str()),
                            sql_query.length()};
  if (impl->connection.execute_direct(sql_text)) {
    /* Error occurred while executing the query */
    result = impl->connection.get_last_errno();
    assert(result);  // last_errno must have been set

    // catch some SQL parse errors in debug
    assert(result != ER_PARSE_ERROR && result != ER_EMPTY_QUERY);
  }
  return result;
}

bool Ndb_local_connection::check_query_error(const std::string &sql_query,
                                             uint last_errno,
                                             const uint *ignore_mysql_errors) {
  if (last_errno) {
    const char *last_errmsg = impl->connection.get_last_error();

    DBUG_PRINT("error", ("Query '%s' failed, error: '%d: %s'",
                         sql_query.c_str(), last_errno, last_errmsg));

    // catch some SQL parse errors in debug
    assert(last_errno != ER_PARSE_ERROR && last_errno != ER_EMPTY_QUERY);

    /* Check if it was a MySQL error that should be ignored */
    if (ignore_mysql_errors &&
        should_ignore_error(ignore_mysql_errors, last_errno)) {
      /* MySQL error suppressed -> return success */
      m_thd->clear_error();
      return false;
    }

    if (m_push_warnings) {
      // Append the error which caused the error to thd's warning list
      push_warning(m_thd, Sql_condition::SL_WARNING, last_errno, last_errmsg);
    } else {
      // Print the error to log file
      ndb_log_error("Query '%s' failed, error: %d: %s", sql_query.c_str(),
                    last_errno, last_errmsg);
    }

    return true;
  }

  return false;  // Success
}

/* Execute query, ignoring particular errors.
   The query may be written to the binlog.
*/
bool Ndb_local_connection::execute_query(const std::string &sql_query,
                                         const uint *ignore_mysql_errors) {
  uint result = execute_query(sql_query);
  return check_query_error(sql_query, result, ignore_mysql_errors);
}

/*
  Execute the query with even higher isolation than what execute_query
  provides to avoid that for example THD's status variables are changed.
  The query will not ever be written to binlog.
*/

bool Ndb_local_connection::execute_query_iso(const std::string &sql_query,
                                             const uint *ignore_mysql_errors) {
  /* Don't allow queries to affect THD's status variables */
  struct System_status_var save_thd_status_var = m_thd->status_var;

  /* Check modified_non_trans_table is false(check if actually needed) */
  assert(!m_thd->get_transaction()->has_modified_non_trans_table(
      Transaction_ctx::STMT));

  /* Turn off binlogging */
  ulonglong save_thd_options = m_thd->variables.option_bits;
  static_assert(
      sizeof(save_thd_options) == sizeof(m_thd->variables.option_bits),
      "Mismatched type for variable used to save option_bits");
  m_thd->variables.option_bits &= ~OPTION_BIN_LOG;

  /*
    Increment query_id, the query_id is used when generating
    the xid for transaction and unless incremented will get
    the same xid in subsequent queries.
  */
  m_thd->set_query_id(next_query_id());

  bool result = execute_query(sql_query, ignore_mysql_errors);

  /* Restore THD settings */
  m_thd->variables.option_bits = save_thd_options;
  m_thd->status_var = save_thd_status_var;

  return result;
}

bool Ndb_local_connection::truncate_table(const std::string &db,
                                          const std::string &table,
                                          bool ignore_no_such_table) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("db: '%s', table: '%s'", db.c_str(), table.c_str()));

  // Create the SQL string
  const std::string query = "TRUNCATE TABLE " + db + "." + table;

  // Setup list of errors to ignore
  uint ignore_mysql_errors[2] = {0, 0};
  if (ignore_no_such_table) {
    ignore_mysql_errors[0] = ER_NO_SUCH_TABLE;
  }

  return execute_query_iso(query, ignore_mysql_errors);
}

bool Ndb_local_connection::delete_rows(const std::string &db,
                                       const std::string &table,
                                       bool ignore_no_such_table,
                                       const std::string &where) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("db: '%s', table: '%s'", db.c_str(), table.c_str()));

  // Create the SQL string
  const std::string query =
      "DELETE FROM " + db + "." + table + " WHERE " + where;

  // Setup list of errors to ignore
  uint ignore_mysql_errors[2] = {0, 0};
  if (ignore_no_such_table) {
    ignore_mysql_errors[0] = ER_NO_SUCH_TABLE;
  }

  return execute_query_iso(query, ignore_mysql_errors);
}

bool Ndb_local_connection::create_util_table(const std::string &table_def_sql) {
  DBUG_TRACE;
  return execute_query_iso(table_def_sql, nullptr);
}

bool Ndb_local_connection::run_acl_statement(const std::string &acl_sql) {
  DBUG_TRACE;
  const uint ignore_mysql_errors[3] = {ER_NO_SUCH_TABLE,
                                       ER_NONEXISTING_TABLE_GRANT, 0};
  m_thd->set_query_id(next_query_id());
  return execute_query(acl_sql, ignore_mysql_errors);
}

bool Ndb_local_connection::try_create_user(const std::string &sql) {
  DBUG_TRACE;
  const uint ignore_mysql_errors[4] = {
      ER_USER_ALREADY_EXISTS, ER_USER_DOES_NOT_EXIST, ER_CANNOT_USER, 0};
  m_thd->set_query_id(next_query_id());
  uint result = execute_query(sql);
  (void)check_query_error(sql, result, ignore_mysql_errors);
  return (bool)result;
}

bool Ndb_local_connection::create_database(const std::string &database_name) {
  DBUG_TRACE;
  const std::string create_db_sql = "CREATE DATABASE `" + database_name + "`";
  return execute_query_iso(create_db_sql, nullptr);
}

bool Ndb_local_connection::drop_database(const std::string &database_name) {
  DBUG_TRACE;
  const std::string drop_db_sql = "DROP DATABASE `" + database_name + "`";
  return execute_query_iso(drop_db_sql, nullptr);
}

bool Ndb_local_connection::execute_database_ddl(const std::string &ddl_query) {
  DBUG_TRACE;
  return execute_query_iso(ddl_query, nullptr);
}

Ed_result_set *Ndb_local_connection::get_results() {
  return impl->connection.get_result_sets();
}

Ndb_privilege_upgrade_connection::Ndb_privilege_upgrade_connection(THD *thd)
    : Ndb_local_connection(thd) {
  m_push_warnings = false;
  m_saved_sql_mode = m_thd->variables.sql_mode;
  m_thd->variables.sql_mode = MODE_NO_ENGINE_SUBSTITUTION;
}

Ndb_privilege_upgrade_connection::~Ndb_privilege_upgrade_connection() {
  m_thd->variables.sql_mode = m_saved_sql_mode;
}

bool Ndb_privilege_upgrade_connection::migrate_privilege_table(
    const std::string &table) {
  const std::string query = "ALTER TABLE mysql." + table + " ENGINE=innodb;";
  return execute_query(query, nullptr);
}
