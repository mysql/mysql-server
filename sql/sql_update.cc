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


/* Update of records */

#include "mysql_priv.h"
#include "sql_acl.h"

/* Return 0 if row hasn't changed */

static bool compare_record(TABLE *table, ulong query_id)
{
  if (!table->blob_fields)
    return cmp_record(table,1);
  if (memcmp(table->null_flags,
	     table->null_flags+table->rec_buff_length,
	     table->null_bytes))
    return 1;					// Diff in NULL value
  for (Field **ptr=table->field ; *ptr ; ptr++)
  {
    if ((*ptr)->query_id == query_id &&
	(*ptr)->cmp_binary_offset(table->rec_buff_length))
      return 1;
  }
  return 0;
}


int mysql_update(THD *thd,
                 TABLE_LIST *table_list,
                 List<Item> &fields,
		 List<Item> &values,
                 COND *conds,
                 ORDER *order,
		 ha_rows limit,
		 enum enum_duplicates handle_duplicates,
		 thr_lock_type lock_type)
{
  bool 		using_limit=limit != HA_POS_ERROR;
  bool		used_key_is_modified, using_transactions;
  int		error=0;
  uint		save_time_stamp, used_index, want_privilege;
  ulong		query_id=thd->query_id, timestamp_query_id;
  key_map	old_used_keys;
  TABLE		*table;
  SQL_SELECT	*select;
  READ_RECORD	info;
  DBUG_ENTER("mysql_update");
  LINT_INIT(used_index);
  LINT_INIT(timestamp_query_id);

  if (!(table = open_ltable(thd,table_list,lock_type)))
    DBUG_RETURN(-1); /* purecov: inspected */
  save_time_stamp=table->time_stamp;
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
  thd->proc_info="init";

  /* Calculate "table->used_keys" based on the WHERE */
  table->used_keys=table->keys_in_use;
  table->quick_keys=0;
  want_privilege=table->grant.want_privilege;
  table->grant.want_privilege=(SELECT_ACL & ~table->grant.privilege);
  if (setup_tables(table_list) || setup_conds(thd,table_list,&conds))
    DBUG_RETURN(-1);				/* purecov: inspected */
  old_used_keys=table->used_keys;		// Keys used in WHERE

  /*
    Change the query_id for the timestamp column so that we can
    check if this is modified directly
  */
  if (table->timestamp_field)
  {
    timestamp_query_id=table->timestamp_field->query_id;
    table->timestamp_field->query_id=thd->query_id-1;
  }

  /* Check the fields we are going to modify */
  table->grant.want_privilege=want_privilege;
  if (setup_fields(thd,table_list,fields,1,0))
    DBUG_RETURN(-1);				/* purecov: inspected */
  if (table->timestamp_field)
  {
    // Don't set timestamp column if this is modified
    if (table->timestamp_field->query_id == thd->query_id)
      table->time_stamp=0;
    else
      table->timestamp_field->query_id=timestamp_query_id;
  }

  /* Check values */
  table->grant.want_privilege=(SELECT_ACL & ~table->grant.privilege);
  if (setup_fields(thd,table_list,values,0,0))
  {
    table->time_stamp=save_time_stamp;		// Restore timestamp pointer
    DBUG_RETURN(-1);				/* purecov: inspected */
  }

  // Don't count on usage of 'only index' when calculating which key to use
  table->used_keys=0;
  select=make_select(table,0,0,conds,&error);
  if (error ||
      (select && select->check_quick(test(thd->options & SQL_SAFE_UPDATES),
				     limit)))
  {
    delete select;
    table->time_stamp=save_time_stamp;		// Restore timestamp pointer
    if (error)
    {
      DBUG_RETURN(-1);				// Error in where
    }
    send_ok(&thd->net);				// No matching records
    DBUG_RETURN(0);
  }
  /* If running in safe sql mode, don't allow updates without keys */
  if (!table->quick_keys)
  {
    thd->lex.select_lex.options|=QUERY_NO_INDEX_USED;
    if ((thd->options & OPTION_SAFE_UPDATES) && limit == HA_POS_ERROR)
    {
      delete select;
      table->time_stamp=save_time_stamp;
      send_error(&thd->net,ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE);
      DBUG_RETURN(1);
    }
  }
  /* Check if we are modifying a key that we are used to search with */
  if (select && select->quick)
    used_key_is_modified= (!select->quick->unique_key_range() &&
			   check_if_key_used(table,
					     (used_index=select->quick->index),
					     fields));
  else if ((used_index=table->file->key_used_on_scan) < MAX_KEY)
    used_key_is_modified=check_if_key_used(table, used_index, fields);
  else
    used_key_is_modified=0;
  if (used_key_is_modified)
  {
    /*
    ** We can't update table directly;  We must first search after all
    ** matching rows before updating the table!
    */
    table->file->extra(HA_EXTRA_DONT_USE_CURSOR_TO_UPDATE);
    IO_CACHE tempfile;
    if (open_cached_file(&tempfile, mysql_tmpdir,TEMP_PREFIX,
			  DISK_BUFFER_SIZE, MYF(MY_WME)))
    {
      delete select; /* purecov: inspected */
      table->time_stamp=save_time_stamp;	// Restore timestamp pointer /* purecov: inspected */
      DBUG_RETURN(-1);
    }
    if (old_used_keys & ((key_map) 1 << used_index))
    {
      table->key_read=1;
      table->file->extra(HA_EXTRA_KEYREAD);
    }

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
          (table->found_records = filesort(&table, sortorder, length,
                                           (SQL_SELECT *) 0, 0L,
					   HA_POS_ERROR, &examined_rows))
          == HA_POS_ERROR)
      {
	delete select;
	table->time_stamp=save_time_stamp;	// Restore timestamp pointer
	DBUG_RETURN(-1);
      }
    }

    init_read_record(&info,thd,table,select,0,1);
    thd->proc_info="searching";

    while (!(error=info.read_record(&info)) && !thd->killed)
    {
      if (!(select && select->skipp_record()))
      {
	table->file->position(table->record[0]);
	if (my_b_write(&tempfile,table->file->ref,
		       table->file->ref_length))
	{
	  error=1; /* purecov: inspected */
	  break; /* purecov: inspected */
	}
      }
      else
      {
	if (!(test_flags & 512))		/* For debugging */
	{
	  DBUG_DUMP("record",(char*) table->record[0],table->reclength);
	}
      }
    }
    end_read_record(&info);
    if (table->key_read)
    {
      table->key_read=0;
      table->file->extra(HA_EXTRA_NO_KEYREAD);
    }
    /* Change select to use tempfile */
    if (select)
    {
      delete select->quick;
      if (select->free_cond)
	delete select->cond;
      select->quick=0;
      select->cond=0;
    }
    else
    {
      select= new SQL_SELECT;
      select->head=table;
    }
    if (reinit_io_cache(&tempfile,READ_CACHE,0L,0,0))
      error=1; /* purecov: inspected */
    select->file=tempfile;			// Read row ptrs from this file
    if (error >= 0)
    {
      delete select;
      table->time_stamp=save_time_stamp;	// Restore timestamp pointer
      DBUG_RETURN(-1);
    }
  }

  if (!(test_flags & TEST_READCHECK))		/* For debugging */
    VOID(table->file->extra(HA_EXTRA_NO_READCHECK));
  if (handle_duplicates == DUP_IGNORE)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  init_read_record(&info,thd,table,select,0,1);

  ha_rows updated=0L,found=0L;
  thd->count_cuted_fields=1;			/* calc cuted fields */
  thd->cuted_fields=0L;
  thd->proc_info="updating";
  query_id=thd->query_id;

  while (!(error=info.read_record(&info)) && !thd->killed)
  {
    if (!(select && select->skipp_record()))
    {
      store_record(table,1);
      if (fill_record(fields,values))
	break; /* purecov: inspected */
      found++;
      if (compare_record(table, query_id))
      {
	if (!(error=table->file->update_row((byte*) table->record[1],
					    (byte*) table->record[0])))
	{
	  updated++;
	  if (!--limit && using_limit)
	  {
	    error= -1;
	    break;
	  }
	}
	else if (handle_duplicates != DUP_IGNORE ||
		 error != HA_ERR_FOUND_DUPP_KEY)
	{
	  table->file->print_error(error,MYF(0));
	  error= 1;
	  break;
	}
      }
    }
  }
  end_read_record(&info);
  thd->proc_info="end";
  VOID(table->file->extra(HA_EXTRA_READCHECK));
  VOID(table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY));
  table->time_stamp=save_time_stamp;	// Restore auto timestamp pointer
  using_transactions=table->file->has_transactions();
  if (updated && (error <= 0 || !using_transactions))
  {
    mysql_update_log.write(thd,thd->query,thd->query_length);
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query, using_transactions);
      if (mysql_bin_log.write(&qinfo) && using_transactions)
	error=1;
    }
    if (!using_transactions)
      thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
  }
  if (using_transactions && ha_autocommit_or_rollback(thd, error >= 0))
    error=1;
  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;
  }
  delete select;
  if (error >= 0)
    send_error(&thd->net,thd->killed ? ER_SERVER_SHUTDOWN : 0); /* purecov: inspected */
  else
  {
    char buff[80];
    sprintf(buff,ER(ER_UPDATE_INFO), (long) found, (long) updated,
	    (long) thd->cuted_fields);
    send_ok(&thd->net,
	    (thd->client_capabilities & CLIENT_FOUND_ROWS) ? found : updated,
	    thd->insert_id_used ? thd->insert_id() : 0L,buff);
    DBUG_PRINT("info",("%d records updated",updated));
  }
  thd->count_cuted_fields=0;			/* calc cuted fields */
  DBUG_RETURN(0);
}
