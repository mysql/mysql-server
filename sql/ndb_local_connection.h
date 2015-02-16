/*
   Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_LOCAL_CONNECTION_H
#define NDB_LOCAL_CONNECTION_H

#include <mysql/mysql_lex_string.h>

class THD;

/*
  Wrapper class for executing queries against the local
  MySQL Server without affecting the current THD's
  settings and status.

  The functionality is implemented by concatenating SQL
  queries and executing those using Ed_connection. Should
  the SQL query fail, the exact error message and all
  warning that occured can be examined in order to handle
  the error in a graceful way.

*/

class Ndb_local_connection {
public:
  Ndb_local_connection(THD* thd);

  bool truncate_table(const char* db, size_t db_length,
                      const char* table, size_t table_length,
                      bool ignore_no_such_table);

  bool flush_table(const char* db, size_t db_length,
                   const char* table, size_t table_length);

  bool delete_rows(const char* db, size_t db_length,
                   const char* table, size_t table_length,
                   bool ignore_no_such_table,
                   ... /* where clause, NULL terminated list of strings */);

  bool create_sys_table(const char* db, size_t db_length,
                        const char* table, size_t table_length,
                        bool create_if_not_exists,
                        const char* create_definiton,
                        const char* create_options);

  /* Don't use this function for new implementation, backward compat. only */
  bool raw_run_query(const char* query, size_t query_length,
                     const int* suppress_errors);

private:
  bool execute_query(MYSQL_LEX_STRING sql_text,
                     const unsigned int* ignore_mysql_errors,
                     const class Suppressor* suppressor = NULL);
  bool execute_query_iso(MYSQL_LEX_STRING sql_text,
                         const unsigned int* ignore_mysql_errors,
                         const class Suppressor* suppressor = NULL);
private:
  THD* m_thd;
  bool m_push_warnings;
};


#endif
