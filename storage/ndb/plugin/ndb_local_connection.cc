/*
   Copyright (c) 2011, 2021, Oracle and/or its affiliates.

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

#include "sql/mysqld.h"  // next_query_id()
#include "sql/sql_class.h"
#include "sql/sql_prepare.h"
#include "storage/ndb/plugin/ndb_log.h"

class Ndb_local_connection::Impl {
 public:
  Impl(THD *thd_arg) : connection(thd_arg) {}
  Ed_connection connection;
};

Ndb_local_connection::Ndb_local_connection(THD *thd_arg)
    : m_thd(thd_arg), impl(std::make_unique<Impl>(thd_arg)) {
  assert(thd_arg);

  /*
    System(or daemon) threads report error to log file
    all other threads use push_warning
  */
  m_push_warnings = (thd_arg->get_command() != COM_DAEMON);
}

Ndb_local_connection::~Ndb_local_connection() = default;

static inline bool should_ignore_error(const uint *ignore_error_list,
                                       uint error) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("error: %u", error));
  const uint *ignore_error = ignore_error_list;
  while (*ignore_error) {
    DBUG_PRINT("info", ("ignore_error: %u", *ignore_error));
    if (*ignore_error == error) return true;
    ignore_error++;
  }
  DBUG_PRINT("info", ("Don't ignore error"));
  return false;
}

class Suppressor {
 public:
  virtual ~Suppressor() {}
  virtual bool should_ignore_error(Ed_connection &con) const = 0;
};

bool Ndb_local_connection::execute_query(MYSQL_LEX_STRING sql_text,
                                         const uint *ignore_mysql_errors,
                                         const Suppressor *suppressor) {
  DBUG_TRACE;

  if (impl->connection.execute_direct(sql_text)) {
    /* Error occured while executing the query */
    const uint last_errno = impl->connection.get_last_errno();
    assert(last_errno);  // last_errno must have been set
    const char *last_errmsg = impl->connection.get_last_error();

    DBUG_PRINT("error", ("Query '%s' failed, error: '%d: %s'", sql_text.str,
                         last_errno, last_errmsg));

    // catch some SQL parse errors in debug
    assert(last_errno != ER_PARSE_ERROR && last_errno != ER_EMPTY_QUERY);

    /* Check if this is a MySQL level errors that should be ignored */
    if (ignore_mysql_errors &&
        should_ignore_error(ignore_mysql_errors, last_errno)) {
      /* MySQL level error suppressed -> return success */
      m_thd->clear_error();
      return false;
    }

    /*
      Call the suppressor to check if it want to silence
      this error
    */
    if (suppressor && suppressor->should_ignore_error(impl->connection)) {
      /* Error suppressed -> return sucess */
      m_thd->clear_error();
      return false;
    }

    if (m_push_warnings) {
      // Append the error which caused the error to thd's warning list
      push_warning(m_thd, Sql_condition::SL_WARNING, last_errno, last_errmsg);
    } else {
      // Print the error to log file
      ndb_log_error("Query '%s' failed, error: %d: %s", sql_text.str,
                    last_errno, last_errmsg);
    }

    return true;
  }

  return false;  // Success
}

/*
  Execute the query with even higher isolation than what execute_query
  provides to avoid that for example THD's status variables are changed
*/

bool Ndb_local_connection::execute_query_iso(MYSQL_LEX_STRING sql_text,
                                             const uint *ignore_mysql_errors,
                                             const Suppressor *suppressor) {
  /* Don't allow queries to affect THD's status variables */
  struct System_status_var save_thd_status_var = m_thd->status_var;

  /* Check modified_non_trans_table is false(check if actually needed) */
  assert(!m_thd->get_transaction()->has_modified_non_trans_table(
      Transaction_ctx::STMT));

  /* Turn off binlogging */
  ulonglong save_thd_options = m_thd->variables.option_bits;
  assert(sizeof(save_thd_options) == sizeof(m_thd->variables.option_bits));
  m_thd->variables.option_bits &= ~OPTION_BIN_LOG;

  /*
    Increment query_id, the query_id is used when generating
    the xid for transaction and unless incremented will get
    the same xid in subsequent queries.
  */
  m_thd->set_query_id(next_query_id());

  bool result = execute_query(sql_text, ignore_mysql_errors, suppressor);

  /* Restore THD settings */
  m_thd->variables.option_bits = save_thd_options;
  m_thd->status_var = save_thd_status_var;

  return result;
}

bool Ndb_local_connection::truncate_table(const char *db, const char *table,
                                          bool ignore_no_such_table) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("db: '%s', table: '%s'", db, table));

  // Create the SQL string
  String sql_text(100);
  sql_text.append(STRING_WITH_LEN("TRUNCATE TABLE "));
  sql_text.append(db);
  sql_text.append(STRING_WITH_LEN("."));
  sql_text.append(table);

  // Setup list of errors to ignore
  uint ignore_mysql_errors[2] = {0, 0};
  if (ignore_no_such_table) ignore_mysql_errors[0] = ER_NO_SUCH_TABLE;

  return execute_query_iso(sql_text.lex_string(), ignore_mysql_errors, NULL);
}

bool Ndb_local_connection::delete_rows(const std::string &db,
                                       const std::string &table,
                                       int ignore_no_such_table,
                                       const std::string &where) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("db: '%s', table: '%s'", db.c_str(), table.c_str()));

  // Create the SQL string
  std::string sql_text;
  sql_text.reserve(db.length() + table.length() + 32 + where.length());
  sql_text.append("DELETE FROM ");
  sql_text.append(db).append(".").append(table);
  sql_text.append(" WHERE ").append(where);

  // Setup list of errors to ignore
  uint ignore_mysql_errors[2] = {0, 0};
  if (ignore_no_such_table) ignore_mysql_errors[0] = ER_NO_SUCH_TABLE;

  const LEX_STRING lex_string = {const_cast<char *>(sql_text.c_str()),
                                 sql_text.length()};
  return execute_query_iso(lex_string, ignore_mysql_errors, NULL);
}

bool Ndb_local_connection::create_util_table(const std::string &table_def_sql) {
  DBUG_TRACE;
  // Don't ignore any errors
  uint ignore_mysql_errors[1] = {0};
  MYSQL_LEX_STRING sql_text = {const_cast<char *>(table_def_sql.c_str()),
                               table_def_sql.length()};

  return execute_query_iso(sql_text, ignore_mysql_errors, nullptr);
}

bool Ndb_local_connection::run_acl_statement(const std::string &acl_sql) {
  DBUG_TRACE;
  uint ignore_mysql_errors[2] = {ER_NO_SUCH_TABLE, ER_NONEXISTING_TABLE_GRANT};

  ndb_log_verbose(30, "run_acl_statement: %s", acl_sql.c_str());
  MYSQL_LEX_STRING sql_text = {const_cast<char *>(acl_sql.c_str()),
                               acl_sql.length()};
  m_thd->set_query_id(next_query_id());
  return execute_query(sql_text, ignore_mysql_errors, nullptr);
}

bool Ndb_local_connection::create_database(const std::string &database_name) {
  DBUG_TRACE;
  // Don't ignore any errors
  uint ignore_mysql_errors[1] = {0};
  const std::string create_db_sql = "CREATE DATABASE `" + database_name + "`";
  MYSQL_LEX_STRING sql_text = {const_cast<char *>(create_db_sql.c_str()),
                               create_db_sql.length()};

  return execute_query_iso(sql_text, ignore_mysql_errors, nullptr);
}

bool Ndb_local_connection::drop_database(const std::string &database_name) {
  DBUG_TRACE;
  // Don't ignore any errors
  uint ignore_mysql_errors[1] = {0};
  const std::string drop_db_sql = "DROP DATABASE `" + database_name + "`";
  MYSQL_LEX_STRING sql_text = {const_cast<char *>(drop_db_sql.c_str()),
                               drop_db_sql.length()};

  return execute_query_iso(sql_text, ignore_mysql_errors, nullptr);
}

bool Ndb_local_connection::execute_database_ddl(const std::string &ddl_query) {
  DBUG_TRACE;
  // Don't ignore any errors
  uint ignore_mysql_errors[1] = {0};
  MYSQL_LEX_STRING sql_text = {const_cast<char *>(ddl_query.c_str()),
                               ddl_query.length()};

  return execute_query_iso(sql_text, ignore_mysql_errors, nullptr);
}

bool Ndb_local_connection::raw_run_query(const char *query, size_t query_length,
                                         const int *suppress_errors) {
  DBUG_TRACE;

  LEX_STRING sql_text = {const_cast<char *>(query), query_length};

  return execute_query_iso(sql_text, (const uint *)suppress_errors, NULL);
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
  std::string query = "ALTER TABLE mysql." + table + " ENGINE=innodb;";
  LEX_STRING sql_text = {const_cast<char *>(query.c_str()), query.length()};
  return execute_query(sql_text, nullptr, nullptr);
}
