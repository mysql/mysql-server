/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/*
  Atomic rename of table;  RENAME TABLE t1 to t2, tmp to t1 [,...]
*/

#include "sql_rename.h"

#include "log.h"              // query_logger
#include "mysqld.h"           // lower_case_table_names
#include "sql_base.h"         // tdc_remove_table,
                              // lock_table_names,
#include "sql_cache.h"        // query_cache
#include "sql_class.h"        // THD
#include "sql_handler.h"      // mysql_ha_rm_tables
#include "sql_table.h"        // write_bin_log,
                              // build_table_filename
#include "sql_trigger.h"      // change_trigger_table_name
#include "sql_view.h"         // mysql_rename_view

#include "dd/dd_table.h"      // dd::table_exists

static TABLE_LIST *rename_tables(THD *thd, TABLE_LIST *table_list,
				 bool skip_error);

static TABLE_LIST *reverse_table_list(TABLE_LIST *table_list);

/*
  Every two entries in the table_list form a pair of original name and
  the new name.
*/

bool mysql_rename_tables(THD *thd, TABLE_LIST *table_list, bool silent)
{
  bool error= 1;
  bool binlog_error= 0;
  TABLE_LIST *ren_table= 0;
  int to_table;
  char *rename_log_table[2]= {NULL, NULL};
  DBUG_ENTER("mysql_rename_tables");

  /*
    Avoid problems with a rename on a table that we have locked or
    if the user is trying to to do this in a transcation context
  */

  if (thd->locked_tables_mode || thd->in_active_multi_stmt_transaction())
  {
    my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
    DBUG_RETURN(1);
  }

  mysql_ha_rm_tables(thd, table_list);

  if (query_logger.is_log_table_enabled(QUERY_LOG_GENERAL) ||
      query_logger.is_log_table_enabled(QUERY_LOG_SLOW))
  {

    /*
      Rules for rename of a log table:

      IF   1. Log tables are enabled
      AND  2. Rename operates on the log table and nothing is being
              renamed to the log table.
      DO   3. Throw an error message.
      ELSE 4. Perform rename.
    */

    for (to_table= 0, ren_table= table_list; ren_table;
         to_table= 1 - to_table, ren_table= ren_table->next_local)
    {
      int log_table_rename= 0;

      if ((log_table_rename= query_logger.check_if_log_table(ren_table, true)))
      {
        /*
          as we use log_table_rename as an array index, we need it to start
          with 0, while QUERY_LOG_SLOW == 1 and QUERY_LOG_GENERAL == 2.
          So, we shift the value to start with 0;
        */
        log_table_rename--;
        if (rename_log_table[log_table_rename])
        {
          if (to_table)
            rename_log_table[log_table_rename]= NULL;
          else
          {
            /*
              Two renames of "log_table TO" w/o rename "TO log_table" in
              between.
            */
            my_error(ER_CANT_RENAME_LOG_TABLE, MYF(0), ren_table->table_name,
                     ren_table->table_name);
            goto err;
          }
        }
        else
        {
          if (to_table)
          {
            /*
              Attempt to rename a table TO log_table w/o renaming
              log_table TO some table.
            */
            my_error(ER_CANT_RENAME_LOG_TABLE, MYF(0), ren_table->table_name,
                     ren_table->table_name);
            goto err;
          }
          else
          {
            /* save the name of the log table to report an error */
            rename_log_table[log_table_rename]=
              const_cast<char*>(ren_table->table_name);
          }
        }
      }
    }
    if (rename_log_table[0] || rename_log_table[1])
    {
      if (rename_log_table[0])
        my_error(ER_CANT_RENAME_LOG_TABLE, MYF(0), rename_log_table[0],
                 rename_log_table[0]);
      else
        my_error(ER_CANT_RENAME_LOG_TABLE, MYF(0), rename_log_table[1],
                 rename_log_table[1]);
      goto err;
    }
  }

  if (lock_table_names(thd, table_list, 0, thd->variables.lock_wait_timeout, 0))
    goto err;

  for (ren_table= table_list; ren_table; ren_table= ren_table->next_local)
    tdc_remove_table(thd, TDC_RT_REMOVE_ALL, ren_table->db,
                     ren_table->table_name, FALSE);

  error=0;
  /*
    An exclusive lock on table names is satisfactory to ensure
    no other thread accesses this table.
  */
  if ((ren_table=rename_tables(thd,table_list,0)))
  {
    /* Rename didn't succeed;  rename back the tables in reverse order */
    TABLE_LIST *table;

    /* Reverse the table list */
    table_list= reverse_table_list(table_list);

    /* Find the last renamed table */
    for (table= table_list;
	 table->next_local != ren_table ;
	 table= table->next_local->next_local) ;
    table= table->next_local->next_local;		// Skip error table
    /* Revert to old names */
    rename_tables(thd, table, 1);

    /* Revert the table list (for prepared statements) */
    table_list= reverse_table_list(table_list);

    error= 1;
  }

  if (!silent && !error)
  {
    binlog_error= write_bin_log(thd, true,
                                thd->query().str, thd->query().length);
    if (!binlog_error)
      my_ok(thd);
  }

  if (!error)
    query_cache.invalidate(thd, table_list, FALSE);

err:
  DBUG_RETURN(error || binlog_error);
}


/*
  reverse table list

  SYNOPSIS
    reverse_table_list()
    table_list pointer to table _list

  RETURN
    pointer to new (reversed) list
*/
static TABLE_LIST *reverse_table_list(TABLE_LIST *table_list)
{
  TABLE_LIST *prev= 0;

  while (table_list)
  {
    TABLE_LIST *next= table_list->next_local;
    table_list->next_local= prev;
    prev= table_list;
    table_list= next;
  }
  return (prev);
}


/*
  Rename a single table or a view

  SYNPOSIS
    do_rename()
      thd               Thread handle
      ren_table         A table/view to be renamed
      new_db            The database to which the table to be moved to
      new_table_name    The new table/view name
      new_table_alias   The new table/view alias
      skip_error        Whether to skip errors

  DESCRIPTION
    Rename a single table or a view.

  RETURN
    false     Ok
    true      rename failed
*/

static bool
do_rename(THD *thd, TABLE_LIST *ren_table,
          const char *new_db, const char *new_table_name,
          const char *new_table_alias, bool skip_error)
{
  const char *new_alias= new_table_name;
  const char *old_alias= ren_table->table_name;

  DBUG_ENTER("do_rename");

  if (lower_case_table_names == 2)
  {
    old_alias= ren_table->alias;
    new_alias= new_table_alias;
  }
  DBUG_ASSERT(new_alias);

  // Fail if the target table already exists
  bool exists;
  if (dd::table_exists<dd::Abstract_table>(thd->dd_client(), new_db,
                                           new_alias, &exists))
    DBUG_RETURN(true);                         // This error cannot be skipped

  if (exists)
  {
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_alias);
    DBUG_RETURN(true);                         // This error cannot be skipped
  }

  // Get the table type of the old table, and fail if it does not exist
  dd::Abstract_table::enum_table_type table_type;
  if (dd::abstract_table_type(thd->dd_client(), ren_table->db,
                              old_alias, &table_type))
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0), ren_table->db, old_alias);
    DBUG_RETURN(!skip_error);
  }

  // So here we know the source table exists and the target table does
  // not exist. Next is to act based on the table type.
  switch (table_type)
  {
  case dd::Abstract_table::TT_BASE_TABLE:
    {
      handlerton *hton= NULL;
      // If the engine is not found, my_error() has already been called
      if (dd::table_storage_engine(thd, ren_table, &hton))
        DBUG_RETURN(!skip_error);

      // If renaming fails, my_error() has already been called
      if (mysql_rename_table(thd, hton, ren_table->db, old_alias, new_db,
                             new_alias, 0))
        DBUG_RETURN(!skip_error);

      // If we fail to update the triggers appropriately, we revert the
      // changes done and report an error.
      if (change_trigger_table_name(thd, ren_table->db, old_alias,
                                         ren_table->table_name,
                                         new_db, new_alias))
      {
        (void) mysql_rename_table(thd, hton, new_db, new_alias,
                                  ren_table->db, old_alias, NO_FK_CHECKS);
        DBUG_RETURN(!skip_error);
      }
      break;
    }
  case dd::Abstract_table::TT_SYSTEM_VIEW: // Fall through
  case dd::Abstract_table::TT_USER_VIEW:
    {
      // Changing the schema of a view is not allowed.
      if (strcmp(ren_table->db, new_db))
      {
        my_error(ER_FORBID_SCHEMA_CHANGE, MYF(0), ren_table->db, new_db);
        DBUG_RETURN(!skip_error);
      }
      else if (mysql_rename_view(thd, new_db, new_alias, ren_table))
        DBUG_RETURN(!skip_error);
      break;
    }
  default:
    DBUG_ASSERT(false); /* purecov: deadcode */
  }

  // Now, we know that rename succeeded, and can log the schema access
  thd->add_to_binlog_accessed_dbs(ren_table->db);
  thd->add_to_binlog_accessed_dbs(new_db);

  DBUG_RETURN(false);
}
/*
  Rename all tables in list; Return pointer to wrong entry if something goes
  wrong.  Note that the table_list may be empty!
*/

/*
  Rename tables/views in the list

  SYNPOSIS
    rename_tables()
      thd               Thread handle
      table_list        List of tables to rename
      skip_error        Whether to skip errors

  DESCRIPTION
    Take a table/view name from and odd list element and rename it to a
    the name taken from list element+1. Note that the table_list may be
    empty.

  RETURN
    false     Ok
    true      rename failed
*/

static TABLE_LIST *
rename_tables(THD *thd, TABLE_LIST *table_list, bool skip_error)
{
  TABLE_LIST *ren_table, *new_table;

  DBUG_ENTER("rename_tables");

  for (ren_table= table_list; ren_table; ren_table= new_table->next_local)
  {
    new_table= ren_table->next_local;
    if (do_rename(thd, ren_table, new_table->db, new_table->table_name,
                  new_table->alias, skip_error))
      DBUG_RETURN(ren_table);
  }
  DBUG_RETURN(0);
}
