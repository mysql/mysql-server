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


/* Update of records 

   Multi-table updates were introduced by Monty and Sinisa <sinisa@mysql.com>

*/

#include "mysql_priv.h"
#include "sql_acl.h"
#include "sql_select.h"

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
  if (setup_tables(table_list) || setup_conds(thd,table_list,&conds)
                               || setup_ftfuncs(thd))
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
  if (setup_fields(thd,table_list,fields,1,0,0))
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
  if (setup_fields(thd,table_list,values,0,0,0))
  {
    table->time_stamp=save_time_stamp;		// Restore timestamp pointer
    DBUG_RETURN(-1);				/* purecov: inspected */
  }

  // Don't count on usage of 'only index' when calculating which key to use
  table->used_keys=0;
  select=make_select(table,0,0,conds,&error);
  if (error ||
      (select && select->check_quick(test(thd->options & SQL_SAFE_UPDATES),
				     limit)) ||
      !limit)
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
  init_ftfuncs(thd,1);
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
  if (used_key_is_modified || order)
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
          (table->found_records = filesort(table, sortorder, length,
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
    thd->proc_info="Searching rows for update";

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
  thd->proc_info="Updating";
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
    else
      table->file->unlock_row();
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
  if (updated)
    query_cache.invalidate(table_list);
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
  free_io_cache(table);
  DBUG_RETURN(0);
}

/***************************************************************************
** update  multiple tables from join 
***************************************************************************/

multi_update::multi_update(THD *thd_arg, TABLE_LIST *ut, List<Item> &fs, 		 
			   enum enum_duplicates handle_duplicates,  thr_lock_type lock_option_arg, uint num)
  : update_tables (ut), thd(thd_arg), updated(0), found(0), fields(fs), lock_option(lock_option_arg),
    dupl(handle_duplicates), num_of_tables(num), num_fields(0), num_updated(0) , error(0),  do_update(false)
{
  save_time_stamps = (uint *) sql_calloc (sizeof(uint) * num_of_tables);
  tmp_tables = (TABLE **)NULL;
  int counter=0;
  ulong timestamp_query_id;
  for (TABLE_LIST *dt=ut ; dt ; dt=dt->next,counter++)
  {
    TABLE *table=ut->table;
    (void) ut->table->file->extra(HA_EXTRA_NO_READCHECK);
    (void) ut->table->file->extra(HA_EXTRA_NO_KEYREAD);
    dt->table->used_keys=0;
    if (table->timestamp_field)
    {
      // Don't set timestamp column if this is modified
      timestamp_query_id=table->timestamp_field->query_id;
      table->timestamp_field->query_id=thd->query_id-1;
      if (table->timestamp_field->query_id == thd->query_id)
	table->time_stamp=0;
      else
	table->timestamp_field->query_id=timestamp_query_id;
    }
    save_time_stamps[counter]=table->time_stamp;
  }
  error = 1; // In case we do not reach prepare we have to reset timestamps
}

int
multi_update::prepare(List<Item> &values)
{
  DBUG_ENTER("multi_update::prepare");
  do_update = true;   
  thd->count_cuted_fields=1;
  thd->cuted_fields=0L;
  thd->proc_info="updating the  main table";
  TABLE_LIST *table_ref;

  if (thd->options & OPTION_SAFE_UPDATES)
  {
    for (table_ref=update_tables;  table_ref; table_ref=table_ref->next)
    {
      TABLE *table=table_ref->table;
      if ((thd->options & OPTION_SAFE_UPDATES) && !table->quick_keys)
      {
	my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,MYF(0));
	DBUG_RETURN(1);
      }
    }
  }
// Here I have to connect fields with tables and only update tables that need to be updated ...

// I calculate num_updated and fill-up table_sequence
// Set table_list->shared  to true or false, depending on whether table is to be updated or not
  Item_field *item;
  List_iterator<Item> it(fields);
  num_fields=fields.elements;
  field_sequence = (uint *) sql_alloc(sizeof(uint)*num_fields);
  uint *int_ptr=field_sequence;
  while ((item= (Item_field *)it++))
  {
    unsigned int counter=0;
    for (table_ref=update_tables;  table_ref; table_ref=table_ref->next, counter++)
    {
      if (table_ref->table == item->field->table && !table_ref->shared)
      {
	num_updated++;
	table_ref->shared=1;
	break;
      }
    }
    if (!table_ref)
    {
      error = 1; // A proper error message is due here 
      DBUG_RETURN(1);
    }
    else
      *int_ptr++=counter;
  }
  if (!num_updated)
  {
    error = 1; // A proper error message is due here 
    DBUG_RETURN(1);
  }

// Here, I have to allocate the array of temporary tables
// I have to treat a case of num_updated=1 differently in send_data() method. 
  if (num_updated > 1)
  {
    tmp_tables = (TABLE **) sql_calloc(sizeof(TABLE *) * (num_updated - 1));
    infos = (COPY_INFO *) sql_calloc(sizeof(COPY_INFO) * (num_updated - 1));
    fields_by_tables = (List_item **)sql_calloc(sizeof(List_item *) * num_updated);
    unsigned int counter;
    List<Item> *temp_fields;
    for (table_ref=update_tables, counter = 0;  table_ref; table_ref=table_ref->next)
    {
      if (!table_ref->shared) 
	continue;
// Here we have to add row offset as an additional field ...
      if (!(temp_fields = (List_item *)sql_calloc(sizeof(List_item))))
      {
	error = 1; // A proper error message is due here 
	DBUG_RETURN(1);
      }
      temp_fields->empty();
      it.rewind(); int_ptr=field_sequence;
      while ((item= (Item_field *)it++))
      {
	if (*int_ptr++ == counter)
	  temp_fields->push_back(item);
      }
      if (counter)
      {
	Field_string offset(table_ref->table->file->ref_length,false,"offset",table_ref->table,true);
	temp_fields->push_front(new Item_field(((Field *)&offset)));
// Here I make tmp tables
	int cnt=counter-1;
	TMP_TABLE_PARAM tmp_table_param;
	bzero((char*) &tmp_table_param,sizeof(tmp_table_param));
	tmp_table_param.field_count=temp_fields->elements;
	if (!(tmp_tables[cnt]=create_tmp_table(thd, &tmp_table_param, *temp_fields,
						   (ORDER*) 0, 1, 0, 0, TMP_TABLE_ALL_COLUMNS)))
	{
	  error = 1; // A proper error message is due here 
	  DBUG_RETURN(1);
	}
	tmp_tables[cnt]->file->extra(HA_EXTRA_WRITE_CACHE);
	tmp_tables[cnt]->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
	infos[cnt].handle_duplicates=DUP_IGNORE;
	temp_fields->pop(); // because we shall use those for values only ...
      }
      fields_by_tables[counter]=temp_fields;
      counter++;
    }
  }
  error = 0; // Timestamps do not need to be restored, so far ...
  DBUG_RETURN(0);
}


void
multi_update::initialize_tables(JOIN *join)
{
/*   We skip it as it only makes a mess ...........
  TABLE_LIST *walk;
  table_map tables_to_update_from=0;
  for (walk= update_tables ; walk ; walk=walk->next)
    tables_to_update_from|= walk->table->map;
  
  walk= update_tables;
  for (JOIN_TAB *tab=join->join_tab, *end=join->join_tab+join->tables;
       tab < end;
       tab++)
  {
    if (tab->table->map & tables_to_update_from)
    {
       We are going to update from this table 
      walk->table=tab->table;
      walk=walk->next;
      if (tab == join->join_tab)
	tab->table->no_keyread=1;
    }
  }
*/
}


multi_update::~multi_update()
{
  /* Add back EXTRA_READCHECK;  In 4.0.1 we shouldn't need this anymore */
  int counter = 0;
  for (table_being_updated=update_tables ;
       table_being_updated ;
       counter++, table_being_updated=table_being_updated->next)
  {
    TABLE *table=table_being_updated->table;
    (void)table->file->extra(HA_EXTRA_READCHECK);
    if (error) 
      table->time_stamp=save_time_stamps[counter];
  }
  if (tmp_tables)
    for (uint counter = 0; counter < num_updated-1; counter++)
      if (tmp_tables[counter])
	free_tmp_table(thd,tmp_tables[counter]);
}


bool multi_update::send_data(List<Item> &values)
{
  List<Item> real_values(values);
  for (uint counter = 0; counter < fields.elements; counter++)
    real_values.pop();
// We have skipped fields ....
  if (num_updated == 1)
  {
    for (table_being_updated=update_tables ;
	 table_being_updated ;
	 table_being_updated=table_being_updated->next)
    {
      if (!table_being_updated->shared) 
	continue;
      TABLE *table=table_being_updated->table;
      /* Check if we are using outer join and we didn't find the row */
      if (table->status & (STATUS_NULL_ROW | STATUS_UPDATED))
	return 0;
      table->file->position(table->record[0]);
// Only one table being updated receives a completely different treatment
      table->status|= STATUS_UPDATED;
      store_record(table,1); 
      if (fill_record(fields,real_values))
	return 1;
      found++;
      if (/* compare_record(table, query_id)  && */
	  !(error=table->file->update_row(table->record[1], table->record[0])))
	updated++;
      return error;
    }
  }
  else
  {
    int secure_counter= -1;
    for (table_being_updated=update_tables ;
	 table_being_updated ;
	 table_being_updated=table_being_updated->next, secure_counter++)
    {
      if (!table_being_updated->shared) 
	continue;
      
      TABLE *table=table_being_updated->table;
      /* Check if we are using outer join and we didn't find the row */
      if (table->status & (STATUS_NULL_ROW | STATUS_UPDATED))
	continue;
      table->file->position(table->record[0]);
      Item *item;
      List_iterator<Item> it(real_values);
      List <Item> values_by_table;
      uint *int_ptr=field_sequence;
      while ((item= (Item *)it++))
      {
	if (*int_ptr++ == (uint) (secure_counter + 1))
	  values_by_table.push_back(item);
      }
// Here I am breaking values as per each table    
      if (secure_counter < 0)
      {
	table->status|= STATUS_UPDATED;
	store_record(table,1); 
	if (fill_record(*fields_by_tables[0],values_by_table))
	  return 1;
	found++;
	if (/*compare_record(table, query_id)  && */
	    !(error=table->file->update_row(table->record[1], table->record[0])))
	  updated++;
	else
	{
	  table->file->print_error(error,MYF(0));
	  if (!error) error=1;
	  return 1;
	}
      }
      else
      {
// Here I insert into each temporary table
	values_by_table.push_front(new Item_string(table->file->ref,table->file->ref_length));
	fill_record(tmp_tables[secure_counter]->field,values_by_table);
	error= write_record(tmp_tables[secure_counter],&(infos[secure_counter]));
	if (error)
	{
	  error=-1;
	  return 1;
	}
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


void multi_update::send_error(uint errcode,const char *err)
{
  /* First send error what ever it is ... */
  ::send_error(&thd->net,errcode,err);

  /* reset used flags */
  update_tables->table->no_keyread=0;

  /* If nothing updated return */
  if (!updated)
    return;
  /* Below can happen when thread is killed early ... */
  if (!table_being_updated)
    table_being_updated=update_tables;

  /*
    If rows from the first table only has been updated and it is transactional,
    just do rollback.
    The same if all tables are transactional, regardless of where we are.
    In all other cases do attempt updates ...
  */
  if ((table_being_updated->table->file->has_transactions() &&
       table_being_updated == update_tables) ||
      !some_table_is_not_transaction_safe(update_tables->next))
    ha_rollback_stmt(thd);
  else if (do_update)
    VOID(do_updates(true));
}


int multi_update::do_updates (bool from_send_error)
{
  int error = 0, counter = 0;

  if (num_updated == 1) return 0;
  if (from_send_error)
  {
    /* Found out table number for 'table_being_updated' */
    for (TABLE_LIST *aux=update_tables;
	 aux != table_being_updated;
	 aux=aux->next)
      counter++;
  }
  else
    table_being_updated = update_tables;

  do_update = false;
  for (table_being_updated=table_being_updated->next;
       table_being_updated ;
       table_being_updated=table_being_updated->next, counter++)
  { 
    if (!table_being_updated->shared) 
      continue;

    TABLE *table = table_being_updated->table;
    TABLE *tmp_table=tmp_tables[counter];
    if (tmp_table->file->extra(HA_EXTRA_NO_CACHE))
    {
      error=1;
      break;
    }
    List<Item> list;
    Field **ptr=tmp_table->field,*field;
// This is supposed to be something like insert_fields
    thd->used_tables|=tmp_table->map;
    while ((field = *ptr++))
    {
      list.push_back((Item *)new Item_field(field));
      if (field->query_id == thd->query_id)
	thd->dupp_field=field;
      field->query_id=thd->query_id;
      tmp_table->used_keys&=field->part_of_key;
    }
    tmp_table->used_fields=tmp_table->fields;
    error=0; list.pop(); // we get position some other way ...
    error = tmp_table->file->rnd_init(1);
    if (error) 
      return error;
    bool not_trans_safe = some_table_is_not_transaction_safe(update_tables);
    while (!(error=tmp_table->file->rnd_next(tmp_table->record[0])) &&
	   (!thd->killed ||  from_send_error || not_trans_safe))
    {
      found++; 
      error= table->file->rnd_pos(table->record[0], (*(tmp_table->field))->ptr);
      if (error)
	return error;
      table->status|= STATUS_UPDATED;
      store_record(table,1); 
      error= fill_record(*fields_by_tables[counter + 1],list) /*|| compare_record(table, query_id)*/  ||
	table->file->update_row(table->record[1],table->record[0]);
      if (error)
      {
	table->file->print_error(error,MYF(0));
	break;
      }
      else
	updated++;
    }
    if (error == HA_ERR_END_OF_FILE)
      error = 0;
  }
  return error;
}


bool multi_update::send_eof()
{
  thd->proc_info="updating the  reference tables";  /* out: 1 if error, 0 if success */

  /* Does updates for the last n - 1 tables, returns 0 if ok */
  int error = do_updates(false);   /* do_updates returns 0 if success */

  /* reset used flags */
  update_tables->table->no_keyread=0;
  if (error == -1) error = 0;
  thd->proc_info="end";
  if (error)
    send_error(error,"An error occured in multi-table update");

  /* Write the SQL statement to the binlog if we updated
   rows and we succeeded, or also in an error case when there
   was a non-transaction-safe table involved, since
   modifications in it cannot be rolled back. */

  if (updated &&
      (!error || some_table_is_not_transaction_safe(update_tables)))
  {
    mysql_update_log.write(thd,thd->query,thd->query_length);
    Query_log_event qinfo(thd, thd->query);

    /* mysql_bin_log is not open if binlogging or replication
    is not used */

    if (mysql_bin_log.is_open() &&  mysql_bin_log.write(&qinfo) &&
	!some_table_is_not_transaction_safe(update_tables))
      error=1;  /* Log write failed: roll back
		   the SQL statement */

    /* Commit or rollback the current SQL statement */ 

    VOID(ha_autocommit_or_rollback(thd,error > 0));
  }
  else
    error=0; // this can happen only if it is end of file error
  if (!error) // if the above log write did not fail ...
  {
    char buff[80];
    sprintf(buff,ER(ER_UPDATE_INFO), (long) found, (long) updated,
	    (long) thd->cuted_fields);
    if (updated)
      query_cache.invalidate(update_tables);
    ::send_ok(&thd->net,
	    (thd->client_capabilities & CLIENT_FOUND_ROWS) ? found : updated,
	    thd->insert_id_used ? thd->insert_id() : 0L,buff);
  }
  thd->count_cuted_fields=0;
  return 0;
}
