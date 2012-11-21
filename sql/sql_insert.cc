/*
   Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

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
  Delayed_insert::get_local_table() the table of the thread is copied
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
#include "slave.h"
#include "rpl_mi.h"

#include "debug_sync.h"

#ifndef EMBEDDED_LIBRARY
static bool delayed_get_table(THD *thd, TABLE_LIST *table_list);
static int write_delayed(THD *thd, TABLE *table, enum_duplicates duplic,
                         LEX_STRING query, bool ignore, bool log_on);
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
  Check that insert/update fields are from the same single table of a view.

  SYNOPSIS
    check_view_single_update()
    fields            The insert/update fields to be checked.
    view              The view for insert.
    map     [in/out]  The insert table map.

  DESCRIPTION
    This function is called in 2 cases:
    1. to check insert fields. In this case *map will be set to 0.
       Insert fields are checked to be all from the same single underlying
       table of the given view. Otherwise the error is thrown. Found table
       map is returned in the map parameter.
    2. to check update fields of the ON DUPLICATE KEY UPDATE clause.
       In this case *map contains table_map found on the previous call of
       the function to check insert fields. Update fields are checked to be
       from the same table as the insert fields.

  RETURN
    0   OK
    1   Error
*/

bool check_view_single_update(List<Item> &fields, TABLE_LIST *view,
                              table_map *map)
{
  /* it is join view => we need to find the table for update */
  List_iterator_fast<Item> it(fields);
  Item *item;
  TABLE_LIST *tbl= 0;            // reset for call to check_single_table()
  table_map tables= 0;

  while ((item= it++))
    tables|= item->used_tables();

  /* Check found map against provided map */
  if (*map)
  {
    if (tables != *map)
      goto error;
    return FALSE;
  }

  if (view->check_single_table(&tbl, tables, view) || tbl == 0)
    goto error;

  view->table= tbl->table;
  *map= tables;

  return FALSE;

error:
  my_error(ER_VIEW_MULTIUPDATE, MYF(0),
           view->view_db.str, view->view_name.str);
  return TRUE;
}


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
                               bool check_unique, table_map *map)
{
  TABLE *table= table_list->table;

  if (!table_list->updatable)
  {
    my_error(ER_NON_INSERTABLE_TABLE, MYF(0), table_list->alias, "INSERT");
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
    Field_iterator_table_ref field_it;
    field_it.set(table_list);
    if (check_grant_all_columns(thd, INSERT_ACL, &field_it))
      return -1;
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
      if (check_view_single_update(fields, table_list, map))
        return -1;
      table= table_list->table;
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
  // For the values we need select_priv
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  table->grant.want_privilege= (SELECT_ACL & ~table->grant.privilege);
#endif

  if (check_key_in_view(thd, table_list) ||
      (table_list->view &&
       check_view_insertability(thd, table_list)))
  {
    my_error(ER_NON_INSERTABLE_TABLE, MYF(0), table_list->alias, "INSERT");
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
                               List<Item> &update_fields, table_map *map)
{
  TABLE *table= insert_table_list->table;
  my_bool timestamp_mark= 0;

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

  if (insert_table_list->effective_algorithm == VIEW_ALGORITHM_MERGE &&
      check_view_single_update(update_fields, insert_table_list, map))
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

/*
  Prepare triggers  for INSERT-like statement.

  SYNOPSIS
    prepare_triggers_for_insert_stmt()
      table   Table to which insert will happen

  NOTE
    Prepare triggers for INSERT-like statement by marking fields
    used by triggers and inform handlers that batching of UPDATE/DELETE 
    cannot be done if there are BEFORE UPDATE/DELETE triggers.
*/

void prepare_triggers_for_insert_stmt(TABLE *table)
{
  if (table->triggers)
  {
    if (table->triggers->has_triggers(TRG_EVENT_DELETE,
                                      TRG_ACTION_AFTER))
    {
      /*
        The table has AFTER DELETE triggers that might access to 
        subject table and therefore might need delete to be done 
        immediately. So we turn-off the batching.
      */ 
      (void) table->file->extra(HA_EXTRA_DELETE_CANNOT_BATCH);
    }
    if (table->triggers->has_triggers(TRG_EVENT_UPDATE,
                                      TRG_ACTION_AFTER))
    {
      /*
        The table has AFTER UPDATE triggers that might access to subject 
        table and therefore might need update to be done immediately. 
        So we turn-off the batching.
      */ 
      (void) table->file->extra(HA_EXTRA_UPDATE_CANNOT_BATCH);
    }
  }
  table->mark_columns_needed_for_insert();
}


/**
  Upgrade table-level lock of INSERT statement to TL_WRITE if
  a more concurrent lock is infeasible for some reason. This is
  necessary for engines without internal locking support (MyISAM).
  An engine with internal locking implementation might later
  downgrade the lock in handler::store_lock() method.
*/

static
void upgrade_lock_type(THD *thd, thr_lock_type *lock_type,
                       enum_duplicates duplic,
                       bool is_multi_insert)
{
  if (duplic == DUP_UPDATE ||
      (duplic == DUP_REPLACE && *lock_type == TL_WRITE_CONCURRENT_INSERT))
  {
    *lock_type= TL_WRITE_DEFAULT;
    return;
  }

  if (*lock_type == TL_WRITE_DELAYED)
  {
    /*
      We do not use delayed threads if:
      - we're running in the safe mode or skip-new mode -- the
        feature is disabled in these modes
      - we're executing this statement on a replication slave --
        we need to ensure serial execution of queries on the
        slave
      - it is INSERT .. ON DUPLICATE KEY UPDATE - in this case the
        insert cannot be concurrent
      - this statement is directly or indirectly invoked from
        a stored function or trigger (under pre-locking) - to
        avoid deadlocks, since INSERT DELAYED involves a lock
        upgrade (TL_WRITE_DELAYED -> TL_WRITE) which we should not
        attempt while keeping other table level locks.
      - this statement itself may require pre-locking.
        We should upgrade the lock even though in most cases
        delayed functionality may work. Unfortunately, we can't
        easily identify whether the subject table is not used in
        the statement indirectly via a stored function or trigger:
        if it is used, that will lead to a deadlock between the
        client connection and the delayed thread.
    */
    if (specialflag & (SPECIAL_NO_NEW_FUNC | SPECIAL_SAFE_MODE) ||
        thd->variables.max_insert_delayed_threads == 0 ||
        thd->prelocked_mode ||
        thd->lex->uses_stored_routines())
    {
      *lock_type= TL_WRITE;
      return;
    }
    if (thd->slave_thread)
    {
      /* Try concurrent insert */
      *lock_type= (duplic == DUP_UPDATE || duplic == DUP_REPLACE) ?
                  TL_WRITE : TL_WRITE_CONCURRENT_INSERT;
      return;
    }

    bool log_on= (thd->options & OPTION_BIN_LOG ||
                  ! (thd->security_ctx->master_access & SUPER_ACL));
    if (global_system_variables.binlog_format == BINLOG_FORMAT_STMT &&
        log_on && mysql_bin_log.is_open() && is_multi_insert)
    {
      /*
        Statement-based binary logging does not work in this case, because:
        a) two concurrent statements may have their rows intermixed in the
        queue, leading to autoincrement replication problems on slave (because
        the values generated used for one statement don't depend only on the
        value generated for the first row of this statement, so are not
        replicable)
        b) if first row of the statement has an error the full statement is
        not binlogged, while next rows of the statement may be inserted.
        c) if first row succeeds, statement is binlogged immediately with a
        zero error code (i.e. "no error"), if then second row fails, query
        will fail on slave too and slave will stop (wrongly believing that the
        master got no error).
        So we fallback to non-delayed INSERT.
        Note that to be fully correct, we should test the "binlog format which
        the delayed thread is going to use for this row". But in the common case
        where the global binlog format is not changed and the session binlog
        format may be changed, that is equal to the global binlog format.
        We test it without mutex for speed reasons (condition rarely true), and
        in the common case (global not changed) it is as good as without mutex;
        if global value is changed, anyway there is uncertainty as the delayed
        thread may be old and use the before-the-change value.
      */
      *lock_type= TL_WRITE;
    }
  }
}


/**
  Find or create a delayed insert thread for the first table in
  the table list, then open and lock the remaining tables.
  If a table can not be used with insert delayed, upgrade the lock
  and open and lock all tables using the standard mechanism.

  @param thd         thread context
  @param table_list  list of "descriptors" for tables referenced
                     directly in statement SQL text.
                     The first element in the list corresponds to
                     the destination table for inserts, remaining
                     tables, if any, are usually tables referenced
                     by sub-queries in the right part of the
                     INSERT.

  @return Status of the operation. In case of success 'table'
  member of every table_list element points to an instance of
  class TABLE.

  @sa open_and_lock_tables for more information about MySQL table
  level locking
*/

static
bool open_and_lock_for_insert_delayed(THD *thd, TABLE_LIST *table_list)
{
  DBUG_ENTER("open_and_lock_for_insert_delayed");

#ifndef EMBEDDED_LIBRARY
  if (thd->locked_tables && thd->global_read_lock)
  {
    /*
      If this connection has the global read lock, the handler thread
      will not be able to lock the table. It will wait for the global
      read lock to go away, but this will never happen since the
      connection thread will be stuck waiting for the handler thread
      to open and lock the table.
      If we are not in locked tables mode, INSERT will seek protection
      against the global read lock (and fail), thus we will only get
      to this point in locked tables mode.
    */
    my_error(ER_CANT_UPDATE_WITH_READLOCK, MYF(0));
    DBUG_RETURN(TRUE);
  }

  if (delayed_get_table(thd, table_list))
    DBUG_RETURN(TRUE);

  if (table_list->table)
  {
    /*
      Open tables used for sub-selects or in stored functions, will also
      cache these functions.
    */
    if (open_and_lock_tables(thd, table_list->next_global))
    {
      end_delayed_insert(thd);
      DBUG_RETURN(TRUE);
    }
    /*
      First table was not processed by open_and_lock_tables(),
      we need to set updatability flag "by hand".
    */
    if (!table_list->derived && !table_list->view)
      table_list->updatable= 1;  // usual table
    DBUG_RETURN(FALSE);
  }
#endif
  /*
    * This is embedded library and we don't have auxiliary
    threads OR
    * a lock upgrade was requested inside delayed_get_table
      because
      - there are too many delayed insert threads OR
      - the table has triggers.
    Use a normal insert.
  */
  table_list->lock_type= TL_WRITE;
  DBUG_RETURN(open_and_lock_tables(thd, table_list));
}


/**
  INSERT statement implementation

  @note Like implementations of other DDL/DML in MySQL, this function
  relies on the caller to close the thread tables. This is done in the
  end of dispatch_command().
*/

bool mysql_insert(THD *thd,TABLE_LIST *table_list,
                  List<Item> &fields,
                  List<List_item> &values_list,
                  List<Item> &update_fields,
                  List<Item> &update_values,
                  enum_duplicates duplic,
		  bool ignore)
{
  int error, res;
  bool transactional_table, joins_freed= FALSE;
  bool changed;
  bool was_insert_delayed= (table_list->lock_type ==  TL_WRITE_DELAYED);
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
  char *query= thd->query();
  /*
    log_on is about delayed inserts only.
    By default, both logs are enabled (this won't cause problems if the server
    runs without --log-update or --log-bin).
  */
  bool log_on= ((thd->options & OPTION_BIN_LOG) ||
                (!(thd->security_ctx->master_access & SUPER_ACL)));
#endif
  thr_lock_type lock_type;
  Item *unused_conds= 0;
  DBUG_ENTER("mysql_insert");

  /*
    Upgrade lock type if the requested lock is incompatible with
    the current connection mode or table operation.
  */
  upgrade_lock_type(thd, &table_list->lock_type, duplic,
                    values_list.elements > 1);

  /*
    We can't write-delayed into a table locked with LOCK TABLES:
    this will lead to a deadlock, since the delayed thread will
    never be able to get a lock on the table. QQQ: why not
    upgrade the lock here instead?
  */
  if (table_list->lock_type == TL_WRITE_DELAYED && thd->locked_tables &&
      find_locked_table(thd, table_list->db, table_list->table_name))
  {
    my_error(ER_DELAYED_INSERT_TABLE_LOCKED, MYF(0),
             table_list->table_name);
    DBUG_RETURN(TRUE);
  }

  if (table_list->lock_type == TL_WRITE_DELAYED)
  {
    if (open_and_lock_for_insert_delayed(thd, table_list))
      DBUG_RETURN(TRUE);
  }
  else
  {
    if (open_and_lock_tables(thd, table_list))
      DBUG_RETURN(TRUE);
  }
  lock_type= table_list->lock_type;

  thd_proc_info(thd, "init");
  thd->lex->used_tables=0;
  values= its++;
  value_count= values->elements;

  if (mysql_prepare_insert(thd, table_list, table, fields, values,
			   update_fields, update_values, duplic, &unused_conds,
                           FALSE,
                           (fields.elements || !value_count ||
                            table_list->view != 0),
                           !ignore && (thd->variables.sql_mode &
                                       (MODE_STRICT_TRANS_TABLES |
                                        MODE_STRICT_ALL_TABLES))))
    goto abort;

  /* mysql_prepare_insert set table_list->table if it was not set */
  table= table_list->table;

  context= &thd->lex->select_lex.context;
  /*
    These three asserts test the hypothesis that the resetting of the name
    resolution context below is not necessary at all since the list of local
    tables for INSERT always consists of one table.
  */
  DBUG_ASSERT(!table_list->next_local);
  DBUG_ASSERT(!context->table_list->next_local);
  DBUG_ASSERT(!context->first_name_resolution_table->next_name_resolution_table);

  /* Save the state of the current name resolution context. */
  ctx_state.save_state(context, table_list);

  /*
    Perform name resolution only in the first table - 'table_list',
    which is the table that is inserted into.
  */
  table_list->next_local= 0;
  context->resolve_in_table_list_only(table_list);

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
  bzero((char*) &info,sizeof(info));
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

#ifdef HAVE_REPLICATION
  if (thd->slave_thread &&
      (info.handle_duplicates == DUP_UPDATE) &&
      (table->next_number_field != NULL) &&
      rpl_master_has_bug(&active_mi->rli, 24432, TRUE, NULL, NULL))
    goto abort;
#endif

  error=0;
  thd_proc_info(thd, "update");
  if (duplic == DUP_REPLACE &&
      (!table->triggers || !table->triggers->has_delete_triggers()))
    table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
  if (duplic == DUP_UPDATE)
    table->file->extra(HA_EXTRA_INSERT_WITH_UPDATE);
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
#ifndef EMBEDDED_LIBRARY
  if (lock_type != TL_WRITE_DELAYED)
#endif /* EMBEDDED_LIBRARY */
  {
    if (duplic != DUP_ERROR || ignore)
      table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
    if (!thd->prelocked_mode)
      table->file->ha_start_bulk_insert(values_list.elements);
  }

  thd->abort_on_warning= (!ignore && (thd->variables.sql_mode &
                                       (MODE_STRICT_TRANS_TABLES |
                                        MODE_STRICT_ALL_TABLES)));

  prepare_triggers_for_insert_stmt(table);


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
	if (values_list.elements != 1 && ! thd->is_error())
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
      if (thd->lex->used_tables)		      // Column used in values()
	restore_record(table,s->default_values);	// Get empty record
      else
      {
        TABLE_SHARE *share= table->s;

        /*
          Fix delete marker. No need to restore rest of record since it will
          be overwritten by fill_record() anyway (and fill_record() does not
          use default values in this case).
        */
        table->record[0][0]= share->default_values[0];

        /* Fix undefined null_bits. */
        if (share->null_bytes > 1 && share->last_null_bit_pos)
        {
          table->record[0][share->null_bytes - 1]= 
            share->default_values[share->null_bytes - 1];
        }
      }
      if (fill_record_n_invoke_before_triggers(thd, table->field, *values, 0,
                                               table->triggers,
                                               TRG_EVENT_INSERT))
      {
	if (values_list.elements != 1 && ! thd->is_error())
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
      LEX_STRING const st_query = { query, thd->query_length() };
      error=write_delayed(thd, table, duplic, st_query, ignore, log_on);
      query=0;
    }
    else
#endif
      error=write_record(thd, table ,&info);
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
      info.copied=values_list.elements;
      end_delayed_insert(thd);
    }
  }
  else
#endif
  {
    /*
      Do not do this release if this is a delayed insert, it would steal
      auto_inc values from the delayed_insert thread as they share TABLE.
    */
    table->file->ha_release_auto_increment();
    if (!thd->prelocked_mode && table->file->ha_end_bulk_insert() && !error)
    {
      table->file->print_error(my_errno,MYF(0));
      error=1;
    }
    if (duplic != DUP_ERROR || ignore)
      table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

    transactional_table= table->file->has_transactions();

    if ((changed= (info.copied || info.deleted || info.updated)))
    {
      /*
        Invalidate the table in the query cache if something changed.
        For the transactional algorithm to work the invalidation must be
        before binlog writing and ha_autocommit_or_rollback
      */
      query_cache_invalidate3(thd, table_list, 1);
    }
    if (error <= 0 ||
        thd->transaction.stmt.modified_non_trans_table ||
        was_insert_delayed)
    {
      if (mysql_bin_log.is_open())
      {
        int errcode= 0;
	if (error <= 0)
        {
	  /*
	    [Guilhem wrote] Temporary errors may have filled
	    thd->net.last_error/errno.  For example if there has
	    been a disk full error when writing the row, and it was
	    MyISAM, then thd->net.last_error/errno will be set to
	    "disk full"... and the my_pwrite() will wait until free
	    space appears, and so when it finishes then the
	    write_row() was entirely successful
	  */
	  /* todo: consider removing */
	  thd->clear_error();
	}
        else
          errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
        
	/* bug#22725:

	A query which per-row-loop can not be interrupted with
	KILLED, like INSERT, and that does not invoke stored
	routines can be binlogged with neglecting the KILLED error.
        
	If there was no error (error == zero) until after the end of
	inserting loop the KILLED flag that appeared later can be
	disregarded since previously possible invocation of stored
	routines did not result in any error due to the KILLED.  In
	such case the flag is ignored for constructing binlog event.
	*/
	DBUG_ASSERT(thd->killed != THD::KILL_BAD_DATA || error > 0);
	if (thd->binlog_query(THD::ROW_QUERY_TYPE,
                              thd->query(), thd->query_length(),
			      transactional_table, FALSE,
			      errcode))
        {
	  error=1;
	}
      }
      if (thd->transaction.stmt.modified_non_trans_table)
	thd->transaction.all.modified_non_trans_table= TRUE;
    }
    DBUG_ASSERT(transactional_table || !changed || 
                thd->transaction.stmt.modified_non_trans_table);
  }
  thd_proc_info(thd, "end");
  /*
    We'll report to the client this id:
    - if the table contains an autoincrement column and we successfully
    inserted an autogenerated value, the autogenerated value.
    - if the table contains no autoincrement column and LAST_INSERT_ID(X) was
    called, X.
    - if the table contains an autoincrement column, and some rows were
    inserted, the id of the last "inserted" row (if IGNORE, that value may not
    have been really inserted but ignored).
  */
  id= (thd->first_successful_insert_id_in_cur_stmt > 0) ?
    thd->first_successful_insert_id_in_cur_stmt :
    (thd->arg_of_last_insert_id_function ?
     thd->first_successful_insert_id_in_prev_stmt :
     ((table->next_number_field && info.copied) ?
     table->next_number_field->val_int() : 0));
  table->next_number_field=0;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  table->auto_increment_field_not_null= FALSE;
  if (duplic == DUP_REPLACE &&
      (!table->triggers || !table->triggers->has_delete_triggers()))
    table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);

  if (error)
    goto abort;
  if (values_list.elements == 1 && (!(thd->options & OPTION_WARNINGS) ||
				    !thd->cuted_fields))
  {
    thd->row_count_func= info.copied + info.deleted +
                         ((thd->client_capabilities & CLIENT_FOUND_ROWS) ?
                          info.touched : info.updated);
    my_ok(thd, (ulong) thd->row_count_func, id);
  }
  else
  {
    char buff[160];
    ha_rows updated=((thd->client_capabilities & CLIENT_FOUND_ROWS) ?
                     info.touched : info.updated);
    if (ignore)
      sprintf(buff, ER(ER_INSERT_INFO), (ulong) info.records,
	      (lock_type == TL_WRITE_DELAYED) ? (ulong) 0 :
	      (ulong) (info.records - info.copied), (ulong) thd->cuted_fields);
    else
      sprintf(buff, ER(ER_INSERT_INFO), (ulong) info.records,
	      (ulong) (info.deleted + updated), (ulong) thd->cuted_fields);
    thd->row_count_func= info.copied + info.deleted + updated;
    ::my_ok(thd, (ulong) thd->row_count_func, id, buff);
  }
  thd->abort_on_warning= 0;
  DBUG_RETURN(FALSE);

abort:
#ifndef EMBEDDED_LIBRARY
  if (lock_type == TL_WRITE_DELAYED)
    end_delayed_insert(thd);
#endif
  if (table != NULL)
    table->file->ha_release_auto_increment();
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
  uint used_fields_buff_size= bitmap_buffer_size(table->s->fields);
  uint32 *used_fields_buff= (uint32*)thd->alloc(used_fields_buff_size);
  MY_BITMAP used_fields;
  enum_mark_columns save_mark_used_columns= thd->mark_used_columns;
  DBUG_ENTER("check_key_in_view");

  if (!used_fields_buff)
    DBUG_RETURN(TRUE);  // EOM

  DBUG_ASSERT(view->table != 0 && view->field_translation != 0);

  VOID(bitmap_init(&used_fields, used_fields_buff, table->s->fields, 0));
  bitmap_clear_all(&used_fields);

  view->contain_auto_increment= 0;
  /* 
    we must not set query_id for fields as they're not 
    really used in this context
  */
  thd->mark_used_columns= MARK_COLUMNS_NONE;
  /* check simplicity and prepare unique test of view */
  for (trans= trans_start; trans != trans_end; trans++)
  {
    if (!trans->item->fixed && trans->item->fix_fields(thd, &trans->item))
    {
      thd->mark_used_columns= save_mark_used_columns;
      DBUG_RETURN(TRUE);
    }
    Item_field *field;
    /* simple SELECT list entry (field without expression) */
    if (!(field= trans->item->filed_for_view_update()))
    {
      thd->mark_used_columns= save_mark_used_columns;
      DBUG_RETURN(TRUE);
    }
    if (field->field->unireg_check == Field::NEXT_NUMBER)
      view->contain_auto_increment= 1;
    /* prepare unique test */
    /*
      remove collation (or other transparent for update function) if we have
      it
    */
    trans->item= field;
  }
  thd->mark_used_columns= save_mark_used_columns;
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

  /*
     first table in list is the one we'll INSERT into, requires INSERT_ACL.
     all others require SELECT_ACL only. the ACL requirement below is for
     new leaves only anyway (view-constituents), so check for SELECT rather
     than INSERT.
  */

  if (setup_tables_and_check_access(thd, &thd->lex->select_lex.context,
                                    &thd->lex->select_lex.top_join_list,
                                    table_list,
                                    &thd->lex->select_lex.leaf_tables,
                                    select_insert, INSERT_ACL, SELECT_ACL))
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
  Get extra info for tables we insert into

  @param table     table(TABLE object) we insert into,
                   might be NULL in case of view
  @param           table(TABLE_LIST object) or view we insert into
*/

static void prepare_for_positional_update(TABLE *table, TABLE_LIST *tables)
{
  if (table)
  {
    if(table->reginfo.lock_type != TL_WRITE_DELAYED)
      table->prepare_for_position();
    return;
  }

  DBUG_ASSERT(tables->view);
  List_iterator<TABLE_LIST> it(*tables->view_tables);
  TABLE_LIST *tbl;
  while ((tbl= it++))
    prepare_for_positional_update(tbl->table, tbl);

  return;
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
    check_fields        TRUE if need to check that all INSERT fields are 
                        given values.
    abort_on_warning    whether to report if some INSERT field is not 
                        assigned as an error (TRUE) or as a warning (FALSE).

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
                          COND **where, bool select_insert,
                          bool check_fields, bool abort_on_warning)
{
  SELECT_LEX *select_lex= &thd->lex->select_lex;
  Name_resolution_context *context= &select_lex->context;
  Name_resolution_context_state ctx_state;
  bool insert_into_view= (table_list->view != 0);
  bool res= 0;
  table_map map= 0;
  DBUG_ENTER("mysql_prepare_insert");
  DBUG_PRINT("enter", ("table_list 0x%lx, table 0x%lx, view %d",
		       (ulong)table_list, (ulong)table,
		       (int)insert_into_view));
  /* INSERT should have a SELECT or VALUES clause */
  DBUG_ASSERT (!select_insert || !values);

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


  /* Prepare the fields in the statement. */
  if (values)
  {
    /* if we have INSERT ... VALUES () we cannot have a GROUP BY clause */
    DBUG_ASSERT (!select_lex->group_list.elements);

    /* Save the state of the current name resolution context. */
    ctx_state.save_state(context, table_list);

    /*
      Perform name resolution only in the first table - 'table_list',
      which is the table that is inserted into.
     */
    table_list->next_local= 0;
    context->resolve_in_table_list_only(table_list);

    res= check_insert_fields(thd, context->table_list, fields, *values,
                             !insert_into_view, &map) ||
      setup_fields(thd, 0, *values, MARK_COLUMNS_READ, 0, 0);

    if (!res && check_fields)
    {
      bool saved_abort_on_warning= thd->abort_on_warning;
      thd->abort_on_warning= abort_on_warning;
      res= check_that_all_fields_are_given_values(thd, 
                                                  table ? table : 
                                                  context->table_list->table,
                                                  context->table_list);
      thd->abort_on_warning= saved_abort_on_warning;
    }

    if (!res && duplic == DUP_UPDATE)
    {
      select_lex->no_wrap_view_item= TRUE;
      res= check_update_fields(thd, context->table_list, update_fields, &map);
      select_lex->no_wrap_view_item= FALSE;
    }

    /* Restore the current context. */
    ctx_state.restore_state(context, table_list);

    if (!res)
      res= setup_fields(thd, 0, update_values, MARK_COLUMNS_READ, 0, 0);
  }

  if (res)
    DBUG_RETURN(res);

  if (!table)
    table= table_list->table;

  if (!select_insert)
  {
    Item *fake_conds= 0;
    TABLE_LIST *duplicate;
    if ((duplicate= unique_table(thd, table_list, table_list->next_global, 1)))
    {
      update_non_unique_table_error(table_list, "INSERT", duplicate);
      DBUG_RETURN(TRUE);
    }
    select_lex->fix_prepare_information(thd, &fake_conds, &fake_conds);
    select_lex->first_execution= 0;
  }
  /*
    Only call prepare_for_posistion() if we are not performing a DELAYED
    operation. It will instead be executed by delayed insert thread.
  */
  if (duplic == DUP_UPDATE || duplic == DUP_REPLACE)
    prepare_for_positional_update(table, table_list);
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

    Sets thd->transaction.stmt.modified_non_trans_table to TRUE if table which is updated didn't have
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
  ulonglong prev_insert_id= table->file->next_insert_id;
  ulonglong insert_id_for_cur_row= 0;
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
      /*
        If we do more than one iteration of this loop, from the second one the
        row will have an explicit value in the autoinc field, which was set at
        the first call of handler::update_auto_increment(). So we must save
        the autogenerated value to avoid thd->insert_id_for_cur_row to become
        0.
      */
      if (table->file->insert_id_for_cur_row > 0)
        insert_id_for_cur_row= table->file->insert_id_for_cur_row;
      else
        table->file->insert_id_for_cur_row= insert_id_for_cur_row;
      bool is_duplicate_key_error;
      if (table->file->is_fatal_error(error, HA_CHECK_DUP))
	goto err;
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
      DEBUG_SYNC(thd, "write_row_replace");

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
	  (insert_id_for_cur_row > 0))
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
	key_copy((uchar*) key,table->record[0],table->key_info+key_nr,0);
	if ((error=(table->file->index_read_idx_map(table->record[1],key_nr,
                                                    (uchar*) key, HA_WHOLE_KEY,
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
                                                 *info->update_values,
                                                 info->ignore,
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

        table->file->restore_auto_increment(prev_insert_id);
        if (table->next_number_field)
          table->file->adjust_next_insert_id_after_explicit_value(
            table->next_number_field->val_int());
        info->touched++;
        if (!records_are_comparable(table) || compare_records(table))
        {
          if ((error=table->file->ha_update_row(table->record[1],
                                                table->record[0])) &&
              error != HA_ERR_RECORD_IS_THE_SAME)
          {
            if (info->ignore &&
                !table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
            {
              goto ok_or_after_trg_err;
            }
            goto err;
          }

          if (error != HA_ERR_RECORD_IS_THE_SAME)
            info->updated++;
          else
            error= 0;
          /*
            If ON DUP KEY UPDATE updates a row instead of inserting one, it's
            like a regular UPDATE statement: it should not affect the value of a
            next SELECT LAST_INSERT_ID() or mysql_insert_id().
            Except if LAST_INSERT_ID(#) was in the INSERT query, which is
            handled separately by THD::arg_of_last_insert_id_function.
          */
          insert_id_for_cur_row= table->file->insert_id_for_cur_row= 0;
          trg_error= (table->triggers &&
                      table->triggers->process_triggers(thd, TRG_EVENT_UPDATE,
                                                        TRG_ACTION_AFTER, TRUE));
          info->copied++;
        }

        if (table->next_number_field)
          table->file->adjust_next_insert_id_after_explicit_value(
            table->next_number_field->val_int());
        info->touched++;

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
          if ((error=table->file->ha_update_row(table->record[1],
					        table->record[0])) &&
              error != HA_ERR_RECORD_IS_THE_SAME)
            goto err;
          if (error != HA_ERR_RECORD_IS_THE_SAME)
            info->deleted++;
          else
            error= 0;
          thd->record_first_successful_insert_id_in_cur_stmt(table->file->insert_id_for_cur_row);
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
            thd->transaction.stmt.modified_non_trans_table= TRUE;
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
        If more than one iteration of the above while loop is done, from the second 
        one the row being inserted will have an explicit value in the autoinc field, 
        which was set at the first call of handler::update_auto_increment(). This 
        value is saved to avoid thd->insert_id_for_cur_row becoming 0. Use this saved
        autoinc value.
     */
    if (table->file->insert_id_for_cur_row == 0)
      table->file->insert_id_for_cur_row= insert_id_for_cur_row;
      
    thd->record_first_successful_insert_id_in_cur_stmt(table->file->insert_id_for_cur_row);
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
    DEBUG_SYNC(thd, "write_row_noreplace");
    if (!info->ignore ||
        table->file->is_fatal_error(error, HA_CHECK_DUP))
      goto err;
    table->file->restore_auto_increment(prev_insert_id);
    goto ok_or_after_trg_err;
  }

after_trg_n_copied_inc:
  info->copied++;
  thd->record_first_successful_insert_id_in_cur_stmt(table->file->insert_id_for_cur_row);
  trg_error= (table->triggers &&
              table->triggers->process_triggers(thd, TRG_EVENT_INSERT,
                                                TRG_ACTION_AFTER, TRUE));

ok_or_after_trg_err:
  if (key)
    my_safe_afree(key,table->s->max_unique_length,MAX_KEY_LENGTH);
  if (!table->file->has_transactions())
    thd->transaction.stmt.modified_non_trans_table= TRUE;
  DBUG_RETURN(trg_error);

err:
  info->last_errno= error;
  /* current_select is NULL if this is a delayed insert */
  if (thd->lex->current_select)
    thd->lex->current_select->no_error= 0;        // Give error
  table->file->print_error(error,MYF(0));
  
before_trg_err:
  table->file->restore_auto_increment(prev_insert_id);
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
        ((*field)->real_type() != MYSQL_TYPE_ENUM))
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
  ulong sql_mode;
  bool auto_increment_field_not_null;
  bool query_start_used, ignore, log_query;
  bool stmt_depends_on_first_successful_insert_id_in_prev_stmt;
  ulonglong first_successful_insert_id_in_prev_stmt;
  ulonglong forced_insert_id;
  ulong auto_increment_increment;
  ulong auto_increment_offset;
  timestamp_auto_set_type timestamp_field_type;
  LEX_STRING query;
  Time_zone *time_zone;

  delayed_row(LEX_STRING const query_arg, enum_duplicates dup_arg,
              bool ignore_arg, bool log_query_arg)
    : record(0), dup(dup_arg), ignore(ignore_arg), log_query(log_query_arg),
      forced_insert_id(0), query(query_arg), time_zone(0)
    {}
  ~delayed_row()
  {
    x_free(query.str);
    x_free(record);
  }
};

/**
  Delayed_insert - context of a thread responsible for delayed insert
  into one table. When processing delayed inserts, we create an own
  thread for every distinct table. Later on all delayed inserts directed
  into that table are handled by a dedicated thread.
*/

class Delayed_insert :public ilink {
  uint locks_in_memory;
  thr_lock_type delayed_lock;
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

  Delayed_insert()
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
    /*
      Statement-based replication of INSERT DELAYED has problems with RAND()
      and user vars, so in mixed mode we go to row-based.
    */
    thd.lex->set_stmt_unsafe();
    thd.set_current_stmt_binlog_row_based_if_mixed();

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
    delayed_lock= global_system_variables.low_priority_updates ?
                                          TL_WRITE_LOW_PRIORITY : TL_WRITE;
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
  }
  ~Delayed_insert()
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
    x_free(thd.query());
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


I_List<Delayed_insert> delayed_threads;


/**
  Return an instance of delayed insert thread that can handle
  inserts into a given table, if it exists. Otherwise return NULL.
*/

static
Delayed_insert *find_handler(THD *thd, TABLE_LIST *table_list)
{
  thd_proc_info(thd, "waiting for delay_list");
  pthread_mutex_lock(&LOCK_delayed_insert);	// Protect master list
  I_List_iterator<Delayed_insert> it(delayed_threads);
  Delayed_insert *di;
  while ((di= it++))
  {
    if (!strcmp(table_list->db, di->table_list.db) &&
	!strcmp(table_list->table_name, di->table_list.table_name))
    {
      di->lock();
      break;
    }
  }
  pthread_mutex_unlock(&LOCK_delayed_insert); // For unlink from list
  return di;
}


/**
  Attempt to find or create a delayed insert thread to handle inserts
  into this table.

  @return In case of success, table_list->table points to a local copy
          of the delayed table or is set to NULL, which indicates a
          request for lock upgrade. In case of failure, value of
          table_list->table is undefined.
  @retval TRUE  - this thread ran out of resources OR
                - a newly created delayed insert thread ran out of
                  resources OR
                - the created thread failed to open and lock the table
                  (e.g. because it does not exist) OR
                - the table opened in the created thread turned out to
                  be a view
  @retval FALSE - table successfully opened OR
                - too many delayed insert threads OR
                - the table has triggers and we have to fall back to
                  a normal INSERT
                Two latter cases indicate a request for lock upgrade.

  XXX: why do we regard INSERT DELAYED into a view as an error and
  do not simply perform a lock upgrade?

  TODO: The approach with using two mutexes to work with the
  delayed thread list -- LOCK_delayed_insert and
  LOCK_delayed_create -- is redundant, and we only need one of
  them to protect the list.  The reason we have two locks is that
  we do not want to block look-ups in the list while we're waiting
  for the newly created thread to open the delayed table. However,
  this wait itself is redundant -- we always call get_local_table
  later on, and there wait again until the created thread acquires
  a table lock.

  As is redundant the concept of locks_in_memory, since we already
  have another counter with similar semantics - tables_in_use,
  both of them are devoted to counting the number of producers for
  a given consumer (delayed insert thread), only at different
  stages of producer-consumer relationship.

  'dead' and 'status' variables in Delayed_insert are redundant
  too, since there is already 'di->thd.killed' and
  di->stacked_inserts.
*/

static
bool delayed_get_table(THD *thd, TABLE_LIST *table_list)
{
  int error;
  Delayed_insert *di;
  DBUG_ENTER("delayed_get_table");

  /* Must be set in the parser */
  DBUG_ASSERT(table_list->db);

  /* Find the thread which handles this table. */
  if (!(di= find_handler(thd, table_list)))
  {
    /*
      No match. Create a new thread to handle the table, but
      no more than max_insert_delayed_threads.
    */
    if (delayed_insert_threads >= thd->variables.max_insert_delayed_threads)
      DBUG_RETURN(0);
    thd_proc_info(thd, "Creating delayed handler");
    pthread_mutex_lock(&LOCK_delayed_create);
    /*
      The first search above was done without LOCK_delayed_create.
      Another thread might have created the handler in between. Search again.
    */
    if (! (di= find_handler(thd, table_list)))
    {
      if (!(di= new Delayed_insert()))
      {
        thd->fatal_error();
        goto end_create;
      }
      pthread_mutex_lock(&LOCK_thread_count);
      thread_count++;
      pthread_mutex_unlock(&LOCK_thread_count);
      di->thd.set_db(table_list->db, (uint) strlen(table_list->db));
      di->thd.set_query(my_strdup(table_list->table_name, MYF(MY_WME)), 0);
      if (di->thd.db == NULL || di->thd.query() == NULL)
      {
        /* The error is reported */
	delete di;
        thd->fatal_error();
        goto end_create;
      }
      di->table_list= *table_list;			// Needed to open table
      /* Replace volatile strings with local copies */
      di->table_list.alias= di->table_list.table_name= di->thd.query();
      di->table_list.db= di->thd.db;
      di->lock();
      pthread_mutex_lock(&di->mutex);
      if ((error= pthread_create(&di->thd.real_id, &connection_attrib,
                                 handle_delayed_insert, (void*) di)))
      {
	DBUG_PRINT("error",
		   ("Can't create thread to handle delayed insert (error %d)",
		    error));
	pthread_mutex_unlock(&di->mutex);
	di->unlock();
	delete di;
	my_error(ER_CANT_CREATE_THREAD, MYF(0), error);
        thd->fatal_error();
        goto end_create;
      }

      /* Wait until table is open */
      thd_proc_info(thd, "waiting for handler open");
      while (!di->thd.killed && !di->table && !thd->killed)
      {
	pthread_cond_wait(&di->cond_client, &di->mutex);
      }
      pthread_mutex_unlock(&di->mutex);
      thd_proc_info(thd, "got old table");
      if (di->thd.killed)
      {
        if (di->thd.is_error())
        {
          /*
            Copy the error message. Note that we don't treat fatal
            errors in the delayed thread as fatal errors in the
            main thread. Use of my_message will enable stored
            procedures continue handlers.
          */
          my_message(di->thd.main_da.sql_errno(), di->thd.main_da.message(),
                     MYF(0));
	}
	di->unlock();
        goto end_create;
      }
      if (thd->killed)
      {
	di->unlock();
	goto end_create;
      }
      pthread_mutex_lock(&LOCK_delayed_insert);
      delayed_threads.append(di);
      pthread_mutex_unlock(&LOCK_delayed_insert);
    }
    pthread_mutex_unlock(&LOCK_delayed_create);
  }

  pthread_mutex_lock(&di->mutex);
  table_list->table= di->get_local_table(thd);
  pthread_mutex_unlock(&di->mutex);
  if (table_list->table)
  {
    DBUG_ASSERT(! thd->is_error());
    thd->di= di;
  }
  /* Unlock the delayed insert object after its last access. */
  di->unlock();
  DBUG_RETURN((table_list->table == NULL));

end_create:
  pthread_mutex_unlock(&LOCK_delayed_create);
  DBUG_RETURN(thd->is_error());
}


/**
  As we can't let many client threads modify the same TABLE
  structure of the dedicated delayed insert thread, we create an
  own structure for each client thread. This includes a row
  buffer to save the column values and new fields that point to
  the new row buffer. The memory is allocated in the client
  thread and is freed automatically.

  @pre This function is called from the client thread.  Delayed
       insert thread mutex must be acquired before invoking this
       function.

  @return Not-NULL table object on success. NULL in case of an error,
                    which is set in client_thd.
*/

TABLE *Delayed_insert::get_local_table(THD* client_thd)
{
  my_ptrdiff_t adjust_ptrs;
  Field **field,**org_field, *found_next_number_field;
  TABLE *copy;
  TABLE_SHARE *share;
  uchar *bitmap;
  DBUG_ENTER("Delayed_insert::get_local_table");

  /* First request insert thread to get a lock */
  status=1;
  tables_in_use++;
  if (!thd.lock)				// Table is not locked
  {
    thd_proc_info(client_thd, "waiting for handler lock");
    pthread_cond_signal(&cond);			// Tell handler to lock table
    while (!dead && !thd.lock && ! client_thd->killed)
    {
      pthread_cond_wait(&cond_client,&mutex);
    }
    thd_proc_info(client_thd, "got handler lock");
    if (client_thd->killed)
      goto error;
    if (dead)
    {
      my_message(thd.main_da.sql_errno(), thd.main_da.message(), MYF(0));
      goto error;
    }
  }
  share= table->s;

  /*
    Allocate memory for the TABLE object, the field pointers array, and
    one record buffer of reclength size. Normally a table has three
    record buffers of rec_buff_length size, which includes alignment
    bytes. Since the table copy is used for creating one record only,
    the other record buffers and alignment are unnecessary.
  */
  thd_proc_info(client_thd, "allocating local table");
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
  bitmap= (uchar*) (field + share->fields + 1);
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
      goto error;
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

static
int write_delayed(THD *thd, TABLE *table, enum_duplicates duplic,
                  LEX_STRING query, bool ignore, bool log_on)
{
  delayed_row *row= 0;
  Delayed_insert *di=thd->di;
  const Discrete_interval *forced_auto_inc;
  DBUG_ENTER("write_delayed");
  DBUG_PRINT("enter", ("query = '%s' length %lu", query.str,
                       (ulong) query.length));

  thd_proc_info(thd, "waiting for handler insert");
  pthread_mutex_lock(&di->mutex);
  while (di->stacked_inserts >= delayed_queue_size && !thd->killed)
    pthread_cond_wait(&di->cond_client,&di->mutex);
  thd_proc_info(thd, "storing row into queue");

  if (thd->killed)
    goto err;

  /*
    Take a copy of the query string, if there is any. The string will
    be free'ed when the row is destroyed. If there is no query string,
    we don't do anything special.
   */

  if (query.str)
  {
    char *str;
    if (!(str= my_strndup(query.str, query.length, MYF(MY_WME))))
      goto err;
    query.str= str;
  }
  row= new delayed_row(query, duplic, ignore, log_on);
  if (row == NULL)
  {
    my_free(query.str, MYF(MY_WME));
    goto err;
  }

  if (!(row->record= (char*) my_malloc(table->s->reclength, MYF(MY_WME))))
    goto err;
  memcpy(row->record, table->record[0], table->s->reclength);
  row->start_time=		thd->start_time;
  row->query_start_used=	thd->query_start_used;
  /*
    those are for the binlog: LAST_INSERT_ID() has been evaluated at this
    time, so record does not need it, but statement-based binlogging of the
    INSERT will need when the row is actually inserted.
    As for SET INSERT_ID, DELAYED does not honour it (BUG#20830).
  */
  row->stmt_depends_on_first_successful_insert_id_in_prev_stmt=
    thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt;
  row->first_successful_insert_id_in_prev_stmt=
    thd->first_successful_insert_id_in_prev_stmt;
  row->timestamp_field_type=    table->timestamp_field_type;

  /* Add session variable timezone
     Time_zone object will not be freed even the thread is ended.
     So we can get time_zone object from thread which handling delayed statement.
     See the comment of my_tz_find() for detail.
  */
  if (thd->time_zone_used)
  {
    row->time_zone = thd->variables.time_zone;
  }
  else
  {
    row->time_zone = NULL;
  }
  /* Copy session variables. */
  row->auto_increment_increment= thd->variables.auto_increment_increment;
  row->auto_increment_offset=    thd->variables.auto_increment_offset;
  row->sql_mode=                 thd->variables.sql_mode;
  row->auto_increment_field_not_null= table->auto_increment_field_not_null;

  /* Copy the next forced auto increment value, if any. */
  if ((forced_auto_inc= thd->auto_inc_intervals_forced.get_next()))
  {
    row->forced_insert_id= forced_auto_inc->minimum();
    DBUG_PRINT("delayed", ("transmitting auto_inc: %lu",
                           (ulong) row->forced_insert_id));
  }

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

/**
  Signal the delayed insert thread that this user connection
  is finished using it for this statement.
*/

static void end_delayed_insert(THD *thd)
{
  DBUG_ENTER("end_delayed_insert");
  Delayed_insert *di=thd->di;
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

  I_List_iterator<Delayed_insert> it(delayed_threads);
  Delayed_insert *di;
  while ((di= it++))
  {
    di->thd.killed= THD::KILL_CONNECTION;
    if (di->thd.mysys_var)
    {
      pthread_mutex_lock(&di->thd.mysys_var->mutex);
      if (di->thd.mysys_var->current_cond)
      {
	/*
	  We need the following test because the main mutex may be locked
	  in handle_delayed_insert()
	*/
	if (&di->mutex != di->thd.mysys_var->current_mutex)
	  pthread_mutex_lock(di->thd.mysys_var->current_mutex);
	pthread_cond_broadcast(di->thd.mysys_var->current_cond);
	if (&di->mutex != di->thd.mysys_var->current_mutex)
	  pthread_mutex_unlock(di->thd.mysys_var->current_mutex);
      }
      pthread_mutex_unlock(&di->thd.mysys_var->mutex);
    }
  }
  VOID(pthread_mutex_unlock(&LOCK_delayed_insert)); // For unlink from list
}


static void handle_delayed_insert_impl(THD *thd, Delayed_insert *di)
{
  DBUG_ENTER("handle_delayed_insert_impl");
  thd->thread_stack= (char*) &thd;
  if (init_thr_lock() || thd->store_globals())
  {
    /* Can't use my_error since store_globals has perhaps failed */
    thd->main_da.set_error_status(thd, ER_OUT_OF_RESOURCES,
                                  ER(ER_OUT_OF_RESOURCES));
    thd->fatal_error();
    goto err;
  }

  /*
    Open table requires an initialized lex in case the table is
    partitioned. The .frm file contains a partial SQL string which is
    parsed using a lex, that depends on initialized thd->lex.
  */
  lex_start(thd);
  thd->lex->sql_command= SQLCOM_INSERT;        // For innodb::store_lock()
  /*
    Statement-based replication of INSERT DELAYED has problems with RAND()
    and user vars, so in mixed mode we go to row-based.
  */
  thd->lex->set_stmt_unsafe();
  thd->set_current_stmt_binlog_row_based_if_mixed();

  /* Open table */
  if (!(di->table= open_n_lock_single_table(thd, &di->table_list,
                                            TL_WRITE_DELAYED)))
  {
    thd->fatal_error();				// Abort waiting inserts
    goto err;
  }
  if (!(di->table->file->ha_table_flags() & HA_CAN_INSERT_DELAYED))
  {
    thd->fatal_error();
    my_error(ER_DELAYED_NOT_SUPPORTED, MYF(0), di->table_list.table_name);
    goto err;
  }
  if (di->table->triggers)
  {
    /*
      Table has triggers. This is not an error, but we do
      not support triggers with delayed insert. Terminate the delayed
      thread without an error and thus request lock upgrade.
    */
    goto err;
  }
  di->table->copy_blobs=1;

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
      thd_proc_info(&(di->thd), "Waiting for INSERT");

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
    thd_proc_info(&(di->thd), 0);

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
      /*
        We need to release next_insert_id before unlocking. This is
        enforced by handler::ha_external_lock().
      */
      di->table->file->ha_release_auto_increment();
      mysql_unlock_tables(thd, lock);
      ha_autocommit_or_rollback(thd, 0);
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

    TODO: This is not true any more, table maps are generated on the
    first call to ha_*_row() instead. Remove code that are used to
    cover for the case outlined above.
   */
  ha_autocommit_or_rollback(thd, 1);

  DBUG_VOID_RETURN;
}


/*
 * Create a new delayed insert thread
*/

pthread_handler_t handle_delayed_insert(void *arg)
{
  Delayed_insert *di=(Delayed_insert*) arg;
  THD *thd= &di->thd;

  pthread_detach_this_thread();
  /* Add thread to THD list so that's it's visible in 'show processlist' */
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id= thd->variables.pseudo_thread_id= thread_id++;
  thd->set_current_time();
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
    /* Can't use my_error since store_globals has not yet been called */
    thd->main_da.set_error_status(thd, ER_OUT_OF_RESOURCES,
                                  ER(ER_OUT_OF_RESOURCES));
    goto end;
  }
#endif

  handle_delayed_insert_impl(thd, di);

#ifndef __WIN__
end:
#endif
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

  return 0;
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
      uchar *str;
      ((Field_blob *) (*ptr))->get_ptr(&str);
      my_free(str,MYF(MY_ALLOW_ZERO_PTR));
      ((Field_blob *) (*ptr))->reset();
    }
  }
}


bool Delayed_insert::handle_inserts(void)
{
  int error;
  ulong max_rows;
  bool using_ignore= 0, using_opt_replace= 0,
       using_bin_log= mysql_bin_log.is_open();
  delayed_row *row;
  DBUG_ENTER("handle_inserts");

  /* Allow client to insert new rows */
  pthread_mutex_unlock(&mutex);

  table->next_number_field=table->found_next_number_field;
  table->use_all_columns();

  thd_proc_info(&thd, "upgrading lock");
  if (thr_upgrade_write_delay_lock(*thd.lock->locks, delayed_lock))
  {
    /*
      This can happen if thread is killed either by a shutdown
      or if another thread is removing the current table definition
      from the table cache.
    */
    my_error(ER_DELAYED_CANT_CHANGE_LOCK,MYF(ME_FATALERROR),
             table->s->table_name.str);
    goto err;
  }

  thd_proc_info(&thd, "insert");
  max_rows= delayed_insert_limit;
  if (thd.killed || table->needs_reopen_or_name_lock())
  {
    thd.killed= THD::KILL_CONNECTION;
    max_rows= ULONG_MAX;                     // Do as much as possible
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
    /*
      To get the exact auto_inc interval to store in the binlog we must not
      use values from the previous interval (of the previous rows).
    */
    bool log_query= (row->log_query && row->query.str != NULL);
    DBUG_PRINT("delayed", ("query: '%s'  length: %lu", row->query.str ?
                           row->query.str : "[NULL]",
                           (ulong) row->query.length));
    if (log_query)
    {
      /*
        This is the first value of an INSERT statement.
        It is the right place to clear a forced insert_id.
        This is usually done after the last value of an INSERT statement,
        but we won't know this in the insert delayed thread. But before
        the first value is sufficiently equivalent to after the last
        value of the previous statement.
      */
      table->file->ha_release_auto_increment();
      thd.auto_inc_intervals_in_cur_stmt_for_binlog.empty();
    }
    thd.first_successful_insert_id_in_prev_stmt= 
      row->first_successful_insert_id_in_prev_stmt;
    thd.stmt_depends_on_first_successful_insert_id_in_prev_stmt= 
      row->stmt_depends_on_first_successful_insert_id_in_prev_stmt;
    table->timestamp_field_type= row->timestamp_field_type;
    table->auto_increment_field_not_null= row->auto_increment_field_not_null;

    /* Copy the session variables. */
    thd.variables.auto_increment_increment= row->auto_increment_increment;
    thd.variables.auto_increment_offset=    row->auto_increment_offset;
    thd.variables.sql_mode=                 row->sql_mode;

    /* Copy a forced insert_id, if any. */
    if (row->forced_insert_id)
    {
      DBUG_PRINT("delayed", ("received auto_inc: %lu",
                             (ulong) row->forced_insert_id));
      thd.force_one_auto_inc_interval(row->forced_insert_id);
    }

    info.ignore= row->ignore;
    info.handle_duplicates= row->dup;
    if (info.ignore ||
	info.handle_duplicates != DUP_ERROR)
    {
      table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
      using_ignore=1;
    }
    if (info.handle_duplicates == DUP_REPLACE &&
        (!table->triggers ||
         !table->triggers->has_delete_triggers()))
    {
      table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
      using_opt_replace= 1;
    }
    if (info.handle_duplicates == DUP_UPDATE)
      table->file->extra(HA_EXTRA_INSERT_WITH_UPDATE);
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
    if (using_opt_replace)
    {
      using_opt_replace= 0;
      table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
    }

    if (log_query && mysql_bin_log.is_open())
    {
      bool backup_time_zone_used = thd.time_zone_used;
      Time_zone *backup_time_zone = thd.variables.time_zone;
      if (row->time_zone != NULL)
      {
        thd.time_zone_used = true;
        thd.variables.time_zone = row->time_zone;
      }

      /* if the delayed insert was killed, the killed status is
         ignored while binlogging */
      int errcode= 0;
      if (thd.killed == THD::NOT_KILLED)
        errcode= query_error_code(&thd, TRUE);
      
      /*
        If the query has several rows to insert, only the first row will come
        here. In row-based binlogging, this means that the first row will be
        written to binlog as one Table_map event and one Rows event (due to an
        event flush done in binlog_query()), then all other rows of this query
        will be binlogged together as one single Table_map event and one
        single Rows event.
      */
      if (thd.binlog_query(THD::ROW_QUERY_TYPE,
                           row->query.str, row->query.length,
                           FALSE, FALSE, errcode))
        goto err;
      
      thd.time_zone_used = backup_time_zone_used;
      thd.variables.time_zone = backup_time_zone;
    }

    if (table->s->blob_fields)
      free_delayed_insert_blobs(table);
    thread_safe_decrement(delayed_rows_in_use,&LOCK_delayed_status);
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
	thd_proc_info(&thd, "reschedule");
	pthread_mutex_unlock(&mutex);
	if ((error=table->file->extra(HA_EXTRA_NO_CACHE)))
	{
	  /* This should never happen */
	  table->file->print_error(error,MYF(0));
	  sql_print_error("%s", thd.main_da.message());
          DBUG_PRINT("error", ("HA_EXTRA_NO_CACHE failed in loop"));
	  goto err;
	}
	query_cache_invalidate3(&thd, table, 1);
	if (thr_reschedule_write_lock(*thd.lock->locks))
	{
    /* This is not known to happen. */
    my_error(ER_DELAYED_CANT_CHANGE_LOCK,MYF(ME_FATALERROR),
             table->s->table_name.str);
    goto err;
	}
	if (!using_bin_log)
	  table->file->extra(HA_EXTRA_WRITE_CACHE);
	pthread_mutex_lock(&mutex);
	thd_proc_info(&thd, "insert");
      }
      if (tables_in_use)
	pthread_cond_broadcast(&cond_client);	// If waiting clients
    }
  }
  thd_proc_info(&thd, 0);
  pthread_mutex_unlock(&mutex);

  /*
    We need to flush the pending event when using row-based
    replication since the flushing normally done in binlog_query() is
    not done last in the statement: for delayed inserts, the insert
    statement is logged *before* all rows are inserted.

    We can flush the pending event without checking the thd->lock
    since the delayed insert *thread* is not inside a stored function
    or trigger.

    TODO: Move the logging to last in the sequence of rows.
   */
  if (thd.current_stmt_binlog_row_based &&
      thd.binlog_flush_pending_rows_event(TRUE))
    goto err;

  if ((error=table->file->extra(HA_EXTRA_NO_CACHE)))
  {						// This shouldn't happen
    table->file->print_error(error,MYF(0));
    sql_print_error("%s", thd.main_da.message());
    DBUG_PRINT("error", ("HA_EXTRA_NO_CACHE failed after loop"));
    goto err;
  }
  query_cache_invalidate3(&thd, table, 1);
  pthread_mutex_lock(&mutex);
  DBUG_RETURN(0);

 err:
#ifndef DBUG_OFF
  max_rows= 0;                                  // For DBUG output
#endif
  /* Remove all not used rows */
  while ((row=rows.get()))
  {
    if (table->s->blob_fields)
    {
      memcpy(table->record[0],row->record,table->s->reclength);
      free_delayed_insert_blobs(table);
    }
    delete row;
    thread_safe_increment(delayed_insert_errors,&LOCK_delayed_status);
    stacked_inserts--;
#ifndef DBUG_OFF
    max_rows++;
#endif
  }
  DBUG_PRINT("error", ("dropped %lu rows after an error", max_rows));
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
    Statement-based replication of INSERT ... SELECT ... LIMIT is not safe
    as order of rows is not defined, so in mixed mode we go to row-based.

    Note that we may consider a statement as safe if ORDER BY primary_key
    is present or we SELECT a constant. However it may confuse users to
    see very similiar statements replicated differently.
  */
  if (lex->current_select->select_limit)
  {
    lex->set_stmt_unsafe();
    thd->set_current_stmt_binlog_row_based_if_mixed();
  }
  /*
    SELECT_LEX do not belong to INSERT statement, so we can't add WHERE
    clause if table is VIEW
  */
  
  if (mysql_prepare_insert(thd, lex->query_tables,
                           lex->query_tables->table, lex->field_list, 0,
                           lex->update_list, lex->value_list,
                           lex->duplicates,
                           &select_lex->where, TRUE, FALSE, FALSE))
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
   autoinc_value_of_last_inserted_row(0),
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
  table_map map= 0;
  SELECT_LEX *lex_current_select_save= lex->current_select;
  DBUG_ENTER("select_insert::prepare");

  unit= u;

  /*
    Since table in which we are going to insert is added to the first
    select, LEX::current_select should point to the first select while
    we are fixing fields from insert list.
  */
  lex->current_select= &lex->select_lex;

  /* Errors during check_insert_fields() should not be ignored. */
  lex->current_select->no_error= FALSE;
  res= check_insert_fields(thd, table_list, *fields, values,
                           !insert_into_view, &map) ||
       setup_fields(thd, 0, values, MARK_COLUMNS_READ, 0, 0);

  if (!res && fields->elements)
  {
    bool saved_abort_on_warning= thd->abort_on_warning;
    thd->abort_on_warning= !info.ignore && (thd->variables.sql_mode &
                                            (MODE_STRICT_TRANS_TABLES |
                                             MODE_STRICT_ALL_TABLES));
    res= check_that_all_fields_are_given_values(thd, table_list->table, 
                                                table_list);
    thd->abort_on_warning= saved_abort_on_warning;
  }

  if (info.handle_duplicates == DUP_UPDATE && !res)
  {
    Name_resolution_context *context= &lex->select_lex.context;
    Name_resolution_context_state ctx_state;

    /* Save the state of the current name resolution context. */
    ctx_state.save_state(context, table_list);

    /* Perform name resolution only in the first table - 'table_list'. */
    table_list->next_local= 0;
    context->resolve_in_table_list_only(table_list);

    lex->select_lex.no_wrap_view_item= TRUE;
    res= res || check_update_fields(thd, context->table_list,
                                    *info.update_fields, &map);
    lex->select_lex.no_wrap_view_item= FALSE;
    /*
      When we are not using GROUP BY and there are no ungrouped aggregate functions 
      we can refer to other tables in the ON DUPLICATE KEY part.
      We use next_name_resolution_table descructively, so check it first (views?)
    */
    DBUG_ASSERT (!table_list->next_name_resolution_table);
    if (lex->select_lex.group_list.elements == 0 &&
        !lex->select_lex.with_sum_func)
      /*
        We must make a single context out of the two separate name resolution contexts :
        the INSERT table and the tables in the SELECT part of INSERT ... SELECT.
        To do that we must concatenate the two lists
      */  
      table_list->next_name_resolution_table= 
        ctx_state.get_first_name_resolution_table();

    res= res || setup_fields(thd, 0, *info.update_values,
                             MARK_COLUMNS_READ, 0, 0);
    if (!res)
    {
      /*
        Traverse the update values list and substitute fields from the
        select for references (Item_ref objects) to them. This is done in
        order to get correct values from those fields when the select
        employs a temporary table.
      */
      List_iterator<Item> li(*info.update_values);
      Item *item;

      while ((item= li++))
      {
        item->transform(&Item::update_value_transformer,
                        (uchar*)lex->current_select);
      }
    }

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
  if (unique_table(thd, table_list, table_list->next_global, 0))
  {
    /* Using same table for INSERT and SELECT */
    lex->current_select->options|= OPTION_BUFFER_RESULT;
    lex->current_select->join->select_options|= OPTION_BUFFER_RESULT;
  }
  else if (!(lex->current_select->options & OPTION_BUFFER_RESULT) &&
           !thd->prelocked_mode)
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

#ifdef HAVE_REPLICATION
  if (thd->slave_thread &&
      (info.handle_duplicates == DUP_UPDATE) &&
      (table->next_number_field != NULL) &&
      rpl_master_has_bug(&active_mi->rli, 24432, TRUE, NULL, NULL))
    DBUG_RETURN(1);
#endif

  thd->cuted_fields=0;
  if (info.ignore || info.handle_duplicates != DUP_ERROR)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  if (info.handle_duplicates == DUP_REPLACE &&
      (!table->triggers || !table->triggers->has_delete_triggers()))
    table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
  if (info.handle_duplicates == DUP_UPDATE)
    table->file->extra(HA_EXTRA_INSERT_WITH_UPDATE);
  thd->abort_on_warning= (!info.ignore &&
                          (thd->variables.sql_mode &
                           (MODE_STRICT_TRANS_TABLES |
                            MODE_STRICT_ALL_TABLES)));
  res= (table_list->prepare_where(thd, 0, TRUE) ||
        table_list->prepare_check_option(thd));

  if (!res)
     prepare_triggers_for_insert_stmt(table);

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
    table->auto_increment_field_not_null= FALSE;
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
  thd->count_cuted_fields= CHECK_FIELD_ERROR_FOR_NULL;
  if (thd->is_error())
  {
    table->auto_increment_field_not_null= FALSE;
    DBUG_RETURN(1);
  }
  if (table_list)                               // Not CREATE ... SELECT
  {
    switch (table_list->view_check_option(thd, info.ignore)) {
    case VIEW_CHECK_SKIP:
      DBUG_RETURN(0);
    case VIEW_CHECK_ERROR:
      DBUG_RETURN(1);
    }
  }
  
  // Release latches in case bulk insert takes a long time
  ha_release_temporary_latches(thd);

  error= write_record(thd, table, &info);
  table->auto_increment_field_not_null= FALSE;
  
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
        If no value has been autogenerated so far, we need to remember the
        value we just saw, we may need to send it to client in the end.
      */
      if (thd->first_successful_insert_id_in_cur_stmt == 0) // optimization
        autoinc_value_of_last_inserted_row= 
          table->next_number_field->val_int();
      /*
        Clear auto-increment field for the next record, if triggers are used
        we will clear it twice, but this should be cheap.
      */
      table->next_number_field->reset();
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

  DBUG_VOID_RETURN;
}


bool select_insert::send_eof()
{
  int error;
  bool const trans_table= table->file->has_transactions();
  ulonglong id;
  bool changed;
  THD::killed_state killed_status= thd->killed;
  DBUG_ENTER("select_insert::send_eof");
  DBUG_PRINT("enter", ("trans_table=%d, table_type='%s'",
                       trans_table, table->file->table_type()));

  error= (!thd->prelocked_mode) ? table->file->ha_end_bulk_insert():0;
  table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
  table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);

  changed= (info.copied || info.deleted || info.updated);
  if (changed)
  {
    /*
      We must invalidate the table in the query cache before binlog writing
      and ha_autocommit_or_rollback.
    */
    query_cache_invalidate3(thd, table, 1);
    if (thd->transaction.stmt.modified_non_trans_table)
      thd->transaction.all.modified_non_trans_table= TRUE;
  }
  DBUG_ASSERT(trans_table || !changed || 
              thd->transaction.stmt.modified_non_trans_table);

  /*
    Write to binlog before commiting transaction.  No statement will
    be written by the write_to_binlog() below in RBR mode.  All the
    events are in the transaction cache and will be written when
    ha_autocommit_or_rollback() is issued below.
  */
  if (mysql_bin_log.is_open() &&
      (!error || thd->transaction.stmt.modified_non_trans_table))
  {
    int errcode= 0;
    if (!error)
      thd->clear_error();
    else
      errcode= query_error_code(thd, killed_status == THD::NOT_KILLED);

    if (write_to_binlog(trans_table, errcode))
    {
      table->file->ha_release_auto_increment();
      DBUG_RETURN(1);
    }
  }
  table->file->ha_release_auto_increment();

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
  thd->row_count_func= info.copied + info.deleted +
                       ((thd->client_capabilities & CLIENT_FOUND_ROWS) ?
                        info.touched : info.updated);

  id= (thd->first_successful_insert_id_in_cur_stmt > 0) ?
    thd->first_successful_insert_id_in_cur_stmt :
    (thd->arg_of_last_insert_id_function ?
     thd->first_successful_insert_id_in_prev_stmt :
     (info.copied ? autoinc_value_of_last_inserted_row : 0));
  ::my_ok(thd, (ulong) thd->row_count_func, id, buff);
  DBUG_RETURN(0);
}

void select_insert::abort() {

  DBUG_ENTER("select_insert::abort");
  /*
    If the creation of the table failed (due to a syntax error, for
    example), no table will have been opened and therefore 'table'
    will be NULL. In that case, we still need to execute the rollback
    and the end of the function.
   */
  if (table)
  {
    bool changed, transactional_table;
    /*
      If we are not in prelocked mode, we end the bulk insert started
      before.
    */
    if (!thd->prelocked_mode)
      table->file->ha_end_bulk_insert();

    /*
      If at least one row has been inserted/modified and will stay in
      the table (the table doesn't have transactions) we must write to
      the binlog (and the error code will make the slave stop).

      For many errors (example: we got a duplicate key error while
      inserting into a MyISAM table), no row will be added to the table,
      so passing the error to the slave will not help since there will
      be an error code mismatch (the inserts will succeed on the slave
      with no error).

      If table creation failed, the number of rows modified will also be
      zero, so no check for that is made.
    */
    changed= (info.copied || info.deleted || info.updated);
    transactional_table= table->file->has_transactions();
    if (thd->transaction.stmt.modified_non_trans_table)
    {
        if (mysql_bin_log.is_open())
        {
          int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
          /* error of writing binary log is ignored */
          write_to_binlog(transactional_table, errcode);
        }
        if (!thd->current_stmt_binlog_row_based && !can_rollback_data())
          thd->transaction.all.modified_non_trans_table= TRUE;
	if (changed)
	  query_cache_invalidate3(thd, table, 1);
    }
    DBUG_ASSERT(transactional_table || !changed ||
		thd->transaction.stmt.modified_non_trans_table);
    table->file->ha_release_auto_increment();
  }

  DBUG_VOID_RETURN;
}

int select_insert::write_to_binlog(bool is_trans, int errcode)
{
  /* It is only for statement mode */
  if (thd->current_stmt_binlog_row_based)
    return 0;

  return thd->binlog_query(THD::ROW_QUERY_TYPE,
                           thd->query(), thd->query_length(),
                           is_trans, FALSE, errcode);
}

/* Override the select_insert::write_to_binlog */
int select_create::write_to_binlog(bool is_trans, int errcode)
{
  /* It is only for statement mode */
  if (thd->current_stmt_binlog_row_based)
    return 0;

  /*
    WL#5370 Keep the compatibility between 5.1 master and 5.5 slave.
    Binlog a 'INSERT ... SELECT' statement only when it has the option
    'IF NOT EXISTS' and the table already exists as a base table.
  */
  if ((create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS) &&
      create_info->table_existed)
  {
    String query;
    int result;

    thd->binlog_start_trans_and_stmt();
    /* Binlog the CREATE TABLE IF NOT EXISTS statement */
    result= binlog_show_create_table(&table, 1, 0);
    if (result)
      return result;

    uint db_len= strlen(create_table->db);
    uint table_len= strlen(create_info->alias);
    uint select_len= thd->query_length() - thd->lex->create_select_pos;
    uint field_len= (table->s->fields - (field - table->field)) *
      (MAX_FIELD_NAME + 3);

    /*
      pre-allocating memory reduces the times of reallocating memory,
      when calling query.appen().
      40bytes is enough for other words("INSERT IGNORE INTO", etc.).
     */
    if (query.real_alloc(40 + db_len + table_len + field_len + select_len))
      return 1;

    if (thd->lex->create_select_in_comment)
      query.append(STRING_WITH_LEN("/*! "));
    if (thd->lex->ignore)
      query.append(STRING_WITH_LEN("INSERT IGNORE INTO "));
    else if (thd->lex->duplicates == DUP_REPLACE)
      query.append(STRING_WITH_LEN("REPLACE INTO "));
    else
      query.append(STRING_WITH_LEN("INSERT INTO "));

    append_identifier(thd, &query, create_table->db, db_len);
    query.append(STRING_WITH_LEN("."));
    append_identifier(thd, &query, create_info->alias, table_len );
    query.append(STRING_WITH_LEN(" "));

    /*
      The insert items.
      Field is the the rightmost columns that the rows are inster in.
    */
    query.append(STRING_WITH_LEN("("));
    for (Field **f= field ; *f ; f++)
    {
      if (f != field)
        query.append(STRING_WITH_LEN(","));

      append_identifier(thd, &query, (*f)->field_name,
                        strlen((*f)->field_name));
    }
    query.append(STRING_WITH_LEN(") "));

    /* The SELECT clause*/
    DBUG_ASSERT(thd->lex->create_select_pos);
    if (thd->lex->create_select_start_with_brace)
      query.append(STRING_WITH_LEN("("));
    if (query.append(thd->query() + thd->lex->create_select_pos, select_len))
      return 1;

    /*
      Avoid to use thd->binlog_query() twice, otherwise it will print the unsafe
      warning twice.
    */
    Query_log_event ev(thd, query.c_ptr_safe(), query.length(), is_trans,
                       FALSE, errcode);
    return mysql_bin_log.write(&ev);
  }
  else
    return select_insert::write_to_binlog(is_trans, errcode);
}

/***************************************************************************
  CREATE TABLE (SELECT) ...
***************************************************************************/

/*
  Create table from lists of fields and items (or just return TABLE
  object for pre-opened existing table).

  SYNOPSIS
    create_table_from_items()
      thd          in     Thread object
      create_info  in     Create information (like MAX_ROWS, ENGINE or
                          temporary table flag)
      create_table in     Pointer to TABLE_LIST object providing database
                          and name for table to be created or to be open
      alter_info   in/out Initial list of columns and indexes for the table
                          to be created
      items        in     List of items which should be used to produce rest
                          of fields for the table (corresponding fields will
                          be added to the end of alter_info->create_list)
      lock         out    Pointer to the MYSQL_LOCK object for table created
                          (or open temporary table) will be returned in this
                          parameter. Since this table is not included in
                          THD::lock caller is responsible for explicitly
                          unlocking this table.
      hooks

  NOTES
    This function behaves differently for base and temporary tables:
    - For base table we assume that either table exists and was pre-opened
      and locked at open_and_lock_tables() stage (and in this case we just
      emit error or warning and return pre-opened TABLE object) or special
      placeholder was put in table cache that guarantees that this table
      won't be created or opened until the placeholder will be removed
      (so there is an exclusive lock on this table).
    - We don't pre-open existing temporary table, instead we either open
      or create and then open table in this function.

    Since this function contains some logic specific to CREATE TABLE ...
    SELECT it should be changed before it can be used in other contexts.

  RETURN VALUES
    non-zero  Pointer to TABLE object for table created or opened
    0         Error
*/

static TABLE *create_table_from_items(THD *thd, HA_CREATE_INFO *create_info,
                                      TABLE_LIST *create_table,
                                      Alter_info *alter_info,
                                      List<Item> *items,
                                      MYSQL_LOCK **lock,
                                      TABLEOP_HOOKS *hooks)
{
  TABLE tmp_table;		// Used during 'Create_field()'
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
  init_tmp_table_share(thd, &share, "", 0, "", "");

  tmp_table.s->db_create_options=0;
  tmp_table.s->blob_ptr_size= portable_sizeof_char_ptr;
  tmp_table.s->db_low_byte_first= 
        test(create_info->db_type == myisam_hton ||
             create_info->db_type == heap_hton);
  tmp_table.null_row=tmp_table.maybe_null=0;

  while ((item=it++))
  {
    Create_field *cr_field;
    Field *field, *def_field;
    if (item->type() == Item::FUNC_ITEM)
      if (item->result_type() != STRING_RESULT)
        field= item->tmp_table_field(&tmp_table);
      else
        field= item->tmp_table_field_from_field_type(&tmp_table, 0);
    else
      field= create_tmp_field(thd, &tmp_table, item, item->type(),
                              (Item ***) 0, &tmp_field, &def_field, 0, 0, 0, 0,
                              0);
    if (!field ||
	!(cr_field=new Create_field(field,(item->type() == Item::FIELD_ITEM ?
					   ((Item_field *)item)->field :
					   (Field*) 0))))
      DBUG_RETURN(0);
    if (item->maybe_null)
      cr_field->flags &= ~NOT_NULL_FLAG;
    alter_info->create_list.push_back(cr_field);
  }

  DBUG_EXECUTE_IF("sleep_create_select_before_create", my_sleep(6000000););

  /*
    Create and lock table.

    Note that we either creating (or opening existing) temporary table or
    creating base table on which name we have exclusive lock. So code below
    should not cause deadlocks or races.

    We don't log the statement, it will be logged later.

    If this is a HEAP table, the automatic DELETE FROM which is written to the
    binlog when a HEAP table is opened for the first time since startup, must
    not be written: 1) it would be wrong (imagine we're in CREATE SELECT: we
    don't want to delete from it) 2) it would be written before the CREATE
    TABLE, which is a wrong order. So we keep binary logging disabled when we
    open_table().
  */
  {
    tmp_disable_binlog(thd);
    if (!mysql_create_table_no_lock(thd, create_table->db,
                                    create_table->table_name,
                                    create_info, alter_info, 0,
                                    select_field_count))
    {
      if (create_info->table_existed &&
          !(create_info->options & HA_LEX_CREATE_TMP_TABLE))
      {
        /*
          This means that someone created table underneath server
          or it was created via different mysqld front-end to the
          cluster. We don't have much options but throw an error.
        */
        my_error(ER_TABLE_EXISTS_ERROR, MYF(0), create_table->table_name);
        DBUG_RETURN(0);
      }

      DBUG_EXECUTE_IF("sleep_create_select_before_open", my_sleep(6000000););

      if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
      {
        VOID(pthread_mutex_lock(&LOCK_open));
        if (reopen_name_locked_table(thd, create_table, FALSE))
        {
          quick_rm_table(create_info->db_type, create_table->db,
                         table_case_name(create_info, create_table->table_name),
                         0);
        }
        else
          table= create_table->table;
        VOID(pthread_mutex_unlock(&LOCK_open));
      }
      else
      {
        if (!(table= open_table(thd, create_table, thd->mem_root, (bool*) 0,
                                MYSQL_OPEN_TEMPORARY_ONLY)) &&
            !create_info->table_existed)
        {
          /*
            This shouldn't happen as creation of temporary table should make
            it preparable for open. But let us do close_temporary_table() here
            just in case.
          */
          drop_temporary_table(thd, create_table);
        }
      }
    }
    reenable_binlog(thd);
    if (!table)                                   // open failed
      DBUG_RETURN(0);
  }

  DBUG_EXECUTE_IF("sleep_create_select_before_lock", my_sleep(6000000););

  table->reginfo.lock_type=TL_WRITE;
  hooks->prelock(&table, 1);                    // Call prelock hooks
  if (! ((*lock)= mysql_lock_tables(thd, &table, 1,
                                    MYSQL_LOCK_IGNORE_FLUSH, &not_used)) ||
        hooks->postlock(&table, 1))
  {
    if (*lock)
    {
      mysql_unlock_tables(thd, *lock);
      *lock= 0;
    }

    if (!create_info->table_existed)
      drop_open_table(thd, table, create_table->db, create_table->table_name);
    DBUG_RETURN(0);
  }
  DBUG_RETURN(table);
}


int
select_create::prepare(List<Item> &values, SELECT_LEX_UNIT *u)
{
  MYSQL_LOCK *extra_lock= NULL;
  DBUG_ENTER("select_create::prepare");

  TABLEOP_HOOKS *hook_ptr= NULL;
  /*
    For row-based replication, the CREATE-SELECT statement is written
    in two pieces: the first one contain the CREATE TABLE statement
    necessary to create the table and the second part contain the rows
    that should go into the table.

    For non-temporary tables, the start of the CREATE-SELECT
    implicitly commits the previous transaction, and all events
    forming the statement will be stored the transaction cache. At end
    of the statement, the entire statement is committed as a
    transaction, and all events are written to the binary log.

    On the master, the table is locked for the duration of the
    statement, but since the CREATE part is replicated as a simple
    statement, there is no way to lock the table for accesses on the
    slave.  Hence, we have to hold on to the CREATE part of the
    statement until the statement has finished.
   */
  class MY_HOOKS : public TABLEOP_HOOKS {
  public:
    MY_HOOKS(select_create *x, TABLE_LIST *create_table,
             TABLE_LIST *select_tables)
      : ptr(x), all_tables(*create_table)
      {
        all_tables.next_global= select_tables;
      }

  private:
    virtual int do_postlock(TABLE **tables, uint count)
    {
      THD *thd= const_cast<THD*>(ptr->get_thd());
      if (int error= decide_logging_format(thd, &all_tables))
        return error;

      TABLE const *const table = *tables;
      if (thd->current_stmt_binlog_row_based  &&
          !table->s->tmp_table &&
          !ptr->get_create_info()->table_existed)
      {
        int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
        if (int error= ptr->binlog_show_create_table(tables, count, errcode))
          return error;
      }
      return 0;
    }

    select_create *ptr;
    TABLE_LIST all_tables;
  };

  MY_HOOKS hooks(this, create_table, select_tables);
  hook_ptr= &hooks;

  unit= u;

  /*
    Start a statement transaction before the create if we are using
    row-based replication for the statement.  If we are creating a
    temporary table, we need to start a statement transaction.
  */
  if ((thd->lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) == 0 &&
      thd->current_stmt_binlog_row_based &&
      mysql_bin_log.is_open())
  {
    thd->binlog_start_trans_and_stmt();
  }

  DBUG_EXECUTE_IF("sleep_create_select_before_check_if_exists", my_sleep(6000000););

  if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
      (create_table->table && create_table->table->db_stat))
  {
    /* Table already exists and was open at open_and_lock_tables() stage. */
    if (create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS)
    {
      /* Mark that table existed */
      create_info->table_existed= 1;
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                          ER_TABLE_EXISTS_ERROR, ER(ER_TABLE_EXISTS_ERROR),
                          create_table->table_name);
      if (thd->current_stmt_binlog_row_based)
      {
        int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
        binlog_show_create_table(&(create_table->table), 1, errcode);
      }
      table= create_table->table;
    }
    else
    {
      my_error(ER_TABLE_EXISTS_ERROR, MYF(0), create_table->table_name);
      DBUG_RETURN(-1);
    }
  }
  else
    if (!(table= create_table_from_items(thd, create_info, create_table,
                                         alter_info, &values,
                                         &extra_lock, hook_ptr)))
      /* abort() deletes table */
      DBUG_RETURN(-1);

  if (extra_lock)
  {
    DBUG_ASSERT(m_plock == NULL);

    if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
      m_plock= &m_lock;
    else
      m_plock= &thd->extra_lock;

    *m_plock= extra_lock;
  }

  if (table->s->fields < values.elements)
  {
    my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), 1L);
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
  if (info.handle_duplicates == DUP_REPLACE &&
      (!table->triggers || !table->triggers->has_delete_triggers()))
    table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
  if (info.handle_duplicates == DUP_UPDATE)
    table->file->extra(HA_EXTRA_INSERT_WITH_UPDATE);
  if (!thd->prelocked_mode)
    table->file->ha_start_bulk_insert((ha_rows) 0);
  thd->abort_on_warning= (!info.ignore &&
                          (thd->variables.sql_mode &
                           (MODE_STRICT_TRANS_TABLES |
                            MODE_STRICT_ALL_TABLES)));
  if (check_that_all_fields_are_given_values(thd, table, table_list))
    DBUG_RETURN(1);
  table->mark_columns_needed_for_insert();
  table->file->extra(HA_EXTRA_WRITE_CACHE);
  DBUG_RETURN(0);
}

int
select_create::binlog_show_create_table(TABLE **tables, uint count, int errcode)
{
  /*
    Note 1: We generate a CREATE TABLE statement for the
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
  DBUG_ASSERT(tables && *tables && count > 0);

  char buf[2048];
  String query(buf, sizeof(buf), system_charset_info);
  int result;
  TABLE_LIST tmp_table_list;

  memset(&tmp_table_list, 0, sizeof(tmp_table_list));
  tmp_table_list.table = *tables;
  query.length(0);      // Have to zero it since constructor doesn't

  result= store_create_info(thd, &tmp_table_list, &query, create_info,
                            /* show_database */ TRUE);
  DBUG_ASSERT(result == 0); /* store_create_info() always return 0 */

  if (mysql_bin_log.is_open())
  {
    result= thd->binlog_query(THD::STMT_QUERY_TYPE,
                              query.ptr(), query.length(),
                              /* is_trans */ TRUE,
                              /* suppress_use */ FALSE,
                              errcode);
  }
  return result;
}

void select_create::store_values(List<Item> &values)
{
  fill_record_n_invoke_before_triggers(thd, field, values, 1,
                                       table->triggers, TRG_EVENT_INSERT);
}


void select_create::send_error(uint errcode,const char *err)
{
  DBUG_ENTER("select_create::send_error");

  DBUG_PRINT("info",
             ("Current statement %s row-based",
              thd->current_stmt_binlog_row_based ? "is" : "is NOT"));
  DBUG_PRINT("info",
             ("Current table (at 0x%lu) %s a temporary (or non-existant) table",
              (ulong) table,
              table && !table->s->tmp_table ? "is NOT" : "is"));
  DBUG_PRINT("info",
             ("Table %s prior to executing this statement",
              get_create_info()->table_existed ? "existed" : "did not exist"));

  /*
    This will execute any rollbacks that are necessary before writing
    the transcation cache.

    We disable the binary log since nothing should be written to the
    binary log.  This disabling is important, since we potentially do
    a "roll back" of non-transactional tables by removing the table,
    and the actual rollback might generate events that should not be
    written to the binary log.

  */
  tmp_disable_binlog(thd);
  select_insert::send_error(errcode, err);
  reenable_binlog(thd);

  DBUG_VOID_RETURN;
}


bool select_create::send_eof()
{
  bool tmp=select_insert::send_eof();
  if (tmp)
    abort();
  else
  {
    /*
      Do an implicit commit at end of statement for non-temporary
      tables.  This can fail, but we should unlock the table
      nevertheless.
    */
    if (!table->s->tmp_table)
    {
      ha_autocommit_or_rollback(thd, 0);
      end_active_trans(thd);
    }

    table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
    if (m_plock)
    {
      mysql_unlock_tables(thd, *m_plock);
      *m_plock= NULL;
      m_plock= NULL;
    }
  }
  return tmp;
}


void select_create::abort()
{
  DBUG_ENTER("select_create::abort");

  /*
    In select_insert::abort() we roll back the statement, including
    truncating the transaction cache of the binary log. To do this, we
    pretend that the statement is transactional, even though it might
    be the case that it was not.

    We roll back the statement prior to deleting the table and prior
    to releasing the lock on the table, since there might be potential
    for failure if the rollback is executed after the drop or after
    unlocking the table.

    We also roll back the statement regardless of whether the creation
    of the table succeeded or not, since we need to reset the binary
    log state.
  */
  tmp_disable_binlog(thd);
  select_insert::abort();
  thd->transaction.stmt.modified_non_trans_table= FALSE;
  reenable_binlog(thd);
  /* possible error of writing binary log is ignored deliberately */
  (void)thd->binlog_flush_pending_rows_event(TRUE);

  if (m_plock)
  {
    mysql_unlock_tables(thd, *m_plock);
    *m_plock= NULL;
    m_plock= NULL;
  }

  if (table)
  {
    if (thd->lex->sql_command == SQLCOM_CREATE_TABLE &&
        thd->current_stmt_binlog_row_based &&
        !(thd->lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) &&
        mysql_bin_log.is_open())
    {
      /*
        This should be removed after BUG#47899.
      */
      mysql_bin_log.reset_gathered_updates(thd);
    }

    table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
    if (!create_info->table_existed)
      drop_open_table(thd, table, create_table->db, create_table->table_name);
    table=0;                                    // Safety
  }
  DBUG_VOID_RETURN;
}


/*****************************************************************************
  Instansiate templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List_iterator_fast<List_item>;
#ifndef EMBEDDED_LIBRARY
template class I_List<Delayed_insert>;
template class I_List_iterator<Delayed_insert>;
template class I_List<delayed_row>;
#endif /* EMBEDDED_LIBRARY */
#endif /* HAVE_EXPLICIT_TEMPLATE_INSTANTIATION */
