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


/* Delete of records */

#include "mysql_priv.h"

/*
  Optimize delete of all rows by doing a full generate of the table
  This will work even if the .ISM and .ISD tables are destroyed
*/

int generate_table(THD *thd, TABLE_LIST *table_list,
			  TABLE *locked_table)
{
  char path[FN_REFLEN];
  int error;
  TABLE **table_ptr;
  DBUG_ENTER("generate_table");

  thd->proc_info="generate_table";
    /* If it is a temporary table, close and regenerate it */
  if ((table_ptr=find_temporary_table(thd,table_list->db,
				      table_list->real_name)))
  {
    TABLE *table= *table_ptr;
    HA_CREATE_INFO create_info;
    table->file->info(HA_STATUS_AUTO | HA_STATUS_NO_LOCK);
    bzero((char*) &create_info,sizeof(create_info));
    create_info.auto_increment_value= table->file->auto_increment_value;
    db_type table_type=table->db_type;

    strmov(path,table->path);
    *table_ptr= table->next;		// Unlink table from list
    close_temporary(table,0);
    *fn_ext(path)=0;				// Remove the .frm extension
    ha_create_table(path, &create_info,1);
    if ((error= (int) !(open_temporary_table(thd, path, table_list->db,
					     table_list->real_name, 1))))
    {
      (void) rm_temporary_table(table_type, path);
    }
  }
  else
  {
    (void) sprintf(path,"%s/%s/%s%s",mysql_data_home,table_list->db,
		   table_list->real_name,reg_ext);
    fn_format(path,path,"","",4);
    VOID(pthread_mutex_lock(&LOCK_open));
    if (locked_table)
      mysql_lock_abort(thd,locked_table);	 // end threads waiting on lock
    // close all copies in use
    if (remove_table_from_cache(thd,table_list->db,table_list->real_name))
    {
      if (!locked_table)
      {
	VOID(pthread_mutex_unlock(&LOCK_open));
	DBUG_RETURN(1);				// We must get a lock on table
      }
    }
    if (locked_table)
      locked_table->file->extra(HA_EXTRA_FORCE_REOPEN);
    if (thd->locked_tables)
      close_data_tables(thd,table_list->db,table_list->real_name);
    else
      close_thread_tables(thd,1);
    HA_CREATE_INFO create_info;
    bzero((char*) &create_info,sizeof(create_info));
    *fn_ext(path)=0;				// Remove the .frm extension
    error= ha_create_table(path,&create_info,1) ? -1 : 0;
    if (thd->locked_tables && reopen_tables(thd,1,0))
      error= -1;
    VOID(pthread_mutex_unlock(&LOCK_open));
  }
  if (!error)
  {
    send_ok(&thd->net);		// This should return record count
    mysql_update_log.write(thd,thd->query,thd->query_length);
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query);
      mysql_bin_log.write(&qinfo);
    }
  }
  DBUG_RETURN(error ? -1 : 0);
}


int mysql_delete(THD *thd,TABLE_LIST *table_list,COND *conds,ha_rows limit,
		 thr_lock_type lock_type)
{
  int		error;
  TABLE		*table;
  SQL_SELECT	*select;
  READ_RECORD	info;
  bool 		using_limit=limit != HA_POS_ERROR;
  bool	        use_generate_table;
  DBUG_ENTER("mysql_delete");

  if (!table_list->db)
    table_list->db=thd->db;
  if ((thd->options & OPTION_SAFE_UPDATES) && !conds)
  {
    send_error(&thd->net,ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE);
    DBUG_RETURN(1);
  }

  use_generate_table= (!using_limit && !conds &&
		       !(specialflag &
			 (SPECIAL_NO_NEW_FUNC | SPECIAL_SAFE_MODE)) &&
		       (thd->options & OPTION_AUTO_COMMIT));
  if (use_generate_table && ! thd->open_tables)
  {
    error=generate_table(thd,table_list,(TABLE*) 0);
    if (error <= 0)
      DBUG_RETURN(error);			// Error or ok
  }
  if (!(table = open_ltable(thd,table_list,
			    limit != HA_POS_ERROR ? TL_WRITE_LOW_PRIORITY :
			    lock_type)))
    DBUG_RETURN(-1);
  thd->proc_info="init";
  if (use_generate_table)
    DBUG_RETURN(generate_table(thd,table_list,table));
  table->map=1;
  if (setup_conds(thd,table_list,&conds))
    DBUG_RETURN(-1);

  table->used_keys=table->quick_keys=0;		// Can't use 'only index'
  select=make_select(table,0,0,conds,&error);
  if (error)
    DBUG_RETURN(-1);
  if (select && select->check_quick(test(thd->options & SQL_SAFE_UPDATES),
				    limit))
  {
    delete select;
    send_ok(&thd->net,0L);
    DBUG_RETURN(0);
  }

  /* If running in safe sql mode, don't allow updates without keys */
  if ((thd->options & OPTION_SAFE_UPDATES) && !table->quick_keys &&
      limit == HA_POS_ERROR)
  {
    delete select;
    send_error(&thd->net,ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE);
    DBUG_RETURN(1);
  }

  (void) table->file->extra(HA_EXTRA_NO_READCHECK);
  init_read_record(&info,thd,table,select,1,1);
  ulong deleted=0L;
  thd->proc_info="updating";
  while (!(error=info.read_record(&info)) && !thd->killed)
  {
    if (!(select && select->skipp_record()))
    {
      if (!(error=table->file->delete_row(table->record[0])))
      {
	deleted++;
	if (!--limit && using_limit)
	{
	  error= -1;
	  break;
	}
      }
      else
      {
	table->file->print_error(error,MYF(0));
	error=0;
	break;
      }
    }
  }
  thd->proc_info="end";
  end_read_record(&info);
  VOID(table->file->extra(HA_EXTRA_READCHECK));
  if (deleted)
  {
    mysql_update_log.write(thd,thd->query, thd->query_length);
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query);
      mysql_bin_log.write(&qinfo);
    }
  }
  if (ha_autocommit_or_rollback(thd,error >= 0))
    error=1;
  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;
  }
  delete select;
  if (error >= 0)				// Fatal error
    send_error(&thd->net,thd->killed ? ER_SERVER_SHUTDOWN: 0);
  else
  {
    send_ok(&thd->net,deleted);
    DBUG_PRINT("info",("%d records deleted",deleted));
  }
  DBUG_RETURN(0);
}


