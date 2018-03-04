/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/ndb_local_connection.h"

#include "sql/mysqld.h" // next_query_id()
#include "sql/ndb_log.h"
#include "sql/sql_class.h"
#include "sql/sql_prepare.h"

Ndb_local_connection::Ndb_local_connection(THD* thd_arg):
  m_thd(thd_arg)
{
  assert(thd_arg);

  /*
    System(or daemon) threads report error to log file
    all other threads use push_warning
  */
  m_push_warnings = (thd_arg->get_command() != COM_DAEMON);
}


static inline bool
should_ignore_error(const uint* ignore_error_list, uint error)
{
  DBUG_ENTER("should_ignore_error");
  DBUG_PRINT("enter", ("error: %u", error));
  const uint* ignore_error = ignore_error_list;
  while(*ignore_error)
  {
    DBUG_PRINT("info", ("ignore_error: %u", *ignore_error));
    if (*ignore_error == error)
      DBUG_RETURN(true);
    ignore_error++;
  }
  DBUG_PRINT("info", ("Don't ignore error"));
  DBUG_RETURN(false);
}


class Suppressor {
public:
  virtual ~Suppressor() {}
  virtual bool should_ignore_error(Ed_connection& con) const = 0;
};


bool
Ndb_local_connection::execute_query(MYSQL_LEX_STRING sql_text,
                                    const uint* ignore_mysql_errors,
                                    const Suppressor* suppressor)
{
  DBUG_ENTER("Ndb_local_connection::execute_query");
  Ed_connection con(m_thd);
  if (con.execute_direct(sql_text))
  {
    /* Error occured while executing the query */
    const uint last_errno = con.get_last_errno();
    assert(last_errno); // last_errno must have been set
    const char* last_errmsg = con.get_last_error();

    DBUG_PRINT("error", ("Query '%s' failed, error: '%d: %s'",
                         sql_text.str,
                         last_errno, last_errmsg));

    // catch some SQL parse errors in debug
    assert(last_errno != ER_PARSE_ERROR &&
           last_errno != ER_EMPTY_QUERY);

    /* Check if this is a MySQL level errors that should be ignored */
    if (ignore_mysql_errors &&
        should_ignore_error(ignore_mysql_errors, last_errno))
    {
      /* MySQL level error suppressed -> return success */
      m_thd->clear_error();
      DBUG_RETURN(false);
    }

    /*
      Call the suppressor to check if it want to silence
      this error
    */
     if (suppressor &&
         suppressor->should_ignore_error(con))
    {
      /* Error suppressed -> return sucess */
      m_thd->clear_error();
      DBUG_RETURN(false);
    }

    if (m_push_warnings)
    {
      // Append the error which caused the error to thd's warning list
      push_warning(m_thd, Sql_condition::SL_WARNING,
                   last_errno, last_errmsg);
    }
    else
    {
      // Print the error to log file
      ndb_log_error("Query '%s' failed, error: %d: %s",
                    sql_text.str, last_errno, last_errmsg);
    }

    DBUG_RETURN(true);
  }

  // Query returned ok, thd should have no error
  assert(!m_thd->is_error());

  DBUG_RETURN(false); // Success
}


/*
  Execute the query with even higher isolation than what execute_query
  provides to avoid that for example THD's status variables are changed
*/

bool
Ndb_local_connection::execute_query_iso(MYSQL_LEX_STRING sql_text,
                                        const uint* ignore_mysql_errors,
                                        const Suppressor* suppressor)
{
  /* Don't allow queries to affect THD's status variables */
  struct System_status_var save_thd_status_var= m_thd->status_var;

  /* Check modified_non_trans_table is false(check if actually needed) */
  assert(!m_thd->get_transaction()->has_modified_non_trans_table(
    Transaction_ctx::STMT));

  /* Turn off binlogging */
  ulonglong save_thd_options= m_thd->variables.option_bits;
  assert(sizeof(save_thd_options) == sizeof(m_thd->variables.option_bits));
  m_thd->variables.option_bits&= ~OPTION_BIN_LOG;

  /*
    Increment query_id, the query_id is used when generating
    the xid for transaction and unless incremented will get
    the same xid in subsequent queries.
  */
  m_thd->set_query_id(next_query_id());

  bool result = execute_query(sql_text,
                              ignore_mysql_errors,
                              suppressor);

  /* Restore THD settings */
  m_thd->variables.option_bits= save_thd_options;
  m_thd->status_var= save_thd_status_var;

  return result;
}


bool
Ndb_local_connection::truncate_table(const char* db, size_t db_length,
                                     const char* table, size_t table_length,
                                     bool ignore_no_such_table)
{
  DBUG_ENTER("Ndb_local_connection::truncate_table");
  DBUG_PRINT("enter", ("db: '%s', table: '%s'", db, table));

  // Create the SQL string
  String sql_text((uint32)(db_length + table_length + 100));
  sql_text.append(STRING_WITH_LEN("TRUNCATE TABLE "));
  sql_text.append(db, (uint32)db_length);
  sql_text.append(STRING_WITH_LEN("."));
  sql_text.append(table, (uint32)table_length);

  // Setup list of errors to ignore
  uint ignore_mysql_errors[2] = {0, 0};
  if (ignore_no_such_table)
    ignore_mysql_errors[0] = ER_NO_SUCH_TABLE;

  DBUG_RETURN(execute_query_iso(sql_text.lex_string(),
                                ignore_mysql_errors,
                                NULL));
}


bool
Ndb_local_connection::flush_table(const char* db, size_t db_length,
                                  const char* table, size_t table_length)
{
  DBUG_ENTER("Ndb_local_connection::flush_table");
  DBUG_PRINT("enter", ("db: '%s', table: '%s'", db, table));

  // Create the SQL string
  String sql_text((uint32)(db_length + table_length + 100));
  sql_text.append(STRING_WITH_LEN("FLUSH TABLES "));
  sql_text.append(db, (uint32)db_length);
  sql_text.append(STRING_WITH_LEN("."));
  sql_text.append(table, (uint32)table_length);

  DBUG_RETURN(execute_query_iso(sql_text.lex_string(),
                                NULL,
                                NULL));
}


bool
Ndb_local_connection::delete_rows(const char* db, size_t db_length,
                                  const char* table, size_t table_length,
                                  bool ignore_no_such_table,
                                  ...)
{
  DBUG_ENTER("Ndb_local_connection::truncate_table");
  DBUG_PRINT("enter", ("db: '%s', table: '%s'", db, table));

  // Create the SQL string
  String sql_text((uint32)(db_length + table_length + 100));
  sql_text.append(STRING_WITH_LEN("DELETE FROM "));
  sql_text.append(db, (uint32)db_length);
  sql_text.append(STRING_WITH_LEN("."));
  sql_text.append(table, (uint32)table_length);
  sql_text.append(" WHERE ");

  va_list args;
  va_start(args, ignore_no_such_table);

  // Append var args strings until ending NULL as WHERE clause
  const char* arg;
  bool empty_where = true;
  while ((arg= va_arg(args, char *)))
  {
    sql_text.append(arg);
    empty_where = false;
  }

  va_end(args);

  if (empty_where)
    sql_text.append("1=1");

  // Setup list of errors to ignore
  uint ignore_mysql_errors[2] = {0, 0};
  if (ignore_no_such_table)
    ignore_mysql_errors[0] = ER_NO_SUCH_TABLE;

  DBUG_RETURN(execute_query_iso(sql_text.lex_string(),
                                ignore_mysql_errors,
                                NULL));
}


class Create_sys_table_suppressor : public Suppressor
{
public:
  virtual ~Create_sys_table_suppressor() {}
  virtual bool should_ignore_error(Ed_connection& con) const
  {
    const uint last_errno = con.get_last_errno();
    const char* last_errmsg = con.get_last_error();
    DBUG_ENTER("Create_sys_table_suppressor::should_ignore_error");
    DBUG_PRINT("enter", ("last_errno: %d, last_errmsg: '%s'",
                         last_errno, last_errmsg));

    if (last_errno == ER_CANT_CREATE_TABLE)
    {
      /*
        The CREATE TABLE failed late and it was classifed as a
        'Can't create table' error.
      */

      /*
        Error message always end with " %d)" in all languages. Find last
        space and convert number from there
      */
      const char* last_space = strrchr(last_errmsg, ' ');
      DBUG_PRINT("info", ("last_space: '%s'", last_space));
      if (!last_space)
      {
        // Could not find last space, parse error
        assert(false);
        DBUG_RETURN(false); // Don't suppress
      }

      int error;
      if (sscanf(last_space, " %d)", &error) != 1)
      {
        // Not a number here, parse error
        assert(false);
        DBUG_RETURN(false); // Don't suppress
      }
      DBUG_PRINT("info", ("error: %d", error));

      switch (error)
      {
        case HA_ERR_TABLE_EXIST:
        {
          /*
            The most common error is that NDB returns error 721
            which means 'No such table' and the error is automatically
            mapped to MySQL error code ER_TABLE_EXISTS_ERROR

            This is most likley caused by another MySQL Server trying
            to create the same table inbetween the check if table
            exists(on local disk and in storage engine) and the actual
            create.
          */
          DBUG_RETURN(true); // Suppress
          break;
        }

        case 701: // System busy with other schema operation
        case 711: // System busy with node restart, no schema operations
        case 702: // Request to non-master(should never pop up to api)
        {
          /* Different errors from NDB, that just need to be retried later */
          DBUG_RETURN(true); // Suppress
          break;
        }

        case 4009: // Cluster failure
        case HA_ERR_NO_CONNECTION: // 4009 auto mapped to this error
        {
          /*
            No connection to cluster, don't spam error log with
            failed to create ndb_xx tables
          */
          DBUG_RETURN(true); // Suppress
          break;
        }
      }
    }
    DBUG_PRINT("info", ("Don't ignore error"));
    DBUG_RETURN(false); // Don't suppress
  }
};


bool
Ndb_local_connection::create_sys_table(const char* db, size_t db_length,
                                       const char* table, size_t table_length,
                                       bool create_if_not_exists,
                                       const char* create_definitions,
                                       const char* create_options)
{
  DBUG_ENTER("Ndb_local_connection::create_table");
  DBUG_PRINT("enter", ("db: '%s', table: '%s'", db, table));

  // Create the SQL string
  String sql_text(512);
  sql_text.append(STRING_WITH_LEN("CREATE TABLE "));

  if (create_if_not_exists)
    sql_text.append(STRING_WITH_LEN("IF NOT EXISTS "));
  sql_text.append(db, (uint32)db_length);
  sql_text.append(STRING_WITH_LEN("."));
  sql_text.append(table, (uint32)table_length);

  sql_text.append(STRING_WITH_LEN(" ( "));
  sql_text.append(create_definitions);
  sql_text.append(STRING_WITH_LEN(" ) "));
  sql_text.append(create_options);

  // List of errors to ignore
  uint ignore_mysql_errors[2] = {ER_TABLE_EXISTS_ERROR, 0};

  /*
    This is the only place where an error is suppressed
    based one the original NDB error, wich is extracted
    by parsing the error string, use a special suppressor
  */
  Create_sys_table_suppressor suppressor;

  DBUG_RETURN(execute_query_iso(sql_text.lex_string(),
                                ignore_mysql_errors,
                                &suppressor));
}


bool
Ndb_local_connection::raw_run_query(const char* query, size_t query_length,
                                    const int* suppress_errors)
{
  DBUG_ENTER("Ndb_local_connection::raw_run_query");

  LEX_STRING sql_text = { (char*)query, query_length };

  DBUG_RETURN(execute_query_iso(sql_text,
                                (const uint*)suppress_errors,
                                NULL));
}

