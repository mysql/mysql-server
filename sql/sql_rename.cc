/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Atomic rename of table;  RENAME TABLE t1 to t2, tmp to t1 [,...]
*/

#include "sql_priv.h"
#include "unireg.h"
#include "sql_rename.h"
#include "sql_cache.h"                          // query_cache_*
#include "sql_table.h"                         // build_table_filename
#include "sql_view.h"             // mysql_frm_type, mysql_rename_view
#include "sql_trigger.h"
#include "lock.h"       // MYSQL_OPEN_SKIP_TEMPORARY
#include "sql_base.h"   // tdc_remove_table, lock_table_names,
#include "sql_handler.h"                        // mysql_ha_rm_tables
#include "datadict.h"

static TABLE_LIST *rename_tables(THD *thd, TABLE_LIST *table_list,
				 bool skip_error);

static TABLE_LIST *reverse_table_list(TABLE_LIST *table_list);

/*
  Every second entry in the table_list is the original name and every
  second entry is the new name.
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
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
               ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
    DBUG_RETURN(1);
  }

  mysql_ha_rm_tables(thd, table_list);

  if (logger.is_log_table_enabled(QUERY_LOG_GENERAL) ||
      logger.is_log_table_enabled(QUERY_LOG_SLOW))
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

      if ((log_table_rename=
           check_if_log_table(ren_table->db_length, ren_table->db,
                              ren_table->table_name_length,
                              ren_table->table_name, 1)))
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
            rename_log_table[log_table_rename]= ren_table->table_name;
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

  if (lock_table_names(thd, table_list, 0, thd->variables.lock_wait_timeout,
                       MYSQL_OPEN_SKIP_TEMPORARY))
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
    binlog_error= write_bin_log(thd, TRUE, thd->query(), thd->query_length());
    if (!binlog_error)
      my_ok(thd);
  }

  if (!error)
    query_cache_invalidate3(thd, table_list, 0);

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
      skip_error        Whether to skip error

  DESCRIPTION
    Rename a single table or a view.

  RETURN
    false     Ok
    true      rename failed
*/

bool
do_rename(THD *thd, TABLE_LIST *ren_table, char *new_db, char *new_table_name,
          char *new_table_alias, bool skip_error)
{
  int rc= 1;
  char name[FN_REFLEN + 1];
  const char *new_alias, *old_alias;
  frm_type_enum frm_type;
  enum legacy_db_type table_type;

  DBUG_ENTER("do_rename");

  if (lower_case_table_names == 2)
  {
    old_alias= ren_table->alias;
    new_alias= new_table_alias;
  }
  else
  {
    old_alias= ren_table->table_name;
    new_alias= new_table_name;
  }
  DBUG_ASSERT(new_alias);

  build_table_filename(name, sizeof(name) - 1,
                       new_db, new_alias, reg_ext, 0);
  if (!access(name,F_OK))
  {
    my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_alias);
    DBUG_RETURN(1);			// This can't be skipped
  }
  build_table_filename(name, sizeof(name) - 1,
                       ren_table->db, old_alias, reg_ext, 0);

  frm_type= dd_frm_type(thd, name, &table_type);
  switch (frm_type)
  {
    case FRMTYPE_TABLE:
      {
        if (!(rc= mysql_rename_table(ha_resolve_by_legacy_type(thd,
                                                               table_type), 
                                     ren_table->db, old_alias,
                                     new_db, new_alias, 0)))
        {
          if ((rc= Table_triggers_list::change_table_name(thd, ren_table->db,
                                                          old_alias,
                                                          ren_table->table_name,
                                                          new_db,
                                                          new_alias)))
          {
            /*
              We've succeeded in renaming table's .frm and in updating
              corresponding handler data, but have failed to update table's
              triggers appropriately. So let us revert operations on .frm
              and handler's data and report about failure to rename table.
            */
            (void) mysql_rename_table(ha_resolve_by_legacy_type(thd,
                                                                table_type),
                                      new_db, new_alias,
                                      ren_table->db, old_alias, 0);
          }
        }
      }
      break;
    case FRMTYPE_VIEW:
      /* 
         change of schema is not allowed
         except of ALTER ...UPGRADE DATA DIRECTORY NAME command
         because a view has valid internal db&table names in this case.
      */
      if (thd->lex->sql_command != SQLCOM_ALTER_DB_UPGRADE &&
          strcmp(ren_table->db, new_db))
        my_error(ER_FORBID_SCHEMA_CHANGE, MYF(0), ren_table->db, 
                 new_db);
      else
        rc= mysql_rename_view(thd, new_db, new_alias, ren_table);
      break;
    default:
      DBUG_ASSERT(0); // should never happen
    case FRMTYPE_ERROR:
      my_error(ER_FILE_NOT_FOUND, MYF(0), name, my_errno);
      break;
  }
  if (rc && !skip_error)
    DBUG_RETURN(1);

  DBUG_RETURN(0);

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
