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


static TABLE_LIST *rename_tables(THD *thd, TABLE_LIST *table_list,
				 bool skip_error);

/*
  Every second entry in the table_list is the original name and every
  second entry is the new name.
*/

bool mysql_rename_tables(THD *thd, TABLE_LIST *table_list)
{
  bool error=1,got_all_locks=1;
  db_type table_type;
  TABLE_LIST *lock_table,*ren_table=0,*new_table;
  DBUG_ENTER("mysql_rename_tables");
  
  /* Avoid problems with a rename on a table that we have locked or
     if the user is trying to to do this in a transcation context */

  if (thd->locked_tables || thd->active_transaction())
  {
    my_error(ER_LOCK_OR_ACTIVE_TRANSACTION,MYF(0));
    DBUG_RETURN(1);
  }
      
  VOID(pthread_mutex_lock(&LOCK_open));
  for (lock_table=table_list ; lock_table ; lock_table=lock_table->next)
  {
    int got_lock;
    if ((got_lock=lock_table_name(thd,lock_table)) < 0)
      goto end;
    if (got_lock)
      got_all_locks=0;
  }
  
  if (!got_all_locks && wait_for_locked_table_names(thd,table_list))
    goto end;

  if (!(ren_table=rename_tables(thd,table_list,0)))
    error=0;
  
end:
  if (ren_table)
  {
    /* Rename didn't succeed;  rename back the tables in reverse order */
    TABLE_LIST *prev=0,*table;
    /* Reverse the table list */

    while (table_list)
    {
      TABLE_LIST *next=table_list->next;
      table_list->next=prev;
      prev=table_list;
      table_list=next;
    }
    table_list=prev;

    /* Find the last renamed table */
    for (table=table_list ;
	 table->next != ren_table ;
	 table=table->next->next) ;
    table=table->next->next;			// Skipp error table
    /* Revert to old names */
    rename_tables(thd, table, 1);
    /* Note that lock_table == 0 here, so the unlock loop will work */
  }
  if (!error)
  {
    mysql_update_log.write(thd->query,thd->query_length);
    Query_log_event qinfo(thd, thd->query);
    mysql_bin_log.write(&qinfo);
    send_ok(&thd->net);
  }
  for (TABLE_LIST *table=table_list ; table != lock_table ; table=table->next)
    unlock_table_name(thd,table);
  pthread_cond_broadcast(&COND_refresh);
  pthread_mutex_unlock(&LOCK_open);
  DBUG_RETURN(error);
}


/*
  Rename all tables in list; Return pointer to wrong entry if something goes
  wrong.  Note that the table_list may be empty!
*/

static TABLE_LIST *
rename_tables(THD *thd, TABLE_LIST *table_list, bool skip_error)
{
  TABLE_LIST *ren_table,*new_table;
  DBUG_ENTER("rename_tables");

  for (ren_table=table_list ; ren_table ; ren_table=new_table->next)
  {
    db_type table_type;
    char name[FN_REFLEN];
    new_table=ren_table->next;

    sprintf(name,"%s/%s/%s%s",mysql_data_home,
	    new_table->db,new_table->name,
	    reg_ext);
    if (!access(name,F_OK))
    {
      my_error(ER_TABLE_EXISTS_ERROR,MYF(0),name);
      return ren_table;				// This can't be skipped
    }
    sprintf(name,"%s/%s/%s%s",mysql_data_home,
	    ren_table->db,ren_table->name,
	    reg_ext);
    if ((table_type=get_table_type(name)) == DB_TYPE_UNKNOWN)
    {
      my_error(ER_FILE_NOT_FOUND, MYF(0), name, my_errno);
      if (!skip_error)
	return ren_table;
    }
    else if (mysql_rename_table(table_type,
				ren_table->db, ren_table->name,
				new_table->db, new_table->name))
    {
      if (!skip_error)
	return ren_table;
    }
  }
  DBUG_RETURN(0);
}
