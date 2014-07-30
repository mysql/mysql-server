/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "ndb_tdc.h"

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include "table.h"          // TABLE_LIST
#include "sql_base.h"       // close_cached_tables()


/*
  Close all tables in MySQL Server's table definition cache
  which aren't in use by any thread
*/

bool ndb_tdc_close_cached_tables(void)
{
  DBUG_ENTER("ndb_tdc_close_cached_tables");

  const int res = close_cached_tables(NULL, // No need for thd pointer
                                      NULL, // Close all tables
                                      false, // Don't wait
                                      0 // Timeout unused when not waiting
                                      );
  DBUG_RETURN(res);
}


/*
  Close table in MySQL Server's table definition cache
  which aren't in use by any thread

  @param     thd     Thread handle
  @param[in] dbname  Name of database table is in
  @param[in] tabname Name of table
*/

bool
ndb_tdc_close_cached_table(THD* thd, const char* dbname, const char* tabname)
{

  DBUG_ENTER("ndb_tdc_close_cached_table");
  DBUG_PRINT("enter", ("dbname: %s, tabname: %s", dbname, tabname));

  // NOTE! initializes only the minimal part of TABLE_LIST
  // required for calling close_cached_tables()
  TABLE_LIST table_list;
  memset(&table_list, 0, sizeof(table_list));
  table_list.db= (char *)dbname;
  table_list.alias= table_list.table_name= (char *)tabname;

  const int res = close_cached_tables(thd,
                                      &table_list,
                                      false, // Don't wait
                                      0 // Timeout unused when not waiting
                                      );
  DBUG_RETURN(res);
}
