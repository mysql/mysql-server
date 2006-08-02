/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Atomic rename of table;  RENAME TABLE t1 to t2, tmp to t1 [,...]
*/

#include "mysql_priv.h"
#include "sql_trigger.h"


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
  TABLE_LIST *ren_table= 0;
  DBUG_ENTER("mysql_rename_tables");

  /*
    Avoid problems with a rename on a table that we have locked or
    if the user is trying to to do this in a transcation context
  */

  if (thd->locked_tables || thd->active_transaction())
  {
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
               ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
    DBUG_RETURN(1);
  }

  if (wait_if_global_read_lock(thd,0,1))
    DBUG_RETURN(1);
  VOID(pthread_mutex_lock(&LOCK_open));
  if (lock_table_names(thd, table_list))
    goto err;
  
  error=0;
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

  /* Lets hope this doesn't fail as the result will be messy */ 
  if (!silent && !error)
  {
    if (mysql_bin_log.is_open())
    {
      thd->clear_error();
      thd->binlog_query(THD::STMT_QUERY_TYPE,
                        thd->query, thd->query_length, FALSE, FALSE);
    }
    send_ok(thd);
  }

  unlock_table_names(thd, table_list, (TABLE_LIST*) 0);

err:
  pthread_mutex_unlock(&LOCK_open);
  start_waiting_global_read_lock(thd);
  DBUG_RETURN(error);
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
  Rename all tables in list; Return pointer to wrong entry if something goes
  wrong.  Note that the table_list may be empty!
*/

static TABLE_LIST *
rename_tables(THD *thd, TABLE_LIST *table_list, bool skip_error)
{
  TABLE_LIST *ren_table,*new_table;
  frm_type_enum frm_type;
  enum legacy_db_type table_type;

  DBUG_ENTER("rename_tables");

  for (ren_table= table_list; ren_table; ren_table= new_table->next_local)
  {
    int rc= 1;
    char name[FN_REFLEN];
    const char *new_alias, *old_alias;

    new_table= ren_table->next_local;
    if (lower_case_table_names == 2)
    {
      old_alias= ren_table->alias;
      new_alias= new_table->alias;
    }
    else
    {
      old_alias= ren_table->table_name;
      new_alias= new_table->table_name;
    }
    build_table_filename(name, sizeof(name),
                         new_table->db, new_alias, reg_ext, 0);
    if (!access(name,F_OK))
    {
      my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_alias);
      DBUG_RETURN(ren_table);			// This can't be skipped
    }
    build_table_filename(name, sizeof(name),
                         ren_table->db, old_alias, reg_ext, 0);

    frm_type= mysql_frm_type(thd, name, &table_type);
    switch (frm_type)
    {
      case FRMTYPE_TABLE:
      {
        if (table_type == DB_TYPE_UNKNOWN) 
          my_error(ER_FILE_NOT_FOUND, MYF(0), name, my_errno);
        else
        {
          if (!(rc= mysql_rename_table(ha_resolve_by_legacy_type(thd,
                                                                 table_type),
                                       ren_table->db, old_alias,
                                       new_table->db, new_alias, 0)))
          {
            if ((rc= Table_triggers_list::change_table_name(thd, ren_table->db,
                                                            old_alias,
                                                            new_table->db,
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
                                        new_table->db, new_alias,
                                        ren_table->db, old_alias, 0);
            }
          }
        }
        break;
      }
      case FRMTYPE_VIEW:
        /* change of schema is not allowed */
        if (strcmp(ren_table->db, new_table->db))
          my_error(ER_FORBID_SCHEMA_CHANGE, MYF(0), ren_table->db, 
                   new_table->db);
        else
          rc= mysql_rename_view(thd, new_alias, ren_table);
        break;
      default:
        DBUG_ASSERT(0); // should never happen
      case FRMTYPE_ERROR:
        my_error(ER_FILE_NOT_FOUND, MYF(0), name, my_errno);
        break;
    }
    if (rc && !skip_error)
      DBUG_RETURN(ren_table);
  }
  DBUG_RETURN(0);
}
