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
  Single table and multi table updates of tables.
  Multi-table updates were introduced by Sinisa & Monty
*/

#include "mysql_priv.h"
#include "sql_select.h"

static bool safe_update_on_fly(JOIN_TAB *join_tab, List<Item> *fields);

/* Return 0 if row hasn't changed */

static bool compare_record(TABLE *table, ulong query_id)
{
  if (!table->blob_fields)
    return cmp_record(table,record[1]);
  /* Compare null bits */
  if (memcmp(table->null_flags,
	     table->null_flags+table->rec_buff_length,
	     table->null_bytes))
    return 1;					// Diff in NULL value
  /* Compare updated fields */
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
                 uint order_num, ORDER *order,
		 ha_rows limit,
		 enum enum_duplicates handle_duplicates,
                 bool ignore)
{
  bool		using_limit=limit != HA_POS_ERROR;
  bool		safe_update= thd->options & OPTION_SAFE_UPDATES;
  bool		used_key_is_modified, transactional_table, log_delayed;
  int		error=0;
  uint		used_index= MAX_KEY;
  bool          need_sort= TRUE;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  uint		want_privilege;
#endif
  ulong		query_id=thd->query_id, timestamp_query_id;
  ha_rows	updated, found;
  key_map	old_used_keys;
  TABLE		*table;
  SQL_SELECT	*select;
  READ_RECORD	info;
  TABLE_LIST    *update_table_list= ((TABLE_LIST*)
				     thd->lex->select_lex.table_list.first);
  DBUG_ENTER("mysql_update");

  LINT_INIT(timestamp_query_id);

  if ((open_and_lock_tables(thd, table_list)))
    DBUG_RETURN(-1);
  thd->proc_info="init";
  table= table_list->table;
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  /* Calculate "table->used_keys" based on the WHERE */
  table->used_keys=table->keys_in_use;
  table->quick_keys.clear_all();

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  want_privilege= table->grant.want_privilege;
#endif
  if ((error= mysql_prepare_update(thd, table_list, update_table_list,
				   &conds, order_num, order)))
    DBUG_RETURN(error);

  old_used_keys= table->used_keys;		// Keys used in WHERE
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
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  table->grant.want_privilege=want_privilege;
#endif
  if (setup_fields(thd, 0, update_table_list, fields, 1, 0, 0))
    DBUG_RETURN(-1);				/* purecov: inspected */
  if (table->timestamp_field)
  {
    // Don't set timestamp column if this is modified
    if (table->timestamp_field->query_id == thd->query_id)
      table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
    else
      table->timestamp_field->query_id=timestamp_query_id;
  }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  /* Check values */
  table->grant.want_privilege=(SELECT_ACL & ~table->grant.privilege);
#endif
  if (setup_fields(thd, 0, update_table_list, values, 1, 0, 0))
  {
    free_underlaid_joins(thd, &thd->lex->select_lex);
    DBUG_RETURN(-1);				/* purecov: inspected */
  }

  // Don't count on usage of 'only index' when calculating which key to use
  table->used_keys.clear_all();
  select=make_select(table,0,0,conds,&error);
  if (error ||
      (select && select->check_quick(thd, safe_update, limit)) || !limit)
  {
    delete select;
    free_underlaid_joins(thd, &thd->lex->select_lex);
    if (error)
    {
      DBUG_RETURN(-1);				// Error in where
    }
    send_ok(thd);				// No matching records
    DBUG_RETURN(0);
  }
  if (!select && limit != HA_POS_ERROR)
  {
    if ((used_index= get_index_for_order(table, order, limit)) != MAX_KEY)
      need_sort= FALSE;
  }
  /* If running in safe sql mode, don't allow updates without keys */
  if (table->quick_keys.is_clear_all())
  {
    thd->server_status|=SERVER_QUERY_NO_INDEX_USED;
    if (safe_update && !using_limit)
    {
      my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
		 ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
      goto err;
    }
  }
  init_ftfuncs(thd, &thd->lex->select_lex, 1);
  
  /* Check if we are modifying a key that we are used to search with */
  if (select && select->quick)
  {
    used_index=select->quick->index;
    used_key_is_modified= (!select->quick->unique_key_range() &&
			   check_if_key_used(table, used_index, fields));
  }
  else
  {
    used_key_is_modified= 0;
    if (used_index == MAX_KEY)                  // no index for sort order
      used_index= table->file->key_used_on_scan;
    if (used_index != MAX_KEY)
      used_key_is_modified= check_if_key_used(table, used_index, fields);
  }

  if (used_key_is_modified || order)
  {
    /*
      We can't update table directly;  We must first search after all
      matching rows before updating the table!
    */
    table->file->extra(HA_EXTRA_RETRIEVE_ALL_COLS);
    if (used_index < MAX_KEY && old_used_keys.is_set(used_index))
    {
      table->key_read=1;
      table->file->extra(HA_EXTRA_KEYREAD);
    }

    /* note: can actually avoid sorting below.. */
    if (order && (need_sort || used_key_is_modified))
    {
      /*
	Doing an ORDER BY;  Let filesort find and sort the rows we are going
	to update
      */
      uint         length;
      SORT_FIELD  *sortorder;
      ha_rows examined_rows;

      used_index= MAX_KEY;                   // For call to init_read_record()
      table->sort.io_cache = (IO_CACHE *) my_malloc(sizeof(IO_CACHE),
						    MYF(MY_FAE | MY_ZEROFILL));
      if (!(sortorder=make_unireg_sortorder(order, &length)) ||
          (table->sort.found_records = filesort(thd, table, sortorder, length,
						select, limit,
						&examined_rows))
          == HA_POS_ERROR)
      {
	free_io_cache(table);
	goto err;
      }
      /*
	Filesort has already found and selected the rows we want to update,
	so we don't need the where clause
      */
      delete select;
      select= 0;
    }
    else
    {
      /*
	We are doing a search on a key that is updated. In this case
	we go trough the matching rows, save a pointer to them and
	update these in a separate loop based on the pointer.
      */

      IO_CACHE tempfile;
      if (open_cached_file(&tempfile, mysql_tmpdir,TEMP_PREFIX,
			   DISK_BUFFER_SIZE, MYF(MY_WME)))
	goto err;

      if (used_index == MAX_KEY)
        init_read_record(&info,thd,table,select,0,1);
      else
        init_read_record_idx(&info, thd, table, 1, used_index);

      thd->proc_info="Searching rows for update";
      uint tmp_limit= limit;

      while (!(error=info.read_record(&info)) && !thd->killed)
      {
	if (!(select && select->skip_record()))
	{
	  table->file->position(table->record[0]);
	  if (my_b_write(&tempfile,table->file->ref,
			 table->file->ref_length))
	  {
	    error=1; /* purecov: inspected */
	    break; /* purecov: inspected */
	  }
	  if (!--limit && using_limit)
	  {
	    error= -1;
	    break;
	  }
	}
      }
      if (thd->killed && !error)
	error= 1;				// Aborted
      limit= tmp_limit;
      end_read_record(&info);
     
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
	goto err;
    }
    if (table->key_read)
    {
      table->key_read=0;
      table->file->extra(HA_EXTRA_NO_KEYREAD);
    }
  }

  if (ignore)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  init_read_record(&info,thd,table,select,0,1);

  updated= found= 0;
  thd->count_cuted_fields= CHECK_FIELD_WARN;		/* calc cuted fields */
  thd->cuted_fields=0L;
  thd->proc_info="Updating";
  query_id=thd->query_id;

  while (!(error=info.read_record(&info)) && !thd->killed)
  {
    if (!(select && select->skip_record()))
    {
      store_record(table,record[1]);
      if (fill_record(fields,values, 0) || thd->net.report_error)
	break; /* purecov: inspected */
      found++;
      if (compare_record(table, query_id))
      {
	if (!(error=table->file->update_row((byte*) table->record[1],
					    (byte*) table->record[0])))
	{
	  updated++;
	}
	else if (!ignore || error != HA_ERR_FOUND_DUPP_KEY)
	{
          thd->fatal_error();                   // Force error message
	  table->file->print_error(error,MYF(0));
	  error= 1;
	  break;
	}
      }
      if (!--limit && using_limit)
      {
	error= -1;				// Simulate end of file
	break;
      }
    }
    else
      table->file->unlock_row();
    thd->row_count++;
  }
  if (thd->killed && !error)
    error= 1;					// Aborted
  end_read_record(&info);
  free_io_cache(table);				// If ORDER BY
  delete select;
  thd->proc_info="end";
  VOID(table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY));

  /*
    Invalidate the table in the query cache if something changed.
    This must be before binlog writing and ha_autocommit_...
  */
  if (updated)
    query_cache_invalidate3(thd, table_list, 1);

  transactional_table= table->file->has_transactions();
  log_delayed= (transactional_table || table->tmp_table);
  if ((updated || (error < 0)) && (error <= 0 || !transactional_table))
  {
    mysql_update_log.write(thd,thd->query,thd->query_length);
    if (mysql_bin_log.is_open())
    {
      if (error <= 0)
        thd->clear_error();
      Query_log_event qinfo(thd, thd->query, thd->query_length,
			    log_delayed, FALSE);
      if (mysql_bin_log.write(&qinfo) && transactional_table)
	error=1;				// Rollback update
    }
    if (!log_delayed)
      thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
  }
  if (transactional_table)
  {
    if (ha_autocommit_or_rollback(thd, error >= 0))
      error=1;
  }

  if (thd->lock)
  {
    mysql_unlock_tables(thd, thd->lock);
    thd->lock=0;
  }

  free_underlaid_joins(thd, &thd->lex->select_lex);
  if (error >= 0)
    send_error(thd,thd->killed ? ER_SERVER_SHUTDOWN : 0); /* purecov: inspected */
  else
  {
    char buff[80];
    sprintf(buff, ER(ER_UPDATE_INFO), (ulong) found, (ulong) updated,
	    (ulong) thd->cuted_fields);
    send_ok(thd,
	    (thd->client_capabilities & CLIENT_FOUND_ROWS) ? found : updated,
	    thd->insert_id_used ? thd->insert_id() : 0L,buff);
    DBUG_PRINT("info",("%d records updated",updated));
  }
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;		/* calc cuted fields */
  free_io_cache(table);
  DBUG_RETURN(0);

err:
  delete select;
  free_underlaid_joins(thd, &thd->lex->select_lex);
  if (table->key_read)
  {
    table->key_read=0;
    table->file->extra(HA_EXTRA_NO_KEYREAD);
  }
  DBUG_RETURN(-1);
}

/*
  Prepare items in UPDATE statement

  SYNOPSIS
    mysql_prepare_update()
    thd			- thread handler
    table_list		- global table list
    update_table_list	- local table list of UPDATE SELECT_LEX
    conds		- conditions
    order_num		- number of ORDER BY list entries
    order		- ORDER BY clause list

  RETURN VALUE
    0  - OK
    1  - error (message is sent to user)
    -1 - error (message is not sent to user)
*/
int mysql_prepare_update(THD *thd, TABLE_LIST *table_list,
			 TABLE_LIST *update_table_list,
			 Item **conds, uint order_num, ORDER *order)
{
  TABLE *table= table_list->table;
  TABLE_LIST tables;
  List<Item> all_fields;
  DBUG_ENTER("mysql_prepare_update");

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  table->grant.want_privilege= (SELECT_ACL & ~table->grant.privilege);
#endif

  bzero((char*) &tables,sizeof(tables));	// For ORDER BY
  tables.table= table;
  tables.alias= table_list->alias;
  thd->allow_sum_func= 0;

  if (setup_tables(update_table_list) ||
      setup_conds(thd, update_table_list, conds) ||
      thd->lex->select_lex.setup_ref_array(thd, order_num) ||
      setup_order(thd, thd->lex->select_lex.ref_pointer_array,
		  update_table_list, all_fields, all_fields, order) ||
      setup_ftfuncs(&thd->lex->select_lex))
    DBUG_RETURN(-1);

  /* Check that we are not using table that we are updating in a sub select */
  if (find_real_table_in_list(table_list->next, 
			      table_list->db, table_list->real_name))
  {
    my_error(ER_UPDATE_TABLE_USED, MYF(0), table_list->real_name);
    DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}


/***************************************************************************
  Update multiple tables from join 
***************************************************************************/

/*
  Get table map for list of Item_field
*/

static table_map get_table_map(List<Item> *items)
{
  List_iterator_fast<Item> item_it(*items);
  Item_field *item;
  table_map map= 0;

  while ((item= (Item_field *) item_it++)) 
    map|= item->used_tables();
  DBUG_PRINT("info",("table_map: 0x%08x", map));
  return map;
}


/*
  Prepare tables for multi-update 
  Analyse which tables need specific privileges and perform locking
  as required
*/

int mysql_multi_update_lock(THD *thd,
			    TABLE_LIST *table_list,
			    List<Item> *fields,
			    SELECT_LEX *select_lex)
{
  int res;
  TABLE_LIST *tl;
  TABLE_LIST *update_list= (TABLE_LIST*) thd->lex->select_lex.table_list.first;
  const bool using_lock_tables= thd->locked_tables != 0;
  bool initialized_dervied= 0;
  DBUG_ENTER("mysql_multi_update_lock");

  /*
    The following loop is here to to ensure that we only lock tables
    that we are going to update with a write lock
  */
  for (;;)
  {
    table_map update_tables, derived_tables=0;
    uint tnr, table_count;

    if ((res=open_tables(thd, table_list, &table_count)))
      DBUG_RETURN(res);

    /* Only need to call lock_tables if we are not using LOCK TABLES */
    if (!using_lock_tables &&
        ((res= lock_tables(thd, table_list, table_count))))
      DBUG_RETURN(res);

    if (!initialized_dervied)
    {
      initialized_dervied= 1;
      relink_tables_for_derived(thd);
      if ((res= mysql_handle_derived(thd->lex)))
        DBUG_RETURN(res);
    }

    /*
      Ensure that we have update privilege for all tables and columns in the
      SET part
      While we are here, initialize the table->map field to check which
      tables are updated and updatability of derived tables
    */
    for (tl= update_list, tnr=0 ; tl ; tl=tl->next)
    {
      TABLE *table= tl->table;
      /*
        Update of derived tables is checked later
        We don't check privileges here, becasue then we would get error
        "UPDATE command denided .. for column N" instead of
        "Target table ... is not updatable"
      */
      if (!tl->derived)
        table->grant.want_privilege= (UPDATE_ACL & ~table->grant.privilege);
      table->map= (table_map) 1 << (tnr++);
    }

    if (setup_fields(thd, 0, update_list, *fields, 1, 0, 0))
      DBUG_RETURN(-1);

    update_tables= get_table_map(fields);

    /* Unlock the tables in preparation for relocking */
    if (!using_lock_tables)
    {
      mysql_unlock_tables(thd, thd->lock);
      thd->lock= 0;
    }

    /*
      Count tables and setup timestamp handling
      Set also the table locking strategy according to the update map
    */
    for (tl= update_list; tl; tl= tl->next)
    {
      TABLE_LIST *save= tl->next;
      TABLE *table= tl->table;
      uint wants;
      /* if table will be updated then check that it is unique */
      if (table->map & update_tables)
      {
        /*
          Multi-update can't be constructed over-union => we always have
          single SELECT on top and have to check underlaying SELECTs of it
        */
        if (select_lex->check_updateable_in_subqueries(tl->db,
                                                       tl->real_name))
        {
          my_error(ER_UPDATE_TABLE_USED, MYF(0),
                   tl->real_name);
          DBUG_RETURN(-1);
        }
	DBUG_PRINT("info",("setting table `%s` for update", tl->alias));
	tl->lock_type= thd->lex->multi_lock_option;
        tl->updating= 1;               // loacal or only list
        if (tl->table_list)
          tl->table_list->updating= 1; // global list (if we have 2 lists)
	wants= UPDATE_ACL;
      }
      else
      {
        DBUG_PRINT("info",("setting table `%s` for read-only", tl->alias));
	// If we are using the binary log, we need TL_READ_NO_INSERT to get
	// correct order of statements. Otherwise, we use a TL_READ lock to
	// improve performance.
	tl->lock_type= using_update_log ? TL_READ_NO_INSERT : TL_READ;
        tl->updating= 0;               // loacal or only list
        if (tl->table_list)
          tl->table_list->updating= 0; // global list (if we have 2 lists)
	wants= SELECT_ACL;
      }

      if (tl->derived)
        derived_tables|= table->map;
      else 
      {
        tl->next= 0;
        if (!using_lock_tables)
	  tl->table->reginfo.lock_type= tl->lock_type;
        if (check_access(thd, wants, tl->db, &tl->grant.privilege, 0, 0) ||
            (grant_option && check_grant(thd, wants, tl, 0, 0, 0)))
        {
          tl->next= save;
          DBUG_RETURN(1);
        }
        tl->next= save;
      }
    }

    if (thd->lex->derived_tables && (update_tables & derived_tables))
    {
      // find derived table which cause error
      for (tl= update_list; tl; tl= tl->next)
      {
        if (tl->derived && (update_tables & tl->table->map))
        {
          my_printf_error(ER_NON_UPDATABLE_TABLE, ER(ER_NON_UPDATABLE_TABLE),
                          MYF(0), tl->alias, "UPDATE");
          DBUG_RETURN(-1);
        }
      }
    }

    /* Relock the tables with the correct modes */
    res= lock_tables(thd, table_list, table_count);
    if (using_lock_tables)
      break;                                 // Don't have to do setup_field()

    /*
      We must setup fields again as the file may have been reopened
      during lock_tables 
    */
    {
      List_iterator_fast<Item> field_it(*fields);
      Item_field *item;

      while ((item= (Item_field *) field_it++)) 
      {
        item->field->query_id= 0;
        item->cleanup();
      }
    }
    if (setup_fields(thd, 0, update_list, *fields, 1, 0, 0))
      DBUG_RETURN(-1);
    /*
      If lock succeded and the table map didn't change since the above lock
      we can continue.
    */
    if (!res && update_tables == get_table_map(fields))
      break;

    /*
      There was some very unexpected changes in the table definition between
      open tables and lock tables. Close tables and try again.
    */
    close_thread_tables(thd);
  }
  
  DBUG_RETURN(res);
}

/*
  Setup multi-update handling and call SELECT to do the join
*/

int mysql_multi_update(THD *thd,
		       TABLE_LIST *table_list,
		       List<Item> *fields,
		       List<Item> *values,
		       COND *conds,
		       ulong options,
		       enum enum_duplicates handle_duplicates, bool ignore,
		       SELECT_LEX_UNIT *unit, SELECT_LEX *select_lex)
{
  int res;
  TABLE_LIST *tl;
  TABLE_LIST *update_list= (TABLE_LIST*) thd->lex->select_lex.table_list.first;
  List<Item> total_list;
  multi_update *result;
  DBUG_ENTER("mysql_multi_update");

  /* Setup timestamp handling */
  for (tl= update_list; tl; tl= tl->next)
  {
    TABLE *table= tl->table;
    /* Only set timestamp column if this is not modified */
    if (table->timestamp_field &&
        table->timestamp_field->query_id == thd->query_id)
      table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

    /* We only need SELECT privilege for columns in the values list */
    table->grant.want_privilege= (SELECT_ACL & ~table->grant.privilege);
  }

  if (!(result=new multi_update(thd, update_list, fields, values,
				handle_duplicates, ignore)))
    DBUG_RETURN(-1);

  res= mysql_select(thd, &select_lex->ref_pointer_array,
		    select_lex->get_table_list(), select_lex->with_wild,
		    total_list,
		    conds, 0, (ORDER *) NULL, (ORDER *)NULL, (Item *) NULL,
		    (ORDER *)NULL,
		    options | SELECT_NO_JOIN_CACHE | SELECT_NO_UNLOCK,
		    result, unit, select_lex);
  delete result;
  DBUG_RETURN(res);
}


multi_update::multi_update(THD *thd_arg, TABLE_LIST *table_list,
			   List<Item> *field_list, List<Item> *value_list,
			   enum enum_duplicates handle_duplicates_arg, bool ignore_arg)
  :all_tables(table_list), update_tables(0), thd(thd_arg), tmp_tables(0),
   updated(0), found(0), fields(field_list), values(value_list),
   table_count(0), copy_field(0), handle_duplicates(handle_duplicates_arg),
   do_update(1), trans_safe(0), transactional_tables(1), ignore(ignore_arg)
{}


/*
  Connect fields with tables and create list of tables that are updated
*/

int multi_update::prepare(List<Item> &not_used_values,
			  SELECT_LEX_UNIT *lex_unit)
{
  TABLE_LIST *table_ref;
  SQL_LIST update;
  table_map tables_to_update;
  Item_field *item;
  List_iterator_fast<Item> field_it(*fields);
  List_iterator_fast<Item> value_it(*values);
  uint i, max_fields;
  DBUG_ENTER("multi_update::prepare");

  thd->count_cuted_fields= CHECK_FIELD_WARN;
  thd->cuted_fields=0L;
  thd->proc_info="updating main table";

  tables_to_update= get_table_map(fields);

  if (!tables_to_update)
  {
    my_error(ER_NO_TABLES_USED, MYF(0));
    DBUG_RETURN(1);
  }

  /*
    We have to check values after setup_tables to get used_keys right in
    reference tables
  */

  if (setup_fields(thd, 0, all_tables, *values, 1, 0, 0))
    DBUG_RETURN(1);

  /*
    Save tables beeing updated in update_tables
    update_table->shared is position for table
    Don't use key read on tables that are updated
  */

  update.empty();
  for (table_ref= all_tables;  table_ref; table_ref=table_ref->next)
  {
    TABLE *table=table_ref->table;
    if (tables_to_update & table->map)
    {
      TABLE_LIST *tl= (TABLE_LIST*) thd->memdup((char*) table_ref,
						sizeof(*tl));
      if (!tl)
	DBUG_RETURN(1);
      update.link_in_list((byte*) tl, (byte**) &tl->next);
      tl->shared= table_count++;
      table->no_keyread=1;
      table->used_keys.clear_all();
      table->pos_in_table_list= tl;
    }
  }


  table_count=  update.elements;
  update_tables= (TABLE_LIST*) update.first;

  tmp_tables = (TABLE **) thd->calloc(sizeof(TABLE *) * table_count);
  tmp_table_param = (TMP_TABLE_PARAM*) thd->calloc(sizeof(TMP_TABLE_PARAM) *
						   table_count);
  fields_for_table= (List_item **) thd->alloc(sizeof(List_item *) *
					      table_count);
  values_for_table= (List_item **) thd->alloc(sizeof(List_item *) *
					      table_count);
  if (thd->is_fatal_error)
    DBUG_RETURN(1);
  for (i=0 ; i < table_count ; i++)
  {
    fields_for_table[i]= new List_item;
    values_for_table[i]= new List_item;
  }
  if (thd->is_fatal_error)
    DBUG_RETURN(1);

  /* Split fields into fields_for_table[] and values_by_table[] */

  while ((item= (Item_field *) field_it++))
  {
    Item *value= value_it++;
    uint offset= item->field->table->pos_in_table_list->shared;
    fields_for_table[offset]->push_back(item);
    values_for_table[offset]->push_back(value);
  }
  if (thd->is_fatal_error)
    DBUG_RETURN(1);

  /* Allocate copy fields */
  max_fields=0;
  for (i=0 ; i < table_count ; i++)
    set_if_bigger(max_fields, fields_for_table[i]->elements);
  copy_field= new Copy_field[max_fields];

  /*
    Mark all copies of tables that are updates to ensure that
    init_read_record() will not try to enable a cache on them

    The problem is that for queries like

    UPDATE t1, t1 AS t2 SET t1.b=t2.c WHERE t1.a=t2.a;

    the row buffer may contain things that doesn't match what is on disk
    which will cause an error when reading a row.
    (This issue is mostly relevent for MyISAM tables)
  */
  for (table_ref= all_tables;  table_ref; table_ref=table_ref->next)
  {
    TABLE *table=table_ref->table;
    if (!(tables_to_update & table->map) && 
	find_real_table_in_list(update_tables, table_ref->db,
				table_ref->real_name))
      table->no_cache= 1;			// Disable row cache
  }
  DBUG_RETURN(thd->is_fatal_error != 0);
}


/*
  Initialize table for multi table

  IMPLEMENTATION
    - Update first table in join on the fly, if possible
    - Create temporary tables to store changed values for all other tables
      that are updated (and main_table if the above doesn't hold).
*/

bool
multi_update::initialize_tables(JOIN *join)
{
  TABLE_LIST *table_ref;
  DBUG_ENTER("initialize_tables");

  if ((thd->options & OPTION_SAFE_UPDATES) && error_if_full_join(join))
    DBUG_RETURN(1);
  main_table=join->join_tab->table;
  trans_safe= transactional_tables= main_table->file->has_transactions();
  log_delayed= trans_safe || main_table->tmp_table != NO_TMP_TABLE;
  table_to_update= 0;

  /* Create a temporary table for keys to all tables, except main table */
  for (table_ref= update_tables; table_ref; table_ref=table_ref->next)
  {
    TABLE *table=table_ref->table;
    uint cnt= table_ref->shared;
    Item_field *ifield;
    List<Item> temp_fields= *fields_for_table[cnt];
    ORDER     group;

    if (table == main_table)			// First table in join
    {
      if (safe_update_on_fly(join->join_tab, &temp_fields))
      {
	table_to_update= main_table;		// Update table on the fly
	continue;
      }
    }

    TMP_TABLE_PARAM *tmp_param= tmp_table_param+cnt;

    /*
      Create a temporary table to store all fields that are changed for this
      table. The first field in the temporary table is a pointer to the
      original row so that we can find and update it
    */

    /* ok to be on stack as this is not referenced outside of this func */
    Field_string offset(table->file->ref_length, 0, "offset",
			table, &my_charset_bin);
    if (!(ifield= new Item_field(((Field *) &offset))))
      DBUG_RETURN(1);
    ifield->maybe_null= 0;
    if (temp_fields.push_front(ifield))
      DBUG_RETURN(1);

    /* Make an unique key over the first field to avoid duplicated updates */
    bzero((char*) &group, sizeof(group));
    group.asc= 1;
    group.item= (Item**) temp_fields.head_ref();

    tmp_param->quick_group=1;
    tmp_param->field_count=temp_fields.elements;
    tmp_param->group_parts=1;
    tmp_param->group_length= table->file->ref_length;
    if (!(tmp_tables[cnt]=create_tmp_table(thd,
					   tmp_param,
					   temp_fields,
					   (ORDER*) &group, 0, 0,
					   TMP_TABLE_ALL_COLUMNS,
					   HA_POS_ERROR,
					   (char *) "")))
      DBUG_RETURN(1);
    tmp_tables[cnt]->file->extra(HA_EXTRA_WRITE_CACHE);
  }
  DBUG_RETURN(0);
}

/*
  Check if table is safe to update on fly

  SYNOPSIS
    safe_update_on_fly
    join_tab		How table is used in join
    fields		Fields that are updated

  NOTES
    We can update the first table in join on the fly if we know that
    a row in this tabel will never be read twice. This is true under
    the folloing conditions:

    - We are doing a table scan and the data is in a separate file (MyISAM) or
      if we don't update a clustered key.

    - We are doing a range scan and we don't update the scan key or
      the primary key for a clustered table handler.

  WARNING
    This code is a bit dependent of how make_join_readinfo() works.

  RETURN
    0		Not safe to update
    1		Safe to update
*/

static bool safe_update_on_fly(JOIN_TAB *join_tab, List<Item> *fields)
{
  TABLE *table= join_tab->table;
  switch (join_tab->type) {
  case JT_SYSTEM:
  case JT_CONST:
  case JT_EQ_REF:
    return 1;					// At most one matching row
  case JT_REF:
    return !check_if_key_used(table, join_tab->ref.key, *fields);
  case JT_ALL:
    /* If range search on index */
    if (join_tab->quick)
      return !check_if_key_used(table, join_tab->quick->index,
				*fields);
    /* If scanning in clustered key */
    if ((table->file->table_flags() & HA_PRIMARY_KEY_IN_READ_INDEX) &&
	table->primary_key < MAX_KEY)
      return !check_if_key_used(table, table->primary_key, *fields);
    return 1;
  default:
    break;					// Avoid compler warning
  }
  return 0;
}


multi_update::~multi_update()
{
  TABLE_LIST *table;
  for (table= update_tables ; table; table= table->next)
    table->table->no_keyread= table->table->no_cache= 0;

  if (tmp_tables)
  {
    for (uint cnt = 0; cnt < table_count; cnt++)
    {
      if (tmp_tables[cnt])
      {
	free_tmp_table(thd, tmp_tables[cnt]);
	tmp_table_param[cnt].cleanup();
      }
    }
  }
  if (copy_field)
    delete [] copy_field;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;		// Restore this setting
  if (!trans_safe)
    thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
}


bool multi_update::send_data(List<Item> &not_used_values)
{
  TABLE_LIST *cur_table;
  DBUG_ENTER("multi_update::send_data");

  for (cur_table= update_tables; cur_table ; cur_table= cur_table->next)
  {
    TABLE *table= cur_table->table;
    /*
      Check if we are using outer join and we didn't find the row
      or if we have already updated this row in the previous call to this
      function.

      The same row may be presented here several times in a join of type
      UPDATE t1 FROM t1,t2 SET t1.a=t2.a

      In this case we will do the update for the first found row combination.
      The join algorithm guarantees that we will not find the a row in
      t1 several times.
    */
    if (table->status & (STATUS_NULL_ROW | STATUS_UPDATED))
      continue;

    uint offset= cur_table->shared;
    table->file->position(table->record[0]);
    if (table == table_to_update)
    {
      table->status|= STATUS_UPDATED;
      store_record(table,record[1]);
      if (fill_record(*fields_for_table[offset], *values_for_table[offset], 0))
	DBUG_RETURN(1);
      found++;
      if (compare_record(table, thd->query_id))
      {
	int error;
	if (!updated++)
	{
	  /*
	    Inform the main table that we are going to update the table even
	    while we may be scanning it.  This will flush the read cache
	    if it's used.
	  */
	  main_table->file->extra(HA_EXTRA_PREPARE_FOR_UPDATE);
	}
	if ((error=table->file->update_row(table->record[1],
					   table->record[0])))
	{
	  updated--;
	  if (!ignore || error != HA_ERR_FOUND_DUPP_KEY)
	  {
            thd->fatal_error();                 // Force error message
	    table->file->print_error(error,MYF(0));
	    DBUG_RETURN(1);
	  }
	}
      }
    }
    else
    {
      int error;
      TABLE *tmp_table= tmp_tables[offset];
      fill_record(tmp_table->field+1, *values_for_table[offset], 1);
      found++;
      /* Store pointer to row */
      memcpy((char*) tmp_table->field[0]->ptr,
	     (char*) table->file->ref, table->file->ref_length);
      /* Write row, ignoring duplicated updates to a row */
      if ((error= tmp_table->file->write_row(tmp_table->record[0])) &&
	  (error != HA_ERR_FOUND_DUPP_KEY &&
	   error != HA_ERR_FOUND_DUPP_UNIQUE))
      {
	if (create_myisam_from_heap(thd, tmp_table, tmp_table_param + offset,
				    error, 1))
	{
	  do_update=0;
	  DBUG_RETURN(1);			// Not a table_is_full error
	}
      }
    }
  }
  DBUG_RETURN(0);
}


void multi_update::send_error(uint errcode,const char *err)
{
  /* First send error what ever it is ... */
  ::send_error(thd,errcode,err);

  /* If nothing updated return */
  if (!updated)
    return;

  /* Something already updated so we have to invalidate cache */
  query_cache_invalidate3(thd, update_tables, 1);

  /*
    If all tables that has been updated are trans safe then just do rollback.
    If not attempt to do remaining updates.
  */

  if (trans_safe)
    ha_rollback_stmt(thd);
  else if (do_update && table_count > 1)
  {
    /* Add warning here */
    VOID(do_updates(0));
  }
}


int multi_update::do_updates(bool from_send_error)
{
  TABLE_LIST *cur_table;
  int local_error;
  ha_rows org_updated;
  TABLE *table, *tmp_table;
  DBUG_ENTER("do_updates");

  do_update= 0;					// Don't retry this function
  if (!found)
    DBUG_RETURN(0);
  for (cur_table= update_tables; cur_table ; cur_table= cur_table->next)
  {
    byte *ref_pos;

    table = cur_table->table;
    if (table == table_to_update)
      continue;					// Already updated
    org_updated= updated;
    tmp_table= tmp_tables[cur_table->shared];
    tmp_table->file->extra(HA_EXTRA_CACHE);	// Change to read cache
    (void) table->file->ha_rnd_init(0);
    table->file->extra(HA_EXTRA_NO_CACHE);

    /*
      Setup copy functions to copy fields from temporary table
    */
    List_iterator_fast<Item> field_it(*fields_for_table[cur_table->shared]);
    Field **field= tmp_table->field+1;		// Skip row pointer
    Copy_field *copy_field_ptr= copy_field, *copy_field_end;
    for ( ; *field ; field++)
    {
      Item_field *item= (Item_field* ) field_it++;
      (copy_field_ptr++)->set(item->field, *field, 0);
    }
    copy_field_end=copy_field_ptr;

    if ((local_error = tmp_table->file->ha_rnd_init(1)))
      goto err;

    ref_pos= (byte*) tmp_table->field[0]->ptr;
    for (;;)
    {
      if (thd->killed && trans_safe)
	goto err;
      if ((local_error=tmp_table->file->rnd_next(tmp_table->record[0])))
      {
	if (local_error == HA_ERR_END_OF_FILE)
	  break;
	if (local_error == HA_ERR_RECORD_DELETED)
	  continue;				// May happen on dup key
	goto err;
      }
      if ((local_error= table->file->rnd_pos(table->record[0], ref_pos)))
	goto err;
      table->status|= STATUS_UPDATED;
      store_record(table,record[1]);

      /* Copy data from temporary table to current table */
      for (copy_field_ptr=copy_field;
	   copy_field_ptr != copy_field_end;
	   copy_field_ptr++)
	(*copy_field_ptr->do_copy)(copy_field_ptr);

      if (compare_record(table, thd->query_id))
      {
	if ((local_error=table->file->update_row(table->record[1],
						 table->record[0])))
	{
	  if (!ignore || local_error != HA_ERR_FOUND_DUPP_KEY)
	    goto err;
	}
	updated++;
	if (table->tmp_table != NO_TMP_TABLE)
	  log_delayed= 1;
      }
    }

    if (updated != org_updated)
    {
      if (table->tmp_table != NO_TMP_TABLE)
	log_delayed= 1;				// Tmp tables forces delay log
      if (table->file->has_transactions())
	log_delayed= transactional_tables= 1;
      else
	trans_safe= 0;				// Can't do safe rollback
    }
    (void) table->file->ha_rnd_end();
    (void) tmp_table->file->ha_rnd_end();
  }
  DBUG_RETURN(0);

err:
  if (!from_send_error)
  {
    thd->fatal_error();
    table->file->print_error(local_error,MYF(0));
  }

  (void) table->file->ha_rnd_end();
  (void) tmp_table->file->ha_rnd_end();

  if (updated != org_updated)
  {
    if (table->tmp_table != NO_TMP_TABLE)
      log_delayed= 1;
    if (table->file->has_transactions())
      log_delayed= transactional_tables= 1;
    else
      trans_safe= 0;
  }
  DBUG_RETURN(1);
}


/* out: 1 if error, 0 if success */

bool multi_update::send_eof()
{
  char buff[80];
  thd->proc_info="updating reference tables";

  /* Does updates for the last n - 1 tables, returns 0 if ok */
  int local_error = (table_count) ? do_updates(0) : 0;
  thd->proc_info= "end";

  /* We must invalidate the query cache before binlog writing and
  ha_autocommit_... */

  if (updated)
  {
    query_cache_invalidate3(thd, update_tables, 1);
  }

  /*
    Write the SQL statement to the binlog if we updated
    rows and we succeeded or if we updated some non
    transacational tables.
    Note that if we updated nothing we don't write to the binlog (TODO:
    fix this).
  */

  if (updated && (local_error <= 0 || !trans_safe))
  {
    mysql_update_log.write(thd,thd->query,thd->query_length);
    if (mysql_bin_log.is_open())
    {
      if (local_error <= 0)
        thd->clear_error();
      Query_log_event qinfo(thd, thd->query, thd->query_length,
			    log_delayed, FALSE);
      if (mysql_bin_log.write(&qinfo) && trans_safe)
	local_error= 1;				// Rollback update
    }
    if (!log_delayed)
      thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
  }

  if (transactional_tables)
  {
    if (ha_autocommit_or_rollback(thd, local_error != 0))
      local_error=1;
  }

  if (local_error > 0) // if the above log write did not fail ...
  {
    /* Safety: If we haven't got an error before (should not happen) */
    my_message(ER_UNKNOWN_ERROR, "An error occured in multi-table update",
	       MYF(0));
    ::send_error(thd);
    return 1;
  }


  sprintf(buff, ER(ER_UPDATE_INFO), (ulong) found, (ulong) updated,
	  (ulong) thd->cuted_fields);
  ::send_ok(thd,
	    (thd->client_capabilities & CLIENT_FOUND_ROWS) ? found : updated,
	    thd->insert_id_used ? thd->insert_id() : 0L,buff);
  return 0;
}
