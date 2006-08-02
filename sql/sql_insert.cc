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

/*
  INSERT DELAYED

  Insert delayed is distinguished from a normal insert by lock_type ==
  TL_WRITE_DELAYED instead of TL_WRITE. It first tries to open a
  "delayed" table (delayed_get_table()), but falls back to
  open_and_lock_tables() on error and proceeds as normal insert then.

  Opening a "delayed" table means to find a delayed insert thread that
  has the table open already. If this fails, a new thread is created and
  waited for to open and lock the table.

  If accessing the thread succeeded, in
  delayed_insert::get_local_table() the table of the thread is copied
  for local use. A copy is required because the normal insert logic
  works on a target table, but the other threads table object must not
  be used. The insert logic uses the record buffer to create a record.
  And the delayed insert thread uses the record buffer to pass the
  record to the table handler. So there must be different objects. Also
  the copied table is not included in the lock, so that the statement
  can proceed even if the real table cannot be accessed at this moment.

  Copying a table object is not a trivial operation. Besides the TABLE
  object there are the field pointer array, the field objects and the
  record buffer. After copying the field objects, their pointers into
  the record must be "moved" to point to the new record buffer.

  After this setup the normal insert logic is used. Only that for
  delayed inserts write_delayed() is called instead of write_record().
  It inserts the rows into a queue and signals the delayed insert thread
  instead of writing directly to the table.

  The delayed insert thread awakes from the signal. It locks the table,
  inserts the rows from the queue, unlocks the table, and waits for the
  next signal. It does normally live until a FLUSH TABLES or SHUTDOWN.

*/

#include "mysql_priv.h"
#include "sp_head.h"
#include "sql_trigger.h"
#include "sql_select.h"
#include "sql_show.h"

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
                                  table->s->db.str, table->s->table_name.str,
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
    bitmap_set_all(table->write_set);
  }
  else
  {						// Part field list
    SELECT_LEX *select_lex= &thd->lex->select_lex;
    Name_resolution_context *context= &select_lex->context;
    Name_resolution_context_state ctx_state;
    int res;

    if (fields.elements != values.elements)
    {
      my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), 1L);
      return -1;
    }

    thd->dup_field= 0;
    select_lex->no_wrap_view_item= TRUE;

    /* Save the state of the current name resolution context. */
    ctx_state.save_state(context, table_list);

    /*
      Perform name resolution only in the first table - 'table_list',
      which is the table that is inserted into.
    */
    table_list->next_local= 0;
    context->resolve_in_table_list_only(table_list);
    res= setup_fields(thd, 0, fields, MARK_COLUMNS_WRITE, 0, 0);

    /* Restore the current context. */
    ctx_state.restore_state(context, table_list);
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

    if (check_unique && thd->dup_field)
    {
      my_error(ER_FIELD_SPECIFIED_TWICE, MYF(0), thd->dup_field->field_name);
      return -1;
    }
    if (table->timestamp_field)	// Don't automaticly set timestamp if used
    {
      if (bitmap_is_set(table->write_set,
                        table->timestamp_field->field_index))
        clear_timestamp_auto_bits(table->timestamp_field_type,
                                  TIMESTAMP_AUTO_SET_ON_INSERT);
      else
      {
        bitmap_set_bit(table->write_set,
                       table->timestamp_field->field_index);
      }
    }
  }
  if (table->found_next_number_field)
    table->mark_auto_increment_column();
  table->mark_columns_needed_for_insert();
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
  my_bool timestamp_mark;

  if (table->timestamp_field)
  {
    /*
      Unmark the timestamp field so that we can check if this is modified
      by update_fields
    */
    timestamp_mark= bitmap_test_and_clear(table->write_set,
                                          table->timestamp_field->field_index);
  }

  /* Check the fields we are going to modify */
  if (setup_fields(thd, 0, update_fields, MARK_COLUMNS_WRITE, 0, 0))
    return -1;

  if (table->timestamp_field)
  {
    /* Don't set timestamp column if this is modified. */
    if (bitmap_is_set(table->write_set,
                      table->timestamp_field->field_index))
      clear_timestamp_auto_bits(table->timestamp_field_type,
                                TIMESTAMP_AUTO_SET_ON_UPDATE);
    if (timestamp_mark)
      bitmap_set_bit(table->write_set,
                     table->timestamp_field->field_index);
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
  bool log_on= ((thd->options & OPTION_BIN_LOG) ||
                (!(thd->security_ctx->master_access & SUPER_ACL)));
  bool transactional_table, joins_freed= FALSE;
  bool changed;
  uint value_count;
  ulong counter = 1;
  ulonglong id;
  COPY_INFO info;
  TABLE *table= 0;
  List_iterator_fast<List_item> its(values_list);
  List_item *values;
  Name_resolution_context *context;
  Name_resolution_context_state ctx_state;
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
      DBUG_ASSERT(table_list->db); /* Must be set in the parser */
      if (find_locked_table(thd, table_list->db, table_list->table_name))
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
  ctx_state.save_state(context, table_list);

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
    if (setup_fields(thd, 0, *values, MARK_COLUMNS_READ, 0, 0))
      goto abort;
  }
  its.rewind ();
 
  /* Restore the current context. */
  ctx_state.restore_state(context, table_list);

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
    We should not start bulk inserts if this statement uses
    functions or invokes triggers since they may access
    to the same table and therefore should not see its
    inconsistent state created by this optimization.
    So we call start_bulk_insert to perform nesessary checks on
    values_list.elements, and - if nothing else - to initialize
    the code to make the call of end_bulk_insert() below safe.
  */
  if (lock_type != TL_WRITE_DELAYED && !thd->prelocked_mode)
    table->file->ha_start_bulk_insert(values_list.elements);

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

  free_underlaid_joins(thd, &thd->lex->select_lex);
  joins_freed= TRUE;

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
    if (!thd->prelocked_mode && table->file->ha_end_bulk_insert() && !error)
    {
      table->file->print_error(my_errno,MYF(0));
      error=1;
    }
    if (id && values_list.elements != 1)
      thd->insert_id(id);			// For update log
    else if (table->next_number_field && info.copied)
      id=table->next_number_field->val_int();	// Return auto_increment value

    transactional_table= table->file->has_transactions();

    if ((changed= (info.copied || info.deleted || info.updated)))
    {
      /*
        Invalidate the table in the query cache if something changed.
        For the transactional algorithm to work the invalidation must be
        before binlog writing and ha_autocommit_or_rollback
      */
      query_cache_invalidate3(thd, table_list, 1);
      if (error <= 0 || !transactional_table)
      {
        if (mysql_bin_log.is_open())
        {
          if (error <= 0)
            thd->clear_error();
          if (thd->binlog_query(THD::ROW_QUERY_TYPE,
                                thd->query, thd->query_length,
                                transactional_table, FALSE) &&
              transactional_table)
          {
            error=1;
          }
        }
        if (!transactional_table)
          thd->options|=OPTION_STATUS_NO_TRANS_UPDATE;
      }
    }
    if (transactional_table)
      error=ha_autocommit_or_rollback(thd,error);

    if (thd->lock)
    {
      mysql_unlock_tables(thd, thd->lock);
      /*
        Invalidate the table in the query cache if something changed
        after unlocking when changes become fisible.
        TODO: this is workaround. right way will be move invalidating in
        the unlock procedure.
      */
      if (lock_type ==  TL_WRITE_CONCURRENT_INSERT && changed)
      {
        query_cache_invalidate3(thd, table_list, 1);
      }
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
  if (table != NULL)
    table->file->release_auto_increment();
  thd->abort_on_warning= 0;
  DBUG_RETURN(FALSE);

abort:
#ifndef EMBEDDED_LIBRARY
  if (lock_type == TL_WRITE_DELAYED)
    end_delayed_insert(thd);
#endif
  if (table != NULL)
    table->file->release_auto_increment();
  if (!joins_freed)
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
  uint used_fields_buff_size= bitmap_buffer_size(table->s->fields);
  uint32 *used_fields_buff= (uint32*)thd->alloc(used_fields_buff_size);
  MY_BITMAP used_fields;
  DBUG_ENTER("check_key_in_view");

  if (!used_fields_buff)
    DBUG_RETURN(TRUE);  // EOM

  DBUG_ASSERT(view->table != 0 && view->field_translation != 0);

  VOID(bitmap_init(&used_fields, used_fields_buff, table->s->fields, 0));
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
                                             List<Item> &fields,
                                             bool select_insert)
{
  bool insert_into_view= (table_list->view != 0);
  DBUG_ENTER("mysql_prepare_insert_check_table");

  if (setup_tables_and_check_access(thd, &thd->lex->select_lex.context,
                                    &thd->lex->select_lex.top_join_list,
                                    table_list,
                                    &thd->lex->select_lex.leaf_tables,
                                    select_insert, INSERT_ACL))
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
  Name_resolution_context_state ctx_state;
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

  if (mysql_prepare_insert_check_table(thd, table_list, fields, select_insert))
    DBUG_RETURN(TRUE);

  /* Save the state of the current name resolution context. */
  ctx_state.save_state(context, table_list);

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
        setup_fields(thd, 0, *values, MARK_COLUMNS_READ, 0, 0)) &&
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
      context->table_list->next_local=       ctx_state.save_next_local;
      /* first_name_resolution_table was set by resolve_in_table_list_only() */
      context->first_name_resolution_table->
        next_name_resolution_table=          ctx_state.save_next_local;
    }
    if (!res)
      res= setup_fields(thd, 0, update_values, MARK_COLUMNS_READ, 0, 0);
  }

  /* Restore the current context. */
  ctx_state.restore_state(context, table_list);

  if (res)
    DBUG_RETURN(res);

  if (!table)
    table= table_list->table;

  if (!select_insert)
  {
    Item *fake_conds= 0;
    TABLE_LIST *duplicate;
    if ((duplicate= unique_table(thd, table_list, table_list->next_global)))
    {
      update_non_unique_table_error(table_list, "INSERT", duplicate);
      DBUG_RETURN(TRUE);
    }
    select_lex->fix_prepare_information(thd, &fake_conds);
    select_lex->first_execution= 0;
  }
  if (duplic == DUP_UPDATE || duplic == DUP_REPLACE)
    table->prepare_for_position();
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
  MY_BITMAP *save_read_set, *save_write_set;
  DBUG_ENTER("write_record");

  info->records++;
  save_read_set=  table->read_set;
  save_write_set= table->write_set;

  if (info->handle_duplicates == DUP_REPLACE ||
      info->handle_duplicates == DUP_UPDATE)
  {
    while ((error=table->file->ha_write_row(table->record[0])))
    {
      uint key_nr;
      bool is_duplicate_key_error;
      if (table->file->is_fatal_error(error, HA_CHECK_DUP))
	goto err;
      table->file->restore_auto_increment(); // it's too early here! BUG#20188
      is_duplicate_key_error= table->file->is_fatal_error(error, 0);
      if (!is_duplicate_key_error)
      {
        /*
          We come here when we had an ignorable error which is not a duplicate
          key error. In this we ignore error if ignore flag is set, otherwise
          report error as usual. We will not do any duplicate key processing.
        */
        if (info->ignore)
          goto ok_or_after_trg_err; /* Ignoring a not fatal error, return 0 */
        goto err;
      }
      if ((int) (key_nr = table->file->get_dup_key(error)) < 0)
      {
	error= HA_ERR_FOUND_DUPP_KEY;         /* Database can't find key */
	goto err;
      }
      /* Read all columns for the row we are going to replace */
      table->use_all_columns();
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
      if (table->file->ha_table_flags() & HA_DUPLICATE_POS)
      {
	if (table->file->rnd_pos(table->record[1],table->file->dup_ref))
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
        if ((error=table->file->ha_update_row(table->record[1],
                                              table->record[0])))
	{
          if (info->ignore &&
              !table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
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
          Another case when conversion should not be performed is when
          we have ON DELETE trigger on table so user may notice that
          we cheat here. Note that it is ok to do such conversion for
          tables which have ON UPDATE but have no ON DELETE triggers,
          we just should not expose this fact to users by invoking
          ON UPDATE triggers.
	*/
	if (last_uniq_key(table,key_nr) &&
	    !table->file->referenced_by_foreign_key() &&
            (table->timestamp_field_type == TIMESTAMP_NO_AUTO_SET ||
             table->timestamp_field_type == TIMESTAMP_AUTO_SET_ON_BOTH) &&
            (!table->triggers || !table->triggers->has_delete_triggers()))
        {
          if (thd->clear_next_insert_id)
          {
            /* Reset auto-increment cacheing if we do an update */
            thd->clear_next_insert_id= 0;
            thd->next_insert_id= 0;
          }
          if ((error=table->file->ha_update_row(table->record[1],
					        table->record[0])))
            goto err;
          info->deleted++;
          /*
            Since we pretend that we have done insert we should call
            its after triggers.
          */
          goto after_trg_n_copied_inc;
        }
        else
        {
          if (table->triggers &&
              table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                                TRG_ACTION_BEFORE, TRUE))
            goto before_trg_err;
          if ((error=table->file->ha_delete_row(table->record[1])))
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
    /*
      Restore column maps if they where replaced during an duplicate key
      problem.
    */
    if (table->read_set != save_read_set ||
        table->write_set != save_write_set)
      table->column_bitmaps_set(save_read_set, save_write_set);
  }
  else if ((error=table->file->ha_write_row(table->record[0])))
  {
    if (!info->ignore ||
        table->file->is_fatal_error(error, HA_CHECK_DUP))
      goto err;
    table->file->restore_auto_increment();
    goto ok_or_after_trg_err;
  }

after_trg_n_copied_inc:
  info->copied++;
  trg_error= (table->triggers &&
              table->triggers->process_triggers(thd, TRG_EVENT_INSERT,
                                                TRG_ACTION_AFTER, TRUE));

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
  table->column_bitmaps_set(save_read_set, save_write_set);
  DBUG_RETURN(1);
}


/******************************************************************************
  Check that all fields with arn't null_fields are used
******************************************************************************/

int check_that_all_fields_are_given_values(THD *thd, TABLE *entry,
                                           TABLE_LIST *table_list)
{
  int err= 0;
  MY_BITMAP *write_set= entry->write_set;

  for (Field **field=entry->field ; *field ; field++)
  {
    if (!bitmap_is_set(write_set, (*field)->field_index) &&
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
  char *record;
  enum_duplicates dup;
  time_t start_time;
  bool query_start_used,last_insert_id_used,insert_id_used, ignore, log_query;
  ulonglong last_insert_id;
  timestamp_auto_set_type timestamp_field_type;

  delayed_row(enum_duplicates dup_arg, bool ignore_arg, bool log_query_arg)
    :record(0), dup(dup_arg), ignore(ignore_arg), log_query(log_query_arg) {}
  ~delayed_row()
  {
    x_free(record);
  }
};


class delayed_insert :public ilink {
  uint locks_in_memory;
  char *query;
  ulong query_length;
  ulong query_allocated;
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
    :locks_in_memory(0), query(0), query_length(0), query_allocated(0),
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
    my_free(query, MYF(MY_WME|MY_ALLOW_ZERO_PTR));
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

  int set_query(char const *q, ulong qlen) {
    if (q && qlen > 0)
    {
      if (query_allocated < qlen + 1)
      {
        ulong const flags(MY_WME|MY_FREE_ON_ERROR|MY_ALLOW_ZERO_PTR);
        query= my_realloc(query, qlen + 1, MYF(flags));
        if (query == 0)
          return HA_ERR_OUT_OF_MEM;
        query_allocated= qlen;
      }
      query_length= qlen;
      memcpy(query, q, qlen + 1);
    }
    else
      query_length= 0;
    return 0;
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
    if (!strcmp(tmp->thd.db, table_list->db) &&
	!strcmp(table_list->table_name, tmp->table->s->table_name.str))
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

  /* Must be set in the parser */
  DBUG_ASSERT(table_list->db);

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
      if (!(tmp=new delayed_insert()))
      {
	my_error(ER_OUTOFMEMORY,MYF(0),sizeof(delayed_insert));
	goto err1;
      }
      pthread_mutex_lock(&LOCK_thread_count);
      thread_count++;
      pthread_mutex_unlock(&LOCK_thread_count);
      tmp->thd.set_db(table_list->db, strlen(table_list->db));
      tmp->thd.query= my_strdup(table_list->table_name,MYF(MY_WME));
      if (tmp->thd.db == NULL || tmp->thd.query == NULL)
      {
	delete tmp;
	my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
	goto err1;
      }
      tmp->table_list= *table_list;			// Needed to open table
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
  TABLE_SHARE *share= table->s;
  byte *bitmap;
  DBUG_ENTER("delayed_insert::get_local_table");

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

  /*
    Allocate memory for the TABLE object, the field pointers array, and
    one record buffer of reclength size. Normally a table has three
    record buffers of rec_buff_length size, which includes alignment
    bytes. Since the table copy is used for creating one record only,
    the other record buffers and alignment are unnecessary.
  */
  client_thd->proc_info="allocating local table";
  copy= (TABLE*) client_thd->alloc(sizeof(*copy)+
				   (share->fields+1)*sizeof(Field**)+
				   share->reclength +
                                   share->column_bitmap_size*2);
  if (!copy)
    goto error;

  /* Copy the TABLE object. */
  *copy= *table;
  /* We don't need to change the file handler here */
  /* Assign the pointers for the field pointers array and the record. */
  field= copy->field= (Field**) (copy + 1);
  bitmap= (byte*) (field + share->fields + 1);
  copy->record[0]= (bitmap + share->column_bitmap_size * 2);
  memcpy((char*) copy->record[0], (char*) table->record[0], share->reclength);
  /*
    Make a copy of all fields.
    The copied fields need to point into the copied record. This is done
    by copying the field objects with their old pointer values and then
    "move" the pointers by the distance between the original and copied
    records. That way we preserve the relative positions in the records.
  */
  adjust_ptrs= PTR_BYTE_DIFF(copy->record[0], table->record[0]);
  found_next_number_field= table->found_next_number_field;
  for (org_field= table->field; *org_field; org_field++, field++)
  {
    if (!(*field= (*org_field)->new_field(client_thd->mem_root, copy, 1)))
      DBUG_RETURN(0);
    (*field)->orig_table= copy;			// Remove connection
    (*field)->move_field_offset(adjust_ptrs);	// Point at copy->record[0]
    if (*org_field == found_next_number_field)
      (*field)->table->found_next_number_field= *field;
  }
  *field=0;

  /* Adjust timestamp */
  if (table->timestamp_field)
  {
    /* Restore offset as this may have been reset in handle_inserts */
    copy->timestamp_field=
      (Field_timestamp*) copy->field[share->timestamp_field_offset];
    copy->timestamp_field->unireg_check= table->timestamp_field->unireg_check;
    copy->timestamp_field_type= copy->timestamp_field->get_auto_set_type();
  }

  /* Adjust in_use for pointing to client thread */
  copy->in_use= client_thd;

  /* Adjust lock_count. This table object is not part of a lock. */
  copy->lock_count= 0;

  /* Adjust bitmaps */
  copy->def_read_set.bitmap= (my_bitmap_map*) bitmap;
  copy->def_write_set.bitmap= ((my_bitmap_map*)
                               (bitmap + share->column_bitmap_size));
  copy->tmp_set.bitmap= 0;                      // To catch errors
  bzero((char*) bitmap, share->column_bitmap_size*2);
  copy->read_set=  &copy->def_read_set;
  copy->write_set= &copy->def_write_set;

  DBUG_RETURN(copy);

  /* Got fatal error */
 error:
  tables_in_use--;
  status=1;
  pthread_cond_signal(&cond);			// Inform thread about abort
  DBUG_RETURN(0);
}


/* Put a question in queue */

static int write_delayed(THD *thd,TABLE *table,enum_duplicates duplic,
                         bool ignore, char *query, uint query_length,
                         bool log_on)
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

  if (!(row->record= (char*) my_malloc(table->s->reclength, MYF(MY_WME))))
    goto err;
  memcpy(row->record, table->record[0], table->s->reclength);
  di->set_query(query, query_length);
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
#if !defined( __WIN__) /* Win32 calls this in pthread_create */
  if (my_thread_init())
  {
    strmov(thd->net.last_error,ER(thd->net.last_errno=ER_OUT_OF_RESOURCES));
    goto end;
  }
#endif

  DBUG_ENTER("handle_delayed_insert");
  thd->thread_stack= (char*) &thd;
  if (init_thr_lock() || thd->store_globals())
  {
    thd->fatal_error();
    strmov(thd->net.last_error,ER(thd->net.last_errno=ER_OUT_OF_RESOURCES));
    goto err;
  }
#if !defined(__WIN__) && !defined(__NETWARE__)
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif

  /* open table */

  if (!(di->table=open_ltable(thd,&di->table_list,TL_WRITE_DELAYED)))
  {
    thd->fatal_error();				// Abort waiting inserts
    goto err;
  }
  if (!(di->table->file->ha_table_flags() & HA_CAN_INSERT_DELAYED))
  {
    thd->fatal_error();
    my_error(ER_ILLEGAL_HA, MYF(0), di->table_list.table_name);
    goto err;
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
	if (error == ETIMEDOUT || error == ETIME)
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

err:
  /*
    mysql_lock_tables() can potentially start a transaction and write
    a table map. In the event of an error, that transaction has to be
    rolled back.  We only need to roll back a potential statement
    transaction, since real transactions are rolled back in
    close_thread_tables().
   */
  ha_rollback_stmt(thd);

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
  bool using_ignore=0,
    using_bin_log= mysql_bin_log.is_open();

  delayed_row *row;
  DBUG_ENTER("handle_inserts");

  /* Allow client to insert new rows */
  pthread_mutex_unlock(&mutex);

  table->next_number_field=table->found_next_number_field;
  table->use_all_columns();

  thd.proc_info="upgrading lock";
  if (thr_upgrade_write_delay_lock(*thd.lock->locks))
  {
    /* This can only happen if thread is killed by shutdown */
    sql_print_error(ER(ER_DELAYED_CANT_CHANGE_LOCK),table->s->table_name.str);
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

  /* Reset auto-increment cacheing */
  if (thd.clear_next_insert_id)
  {
    thd.next_insert_id= 0;
    thd.clear_next_insert_id= 0;
  }

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
	(!(row->log_query & using_bin_log)))
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
	  sql_print_error(ER(ER_DELAYED_CANT_CHANGE_LOCK),
                          table->s->table_name.str);
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
  pthread_mutex_unlock(&mutex);

  /* After releasing the mutex, to prevent deadlocks. */
  if (mysql_bin_log.is_open())
    thd.binlog_query(THD::ROW_QUERY_TYPE, query, query_length, FALSE, FALSE);

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
       setup_fields(thd, 0, values, MARK_COLUMNS_READ, 0, 0);

  if (info.handle_duplicates == DUP_UPDATE)
  {
    /* Save the state of the current name resolution context. */
    Name_resolution_context *context= &lex->select_lex.context;
    Name_resolution_context_state ctx_state;

    /* Save the state of the current name resolution context. */
    ctx_state.save_state(context, table_list);

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
      context->table_list->next_local=       ctx_state.save_next_local;
      /* first_name_resolution_table was set by resolve_in_table_list_only() */
      context->first_name_resolution_table->
        next_name_resolution_table=          ctx_state.save_next_local;
    }
    res= res || setup_fields(thd, 0, *info.update_values, MARK_COLUMNS_READ,
                             0, 0);

    /* Restore the current context. */
    ctx_state.restore_state(context, table_list);
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
      unique_table(thd, table_list, table_list->next_global))
  {
    /* Using same table for INSERT and SELECT */
    lex->current_select->options|= OPTION_BUFFER_RESULT;
    lex->current_select->join->select_options|= OPTION_BUFFER_RESULT;
  }
  else if (!thd->prelocked_mode)
  {
    /*
      We must not yet prepare the result table if it is the same as one of the 
      source tables (INSERT SELECT). The preparation may disable 
      indexes on the result table, which may be used during the select, if it
      is the same table (Bug #6034). Do the preparation after the select phase
      in select_insert::prepare2().
      We won't start bulk inserts at all if this statement uses functions or
      should invoke triggers since they may access to the same table too.
    */
    table->file->ha_start_bulk_insert((ha_rows) 0);
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
  if (thd->lex->current_select->options & OPTION_BUFFER_RESULT &&
      !thd->prelocked_mode)
    table->file->ha_start_bulk_insert((ha_rows) 0);
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

  error= write_record(thd, table, &info);
    
  if (!error)
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
  table->file->release_auto_increment();
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

  /* Avoid an extra 'unknown error' message if we already reported an error */
  if (errcode != ER_UNKNOWN_ERROR && !thd->net.report_error)
    my_message(errcode, err, MYF(0));

  if (!table)
  {
    /*
      This can only happen when using CREATE ... SELECT and the table was not
      created becasue of an syntax error
    */
    DBUG_VOID_RETURN;
  }
  if (!thd->prelocked_mode)
    table->file->ha_end_bulk_insert();
  /*
    If at least one row has been inserted/modified and will stay in the table
    (the table doesn't have transactions) we must write to the binlog (and
    the error code will make the slave stop).

    For many errors (example: we got a duplicate key error while
    inserting into a MyISAM table), no row will be added to the table,
    so passing the error to the slave will not help since there will
    be an error code mismatch (the inserts will succeed on the slave
    with no error).

    If we are using row-based replication we have two cases where this
    code is executed: replication of CREATE-SELECT and replication of
    INSERT-SELECT.

    When replicating a CREATE-SELECT statement, we shall not write the
    events to the binary log and should thus not set
    OPTION_STATUS_NO_TRANS_UPDATE.

    When replicating INSERT-SELECT, we shall not write the events to
    the binary log for transactional table, but shall write all events
    if there is one or more writes to non-transactional tables. In
    this case, the OPTION_STATUS_NO_TRANS_UPDATE is set if there is a
    write to a non-transactional table, otherwise it is cleared.
  */
  if (info.copied || info.deleted || info.updated)
  {
    if (!table->file->has_transactions())
    {
      if (last_insert_id)
        thd->insert_id(last_insert_id);		// For binary log
      if (mysql_bin_log.is_open())
      {
        thd->binlog_query(THD::ROW_QUERY_TYPE, thd->query, thd->query_length,
                          table->file->has_transactions(), FALSE);
      }
      if (!thd->current_stmt_binlog_row_based && !table->s->tmp_table &&
          !can_rollback_data())
        thd->options|= OPTION_STATUS_NO_TRANS_UPDATE;
      query_cache_invalidate3(thd, table, 1);
    }
  }
  ha_rollback_stmt(thd);
  DBUG_VOID_RETURN;
}


bool select_insert::send_eof()
{
  int error,error2;
  DBUG_ENTER("select_insert::send_eof");

  error= (!thd->prelocked_mode) ? table->file->ha_end_bulk_insert():0;
  table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

  if (info.copied || info.deleted || info.updated)
  {
    /*
      We must invalidate the table in the query cache before binlog writing
      and ha_autocommit_or_rollback.
    */
    query_cache_invalidate3(thd, table, 1);
    /*
      Mark that we have done permanent changes if all of the below is true
      - Table doesn't support transactions
      - It's a normal (not temporary) table. (Changes to temporary tables
        are not logged in RBR)
      - We are using statement based replication
    */
    if (!table->file->has_transactions() &&
        (!table->s->tmp_table ||
         !thd->current_stmt_binlog_row_based))
      thd->options|= OPTION_STATUS_NO_TRANS_UPDATE;
   }

  if (last_insert_id)
    thd->insert_id(last_insert_id);		// For binary log
  /*
    Write to binlog before commiting transaction.  No statement will
    be written by the binlog_query() below in RBR mode.  All the
    events are in the transaction cache and will be written when
    ha_autocommit_or_rollback() is issued below.
  */
  if (mysql_bin_log.is_open())
  {
    if (!error)
      thd->clear_error();
    thd->binlog_query(THD::ROW_QUERY_TYPE,
                      thd->query, thd->query_length,
                      table->file->has_transactions(), FALSE);
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

/*
  Create table from lists of fields and items (or open existing table
  with same name).

  SYNOPSIS
    create_table_from_items()
      thd          in     Thread object
      create_info  in     Create information (like MAX_ROWS, ENGINE or
                          temporary table flag)
      create_table in     Pointer to TABLE_LIST object providing database
                          and name for table to be created or to be open
      extra_fields in/out Initial list of fields for table to be created
      keys         in     List of keys for table to be created
      items        in     List of items which should be used to produce rest
                          of fields for the table (corresponding fields will
                          be added to the end of 'extra_fields' list)
      lock         out    Pointer to the MYSQL_LOCK object for table created
                          (open) will be returned in this parameter. Since
                          this table is not included in THD::lock caller is
                          responsible for explicitly unlocking this table.
      hooks

  NOTES
    If 'create_info->options' bitmask has HA_LEX_CREATE_IF_NOT_EXISTS
    flag and table with name provided already exists then this function will
    simply open existing table.
    Also note that create, open and lock sequence in this function is not
    atomic and thus contains gap for deadlock and can cause other troubles.
    Since this function contains some logic specific to CREATE TABLE ... SELECT
    it should be changed before it can be used in other contexts.

  RETURN VALUES
    non-zero  Pointer to TABLE object for table created or opened
    0         Error
*/

static TABLE *create_table_from_items(THD *thd, HA_CREATE_INFO *create_info,
                                      TABLE_LIST *create_table,
                                      List<create_field> *extra_fields,
                                      List<Key> *keys,
                                      List<Item> *items,
                                      MYSQL_LOCK **lock,
                                      TABLEOP_HOOKS *hooks)
{
  TABLE tmp_table;		// Used during 'create_field()'
  TABLE_SHARE share;
  TABLE *table= 0;
  uint select_field_count= items->elements;
  /* Add selected items to field list */
  List_iterator_fast<Item> it(*items);
  Item *item;
  Field *tmp_field;
  bool not_used;
  DBUG_ENTER("create_table_from_items");

  tmp_table.alias= 0;
  tmp_table.timestamp_field= 0;
  tmp_table.s= &share;
  init_tmp_table_share(&share, "", 0, "", "");

  tmp_table.s->db_create_options=0;
  tmp_table.s->blob_ptr_size= portable_sizeof_char_ptr;
  tmp_table.s->db_low_byte_first= 
        test(create_info->db_type == &myisam_hton ||
             create_info->db_type == &heap_hton);
  tmp_table.null_row=tmp_table.maybe_null=0;

  while ((item=it++))
  {
    create_field *cr_field;
    Field *field, *def_field;
    if (item->type() == Item::FUNC_ITEM)
      field= item->tmp_table_field(&tmp_table);
    else
      field= create_tmp_field(thd, &tmp_table, item, item->type(),
                              (Item ***) 0, &tmp_field, &def_field, 0, 0, 0, 0,
                              0);
    if (!field ||
	!(cr_field=new create_field(field,(item->type() == Item::FIELD_ITEM ?
					   ((Item_field *)item)->field :
					   (Field*) 0))))
      DBUG_RETURN(0);
    if (item->maybe_null)
      cr_field->flags &= ~NOT_NULL_FLAG;
    extra_fields->push_back(cr_field);
  }
  /*
    create and lock table

    We don't log the statement, it will be logged later.

    If this is a HEAP table, the automatic DELETE FROM which is written to the
    binlog when a HEAP table is opened for the first time since startup, must
    not be written: 1) it would be wrong (imagine we're in CREATE SELECT: we
    don't want to delete from it) 2) it would be written before the CREATE
    TABLE, which is a wrong order. So we keep binary logging disabled when we
    open_table().
    NOTE: By locking table which we just have created (or for which we just
    have have found that it already exists) separately from other tables used
    by the statement we create potential window for deadlock.
    TODO: create and open should be done atomic !
  */
  {
    tmp_disable_binlog(thd);
    if (!mysql_create_table(thd, create_table->db, create_table->table_name,
                            create_info, *extra_fields, *keys, 0,
                            select_field_count, 0))
    {
      /*
        If we are here in prelocked mode we either create temporary table
        or prelocked mode is caused by the SELECT part of this statement.
      */
      DBUG_ASSERT(!thd->prelocked_mode ||
                  create_info->options & HA_LEX_CREATE_TMP_TABLE ||
                  thd->lex->requires_prelocking());

      /*
        NOTE: We don't want to ignore set of locked tables here if we are
              under explicit LOCK TABLES since it will open gap for deadlock
              too wide (and also is not backward compatible).
      */

      if (! (table= open_table(thd, create_table, thd->mem_root, (bool*) 0,
                               (MYSQL_LOCK_IGNORE_FLUSH |
                                ((thd->prelocked_mode == PRELOCKED) ?
                                 MYSQL_OPEN_IGNORE_LOCKED_TABLES:0)))))
        quick_rm_table(create_info->db_type, create_table->db,
                       table_case_name(create_info, create_table->table_name),
                       0);
    }
    reenable_binlog(thd);
    if (!table)                                   // open failed
      DBUG_RETURN(0);
  }

  /*
    FIXME: What happens if trigger manages to be created while we are
           obtaining this lock ? May be it is sensible just to disable
           trigger execution in this case ? Or will MYSQL_LOCK_IGNORE_FLUSH
           save us from that ?
  */
  table->reginfo.lock_type=TL_WRITE;
  hooks->prelock(&table, 1);                    // Call prelock hooks
  if (! ((*lock)= mysql_lock_tables(thd, &table, 1,
                                    MYSQL_LOCK_IGNORE_FLUSH, &not_used)))
  {
    VOID(pthread_mutex_lock(&LOCK_open));
    hash_delete(&open_cache,(byte*) table);
    VOID(pthread_mutex_unlock(&LOCK_open));
    quick_rm_table(create_info->db_type, create_table->db,
		   table_case_name(create_info, create_table->table_name), 0);
    DBUG_RETURN(0);
  }
  table->file->extra(HA_EXTRA_WRITE_CACHE);
  DBUG_RETURN(table);
}


class MY_HOOKS : public TABLEOP_HOOKS
{
public:
  MY_HOOKS(select_create *x) : ptr(x) { }
  virtual void do_prelock(TABLE **tables, uint count)
  {
    if (ptr->get_thd()->current_stmt_binlog_row_based)
      ptr->binlog_show_create_table(tables, count);
  }

private:
  select_create *ptr;
};


int
select_create::prepare(List<Item> &values, SELECT_LEX_UNIT *u)
{
  DBUG_ENTER("select_create::prepare");

  TABLEOP_HOOKS *hook_ptr= NULL;
#ifdef HAVE_ROW_BASED_REPLICATION
  class MY_HOOKS : public TABLEOP_HOOKS {
  public:
    MY_HOOKS(select_create *x) : ptr(x) { }
    virtual void do_prelock(TABLE **tables, uint count)
    {
      if (ptr->get_thd()->current_stmt_binlog_row_based)
        ptr->binlog_show_create_table(tables, count);
    }

  private:
    select_create *ptr;
  };

  MY_HOOKS hooks(this);
  hook_ptr= &hooks;
#endif

  unit= u;
  if (!(table= create_table_from_items(thd, create_info, create_table,
                                       extra_fields, keys, &values,
                                       &thd->extra_lock, hook_ptr)))
    DBUG_RETURN(-1);				// abort() deletes table

  if (table->s->fields < values.elements)
  {
    my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), 1);
    DBUG_RETURN(-1);
  }

 /* First field to copy */
  field= table->field+table->s->fields - values.elements;

  /* Mark all fields that are given values */
  for (Field **f= field ; *f ; f++)
    bitmap_set_bit(table->write_set, (*f)->field_index);

  /* Don't set timestamp if used */
  table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
  table->next_number_field=table->found_next_number_field;

  restore_record(table,s->default_values);      // Get empty record
  thd->cuted_fields=0;
  if (info.ignore || info.handle_duplicates != DUP_ERROR)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  if (!thd->prelocked_mode)
    table->file->ha_start_bulk_insert((ha_rows) 0);
  thd->no_trans_update= 0;
  thd->abort_on_warning= (!info.ignore &&
                          (thd->variables.sql_mode &
                           (MODE_STRICT_TRANS_TABLES |
                            MODE_STRICT_ALL_TABLES)));
  DBUG_RETURN(check_that_all_fields_are_given_values(thd, table,
                                                     table_list));
}


#ifdef HAVE_ROW_BASED_REPLICATION
void
select_create::binlog_show_create_table(TABLE **tables, uint count)
{
  /*
    Note 1: In RBR mode, we generate a CREATE TABLE statement for the
    created table by calling store_create_info() (behaves as SHOW
    CREATE TABLE).  In the event of an error, nothing should be
    written to the binary log, even if the table is non-transactional;
    therefore we pretend that the generated CREATE TABLE statement is
    for a transactional table.  The event will then be put in the
    transaction cache, and any subsequent events (e.g., table-map
    events and binrow events) will also be put there.  We can then use
    ha_autocommit_or_rollback() to either throw away the entire
    kaboodle of events, or write them to the binary log.

    We write the CREATE TABLE statement here and not in prepare()
    since there potentially are sub-selects or accesses to information
    schema that will do a close_thread_tables(), destroying the
    statement transaction cache.
  */
  DBUG_ASSERT(thd->current_stmt_binlog_row_based);
  DBUG_ASSERT(tables && *tables && count > 0);

  char buf[2048];
  String query(buf, sizeof(buf), system_charset_info);
  int result;
  TABLE_LIST table_list;

  memset(&table_list, 0, sizeof(table_list));
  table_list.table = *tables;
  query.length(0);      // Have to zero it since constructor doesn't

  result= store_create_info(thd, &table_list, &query, create_info);
  DBUG_ASSERT(result == 0); /* store_create_info() always return 0 */

  thd->binlog_query(THD::STMT_QUERY_TYPE,
                    query.ptr(), query.length(),
                    /* is_trans */ TRUE,
                    /* suppress_use */ FALSE);
}
#endif // HAVE_ROW_BASED_REPLICATION

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
    mysql_unlock_tables(thd, thd->extra_lock);
    if (!table->s->tmp_table)
    {
      if (close_thread_table(thd, &table))
        broadcast_refresh();
    }
    thd->extra_lock=0;
    table=0;
    VOID(pthread_mutex_unlock(&LOCK_open));
  }
  return tmp;
}

void select_create::abort()
{
  VOID(pthread_mutex_lock(&LOCK_open));
  if (thd->extra_lock)
  {
    mysql_unlock_tables(thd, thd->extra_lock);
    thd->extra_lock=0;
  }
  if (table)
  {
    table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    handlerton *table_type=table->s->db_type;
    if (!table->s->tmp_table)
    {
      ulong version= table->s->version;
      table->s->version= 0;
      hash_delete(&open_cache,(byte*) table);
      if (!create_info->table_existed)
        quick_rm_table(table_type, create_table->db,
                       create_table->table_name, 0);
      /* Tell threads waiting for refresh that something has happened */
      if (version != refresh_version)
        broadcast_refresh();
    }
    else if (!create_info->table_existed)
      close_temporary_table(thd, table, 1, 1);
    table=0;                                    // Safety
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
