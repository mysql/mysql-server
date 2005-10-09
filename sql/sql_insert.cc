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


/* Insert of records */

#include "mysql_priv.h"
#include "sp_head.h"
#include "sql_trigger.h"
#include "sql_select.h"

static int check_null_fields(THD *thd,TABLE *entry);
#ifndef EMBEDDED_LIBRARY
static TABLE *delayed_get_table(THD *thd,TABLE_LIST *table_list);
static int write_delayed(THD *thd,TABLE *table, enum_duplicates dup, bool ignore,
			 char *query, uint query_length, bool log_on);
static void end_delayed_insert(THD *thd);
pthread_handler_t handle_delayed_insert(void *arg);
static void unlink_blobs(register TABLE *table);
#endif
static bool check_view_insertability(THD *thd, TABLE_LIST *view);

/* Define to force use of my_malloc() if the allocated memory block is big */

#ifndef HAVE_ALLOCA
#define my_safe_alloca(size, min_length) my_alloca(size)
#define my_safe_afree(ptr, size, min_length) my_afree(ptr)
#else
#define my_safe_alloca(size, min_length) ((size <= min_length) ? my_alloca(size) : my_malloc(size,MYF(0)))
#define my_safe_afree(ptr, size, min_length) if (size > min_length) my_free(ptr,MYF(0))
#endif


/*
  Check if insert fields are correct.

  SYNOPSIS
    check_insert_fields()
    thd                         The current thread.
    table                       The table for insert.
    fields                      The insert fields.
    values                      The insert values.
    check_unique                If duplicate values should be rejected.

  NOTE
    Clears TIMESTAMP_AUTO_SET_ON_INSERT from table->timestamp_field_type
    or leaves it as is, depending on if timestamp should be updated or
    not.

  RETURN
    0           OK
    -1          Error
*/

static int check_insert_fields(THD *thd, TABLE_LIST *table_list,
                               List<Item> &fields, List<Item> &values,
                               bool check_unique)
{
  TABLE *table= table_list->table;

  if (!table_list->updatable)
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "INSERT");
    return -1;
  }

  if (fields.elements == 0 && values.elements != 0)
  {
    if (!table)
    {
      my_error(ER_VIEW_NO_INSERT_FIELD_LIST, MYF(0),
               table_list->view_db.str, table_list->view_name.str);
      return -1;
    }
    if (values.elements != table->s->fields)
    {
      my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), 1L);
      return -1;
    }
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    if (grant_option)
    {
      Field_iterator_table fields;
      fields.set_table(table);
      if (check_grant_all_columns(thd, INSERT_ACL, &table->grant,
                                  table->s->db, table->s->table_name,
                                  &fields))
        return -1;
    }
#endif
    clear_timestamp_auto_bits(table->timestamp_field_type,
                              TIMESTAMP_AUTO_SET_ON_INSERT);
    /*
      No fields are provided so all fields must be provided in the values.
      Thus we set all bits in the write set.
    */
    table->file->ha_set_all_bits_in_write_set();
  }
  else
  {						// Part field list
    SELECT_LEX *select_lex= &thd->lex->select_lex;
    Name_resolution_context *context= &select_lex->context;
    TABLE_LIST *save_next_local;
    TABLE_LIST *save_table_list;
    TABLE_LIST *save_first_name_resolution_table;
    TABLE_LIST *save_next_name_resolution_table;
    bool        save_resolve_in_select_list;
    int res;

    if (fields.elements != values.elements)
    {
      my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), 1L);
      return -1;
    }

    thd->dupp_field=0;
    select_lex->no_wrap_view_item= TRUE;

    /* Save the state of the current name resolution context. */
    save_table_list=                  context->table_list;
    save_first_name_resolution_table= context->first_name_resolution_table;
    save_next_name_resolution_table=  (context->first_name_resolution_table) ?
                                      context->first_name_resolution_table->
                                               next_name_resolution_table :
                                      NULL;
    save_resolve_in_select_list=      context->resolve_in_select_list;
    save_next_local=                  table_list->next_local;

    /*
      Perform name resolution only in the first table - 'table_list',
      which is the table that is inserted into.
    */
    table_list->next_local= 0;
    context->resolve_in_table_list_only(table_list);
    /*
      Indicate fields in list is to be updated by setting set_query_id
      parameter to 2. This sets the bit in the write_set for each field.
    */
    res= setup_fields(thd, 0, fields, 2, 0, 0);

    /* Restore the current context. */
    table_list->next_local=                save_next_local;
    context->table_list=                   save_table_list;
    context->first_name_resolution_table=  save_first_name_resolution_table;
    if (context->first_name_resolution_table)
      context->first_name_resolution_table->
               next_name_resolution_table= save_next_name_resolution_table;
    context->resolve_in_select_list=       save_resolve_in_select_list;
    thd->lex->select_lex.no_wrap_view_item= FALSE;

    if (res)
      return -1;

    if (table_list->effective_algorithm == VIEW_ALGORITHM_MERGE)
    {
      /* it is join view => we need to find table for update */
      List_iterator_fast<Item> it(fields);
      Item *item;
      TABLE_LIST *tbl= 0;            // reset for call to check_single_table()
      table_map map= 0;

      while ((item= it++))
        map|= item->used_tables();
      if (table_list->check_single_table(&tbl, map, table_list) || tbl == 0)
      {
        my_error(ER_VIEW_MULTIUPDATE, MYF(0),
                 table_list->view_db.str, table_list->view_name.str);
        return -1;
      }
      table_list->table= table= tbl->table;
    }

    if (check_unique && thd->dupp_field)
    {
      my_error(ER_FIELD_SPECIFIED_TWICE, MYF(0), thd->dupp_field->field_name);
      return -1;
    }
    if (table->timestamp_field &&	// Don't set timestamp if used
	table->timestamp_field->query_id == thd->query_id)
      clear_timestamp_auto_bits(table->timestamp_field_type,
                                TIMESTAMP_AUTO_SET_ON_INSERT);
  }
  // For the values we need select_priv
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  table->grant.want_privilege= (SELECT_ACL & ~table->grant.privilege);
#endif

  if (check_key_in_view(thd, table_list) ||
      (table_list->view &&
       check_view_insertability(thd, table_list)))
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "INSERT");
    return -1;
  }

  return 0;
}


/*
  Check update fields for the timestamp field.

  SYNOPSIS
    check_update_fields()
    thd                         The current thread.
    insert_table_list           The insert table list.
    table                       The table for update.
    update_fields               The update fields.

  NOTE
    If the update fields include the timestamp field,
    remove TIMESTAMP_AUTO_SET_ON_UPDATE from table->timestamp_field_type.

  RETURN
    0           OK
    -1          Error
*/

static int check_update_fields(THD *thd, TABLE_LIST *insert_table_list,
                               List<Item> &update_fields)
{
  TABLE *table= insert_table_list->table;
  query_id_t timestamp_query_id;
  LINT_INIT(timestamp_query_id);

  /*
    Change the query_id for the timestamp column so that we can
    check if this is modified directly.
  */
  if (table->timestamp_field)
  {
    timestamp_query_id= table->timestamp_field->query_id;
    table->timestamp_field->query_id= thd->query_id - 1;
  }

  /*
    Check the fields we are going to modify. This will set the query_id
    of all used fields to the threads query_id. It will also set all
    fields into the write set of this table.
  */
  if (setup_fields(thd, 0, update_fields, 2, 0, 0))
    return -1;

  if (table->timestamp_field)
  {
    /* Don't set timestamp column if this is modified. */
    if (table->timestamp_field->query_id == thd->query_id)
      clear_timestamp_auto_bits(table->timestamp_field_type,
                                TIMESTAMP_AUTO_SET_ON_UPDATE);
    else
    {
      table->timestamp_field->query_id= timestamp_query_id;
      table->file->ha_set_bit_in_write_set(table->timestamp_field->fieldnr);
    }
  }

  return 0;
}


bool mysql_insert(THD *thd,TABLE_LIST *table_list,
                  List<Item> &fields,
                  List<List_item> &values_list,
                  List<Item> &update_fields,
                  List<Item> &update_values,
                  enum_duplicates duplic,
		  bool ignore)
{
  int error, res;
  /*
    log_on is about delayed inserts only.
    By default, both logs are enabled (this won't cause problems if the server
    runs without --log-update or --log-bin).
  */
  bool log_on= (thd->options & OPTION_BIN_LOG) ||
    (!(thd->security_ctx->master_access & SUPER_ACL));
  bool transactional_table;
  uint value_count;
  ulong counter = 1;
  ulonglong id;
  COPY_INFO info;
  TABLE *table= 0;
  TABLE_LIST *save_table_list;
  TABLE_LIST *save_next_local;
  TABLE_LIST *save_first_name_resolution_table;
  TABLE_LIST *save_next_name_resolution_table;
  List_iterator_fast<List_item> its(values_list);
  List_item *values;
  Name_resolution_context *context;
#ifndef EMBEDDED_LIBRARY
  char *query= thd->query;
#endif
  thr_lock_type lock_type = table_list->lock_type;
  Item *unused_conds= 0;
  DBUG_ENTER("mysql_insert");

  /*
    in safe mode or with skip-new change delayed insert to be regular
    if we are told to replace duplicates, the insert cannot be concurrent
    delayed insert changed to regular in slave thread
   */
#ifdef EMBEDDED_LIBRARY
  if (lock_type == TL_WRITE_DELAYED)
    lock_type=TL_WRITE;
#else
  if ((lock_type == TL_WRITE_DELAYED &&
       ((specialflag & (SPECIAL_NO_NEW_FUNC | SPECIAL_SAFE_MODE)) ||
	thd->slave_thread || !thd->variables.max_insert_delayed_threads)) ||
      (lock_type == TL_WRITE_CONCURRENT_INSERT && duplic == DUP_REPLACE) ||
      (duplic == DUP_UPDATE))
    lock_type=TL_WRITE;
#endif
  table_list->lock_type= lock_type;

#ifndef EMBEDDED_LIBRARY
  if (lock_type == TL_WRITE_DELAYED)
  {
    if (thd->locked_tables)
    {
      if (find_locked_table(thd,
			    table_list->db ? table_list->db : thd->db,
			    table_list->table_name))
      {
	my_error(ER_DELAYED_INSERT_TABLE_LOCKED, MYF(0),
                 table_list->table_name);
	DBUG_RETURN(TRUE);
      }
    }
    if ((table= delayed_get_table(thd,table_list)) && !thd->is_fatal_error)
    {
      /*
        Open tables used for sub-selects or in stored functions, will also
        cache these functions.
      */
      res= open_and_lock_tables(thd, table_list->next_global);
      /*
	First is not processed by open_and_lock_tables() => we need set
	updateability flags "by hands".
      */
      if (!table_list->derived && !table_list->view)
        table_list->updatable= 1;  // usual table
    }
    else
    {
      /* Too many delayed insert threads;  Use a normal insert */
      table_list->lock_type= lock_type= TL_WRITE;
      res= open_and_lock_tables(thd, table_list);
    }
  }
  else
#endif /* EMBEDDED_LIBRARY */
    res= open_and_lock_tables(thd, table_list);
  if (res || thd->is_fatal_error)
    DBUG_RETURN(TRUE);

  thd->proc_info="init";
  thd->used_tables=0;
  values= its++;

  if (mysql_prepare_insert(thd, table_list, table, fields, values,
			   update_fields, update_values, duplic, &unused_conds,
                           FALSE))
    goto abort;

  /* mysql_prepare_insert set table_list->table if it was not set */
  table= table_list->table;

  context= &thd->lex->select_lex.context;
  /* Save the state of the current name resolution context. */
  save_table_list=                  context->table_list;
  save_first_name_resolution_table= context->first_name_resolution_table;
  save_next_name_resolution_table=  (context->first_name_resolution_table) ?
                                    context->first_name_resolution_table->
                                             next_name_resolution_table :
                                    NULL;
  save_next_local=                  table_list->next_local;

  /*
    Perform name resolution only in the first table - 'table_list',
    which is the table that is inserted into.
  */
  table_list->next_local= 0;
  context->resolve_in_table_list_only(table_list);

  value_count= values->elements;
  while ((values= its++))
  {
    counter++;
    if (values->elements != value_count)
    {
      my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), counter);
      goto abort;
    }
    if (setup_fields(thd, 0, *values, 0, 0, 0))
      goto abort;
  }
  its.rewind ();
 
  /* Restore the current context. */
  table_list->next_local= save_next_local;
  context->first_name_resolution_table= save_first_name_resolution_table;
  if (context->first_name_resolution_table)
    context->first_name_resolution_table->
             next_name_resolution_table= save_next_name_resolution_table;

  /*
    Fill in the given fields and dump it to the table file
  */

  info.records= info.deleted= info.copied= info.updated= 0;
  info.ignore= ignore;
  info.handle_duplicates=duplic;
  info.update_fields= &update_fields;
  info.update_values= &update_values;
  info.view= (table_list->view ? table_list : 0);

  /*
    Count warnings for all inserts.
    For single line insert, generate an error if try to set a NOT NULL field
    to NULL.
  */
  thd->count_cuted_fields= ((values_list.elements == 1 &&
                             !ignore) ?
			    CHECK_FIELD_ERROR_FOR_NULL :
			    CHECK_FIELD_WARN);
  thd->cuted_fields = 0L;
  table->next_number_field=table->found_next_number_field;

  error=0;
  id=0;
  thd->proc_info="update";
  if (duplic != DUP_ERROR || ignore)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  /*
    let's *try* to start bulk inserts. It won't necessary
    start them as values_list.elements should be greater than
    some - handler dependent - threshold.
    So we call start_bulk_insert to perform nesessary checks on
    values_list.elements, and - if nothing else - to initialize
    the code to make the call of end_bulk_insert() below safe.
  */
  if (lock_type != TL_WRITE_DELAYED)
    table->file->start_bulk_insert(values_list.elements);

  thd->no_trans_update= 0;
  thd->abort_on_warning= (!ignore &&
                          (thd->variables.sql_mode &
                           (MODE_STRICT_TRANS_TABLES |
                            MODE_STRICT_ALL_TABLES)));

  if ((fields.elements || !value_count) &&
      check_that_all_fields_are_given_values(thd, table, table_list))
  {
    /* thd->net.report_error is now set, which will abort the next loop */
    error= 1;
  }

  if (table_list->prepare_where(thd, 0, TRUE) ||
      table_list->prepare_check_option(thd))
    error= 1;

  while ((values= its++))
  {
    if (fields.elements || !value_count)
    {
      restore_record(table,s->default_values);	// Get empty record
      if (fill_record_n_invoke_before_triggers(thd, fields, *values, 0,
                                               table->triggers,
                                               TRG_EVENT_INSERT))
      {
	if (values_list.elements != 1 && !thd->net.report_error)
	{
	  info.records++;
	  continue;
	}
	/*
	  TODO: set thd->abort_on_warning if values_list.elements == 1
	  and check that all items return warning in case of problem with
	  storing field.
        */
	error=1;
	break;
      }
    }
    else
    {
      if (thd->used_tables)			// Column used in values()
	restore_record(table,s->default_values);	// Get empty record
      else
      {
        /*
          Fix delete marker. No need to restore rest of record since it will
          be overwritten by fill_record() anyway (and fill_record() does not
          use default values in this case).
        */
	table->record[0][0]= table->s->default_values[0];
      }
      if (fill_record_n_invoke_before_triggers(thd, table->field, *values, 0,
                                               table->triggers,
                                               TRG_EVENT_INSERT))
      {
	if (values_list.elements != 1 && ! thd->net.report_error)
	{
	  info.records++;
	  continue;
	}
	error=1;
	break;
      }
    }

    if ((res= table_list->view_check_option(thd,
					    (values_list.elements == 1 ?
					     0 :
					     ignore))) ==
        VIEW_CHECK_SKIP)
      continue;
    else if (res == VIEW_CHECK_ERROR)
    {
      error= 1;
      break;
    }
#ifndef EMBEDDED_LIBRARY
    if (lock_type == TL_WRITE_DELAYED)
    {
      error=write_delayed(thd, table, duplic, ignore, query, thd->query_length, log_on);
      query=0;
    }
    else
#endif
      error=write_record(thd, table ,&info);
    /*
      If auto_increment values are used, save the first one
       for LAST_INSERT_ID() and for the update log.
       We can't use insert_id() as we don't want to touch the
       last_insert_id_used flag.
    */
    if (! id && thd->insert_id_used)
    {						// Get auto increment value
      id= thd->last_insert_id;
    }
    if (error)
      break;
    thd->row_count++;
  }

  /*
    Now all rows are inserted.  Time to update logs and sends response to
    user
  */
#ifndef EMBEDDED_LIBRARY
  if (lock_type == TL_WRITE_DELAYED)
  {
    if (!error)
    {
      id=0;					// No auto_increment id
      info.copied=values_list.elements;
      end_delayed_insert(thd);
    }
    query_cache_invalidate3(thd, table_list, 1);
  }
  else
#endif
  {
    if (table->file->end_bulk_insert() && !error)
    {
      table->file->print_error(my_errno,MYF(0));
      error=1;
    }
    if (id && values_list.elements != 1)
      thd->insert_id(id);			// For update log
    else if (table->next_number_field && info.copied)
      id=table->next_number_field->val_int();	// Return auto_increment value

    /*
      Invalidate the table in the query cache if something changed.
      For the transactional algorithm to work the invalidation must be
      before binlog writing and ha_autocommit_or_rollback
    */
    if (info.copied || info.deleted || info.updated)
    {
      query_cache_invalidate3(thd, table_list, 1);
    }

    transactional_table= table->file->has_transactions();

    if ((info.copied || info.deleted || info.updated) &&
	(error <= 0 || !transactional_table))
    {
      if (mysql_bin_log.is_open())
      {
        if (error <= 0)
          thd->clear_error();
	Query_log_event qinfo(thd, thd->query, thd->query_length,
			      transactional_table, FALSE);
	if (mysql_bin_log.write(&qinfo) && transactional_table)
	  error=1;
      }
      if (!transactional_table)
	thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
    }
    if (transactional_table)
      error=ha_autocommit_or_rollback(thd,error);

    if (thd->lock)
    {
      mysql_unlock_tables(thd, thd->lock);
      thd->lock=0;
    }
  }
  thd->proc_info="end";
  table->next_number_field=0;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  thd->next_insert_id=0;			// Reset this if wrongly used
  if (duplic != DUP_ERROR || ignore)
    table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

  /* Reset value of LAST_INSERT_ID if no rows where inserted */
  if (!info.copied && thd->insert_id_used)
  {
    thd->insert_id(0);
    id=0;
  }
  if (error)
    goto abort;
  if (values_list.elements == 1 && (!(thd->options & OPTION_WARNINGS) ||
				    !thd->cuted_fields))
  {
    thd->row_count_func= info.copied+info.deleted+info.updated;
    send_ok(thd, (ulong) thd->row_count_func, id);
  }
  else
  {
    char buff[160];
    if (ignore)
      sprintf(buff, ER(ER_INSERT_INFO), (ulong) info.records,
	      (lock_type == TL_WRITE_DELAYED) ? (ulong) 0 :
	      (ulong) (info.records - info.copied), (ulong) thd->cuted_fields);
    else
      sprintf(buff, ER(ER_INSERT_INFO), (ulong) info.records,
	      (ulong) (info.deleted+info.updated), (ulong) thd->cuted_fields);
    thd->row_count_func= info.copied+info.deleted+info.updated;
    ::send_ok(thd, (ulong) thd->row_count_func, id, buff);
  }
  free_underlaid_joins(thd, &thd->lex->select_lex);
  thd->abort_on_warning= 0;
  DBUG_RETURN(FALSE);

abort:
#ifndef EMBEDDED_LIBRARY
  if (lock_type == TL_WRITE_DELAYED)
    end_delayed_insert(thd);
#endif
  free_underlaid_joins(thd, &thd->lex->select_lex);
  thd->abort_on_warning= 0;
  DBUG_RETURN(TRUE);
}


/*
  Additional check for insertability for VIEW

  SYNOPSIS
    check_view_insertability()
    thd     - thread handler
    view    - reference on VIEW

  IMPLEMENTATION
    A view is insertable if the folloings are true:
    - All columns in the view are columns from a table
    - All not used columns in table have a default values
    - All field in view are unique (not referring to the same column)

  RETURN
    FALSE - OK
      view->contain_auto_increment is 1 if and only if the view contains an
      auto_increment field

    TRUE  - can't be used for insert
*/

static bool check_view_insertability(THD * thd, TABLE_LIST *view)
{
  uint num= view->view->select_lex.item_list.elements;
  TABLE *table= view->table;
  Field_translator *trans_start= view->field_translation,
		   *trans_end= trans_start + num;
  Field_translator *trans;
  Field **field_ptr= table->field;
  uint used_fields_buff_size= (table->s->fields + 7) / 8;
  uint32 *used_fields_buff= (uint32*)thd->alloc(used_fields_buff_size);
  MY_BITMAP used_fields;
  DBUG_ENTER("check_key_in_view");

  if (!used_fields_buff)
    DBUG_RETURN(TRUE);  // EOM

  DBUG_ASSERT(view->table != 0 && view->field_translation != 0);

  bitmap_init(&used_fields, used_fields_buff, used_fields_buff_size * 8, 0);
  bitmap_clear_all(&used_fields);

  view->contain_auto_increment= 0;
  /* check simplicity and prepare unique test of view */
  for (trans= trans_start; trans != trans_end; trans++)
  {
    if (!trans->item->fixed && trans->item->fix_fields(thd, &trans->item))
      return TRUE;
    Item_field *field;
    /* simple SELECT list entry (field without expression) */
    if (!(field= trans->item->filed_for_view_update()))
      DBUG_RETURN(TRUE);
    if (field->field->unireg_check == Field::NEXT_NUMBER)
      view->contain_auto_increment= 1;
    /* prepare unique test */
    /*
      remove collation (or other transparent for update function) if we have
      it
    */
    trans->item= field;
  }
  /* unique test */
  for (trans= trans_start; trans != trans_end; trans++)
  {
    /* Thanks to test above, we know that all columns are of type Item_field */
    Item_field *field= (Item_field *)trans->item;
    /* check fields belong to table in which we are inserting */
    if (field->field->table == table &&
        bitmap_fast_test_and_set(&used_fields, field->field->field_index))
      DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}


/*
  Check if table can be updated

  SYNOPSIS
     mysql_prepare_insert_check_table()
     thd		Thread handle
     table_list		Table list
     fields		List of fields to be updated
     where		Pointer to where clause
     select_insert      Check is making for SELECT ... INSERT

   RETURN
     FALSE ok
     TRUE  ERROR
*/

static bool mysql_prepare_insert_check_table(THD *thd, TABLE_LIST *table_list,
                                             List<Item> &fields, COND **where,
                                             bool select_insert)
{
  bool insert_into_view= (table_list->view != 0);
  DBUG_ENTER("mysql_prepare_insert_check_table");

  if (setup_tables(thd, &thd->lex->select_lex.context,
                   &thd->lex->select_lex.top_join_list,
                   table_list, where, &thd->lex->select_lex.leaf_tables,
		   select_insert))
    DBUG_RETURN(TRUE);

  if (insert_into_view && !fields.elements)
  {
    thd->lex->empty_field_list_on_rset= 1;
    if (!table_list->table)
    {
      my_error(ER_VIEW_NO_INSERT_FIELD_LIST, MYF(0),
               table_list->view_db.str, table_list->view_name.str);
      DBUG_RETURN(TRUE);
    }
    DBUG_RETURN(insert_view_fields(thd, &fields, table_list));
  }

  DBUG_RETURN(FALSE);
}


/*
  Prepare items in INSERT statement

  SYNOPSIS
    mysql_prepare_insert()
    thd			Thread handler
    table_list	        Global/local table list
    table		Table to insert into (can be NULL if table should
			be taken from table_list->table)    
    where		Where clause (for insert ... select)
    select_insert	TRUE if INSERT ... SELECT statement

  TODO (in far future)
    In cases of:
    INSERT INTO t1 SELECT a, sum(a) as sum1 from t2 GROUP BY a
    ON DUPLICATE KEY ...
    we should be able to refer to sum1 in the ON DUPLICATE KEY part

  WARNING
    You MUST set table->insert_values to 0 after calling this function
    before releasing the table object.
  
  RETURN VALUE
    FALSE OK
    TRUE  error
*/

bool mysql_prepare_insert(THD *thd, TABLE_LIST *table_list,
                          TABLE *table, List<Item> &fields, List_item *values,
                          List<Item> &update_fields, List<Item> &update_values,
                          enum_duplicates duplic,
                          COND **where, bool select_insert)
{
  SELECT_LEX *select_lex= &thd->lex->select_lex;
  Name_resolution_context *context= &select_lex->context;
  TABLE_LIST *save_table_list;
  TABLE_LIST *save_next_local;
  TABLE_LIST *save_first_name_resolution_table;
  TABLE_LIST *save_next_name_resolution_table;
  bool        save_resolve_in_select_list;
  bool insert_into_view= (table_list->view != 0);
  bool res= 0;
  DBUG_ENTER("mysql_prepare_insert");
  DBUG_PRINT("enter", ("table_list 0x%lx, table 0x%lx, view %d",
		       (ulong)table_list, (ulong)table,
		       (int)insert_into_view));

  /*
    For subqueries in VALUES() we should not see the table in which we are
    inserting (for INSERT ... SELECT this is done by changing table_list,
    because INSERT ... SELECT share SELECT_LEX it with SELECT.
  */
  if (!select_insert)
  {
    for (SELECT_LEX_UNIT *un= select_lex->first_inner_unit();
         un;
         un= un->next_unit())
    {
      for (SELECT_LEX *sl= un->first_select();
           sl;
           sl= sl->next_select())
      {
        sl->context.outer_context= 0;
      }
    }
  }

  if (duplic == DUP_UPDATE)
  {
    /* it should be allocated before Item::fix_fields() */
    if (table_list->set_insert_values(thd->mem_root))
      DBUG_RETURN(TRUE);
  }

  if (mysql_prepare_insert_check_table(thd, table_list, fields, where,
                                       select_insert))
    DBUG_RETURN(TRUE);

  /* Save the state of the current name resolution context. */
  save_table_list=                  context->table_list;
  /* Here first_name_resolution_table points to the first select table. */
  save_first_name_resolution_table= context->first_name_resolution_table;
  save_next_name_resolution_table=  (context->first_name_resolution_table) ?
                                    context->first_name_resolution_table->
                                             next_name_resolution_table :
                                    NULL;
  save_resolve_in_select_list=      context->resolve_in_select_list;
  save_next_local=                  table_list->next_local;

  /*
    Perform name resolution only in the first table - 'table_list',
    which is the table that is inserted into.
  */
  table_list->next_local= 0;
  context->resolve_in_table_list_only(table_list);

  /* Prepare the fields in the statement. */
  if (values &&
      !(res= check_insert_fields(thd, context->table_list, fields, *values,
                                 !insert_into_view) ||
        setup_fields(thd, 0, *values, 0, 0, 0)) &&
      duplic == DUP_UPDATE)
  {
    select_lex->no_wrap_view_item= TRUE;
    res= check_update_fields(thd, context->table_list, update_fields);
    select_lex->no_wrap_view_item= FALSE;
    /*
      When we are not using GROUP BY we can refer to other tables in the
      ON DUPLICATE KEY part.
    */       
    if (select_lex->group_list.elements == 0)
    {
      context->table_list->next_local=       save_next_local;
      /* first_name_resolution_table was set by resolve_in_table_list_only() */
      context->first_name_resolution_table->
        next_name_resolution_table=          save_next_local;
    }
    if (!res)
      res= setup_fields(thd, 0, update_values, 1, 0, 0);
  }

  /* Restore the current context. */
  table_list->next_local= save_next_local;
  context->table_list= save_table_list;
  context->first_name_resolution_table= save_first_name_resolution_table;
  if (context->first_name_resolution_table)
    context->first_name_resolution_table->
             next_name_resolution_table= save_next_name_resolution_table;
  context->resolve_in_select_list= save_resolve_in_select_list;

  if (res)
    DBUG_RETURN(res);

  if (!table)
    table= table_list->table;

  if (!select_insert)
  {
    Item *fake_conds= 0;
    TABLE_LIST *duplicate;
    if ((duplicate= unique_table(table_list, table_list->next_global)))
    {
      update_non_unique_table_error(table_list, "INSERT", duplicate);
      DBUG_RETURN(TRUE);
    }
    select_lex->fix_prepare_information(thd, &fake_conds);
    select_lex->first_execution= 0;
  }
  if (duplic == DUP_UPDATE || duplic == DUP_REPLACE)
    table->file->ha_retrieve_all_pk();
  DBUG_RETURN(FALSE);
}


	/* Check if there is more uniq keys after field */

static int last_uniq_key(TABLE *table,uint keynr)
{
  while (++keynr < table->s->keys)
    if (table->key_info[keynr].flags & HA_NOSAME)
      return 0;
  return 1;
}


/*
  Write a record to table with optional deleting of conflicting records,
  invoke proper triggers if needed.

  SYNOPSIS
     write_record()
      thd   - thread context
      table - table to which record should be written
      info  - COPY_INFO structure describing handling of duplicates
              and which is used for counting number of records inserted
              and deleted.

  NOTE
    Once this record will be written to table after insert trigger will
    be invoked. If instead of inserting new record we will update old one
    then both on update triggers will work instead. Similarly both on
    delete triggers will be invoked if we will delete conflicting records.

    Sets thd->no_trans_update if table which is updated didn't have
    transactions.

  RETURN VALUE
    0     - success
    non-0 - error
*/


int write_record(THD *thd, TABLE *table,COPY_INFO *info)
{
  int error, trg_error= 0;
  char *key=0;
  DBUG_ENTER("write_record");

  info->records++;
  if (info->handle_duplicates == DUP_REPLACE ||
      info->handle_duplicates == DUP_UPDATE)
  {
    while ((error=table->file->write_row(table->record[0])))
    {
      uint key_nr;
      if (error != HA_WRITE_SKIP)
	goto err;
      table->file->restore_auto_increment();
      if ((int) (key_nr = table->file->get_dup_key(error)) < 0)
      {
	error=HA_WRITE_SKIP;			/* Database can't find key */
	goto err;
      }
      /*
	Don't allow REPLACE to replace a row when a auto_increment column
	was used.  This ensures that we don't get a problem when the
	whole range of the key has been used.
      */
      if (info->handle_duplicates == DUP_REPLACE &&
          table->next_number_field &&
          key_nr == table->s->next_number_index &&
	  table->file->auto_increment_column_changed)
	goto err;
      if (table->file->table_flags() & HA_DUPP_POS)
      {
	if (table->file->rnd_pos(table->record[1],table->file->dupp_ref))
	  goto err;
      }
      else
      {
	if (table->file->extra(HA_EXTRA_FLUSH_CACHE)) /* Not needed with NISAM */
	{
	  error=my_errno;
	  goto err;
	}

	if (!key)
	{
	  if (!(key=(char*) my_safe_alloca(table->s->max_unique_length,
					   MAX_KEY_LENGTH)))
	  {
	    error=ENOMEM;
	    goto err;
	  }
	}
	key_copy((byte*) key,table->record[0],table->key_info+key_nr,0);
	if ((error=(table->file->index_read_idx(table->record[1],key_nr,
						(byte*) key,
						table->key_info[key_nr].
						key_length,
						HA_READ_KEY_EXACT))))
	  goto err;
      }
      if (info->handle_duplicates == DUP_UPDATE)
      {
        int res= 0;
        /*
          We don't check for other UNIQUE keys - the first row
          that matches, is updated. If update causes a conflict again,
          an error is returned
        */
	DBUG_ASSERT(table->insert_values != NULL);
        store_record(table,insert_values);
        restore_record(table,record[1]);
        DBUG_ASSERT(info->update_fields->elements ==
                    info->update_values->elements);
        if (fill_record_n_invoke_before_triggers(thd, *info->update_fields,
                                                 *info->update_values, 0,
                                                 table->triggers,
                                                 TRG_EVENT_UPDATE))
          goto before_trg_err;

        /* CHECK OPTION for VIEW ... ON DUPLICATE KEY UPDATE ... */
        if (info->view &&
            (res= info->view->view_check_option(current_thd, info->ignore)) ==
            VIEW_CHECK_SKIP)
          goto ok_or_after_trg_err;
        if (res == VIEW_CHECK_ERROR)
          goto before_trg_err;

        if (thd->clear_next_insert_id)
        {
          /* Reset auto-increment cacheing if we do an update */
          thd->clear_next_insert_id= 0;
          thd->next_insert_id= 0;
        }
        if ((error=table->file->update_row(table->record[1],table->record[0])))
	{
	  if ((error == HA_ERR_FOUND_DUPP_KEY) && info->ignore)
            goto ok_or_after_trg_err;
          goto err;
	}
        info->updated++;

        trg_error= (table->triggers &&
                    table->triggers->process_triggers(thd, TRG_EVENT_UPDATE,
                                                      TRG_ACTION_AFTER, TRUE));
        info->copied++;
        goto ok_or_after_trg_err;
      }
      else /* DUP_REPLACE */
      {
	/*
	  The manual defines the REPLACE semantics that it is either
	  an INSERT or DELETE(s) + INSERT; FOREIGN KEY checks in
	  InnoDB do not function in the defined way if we allow MySQL
	  to convert the latter operation internally to an UPDATE.
          We also should not perform this conversion if we have 
          timestamp field with ON UPDATE which is different from DEFAULT.
	*/
	if (last_uniq_key(table,key_nr) &&
	    !table->file->referenced_by_foreign_key() &&
            (table->timestamp_field_type == TIMESTAMP_NO_AUTO_SET ||
             table->timestamp_field_type == TIMESTAMP_AUTO_SET_ON_BOTH))
        {
          if (table->triggers &&
              table->triggers->process_triggers(thd, TRG_EVENT_UPDATE,
                                                TRG_ACTION_BEFORE, TRUE))
            goto before_trg_err;
          if (thd->clear_next_insert_id)
          {
            /* Reset auto-increment cacheing if we do an update */
            thd->clear_next_insert_id= 0;
            thd->next_insert_id= 0;
          }
          if ((error=table->file->update_row(table->record[1],
					     table->record[0])))
            goto err;
          info->deleted++;
          trg_error= (table->triggers &&
                      table->triggers->process_triggers(thd, TRG_EVENT_UPDATE,
                                                        TRG_ACTION_AFTER,
                                                        TRUE));
          /* Update logfile and count */
          info->copied++;
          goto ok_or_after_trg_err;
        }
        else
        {
          if (table->triggers &&
              table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                                TRG_ACTION_BEFORE, TRUE))
            goto before_trg_err;
          if ((error=table->file->delete_row(table->record[1])))
            goto err;
          info->deleted++;
          if (!table->file->has_transactions())
            thd->no_trans_update= 1;
          if (table->triggers &&
              table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                                TRG_ACTION_AFTER, TRUE))
          {
            trg_error= 1;
            goto ok_or_after_trg_err;
          }
          /* Let us attempt do write_row() once more */
        }
      }
    }
    info->copied++;
    trg_error= (table->triggers &&
                table->triggers->process_triggers(thd, TRG_EVENT_INSERT,
                                                  TRG_ACTION_AFTER, TRUE));
  }
  else if ((error=table->file->write_row(table->record[0])))
  {
    if (!info->ignore ||
	(error != HA_ERR_FOUND_DUPP_KEY && error != HA_ERR_FOUND_DUPP_UNIQUE))
      goto err;
    table->file->restore_auto_increment();
  }
  else
  {
    info->copied++;
    trg_error= (table->triggers &&
                table->triggers->process_triggers(thd, TRG_EVENT_INSERT,
                                                  TRG_ACTION_AFTER, TRUE));
  }

ok_or_after_trg_err:
  if (key)
    my_safe_afree(key,table->s->max_unique_length,MAX_KEY_LENGTH);
  if (!table->file->has_transactions())
    thd->no_trans_update= 1;
  DBUG_RETURN(trg_error);

err:
  info->last_errno= error;
  /* current_select is NULL if this is a delayed insert */
  if (thd->lex->current_select)
    thd->lex->current_select->no_error= 0;        // Give error
  table->file->print_error(error,MYF(0));

before_trg_err:
  if (key)
    my_safe_afree(key, table->s->max_unique_length, MAX_KEY_LENGTH);
  DBUG_RETURN(1);
}


/******************************************************************************
  Check that all fields with arn't null_fields are used
******************************************************************************/

int check_that_all_fields_are_given_values(THD *thd, TABLE *entry,
                                           TABLE_LIST *table_list)
{
  int err= 0;
  for (Field **field=entry->field ; *field ; field++)
  {
    if ((*field)->query_id != thd->query_id &&
        ((*field)->flags & NO_DEFAULT_VALUE_FLAG) &&
        ((*field)->real_type() != FIELD_TYPE_ENUM))
    {
      bool view= FALSE;
      if (table_list)
      {
        table_list= table_list->top_table();
        view= test(table_list->view);
      }
      if (view)
      {
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_NO_DEFAULT_FOR_VIEW_FIELD,
                            ER(ER_NO_DEFAULT_FOR_VIEW_FIELD),
                            table_list->view_db.str,
                            table_list->view_name.str);
      }
      else
      {
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_NO_DEFAULT_FOR_FIELD,
                            ER(ER_NO_DEFAULT_FOR_FIELD),
                            (*field)->field_name);
      }
      err= 1;
    }
  }
  return thd->abort_on_warning ? err : 0;
}

/*****************************************************************************
  Handling of delayed inserts
  A thread is created for each table that one uses with the DELAYED attribute.
*****************************************************************************/

#ifndef EMBEDDED_LIBRARY

class delayed_row :public ilink {
public:
  char *record,*query;
  enum_duplicates dup;
  time_t start_time;
  bool query_start_used,last_insert_id_used,insert_id_used, ignore, log_query;
  ulonglong last_insert_id;
  timestamp_auto_set_type timestamp_field_type;
  uint query_length;

  delayed_row(enum_duplicates dup_arg, bool ignore_arg, bool log_query_arg)
    :record(0), query(0), dup(dup_arg), ignore(ignore_arg), log_query(log_query_arg) {}
  ~delayed_row()
  {
    x_free(record);
  }
};


class delayed_insert :public ilink {
  uint locks_in_memory;
public:
  THD thd;
  TABLE *table;
  pthread_mutex_t mutex;
  pthread_cond_t cond,cond_client;
  volatile uint tables_in_use,stacked_inserts;
  volatile bool status,dead;
  COPY_INFO info;
  I_List<delayed_row> rows;
  ulong group_count;
  TABLE_LIST table_list;			// Argument

  delayed_insert()
    :locks_in_memory(0),
     table(0),tables_in_use(0),stacked_inserts(0), status(0), dead(0),
     group_count(0)
  {
    thd.security_ctx->user=thd.security_ctx->priv_user=(char*) delayed_user;
    thd.security_ctx->host=(char*) my_localhost;
    thd.current_tablenr=0;
    thd.version=refresh_version;
    thd.command=COM_DELAYED_INSERT;
    thd.lex->current_select= 0; 		// for my_message_sql
    thd.lex->sql_command= SQLCOM_INSERT;        // For innodb::store_lock()

    bzero((char*) &thd.net, sizeof(thd.net));		// Safety
    bzero((char*) &table_list, sizeof(table_list));	// Safety
    thd.system_thread= SYSTEM_THREAD_DELAYED_INSERT;
    thd.security_ctx->host_or_ip= "";
    bzero((char*) &info,sizeof(info));
    pthread_mutex_init(&mutex,MY_MUTEX_INIT_FAST);
    pthread_cond_init(&cond,NULL);
    pthread_cond_init(&cond_client,NULL);
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    delayed_insert_threads++;
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
  }
  ~delayed_insert()
  {
    /* The following is not really needed, but just for safety */
    delayed_row *row;
    while ((row=rows.get()))
      delete row;
    if (table)
      close_thread_tables(&thd);
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
    pthread_cond_destroy(&cond_client);
    thd.unlink();				// Must be unlinked under lock
    x_free(thd.query);
    thd.security_ctx->user= thd.security_ctx->host=0;
    thread_count--;
    delayed_insert_threads--;
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
    VOID(pthread_cond_broadcast(&COND_thread_count)); /* Tell main we are ready */
  }

  /* The following is for checking when we can delete ourselves */
  inline void lock()
  {
    locks_in_memory++;				// Assume LOCK_delay_insert
  }
  void unlock()
  {
    pthread_mutex_lock(&LOCK_delayed_insert);
    if (!--locks_in_memory)
    {
      pthread_mutex_lock(&mutex);
      if (thd.killed && ! stacked_inserts && ! tables_in_use)
      {
	pthread_cond_signal(&cond);
	status=1;
      }
      pthread_mutex_unlock(&mutex);
    }
    pthread_mutex_unlock(&LOCK_delayed_insert);
  }
  inline uint lock_count() { return locks_in_memory; }

  TABLE* get_local_table(THD* client_thd);
  bool handle_inserts(void);
};


I_List<delayed_insert> delayed_threads;


delayed_insert *find_handler(THD *thd, TABLE_LIST *table_list)
{
  thd->proc_info="waiting for delay_list";
  pthread_mutex_lock(&LOCK_delayed_insert);	// Protect master list
  I_List_iterator<delayed_insert> it(delayed_threads);
  delayed_insert *tmp;
  while ((tmp=it++))
  {
    if (!strcmp(tmp->thd.db,table_list->db) &&
	!strcmp(table_list->table_name,tmp->table->s->table_name))
    {
      tmp->lock();
      break;
    }
  }
  pthread_mutex_unlock(&LOCK_delayed_insert); // For unlink from list
  return tmp;
}


static TABLE *delayed_get_table(THD *thd,TABLE_LIST *table_list)
{
  int error;
  delayed_insert *tmp;
  TABLE *table;
  DBUG_ENTER("delayed_get_table");

  if (!table_list->db)
    table_list->db=thd->db;

  /* Find the thread which handles this table. */
  if (!(tmp=find_handler(thd,table_list)))
  {
    /*
      No match. Create a new thread to handle the table, but
      no more than max_insert_delayed_threads.
    */
    if (delayed_insert_threads >= thd->variables.max_insert_delayed_threads)
      DBUG_RETURN(0);
    thd->proc_info="Creating delayed handler";
    pthread_mutex_lock(&LOCK_delayed_create);
    /*
      The first search above was done without LOCK_delayed_create.
      Another thread might have created the handler in between. Search again.
    */
    if (! (tmp= find_handler(thd, table_list)))
    {
      /*
        Avoid that a global read lock steps in while we are creating the
        new thread. It would block trying to open the table. Hence, the
        DI thread and this thread would wait until after the global
        readlock is gone. Since the insert thread needs to wait for a
        global read lock anyway, we do it right now. Note that
        wait_if_global_read_lock() sets a protection against a new
        global read lock when it succeeds. This needs to be released by
        start_waiting_global_read_lock().
      */
      if (wait_if_global_read_lock(thd, 0, 1))
        goto err;
      if (!(tmp=new delayed_insert()))
      {
	my_error(ER_OUTOFMEMORY,MYF(0),sizeof(delayed_insert));
	goto err1;
      }
      pthread_mutex_lock(&LOCK_thread_count);
      thread_count++;
      pthread_mutex_unlock(&LOCK_thread_count);
      if (!(tmp->thd.db=my_strdup(table_list->db,MYF(MY_WME))) ||
	  !(tmp->thd.query=my_strdup(table_list->table_name,MYF(MY_WME))))
      {
	delete tmp;
	my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
	goto err1;
      }
      tmp->table_list= *table_list;			// Needed to open table
      tmp->table_list.db= tmp->thd.db;
      tmp->table_list.alias= tmp->table_list.table_name= tmp->thd.query;
      tmp->lock();
      pthread_mutex_lock(&tmp->mutex);
      if ((error=pthread_create(&tmp->thd.real_id,&connection_attrib,
				handle_delayed_insert,(void*) tmp)))
      {
	DBUG_PRINT("error",
		   ("Can't create thread to handle delayed insert (error %d)",
		    error));
	pthread_mutex_unlock(&tmp->mutex);
	tmp->unlock();
	delete tmp;
	my_error(ER_CANT_CREATE_THREAD, MYF(0), error);
	goto err1;
      }

      /* Wait until table is open */
      thd->proc_info="waiting for handler open";
      while (!tmp->thd.killed && !tmp->table && !thd->killed)
      {
	pthread_cond_wait(&tmp->cond_client,&tmp->mutex);
      }
      pthread_mutex_unlock(&tmp->mutex);
      /*
        Release the protection against the global read lock and wake
        everyone, who might want to set a global read lock.
      */
      start_waiting_global_read_lock(thd);
      thd->proc_info="got old table";
      if (tmp->thd.killed)
      {
	if (tmp->thd.is_fatal_error)
	{
	  /* Copy error message and abort */
	  thd->fatal_error();
	  strmov(thd->net.last_error,tmp->thd.net.last_error);
	  thd->net.last_errno=tmp->thd.net.last_errno;
	}
	tmp->unlock();
	goto err;
      }
      if (thd->killed)
      {
	tmp->unlock();
	goto err;
      }
    }
    pthread_mutex_unlock(&LOCK_delayed_create);
  }

  pthread_mutex_lock(&tmp->mutex);
  table= tmp->get_local_table(thd);
  pthread_mutex_unlock(&tmp->mutex);
  if (table)
    thd->di=tmp;
  else if (tmp->thd.is_fatal_error)
    thd->fatal_error();
  /* Unlock the delayed insert object after its last access. */
  tmp->unlock();
  DBUG_RETURN((table_list->table=table));

 err1:
  thd->fatal_error();
  /*
    Release the protection against the global read lock and wake
    everyone, who might want to set a global read lock.
  */
  start_waiting_global_read_lock(thd);
 err:
  pthread_mutex_unlock(&LOCK_delayed_create);
  DBUG_RETURN(0); // Continue with normal insert
}


/*
  As we can't let many threads modify the same TABLE structure, we create
  an own structure for each tread.  This includes a row buffer to save the
  column values and new fields that points to the new row buffer.
  The memory is allocated in the client thread and is freed automaticly.
*/

TABLE *delayed_insert::get_local_table(THD* client_thd)
{
  my_ptrdiff_t adjust_ptrs;
  Field **field,**org_field, *found_next_number_field;
  TABLE *copy;

  /* First request insert thread to get a lock */
  status=1;
  tables_in_use++;
  if (!thd.lock)				// Table is not locked
  {
    client_thd->proc_info="waiting for handler lock";
    pthread_cond_signal(&cond);			// Tell handler to lock table
    while (!dead && !thd.lock && ! client_thd->killed)
    {
      pthread_cond_wait(&cond_client,&mutex);
    }
    client_thd->proc_info="got handler lock";
    if (client_thd->killed)
      goto error;
    if (dead)
    {
      strmov(client_thd->net.last_error,thd.net.last_error);
      client_thd->net.last_errno=thd.net.last_errno;
      goto error;
    }
  }

  client_thd->proc_info="allocating local table";
  copy= (TABLE*) client_thd->alloc(sizeof(*copy)+
				   (table->s->fields+1)*sizeof(Field**)+
				   table->s->reclength);
  if (!copy)
    goto error;
  *copy= *table;
  copy->s= &copy->share_not_to_be_used;
  // No name hashing
  bzero((char*) &copy->s->name_hash,sizeof(copy->s->name_hash));
  /* We don't need to change the file handler here */

  field=copy->field=(Field**) (copy+1);
  copy->record[0]=(byte*) (field+table->s->fields+1);
  memcpy((char*) copy->record[0],(char*) table->record[0],table->s->reclength);

  /* Make a copy of all fields */

  adjust_ptrs=PTR_BYTE_DIFF(copy->record[0],table->record[0]);

  found_next_number_field=table->found_next_number_field;
  for (org_field=table->field ; *org_field ; org_field++,field++)
  {
    if (!(*field= (*org_field)->new_field(client_thd->mem_root,copy)))
      return 0;
    (*field)->orig_table= copy;			// Remove connection
    (*field)->move_field(adjust_ptrs);		// Point at copy->record[0]
    if (*org_field == found_next_number_field)
      (*field)->table->found_next_number_field= *field;
  }
  *field=0;

  /* Adjust timestamp */
  if (table->timestamp_field)
  {
    /* Restore offset as this may have been reset in handle_inserts */
    copy->timestamp_field=
      (Field_timestamp*) copy->field[table->s->timestamp_field_offset];
    copy->timestamp_field->unireg_check= table->timestamp_field->unireg_check;
    copy->timestamp_field_type= copy->timestamp_field->get_auto_set_type();
  }

  /* _rowid is not used with delayed insert */
  copy->rowid_field=0;

  /* Adjust in_use for pointing to client thread */
  copy->in_use= client_thd;
  
  return copy;

  /* Got fatal error */
 error:
  tables_in_use--;
  status=1;
  pthread_cond_signal(&cond);			// Inform thread about abort
  return 0;
}


/* Put a question in queue */

static int write_delayed(THD *thd,TABLE *table,enum_duplicates duplic, bool ignore,
			 char *query, uint query_length, bool log_on)
{
  delayed_row *row=0;
  delayed_insert *di=thd->di;
  DBUG_ENTER("write_delayed");

  thd->proc_info="waiting for handler insert";
  pthread_mutex_lock(&di->mutex);
  while (di->stacked_inserts >= delayed_queue_size && !thd->killed)
    pthread_cond_wait(&di->cond_client,&di->mutex);
  thd->proc_info="storing row into queue";

  if (thd->killed || !(row= new delayed_row(duplic, ignore, log_on)))
    goto err;

  if (!query)
    query_length=0;
  if (!(row->record= (char*) my_malloc(table->s->reclength+query_length+1,
				       MYF(MY_WME))))
    goto err;
  memcpy(row->record, table->record[0], table->s->reclength);
  if (query_length)
  {
    row->query= row->record+table->s->reclength;
    memcpy(row->query,query,query_length+1);
  }
  row->query_length=		query_length;
  row->start_time=		thd->start_time;
  row->query_start_used=	thd->query_start_used;
  row->last_insert_id_used=	thd->last_insert_id_used;
  row->insert_id_used=		thd->insert_id_used;
  row->last_insert_id=		thd->last_insert_id;
  row->timestamp_field_type=    table->timestamp_field_type;

  di->rows.push_back(row);
  di->stacked_inserts++;
  di->status=1;
  if (table->s->blob_fields)
    unlink_blobs(table);
  pthread_cond_signal(&di->cond);

  thread_safe_increment(delayed_rows_in_use,&LOCK_delayed_status);
  pthread_mutex_unlock(&di->mutex);
  DBUG_RETURN(0);

 err:
  delete row;
  pthread_mutex_unlock(&di->mutex);
  DBUG_RETURN(1);
}


static void end_delayed_insert(THD *thd)
{
  DBUG_ENTER("end_delayed_insert");
  delayed_insert *di=thd->di;
  pthread_mutex_lock(&di->mutex);
  DBUG_PRINT("info",("tables in use: %d",di->tables_in_use));
  if (!--di->tables_in_use || di->thd.killed)
  {						// Unlock table
    di->status=1;
    pthread_cond_signal(&di->cond);
  }
  pthread_mutex_unlock(&di->mutex);
  DBUG_VOID_RETURN;
}


/* We kill all delayed threads when doing flush-tables */

void kill_delayed_threads(void)
{
  VOID(pthread_mutex_lock(&LOCK_delayed_insert)); // For unlink from list

  I_List_iterator<delayed_insert> it(delayed_threads);
  delayed_insert *tmp;
  while ((tmp=it++))
  {
    /* Ensure that the thread doesn't kill itself while we are looking at it */
    pthread_mutex_lock(&tmp->mutex);
    tmp->thd.killed= THD::KILL_CONNECTION;
    if (tmp->thd.mysys_var)
    {
      pthread_mutex_lock(&tmp->thd.mysys_var->mutex);
      if (tmp->thd.mysys_var->current_cond)
      {
	/*
	  We need the following test because the main mutex may be locked
	  in handle_delayed_insert()
	*/
	if (&tmp->mutex != tmp->thd.mysys_var->current_mutex)
	  pthread_mutex_lock(tmp->thd.mysys_var->current_mutex);
	pthread_cond_broadcast(tmp->thd.mysys_var->current_cond);
	if (&tmp->mutex != tmp->thd.mysys_var->current_mutex)
	  pthread_mutex_unlock(tmp->thd.mysys_var->current_mutex);
      }
      pthread_mutex_unlock(&tmp->thd.mysys_var->mutex);
    }
    pthread_mutex_unlock(&tmp->mutex);
  }
  VOID(pthread_mutex_unlock(&LOCK_delayed_insert)); // For unlink from list
}


/*
 * Create a new delayed insert thread
*/

pthread_handler_t handle_delayed_insert(void *arg)
{
  delayed_insert *di=(delayed_insert*) arg;
  THD *thd= &di->thd;

  pthread_detach_this_thread();
  /* Add thread to THD list so that's it's visible in 'show processlist' */
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id=thread_id++;
  thd->end_time();
  threads.append(thd);
  thd->killed=abort_loop ? THD::KILL_CONNECTION : THD::NOT_KILLED;
  pthread_mutex_unlock(&LOCK_thread_count);

  /*
    Wait until the client runs into pthread_cond_wait(),
    where we free it after the table is opened and di linked in the list.
    If we did not wait here, the client might detect the opened table
    before it is linked to the list. It would release LOCK_delayed_create
    and allow another thread to create another handler for the same table,
    since it does not find one in the list.
  */
  pthread_mutex_lock(&di->mutex);
#if !defined( __WIN__) && !defined(OS2)	/* Win32 calls this in pthread_create */
  if (my_thread_init())
  {
    strmov(thd->net.last_error,ER(thd->net.last_errno=ER_OUT_OF_RESOURCES));
    goto end;
  }
#endif

  DBUG_ENTER("handle_delayed_insert");
  if (init_thr_lock() || thd->store_globals())
  {
    thd->fatal_error();
    strmov(thd->net.last_error,ER(thd->net.last_errno=ER_OUT_OF_RESOURCES));
    goto end;
  }
#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif

  /* open table */

  if (!(di->table=open_ltable(thd,&di->table_list,TL_WRITE_DELAYED)))
  {
    thd->fatal_error();				// Abort waiting inserts
    goto end;
  }
  if (!(di->table->file->table_flags() & HA_CAN_INSERT_DELAYED))
  {
    thd->fatal_error();
    my_error(ER_ILLEGAL_HA, MYF(0), di->table_list.table_name);
    goto end;
  }
  di->table->copy_blobs=1;

  /* One can now use this */
  pthread_mutex_lock(&LOCK_delayed_insert);
  delayed_threads.append(di);
  pthread_mutex_unlock(&LOCK_delayed_insert);

  /* Tell client that the thread is initialized */
  pthread_cond_signal(&di->cond_client);

  /* Now wait until we get an insert or lock to handle */
  /* We will not abort as long as a client thread uses this thread */

  for (;;)
  {
    if (thd->killed == THD::KILL_CONNECTION)
    {
      uint lock_count;
      /*
	Remove this from delay insert list so that no one can request a
	table from this
      */
      pthread_mutex_unlock(&di->mutex);
      pthread_mutex_lock(&LOCK_delayed_insert);
      di->unlink();
      lock_count=di->lock_count();
      pthread_mutex_unlock(&LOCK_delayed_insert);
      pthread_mutex_lock(&di->mutex);
      if (!lock_count && !di->tables_in_use && !di->stacked_inserts)
	break;					// Time to die
    }

    if (!di->status && !di->stacked_inserts)
    {
      struct timespec abstime;
      set_timespec(abstime, delayed_insert_timeout);

      /* Information for pthread_kill */
      di->thd.mysys_var->current_mutex= &di->mutex;
      di->thd.mysys_var->current_cond= &di->cond;
      di->thd.proc_info="Waiting for INSERT";

      DBUG_PRINT("info",("Waiting for someone to insert rows"));
      while (!thd->killed)
      {
	int error;
#if defined(HAVE_BROKEN_COND_TIMEDWAIT)
	error=pthread_cond_wait(&di->cond,&di->mutex);
#else
	error=pthread_cond_timedwait(&di->cond,&di->mutex,&abstime);
#ifdef EXTRA_DEBUG
	if (error && error != EINTR && error != ETIMEDOUT)
	{
	  fprintf(stderr, "Got error %d from pthread_cond_timedwait\n",error);
	  DBUG_PRINT("error",("Got error %d from pthread_cond_timedwait",
			      error));
	}
#endif
#endif
	if (thd->killed || di->status)
	  break;
	if (error == ETIMEDOUT)
	{
	  thd->killed= THD::KILL_CONNECTION;
	  break;
	}
      }
      /* We can't lock di->mutex and mysys_var->mutex at the same time */
      pthread_mutex_unlock(&di->mutex);
      pthread_mutex_lock(&di->thd.mysys_var->mutex);
      di->thd.mysys_var->current_mutex= 0;
      di->thd.mysys_var->current_cond= 0;
      pthread_mutex_unlock(&di->thd.mysys_var->mutex);
      pthread_mutex_lock(&di->mutex);
    }
    di->thd.proc_info=0;

    if (di->tables_in_use && ! thd->lock)
    {
      bool not_used;
      /*
        Request for new delayed insert.
        Lock the table, but avoid to be blocked by a global read lock.
        If we got here while a global read lock exists, then one or more
        inserts started before the lock was requested. These are allowed
        to complete their work before the server returns control to the
        client which requested the global read lock. The delayed insert
        handler will close the table and finish when the outstanding
        inserts are done.
      */
      if (! (thd->lock= mysql_lock_tables(thd, &di->table, 1,
                                          MYSQL_LOCK_IGNORE_GLOBAL_READ_LOCK,
                                          &not_used)))
      {
	/* Fatal error */
	di->dead= 1;
	thd->killed= THD::KILL_CONNECTION;
      }
      pthread_cond_broadcast(&di->cond_client);
    }
    if (di->stacked_inserts)
    {
      if (di->handle_inserts())
      {
	/* Some fatal error */
	di->dead= 1;
	thd->killed= THD::KILL_CONNECTION;
      }
    }
    di->status=0;
    if (!di->stacked_inserts && !di->tables_in_use && thd->lock)
    {
      /*
        No one is doing a insert delayed
        Unlock table so that other threads can use it
      */
      MYSQL_LOCK *lock=thd->lock;
      thd->lock=0;
      pthread_mutex_unlock(&di->mutex);
      mysql_unlock_tables(thd, lock);
      di->group_count=0;
      pthread_mutex_lock(&di->mutex);
    }
    if (di->tables_in_use)
      pthread_cond_broadcast(&di->cond_client); // If waiting clients
  }

end:
  /*
    di should be unlinked from the thread handler list and have no active
    clients
  */

  close_thread_tables(thd);			// Free the table
  di->table=0;
  di->dead= 1;                                  // If error
  thd->killed= THD::KILL_CONNECTION;	        // If error
  pthread_cond_broadcast(&di->cond_client);	// Safety
  pthread_mutex_unlock(&di->mutex);

  pthread_mutex_lock(&LOCK_delayed_create);	// Because of delayed_get_table
  pthread_mutex_lock(&LOCK_delayed_insert);	
  delete di;
  pthread_mutex_unlock(&LOCK_delayed_insert);
  pthread_mutex_unlock(&LOCK_delayed_create);  

  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(0);
}


/* Remove pointers from temporary fields to allocated values */

static void unlink_blobs(register TABLE *table)
{
  for (Field **ptr=table->field ; *ptr ; ptr++)
  {
    if ((*ptr)->flags & BLOB_FLAG)
      ((Field_blob *) (*ptr))->clear_temporary();
  }
}

/* Free blobs stored in current row */

static void free_delayed_insert_blobs(register TABLE *table)
{
  for (Field **ptr=table->field ; *ptr ; ptr++)
  {
    if ((*ptr)->flags & BLOB_FLAG)
    {
      char *str;
      ((Field_blob *) (*ptr))->get_ptr(&str);
      my_free(str,MYF(MY_ALLOW_ZERO_PTR));
      ((Field_blob *) (*ptr))->reset();
    }
  }
}


bool delayed_insert::handle_inserts(void)
{
  int error;
  ulong max_rows;
  bool using_ignore=0, using_bin_log=mysql_bin_log.is_open();
  delayed_row *row;
  DBUG_ENTER("handle_inserts");

  /* Allow client to insert new rows */
  pthread_mutex_unlock(&mutex);

  table->next_number_field=table->found_next_number_field;

  thd.proc_info="upgrading lock";
  if (thr_upgrade_write_delay_lock(*thd.lock->locks))
  {
    /* This can only happen if thread is killed by shutdown */
    sql_print_error(ER(ER_DELAYED_CANT_CHANGE_LOCK),table->s->table_name);
    goto err;
  }

  thd.proc_info="insert";
  max_rows= delayed_insert_limit;
  if (thd.killed || table->s->version != refresh_version)
  {
    thd.killed= THD::KILL_CONNECTION;
    max_rows= ~(ulong)0;                        // Do as much as possible
  }

  /*
    We can't use row caching when using the binary log because if
    we get a crash, then binary log will contain rows that are not yet
    written to disk, which will cause problems in replication.
  */
  if (!using_bin_log)
    table->file->extra(HA_EXTRA_WRITE_CACHE);
  pthread_mutex_lock(&mutex);
  while ((row=rows.get()))
  {
    stacked_inserts--;
    pthread_mutex_unlock(&mutex);
    memcpy(table->record[0],row->record,table->s->reclength);

    thd.start_time=row->start_time;
    thd.query_start_used=row->query_start_used;
    thd.last_insert_id=row->last_insert_id;
    thd.last_insert_id_used=row->last_insert_id_used;
    thd.insert_id_used=row->insert_id_used;
    table->timestamp_field_type= row->timestamp_field_type;

    info.ignore= row->ignore;
    info.handle_duplicates= row->dup;
    if (info.ignore ||
	info.handle_duplicates != DUP_ERROR)
    {
      table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
      using_ignore=1;
    }
    thd.clear_error(); // reset error for binlog
    if (write_record(&thd, table, &info))
    {
      info.error_count++;				// Ignore errors
      thread_safe_increment(delayed_insert_errors,&LOCK_delayed_status);
      row->log_query = 0;
    }
    if (using_ignore)
    {
      using_ignore=0;
      table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    }
    if (row->query && row->log_query && using_bin_log)
    {
      Query_log_event qinfo(&thd, row->query, row->query_length, 0, FALSE);
      mysql_bin_log.write(&qinfo);
    }
    if (table->s->blob_fields)
      free_delayed_insert_blobs(table);
    thread_safe_sub(delayed_rows_in_use,1,&LOCK_delayed_status);
    thread_safe_increment(delayed_insert_writes,&LOCK_delayed_status);
    pthread_mutex_lock(&mutex);

    delete row;
    /*
      Let READ clients do something once in a while
      We should however not break in the middle of a multi-line insert
      if we have binary logging enabled as we don't want other commands
      on this table until all entries has been processed
    */
    if (group_count++ >= max_rows && (row= rows.head()) &&
	(!(row->log_query & using_bin_log) ||
	 row->query))
    {
      group_count=0;
      if (stacked_inserts || tables_in_use)	// Let these wait a while
      {
	if (tables_in_use)
	  pthread_cond_broadcast(&cond_client); // If waiting clients
	thd.proc_info="reschedule";
	pthread_mutex_unlock(&mutex);
	if ((error=table->file->extra(HA_EXTRA_NO_CACHE)))
	{
	  /* This should never happen */
	  table->file->print_error(error,MYF(0));
	  sql_print_error("%s",thd.net.last_error);
	  goto err;
	}
	query_cache_invalidate3(&thd, table, 1);
	if (thr_reschedule_write_lock(*thd.lock->locks))
	{
	  /* This should never happen */
	  sql_print_error(ER(ER_DELAYED_CANT_CHANGE_LOCK),table->s->table_name);
	}
	if (!using_bin_log)
	  table->file->extra(HA_EXTRA_WRITE_CACHE);
	pthread_mutex_lock(&mutex);
	thd.proc_info="insert";
      }
      if (tables_in_use)
	pthread_cond_broadcast(&cond_client);	// If waiting clients
    }
  }

  thd.proc_info=0;
  table->next_number_field=0;
  pthread_mutex_unlock(&mutex);
  if ((error=table->file->extra(HA_EXTRA_NO_CACHE)))
  {						// This shouldn't happen
    table->file->print_error(error,MYF(0));
    sql_print_error("%s",thd.net.last_error);
    goto err;
  }
  query_cache_invalidate3(&thd, table, 1);
  pthread_mutex_lock(&mutex);
  DBUG_RETURN(0);

 err:
  /* Remove all not used rows */
  while ((row=rows.get()))
  {
    delete row;
    thread_safe_increment(delayed_insert_errors,&LOCK_delayed_status);
    stacked_inserts--;
  }
  thread_safe_increment(delayed_insert_errors, &LOCK_delayed_status);
  pthread_mutex_lock(&mutex);
  DBUG_RETURN(1);
}
#endif /* EMBEDDED_LIBRARY */

/***************************************************************************
  Store records in INSERT ... SELECT *
***************************************************************************/


/*
  make insert specific preparation and checks after opening tables

  SYNOPSIS
    mysql_insert_select_prepare()
    thd         thread handler

  RETURN
    FALSE OK
    TRUE  Error
*/

bool mysql_insert_select_prepare(THD *thd)
{
  LEX *lex= thd->lex;
  SELECT_LEX *select_lex= &lex->select_lex;
  TABLE_LIST *first_select_leaf_table;
  DBUG_ENTER("mysql_insert_select_prepare");

  /*
    SELECT_LEX do not belong to INSERT statement, so we can't add WHERE
    clause if table is VIEW
  */
  
  if (mysql_prepare_insert(thd, lex->query_tables,
                           lex->query_tables->table, lex->field_list, 0,
                           lex->update_list, lex->value_list,
                           lex->duplicates,
                           &select_lex->where, TRUE))
    DBUG_RETURN(TRUE);

  /*
    exclude first table from leaf tables list, because it belong to
    INSERT
  */
  DBUG_ASSERT(select_lex->leaf_tables != 0);
  lex->leaf_tables_insert= select_lex->leaf_tables;
  /* skip all leaf tables belonged to view where we are insert */
  for (first_select_leaf_table= select_lex->leaf_tables->next_leaf;
       first_select_leaf_table &&
       first_select_leaf_table->belong_to_view &&
       first_select_leaf_table->belong_to_view ==
       lex->leaf_tables_insert->belong_to_view;
       first_select_leaf_table= first_select_leaf_table->next_leaf)
  {}
  select_lex->leaf_tables= first_select_leaf_table;
  DBUG_RETURN(FALSE);
}


select_insert::select_insert(TABLE_LIST *table_list_par, TABLE *table_par,
                             List<Item> *fields_par,
                             List<Item> *update_fields,
                             List<Item> *update_values,
                             enum_duplicates duplic,
                             bool ignore_check_option_errors)
  :table_list(table_list_par), table(table_par), fields(fields_par),
   last_insert_id(0),
   insert_into_view(table_list_par && table_list_par->view != 0)
{
  bzero((char*) &info,sizeof(info));
  info.handle_duplicates= duplic;
  info.ignore= ignore_check_option_errors;
  info.update_fields= update_fields;
  info.update_values= update_values;
  if (table_list_par)
    info.view= (table_list_par->view ? table_list_par : 0);
}


int
select_insert::prepare(List<Item> &values, SELECT_LEX_UNIT *u)
{
  LEX *lex= thd->lex;
  int res;
  SELECT_LEX *lex_current_select_save= lex->current_select;
  DBUG_ENTER("select_insert::prepare");

  unit= u;
  /*
    Since table in which we are going to insert is added to the first
    select, LEX::current_select should point to the first select while
    we are fixing fields from insert list.
  */
  lex->current_select= &lex->select_lex;
  res= check_insert_fields(thd, table_list, *fields, values,
                           !insert_into_view) ||
       setup_fields(thd, 0, values, 0, 0, 0);

  if (info.handle_duplicates == DUP_UPDATE)
  {
    /* Save the state of the current name resolution context. */
    Name_resolution_context *context= &lex->select_lex.context;
    TABLE_LIST *save_table_list;
    TABLE_LIST *save_next_local;
    TABLE_LIST *save_first_name_resolution_table;
    TABLE_LIST *save_next_name_resolution_table;
    save_table_list=                  context->table_list;
    save_first_name_resolution_table= context->first_name_resolution_table;
    save_next_name_resolution_table=  (context->first_name_resolution_table) ?
                                      context->first_name_resolution_table->
                                               next_name_resolution_table :
                                      NULL;
    save_next_local= table_list->next_local;

    /* Perform name resolution only in the first table - 'table_list'. */
    table_list->next_local= 0;
    context->resolve_in_table_list_only(table_list);

    lex->select_lex.no_wrap_view_item= TRUE;
    res= res || check_update_fields(thd, context->table_list,
                                    *info.update_fields);
    lex->select_lex.no_wrap_view_item= FALSE;
    /*
      When we are not using GROUP BY we can refer to other tables in the
      ON DUPLICATE KEY part
    */       
    if (lex->select_lex.group_list.elements == 0)
    {
      context->table_list->next_local=       save_next_local;
      /* first_name_resolution_table was set by resolve_in_table_list_only() */
      context->first_name_resolution_table->
        next_name_resolution_table=          save_next_local;
    }
    res= res || setup_fields(thd, 0, *info.update_values, 1, 0, 0);

    /* Restore the current context. */
    table_list->next_local= save_next_local;
    context->first_name_resolution_table= save_first_name_resolution_table;
    if (context->first_name_resolution_table)
      context->first_name_resolution_table->
               next_name_resolution_table= save_next_name_resolution_table;

  }

  lex->current_select= lex_current_select_save;
  if (res)
    DBUG_RETURN(1);
  /*
    if it is INSERT into join view then check_insert_fields already found
    real table for insert
  */
  table= table_list->table;

  /*
    Is table which we are changing used somewhere in other parts of
    query
  */
  if (!(lex->current_select->options & OPTION_BUFFER_RESULT) &&
      unique_table(table_list, table_list->next_global))
  {
    /* Using same table for INSERT and SELECT */
    lex->current_select->options|= OPTION_BUFFER_RESULT;
    lex->current_select->join->select_options|= OPTION_BUFFER_RESULT;
  }
  else
  {
    /*
      We must not yet prepare the result table if it is the same as one of the 
      source tables (INSERT SELECT). The preparation may disable 
      indexes on the result table, which may be used during the select, if it
      is the same table (Bug #6034). Do the preparation after the select phase
      in select_insert::prepare2().
    */
    table->file->start_bulk_insert((ha_rows) 0);
  }
  restore_record(table,s->default_values);		// Get empty record
  table->next_number_field=table->found_next_number_field;
  thd->cuted_fields=0;
  if (info.ignore || info.handle_duplicates != DUP_ERROR)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  thd->no_trans_update= 0;
  thd->abort_on_warning= (!info.ignore &&
                          (thd->variables.sql_mode &
                           (MODE_STRICT_TRANS_TABLES |
                            MODE_STRICT_ALL_TABLES)));
  res= ((fields->elements &&
         check_that_all_fields_are_given_values(thd, table, table_list)) ||
        table_list->prepare_where(thd, 0, TRUE) ||
        table_list->prepare_check_option(thd));
  DBUG_RETURN(res);
}


/*
  Finish the preparation of the result table.

  SYNOPSIS
    select_insert::prepare2()
    void

  DESCRIPTION
    If the result table is the same as one of the source tables (INSERT SELECT),
    the result table is not finally prepared at the join prepair phase.
    Do the final preparation now.
		       
  RETURN
    0   OK
*/

int select_insert::prepare2(void)
{
  DBUG_ENTER("select_insert::prepare2");
  if (thd->lex->current_select->options & OPTION_BUFFER_RESULT)
    table->file->start_bulk_insert((ha_rows) 0);
  DBUG_RETURN(0);
}


void select_insert::cleanup()
{
  /* select_insert/select_create are never re-used in prepared statement */
  DBUG_ASSERT(0);
}

select_insert::~select_insert()
{
  DBUG_ENTER("~select_insert");
  if (table)
  {
    table->next_number_field=0;
    table->file->ha_reset();
  }
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  thd->abort_on_warning= 0;
  DBUG_VOID_RETURN;
}


bool select_insert::send_data(List<Item> &values)
{
  DBUG_ENTER("select_insert::send_data");
  bool error=0;
  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(0);
  }

  thd->count_cuted_fields= CHECK_FIELD_WARN;	// Calculate cuted fields
  store_values(values);
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  if (thd->net.report_error)
    DBUG_RETURN(1);
  if (table_list)                               // Not CREATE ... SELECT
  {
    switch (table_list->view_check_option(thd, info.ignore)) {
    case VIEW_CHECK_SKIP:
      DBUG_RETURN(0);
    case VIEW_CHECK_ERROR:
      DBUG_RETURN(1);
    }
  }
  if (!(error= write_record(thd, table, &info)))
  {
    if (table->triggers || info.handle_duplicates == DUP_UPDATE)
    {
      /*
        Restore fields of the record since it is possible that they were
        changed by ON DUPLICATE KEY UPDATE clause.
    
        If triggers exist then whey can modify some fields which were not
        originally touched by INSERT ... SELECT, so we have to restore
        their original values for the next row.
      */
      restore_record(table, s->default_values);
    }
    if (table->next_number_field)
    {
      /*
        Clear auto-increment field for the next record, if triggers are used
        we will clear it twice, but this should be cheap.
      */
      table->next_number_field->reset();
      if (!last_insert_id && thd->insert_id_used)
        last_insert_id= thd->insert_id();
    }
  }
  DBUG_RETURN(error);
}


void select_insert::store_values(List<Item> &values)
{
  if (fields->elements)
    fill_record_n_invoke_before_triggers(thd, *fields, values, 1,
                                         table->triggers, TRG_EVENT_INSERT);
  else
    fill_record_n_invoke_before_triggers(thd, table->field, values, 1,
                                         table->triggers, TRG_EVENT_INSERT);
}

void select_insert::send_error(uint errcode,const char *err)
{
  DBUG_ENTER("select_insert::send_error");

  my_message(errcode, err, MYF(0));

  if (!table)
  {
    /*
      This can only happen when using CREATE ... SELECT and the table was not
      created becasue of an syntax error
    */
    DBUG_VOID_RETURN;
  }
  table->file->end_bulk_insert();
  /*
    If at least one row has been inserted/modified and will stay in the table
    (the table doesn't have transactions) (example: we got a duplicate key
    error while inserting into a MyISAM table) we must write to the binlog (and
    the error code will make the slave stop).
  */
  if ((info.copied || info.deleted || info.updated) &&
      !table->file->has_transactions())
  {
    if (last_insert_id)
      thd->insert_id(last_insert_id);		// For binary log
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query, thd->query_length,
                            table->file->has_transactions(), FALSE);
      mysql_bin_log.write(&qinfo);
    }
    if (!table->s->tmp_table)
      thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
  }
  if (info.copied || info.deleted || info.updated)
  {
    query_cache_invalidate3(thd, table, 1);
  }
  ha_rollback_stmt(thd);
  DBUG_VOID_RETURN;
}


bool select_insert::send_eof()
{
  int error,error2;
  DBUG_ENTER("select_insert::send_eof");

  error=table->file->end_bulk_insert();
  table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

  /*
    We must invalidate the table in the query cache before binlog writing
    and ha_autocommit_or_rollback
  */

  if (info.copied || info.deleted || info.updated)
  {
    query_cache_invalidate3(thd, table, 1);
    if (!(table->file->has_transactions() || table->s->tmp_table))
      thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
  }

  if (last_insert_id)
    thd->insert_id(last_insert_id);		// For binary log
  /* Write to binlog before commiting transaction */
  if (mysql_bin_log.is_open())
  {
    if (!error)
      thd->clear_error();
    Query_log_event qinfo(thd, thd->query, thd->query_length,
			  table->file->has_transactions(), FALSE);
    mysql_bin_log.write(&qinfo);
  }
  if ((error2=ha_autocommit_or_rollback(thd,error)) && ! error)
    error=error2;
  if (error)
  {
    table->file->print_error(error,MYF(0));
    DBUG_RETURN(1);
  }
  char buff[160];
  if (info.ignore)
    sprintf(buff, ER(ER_INSERT_INFO), (ulong) info.records,
	    (ulong) (info.records - info.copied), (ulong) thd->cuted_fields);
  else
    sprintf(buff, ER(ER_INSERT_INFO), (ulong) info.records,
	    (ulong) (info.deleted+info.updated), (ulong) thd->cuted_fields);
  thd->row_count_func= info.copied+info.deleted+info.updated;
  ::send_ok(thd, (ulong) thd->row_count_func, last_insert_id, buff);
  DBUG_RETURN(0);
}


/***************************************************************************
  CREATE TABLE (SELECT) ...
***************************************************************************/

int
select_create::prepare(List<Item> &values, SELECT_LEX_UNIT *u)
{
  DBUG_ENTER("select_create::prepare");

  unit= u;
  table= create_table_from_items(thd, create_info, create_table,
				 extra_fields, keys, &values, &lock);
  if (!table)
    DBUG_RETURN(-1);				// abort() deletes table

  if (table->s->fields < values.elements)
  {
    my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), 1);
    DBUG_RETURN(-1);
  }

  /* First field to copy */
  field=table->field+table->s->fields - values.elements;

  /* Don't set timestamp if used */
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

  table->next_number_field=table->found_next_number_field;

  restore_record(table,s->default_values);      // Get empty record
  thd->cuted_fields=0;
  if (info.ignore || info.handle_duplicates != DUP_ERROR)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  table->file->start_bulk_insert((ha_rows) 0);
  thd->no_trans_update= 0;
  thd->abort_on_warning= (!info.ignore &&
                          (thd->variables.sql_mode &
                           (MODE_STRICT_TRANS_TABLES |
                            MODE_STRICT_ALL_TABLES)));
  DBUG_RETURN(check_that_all_fields_are_given_values(thd, table,
                                                     table_list));
}


void select_create::store_values(List<Item> &values)
{
  fill_record_n_invoke_before_triggers(thd, field, values, 1,
                                       table->triggers, TRG_EVENT_INSERT);
}


void select_create::send_error(uint errcode,const char *err)
{
  /*
   Disable binlog, because we "roll back" partial inserts in ::abort
   by removing the table, even for non-transactional tables.
  */
  tmp_disable_binlog(thd);
  select_insert::send_error(errcode, err);
  reenable_binlog(thd);
}


bool select_create::send_eof()
{
  bool tmp=select_insert::send_eof();
  if (tmp)
    abort();
  else
  {
    table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    VOID(pthread_mutex_lock(&LOCK_open));
    mysql_unlock_tables(thd, lock);
    /*
      TODO:
      Check if we can remove the following two rows.
      We should be able to just keep the table in the table cache.
    */
    if (!table->s->tmp_table)
    {
      ulong version= table->s->version;
      hash_delete(&open_cache,(byte*) table);
      /* Tell threads waiting for refresh that something has happened */
      if (version != refresh_version)
        VOID(pthread_cond_broadcast(&COND_refresh));
    }
    lock=0;
    table=0;
    VOID(pthread_mutex_unlock(&LOCK_open));
  }
  return tmp;
}

void select_create::abort()
{
  VOID(pthread_mutex_lock(&LOCK_open));
  if (lock)
  {
    mysql_unlock_tables(thd, lock);
    lock=0;
  }
  if (table)
  {
    table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    enum db_type table_type=table->s->db_type;
    if (!table->s->tmp_table)
    {
      ulong version= table->s->version;
      hash_delete(&open_cache,(byte*) table);
      if (!create_info->table_existed)
        quick_rm_table(table_type, create_table->db, create_table->table_name);
      /* Tell threads waiting for refresh that something has happened */
      if (version != refresh_version)
        VOID(pthread_cond_broadcast(&COND_refresh));
    }
    else if (!create_info->table_existed)
      close_temporary_table(thd, create_table->db, create_table->table_name);
    table=0;
  }
  VOID(pthread_mutex_unlock(&LOCK_open));
}


/*****************************************************************************
  Instansiate templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List_iterator_fast<List_item>;
#ifndef EMBEDDED_LIBRARY
template class I_List<delayed_insert>;
template class I_List_iterator<delayed_insert>;
template class I_List<delayed_row>;
#endif /* EMBEDDED_LIBRARY */
#endif /* HAVE_EXPLICIT_TEMPLATE_INSTANTIATION */
