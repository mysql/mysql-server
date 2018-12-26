/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_insert.h"

#include "auth_common.h"              // check_grant_all_columns
#include "debug_sync.h"               // DEBUG_SYNC
#include "item.h"                     // Item
#include "lock.h"                     // mysql_unlock_tables
#include "opt_explain.h"              // Modification_plan
#include "opt_explain_format.h"       // enum_mod_type
#include "rpl_rli.h"                  // Relay_log_info
#include "rpl_slave.h"                // rpl_master_has_bug
#include "sql_base.h"                 // setup_fields
#include "sql_resolver.h"             // Column_privilege_tracker
#include "sql_select.h"               // free_underlaid_joins
#include "sql_show.h"                 // store_create_info
#include "sql_table.h"                // quick_rm_table
#include "sql_tmp_table.h"            // create_tmp_field
#include "sql_update.h"               // records_are_comparable
#include "sql_view.h"                 // check_key_in_view
#include "table_trigger_dispatcher.h" // Table_trigger_dispatcher
#include "transaction.h"              // trans_commit_stmt
#include "sql_resolver.h"             // validate_gc_assignment
#include "partition_info.h"           // partition_info
#include "probes_mysql.h"             // MYSQL_INSERT_START

static bool check_view_insertability(THD *thd, TABLE_LIST *view,
                                     const TABLE_LIST *insert_table_ref);

static void prepare_for_positional_update(TABLE *table, TABLE_LIST *tables);

/**
  Check that insert fields are from a single table of a multi-table view.

  @param fields            The insert fields to be checked.
  @param view              The view for insert.
  @param insert_table_ref[out] Reference to table to insert into

  This function is called to check that the fields being inserted into
  are from a single base table. This must be checked when the table to
  be inserted into is a multi-table view.

  @return false if success, true if an error was raised.
*/

static bool check_single_table_insert(List<Item> &fields, TABLE_LIST *view,
                                      TABLE_LIST **insert_table_ref)
{
  // It is join view => we need to find the table for insert
  List_iterator_fast<Item> it(fields);
  Item *item;
  *insert_table_ref= NULL;          // reset for call to check_single_table()
  table_map tables= 0;

  while ((item= it++))
    tables|= item->used_tables();

  if (view->check_single_table(insert_table_ref, tables))
  {
    my_error(ER_VIEW_MULTIUPDATE, MYF(0),
             view->view_db.str, view->view_name.str);
    return true;
  }
  DBUG_ASSERT(*insert_table_ref && (*insert_table_ref)->is_insertable());

  return false;
}


/**
  Check insert fields.

  @param thd          The current thread.
  @param table_list   The table for insert.
  @param fields       The insert fields.
  @param value_count  Number of values supplied
  @param value_count_known if false, delay field count check
                      @todo: Eliminate this when preparation is properly phased
  @param check_unique If duplicate values should be rejected.

  @return false if success, true if error

  Resolved reference to base table is returned in lex->insert_table_leaf.

  @todo check_insert_fields() should be refactored as follows:
        - Remove the argument value_count_known and all predicates involving it.
        - Rearrange the call to check_insert_fields() from
          mysql_prepare_insert() so that the value_count is known also when
          processing a prepared statement.
*/

static bool check_insert_fields(THD *thd, TABLE_LIST *table_list,
                                List<Item> &fields, uint value_count,
                                bool value_count_known, bool check_unique)
{
  LEX *const lex= thd->lex;

#ifndef DBUG_OFF
  TABLE_LIST *const saved_insert_table_leaf= lex->insert_table_leaf;
#endif

  TABLE *table= table_list->table;

  DBUG_ASSERT(table_list->is_insertable());

  if (fields.elements == 0 && value_count_known && value_count > 0)
  {
    /*
      No field list supplied, but a value list has been supplied.
      Use field list of table being updated.
    */
    DBUG_ASSERT(table);    // This branch is not reached with a view:

    lex->insert_table_leaf= table_list;

    // Values for all fields in table are needed
    if (value_count != table->s->fields)
    {
      my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), 1L);
      return true;
    }
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    Field_iterator_table_ref field_it;
    field_it.set(table_list);
    if (check_grant_all_columns(thd, INSERT_ACL, &field_it))
      return true;
#endif
    /*
      No fields are provided so all fields must be provided in the values.
      Thus we set all bits in the write set.
    */
    bitmap_set_all(table->write_set);
  }
  else
  {
    // INSERT with explicit field list.
    SELECT_LEX *select_lex= thd->lex->select_lex;
    Name_resolution_context *context= &select_lex->context;
    Name_resolution_context_state ctx_state;
    int res;

    if (value_count_known && fields.elements != value_count)
    {
      my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), 1L);
      return true;
    }

    thd->dup_field= 0;

    /* Save the state of the current name resolution context. */
    ctx_state.save_state(context, table_list);

    /*
      Perform name resolution only in the first table - 'table_list',
      which is the table that is inserted into.
    */
    table_list->next_local= NULL;
    context->resolve_in_table_list_only(table_list);
    res= setup_fields(thd, Ref_ptr_array(), fields, INSERT_ACL, NULL,
                      false, true);

    /* Restore the current context. */
    ctx_state.restore_state(context, table_list);

    if (res)
      return true;

    if (table_list->is_merged())
    {
      if (check_single_table_insert(fields, table_list,
                                    &lex->insert_table_leaf))
        return true;
      table= lex->insert_table_leaf->table;
    }
    else
    {
      lex->insert_table_leaf= table_list;
    }

    if (check_unique && thd->dup_field)
    {
      my_error(ER_FIELD_SPECIFIED_TWICE, MYF(0), thd->dup_field->field_name);
      return true;
    }
  }
  /* Mark all generated columns for write*/
  if (table->vfield)
    table->mark_generated_columns(false);

  if (check_key_in_view(thd, table_list, lex->insert_table_leaf) ||
      (table_list->is_view() &&
       check_view_insertability(thd, table_list, lex->insert_table_leaf)))
  {
    my_error(ER_NON_INSERTABLE_TABLE, MYF(0), table_list->alias, "INSERT");
    return true;
  }

  DBUG_ASSERT(saved_insert_table_leaf == NULL ||
              lex->insert_table_leaf == saved_insert_table_leaf);

  return false;
}


/**
  Check that table references are restricted to the supplied table map.
  The check can be ignored if the supplied table is a base table.

  @param view   Table being specified
  @param values Values whose used tables are to be matched against table map
  @param map    Table map to match against

  @return false if success, true if error
*/

static bool check_valid_table_refs(const TABLE_LIST *view, List<Item> &values,
                                   table_map map)
{
  List_iterator_fast<Item> it(values);
  Item *item;

  // A base table will always match the supplied map.
  DBUG_ASSERT(view->is_view() || (view->table && map));

  if (!view->is_view())       // Ignore check if called with base table.
    return false;

  map|= PSEUDO_TABLE_BITS;

  while ((item= it++))
  {
    if (item->used_tables() & ~map)
    {
      my_error(ER_VIEW_MULTIUPDATE, MYF(0),
               view->view_db.str, view->view_name.str);
      return true;
    }
  }
  return false;
}


/**
  Validates default value of fields which are not specified in
  the column list of INSERT statement.

  @Note table->record[0] should be be populated with default values
        before calling this function.

  @param thd              thread context
  @param table            table to which values are inserted.

  @return
    @retval false Success.
    @retval true  Failure.
*/

bool validate_default_values_of_unset_fields(THD *thd, TABLE *table)
{
  MY_BITMAP *write_set= table->write_set;
  DBUG_ENTER("validate_default_values_of_unset_fields");

  for (Field **field= table->field; *field; field++)
  {
    if (!bitmap_is_set(write_set, (*field)->field_index) &&
        !((*field)->flags & NO_DEFAULT_VALUE_FLAG))
    {
      if ((*field)->validate_stored_val(thd) && thd->is_error())
        DBUG_RETURN(true);
    }
  }

  DBUG_RETURN(false);
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
  Setup data for field BLOB/GEOMETRY field types for execution of
  "INSERT...UPDATE" statement. For a expression in 'UPDATE' clause
  like "a= VALUES(a)", let as call Field* referring 'a' as LHS_FIELD
  and Field* referring field 'a' in "VALUES(a)" as RHS_FIELD

  This function creates a separate copy of the blob value for RHS_FIELD,
  if the field is updated as well as accessed through VALUES()
  function in 'UPDATE' clause of "INSERT...UPDATE" statement.

  @param [in] thd
    Pointer to THD object.

  @param [in] fields
    List of fields representing LHS_FIELD of all expressions
    in 'UPDATE' clause.

  @return - Can fail only when we are out of memory.
    @retval false   Success
    @retval true    Failure
*/

bool mysql_prepare_blob_values(THD *thd, List<Item> &fields, MEM_ROOT *mem_root)
{
  DBUG_ENTER("mysql_prepare_blob_values");

  if (fields.elements <= 1)
    DBUG_RETURN(false);

  // Collect LHS_FIELD's which are updated in a 'set'.
  // This 'set' helps decide if we need to make copy of BLOB value
  // or not.

  Prealloced_array<Field_blob *, 16, true>
    blob_update_field_set(PSI_NOT_INSTRUMENTED);
  if (blob_update_field_set.reserve(fields.elements))
    DBUG_RETURN(true);

  List_iterator_fast<Item> f(fields);
  Item *fld;
  while ((fld= f++))
  {
    Item_field *field= fld->field_for_view_update();
    Field *lhs_field= field->field;

    if (lhs_field->type() == MYSQL_TYPE_BLOB ||
        lhs_field->type() == MYSQL_TYPE_GEOMETRY)
      blob_update_field_set.insert_unique(down_cast<Field_blob *>(lhs_field));
  }

  // Traverse through thd->lex->insert_update_values_map
  // and make copy of BLOB values in RHS_FIELD, if the same field is
  // modified (present in above 'set' prepared).
  if (thd->lex->has_values_map())
  {
    std::map<Field *, Field *>::iterator iter;
    for(iter= thd->lex->begin_values_map();
        iter != thd->lex->end_values_map();
        ++iter)
    {
      // Retrieve the Field_blob pointers from the map.
      // and initialize newly declared variables immediately.
      Field_blob *lhs_field= down_cast<Field_blob *>(iter->first);
      Field_blob *rhs_field= down_cast<Field_blob *>(iter->second);

      // Check if the Field_blob object is updated before making a copy.
      if (blob_update_field_set.count_unique(lhs_field) == 0)
        continue;

      // Copy blob value
      if(rhs_field->copy_blob_value(mem_root))
        DBUG_RETURN(true);
    }
  }

  DBUG_RETURN(false);
}

/**
  INSERT statement implementation

  @note Like implementations of other DDL/DML in MySQL, this function
  relies on the caller to close the thread tables. This is done in the
  end of dispatch_command().
*/

bool Sql_cmd_insert::mysql_insert(THD *thd,TABLE_LIST *table_list)
{
  DBUG_ENTER("mysql_insert");

  LEX *const lex= thd->lex;
  int error, res;
  bool err= true;
  bool transactional_table, joins_freed= FALSE;
  bool changed;
  bool is_locked= false;
  ulong counter= 0;
  ulonglong id;
  /*
    We have three alternative syntax rules for the INSERT statement:
    1) "INSERT (columns) VALUES ...", so non-listed columns need a default
    2) "INSERT VALUES (), ..." so all columns need a default;
    note that "VALUES (),(expr_1, ..., expr_n)" is not allowed, so checking
    emptiness of the first row is enough
    3) "INSERT VALUES (expr_1, ...), ..." so no defaults are needed; even if
    expr_i is "DEFAULT" (in which case the column is set by
    Item_default_value::save_in_field_inner()).
  */
  const bool manage_defaults=
    insert_field_list.elements != 0 ||          // 1)
    insert_many_values.head()->elements == 0;   // 2)
  COPY_INFO info(COPY_INFO::INSERT_OPERATION,
                 &insert_field_list,
                 manage_defaults,
                 duplicates);
  COPY_INFO update(COPY_INFO::UPDATE_OPERATION, &insert_update_list,
                   &insert_value_list);
  Name_resolution_context *context;
  Name_resolution_context_state ctx_state;
  uint num_partitions= 0;
  enum partition_info::enum_can_prune can_prune_partitions=
                                                  partition_info::PRUNE_NO;
  MY_BITMAP used_partitions;
  bool prune_needs_default_values;

  SELECT_LEX *const select_lex= lex->select_lex;

  select_lex->make_active_options(0, 0);

  if (open_tables_for_query(thd, table_list, 0))
    DBUG_RETURN(true);

  if (run_before_dml_hook(thd))
    DBUG_RETURN(true);

  THD_STAGE_INFO(thd, stage_init);
  lex->used_tables=0;

  List_iterator_fast<List_item> its(insert_many_values);
  List_item *values= its++;
  const uint value_count= values->elements;
  TABLE      *insert_table= NULL;
  if (mysql_prepare_insert(thd, table_list, values, false))
    goto exit_without_my_ok;

  insert_table= lex->insert_table_leaf->table;

  if (duplicates == DUP_UPDATE || duplicates == DUP_REPLACE)
    prepare_for_positional_update(insert_table, table_list);

  /* Must be done before can_prune_insert, due to internal initialization. */
  if (info.add_function_default_columns(insert_table, insert_table->write_set))
    goto exit_without_my_ok; /* purecov: inspected */
  if (duplicates == DUP_UPDATE &&
      update.add_function_default_columns(insert_table,
                                          insert_table->write_set))
    goto exit_without_my_ok; /* purecov: inspected */

  context= &select_lex->context;
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
  DBUG_ASSERT(table_list->next_local == 0);
  context->resolve_in_table_list_only(table_list);

  if (!is_locked && insert_table->part_info)
  {
    if (insert_table->part_info->can_prune_insert(thd,
                                           duplicates,
                                           update,
                                           insert_update_list,
                                           insert_field_list,
                                           !MY_TEST(values->elements),
                                           &can_prune_partitions,
                                           &prune_needs_default_values,
                                           &used_partitions))
      goto exit_without_my_ok; /* purecov: inspected */

    if (can_prune_partitions != partition_info::PRUNE_NO)
    {
      num_partitions= insert_table->part_info->lock_partitions.n_bits;
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
      if (insert_table->part_info->set_used_partition(insert_field_list,
                                               *values,
                                               info,
                                               prune_needs_default_values,
                                               &used_partitions))
        can_prune_partitions= partition_info::PRUNE_NO;
    }
  }

  its.rewind();
  while ((values= its++))
  {
    counter++;
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
      if (insert_table->part_info->set_used_partition(insert_field_list,
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
  }
  insert_table->auto_increment_field_not_null= false;
  its.rewind ();
 
  /* Restore the current context. */
  ctx_state.restore_state(context, table_list);

  { // Statement plan is available within these braces
  Modification_plan plan(thd,
                         (lex->sql_command == SQLCOM_INSERT) ?
                         MT_INSERT : MT_REPLACE, insert_table,
                         NULL, false, 0);
  DEBUG_SYNC(thd, "planned_single_insert");

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
    bitmap_intersect(&insert_table->part_info->read_partitions,
                     &used_partitions);
    bitmap_intersect(&insert_table->part_info->lock_partitions,
                     &used_partitions);
  }

  // Lock the tables now if not locked already.
  if (!is_locked &&
      lock_tables(thd, table_list, lex->table_count, 0))
    DBUG_RETURN(true);
 
  if (lex->describe)
  {
    err= explain_single_table_modification(thd, &plan, select_lex);
    goto exit_without_my_ok;
  }

  /*
    Count warnings for all inserts.
    For single line insert, generate an error if try to set a NOT NULL field
    to NULL.
  */
  thd->count_cuted_fields= ((insert_many_values.elements == 1 &&
                             !lex->is_ignore()) ?
                            CHECK_FIELD_ERROR_FOR_NULL :
                            CHECK_FIELD_WARN);
  thd->cuted_fields = 0L;
  insert_table->next_number_field= insert_table->found_next_number_field;

#ifdef HAVE_REPLICATION
    if (thd->slave_thread)
    {
      /* Get SQL thread's rli, even for a slave worker thread */
      Relay_log_info* c_rli= thd->rli_slave->get_c_rli();
      DBUG_ASSERT(c_rli != NULL);
      if(info.get_duplicate_handling() == DUP_UPDATE &&
         insert_table->next_number_field != NULL &&
         rpl_master_has_bug(c_rli, 24432, TRUE, NULL, NULL))
        goto exit_without_my_ok;
    }
#endif

  error=0;
  THD_STAGE_INFO(thd, stage_update);
  if (duplicates == DUP_REPLACE &&
      (!insert_table->triggers ||
       !insert_table->triggers->has_delete_triggers()))
    insert_table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
  if (duplicates == DUP_UPDATE)
    insert_table->file->extra(HA_EXTRA_INSERT_WITH_UPDATE);
  /*
    let's *try* to start bulk inserts. It won't necessary
    start them as insert_many_values.elements should be greater than
    some - handler dependent - threshold.
    We should not start bulk inserts if this statement uses
    functions or invokes triggers since they may access
    to the same table and therefore should not see its
    inconsistent state created by this optimization.
    So we call start_bulk_insert to perform nesessary checks on
    insert_many_values.elements, and - if nothing else - to initialize
    the code to make the call of end_bulk_insert() below safe.
  */
  if (duplicates != DUP_ERROR || lex->is_ignore())
    insert_table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  /**
     This is a simple check for the case when the table has a trigger
     that reads from it, or when the statement invokes a stored function
     that reads from the table being inserted to.
     Engines can't handle a bulk insert in parallel with a read form the
     same table in the same connection.
  */
  if (thd->locked_tables_mode <= LTM_LOCK_TABLES)
    insert_table->file->ha_start_bulk_insert(insert_many_values.elements);

  prepare_triggers_for_insert_stmt(insert_table);

  for (Field** next_field= insert_table->field; *next_field; ++next_field)
  {
    (*next_field)->reset_warnings();
  }

  while ((values= its++))
  {
    if (insert_field_list.elements || !value_count)
    {
      restore_record(insert_table, s->default_values);  // Get empty record

      /*
        Check whether default values of the insert_field_list not specified in
        column list are correct or not.
      */
      if (validate_default_values_of_unset_fields(thd, insert_table))
      {
        error= 1;
        break;
      }
      if (fill_record_n_invoke_before_triggers(thd, &info, insert_field_list,
                                               *values, insert_table,
                                               TRG_EVENT_INSERT,
                                               insert_table->s->fields))
      {
        DBUG_ASSERT(thd->is_error());
        /*
          TODO: Convert warnings to errors if values_list.elements == 1
          and check that all items return warning in case of problem with
          storing field.
        */
        error= 1;
        break;
      }

      res= check_that_all_fields_are_given_values(thd, insert_table,
                                                  table_list);
      if (res)
      {
        DBUG_ASSERT(thd->is_error());
        error= 1;
        break;
      }
    }
    else
    {
      if (lex->used_tables)               // Column used in values()
        restore_record(insert_table, s->default_values); // Get empty record
      else
      {
        TABLE_SHARE *share= insert_table->s;

        /*
          Fix delete marker. No need to restore rest of record since it will
          be overwritten by fill_record() anyway (and fill_record() does not
          use default values in this case).
        */
        insert_table->record[0][0]= share->default_values[0];

        /* Fix undefined null_bits. */
        if (share->null_bytes > 1 && share->last_null_bit_pos)
        {
          insert_table->record[0][share->null_bytes - 1]=
            share->default_values[share->null_bytes - 1];
        }
      }
      if (fill_record_n_invoke_before_triggers(thd, insert_table->field,
                                               *values, insert_table,
                                               TRG_EVENT_INSERT,
                                               insert_table->s->fields))
      {
        DBUG_ASSERT(thd->is_error());
        error= 1;
        break;
      }
    }

    if ((res= table_list->view_check_option(thd)) == VIEW_CHECK_SKIP)
      continue;
    else if (res == VIEW_CHECK_ERROR)
    {
      error= 1;
      break;
    }
    error= write_record(thd, insert_table, &info, &update);
    if (error)
      break;
    thd->get_stmt_da()->inc_current_row_for_condition();
  }
  } // Statement plan is available within these braces

  error= thd->get_stmt_da()->is_error();
  free_underlaid_joins(thd, select_lex);
  joins_freed= true;

  /*
    Now all rows are inserted.  Time to update logs and sends response to
    user
  */
  {
    /* TODO: Only call this if insert_table->found_next_number_field.*/
    insert_table->file->ha_release_auto_increment();
    /*
      Make sure 'end_bulk_insert()' is called regardless of current error
    */
    int loc_error= 0;
    if (thd->locked_tables_mode <= LTM_LOCK_TABLES)
      loc_error= insert_table->file->ha_end_bulk_insert();
    /*
      Report error if 'end_bulk_insert()' failed, and set 'error' to 1
    */
    if (loc_error && !error)
    {
      /* purecov: begin inspected */
      myf error_flags= MYF(0);
      if (insert_table->file->is_fatal_error(loc_error))
        error_flags|= ME_FATALERROR;

      insert_table->file->print_error(loc_error, error_flags);
      error= 1;
      /* purecov: end */
    }
    if (duplicates != DUP_ERROR || lex->is_ignore())
      insert_table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

    transactional_table= insert_table->file->has_transactions();

    if ((changed= (info.stats.copied || info.stats.deleted || info.stats.updated)))
    {
      /*
        Invalidate the table in the query cache if something changed.
        For the transactional algorithm to work the invalidation must be
        before binlog writing and ha_autocommit_or_rollback
      */
      query_cache.invalidate_single(thd, lex->insert_table_leaf, true);
      DEBUG_SYNC(thd, "wait_after_query_cache_invalidate");
    }

    if (error <= 0 || thd->get_transaction()->cannot_safely_rollback(
        Transaction_ctx::STMT))
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
                              thd->query().str, thd->query().length,
			           transactional_table, FALSE, FALSE,
                                   errcode))
	  error= 1;
      }
    }
    DBUG_ASSERT(transactional_table || !changed || 
                thd->get_transaction()->cannot_safely_rollback(
                  Transaction_ctx::STMT));
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
     ((insert_table->next_number_field && info.stats.copied) ?
     insert_table->next_number_field->val_int() : 0));
  insert_table->next_number_field= 0;
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  insert_table->auto_increment_field_not_null= FALSE;
  if (duplicates == DUP_REPLACE &&
      (!insert_table->triggers ||
       !insert_table->triggers->has_delete_triggers()))
    insert_table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);

  if (thd->is_error())
    goto exit_without_my_ok;

  if (insert_many_values.elements == 1 &&
      (!(thd->variables.option_bits & OPTION_WARNINGS) || !thd->cuted_fields))
  {
    my_ok(thd, info.stats.copied + info.stats.deleted +
          (thd->get_protocol()->has_client_capability(CLIENT_FOUND_ROWS) ?
           info.stats.touched : info.stats.updated),
          id);
  }
  else
  {
    char buff[160];
    ha_rows updated=
      thd->get_protocol()->has_client_capability(CLIENT_FOUND_ROWS) ?
        info.stats.touched : info.stats.updated;
    if (lex->is_ignore())
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
  DBUG_RETURN(FALSE);

exit_without_my_ok:
  thd->lex->clear_values_map();
  if (!joins_freed)
    free_underlaid_joins(thd, select_lex);
  DBUG_RETURN(err);
}


/**
  Additional check for insertability for VIEW

  A view is insertable if the following conditions are true:
  - All columns being inserted into are from a single table.
  - All not used columns in table have default values.
  - All columns in view are distinct (not referring to the same column).

  @param thd              thread handler
  @param[in,out] view     reference to view being inserted into.
                          view->contain_auto_increment is true if and only if
                          the view contains an auto_increment field.
  @param insert_table_ref reference to underlying table being inserted into

  @return false if success, true if error
*/

static bool check_view_insertability(THD *thd, TABLE_LIST *view,
                                     const TABLE_LIST *insert_table_ref)
 {
  DBUG_ENTER("check_view_insertability");

  const uint num= view->view_query()->select_lex->item_list.elements;
  TABLE *const table= insert_table_ref->table;
  MY_BITMAP used_fields;
  enum_mark_columns save_mark_used_columns= thd->mark_used_columns;

  const uint used_fields_buff_size= bitmap_buffer_size(table->s->fields);
  uint32 *const used_fields_buff= (uint32*)thd->alloc(used_fields_buff_size);
  if (!used_fields_buff)
    DBUG_RETURN(true);                      /* purecov: inspected */

  DBUG_ASSERT(view->table == NULL &&
              table != NULL &&
              view->field_translation != 0);

  (void) bitmap_init(&used_fields, used_fields_buff, table->s->fields, 0);
  bitmap_clear_all(&used_fields);

  view->contain_auto_increment= false;

  thd->mark_used_columns= MARK_COLUMNS_NONE;

  // No privilege checking is done for these columns
  Column_privilege_tracker column_privilege(thd, 0);

  /* check simplicity and prepare unique test of view */
  Field_translator *const trans_start= view->field_translation;
  Field_translator *const trans_end= trans_start + num;

  for (Field_translator *trans= trans_start; trans != trans_end; trans++)
  {
    if (trans->item == NULL)
      continue;
    /*
      @todo
      This fix_fields() call is necessary for execution of prepared statements.
      When repeated preparation is eliminated the call can be deleted.
    */
    if (!trans->item->fixed && trans->item->fix_fields(thd, &trans->item))
      DBUG_RETURN(true);  /* purecov: inspected */

    Item_field *field;
    /* simple SELECT list entry (field without expression) */
    if (!(field= trans->item->field_for_view_update()))
      DBUG_RETURN(true);

    if (field->field->unireg_check == Field::NEXT_NUMBER)
      view->contain_auto_increment= true;
    /* prepare unique test */
    /*
      remove collation (or other transparent for update function) if we have
      it
    */
    trans->item= field;
  }
  thd->mark_used_columns= save_mark_used_columns;

  /* unique test */
  for (Field_translator *trans= trans_start; trans != trans_end; trans++)
  {
    if (trans->item == NULL)
      continue;
    /* Thanks to test above, we know that all columns are of type Item_field */
    Item_field *field= (Item_field *)trans->item;
    /* check fields belong to table in which we are inserting */
    if (field->field->table == table &&
        bitmap_fast_test_and_set(&used_fields, field->field->field_index))
      DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


/**
  Recursive helper function for resolving join conditions for
  insertion into view for prepared statements.

  @param thd      Thread handler
  @param tr       Table structure which is traversed recursively

  @return false if success, true if error
*/
static bool fix_join_cond_for_insert(THD *thd, TABLE_LIST *tr)
{
  if (tr->join_cond() && !tr->join_cond()->fixed)
  {
    Column_privilege_tracker column_privilege(thd, SELECT_ACL);

    if (tr->join_cond()->fix_fields(thd, NULL))
      return true;             /* purecov: inspected */
  }

  if (tr->nested_join == NULL)
    return false;

  List_iterator<TABLE_LIST> li(tr->nested_join->join_list);
  TABLE_LIST *ti;

  while ((ti= li++))
  {
    if (fix_join_cond_for_insert(thd, ti))
      return true;             /* purecov: inspected */
  }
  return false;
}

/**
  Check if table can be updated

  @param thd           Thread handle
  @param table_list    Table reference
  @param fields        List of fields to be inserted
  @param select_insert True if processing INSERT ... SELECT statement

  @return false if success, true if error
*/

bool
Sql_cmd_insert_base::mysql_prepare_insert_check_table(THD *thd,
                                                      TABLE_LIST *table_list,
                                                      List<Item> &fields,
                                                      bool select_insert)
{
  DBUG_ENTER("mysql_prepare_insert_check_table");

  SELECT_LEX *const select= thd->lex->select_lex;
  const bool insert_into_view= table_list->is_view();

  if (select->setup_tables(thd, table_list, select_insert))
    DBUG_RETURN(true);             /* purecov: inspected */

  if (insert_into_view)
  {
    // Allowing semi-join would transform this table into a "join view"
    if (table_list->resolve_derived(thd, false))
      DBUG_RETURN(true);

    if (select->merge_derived(thd, table_list))
      DBUG_RETURN(true);           /* purecov: inspected */

    /*
      On second preparation, we may need to resolve view condition generated
      when merging the view.
    */
    if (!select->first_execution && table_list->is_merged() &&
        fix_join_cond_for_insert(thd, table_list))
      DBUG_RETURN(true);           /* purecov: inspected */
  }

  if (!table_list->is_insertable())
  {
    my_error(ER_NON_INSERTABLE_TABLE, MYF(0), table_list->alias, "INSERT");
    DBUG_RETURN(true);
  }

  // Allow semi-join for selected tables containing subqueries
  if (select->derived_table_count && select->resolve_derived(thd, true))
    DBUG_RETURN(true);

  /*
    First table in list is the one being inserted into, requires INSERT_ACL.
    All other tables require SELECT_ACL only.
  */
  if (select->derived_table_count &&
      select->check_view_privileges(thd, INSERT_ACL, SELECT_ACL))
    DBUG_RETURN(true);

  // Precompute and store the row types of NATURAL/USING joins.
  if (setup_natural_join_row_types(thd, select->join_list, &select->context))
    DBUG_RETURN(true);

  if (insert_into_view && !fields.elements)
  {
    empty_field_list_on_rset= true;
    if (table_list->is_multiple_tables())
    {
      my_error(ER_VIEW_NO_INSERT_FIELD_LIST, MYF(0),
               table_list->view_db.str, table_list->view_name.str);
      DBUG_RETURN(true);
    }
    if (insert_view_fields(thd, &fields, table_list))
      DBUG_RETURN(true);
    /*
       Item_fields inserted above from field_translation list have been
       already fixed in resolved_derived(), thus setup_fields() in
       check_insert_fields() will not process them, not mark them in write_set;
       we have to do it:
    */
    bitmap_set_all(table_list->updatable_base_table()->table->write_set);
  }

  DBUG_RETURN(false);
}


/**
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

  DBUG_ASSERT(tables->is_view());
  List_iterator<TABLE_LIST> it(*tables->view_tables);
  TABLE_LIST *tbl;
  while ((tbl= it++))
    prepare_for_positional_update(tbl->table, tbl);

  return;
}


/**
  Prepare items in INSERT statement

  @param thd                   Thread handler
  @param table_list            Global/local table list
  @param values                List of values to be inserted
  @param duplic                What to do on duplicate key error
  @param where                 Where clause (for insert ... select)
  @param select_insert         TRUE if INSERT ... SELECT statement

  @todo (in far future)
    In cases of:
    INSERT INTO t1 SELECT a, sum(a) as sum1 from t2 GROUP BY a
    ON DUPLICATE KEY ...
    we should be able to refer to sum1 in the ON DUPLICATE KEY part

  WARNING
    You MUST set table->insert_values to 0 after calling this function
    before releasing the table object.
  
  @return false if success, true if error
*/

bool Sql_cmd_insert_base::mysql_prepare_insert(THD *thd, TABLE_LIST *table_list,
                                               List_item *values,
                                               bool select_insert)
{
  DBUG_ENTER("mysql_prepare_insert");

  // INSERT should have a SELECT or VALUES clause
  DBUG_ASSERT (!select_insert || !values);

  // Number of update fields must match number of update values
  DBUG_ASSERT(insert_update_list.elements == insert_value_list.elements);

  LEX * const lex= thd->lex;
  SELECT_LEX *const select_lex= lex->select_lex;
  Name_resolution_context *const context= &select_lex->context;
  Name_resolution_context_state ctx_state;
  const bool insert_into_view= table_list->is_view();
  bool res= false;

  DBUG_PRINT("enter", ("table_list 0x%lx, view %d",
                       (ulong)table_list,
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

  if (mysql_prepare_insert_check_table(thd, table_list, insert_field_list,
                                       select_insert))
    DBUG_RETURN(true);

  // REPLACE for a JOIN view is not permitted.
  if (table_list->is_multiple_tables() && duplicates == DUP_REPLACE)
  {
    my_error(ER_VIEW_DELETE_MERGE_VIEW, MYF(0),
             table_list->view_db.str, table_list->view_name.str);
    DBUG_RETURN(true);
  }

  if (duplicates == DUP_UPDATE)
  {
    /* it should be allocated before Item::fix_fields() */
    if (table_list->set_insert_values(thd->mem_root))
      DBUG_RETURN(true);                       /* purecov: inspected */
  }

  // Save the state of the current name resolution context.
  ctx_state.save_state(context, table_list);

  // Prepare the lists of columns and values in the statement.
  if (values)
  {
    // if we have INSERT ... VALUES () we cannot have a GROUP BY clause
    DBUG_ASSERT (!select_lex->group_list.elements);

    /*
      Perform name resolution only in the first table - 'table_list',
      which is the table that is inserted into.
     */
    DBUG_ASSERT(table_list->next_local == NULL);
    table_list->next_local= NULL;
    context->resolve_in_table_list_only(table_list);

    if (!res)
      res= check_insert_fields(thd, context->table_list, insert_field_list,
                               values->elements, true, !insert_into_view);
    table_map map= 0;
    if (!res)
      map= lex->insert_table_leaf->map();

    // values is reset here to cover all the rows in the VALUES-list.
    List_iterator_fast<List_item> its(insert_many_values);

    // Check whether all rows have the same number of fields.
    const uint value_count= values->elements;
    ulong counter= 0;
    while ((values= its++))
    {
      counter++;
      if (values->elements != value_count)
      {
        my_error(ER_WRONG_VALUE_COUNT_ON_ROW, MYF(0), counter);
        DBUG_RETURN(true);
      }

      if (!res)
        res= setup_fields(thd, Ref_ptr_array(), *values, SELECT_ACL, NULL,
                          false, false);
      if (!res)
        res= check_valid_table_refs(table_list, *values, map);

      if (!res && lex->insert_table_leaf->table->has_gcol())
        res= validate_gc_assignment(thd, &insert_field_list, values,
                                    lex->insert_table_leaf->table);
    }
    its.rewind();
    values= its++;

    if (!res && duplicates == DUP_UPDATE)
    {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      table_list->set_want_privilege(UPDATE_ACL);
#endif
      // Setup the columns to be updated
      res= setup_fields(thd, Ref_ptr_array(),
                        insert_update_list, UPDATE_ACL, NULL, false, true);
      if (!res)
        res= check_valid_table_refs(table_list, insert_update_list, map);

      // Setup the corresponding values
      thd->lex->in_update_value_clause= true;
      if (!res)
        res= setup_fields(thd, Ref_ptr_array(), insert_value_list, SELECT_ACL,
                          NULL, false, false);
      thd->lex->in_update_value_clause= false;

      if (!res)
        res= check_valid_table_refs(table_list, insert_value_list, map);

      if (!res && lex->insert_table_leaf->table->has_gcol())
        res= validate_gc_assignment(thd, &insert_update_list,
                                    &insert_value_list,
                                    lex->insert_table_leaf->table);
    }
  }
  else if (thd->stmt_arena->is_stmt_prepare())
  {
    /*
      This section of code is more or less a duplicate of the code  in
      Query_result_insert::prepare, and the 'if' branch above.
      @todo Consolidate these three sections into one.
    */
    /*
      Perform name resolution only in the first table - 'table_list',
      which is the table that is inserted into.
     */
    table_list->next_local= NULL;
    thd->dup_field= NULL;
    context->resolve_in_table_list_only(table_list);

    /*
      When processing a prepared INSERT ... SELECT statement,
      mysql_prepare_insert() is called from
      mysql_insert_select_prepare_tester(), when the values list (aka the
      SELECT list from the SELECT) is not resolved yet, so pass "false"
      for value_count_known.
    */
    res= check_insert_fields(thd, context->table_list, insert_field_list, 0,
                             false, !insert_into_view);
    table_map map= 0;
    if (!res)
      map= lex->insert_table_leaf->map();

    if (!res && lex->insert_table_leaf->table->vfield)
      res= validate_gc_assignment(thd, &insert_field_list, values,
                                  lex->insert_table_leaf->table);

    if (!res && duplicates == DUP_UPDATE)
    {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      table_list->set_want_privilege(UPDATE_ACL);
#endif
      // Setup the columns to be modified
      res= setup_fields(thd, Ref_ptr_array(),
                        insert_update_list, UPDATE_ACL, NULL, false, true);
      if (!res)
        res= check_valid_table_refs(table_list, insert_update_list, map);

      if (!res && lex->insert_table_leaf->table->vfield)
        res= validate_gc_assignment(thd, &insert_update_list,
                                    &insert_value_list,
                                    lex->insert_table_leaf->table);
      DBUG_ASSERT(!table_list->next_name_resolution_table);
      if (select_lex->group_list.elements == 0 && !select_lex->with_sum_func)
      {
        /*
          There are two separata name resolution contexts:
          the INSERT table and the tables in the SELECT expression 
          Make a single context out of them by concatenating the lists:
        */  
        table_list->next_name_resolution_table= 
          ctx_state.get_first_name_resolution_table();
      }
      thd->lex->in_update_value_clause= true;
      if (!res)
        res= setup_fields(thd, Ref_ptr_array(), insert_value_list,
                          SELECT_ACL, NULL, false, false);
      thd->lex->in_update_value_clause= false;

      /*
        Notice that there is no need to apply the Item::update_value_transformer
        here, as this will be done during EXECUTE in
        Query_result_insert::prepare().
      */
    }
  }

  // Restore the current name resolution context
  ctx_state.restore_state(context, table_list);

  if (res)
    DBUG_RETURN(res);

  if (!select_insert)
  {
    TABLE_LIST *const duplicate=
      unique_table(thd, lex->insert_table_leaf, table_list->next_global, true);
    if (duplicate)
    {
      update_non_unique_table_error(table_list, "INSERT", duplicate);
      DBUG_RETURN(true);
    }
  }

  if (table_list->is_merged())
  {
    Column_privilege_tracker column_privilege(thd, SELECT_ACL);

    if (table_list->prepare_check_option(thd))
      DBUG_RETURN(true);

    if (duplicates == DUP_REPLACE &&
        table_list->prepare_replace_filter(thd))
      DBUG_RETURN(true);
  }

  if (!select_insert && select_lex->apply_local_transforms(thd, false))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
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
  MEM_ROOT mem_root;
  DBUG_ENTER("write_record");

  /* Here we are using separate MEM_ROOT as this memory should be freed once we
     exit write_record() function. This is marked as not instumented as it is
     allocated for very short time in a very specific case.
  */
  init_sql_alloc(PSI_NOT_INSTRUMENTED, &mem_root, 256, 0);
  info->stats.records++;
  save_read_set=  table->read_set;
  save_write_set= table->write_set;

  info->set_function_defaults(table);

  const enum_duplicates duplicate_handling= info->get_duplicate_handling();

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
      if (!table->file->is_ignorable_error(error))
	goto err;
      is_duplicate_key_error= (error == HA_ERR_FOUND_DUPP_KEY ||
                               error == HA_ERR_FOUND_DUPP_UNIQUE);
      if (!is_duplicate_key_error)
      {
        /*
          We come here when we had an ignorable error which is not a duplicate
          key error. In this we ignore error if ignore flag is set, otherwise
          report error as usual. We will not do any duplicate key processing.
        */
         info->last_errno= error;
         table->file->print_error(error, MYF(0));
         /*
           If IGNORE option is used, handler errors will be downgraded
           to warnings and don't have to stop the iteration.
         */
         if (thd->is_error())
           goto before_trg_err;
         goto ok_or_after_trg_err; /* Ignoring a not fatal error, return 0 */
      }
      if ((int) (key_nr = table->file->get_dup_key(error)) < 0)
      {
	error= HA_ERR_FOUND_DUPP_KEY;         /* Database can't find key */
	goto err;
      }
      /*
        key index value is either valid in the range [0-MAX_KEY) or
        has value MAX_KEY as a marker for the case when no information
        about key can be found. In the last case we have to require
        that storage engine has the flag HA_DUPLICATE_POS turned on.
        If this invariant is false then DBUG_ASSERT will crash
        the server built in debug mode. For the server that was built
        without DEBUG we have additional check for the value of key_nr
        in the code below in order to report about error in any case.
      */
      DBUG_ASSERT(key_nr != MAX_KEY ||
                  (key_nr == MAX_KEY &&
                   (table->file->ha_table_flags() & HA_DUPLICATE_POS)));

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
      /*
        If the key index is equal to MAX_KEY it's treated as unknown key case
        and we shouldn't try to locate key info.
      */
      else if (key_nr < MAX_KEY)
      {
	if (table->file->extra(HA_EXTRA_FLUSH_CACHE)) /* Not needed with NISAM */
	{
	  error=my_errno();
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
      else
      {
        /*
          For the server built in non-debug mode returns error if
          handler::get_dup_key() returned MAX_KEY as the value of key index.
        */
        error= HA_ERR_FOUND_DUPP_KEY;         /* Database can't find key */
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
        /*
          Special check for BLOB/GEOMETRY field in statements with
          "ON DUPLICATE KEY UPDATE" clause.
          See mysql_prepare_blob_values() function for more details.
        */
        if (mysql_prepare_blob_values(thd,
                                      *update->get_changed_columns(),
                                      &mem_root))
           goto before_trg_err;
        restore_record(table,record[1]);
        DBUG_ASSERT(update->get_changed_columns()->elements ==
                    update->update_values->elements);
        if (fill_record_n_invoke_before_triggers(thd, update,
                                                 *update->get_changed_columns(),
                                                 *update->update_values,
                                                 table, TRG_EVENT_UPDATE, 0))
          goto before_trg_err;

        bool insert_id_consumed= false;
        if (// UPDATE clause specifies a value for the auto increment field
            table->auto_increment_field_not_null &&
            // An auto increment value has been generated for this row
            (insert_id_for_cur_row > 0))
        {
          // After-update value:
          const ulonglong auto_incr_val= table->next_number_field->val_int();
          if (auto_incr_val == insert_id_for_cur_row)
          {
            // UPDATE wants to use the generated value
            insert_id_consumed= true;
          }
          else if (table->file->auto_inc_interval_for_cur_row.
                   in_range(auto_incr_val))
          {
            /*
              UPDATE wants to use one auto generated value which we have already
              reserved for another (previous or following) row. That may cause
              a duplicate key error if we later try to insert the reserved
              value. Such conflicts on auto generated values would be strange
              behavior, so we return a clear error now.
            */
            my_error(ER_AUTO_INCREMENT_CONFLICT, MYF(0));
	    goto before_trg_err;
          }
        }

        if (!insert_id_consumed)
          table->file->restore_auto_increment(prev_insert_id);

        /* CHECK OPTION for VIEW ... ON DUPLICATE KEY UPDATE ... */
        {
          const TABLE_LIST *inserted_view=
            table->pos_in_table_list->belong_to_view;
          if (inserted_view != NULL)
          {
            res= inserted_view->view_check_option(thd);
            if (res == VIEW_CHECK_SKIP)
              goto ok_or_after_trg_err;
            if (res == VIEW_CHECK_ERROR)
              goto before_trg_err;
          }
        }

        info->stats.touched++;
        if (!records_are_comparable(table) || compare_records(table))
        {
          // Handle the INSERT ON DUPLICATE KEY UPDATE operation
          update->set_function_defaults(table);

          if ((error=table->file->ha_update_row(table->record[1],
                                                table->record[0])) &&
              error != HA_ERR_RECORD_IS_THE_SAME)
          {
             info->last_errno= error;
             myf error_flags= MYF(0);
             if (table->file->is_fatal_error(error))
               error_flags|= ME_FATALERROR;
             table->file->print_error(error, error_flags);
             /*
               If IGNORE option is used, handler errors will be downgraded
               to warnings and don't  have to stop the iteration.
             */
             if (thd->is_error())
               goto before_trg_err;
             goto ok_or_after_trg_err; /* Ignoring a not fatal error, return 0 */
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
          info->stats.copied++;
        }

        // Execute the 'AFTER, ON UPDATE' trigger
        trg_error= (table->triggers &&
                    table->triggers->process_triggers(thd, TRG_EVENT_UPDATE,
                                                      TRG_ACTION_AFTER, TRUE));
        goto ok_or_after_trg_err;
      }
      else /* DUP_REPLACE */
      {
        TABLE_LIST *view= table->pos_in_table_list->belong_to_view;

        if (view && view->replace_filter)
        {
          const size_t record_length= table->s->reclength;

          void *record0_saved= my_malloc(PSI_NOT_INSTRUMENTED, record_length,
                                         MYF(MY_WME));

          if (!record0_saved)
          {
            error= ENOMEM;
            goto err;
          }

          // Save the record used for comparison.
          memcpy(record0_saved, table->record[0], record_length);

          // Preparing the record for comparison.
          memcpy(table->record[0], table->record[1], record_length);

          // Checking if the row being conflicted is visible by the view.
          bool found_row_in_view= view->replace_filter->val_int();

          // Restoring the record back.
          memcpy(table->record[0], record0_saved, record_length);

          my_free(record0_saved);

          if (!found_row_in_view)
          {
            my_error(ER_REPLACE_INACCESSIBLE_ROWS, MYF(0));
            goto err;
          }
        }

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
            thd->get_transaction()->mark_modified_non_trans_table(
              Transaction_ctx::STMT);
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
    info->last_errno= error;
    myf error_flags= MYF(0);
    if (table->file->is_fatal_error(error))
      error_flags|= ME_FATALERROR;
    table->file->print_error(error, error_flags);
    /*
      If IGNORE option is used, handler errors will be downgraded
      to warnings and don't  have to stop the iteration.
    */
    if (thd->is_error())
      goto before_trg_err;
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
    thd->get_transaction()->mark_modified_non_trans_table(
      Transaction_ctx::STMT);
  free_root(&mem_root, MYF(0));
  DBUG_RETURN(trg_error);

err:
  {
    myf error_flags= MYF(0);                      /**< Flag for fatal errors */
    info->last_errno= error;
    DBUG_ASSERT(thd->lex->current_select() != NULL);
    if (table->file->is_fatal_error(error))
      error_flags|= ME_FATALERROR;

    table->file->print_error(error, error_flags);
  }

before_trg_err:
  table->file->restore_auto_increment(prev_insert_id);
  if (key)
    my_safe_afree(key, table->s->max_unique_length, MAX_KEY_LENGTH);
  table->column_bitmaps_set(save_read_set, save_write_set);
  free_root(&mem_root, MYF(0));
  DBUG_RETURN(1);
}


/******************************************************************************
  Check that all fields with arn't null_fields are used
******************************************************************************/

int check_that_all_fields_are_given_values(THD *thd, TABLE *entry,
                                           TABLE_LIST *table_list)
{
  int err= 0;
  MY_BITMAP *write_set= entry->fields_set_during_insert;

  for (Field **field=entry->field ; *field ; field++)
  {
    if (!bitmap_is_set(write_set, (*field)->field_index) &&
        ((*field)->flags & NO_DEFAULT_VALUE_FLAG) &&
        ((*field)->real_type() != MYSQL_TYPE_ENUM))
    {
      bool view= false;
      if (table_list)
      {
        table_list= table_list->top_table();
        view= table_list->is_view();
      }
      if (view)
        (*field)->set_warning(Sql_condition::SL_WARNING,
                              ER_NO_DEFAULT_FOR_VIEW_FIELD, 1,
                              table_list->view_db.str,
                              table_list->view_name.str);
      else
        (*field)->set_warning(Sql_condition::SL_WARNING,
                              ER_NO_DEFAULT_FOR_FIELD, 1);
      err= 1;
    }
  }
  bitmap_clear_all(write_set);
  return (!thd->lex->is_ignore() && thd->is_strict_mode()) ? err : 0;
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

bool Sql_cmd_insert_select::mysql_insert_select_prepare(THD *thd)
{
  LEX *lex= thd->lex;
  SELECT_LEX *select_lex= lex->select_lex;
  DBUG_ENTER("mysql_insert_select_prepare");

  /*
    SELECT_LEX do not belong to INSERT statement, so we can't add WHERE
    clause if table is VIEW
  */
  if (mysql_prepare_insert(thd, lex->query_tables, NULL, true))
    DBUG_RETURN(true);

  /*
    exclude first table from leaf tables list, because it belong to
    INSERT
  */
  DBUG_ASSERT(select_lex->leaf_tables != NULL);
  DBUG_ASSERT(lex->insert_table == select_lex->leaf_tables->top_table());

  select_lex->leaf_tables= lex->insert_table->next_local;
  if (select_lex->leaf_tables != NULL)
    select_lex->leaf_tables= select_lex->leaf_tables->first_leaf_table();
  select_lex->leaf_table_count-= 
    lex->insert_table->is_view() ? lex->insert_table->leaf_tables_count() : 1;
  DBUG_RETURN(false);
}


int Query_result_insert::prepare(List<Item> &values, SELECT_LEX_UNIT *u)
{
  DBUG_ENTER("Query_result_insert::prepare");

  LEX *const lex= thd->lex;
  bool res;
  SELECT_LEX *const lex_current_select_save= lex->current_select();
  const enum_duplicates duplicate_handling= info.get_duplicate_handling();

  unit= u;

  /*
    Since table in which we are going to insert is added to the first
    select, LEX::current_select() should point to the first select while
    we are fixing fields from insert list.
  */
  lex->set_current_select(lex->select_lex);

  res= check_insert_fields(thd, table_list, *fields, values.elements, true,
                           !insert_into_view);
  if (!res)
    res= setup_fields(thd, Ref_ptr_array(), values, SELECT_ACL, NULL,
                      false, false);

  if (!res && lex->insert_table_leaf->table->has_gcol())
    res= validate_gc_assignment(thd, fields, &values,
                                lex->insert_table_leaf->table);

  if (duplicate_handling == DUP_UPDATE && !res)
  {
    Name_resolution_context *const context= &lex->select_lex->context;
    Name_resolution_context_state ctx_state;

    /* Save the state of the current name resolution context. */
    ctx_state.save_state(context, table_list);

    /* Perform name resolution only in the first table - 'table_list'. */
    table_list->next_local= NULL;
    context->resolve_in_table_list_only(table_list);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
    table_list->set_want_privilege(UPDATE_ACL);
#endif
    if (!res)
      res= setup_fields(thd, Ref_ptr_array(), *update.get_changed_columns(),
                        UPDATE_ACL, NULL, false, true);

    if (!res && lex->insert_table_leaf->table->has_gcol())
      res= validate_gc_assignment(thd, update.get_changed_columns(),
                                  update.update_values,
                                  lex->insert_table_leaf->table);
    /*
      When we are not using GROUP BY and there are no ungrouped aggregate
      functions 
      we can refer to other tables in the ON DUPLICATE KEY part.
      We use next_name_resolution_table destructively, so check it first
      (views?).
    */
    DBUG_ASSERT (!table_list->next_name_resolution_table);
    if (lex->select_lex->group_list.elements == 0 &&
        !lex->select_lex->with_sum_func)
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
    lex->in_update_value_clause= true;
    if (!res)
      res= setup_fields(thd, Ref_ptr_array(), *update.update_values,
                        SELECT_ACL, NULL, false, false);
    lex->in_update_value_clause= false;
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
                        (uchar*)lex->current_select());
      }
    }

    /* Restore the current context. */
    ctx_state.restore_state(context, table_list);
  }

  lex->set_current_select(lex_current_select_save);
  if (res)
    DBUG_RETURN(1);
  /*
    if it is INSERT into join view then check_insert_fields already found
    real table for insert
  */
  table= lex->insert_table_leaf->table;

  if (duplicate_handling == DUP_UPDATE || duplicate_handling == DUP_REPLACE)
    prepare_for_positional_update(table, table_list);

  if (info.add_function_default_columns(table, table->write_set))
    DBUG_RETURN(1);
  if ((duplicate_handling == DUP_UPDATE) &&
      update.add_function_default_columns(table, table->write_set))
    DBUG_RETURN(1);

  /*
    Is table which we are changing used somewhere in other parts of
    query
  */
  if (unique_table(thd, lex->insert_table_leaf, table_list->next_global, 0))
  {
    // Using same table for INSERT and SELECT
    /*
      @todo: Use add_base_options instead of add_active_options, and only
      if first_execution is true; but this can be implemented only when this
      function is called before first_execution is set to true.
      if (lex->current_select()->first_execution)
        lex->current_select()->add_base_options(OPTION_BUFFER_RESULT);
    */
    lex->current_select()->add_active_options(OPTION_BUFFER_RESULT);
  }
  restore_record(table,s->default_values);		// Get empty record
  table->next_number_field=table->found_next_number_field;

#ifdef HAVE_REPLICATION
  if (thd->slave_thread)
  {
    /* Get SQL thread's rli, even for a slave worker thread */
    Relay_log_info *c_rli= thd->rli_slave->get_c_rli();
    DBUG_ASSERT(c_rli != NULL);
    if (duplicate_handling == DUP_UPDATE &&
        table->next_number_field != NULL &&
        rpl_master_has_bug(c_rli, 24432, TRUE, NULL, NULL))
      DBUG_RETURN(1);
  }
#endif

  thd->cuted_fields=0;
  if (thd->lex->is_ignore() || duplicate_handling != DUP_ERROR)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  if (duplicate_handling == DUP_REPLACE &&
      (!table->triggers || !table->triggers->has_delete_triggers()))
    table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
  if (duplicate_handling == DUP_UPDATE)
    table->file->extra(HA_EXTRA_INSERT_WITH_UPDATE);

  if (!res)
  {
     prepare_triggers_for_insert_stmt(table);
  }

  for (Field** next_field= table->field; *next_field; ++next_field)
  {
    (*next_field)->reset_warnings();
    (*next_field)->reset_tmp_null();
  }

  DBUG_RETURN(res ? -1 : 0);
}


/*
  Finish the preparation of the result table.

  SYNOPSIS
    Query_result_insert::prepare2()
    void

  DESCRIPTION
    If the result table is the same as one of the source tables (INSERT SELECT),
    the result table is not finally prepared at the join prepair phase.
    Do the final preparation now.

  RETURN
    0   OK
*/

int Query_result_insert::prepare2()
{
  DBUG_ENTER("Query_result_insert::prepare2");
  if (thd->locked_tables_mode <= LTM_LOCK_TABLES &&
      !thd->lex->describe)
  {
    DBUG_ASSERT(!bulk_insert_started);
    // TODO: Is there no better estimation than 0 == Unknown number of rows?
    table->file->ha_start_bulk_insert((ha_rows) 0);
    bulk_insert_started= true;
  }
  DBUG_RETURN(0);
}


void Query_result_insert::cleanup()
{
  /*
    Query_result_insert/Query_result_create are never re-used
    in prepared statement
  */
  DBUG_ASSERT(0);
}


Query_result_insert::~Query_result_insert()
{
  DBUG_ENTER("~Query_result_insert");
  if (table)
  {
    table->next_number_field=0;
    table->auto_increment_field_not_null= FALSE;
    table->file->ha_reset();
  }
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;
  DBUG_VOID_RETURN;
}


bool Query_result_insert::send_data(List<Item> &values)
{
  DBUG_ENTER("Query_result_insert::send_data");
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
    switch (table_list->view_check_option(thd)) {
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

  DEBUG_SYNC(thd, "create_select_after_write_rows_event");

  if (!error &&
      (table->triggers || info.get_duplicate_handling() == DUP_UPDATE))
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
  if (!error && table->next_number_field)
  {
    /*
      If no value has been autogenerated so far, we need to remember the
      value we just saw, we may need to send it to client in the end.
    */
    if (thd->first_successful_insert_id_in_cur_stmt == 0) // optimization
      autoinc_value_of_last_inserted_row= table->next_number_field->val_int();
    /*
      Clear auto-increment field for the next record, if triggers are used
      we will clear it twice, but this should be cheap.
    */
    table->next_number_field->reset();
  }

  DBUG_RETURN(error);
}


void Query_result_insert::store_values(List<Item> &values)
{
  if (fields->elements)
  {
    restore_record(table, s->default_values);
    if (!validate_default_values_of_unset_fields(thd, table))
      fill_record_n_invoke_before_triggers(thd, &info, *fields, values,
                                           table, TRG_EVENT_INSERT,
                                           table->s->fields);
  }
  else
    fill_record_n_invoke_before_triggers(thd, table->field, values,
                                         table, TRG_EVENT_INSERT,
                                         table->s->fields);

  check_that_all_fields_are_given_values(thd, table, table_list);
}

void Query_result_insert::send_error(uint errcode,const char *err)
{
  DBUG_ENTER("Query_result_insert::send_error");

  my_message(errcode, err, MYF(0));

  DBUG_VOID_RETURN;
}


bool Query_result_insert::send_eof()
{
  int error;
  bool const trans_table= table->file->has_transactions();
  ulonglong id, row_count;
  bool changed;
  THD::killed_state killed_status= thd->killed;
  DBUG_ENTER("Query_result_insert::send_eof");
  DBUG_PRINT("enter", ("trans_table=%d, table_type='%s'",
                       trans_table, table->file->table_type()));

  error= (bulk_insert_started ?
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
    query_cache.invalidate(thd, table, TRUE);
  }

  DBUG_ASSERT(trans_table || !changed || 
              thd->get_transaction()->cannot_safely_rollback(
                Transaction_ctx::STMT));

  /*
    Write to binlog before commiting transaction.  No statement will
    be written by the binlog_query() below in RBR mode.  All the
    events are in the transaction cache and will be written when
    ha_autocommit_or_rollback() is issued below.
  */
  if (mysql_bin_log.is_open() &&
      (!error || thd->get_transaction()->cannot_safely_rollback(
        Transaction_ctx::STMT)))
  {
    int errcode= 0;
    if (!error)
      thd->clear_error();
    else
      errcode= query_error_code(thd, killed_status == THD::NOT_KILLED);
    if (thd->binlog_query(THD::ROW_QUERY_TYPE,
                          thd->query().str, thd->query().length,
                          trans_table, false, false, errcode))
    {
      table->file->ha_release_auto_increment();
      DBUG_RETURN(1);
    }
  }
  table->file->ha_release_auto_increment();

  if (error)
  {
    myf error_flags= MYF(0);
    if (table->file->is_fatal_error(my_errno()))
      error_flags|= ME_FATALERROR;

    table->file->print_error(my_errno(), error_flags);
    DBUG_RETURN(1);
  }

  /*
    For the strict_mode call of push_warning above results to set
    error in Diagnostic_area. Therefore it is necessary to check whether
    the error was set and leave method if it is true. If we didn't do
    so we would failed later when my_ok is called.
  */
  if (thd->get_stmt_da()->is_error())
    DBUG_RETURN(true);

  char buff[160];
  if (thd->lex->is_ignore())
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
             (thd->get_protocol()->has_client_capability(CLIENT_FOUND_ROWS) ?
              info.stats.touched : info.stats.updated);
  id= (thd->first_successful_insert_id_in_cur_stmt > 0) ?
    thd->first_successful_insert_id_in_cur_stmt :
    (thd->arg_of_last_insert_id_function ?
     thd->first_successful_insert_id_in_prev_stmt :
     (info.stats.copied ? autoinc_value_of_last_inserted_row : 0));
  my_ok(thd, row_count, id, buff);
  DBUG_RETURN(0);
}


void Query_result_insert::abort_result_set()
{
  DBUG_ENTER("Query_result_insert::abort_result_set");
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
    if (bulk_insert_started)
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
    if (thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT))
    {
        if (mysql_bin_log.is_open())
        {
          int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
          /* error of writing binary log is ignored */
          (void) thd->binlog_query(THD::ROW_QUERY_TYPE, thd->query().str,
                                   thd->query().length,
                                   transactional_table, FALSE, FALSE, errcode);
        }
	if (changed)
	  query_cache.invalidate(thd, table, TRUE);
    }
    DBUG_ASSERT(transactional_table || !changed ||
		thd->get_transaction()->cannot_safely_rollback(
		  Transaction_ctx::STMT));
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
        MY_TEST(create_info->db_type == myisam_hton ||
                create_info->db_type == heap_hton);
  tmp_table.reset_null_row();

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
                                        NULL,
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

    DBUG_ASSERT(tmp_table_field->gcol_info== NULL && tmp_table_field->stored_in_db);
    Create_field *cr_field= new Create_field(tmp_table_field, table_field);

    if (!cr_field)
      DBUG_RETURN(NULL);

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

int Query_result_create::prepare(List<Item> &values, SELECT_LEX_UNIT *u)
{
  DBUG_ENTER("Query_result_create::prepare");

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
  for (Field **f= field ; *f ; f++)
  {
    if ((*f)->gcol_info)
    {
      /*
        Generated columns are not allowed to be given a value for CREATE TABLE ..
        SELECT statment.
      */
      my_error(ER_NON_DEFAULT_VALUE_FOR_GENERATED_COLUMN, MYF(0),
               (*f)->field_name, (*f)->table->s->table_name.str);
      DBUG_RETURN(true);
    }
  }

  // Turn off function defaults for columns filled from SELECT list:
  const bool retval= info.ignore_last_columns(table, values.elements);
  DBUG_RETURN(retval);
}


/**
  Lock the newly created table and prepare it for insertion.

  @return Operation status.
    @retval 0   Success
    @retval !=0 Failure
*/

int Query_result_create::prepare2()
{
  DBUG_ENTER("Query_result_create::prepare2");
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
    MY_HOOKS(Query_result_create *x, TABLE_LIST *create_table_arg,
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
    Query_result_create *ptr;
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
    table= 0;
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
  {
    bitmap_set_bit(table->write_set, (*f)->field_index);
    bitmap_set_bit(table->fields_set_during_insert, (*f)->field_index);
  }

  // Set up an empty bitmap of function defaults
  if (info.add_function_default_columns(table, table->write_set))
    DBUG_RETURN(1);

  if (info.add_function_default_columns(table,
                                        table->fields_set_during_insert))
    DBUG_RETURN(1);

  table->next_number_field=table->found_next_number_field;

  restore_record(table,s->default_values);      // Get empty record
  thd->cuted_fields=0;

  const enum_duplicates duplicate_handling= info.get_duplicate_handling();

  if (thd->lex->is_ignore() || duplicate_handling != DUP_ERROR)
    table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  if (duplicate_handling == DUP_REPLACE &&
      (!table->triggers || !table->triggers->has_delete_triggers()))
    table->file->extra(HA_EXTRA_WRITE_CAN_REPLACE);
  if (duplicate_handling == DUP_UPDATE)
    table->file->extra(HA_EXTRA_INSERT_WITH_UPDATE);
  if (thd->locked_tables_mode <= LTM_LOCK_TABLES)
  {
    table->file->ha_start_bulk_insert((ha_rows) 0);
    bulk_insert_started= true;
  }

  enum_check_fields save_count_cuted_fields= thd->count_cuted_fields;
  thd->count_cuted_fields= CHECK_FIELD_WARN;

  if (check_that_all_fields_are_given_values(thd, table, table_list))
    DBUG_RETURN(1);

  thd->count_cuted_fields= save_count_cuted_fields;

  table->mark_columns_needed_for_insert();
  table->file->extra(HA_EXTRA_WRITE_CACHE);
  DBUG_RETURN(0);
}


int Query_result_create::binlog_show_create_table(TABLE **tables, uint count)
{
  DBUG_ENTER("select_create::binlog_show_create_table");
  /*
    Note 1: In RBR mode, we generate a CREATE TABLE statement for the
    created table by calling store_create_info() (behaves as SHOW
    CREATE TABLE). The 'CREATE TABLE' event will be put in the
    binlog statement cache with an Anonymous_gtid_log_event, and
    any subsequent events (e.g., table-map events and rows event)
    will be put in the binlog transaction cache with an
    Anonymous_gtid_log_event. So that the 'CREATE...SELECT'
    statement is logged as:
      Anonymous_gtid_log_event
      CREATE TABLE event
      Anonymous_gtid_log_event
      BEGIN
      rows event
      COMMIT

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

  tmp_table_list.table = *tables;
  query.length(0);      // Have to zero it since constructor doesn't

  result= store_create_info(thd, &tmp_table_list, &query, create_info,
                            /* show_database */ TRUE);
  DBUG_ASSERT(result == 0); /* store_create_info() always return 0 */

  if (mysql_bin_log.is_open())
  {
    DEBUG_SYNC(thd, "create_select_before_write_create_event");
    int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
    result= thd->binlog_query(THD::STMT_QUERY_TYPE,
                              query.ptr(), query.length(),
                              /* is_trans */ false,
                              /* direct */ true,
                              /* suppress_use */ FALSE,
                              errcode);
    DEBUG_SYNC(thd, "create_select_after_write_create_event");
  }
  DBUG_RETURN(result);
}


void Query_result_create::store_values(List<Item> &values)
{
  fill_record_n_invoke_before_triggers(thd, field, values,
                                       table, TRG_EVENT_INSERT,
                                       table->s->fields);
}


void Query_result_create::send_error(uint errcode,const char *err)
{
  DBUG_ENTER("Query_result_create::send_error");

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
  Query_result_insert::send_error(errcode, err);
  reenable_binlog(thd);

  DBUG_VOID_RETURN;
}


bool Query_result_create::send_eof()
{
  /*
    The routine that writes the statement in the binary log
    is in Query_result_insert::send_eof(). For that reason, we
    mark the flag at this point.
  */
  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
    thd->get_transaction()->mark_created_temp_table(Transaction_ctx::STMT);

  bool tmp= Query_result_insert::send_eof();
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


void Query_result_create::abort_result_set()
{
  DBUG_ENTER("Query_result_create::abort_result_set");

  /*
    In Query_result_insert::abort_result_set() we roll back the statement, including
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
  Query_result_insert::abort_result_set();
  thd->get_transaction()->reset_unsafe_rollback_flags(Transaction_ctx::STMT);
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


bool Sql_cmd_insert::execute(THD *thd)
{
  DBUG_ASSERT(thd->lex->sql_command == SQLCOM_REPLACE ||
              thd->lex->sql_command == SQLCOM_INSERT);

  bool res= false;
  LEX *const lex= thd->lex;
  SELECT_LEX *const select_lex= lex->select_lex;
  TABLE_LIST *const first_table= select_lex->get_table_list();
  TABLE_LIST *const all_tables= first_table;

  if (open_temporary_tables(thd, all_tables))
    return true;

  if (insert_precheck(thd, all_tables))
    return true;

  /* Push ignore / strict error handler */
  Ignore_error_handler ignore_handler;
  Strict_error_handler strict_handler;
  if (thd->lex->is_ignore())
    thd->push_internal_handler(&ignore_handler);
  else if (thd->is_strict_mode())
    thd->push_internal_handler(&strict_handler);

  MYSQL_INSERT_START(const_cast<char*>(thd->query().str));
  res= mysql_insert(thd, all_tables);
  MYSQL_INSERT_DONE(res, (ulong) thd->get_row_count_func());

  /* Pop ignore / strict error handler */
  if (thd->lex->is_ignore() || thd->is_strict_mode())
    thd->pop_internal_handler();

  /*
    If we have inserted into a VIEW, and the base table has
    AUTO_INCREMENT column, but this column is not accessible through
    a view, then we should restore LAST_INSERT_ID to the value it
    had before the statement.
  */
  if (first_table->is_view() && !first_table->contain_auto_increment)
    thd->first_successful_insert_id_in_cur_stmt=
      thd->first_successful_insert_id_in_prev_stmt;

  DBUG_EXECUTE_IF("after_mysql_insert",
                  {
                    const char act[]=
                      "now "
                      "wait_for signal.continue";
                    DBUG_ASSERT(opt_debug_sync_timeout > 0);
                    DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                       STRING_WITH_LEN(act)));
                  };);

  thd->lex->clear_values_map();
  return res;
}


bool Sql_cmd_insert_select::execute(THD *thd)
{
  DBUG_ASSERT(thd->lex->sql_command == SQLCOM_REPLACE_SELECT ||
              thd->lex->sql_command == SQLCOM_INSERT_SELECT);

  bool res= false;
  LEX *const lex= thd->lex;
  SELECT_LEX *const select_lex= lex->select_lex;
  SELECT_LEX_UNIT *const unit= lex->unit;
  TABLE_LIST *const first_table= select_lex->get_table_list();
  TABLE_LIST *const all_tables= first_table;

  Query_result_insert *sel_result;
  if (insert_precheck(thd, all_tables))
    return true;
  /*
    INSERT...SELECT...ON DUPLICATE KEY UPDATE/REPLACE SELECT/
    INSERT...IGNORE...SELECT can be unsafe, unless ORDER BY PRIMARY KEY
    clause is used in SELECT statement. We therefore use row based
    logging if mixed or row based logging is available.
    TODO: Check if the order of the output of the select statement is
    deterministic. Waiting for BUG#42415
  */
  if (lex->sql_command == SQLCOM_INSERT_SELECT &&
      lex->duplicates == DUP_UPDATE)
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_INSERT_SELECT_UPDATE);

  if (lex->sql_command == SQLCOM_INSERT_SELECT && lex->is_ignore())
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_INSERT_IGNORE_SELECT);

  if (lex->sql_command == SQLCOM_REPLACE_SELECT)
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_REPLACE_SELECT);

  unit->set_limit(select_lex);

  if (!(res= open_tables_for_query(thd, all_tables, 0)))
  {
    if (run_before_dml_hook(thd))
      return true;

    MYSQL_INSERT_SELECT_START(const_cast<char*>(thd->query().str));

    /* Skip first table, which is the table we are inserting in */
    TABLE_LIST *second_table= first_table->next_local;
    select_lex->table_list.first= second_table;
    select_lex->context.table_list=
      select_lex->context.first_name_resolution_table= second_table;

    res= mysql_insert_select_prepare(thd);
    if (!res && (sel_result= new Query_result_insert(first_table,
                                                     first_table->table,
                                                     &insert_field_list,
                                                     &insert_field_list,
                                                     &insert_update_list,
                                                     &insert_value_list,
                                                     lex->duplicates)))
    {
      Ignore_error_handler ignore_handler;
      Strict_error_handler strict_handler;
      if (thd->lex->is_ignore())
        thd->push_internal_handler(&ignore_handler);
      else if (thd->is_strict_mode())
        thd->push_internal_handler(&strict_handler);

      res= handle_query(thd, lex, sel_result,
                        // Don't unlock tables until command is written
                        // to binary log
                        OPTION_SETUP_TABLES_DONE | SELECT_NO_UNLOCK,
                        0);

      if (thd->lex->is_ignore() || thd->is_strict_mode())
        thd->pop_internal_handler();

      delete sel_result;
    }
    /* revert changes for SP */
    MYSQL_INSERT_SELECT_DONE(res, (ulong) thd->get_row_count_func());
    select_lex->table_list.first= first_table;
  }
  /*
    If we have inserted into a VIEW, and the base table has
    AUTO_INCREMENT column, but this column is not accessible through
    a view, then we should restore LAST_INSERT_ID to the value it
    had before the statement.
  */
  if (first_table->is_view() && !first_table->contain_auto_increment)
    thd->first_successful_insert_id_in_cur_stmt=
      thd->first_successful_insert_id_in_prev_stmt;

  thd->lex->clear_values_map();
  return res;
}


bool Sql_cmd_insert::prepared_statement_test(THD *thd)
{
  LEX *lex= thd->lex;
  return mysql_test_insert(thd, lex->query_tables);
}
