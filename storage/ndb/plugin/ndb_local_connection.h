/*
   Copyright (c) 2011, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_LOCAL_CONNECTION_H
#define NDB_LOCAL_CONNECTION_H

#include <memory>
#include <string>

#include "my_inttypes.h"
#include "mysql/mysql_lex_string.h"

class THD;

/*
  Wrapper class for executing queries against the local
  MySQL Server without affecting the current THD's
  settings and status.

  The functionality is implemented by concatenating SQL
  queries and executing those using Ed_connection. Should
  the SQL query fail, the exact error message and all
  warning that occurred can be examined in order to handle
  the error in a graceful way.

*/

class Ndb_local_connection {
 public:
  Ndb_local_connection(THD *thd);
  ~Ndb_local_connection();

  bool truncate_table(const char *db, const char *table,
                      bool ignore_no_such_table);

  bool delete_rows(const std::string &db, const std::string &table,
                   int ignore_no_such_table, const std::string &where);

  bool create_util_table(const std::string &table_def_sql);

  bool create_database(const std::string &database_name);

  bool drop_database(const std::string &database_name);

  bool execute_database_ddl(const std::string &ddl_query);

  bool run_acl_statement(const std::string &acl_sql);

  /* Don't use this function for new implementation, backward compat. only */
  bool raw_run_query(const char *query, size_t query_length,
                     const int *suppress_errors);

 protected:
  bool execute_query_iso(MYSQL_LEX_STRING sql_text,
                         const uint *ignore_mysql_errors,
                         const class Suppressor *suppressor = NULL);

  class Ed_result_set *get_results();

  bool execute_query(MYSQL_LEX_STRING sql_text, const uint *ignore_mysql_errors,
                     const class Suppressor *suppressor = NULL);

  bool m_push_warnings;
  THD *m_thd;

 private:
  class Impl;
  std::unique_ptr<class Impl> impl;
};

class Ndb_privilege_upgrade_connection : public Ndb_local_connection {
 public:
  Ndb_privilege_upgrade_connection(THD *thd);
  ~Ndb_privilege_upgrade_connection();

  bool migrate_privilege_table(const std::string &table_name);

 private:
  ulonglong m_saved_sql_mode;
};

#endif
