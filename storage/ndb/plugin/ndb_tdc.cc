/*
   Copyright (c) 2014, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "storage/ndb/plugin/ndb_tdc.h"

#include "sql/sql_base.h"  // close_cached_tables()
#include "sql/table.h"     // Table_ref

/*
  Close all tables in MySQL Server's table definition cache
  which aren't in use by any thread
*/

bool ndb_tdc_close_cached_tables(void) {
  DBUG_TRACE;

  const int res = close_cached_tables(nullptr,  // No need for thd pointer
                                      nullptr,  // Close all tables
                                      false,    // Don't wait
                                      0  // Timeout unused when not waiting
  );
  return res;
}

/*
  Close table in MySQL Server's table definition cache
  which aren't in use by any thread

  @param     thd     Thread handle
  @param[in] dbname  Name of database table is in
  @param[in] tabname Name of table
*/

bool ndb_tdc_close_cached_table(THD *thd, const char *dbname,
                                const char *tabname) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("dbname: %s, tabname: %s", dbname, tabname));

  // NOTE! initializes only the minimal part of Table_ref
  // required for calling close_cached_tables()
  Table_ref table_list;
  table_list.db = dbname;
  table_list.alias = table_list.table_name = tabname;

  const int res = close_cached_tables(thd, &table_list,
                                      false,  // Don't wait
                                      0       // Timeout unused when not waiting
  );
  return res;
}
