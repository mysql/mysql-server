/* Copyright (C) 2000 MySQL AB
   
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

/* Multi-table deletes were introduced by Monty and Sinisa */

#include "mysql_priv.h"
#include "ha_innobase.h"
#include "sql_select.h"

/*
  Optimize delete of all rows by doing a full generate of the table
  This will work even if the .ISM and .ISD tables are destroyed
*/

int generate_table(THD *thd, TABLE_LIST *table_list, TABLE *locked_table)
{
  char path[FN_REFLEN];
  int error;
  TABLE **table_ptr;
  DBUG_ENTER("generate_table");

  thd->proc_info="generate_table";

  if (global_read_lock)
  {
    if(thd->global_read_lock)
    {
      my_error(ER_TABLE_NOT_LOCKED_FOR_WRITE,MYF(0),
	       table_list->real_name);
      DBUG_RETURN(-1);
    }
    pthread_mutex_lock(&LOCK_open);
    while (global_read_lock && ! thd->killed ||
	   thd->version != refresh_version)
    {
      (void) pthread_cond_wait(&COND_refresh,&LOCK_open);
    }
    pthread_mutex_unlock(&LOCK_open);
  }

  
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
    mysql_update_log.write(thd,thd->query,thd->query_length);
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query);
      mysql_bin_log.write(&qinfo);
    }
    send_ok(&thd->net);		// This should return record count
  }
  DBUG_RETURN(error ? -1 : 0);
}


int mysql_delete(THD *thd,
                 TABLE_LIST *table_list,
                 COND *conds,
                 ORDER *order,
                 ha_rows limit,
		 thr_lock_type lock_type,
                 ulong options)
{
  int		error;
  TABLE		*table;
  SQL_SELECT	*select;
  READ_RECORD	info;
  bool 		using_limit=limit != HA_POS_ERROR;
  bool	        use_generate_table,using_transactions;
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
		       !(thd->options &
			 (OPTION_NOT_AUTO_COMMIT | OPTION_BEGIN)));
#ifdef HAVE_INNOBASE_DB
  /* We need to add code to not generate table based on the table type */
  if (!innodb_skip)
    use_generate_table=0;		// Innobase can't use re-generate table
#endif
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
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
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
  if ((select && select->check_quick(test(thd->options & SQL_SAFE_UPDATES),
				     limit)) || 
      !limit)
  {
    delete select;
    send_ok(&thd->net,0L);
    DBUG_RETURN(0);				// Nothing to delete
  }

  /* If running in safe sql mode, don't allow updates without keys */
  if (!table->quick_keys)
  {
    thd->lex.select_lex.options|=QUERY_NO_INDEX_USED;
    if ((thd->options & OPTION_SAFE_UPDATES) && limit == HA_POS_ERROR)
    {
      delete select;
      send_error(&thd->net,ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE);
      DBUG_RETURN(1);
    }
  }
  (void) table->file->extra(HA_EXTRA_NO_READCHECK);
  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_QUICK);

  if (order)
  {
    uint         length;
    SORT_FIELD  *sortorder;
    TABLE_LIST   tables;
    List<Item>   fields;
    List<Item>   all_fields;
    ha_rows examined_rows;

    bzero((char*) &tables,sizeof(tables));
    tables.table = table;

    table->io_cache = (IO_CACHE *) my_malloc(sizeof(IO_CACHE),
                                             MYF(MY_FAE | MY_ZEROFILL));
    if (setup_order(thd, &tables, fields, all_fields, order) ||
        !(sortorder=make_unireg_sortorder(order, &length)) ||
        (table->found_records = filesort(table, sortorder, length,
                                        (SQL_SELECT *) 0, 0L, HA_POS_ERROR,
					 &examined_rows))
        == HA_POS_ERROR)
    {
      delete select;
      DBUG_RETURN(-1);		// This will force out message
    }
  }

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
  /* if (order) free_io_cache(table); */ /* QQ Should not be needed */
  (void) table->file->extra(HA_EXTRA_READCHECK);
  if (options & OPTION_QUICK)
    (void) table->file->extra(HA_EXTRA_NORMAL);
  using_transactions=table->file->has_transactions();
  if (deleted && (error <= 0 || !using_transactions))
  {
    mysql_update_log.write(thd,thd->query, thd->query_length);
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query, using_transactions);
      if (mysql_bin_log.write(&qinfo) && using_transactions)
	error=1;
    }
    if (!using_transactions)
      thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
  }
  if (using_transactions && ha_autocommit_or_rollback(thd,error >= 0))
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


/***************************************************************************
** delete multiple tables from join 
***************************************************************************/

#define MEM_STRIP_BUF_SIZE sortbuff_size

int refposcmp2(void* arg, const void *a,const void *b)
{
  return memcmp(a,b,(int) arg);
}

multi_delete::multi_delete(THD *thd_arg, TABLE_LIST *dt,
			   thr_lock_type lock_option_arg,
			   uint num_of_tables_arg)
  : delete_tables (dt), thd(thd_arg), deleted(0),
    num_of_tables(num_of_tables_arg), error(0), lock_option(lock_option_arg),
    do_delete(false)
{
  uint counter=0;
  tempfiles = (Unique **) sql_calloc(sizeof(Unique *) * (num_of_tables-1));

  (void) dt->table->file->extra(HA_EXTRA_NO_READCHECK);
  /* Don't use key read with MULTI-TABLE-DELETE */
  (void) dt->table->file->extra(HA_EXTRA_NO_KEYREAD);
  dt->table->used_keys=0;
  for (dt=dt->next ; dt ; dt=dt->next,counter++)
  {
    TABLE *table=dt->table;
    (void) dt->table->file->extra(HA_EXTRA_NO_READCHECK);
    (void) dt->table->file->extra(HA_EXTRA_NO_KEYREAD);
    tempfiles[counter] = new Unique (refposcmp2,
				     (void *) table->file->ref_length,
				     table->file->ref_length,
				     MEM_STRIP_BUF_SIZE);
  }
}


int
multi_delete::prepare(List<Item> &values)
{
  DBUG_ENTER("multi_delete::prepare");
  do_delete = true;   
  thd->proc_info="deleting from main table";

  if (thd->options & OPTION_SAFE_UPDATES)
  {
    TABLE_LIST *table_ref;
    for (table_ref=delete_tables;  table_ref; table_ref=table_ref->next)
    {
      TABLE *table=table_ref->table;
      if ((thd->options & OPTION_SAFE_UPDATES) && !table->quick_keys)
      {
	my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,MYF(0));
	DBUG_RETURN(1);
      }
    }
  }
  DBUG_RETURN(0);
}


void
multi_delete::initialize_tables(JOIN *join)
{
  TABLE_LIST *walk;
  table_map tables_to_delete_from=0;
  for (walk= delete_tables ; walk ; walk=walk->next)
    tables_to_delete_from|= walk->table->map;
  
  walk= delete_tables;
  for (JOIN_TAB *tab=join->join_tab, *end=join->join_tab+join->tables;
       tab < end;
       tab++)
  {
    if (tab->table->map & tables_to_delete_from)
    {
      /* We are going to delete from this table */
      walk->table=tab->table;
      walk=walk->next;
    }
  }
}


multi_delete::~multi_delete()
{
  /* Add back EXTRA_READCHECK;  In 4.0.1 we shouldn't need this anymore */
  for (table_being_deleted=delete_tables ;
       table_being_deleted ;
       table_being_deleted=table_being_deleted->next)
    (void) table_being_deleted->table->file->extra(HA_EXTRA_READCHECK);

  for (uint counter = 0; counter < num_of_tables-1; counter++)
  {
    if (tempfiles[counter])
      delete tempfiles[counter];
  }
}


bool multi_delete::send_data(List<Item> &values)
{
  int secure_counter= -1;
  for (table_being_deleted=delete_tables ;
       table_being_deleted ;
       table_being_deleted=table_being_deleted->next, secure_counter++)
  {
    TABLE *table=table_being_deleted->table;

    /* Check if we are using outer join and we didn't find the row */
    if (table->status & (STATUS_NULL_ROW | STATUS_DELETED))
      continue;

    table->file->position(table->record[0]);
    int rl = table->file->ref_length;
    
    if (secure_counter < 0)
    {
      table->status|= STATUS_DELETED;
      if (!(error=table->file->delete_row(table->record[0])))
	deleted++;
      else
      {
	table->file->print_error(error,MYF(0));
	return 1;
      }
    }
    else
    {
      error=tempfiles[secure_counter]->unique_add(table->file->ref);
      if (error)
      {
	error=-1;
	return 1;
      }
    }
  }
  return 0;
}



/* Return true if some table is not transaction safe */

static bool some_table_is_not_transaction_safe (TABLE_LIST *tl)
{
  for (; tl ; tl=tl->next)
  { 
    if (!(tl->table->file->has_transactions()))
      return true;
  }
  return false;
}


void multi_delete::send_error(uint errcode,const char *err)
{
  /* First send error what ever it is ... */
  ::send_error(&thd->net,errcode,err);
  /* If nothing deleted return */
  if (!deleted)
    return;
  /* Below can happen when thread is killed early ... */
  if (!table_being_deleted)
    table_being_deleted=delete_tables;

  /*
    If rows from the first table only has been deleted and it is transactional,
    just do rollback.
    The same if all tables are transactional, regardless of where we are.
    In all other cases do attempt deletes ...
  */
  if ((table_being_deleted->table->file->has_transactions() &&
       table_being_deleted == delete_tables) ||
      !some_table_is_not_transaction_safe(delete_tables->next))
    ha_rollback(thd);
  else if (do_delete)
    VOID(do_deletes(true));
}


int multi_delete::do_deletes (bool from_send_error)
{
  int error = 0, counter = 0, count;

  if (from_send_error)
  {
    /* Found out table number for 'table_being_deleted' */
    for (TABLE_LIST *aux=delete_tables;
	 aux != table_being_deleted;
	 aux=aux->next)
      counter++;
  }
  else
    table_being_deleted = delete_tables;

  do_delete = false;
  for (table_being_deleted=table_being_deleted->next;
       table_being_deleted ;
       table_being_deleted=table_being_deleted->next, counter++)
  { 
    TABLE *table = table_being_deleted->table;
    int rl = table->file->ref_length;
    if (tempfiles[counter]->get(table))
    {
      error=1;
      break;
    }

#if USE_REGENERATE_TABLE
    // nice little optimization ....
    // but Monty has to fix generate_table...
    // This will not work for transactional tables because for other types
    // records is not absolute
    if (num_of_positions == table->file->records) 
    {
      TABLE_LIST table_list;
      bzero((char*) &table_list,sizeof(table_list));
      table_list.name=table->table_name; table_list.real_name=table_being_deleted->real_name;
      table_list.table=table;
      table_list.grant=table->grant;
      table_list.db = table_being_deleted->db;
      error=generate_table(thd,&table_list,(TABLE *)0);
      if (error <= 0) {error = 1; break;}
      deleted += num_of_positions;
      continue;
    }
#endif /* USE_REGENERATE_TABLE */

    READ_RECORD	info;
    error=0;
    init_read_record(&info,thd,table,NULL,0,0);
    bool not_trans_safe = some_table_is_not_transaction_safe(delete_tables);
    while (!(error=info.read_record(&info)) &&
	   (!thd->killed ||  from_send_error || not_trans_safe))
    {
      error=table->file->delete_row(table->record[0]);
      if (error)
      {
	table->file->print_error(error,MYF(0));
	break;
      }
      else
	deleted++;
    }
    end_read_record(&info);
    if (error == -1)
      error = 0;
  }
  return error;
}


bool multi_delete::send_eof()
{
  thd->proc_info="deleting from reference tables";
  int error = do_deletes(false);

  thd->proc_info="end";
  if (error && error != -1)
  {
    ::send_error(&thd->net);
    return 1;
  }

  if (deleted &&
      (error <= 0 || some_table_is_not_transaction_safe(delete_tables)))
  {
    mysql_update_log.write(thd,thd->query,thd->query_length);
    Query_log_event qinfo(thd, thd->query);
    if (mysql_bin_log.write(&qinfo) &&
	!some_table_is_not_transaction_safe(delete_tables))
      error=1;					// Rollback
    VOID(ha_autocommit_or_rollback(thd,error >= 0));
  }
  ::send_ok(&thd->net,deleted);
  return 0;
}
