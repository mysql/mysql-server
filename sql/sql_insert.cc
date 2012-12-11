/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "sql_priv.h"
#include "unireg.h"                    // REQUIRED: for other includes
#include "sql_insert.h"
#include "sql_update.h"                         // compare_record
#include "sql_base.h"                           // close_thread_tables
#include "sql_cache.h"                          // query_cache_*
#include "key.h"                                // key_copy
#include "lock.h"                               // mysql_unlock_tables
#include "sp_head.h"
#include "sql_view.h"         // check_key_in_view, insert_view_fields
#include "sql_table.h"        // mysql_create_table_no_lock
#include "sql_acl.h"          // *_ACL, check_grant_all_columns
#include "sql_trigger.h"
#include "sql_select.h"
#include "sql_show.h"
#include "rpl_slave.h"
#include "sql_parse.h"                          // end_active_trans
#include "rpl_mi.h"
#include "transaction.h"
#include "sql_audit.h"
#include "debug_sync.h"
#include "opt_explain.h"
#include "sql_tmp_table.h"    // tmp tables
#include "sql_optimizer.h"    // JOIN
#include "global_threads.h"
#ifdef WITH_PARTITION_STORAGE_ENGINE
#include "sql_partition.h"
#include "partition_info.h"            // partition_info
#endif /* WITH_PARTITION_STORAGE_ENGINE */

#include "debug_sync.h"

static bool check_view_insertability(THD *thd, TABLE_LIST *view);

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

bool check_view_single_update(List<Item> &fields, List<Item> *values,
                              TABLE_LIST *view, table_map *map)
{
  /* it is join view => we need to find the table for update */
  List_iterator_fast<Item> it(fields);
  Item *item;
  TABLE_LIST *tbl= 0;            // reset for call to check_single_table()
  table_map tables= 0;

  while ((item= it++))
    tables|= item->used_tables();

  if (values)
  {
    it.init(*values);
    while ((item= it++))
      tables|= item->used_tables();
  }

  /* Convert to real table bits */
  tables&= ~PSEUDO_TABLE_BITS;


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

  RETURN
    0           OK
    -1          Error
*/

static int check_insert_fields(THD *thd, TABLE_LIST *table_list,
                               List<Item> &fields, List<Item> &values,
                               bool check_unique,
                               bool fields_and_values_from_different_maps,
                               table_map *map)
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
    res= setup_fields(thd, Ref_ptr_array(), fields, MARK_COLUMNS_WRITE, 0, 0);

    /* Restore the current context. */
    ctx_state.restore_state(context, table_list);
    thd->lex->select_lex.no_wrap_view_item= FALSE;

    if (res)
      return -1;

    if (table_list->effective_algorithm == VIEW_ALGORITHM_MERGE)
    {
      if (check_view_single_update(fields,
                                   fields_and_values_from_different_maps ?
                                   (List<Item>*) 0 : &values,
                                   table_list, map))
        return -1;
      table= table_list->table;
    }

    if (check_unique && thd->dup_field)
    {
      my_error(ER_FIELD_SPECIFIED_TWICE, MYF(0), thd->dup_field->field_name);
      return -1;
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

  RETURN
    0           OK
    -1          Error
*/

static int check_update_fields(THD *thd, TABLE_LIST *insert_table_list,
                               List<Item> &update_fields,
                               List<Item> &update_values, table_map *map)
{
  /* Check the fields we are going to modify */
  if (setup_fields(thd, Ref_ptr_array(),
                   update_fields, MARK_COLUMNS_WRITE, 0, 0))
    return -1;

  if (insert_table_list->effective_algorithm == VIEW_ALGORITHM_MERGE &&
      check_view_single_update(update_fields, &update_values,
                               insert_table_list, map))
    return -1;
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
   Wrapper for invocation of function check_that_all_fields_are_given_value.

   @param[in] thd                 Thread handler
   @param[in] table               Table to insert into
   @param[in] table_list          Table list
   @param[in]  abort_on_warning   Whether to report an error or a warning
                                  if some INSERT field is not assigned.

  @return Operation status.
    @retval false   Success
    @retval true    Failure
 */
static bool
safely_check_that_all_fields_are_given_values(THD* thd, TABLE* table,
                                              TABLE_LIST* table_list,
                                              bool abort_on_warning)
{
  bool saved_abort_on_warning= thd->abort_on_warning;
  thd->abort_on_warning= abort_on_warning;

  bool res= check_that_all_fields_are_given_values(thd, table, table_list);

  thd->abort_on_warning= saved_abort_on_warning;

  return res;
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
  bool err= true;
  bool transactional_table, joins_freed= FALSE;
  bool changed;
  bool is_locked= false;
  ulong counter = 1;
  ulonglong id;
  /*
    (1):
    * if the statement lists columns then non-listed columns need a default.
    * if it lists no columns:
    ** if it is of the form "INSERT VALUES (),(),..." then all columns
       need a default; note that "VALUES (), (column_1, ..., column_n)"
       is not allowed, so checking emptiness of the first row is enough.
    ** if it has a "DEFAULT" in VALUES then the column is set by
       Item_default_value::save_in_field(), not by COPY_INFO.
  */

  COPY_INFO info(COPY_INFO::INSERT_OPERATION,
                 &fields,
                 // manage_defaults (1)
                 fields.elements != 0 || values_list.head()->elements == 0,
                 duplic,
                 ignore);
  COPY_INFO update(COPY_INFO::UPDATE_OPERATION, &update_fields, &update_values);
  Name_resolution_context *context;
  Name_resolution_context_state ctx_state;
  Item *unused_conds= 0;
#ifdef WITH_PARTITION_STORAGE_ENGINE
  uint num_partitions= 0;
  enum partition_info::enum_can_prune can_prune_partitions=
                                                  partition_info::PRUNE_NO;
  MY_BITMAP used_partitions;
  bool prune_needs_default_values;
#endif /* WITH_PARITITION_STORAGE_ENGINE */
  DBUG_ENTER("mysql_insert");

  if (open_normal_and_derived_tables(thd, table_list, 0))
    DBUG_RETURN(true);

  THD_STAGE_INFO(thd, stage_init);
  thd->lex->used_tables=0;

  List_iterator_fast<List_item> its(values_list);
  List_item *values= its++;
  const uint value_count= values->elements;
  TABLE *table= NULL;
  if (mysql_prepare_insert(thd, table_list, table, fields, values,
			   update_fields, update_values, duplic, &unused_conds,
                           FALSE,
                           (fields.elements || !value_count ||
                            table_list->view != 0),
                           !ignore && thd->is_strict_mode()))
    goto exit_without_my_ok;

  /* mysql_prepare_insert set table_list->table if it was not set */
  table= table_list->table;

  /* Must be done before can_prune_insert, due to internal initialization. */
  if (info.add_function_default_columns(table, table->write_set))
    goto exit_without_my_ok;
  if (duplic == DUP_UPDATE &&
      update.add_function_default_columns(table, table->write_set))
    goto exit_without_my_ok;

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

#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (!is_locked && table->part_info)
  {
    if (table->part_info->can_prune_insert(thd,
                                           duplic,
                                           update,
                                           update_fields,
                                           fields,
                                           !test(values->elements),
                                           &can_prune_partitions,
                                           &prune_needs_default_values,
                                           &used_partitions))
      goto exit_without_my_ok;

    if (can_prune_partitions != partition_info::PRUNE_NO)
    {
      num_partitions= table->part_info->lock_partitions.n_bits;
      /*
        Pruning probably possible, all partitions is unmarked for read/lock,
        and we must now add them on row by row basis.

        Check the first INSERT value.
        Do not fail here, since that would break MyISAM behavior of inserting
        all rows before the failing row.

        PRUNE_DEFAULTS means the partitioning fields are only set to DEFAULT
        values, so we only need to check the first INSERT value, since all the
        rest will be in the same partition.
      */
      if (table->part_info->set_used_partition(fields,
                                               *values,
                                               info,
                                               prune_needs_default_values,
                                               &used_partitions))
        can_prune_partitions= partition_info::PRUNE_NO;
    }
  }
#endif /* WITH_PARTITION_STORAGE_ENGINE */

  while ((values= its++))
  {
    counter++;
    if (values->elements != value_count)
    {
      my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), counter);
      goto exit_without_my_ok;
    }
    if (setup_fields(thd, Ref_ptr_array(), *values, MARK_COLUMNS_READ, 0, 0))
      goto exit_without_my_ok;

#ifdef WITH_PARTITION_STORAGE_ENGINE
    /*
      To make it possible to increase concurrency on table level locking
      engines such as MyISAM, we check pruning for each row until we will use
      all partitions, Even if the number of rows is much higher than the
      number of partitions.
      TODO: Cache the calculated part_id and reuse in
      ha_partition::write_row() if possible.
    */
    if (can_prune_partitions == partition_info::PRUNE_YES)
    {
      if (table->part_info->set_used_partition(fields,
                                               *values,
                                               info,
                                               prune_needs_default_values,
                                               &used_partitions))
        can_prune_partitions= partition_info::PRUNE_NO;
      if (!(counter % num_partitions))
      {
        /*
          Check if we using all partitions in table after adding partition
          for current row to the set of used partitions. Do it only from
          time to time to avoid overhead from bitmap_is_set_all() call.
        */
        if (bitmap_is_set_all(&used_partitions))
          can_prune_partitions= partition_info::PRUNE_NO;
      }
    }
#endif /* WITH_PARTITION_STORAGE_ENGINE */
  }
  table->auto_increment_field_not_null= false;
  its.rewind ();
 
  /* Restore the current context. */
  ctx_state.restore_state(context, table_list);

  if (thd->lex->describe)
  {
    /*
      Send "No tables used" and stop execution here since
      there is no SELECT to explain.
    */

    err= explain_no_table(thd, "No tables used");
    goto exit_without_my_ok;
  }

#ifdef WITH_PARTITION_STORAGE_ENGINE
  if (can_prune_partitions != partition_info::PRUNE_NO)
  {
    /*
      Only lock the partitions we will insert into.
      And also only read from those partitions (duplicates etc.).
      If explicit partition selection 'INSERT INTO t PARTITION (p1)' is used,
      the new set of read/lock partitions is the intersection of read/lock
      partitions and used partitions, i.e only the partitions that exists in
      both sets will be marked for read/lock.
      It is also safe for REPLACE, since all potentially conflicting records
      always belong to the same partition as the one which we try to
      insert a row. This is because ALL unique/primary keys must
      include ALL partitioning columns.
    */
    bitmap_intersect(&table->part_info->read_partitions,
                     &used_partitions);
    bitmap_intersect(&table->part_info->lock_partitions,
                     &used_partitions);
  }
#endif /* WITH_PARTITION_STORAGE_ENGINE */

  // Lock the tables now if not locked already.
  if (!is_locked &&
      lock_tables(thd, table_list, thd->lex->table_count, 0))
    DBUG_RETURN(true);
 
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
  if (thd->slave_thread)
  {
    DBUG_ASSERT(active_mi != NULL);
    if(info.get_duplicate_handling() == DUP_UPDATE &&
       table->next_number_field != NULL &&
       rpl_master_has_bug(active_mi->rli, 24432, TRUE, NULL, NULL))
      goto exit_without_my_ok;
  }
#endif

  error=0;
  THD_STAGE_INFO(thd, stage_update);
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
  if (duplic != DUP_ERROR || ignore)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  /**
     This is a simple check for the case when the table has a trigger
     that reads from it, or when the statement invokes a stored function
     that reads from the table being inserted to.
     Engines can't handle a bulk insert in parallel with a read form the
     same table in the same connection.
  */
  if (thd->locked_tables_mode <= LTM_LOCK_TABLES)
    table->file->ha_start_bulk_insert(values_list.elements);

  thd->abort_on_warning= (!ignore && thd->is_strict_mode());

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
	  info.stats.records++;
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
      if (thd->lex->used_tables)               // Column used in values()
        restore_record(table,s->default_values); // Get empty record
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
	  info.stats.records++;
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
    error= write_record(thd, table, &info, &update);
    if (error)
      break;
    thd->get_stmt_da()->inc_current_row_for_condition();
  }

  free_underlaid_joins(thd, &thd->lex->select_lex);
  joins_freed= TRUE;

  /*
    Now all rows are inserted.  Time to update logs and sends response to
    user
  */
  {
    table->file->ha_release_auto_increment();
    if (thd->locked_tables_mode <= LTM_LOCK_TABLES &&
        table->file->ha_end_bulk_insert() && !error)
    {
      table->file->print_error(my_errno,MYF(0));
      error=1;
    }
    if (duplic != DUP_ERROR || ignore)
      table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

    transactional_table= table->file->has_transactions();

    if ((changed= (info.stats.copied || info.stats.deleted || info.stats.updated)))
    {
      /*
        Invalidate the table in the query cache if something changed.
        For the transactional algorithm to work the invalidation must be
        before binlog writing and ha_autocommit_or_rollback
      */
      query_cache_invalidate3(thd, table_list, 1);
    }

    if (error <= 0 || thd->transaction.stmt.cannot_safely_rollback())
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
            "disk full"... and the mysql_file_pwrite() will wait until free
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
                              transactional_table, FALSE, FALSE,
                              errcode))
          error= 1;
      }
    }
    DBUG_ASSERT(transactional_table || !changed || 
                thd->transaction.stmt.cannot_safely_rollback());
  }
  THD_STAGE_INFO(thd, stage_end);
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
     ((table->next_number_field && info.stats.copied) ?
     table->next_number_field->val_int() : 0));
  table->next_number_field=0;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  table->auto_increment_field_not_null= FALSE;
  if (duplic == DUP_REPLACE &&
      (!table->triggers || !table->triggers->has_delete_triggers()))
    table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);

  if (error)
    goto exit_without_my_ok;
  if (values_list.elements == 1 && (!(thd->variables.option_bits & OPTION_WARNINGS) ||
				    !thd->cuted_fields))
  {
    my_ok(thd, info.stats.copied + info.stats.deleted +
               ((thd->client_capabilities & CLIENT_FOUND_ROWS) ?
                info.stats.touched : info.stats.updated),
          id);
  }
  else
  {
    char buff[160];
    ha_rows updated=((thd->client_capabilities & CLIENT_FOUND_ROWS) ?
                     info.stats.touched : info.stats.updated);
    if (ignore)
      my_snprintf(buff, sizeof(buff),
                  ER(ER_INSERT_INFO), (long) info.stats.records,
                  (long) (info.stats.records - info.stats.copied),
                  (long) thd->get_stmt_da()->current_statement_cond_count());
    else
      my_snprintf(buff, sizeof(buff),
                  ER(ER_INSERT_INFO), (long) info.stats.records,
                  (long) (info.stats.deleted + updated),
                  (long) thd->get_stmt_da()->current_statement_cond_count());
    my_ok(thd, info.stats.copied + info.stats.deleted + updated, id, buff);
  }
  thd->abort_on_warning= 0;
  DBUG_RETURN(FALSE);

exit_without_my_ok:
  if (!joins_freed)
    free_underlaid_joins(thd, &thd->lex->select_lex);
  thd->abort_on_warning= 0;
  DBUG_RETURN(err);
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

  (void) bitmap_init(&used_fields, used_fields_buff, table->s->fields, 0);
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
    if (!(field= trans->item->field_for_view_update()))
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

  if (!table_list->updatable)
  {
    my_error(ER_NON_INSERTABLE_TABLE, MYF(0), table_list->alias, "INSERT");
    DBUG_RETURN(TRUE);
  }
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
                          Item **where, bool select_insert,
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

    res= (setup_fields(thd, Ref_ptr_array(),
                       *values, MARK_COLUMNS_READ, 0, 0) ||
          check_insert_fields(thd, context->table_list, fields, *values,
                              !insert_into_view, 0, &map));

    if (!res && check_fields)
    {
      TABLE *t= table;
      if (!t)
        t= context->table_list->table;
      res= safely_check_that_all_fields_are_given_values(thd, t,
                                                         context->table_list,
                                                         abort_on_warning);
    }
    if (!res)
      res= setup_fields(thd, Ref_ptr_array(),
                        update_values, MARK_COLUMNS_READ, 0, 0);

    if (!res && duplic == DUP_UPDATE)
    {
      select_lex->no_wrap_view_item= TRUE;
      res= check_update_fields(thd, context->table_list, update_fields,
                               update_values, &map);
      select_lex->no_wrap_view_item= FALSE;
    }

    /* Restore the current context. */
    ctx_state.restore_state(context, table_list);
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
  if (duplic == DUP_UPDATE || duplic == DUP_REPLACE)
    prepare_for_positional_update(table, table_list);
  DBUG_RETURN(FALSE);
}


	/* Check if there is more uniq keys after field */

static int last_uniq_key(TABLE *table,uint keynr)
{
  /*
    When an underlying storage engine informs that the unique key
    conflicts are not reported in the ascending order by setting
    the HA_DUPLICATE_KEY_NOT_IN_ORDER flag, we cannot rely on this
    information to determine the last key conflict.
   
    The information about the last key conflict will be used to
    do a replace of the new row on the conflicting row, rather
    than doing a delete (of old row) + insert (of new row).
   
    Hence check for this flag and disable replacing the last row
    by returning 0 always. Returning 0 will result in doing
    a delete + insert always.
  */
  if (table->file->ha_table_flags() & HA_DUPLICATE_KEY_NOT_IN_ORDER)
    return 0;

  while (++keynr < table->s->keys)
    if (table->key_info[keynr].flags & HA_NOSAME)
      return 0;
  return 1;
}


/**
  Write a record to table with optional deletion of conflicting records,
  invoke proper triggers if needed.

  SYNOPSIS
     write_record()
      thd   - thread context
      table - table to which record should be written
      info  - COPY_INFO structure describing handling of duplicates
              and which is used for counting number of records inserted
              and deleted.
      update - COPY_INFO structure describing the UPDATE part (only used for
               INSERT ON DUPLICATE KEY UPDATE)

  @note

  Once this record is written to the table buffer, any AFTER INSERT trigger
  will be invoked. If instead of inserting a new record we end up updating an
  old one, both ON UPDATE triggers will fire instead. Similarly both ON
  DELETE triggers will be invoked if are to delete conflicting records.

  Call thd->transaction.stmt.mark_modified_non_trans_table() if table is a
  non-transactional table.

  RETURN VALUE
    0     - success
    non-0 - error
*/

int write_record(THD *thd, TABLE *table, COPY_INFO *info, COPY_INFO *update)
{
  int error, trg_error= 0;
  char *key=0;
  MY_BITMAP *save_read_set, *save_write_set;
  ulonglong prev_insert_id= table->file->next_insert_id;
  ulonglong insert_id_for_cur_row= 0;
  DBUG_ENTER("write_record");

  info->stats.records++;
  save_read_set=  table->read_set;
  save_write_set= table->write_set;

  info->set_function_defaults(table);

  const enum_duplicates duplicate_handling= info->get_duplicate_handling();
  const bool ignore_errors= info->get_ignore_errors();

  if (duplicate_handling == DUP_REPLACE || duplicate_handling == DUP_UPDATE)
  {
    DBUG_ASSERT(duplicate_handling != DUP_UPDATE || update != NULL);
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
        if (ignore_errors)
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
      if (duplicate_handling == DUP_REPLACE &&
          table->next_number_field &&
          key_nr == table->s->next_number_index &&
	  (insert_id_for_cur_row > 0))
	goto err;
      if (table->file->ha_table_flags() & HA_DUPLICATE_POS)
      {
        if (table->file->ha_rnd_pos(table->record[1],table->file->dup_ref))
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
	if ((error=(table->file->ha_index_read_idx_map(table->record[1],key_nr,
                                                       (uchar*) key, HA_WHOLE_KEY,
                                                       HA_READ_KEY_EXACT))))
	  goto err;
      }
      if (duplicate_handling == DUP_UPDATE)
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
        DBUG_ASSERT(update->get_changed_columns()->elements ==
                    update->update_values->elements);
        if (fill_record_n_invoke_before_triggers(thd,
                                                 *update->get_changed_columns(),
                                                 *update->update_values,
                                                 ignore_errors,
                                                 table->triggers,
                                                 TRG_EVENT_UPDATE))
          goto before_trg_err;

        /* CHECK OPTION for VIEW ... ON DUPLICATE KEY UPDATE ... */
        {
          const TABLE_LIST *inserted_view=
            table->pos_in_table_list->belong_to_view;
          if (inserted_view != NULL)
          {
            res= inserted_view->view_check_option(thd, ignore_errors);
            if (res == VIEW_CHECK_SKIP)
              goto ok_or_after_trg_err;
            if (res == VIEW_CHECK_ERROR)
              goto before_trg_err;
          }
        }

        table->file->restore_auto_increment(prev_insert_id);
        info->stats.touched++;
        if (!records_are_comparable(table) || compare_records(table))
        {
          // Handle the INSERT ON DUPLICATE KEY UPDATE operation
          update->set_function_defaults(table);

          if ((error=table->file->ha_update_row(table->record[1],
                                                table->record[0])) &&
              error != HA_ERR_RECORD_IS_THE_SAME)
          {
            if (ignore_errors &&
                !table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
            {
              goto ok_or_after_trg_err;
            }
            goto err;
          }

          if (error != HA_ERR_RECORD_IS_THE_SAME)
            info->stats.updated++;
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
          info->stats.copied++;
        }

        if (table->next_number_field)
          table->file->adjust_next_insert_id_after_explicit_value(
            table->next_number_field->val_int());
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
            (!table->triggers || !table->triggers->has_delete_triggers()))
        {
          if ((error=table->file->ha_update_row(table->record[1],
					        table->record[0])) &&
              error != HA_ERR_RECORD_IS_THE_SAME)
            goto err;
          if (error != HA_ERR_RECORD_IS_THE_SAME)
            info->stats.deleted++;
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
          info->stats.deleted++;
          if (!table->file->has_transactions())
            thd->transaction.stmt.mark_modified_non_trans_table();
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
    if (!ignore_errors ||
        table->file->is_fatal_error(error, HA_CHECK_DUP))
      goto err;
    table->file->restore_auto_increment(prev_insert_id);
    goto ok_or_after_trg_err;
  }

after_trg_n_copied_inc:
  info->stats.copied++;
  thd->record_first_successful_insert_id_in_cur_stmt(table->file->insert_id_for_cur_row);
  trg_error= (table->triggers &&
              table->triggers->process_triggers(thd, TRG_EVENT_INSERT,
                                                TRG_ACTION_AFTER, TRUE));

ok_or_after_trg_err:
  if (key)
    my_safe_afree(key,table->s->max_unique_length,MAX_KEY_LENGTH);
  if (!table->file->has_transactions())
    thd->transaction.stmt.mark_modified_non_trans_table();
  DBUG_RETURN(trg_error);

err:
  info->last_errno= error;
  DBUG_ASSERT(thd->lex->current_select != NULL);
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
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_NO_DEFAULT_FOR_VIEW_FIELD,
                            ER(ER_NO_DEFAULT_FOR_VIEW_FIELD),
                            table_list->view_db.str,
                            table_list->view_name.str);
      }
      else
      {
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_NO_DEFAULT_FOR_FIELD,
                            ER(ER_NO_DEFAULT_FOR_FIELD),
                            (*field)->field_name);
      }
      err= 1;
    }
  }
  return thd->abort_on_warning ? err : 0;
}


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


int
select_insert::prepare(List<Item> &values, SELECT_LEX_UNIT *u)
{
  LEX *lex= thd->lex;
  int res;
  table_map map= 0;
  SELECT_LEX *lex_current_select_save= lex->current_select;
  DBUG_ENTER("select_insert::prepare");

  const enum_duplicates duplicate_handling= info.get_duplicate_handling();
  const bool ignore_errors= info.get_ignore_errors();

  unit= u;

  /*
    Since table in which we are going to insert is added to the first
    select, LEX::current_select should point to the first select while
    we are fixing fields from insert list.
  */
  lex->current_select= &lex->select_lex;

  /* Errors during check_insert_fields() should not be ignored. */
  lex->current_select->no_error= FALSE;
  res= (setup_fields(thd, Ref_ptr_array(), values, MARK_COLUMNS_READ, 0, 0) ||
        check_insert_fields(thd, table_list, *fields, values,
                            !insert_into_view, 1, &map));

  if (duplicate_handling == DUP_UPDATE && !res)
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
                                    *update.get_changed_columns(),
                                    *update.update_values,
                                    &map);
    lex->select_lex.no_wrap_view_item= FALSE;
    /*
      When we are not using GROUP BY and there are no ungrouped aggregate
      functions 
      we can refer to other tables in the ON DUPLICATE KEY part.
      We use next_name_resolution_table destructively, so check it first
      (views?).
    */
    DBUG_ASSERT (!table_list->next_name_resolution_table);
    if (lex->select_lex.group_list.elements == 0 &&
        !lex->select_lex.with_sum_func)
    {
      /*
        We must make a single context out of the two separate name resolution
        contexts:
        the INSERT table and the tables in the SELECT part of INSERT ... SELECT.
        To do that we must concatenate the two lists
      */  
      table_list->next_name_resolution_table= 
        ctx_state.get_first_name_resolution_table();
    }
    res= res || setup_fields(thd, Ref_ptr_array(), *update.update_values,
                             MARK_COLUMNS_READ, 0, 0);
    if (!res)
    {
      /*
        Traverse the update values list and substitute fields from the
        select for references (Item_ref objects) to them. This is done in
        order to get correct values from those fields when the select
        employs a temporary table.
      */
      List_iterator<Item> li(*update.update_values);
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

  if (info.add_function_default_columns(table, table->write_set))
    DBUG_RETURN(1);
  if ((duplicate_handling == DUP_UPDATE) &&
      update.add_function_default_columns(table, table->write_set))
    DBUG_RETURN(1);

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
  restore_record(table,s->default_values);		// Get empty record
  table->next_number_field=table->found_next_number_field;

#ifdef HAVE_REPLICATION
  if (thd->slave_thread)
  { 
    DBUG_ASSERT(active_mi != NULL);
    if (duplicate_handling == DUP_UPDATE &&
        table->next_number_field != NULL &&
        rpl_master_has_bug(active_mi->rli, 24432, TRUE, NULL, NULL))
      DBUG_RETURN(1);
  }
#endif

  thd->cuted_fields=0;
  if (ignore_errors || duplicate_handling != DUP_ERROR)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  if (duplicate_handling == DUP_REPLACE &&
      (!table->triggers || !table->triggers->has_delete_triggers()))
    table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
  if (duplicate_handling == DUP_UPDATE)
    table->file->extra(HA_EXTRA_INSERT_WITH_UPDATE);
  thd->abort_on_warning= (!ignore_errors && thd->is_strict_mode());
  res= (table_list->prepare_where(thd, 0, TRUE) ||
        table_list->prepare_check_option(thd));

  if (!res)
  {
     prepare_triggers_for_insert_stmt(table);

     // Check fields for the INSERT INTO ... SELECT statement.
     if (fields->elements)
     {
       res= safely_check_that_all_fields_are_given_values(thd,
                                                          table_list->table,
                                                          table_list,
                                                          !ignore_errors &&
                                                          thd->is_strict_mode());
     }
  }
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
  if (thd->locked_tables_mode <= LTM_LOCK_TABLES &&
      !thd->lex->describe)
  {
    // TODO: Is there no better estimation than 0 == Unknown number of rows?
    table->file->ha_start_bulk_insert((ha_rows) 0);
  }
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
    switch (table_list->view_check_option(thd, info.get_ignore_errors())) {
    case VIEW_CHECK_SKIP:
      DBUG_RETURN(0);
    case VIEW_CHECK_ERROR:
      DBUG_RETURN(1);
    }
  }

  // Release latches in case bulk insert takes a long time
  ha_release_temporary_latches(thd);

  error= write_record(thd, table, &info, &update);
  table->auto_increment_field_not_null= FALSE;
  
  if (!error)
  {
    if (table->triggers || info.get_duplicate_handling() == DUP_UPDATE)
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
  const bool ignore_err= true;
  if (fields->elements)
    fill_record_n_invoke_before_triggers(thd, *fields, values, ignore_err,
                                         table->triggers, TRG_EVENT_INSERT);
  else
    fill_record_n_invoke_before_triggers(thd, table->field, values, ignore_err,
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
  ulonglong id, row_count;
  bool changed;
  THD::killed_state killed_status= thd->killed;
  DBUG_ENTER("select_insert::send_eof");
  DBUG_PRINT("enter", ("trans_table=%d, table_type='%s'",
                       trans_table, table->file->table_type()));

  error= (thd->locked_tables_mode <= LTM_LOCK_TABLES ?
          table->file->ha_end_bulk_insert() : 0);
  if (!error && thd->is_error())
    error= thd->get_stmt_da()->mysql_errno();

  table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
  table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);

  changed= (info.stats.copied || info.stats.deleted || info.stats.updated);
  if (changed)
  {
    /*
      We must invalidate the table in the query cache before binlog writing
      and ha_autocommit_or_rollback.
    */
    query_cache_invalidate3(thd, table, 1);
  }

  DBUG_ASSERT(trans_table || !changed || 
              thd->transaction.stmt.cannot_safely_rollback());

  /*
    Write to binlog before commiting transaction.  No statement will
    be written by the binlog_query() below in RBR mode.  All the
    events are in the transaction cache and will be written when
    ha_autocommit_or_rollback() is issued below.
  */
  if (mysql_bin_log.is_open() &&
      (!error || thd->transaction.stmt.cannot_safely_rollback()))
  {
    int errcode= 0;
    if (!error)
      thd->clear_error();
    else
      errcode= query_error_code(thd, killed_status == THD::NOT_KILLED);
    if (thd->binlog_query(THD::ROW_QUERY_TYPE,
                      thd->query(), thd->query_length(),
                      trans_table, FALSE, FALSE, errcode))
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
  if (info.get_ignore_errors())
    my_snprintf(buff, sizeof(buff),
                ER(ER_INSERT_INFO), (long) info.stats.records,
                (long) (info.stats.records - info.stats.copied),
                (long) thd->get_stmt_da()->current_statement_cond_count());
  else
    my_snprintf(buff, sizeof(buff),
                ER(ER_INSERT_INFO), (long) info.stats.records,
                (long) (info.stats.deleted+info.stats.updated),
                (long) thd->get_stmt_da()->current_statement_cond_count());
  row_count= info.stats.copied + info.stats.deleted +
             ((thd->client_capabilities & CLIENT_FOUND_ROWS) ?
              info.stats.touched : info.stats.updated);
  id= (thd->first_successful_insert_id_in_cur_stmt > 0) ?
    thd->first_successful_insert_id_in_cur_stmt :
    (thd->arg_of_last_insert_id_function ?
     thd->first_successful_insert_id_in_prev_stmt :
     (info.stats.copied ? autoinc_value_of_last_inserted_row : 0));
  my_ok(thd, row_count, id, buff);
  DBUG_RETURN(0);
}

void select_insert::abort_result_set() {

  DBUG_ENTER("select_insert::abort_result_set");
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
      Try to end the bulk insert which might have been started before.
      We don't need to do this if we are in prelocked mode (since we
      don't use bulk insert in this case). Also we should not do this
      if tables are not locked yet (bulk insert is not started yet
      in this case).
    */
    if (thd->locked_tables_mode <= LTM_LOCK_TABLES &&
        thd->lex->is_query_tables_locked())
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
    changed= (info.stats.copied || info.stats.deleted || info.stats.updated);
    transactional_table= table->file->has_transactions();
    if (thd->transaction.stmt.cannot_safely_rollback())
    {
        if (mysql_bin_log.is_open())
        {
          int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
          /* error of writing binary log is ignored */
          (void) thd->binlog_query(THD::ROW_QUERY_TYPE, thd->query(),
                                   thd->query_length(),
                                   transactional_table, FALSE, FALSE, errcode);
        }
	if (changed)
	  query_cache_invalidate3(thd, table, 1);
    }
    DBUG_ASSERT(transactional_table || !changed ||
		thd->transaction.stmt.cannot_safely_rollback());
    table->file->ha_release_auto_increment();
  }

  DBUG_VOID_RETURN;
}


/***************************************************************************
  CREATE TABLE (SELECT) ...
***************************************************************************/

/**
  Create table from lists of fields and items (or just return TABLE
  object for pre-opened existing table). Used by CREATE SELECT.

  Let "source table" be the table in the SELECT part.

  Let "source table columns" be the set of columns in the SELECT list.

  An interesting peculiarity in the syntax CREATE TABLE (<columns>) SELECT is
  that function defaults are stripped from the the source table columns, but
  not from the additional columns defined in the CREATE TABLE part. The first
  @c TIMESTAMP column there is also subject to promotion to @c TIMESTAMP @c
  DEFAULT @c CURRENT_TIMESTAMP @c ON @c UPDATE @c CURRENT_TIMESTAMP, as usual.


  @param thd           [in]     Thread object
  @param create_info   [in]     Create information (like MAX_ROWS, ENGINE or
                                temporary table flag)
  @param create_table  [in]     Pointer to TABLE_LIST object providing database
                                and name for table to be created or to be open
  @param alter_info    [in/out] Initial list of columns and indexes for the
                                table to be created
  @param items         [in]     The source table columns. Corresponding column
                                definitions (Create_field's) will be added to
                                the end of alter_info->create_list.
  @param lock          [out]    Pointer to the MYSQL_LOCK object for table
                                created will be returned in this parameter.
                                Since this table is not included in THD::lock
                                caller is responsible for explicitly unlocking
                                this table.
  @param hooks         [in]     Hooks to be invoked before and after obtaining
                                table lock on the table being created.

  @note
    This function assumes that either table exists and was pre-opened and
    locked at open_and_lock_tables() stage (and in this case we just emit
    error or warning and return pre-opened TABLE object) or an exclusive
    metadata lock was acquired on table so we can safely create, open and
    lock table in it (we don't acquire metadata lock if this create is
    for temporary table).

  @note
    Since this function contains some logic specific to CREATE TABLE ...
    SELECT it should be changed before it can be used in other contexts.

  @retval non-zero  Pointer to TABLE object for table created or opened
  @retval 0         Error
*/

static TABLE *create_table_from_items(THD *thd, HA_CREATE_INFO *create_info,
                                      TABLE_LIST *create_table,
                                      Alter_info *alter_info,
                                      List<Item> *items)
{
  TABLE tmp_table;		// Used during 'Create_field()'
  TABLE_SHARE share;
  TABLE *table= 0;
  uint select_field_count= items->elements;
  /* Add selected items to field list */
  List_iterator_fast<Item> it(*items);
  Item *item;

  DBUG_ENTER("create_table_from_items");

  tmp_table.alias= 0;
  tmp_table.s= &share;
  init_tmp_table_share(thd, &share, "", 0, "", "");

  tmp_table.s->db_create_options=0;
  tmp_table.s->db_low_byte_first= 
        test(create_info->db_type == myisam_hton ||
             create_info->db_type == heap_hton);
  tmp_table.null_row=tmp_table.maybe_null=0;

  if (!thd->variables.explicit_defaults_for_timestamp)
    promote_first_timestamp_column(&alter_info->create_list);

  while ((item=it++))
  {
    Field *tmp_table_field;
    if (item->type() == Item::FUNC_ITEM)
    {
      if (item->result_type() != STRING_RESULT)
        tmp_table_field= item->tmp_table_field(&tmp_table);
      else
        tmp_table_field= item->tmp_table_field_from_field_type(&tmp_table,
                                                               false);
    }
    else
    {
      Field *from_field, *default_field;
      tmp_table_field= create_tmp_field(thd, &tmp_table, item, item->type(),
                                        (Item ***) NULL,
                                        &from_field, &default_field,
                                        false, false, false, false);
    }

    if (!tmp_table_field)
      DBUG_RETURN(NULL);

    Field *table_field;

    switch (item->type())
    {
    /*
      We have to take into account both the real table's fields and
      pseudo-fields used in trigger's body. These fields are used
      to copy defaults values later inside constructor of
      the class Create_field.
    */
    case Item::FIELD_ITEM:
    case Item::TRIGGER_FIELD_ITEM:
      table_field= ((Item_field *) item)->field;
      break;
    default:
      table_field= NULL;
    }

    Create_field *cr_field= new Create_field(tmp_table_field, table_field);

    if (!cr_field)
      DBUG_RETURN(NULL);

    /* Function defaults are removed */
    if (cr_field->unireg_check == Field::TIMESTAMP_DN_FIELD ||
        cr_field->unireg_check == Field::TIMESTAMP_UN_FIELD ||
        cr_field->unireg_check == Field::TIMESTAMP_DNUN_FIELD)
    {
      cr_field->unireg_check= Field::NONE;
    }

    if (item->maybe_null)
      cr_field->flags &= ~NOT_NULL_FLAG;
    alter_info->create_list.push_back(cr_field);
  }

  DEBUG_SYNC(thd,"create_table_select_before_create");

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
    if (!mysql_create_table_no_lock(thd, create_table->db,
                                    create_table->table_name,
                                    create_info, alter_info,
                                    select_field_count, NULL))
    {
      DEBUG_SYNC(thd,"create_table_select_before_open");

      if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE))
      {
        Open_table_context ot_ctx(thd, MYSQL_OPEN_REOPEN);
        /*
          Here we open the destination table, on which we already have
          an exclusive metadata lock.
        */
        if (open_table(thd, create_table, &ot_ctx))
        {
          quick_rm_table(thd, create_info->db_type, create_table->db,
                         table_case_name(create_info, create_table->table_name),
                         0);
        }
        else
          table= create_table->table;
      }
      else
      {
        if (open_temporary_table(thd, create_table))
        {
          /*
            This shouldn't happen as creation of temporary table should make
            it preparable for open. Anyway we can't drop temporary table if
            we are unable to fint it.
          */
          DBUG_ASSERT(0);
        }
        else
        {
          table= create_table->table;
        }
      }
    }
    if (!table)                                   // open failed
      DBUG_RETURN(NULL);
  }
  DBUG_RETURN(table);
}


/**
  Create the new table from the selected items.

  @param values  List of items to be used as new columns
  @param u       Select

  @return Operation status.
    @retval 0   Success
    @retval !=0 Failure
*/

int
select_create::prepare(List<Item> &values, SELECT_LEX_UNIT *u)
{
  DBUG_ENTER("select_create::prepare");

  unit= u;
  DBUG_ASSERT(create_table->table == NULL);

  DEBUG_SYNC(thd,"create_table_select_before_check_if_exists");

  if (!(table= create_table_from_items(thd, create_info, create_table,
                                       alter_info, &values)))
    /* abort() deletes table */
    DBUG_RETURN(-1);

  if (table->s->fields < values.elements)
  {
    my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), 1L);
    DBUG_RETURN(-1);
  }
  /* First field to copy */
  field= table->field+table->s->fields - values.elements;

  DBUG_RETURN(0);
}


/**
  Lock the newly created table and prepare it for insertion.

  @return Operation status.
    @retval 0   Success
    @retval !=0 Failure
*/

int
select_create::prepare2()
{
  DBUG_ENTER("select_create::prepare2");
  DEBUG_SYNC(thd,"create_table_select_before_lock");

  MYSQL_LOCK *extra_lock= NULL;
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
    MY_HOOKS(select_create *x, TABLE_LIST *create_table_arg,
             TABLE_LIST *select_tables_arg)
      : ptr(x),
        create_table(create_table_arg),
        select_tables(select_tables_arg)
      {
      }

  private:
    virtual int do_postlock(TABLE **tables, uint count)
    {
      int error;
      THD *thd= const_cast<THD*>(ptr->get_thd());
      TABLE_LIST *save_next_global= create_table->next_global;

      create_table->next_global= select_tables;

      error= thd->decide_logging_format(create_table);

      create_table->next_global= save_next_global;

      if (error)
        return error;

      TABLE const *const table = *tables;
      if (thd->is_current_stmt_binlog_format_row()  &&
          !table->s->tmp_table)
      {
        if (int error= ptr->binlog_show_create_table(tables, count))
          return error;
      }
      return 0;
    }
    select_create *ptr;
    TABLE_LIST *create_table;
    TABLE_LIST *select_tables;
  };

  MY_HOOKS hooks(this, create_table, select_tables);
 
  table->reginfo.lock_type=TL_WRITE;
  hooks.prelock(&table, 1);                    // Call prelock hooks
  /*
    mysql_lock_tables() below should never fail with request to reopen table
    since it won't wait for the table lock (we have exclusive metadata lock on
    the table) and thus can't get aborted.
  */
  if (! (extra_lock= mysql_lock_tables(thd, &table, 1, 0)) ||
        hooks.postlock(&table, 1))
  {
    if (extra_lock)
    {
      mysql_unlock_tables(thd, extra_lock);
      extra_lock= 0;
    }
    drop_open_table(thd, table, create_table->db, create_table->table_name);
    DBUG_RETURN(1);
  }
  if (extra_lock)
  {
    DBUG_ASSERT(m_plock == NULL);

    if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
      m_plock= &m_lock;
    else
      m_plock= &thd->extra_lock;

    *m_plock= extra_lock;
  }
  /* Mark all fields that are given values */
  for (Field **f= field ; *f ; f++)
    bitmap_set_bit(table->write_set, (*f)->field_index);

  // Set up an empty bitmap of function defaults
  if (info.add_function_default_columns(table, table->write_set))
    DBUG_RETURN(1);

  table->next_number_field=table->found_next_number_field;

  restore_record(table,s->default_values);      // Get empty record
  thd->cuted_fields=0;

  const enum_duplicates duplicate_handling= info.get_duplicate_handling();
  const bool ignore_errors= info.get_ignore_errors();

  if (ignore_errors || duplicate_handling != DUP_ERROR)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  if (duplicate_handling == DUP_REPLACE &&
      (!table->triggers || !table->triggers->has_delete_triggers()))
    table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
  if (duplicate_handling == DUP_UPDATE)
    table->file->extra(HA_EXTRA_INSERT_WITH_UPDATE);
  if (thd->locked_tables_mode <= LTM_LOCK_TABLES)
    table->file->ha_start_bulk_insert((ha_rows) 0);
  thd->abort_on_warning= (!ignore_errors && thd->is_strict_mode());
  if (check_that_all_fields_are_given_values(thd, table, table_list))
    DBUG_RETURN(1);
  table->mark_columns_needed_for_insert();
  table->file->extra(HA_EXTRA_WRITE_CACHE);
  DBUG_RETURN(0);
}

int
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
  DBUG_ASSERT(thd->is_current_stmt_binlog_format_row());
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
    int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
    result= thd->binlog_query(THD::STMT_QUERY_TYPE,
                              query.ptr(), query.length(),
                              /* is_trans */ TRUE,
                              /* direct */ FALSE,
                              /* suppress_use */ FALSE,
                              errcode);
  }
  return result;
}

void select_create::store_values(List<Item> &values)
{
  const bool ignore_err= true;
  fill_record_n_invoke_before_triggers(thd, field, values, ignore_err,
                                       table->triggers, TRG_EVENT_INSERT);
}


void select_create::send_error(uint errcode,const char *err)
{
  DBUG_ENTER("select_create::send_error");

  DBUG_PRINT("info",
             ("Current statement %s row-based",
              thd->is_current_stmt_binlog_format_row() ? "is" : "is NOT"));
  DBUG_PRINT("info",
             ("Current table (at 0x%lu) %s a temporary (or non-existant) table",
              (ulong) table,
              table && !table->s->tmp_table ? "is NOT" : "is"));
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
  /*
    The routine that writes the statement in the binary log
    is in select_insert::send_eof(). For that reason, we
    mark the flag at this point.
  */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
    thd->transaction.stmt.mark_created_temp_table();

  bool tmp=select_insert::send_eof();
  if (tmp)
    abort_result_set();
  else
  {
    /*
      Do an implicit commit at end of statement for non-temporary
      tables.  This can fail, but we should unlock the table
      nevertheless.
    */
    if (!table->s->tmp_table)
    {
      trans_commit_stmt(thd);
      trans_commit_implicit(thd);
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


void select_create::abort_result_set()
{
  DBUG_ENTER("select_create::abort_result_set");

  /*
    In select_insert::abort_result_set() we roll back the statement, including
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
  select_insert::abort_result_set();
  thd->transaction.stmt.reset_unsafe_rollback_flags();
  reenable_binlog(thd);
  /* possible error of writing binary log is ignored deliberately */
  (void) thd->binlog_flush_pending_rows_event(TRUE, TRUE);

  if (m_plock)
  {
    mysql_unlock_tables(thd, *m_plock);
    *m_plock= NULL;
    m_plock= NULL;
  }

  if (table)
  {
    table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
    table->auto_increment_field_not_null= FALSE;
    drop_open_table(thd, table, create_table->db, create_table->table_name);
    table=0;                                    // Safety
  }
  DBUG_VOID_RETURN;
}
