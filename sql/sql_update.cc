/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


/*
  Single table and multi table updates of tables.
  Multi-table updates were introduced by Sinisa & Monty
*/

#include "sql_update.h"

#include "auth_common.h"              // check_table_access
#include "binlog.h"                   // mysql_bin_log
#include "debug_sync.h"               // DEBUG_SYNC
#include "field.h"                    // Field
#include "item.h"                     // Item
#include "key.h"                      // is_key_used
#include "opt_explain.h"              // Modification_plan
#include "opt_trace.h"                // Opt_trace_object
#include "records.h"                  // READ_RECORD
#include "sql_base.h"                 // open_tables_for_query
#include "sql_optimizer.h"            // build_equal_items, substitute_gc
#include "sql_resolver.h"             // setup_order
#include "sql_select.h"               // free_underlaid_joins
#include "sql_tmp_table.h"            // create_tmp_table
#include "sql_view.h"                 // check_key_in_view
#include "table.h"                    // TABLE
#include "table_trigger_dispatcher.h" // Table_trigger_dispatcher
#include "sql_partition.h"            // partition_key_modified
#include "sql_prepare.h"              // select_like_stmt_cmd_test
#include "probes_mysql.h"             // MYSQL_UPDATE_START
#include "sql_parse.h"                // all_tables_not_ok

/**
   True if the table's input and output record buffers are comparable using
   compare_records(TABLE*).
 */
bool records_are_comparable(const TABLE *table) {
  return ((table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ) == 0) ||
    bitmap_is_subset(table->write_set, table->read_set);
}


/**
   Compares the input and outbut record buffers of the table to see if a row
   has changed. The algorithm iterates over updated columns and if they are
   nullable compares NULL bits in the buffer before comparing actual
   data. Special care must be taken to compare only the relevant NULL bits and
   mask out all others as they may be undefined. The storage engine will not
   and should not touch them.

   @param table The table to evaluate.

   @return true if row has changed.
   @return false otherwise.
*/
bool compare_records(const TABLE *table)
{
  DBUG_ASSERT(records_are_comparable(table));

  if ((table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ) != 0)
  {
    /*
      Storage engine may not have read all columns of the record.  Fields
      (including NULL bits) not in the write_set may not have been read and
      can therefore not be compared.
    */ 
    for (Field **ptr= table->field ; *ptr != NULL; ptr++)
    {
      Field *field= *ptr;
      if (bitmap_is_set(table->write_set, field->field_index))
      {
        if (field->real_maybe_null())
        {
          uchar null_byte_index= field->null_offset();
          
          if (((table->record[0][null_byte_index]) & field->null_bit) !=
              ((table->record[1][null_byte_index]) & field->null_bit))
            return TRUE;
        }
        if (field->cmp_binary_offset(table->s->rec_buff_length))
          return TRUE;
      }
    }
    return FALSE;
  }
  
  /* 
     The storage engine has read all columns, so it's safe to compare all bits
     including those not in the write_set. This is cheaper than the field-by-field
     comparison done above.
  */ 
  if (table->s->blob_fields + table->s->varchar_fields == 0)
    // Fixed-size record: do bitwise comparison of the records 
    return cmp_record(table,record[1]);
  /* Compare null bits */
  if (memcmp(table->null_flags,
	     table->null_flags+table->s->rec_buff_length,
	     table->s->null_bytes))
    return TRUE;				// Diff in NULL value
  /* Compare updated fields */
  for (Field **ptr= table->field ; *ptr ; ptr++)
  {
    if (bitmap_is_set(table->write_set, (*ptr)->field_index) &&
	(*ptr)->cmp_binary_offset(table->s->rec_buff_length))
      return TRUE;
  }
  return FALSE;
}


/**
  Check that all fields are base table columns.
  Replace columns from views with base table columns, and save original items in
  a list for later privilege checking.

  @param      thd              thread handler
  @param      items            Items for check
  @param[out] original_columns Saved list of items which are the original
                               resolved columns (if not NULL)

  @return false if success, true if error (Items not updatable columns or OOM)
*/

static bool check_fields(THD *thd, List<Item> &items,
                         List<Item> *original_columns)
{
  List_iterator<Item> it(items);
  Item *item;

  while ((item= it++))
  {
    // Save original item for later privilege checking
    if (original_columns && original_columns->push_back(item))
      return true;                   /* purecov: inspected */

    /*
      we make temporary copy of Item_field, to avoid influence of changing
      result_field on Item_ref which refer on this field
    */
    Item_field *const base_table_field= item->field_for_view_update();
    DBUG_ASSERT(base_table_field != NULL);

    Item_field *const cloned_field= new Item_field(thd, base_table_field);
    if (!cloned_field)
      return true;                  /* purecov: inspected */

    thd->change_item_tree(it.ref(), cloned_field);
  }
  return false;
}


/**
  Check if all expressions in list are constant expressions

  @param[in] values List of expressions

  @retval true Only constant expressions
  @retval false At least one non-constant expression
*/

static bool check_constant_expressions(List<Item> &values)
{
  Item *value;
  List_iterator_fast<Item> v(values);
  DBUG_ENTER("check_constant_expressions");

  while ((value= v++))
  {
    if (!value->const_item())
    {
      DBUG_PRINT("exit", ("expression is not constant"));
      DBUG_RETURN(false);
    }
  }
  DBUG_PRINT("exit", ("expression is constant"));
  DBUG_RETURN(true);
}


/**
  Prepare a table for an UPDATE operation

  @param thd     Thread pointer
  @param select  Query block

  @returns false if success, true if error
*/

bool mysql_update_prepare_table(THD *thd, SELECT_LEX *select)
{
  TABLE_LIST *const tr= select->table_list.first;

  if (!tr->is_view())
    return false;

  // Semi-join is not possible, as single-table UPDATE does not support joins
  if (tr->resolve_derived(thd, false))
    return true;

  if (select->merge_derived(thd, tr))
    return true;                 /* purecov: inspected */

  if (!tr->is_updatable())
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), tr->alias, "UPDATE");
    return true;
  }

  return false;
}

/*
  Process usual UPDATE

  SYNOPSIS
    mysql_update()
    thd			thread handler
    fields		fields for update
    values		values of fields for update
    limit		limit clause
    handle_duplicates	how to handle duplicates

  RETURN
    false - OK
    true  - error
*/

bool mysql_update(THD *thd,
                  List<Item> &fields,
                  List<Item> &values,
                  ha_rows limit,
                  enum enum_duplicates handle_duplicates,
                  ha_rows *found_return, ha_rows *updated_return)
{
  DBUG_ENTER("mysql_update");

  myf           error_flags= MYF(0);            /**< Flag for fatal errors */
  const bool    using_limit= limit != HA_POS_ERROR;
  bool          used_key_is_modified= false;
  bool          transactional_table, will_batch;
  int           res;
  int           error= 1;
  int           loc_error;
  uint          used_index, dup_key_found;
  bool          need_sort= true;
  bool          reverse= false;
  bool          using_filesort;
  bool          read_removal= false;
  ha_rows       updated, found;
  READ_RECORD   info;
  ulonglong     id;
  THD::killed_state killed_status= THD::NOT_KILLED;
  COPY_INFO update(COPY_INFO::UPDATE_OPERATION, &fields, &values);

  SELECT_LEX   *const select_lex= thd->lex->select_lex;
  TABLE_LIST   *const table_list= select_lex->get_table_list();

  select_lex->make_active_options(0, 0);

  const bool safe_update= thd->variables.option_bits & OPTION_SAFE_UPDATES;

  THD_STAGE_INFO(thd, stage_init);

  if (!table_list->is_updatable())
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "UPDATE");
    DBUG_RETURN(true);
  }

  TABLE_LIST *const update_table_ref= table_list->updatable_base_table();
  TABLE      *const table= update_table_ref->table;

  /* Calculate "table->covering_keys" based on the WHERE */
  table->covering_keys= table->s->keys_in_use;
  table->quick_keys.clear_all();
  table->possible_quick_keys.clear_all();

  key_map covering_keys_for_cond;
  if (mysql_prepare_update(thd, update_table_ref, &covering_keys_for_cond,
                           values))
    DBUG_RETURN(1);

  Item *conds;
  if (select_lex->get_optimizable_conditions(thd, &conds, NULL))
    DBUG_RETURN(1);

  if (update.add_function_default_columns(table, table->write_set))
    DBUG_RETURN(1);

  ORDER *order= select_lex->order_list.first;

  /*
    See if we can substitute expressions with equivalent generated
    columns in the WHERE and ORDER BY clauses of the UPDATE statement.
    It is unclear if this is best to do before or after the other
    substitutions performed by substitute_for_best_equal_field(). Do
    it here for now, to keep it consistent with how multi-table
    updates are optimized in JOIN::optimize().
  */
  if (conds || order)
    static_cast<void>(substitute_gc(thd, select_lex, conds, NULL, order));

  if ((table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ) != 0 &&
      update.function_defaults_apply(table))
    /*
      A column is to be set to its ON UPDATE function default only if other
      columns of the row are changing. To know this, we must be able to
      compare the "before" and "after" value of those columns
      (i.e. records_are_comparable() must be true below). Thus, we must read
      those columns:
    */
    bitmap_union(table->read_set, table->write_set);

  // Don't count on usage of 'only index' when calculating which key to use
  table->covering_keys.clear_all();

  /*
    This must be done before partitioning pruning, since prune_partitions()
    uses the table->write_set to determine may prune locks too.
  */
  if (table->triggers && table->triggers->mark_fields(TRG_EVENT_UPDATE))
    DBUG_RETURN(true);

  QEP_TAB_standalone qep_tab_st;
  QEP_TAB &qep_tab= qep_tab_st.as_QEP_TAB();

  if (table->part_info)
  {
    if (prune_partitions(thd, table, conds))
      DBUG_RETURN(1);
    if (table->all_partitions_pruned_away)
    {
      /* No matching records */
      if (thd->lex->describe)
      {
        /*
          Initialize plan only for regular EXPLAIN. Don't do it for EXPLAIN
          FOR CONNECTION as the plan would exist for very short period of time
          but will cost taking/releasing of a mutex, so it's not worth
          bothering with. Same for similar cases below.
        */
        Modification_plan plan(thd, MT_UPDATE, table,
                               "No matching rows after partition pruning",
                               true, 0);
        error= explain_single_table_modification(thd, &plan, select_lex);
        goto exit_without_my_ok;
      }
      my_ok(thd);
      DBUG_RETURN(0);
    }
  }
  if (lock_tables(thd, table_list, thd->lex->table_count, 0))
    DBUG_RETURN(1);

  // Must be done after lock_tables()
  if (conds)
  {
    COND_EQUAL *cond_equal= NULL;
    Item::cond_result result;
    if (table_list->check_option)
    {
      /*
        If this UPDATE is on a view with CHECK OPTION, Item_fields
        must not be replaced by constants. The reason is that when
        'conds' is optimized, 'check_option' is also optimized (it is
        part of 'conds'). Const replacement is fine for 'conds'
        because it is evaluated on a read row, but 'check_option' is
        evaluated on a row with updated fields and needs those updated
        values to be correct.

        Example:
        CREATE VIEW v1 ... WHERE fld < 2 WITH CHECK_OPTION
        UPDATE v1 SET fld=4 WHERE fld=1

        check_option is  "(fld < 2)"
        conds is         "(fld < 2) and (fld = 1)"

        optimize_cond() would propagate fld=1 to the first argument of
        the AND to create "(1 < 2) AND (fld = 1)". After this,
        check_option would be "(1 < 2)". But for check_option to work
        it must be evaluated with the *updated* value of fld: 4.
        Otherwise it will evaluate to true even when it should be
        false, which is the case for the UPDATE statement above.

        Thus, if there is a check_option, we do only the "safe" parts
        of optimize_cond(): Item_row -> Item_func_eq conversion (to
        enable range access) and removal of always true/always false
        predicates.

        An alternative to restricting this optimization of 'conds' in
        the presense of check_option: the Item-tree of 'check_option'
        could be cloned before optimizing 'conds' and thereby avoid
        const replacement. However, at the moment there is no such
        thing as Item::clone().
      */
      if (build_equal_items(thd, conds, &conds, NULL, false,
                            select_lex->join_list, &cond_equal))
        goto exit_without_my_ok;
      if (remove_eq_conds(thd, conds, &conds, &result))
        goto exit_without_my_ok;
    }
    else
    {
      if (optimize_cond(thd, &conds, &cond_equal, select_lex->join_list,
                        &result))
        goto exit_without_my_ok;
    }

    if (result == Item::COND_FALSE)
    {
      limit= 0;                                   // Impossible WHERE
      if (thd->lex->describe)
      {
        Modification_plan plan(thd, MT_UPDATE, table,
                               "Impossible WHERE", true, 0);
        error= explain_single_table_modification(thd, &plan, select_lex);
        goto exit_without_my_ok;
      }
    }
    if (conds)
    {
      conds= substitute_for_best_equal_field(conds, cond_equal, 0);
      if (conds == NULL)
      {
        error= true;
        goto exit_without_my_ok;
      }
      conds->update_used_tables();
    }
  }

  /*
    Also try a second time after locking, to prune when subqueries and
    stored programs can be evaluated.
  */
  if (table->part_info)
  {
    if (prune_partitions(thd, table, conds))
      DBUG_RETURN(1);
    if (table->all_partitions_pruned_away)
    {
      if (thd->lex->describe)
      {
        Modification_plan plan(thd, MT_UPDATE, table,
                               "No matching rows after partition pruning",
                               true, 0);
        error= explain_single_table_modification(thd, &plan, select_lex);
        goto exit_without_my_ok;
      }
      my_ok(thd);
      DBUG_RETURN(0);
    }
  }
  // Initialize the cost model that will be used for this table
  table->init_cost_model(thd->cost_model());

  /* Update the table->file->stats.records number */
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  table->mark_columns_needed_for_update(false/*mark_binlog_columns=false*/);
  if (table->vfield &&
      validate_gc_assignment(thd, &fields, &values, table))
    DBUG_RETURN(0);

  error= 0;
  qep_tab.set_table(table);
  qep_tab.set_condition(conds);

  { // Enter scope for optimizer trace wrapper
    Opt_trace_object wrapper(&thd->opt_trace);
    wrapper.add_utf8_table(update_table_ref);

    bool impossible= false;
    if (error || limit == 0)
      impossible= true;
    else if (conds != NULL)
    {
      key_map keys_to_use(key_map::ALL_BITS), needed_reg_dummy;
      QUICK_SELECT_I *qck;
      impossible= test_quick_select(thd, keys_to_use, 0, limit, safe_update,
                                    ORDER::ORDER_NOT_RELEVANT, &qep_tab,
                                    conds, &needed_reg_dummy, &qck) < 0;
      qep_tab.set_quick(qck);
    }
    if (impossible)
    {
      if (thd->lex->describe && !error && !thd->is_error())
      {
        Modification_plan plan(thd, MT_UPDATE, table,
                               "Impossible WHERE", true, 0);
        error= explain_single_table_modification(thd, &plan, select_lex);
        goto exit_without_my_ok;
      }
      free_underlaid_joins(thd, select_lex);
      /*
        There was an error or the error was already sent by
        the quick select evaluation.
        TODO: Add error code output parameter to Item::val_xxx() methods.
        Currently they rely on the user checking DA for
        errors when unwinding the stack after calling Item::val_xxx().
      */
      if (error || thd->is_error())
      {
        DBUG_RETURN(1);				// Error in where
      }

      char buff[MYSQL_ERRMSG_SIZE];
      my_snprintf(buff, sizeof(buff), ER(ER_UPDATE_INFO), 0, 0,
                  (long) thd->get_stmt_da()->current_statement_cond_count());
      my_ok(thd, 0, 0, buff);

      DBUG_PRINT("info",("0 records updated"));
      DBUG_RETURN(0);
    }
  } // Ends scope for optimizer trace wrapper

  /* If running in safe sql mode, don't allow updates without keys */
  if (table->quick_keys.is_clear_all())
  {
    thd->server_status|=SERVER_QUERY_NO_INDEX_USED;
    if (safe_update && !using_limit)
    {
      my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
		 ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
      goto exit_without_my_ok;
    }
  }
  if (select_lex->has_ft_funcs() && init_ftfuncs(thd, select_lex))
    goto exit_without_my_ok;

  table->update_const_key_parts(conds);
  order= simple_remove_const(order, conds);
        
  used_index= get_index_for_order(order, &qep_tab, limit,
                                  &need_sort, &reverse);
  if (need_sort)
  { // Assign table scan index to check below for modified key fields:
    used_index= table->file->key_used_on_scan;
  }
  if (used_index != MAX_KEY)
  { // Check if we are modifying a key that we are used to search with:
    used_key_is_modified= is_key_used(table, used_index, table->write_set);
  }
  else if (qep_tab.quick())
  {
    /*
      select->quick != NULL and used_index == MAX_KEY happens for index
      merge and should be handled in a different way.
    */
    used_key_is_modified= (!qep_tab.quick()->unique_key_range() &&
                           qep_tab.quick()->is_keys_used(table->write_set));
  }

  used_key_is_modified|= partition_key_modified(table, table->write_set);
  table->mark_columns_per_binlog_row_image();

  using_filesort= order && need_sort;

  {
    ha_rows rows;
    if (qep_tab.quick())
      rows= qep_tab.quick()->records;
    else if (!conds && !need_sort && limit != HA_POS_ERROR)
      rows= limit;
    else
    {
      DBUG_ASSERT(table->pos_in_table_list == update_table_ref);
      update_table_ref->fetch_number_of_rows();
      rows= table->file->stats.records;
    }
    qep_tab.set_quick_optim();
    qep_tab.set_condition_optim();
    DEBUG_SYNC(thd, "before_single_update");
    Modification_plan plan(thd, MT_UPDATE, &qep_tab,
                           used_index, limit,
                           (!using_filesort && (used_key_is_modified || order)),
                           using_filesort, used_key_is_modified, rows);
    DEBUG_SYNC(thd, "planned_single_update");
    if (thd->lex->describe)
    {
      error= explain_single_table_modification(thd, &plan, select_lex);
      goto exit_without_my_ok;
    }

    if (used_key_is_modified || order)
    {
      /*
        We can't update table directly;  We must first search after all
        matching rows before updating the table!
      */

      if (used_index < MAX_KEY && covering_keys_for_cond.is_set(used_index))
        table->set_keyread(true);

      /* note: We avoid sorting if we sort on the used index */
      if (using_filesort)
      {
        /*
          Doing an ORDER BY;  Let filesort find and sort the rows we are going
          to update
          NOTE: filesort will call table->prepare_for_position()
        */
        ha_rows examined_rows, found_rows, returned_rows;
        Filesort fsort(&qep_tab, order, limit);

        DBUG_ASSERT(table->sort.io_cache == NULL);
        table->sort.io_cache= (IO_CACHE*) my_malloc(key_memory_TABLE_sort_io_cache,
                                                    sizeof(IO_CACHE),
                                                    MYF(MY_FAE | MY_ZEROFILL));

        if (filesort(thd, &fsort, true,
                     &examined_rows, &found_rows, &returned_rows))
          goto exit_without_my_ok;

        table->sort.found_records= returned_rows;
        thd->inc_examined_row_count(examined_rows);
        /*
          Filesort has already found and selected the rows we want to update,
          so we don't need the where clause
        */
        qep_tab.set_quick(NULL);
        qep_tab.set_condition(NULL);
      }
      else
      {
        /*
          We are doing a search on a key that is updated. In this case
          we go trough the matching rows, save a pointer to them and
          update these in a separate loop based on the pointer.
        */
        table->prepare_for_position();

        /* If quick select is used, initialize it before retrieving rows. */
        if (qep_tab.quick() && (error= qep_tab.quick()->reset()))
        {
          if (table->file->is_fatal_error(error))
            error_flags|= ME_FATALERROR;

          table->file->print_error(error, error_flags);
          goto exit_without_my_ok;
        }
        table->file->try_semi_consistent_read(1);

        /*
          When we get here, we have one of the following options:
          A. used_index == MAX_KEY
          This means we should use full table scan, and start it with
          init_read_record call
          B. used_index != MAX_KEY
          B.1 quick select is used, start the scan with init_read_record
          B.2 quick select is not used, this is full index scan (with LIMIT)
          Full index scan must be started with init_read_record_idx
        */

        if (used_index == MAX_KEY || (qep_tab.quick()))
          error= init_read_record(&info, thd, NULL, &qep_tab, 0, 1, FALSE);
        else
          error= init_read_record_idx(&info, thd, table, 1, used_index, reverse);

        if (error)
          goto exit_without_my_ok;

        THD_STAGE_INFO(thd, stage_searching_rows_for_update);
        ha_rows tmp_limit= limit;

        IO_CACHE *tempfile= (IO_CACHE*) my_malloc(key_memory_TABLE_sort_io_cache,
                                                  sizeof(IO_CACHE),
                                                  MYF(MY_FAE | MY_ZEROFILL));

        if (open_cached_file(tempfile, mysql_tmpdir,TEMP_PREFIX,
                             DISK_BUFFER_SIZE, MYF(MY_WME)))
        {
          my_free(tempfile);
          goto exit_without_my_ok;
        }

        while (!(error=info.read_record(&info)) && !thd->killed)
        {
          thd->inc_examined_row_count(1);
          bool skip_record= FALSE;
          if (qep_tab.skip_record(thd, &skip_record))
          {
            error= 1;
            /*
             Don't try unlocking the row if skip_record reported an error since
             in this case the transaction might have been rolled back already.
            */
            break;
          }
          if (!skip_record)
          {
            if (table->file->was_semi_consistent_read())
              continue;  /* repeat the read of the same row if it still exists */

            table->file->position(table->record[0]);
            if (my_b_write(tempfile, table->file->ref,
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
          else
            table->file->unlock_row();
        }
        if (thd->killed && !error)				// Aborted
          error= 1; /* purecov: inspected */
        limit= tmp_limit;
        table->file->try_semi_consistent_read(0);
        end_read_record(&info);
        /* Change select to use tempfile */
        if (reinit_io_cache(tempfile, READ_CACHE, 0L, 0, 0))
          error=1; /* purecov: inspected */

        DBUG_ASSERT(table->sort.io_cache == NULL);
        /*
          After this assignment, init_read_record() will run, and decide to
          read from sort.io_cache. This cache will be freed when qep_tab is
          destroyed.
         */
        table->sort.io_cache= tempfile;
        qep_tab.set_quick(NULL);
        qep_tab.set_condition(NULL);
        if (error >= 0)
          goto exit_without_my_ok;
      }
      if (used_index < MAX_KEY && covering_keys_for_cond.is_set(used_index))
        table->set_keyread(false);
      table->file->ha_index_or_rnd_end();
    }

    if (thd->lex->is_ignore())
      table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  
    if (qep_tab.quick() && (error= qep_tab.quick()->reset()))
    {
      if (table->file->is_fatal_error(error))
        error_flags|= ME_FATALERROR;

      table->file->print_error(error, error_flags);
      goto exit_without_my_ok;
    }

    table->file->try_semi_consistent_read(1);
    if ((error= init_read_record(&info, thd, NULL, &qep_tab, 0, 1, FALSE)))
      goto exit_without_my_ok;

    updated= found= 0;
    /*
      Generate an error (in TRADITIONAL mode) or warning
      when trying to set a NOT NULL field to NULL.
    */
    thd->count_cuted_fields= CHECK_FIELD_WARN;
    thd->cuted_fields=0L;
    THD_STAGE_INFO(thd, stage_updating);

    transactional_table= table->file->has_transactions();

    if (table->triggers &&
        table->triggers->has_triggers(TRG_EVENT_UPDATE,
                                      TRG_ACTION_AFTER))
    {
      /*
        The table has AFTER UPDATE triggers that might access to subject 
        table and therefore might need update to be done immediately. 
        So we turn-off the batching.
      */ 
      (void) table->file->extra(HA_EXTRA_UPDATE_CANNOT_BATCH);
      will_batch= FALSE;
    }
    else
      will_batch= !table->file->start_bulk_update();

    if ((table->file->ha_table_flags() & HA_READ_BEFORE_WRITE_REMOVAL) &&
        !thd->lex->is_ignore() && !using_limit &&
        !(table->triggers && table->triggers->has_update_triggers()) &&
        qep_tab.quick() && qep_tab.quick()->index != MAX_KEY &&
        check_constant_expressions(values))
      read_removal= table->check_read_removal(qep_tab.quick()->index);

    while (true)
    {
      error= info.read_record(&info);
      if (error || thd->killed)
        break;
      thd->inc_examined_row_count(1);
      bool skip_record;
      if ((!qep_tab.skip_record(thd, &skip_record) && !skip_record))
      {
        if (table->file->was_semi_consistent_read())
          continue;  /* repeat the read of the same row if it still exists */

        store_record(table,record[1]);
        if (fill_record_n_invoke_before_triggers(thd, &update, fields, values,
                                                 table, TRG_EVENT_UPDATE, 0))
          break; /* purecov: inspected */

        found++;

        if (!records_are_comparable(table) || compare_records(table))
        {
          if ((res= table_list->view_check_option(thd)) != VIEW_CHECK_OK)
          {
            found--;
            if (res == VIEW_CHECK_SKIP)
              continue;
            else if (res == VIEW_CHECK_ERROR)
            {
              error= 1;
              break;
            }
          }

          /*
            In order to keep MySQL legacy behavior, we do this update *after*
            the CHECK OPTION test. Proper behavior is probably to throw an
            error, though.
          */
          update.set_function_defaults(table);

          if (will_batch)
          {
            /*
              Typically a batched handler can execute the batched jobs when:
              1) When specifically told to do so
              2) When it is not a good idea to batch anymore
              3) When it is necessary to send batch for other reasons
              (One such reason is when READ's must be performed)

              1) is covered by exec_bulk_update calls.
              2) and 3) is handled by the bulk_update_row method.
            
              bulk_update_row can execute the updates including the one
              defined in the bulk_update_row or not including the row
              in the call. This is up to the handler implementation and can
              vary from call to call.

              The dup_key_found reports the number of duplicate keys found
              in those updates actually executed. It only reports those if
              the extra call with HA_EXTRA_IGNORE_DUP_KEY have been issued.
              If this hasn't been issued it returns an error code and can
              ignore this number. Thus any handler that implements batching
              for UPDATE IGNORE must also handle this extra call properly.

              If a duplicate key is found on the record included in this
              call then it should be included in the count of dup_key_found
              and error should be set to 0 (only if these errors are ignored).
            */
            error= table->file->ha_bulk_update_row(table->record[1],
                                                   table->record[0],
                                                   &dup_key_found);
            limit+= dup_key_found;
            updated-= dup_key_found;
          }
          else
          {
            /* Non-batched update */
            error= table->file->ha_update_row(table->record[1],
                                              table->record[0]);
          }
          if (error == 0)
            updated++;
          else if (error == HA_ERR_RECORD_IS_THE_SAME)
            error= 0;
          else
          {
            if (table->file->is_fatal_error(error))
              error_flags|= ME_FATALERROR;

            table->file->print_error(error, error_flags);

            // The error can have been downgraded to warning by IGNORE.
            if (thd->is_error())
              break;
          }
        }

        if (!error && table->triggers &&
            table->triggers->process_triggers(thd, TRG_EVENT_UPDATE,
                                              TRG_ACTION_AFTER, TRUE))
        {
          error= 1;
          break;
        }

        if (!--limit && using_limit)
        {
          /*
            We have reached end-of-file in most common situations where no
            batching has occurred and if batching was supposed to occur but
            no updates were made and finally when the batch execution was
            performed without error and without finding any duplicate keys.
            If the batched updates were performed with errors we need to
            check and if no error but duplicate key's found we need to
            continue since those are not counted for in limit.
          */
          if (will_batch &&
              ((error= table->file->exec_bulk_update(&dup_key_found)) ||
               dup_key_found))
          {
 	    if (error)
            {
              /* purecov: begin inspected */
              /*
                The handler should not report error of duplicate keys if they
                are ignored. This is a requirement on batching handlers.
              */
              if (table->file->is_fatal_error(error))
                error_flags|= ME_FATALERROR;

              table->file->print_error(error, error_flags);
              error= 1;
              break;
              /* purecov: end */
            }
            /*
              Either an error was found and we are ignoring errors or there
              were duplicate keys found. In both cases we need to correct
              the counters and continue the loop.
            */
            limit= dup_key_found; //limit is 0 when we get here so need to +
            updated-= dup_key_found;
          }
          else
          {
            error= -1;				// Simulate end of file
            break;
          }
        }
      }
      /*
        Don't try unlocking the row if skip_record reported an error since in
        this case the transaction might have been rolled back already.
      */
      else if (!thd->is_error())
        table->file->unlock_row();
      else
      {
        error= 1;
        break;
      }
      thd->get_stmt_da()->inc_current_row_for_condition();
      if (thd->is_error())
      {
        error= 1;
        break;
      }
    }
    table->auto_increment_field_not_null= FALSE;
    dup_key_found= 0;
    /*
      Caching the killed status to pass as the arg to query event constuctor;
      The cached value can not change whereas the killed status can
      (externally) since this point and change of the latter won't affect
      binlogging.
      It's assumed that if an error was set in combination with an effective 
      killed status then the error is due to killing.
    */
    killed_status= thd->killed; // get the status of the volatile 
    // simulated killing after the loop must be ineffective for binlogging
    DBUG_EXECUTE_IF("simulate_kill_bug27571",
                    {
                      thd->killed= THD::KILL_QUERY;
                    };);
    error= (killed_status == THD::NOT_KILLED)?  error : 1;
  
    if (error &&
        will_batch &&
        (loc_error= table->file->exec_bulk_update(&dup_key_found)))
      /*
        An error has occurred when a batched update was performed and returned
        an error indication. It cannot be an allowed duplicate key error since
        we require the batching handler to treat this as a normal behavior.

        Otherwise we simply remove the number of duplicate keys records found
        in the batched update.
      */
    {
      /* purecov: begin inspected */
      error_flags= MYF(0);
      if (table->file->is_fatal_error(loc_error))
        error_flags|= ME_FATALERROR;

      table->file->print_error(loc_error, error_flags);
      error= 1;
      /* purecov: end */
    }
    else
      updated-= dup_key_found;
    if (will_batch)
      table->file->end_bulk_update();
    table->file->try_semi_consistent_read(0);

    if (read_removal)
    {
      /* Only handler knows how many records really was written */
      updated= table->file->end_read_removal();
      if (!records_are_comparable(table))
        found= updated;
    }

  } // End of scope for Modification_plan

  if (!transactional_table && updated > 0)
    thd->get_transaction()->mark_modified_non_trans_table(
      Transaction_ctx::STMT);

  end_read_record(&info);
  THD_STAGE_INFO(thd, stage_end);
  (void) table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);

  /*
    Invalidate the table in the query cache if something changed.
    This must be before binlog writing and ha_autocommit_...
  */
  if (updated)
    query_cache.invalidate_single(thd, update_table_ref, true);
  
  /*
    error < 0 means really no error at all: we processed all rows until the
    last one without error. error > 0 means an error (e.g. unique key
    violation and no IGNORE or REPLACE). error == 0 is also an error (if
    preparing the record or invoking before triggers fails). See
    ha_autocommit_or_rollback(error>=0) and DBUG_RETURN(error>=0) below.
    Sometimes we want to binlog even if we updated no rows, in case user used
    it to be sure master and slave are in same state.
  */
  if ((error < 0) || thd->get_transaction()->cannot_safely_rollback(
      Transaction_ctx::STMT))
  {
    if (mysql_bin_log.is_open())
    {
      int errcode= 0;
      if (error < 0)
        thd->clear_error();
      else
        errcode= query_error_code(thd, killed_status == THD::NOT_KILLED);

      if (thd->binlog_query(THD::ROW_QUERY_TYPE,
                            thd->query().str, thd->query().length,
                            transactional_table, FALSE, FALSE, errcode))
      {
        error=1;				// Rollback update
      }
    }
  }
  DBUG_ASSERT(transactional_table || !updated ||
              thd->get_transaction()->cannot_safely_rollback(
                Transaction_ctx::STMT));
  free_underlaid_joins(thd, select_lex);

  /* If LAST_INSERT_ID(X) was used, report X */
  id= thd->arg_of_last_insert_id_function ?
    thd->first_successful_insert_id_in_prev_stmt : 0;

  if (error < 0)
  {
    char buff[MYSQL_ERRMSG_SIZE];
    my_snprintf(buff, sizeof(buff), ER(ER_UPDATE_INFO), (long) found,
                (long) updated,
                (long) thd->get_stmt_da()->current_statement_cond_count());
    my_ok(thd, thd->get_protocol()->has_client_capability(CLIENT_FOUND_ROWS) ?
          found : updated, id, buff);
    DBUG_PRINT("info",("%ld records updated", (long) updated));
  }
  thd->count_cuted_fields= CHECK_FIELD_IGNORE;		/* calc cuted fields */
  *found_return= found;
  *updated_return= updated;
  DBUG_RETURN((error >= 0 || thd->is_error()) ? 1 : 0);

exit_without_my_ok:
  free_underlaid_joins(thd, select_lex);
  table->set_keyread(FALSE);
  DBUG_RETURN(error);
}

/**
  Prepare items in UPDATE statement

  @param thd              thread handler
  @param update_table_ref Reference to table being updated
  @param[out] covering_keys_for_cond Keys which are covering for conditions
                                     and ORDER BY clause.

  @return false if success, true if error
*/
bool mysql_prepare_update(THD *thd, const TABLE_LIST *update_table_ref,
                          key_map *covering_keys_for_cond,
                          List<Item> &update_value_list)
{
  List<Item> all_fields;
  LEX *const lex= thd->lex;
  SELECT_LEX *const select= lex->select_lex;
  TABLE_LIST *const table_list= select->get_table_list();
  DBUG_ENTER("mysql_prepare_update");

  DBUG_ASSERT(select->item_list.elements == update_value_list.elements);

  lex->allow_sum_func= 0;

  if (select->setup_tables(thd, table_list, false))
    DBUG_RETURN(true);
  if (select->derived_table_count &&
      select->check_view_privileges(thd, UPDATE_ACL, SELECT_ACL))
    DBUG_RETURN(true);

  thd->want_privilege= SELECT_ACL;
  enum enum_mark_columns mark_used_columns_saved= thd->mark_used_columns;
  thd->mark_used_columns= MARK_COLUMNS_READ;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  table_list->set_want_privilege(SELECT_ACL);
#endif
  if (select->setup_conds(thd))
    DBUG_RETURN(true);
  if (select->setup_ref_array(thd))
    DBUG_RETURN(true);                          /* purecov: inspected */
  if (select->order_list.first &&
      setup_order(thd, select->ref_pointer_array,
                  table_list, all_fields, all_fields,
                  select->order_list.first))
    DBUG_RETURN(true);

  // Return covering keys derived from conditions and ORDER BY clause:
  *covering_keys_for_cond= update_table_ref->table->covering_keys;

  // Check the fields we are going to modify
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  table_list->set_want_privilege(UPDATE_ACL);
#endif
  if (setup_fields(thd, Ref_ptr_array(), select->item_list, UPDATE_ACL, NULL,
                   false, true))
    DBUG_RETURN(true);                     /* purecov: inspected */

  if (check_fields(thd, select->item_list, NULL))
    DBUG_RETURN(true);

  // check_key_in_view() may send an SQL note, but we only want it once.
  if (select->first_execution &&
      check_key_in_view(thd, table_list, update_table_ref))
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "UPDATE");
    DBUG_RETURN(true);
  }

  table_list->set_want_privilege(SELECT_ACL);

  if (setup_fields(thd, Ref_ptr_array(), update_value_list, SELECT_ACL, NULL,
                   false, false))
    DBUG_RETURN(true);                          /* purecov: inspected */

  thd->mark_used_columns= mark_used_columns_saved;

  // Check that table to be updated is not used in a subquery
  TABLE_LIST *const duplicate= unique_table(thd, update_table_ref,
                                            table_list->next_global, 0);
  if (duplicate)
  {
    update_non_unique_table_error(table_list, "UPDATE", duplicate);
    DBUG_RETURN(true);
  }

  if (setup_ftfuncs(select))
    DBUG_RETURN(true);                          /* purecov: inspected */

  if (select->inner_refs_list.elements && select->fix_inner_refs(thd))
    DBUG_RETURN(true);  /* purecov: inspected */

  if (select->apply_local_transforms(thd, false))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
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
  DBUG_PRINT("info", ("table_map: 0x%08lx", (long) map));
  return map;
}

/**
  If one row is updated through two different aliases and the first
  update physically moves the row, the second update will error
  because the row is no longer located where expected. This function
  checks if the multiple-table update is about to do that and if so
  returns with an error.

  The following update operations physically moves rows:
    1) Update of a column in a clustered primary key
    2) Update of a column used to calculate which partition the row belongs to

  This function returns with an error if both of the following are
  true:

    a) A table in the multiple-table update statement is updated
       through multiple aliases (including views)
    b) At least one of the updates on the table from a) may physically
       moves the row. Note: Updating a column used to calculate which
       partition a row belongs to does not necessarily mean that the
       row is moved. The new value may or may not belong to the same
       partition.

  @param leaves               First leaf table
  @param tables_for_update    Map of tables that are updated

  @return
    true   if the update is unsafe, in which case an error message is also set,
    false  otherwise.
*/
static
bool unsafe_key_update(TABLE_LIST *leaves, table_map tables_for_update)
{
  TABLE_LIST *tl= leaves;

  for (tl= leaves; tl ; tl= tl->next_leaf)
  {
    if (tl->map() & tables_for_update)
    {
      TABLE *table1= tl->table;
      bool primkey_clustered= (table1->file->primary_key_is_clustered() &&
                               table1->s->primary_key != MAX_KEY);

      bool table_partitioned= (table1->part_info != NULL);

      if (!table_partitioned && !primkey_clustered)
        continue;

      for (TABLE_LIST* tl2= tl->next_leaf; tl2 ; tl2= tl2->next_leaf)
      {
        /*
          Look at "next" tables only since all previous tables have
          already been checked
        */
        TABLE *table2= tl2->table;
        if (tl2->map() & tables_for_update && table1->s == table2->s)
        {
          // A table is updated through two aliases
          if (table_partitioned &&
              (partition_key_modified(table1, table1->write_set) ||
               partition_key_modified(table2, table2->write_set)))
          {
            // Partitioned key is updated
            my_error(ER_MULTI_UPDATE_KEY_CONFLICT, MYF(0),
                     tl->belong_to_view ? tl->belong_to_view->alias
                                        : tl->alias,
                     tl2->belong_to_view ? tl2->belong_to_view->alias
                                         : tl2->alias);
            return true;
          }

          if (primkey_clustered)
          {
            // The primary key can cover multiple columns
            KEY key_info= table1->key_info[table1->s->primary_key];
            KEY_PART_INFO *key_part= key_info.key_part;
            KEY_PART_INFO *key_part_end= key_part +
              key_info.user_defined_key_parts;

            for (;key_part != key_part_end; ++key_part)
            {
              if (bitmap_is_set(table1->write_set, key_part->fieldnr-1) ||
                  bitmap_is_set(table2->write_set, key_part->fieldnr-1))
              {
                // Clustered primary key is updated
                my_error(ER_MULTI_UPDATE_KEY_CONFLICT, MYF(0),
                         tl->belong_to_view ? tl->belong_to_view->alias
                         : tl->alias,
                         tl2->belong_to_view ? tl2->belong_to_view->alias
                         : tl2->alias);
                return true;
              }
            }
          }
        }
      }
    }
  }
  return false;
}


/**
  Check if there is enough privilege on specific table used by the
  main select list of multi-update directly or indirectly (through
  a view).

  @param[in]  thd               Thread context.
  @param[in]  table             Table list element for the table.
  @param[in]  tables_for_update Bitmap with tables being updated.
  @param[in]  updatable         True if table in question is updatable.
                                Should be initialized to true by the caller
                                before a sequence of calls to this function.
  @param[out] updated           Return whether the table is actually updated.
                                Should be initialized to false by caller.

  @note To determine which tables/views are updated we have to go from
        leaves to root since tables_for_update contains map of leaf
        tables being updated and doesn't include non-leaf tables
        (fields are already resolved to leaf tables).

  @retval false - Success, all necessary privileges on all tables are
                  present or might be present on column-level.
  @retval true  - Failure, some necessary privilege on some table is
                  missing.
*/

static bool multi_update_check_table_access(THD *thd, TABLE_LIST *table,
                                            table_map tables_for_update,
                                            bool updatable, bool *updated)
{
  // Adjust updatability based on table/view's properties:
  updatable&= table->is_updatable();

  if (table->is_view_or_derived())
  {
    // Determine if this view/derived table is updated:
    bool view_updated= false;
    /*
      If it is a mergeable view then we need to check privileges on its
      underlying tables being merged (including views). We also need to
      check if any of them is updated in order to find if this view is
      updated.
      If it is a non-mergeable view or a derived table then it can't be updated.
    */
    DBUG_ASSERT(table->merge_underlying_list ||
                (!table->is_updatable() &&
                 !(table->map() & tables_for_update)));

    Internal_error_handler_holder<View_error_handler, TABLE_LIST>
      view_handler(thd, true, table->merge_underlying_list);
    for (TABLE_LIST *tbl= table->merge_underlying_list; tbl;
         tbl= tbl->next_local)
    {
      if (multi_update_check_table_access(thd, tbl, tables_for_update,
                                          updatable, &view_updated))
        return true;
    }
    table->set_want_privilege(
      view_updated ? UPDATE_ACL | SELECT_ACL: SELECT_ACL);
    table->updating= view_updated;
    *updated|= view_updated;
  }
  else
  {
    // Must be a base table.
    const bool base_table_updated= table->map() & tables_for_update;
    if (base_table_updated && !updatable)
    {
      my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table->alias, "UPDATE");
      return true;
    }
    table->set_want_privilege(
      base_table_updated ? UPDATE_ACL | SELECT_ACL : SELECT_ACL);

    table->updating= base_table_updated;
    *updated|= base_table_updated;
  }

  return false;
}


/*
  make update specific preparation and checks after opening tables

  SYNOPSIS
    mysql_multi_update_prepare()
    thd         thread handler

  RETURN
    FALSE OK
    TRUE  Error
*/

int Sql_cmd_update::mysql_multi_update_prepare(THD *thd)
{
  LEX *lex= thd->lex;
  SELECT_LEX *select= lex->select_lex;
  TABLE_LIST *table_list= lex->query_tables;
  List<Item> *fields= &select->item_list;
  table_map tables_for_update;
  const bool using_lock_tables= thd->locked_tables_mode != LTM_NONE;
  bool original_multiupdate= (thd->lex->sql_command == SQLCOM_UPDATE_MULTI);
  DBUG_ENTER("mysql_multi_update_prepare");

  DBUG_ASSERT(select->item_list.elements == update_value_list.elements);

  Prepare_error_tracker tracker(thd);

  /* following need for prepared statements, to run next time multi-update */
  sql_command= thd->lex->sql_command= SQLCOM_UPDATE_MULTI;

  /*
    Open tables and create derived ones, but do not lock and fill them yet.

    During prepare phase acquire only S metadata locks instead of SW locks to
    keep prepare of multi-UPDATE compatible with concurrent LOCK TABLES WRITE
    and global read lock.
  */
  if (original_multiupdate &&
      open_tables_for_query(thd, table_list,
                            thd->stmt_arena->is_stmt_prepare() ?
                            MYSQL_OPEN_FORCE_SHARED_MDL : 0))
    DBUG_RETURN(true);

  if (run_before_dml_hook(thd))
    DBUG_RETURN(true);

  /*
    setup_tables() need for VIEWs. SELECT_LEX::prepare() will call
    setup_tables() second time, but this call will do nothing (there are check
    for second call in setup_tables()).
  */

  if (select->setup_tables(thd, table_list, false))
    DBUG_RETURN(true);             /* purecov: inspected */

  /*
    A view's CHECK OPTION is incompatible with semi-join.
    @note We could let non-updated views do semi-join, and we could let
          updated views without CHECK OPTION do semi-join.
          But since we resolve derived tables before we know this context,
          we cannot use semi-join in any case currently.
          The problem is that the CHECK OPTION condition serves as
          part of the semi-join condition, and a standalone condition
          to be evaluated as part of the UPDATE, and those two uses are
          incompatible.
  */
  if (select->derived_table_count && select->resolve_derived(thd, false))
    DBUG_RETURN(true);

  if (setup_natural_join_row_types(thd, select->join_list, &select->context))
    DBUG_RETURN(true);

  /*
    In multi-table UPDATE, tables to be updated are determined based on
    which fields are updated. Hence, tables that need to be checked
    for update have not been established yet, and thus 0 is supplied
    for want_privilege argument below.
    Later, the combined used_tables information for these columns are used
    to determine updatable tables, those tables are prepared for update,
    and finally the columns can be checked for proper update privileges.
  */
  if (setup_fields(thd, Ref_ptr_array(), *fields, 0, NULL, false, true))
    DBUG_RETURN(true);

  List<Item> original_update_fields;
  if (check_fields(thd, *fields, &original_update_fields))
    DBUG_RETURN(true);

  thd->table_map_for_update= tables_for_update= get_table_map(fields);

  /*
    Setup timestamp handling and locking mode
  */
  for (TABLE_LIST *tl= select->leaf_tables; tl; tl= tl->next_leaf)
  {
    // if table will be updated then check that it is updatable and unique:
    if (tl->map() & tables_for_update)
    {
      if (!tl->is_updatable() || check_key_in_view(thd, tl, tl))
      {
        my_error(ER_NON_UPDATABLE_TABLE, MYF(0), tl->alias, "UPDATE");
        DBUG_RETURN(true);
      }

      DBUG_PRINT("info",("setting table `%s` for update", tl->alias));
      /*
        If table will be updated we should not downgrade lock for it and
        leave it as is.
      */
    }
    else
    {
      DBUG_PRINT("info",("setting table `%s` for read-only", tl->alias));
      /*
        If we are using the binary log, we need TL_READ_NO_INSERT to get
        correct order of statements. Otherwise, we use a TL_READ lock to
        improve performance.
        We don't downgrade metadata lock from SW to SR in this case as
        there is no guarantee that the same ticket is not used by
        another table instance used by this statement which is going to
        be write-locked (for example, trigger to be invoked might try
        to update this table).
        Last argument routine_modifies_data for read_lock_type_for_table()
        is ignored, as prelocking placeholder will never be set here.
      */
      DBUG_ASSERT(tl->prelocking_placeholder == false);
      tl->lock_type= read_lock_type_for_table(thd, lex, tl, true);
      /* Update TABLE::lock_type accordingly. */
      if (!tl->is_placeholder() && !using_lock_tables)
        tl->table->reginfo.lock_type= tl->lock_type;
    }
  }

  /*
    Check privileges for tables being updated or read.
    Note that we need to iterate here not only through all leaf tables but
    also through all view hierarchy.
  */
  for (TABLE_LIST *tl= table_list; tl; tl= tl->next_local)
  {
    bool updated= false;
    if (multi_update_check_table_access(thd, tl, tables_for_update, true,
                                        &updated))
      DBUG_RETURN(true);
  }

  // Check privileges for columns that are updated
  {  // Opening scope for the RAII object
  Item *item;
  List_iterator_fast<Item> orig_it(original_update_fields);
  Mark_field mf(MARK_COLUMNS_WRITE);
  Column_privilege_tracker priv_tracker(thd, UPDATE_ACL);
  while ((item= orig_it++))
  {
    if (item->walk(&Item::check_column_privileges, Item::WALK_PREFIX,
                   (uchar *)thd))
      DBUG_RETURN(true);

    item->walk(&Item::mark_field_in_map, Item::WALK_POSTFIX, (uchar *)&mf);
  }
  } // Closing scope for the RAII object

  if (unsafe_key_update(select->leaf_tables, tables_for_update))
    DBUG_RETURN(true);

  /*
    When using a multi-table UPDATE command as a prepared statement,
    1) We must validate values (the right argument 'expr' of 'SET col1=expr')
    during PREPARE, so that:
    - bad columns are reported by PREPARE
    - cached_table is set for fields before query transformations (semijoin,
    view merging...) are done and make resolution more difficult.
    2) This validation is done by Query_result_update::prepare() but it is
    not called by PREPARE.
    3) So we do it below.
    @todo Remove this code duplication as part of WL#6570
  */
  if (thd->stmt_arena->is_stmt_prepare())
  {
    if (setup_fields(thd, Ref_ptr_array(), update_value_list, SELECT_ACL,
                     NULL, false, false))
      DBUG_RETURN(true);

    /*
      Check that table being updated is not being used in a subquery, but
      skip all tables of the UPDATE query block itself
    */
    select->exclude_from_table_unique_test= true;
    for (TABLE_LIST *tr= select->leaf_tables; tr; tr= tr->next_leaf)
    {
      if (tr->lock_type != TL_READ &&
          tr->lock_type != TL_READ_NO_INSERT)
      {
        TABLE_LIST *duplicate= unique_table(thd, tr, select->leaf_tables, 0);
        if (duplicate != NULL)
        {
          update_non_unique_table_error(select->leaf_tables, "UPDATE",
                                        duplicate);
          DBUG_RETURN(true);
        }
      }
    }
  }

  /* check single table update for view compound from several tables */
  for (TABLE_LIST *tl= table_list; tl; tl= tl->next_local)
  {
    if (tl->is_merged())
    {
      DBUG_ASSERT(tl->is_view_or_derived());
      TABLE_LIST *for_update= NULL;
      if (tl->check_single_table(&for_update, tables_for_update))
      {
	my_error(ER_VIEW_MULTIUPDATE, MYF(0),
		 tl->view_db.str, tl->view_name.str);
	DBUG_RETURN(true);
      }
    }
  }

  // Downgrade desired privileges for updated tables to SELECT
  for (TABLE_LIST *tl= table_list; tl; tl= tl->next_local)
  {
    if (tl->updating)
      tl->set_want_privilege(SELECT_ACL);
  }

  /* @todo: downgrade the metadata locks here. */

  /*
    Syntax rule for multi-table update prevents these constructs.
    But they are possible for single-table UPDATE against multi-table view.
  */
  if (select->order_list.elements)
  {
    my_error(ER_WRONG_USAGE, MYF(0), "UPDATE", "ORDER BY");
    DBUG_RETURN(true);
  }
  if (select->select_limit)
  {
    my_error(ER_WRONG_USAGE, MYF(0), "UPDATE", "LIMIT");
    DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}


/*
  Setup multi-update handling and call SELECT to do the join
*/

bool mysql_multi_update(THD *thd,
                        List<Item> *fields,
                        List<Item> *values,
                        enum enum_duplicates handle_duplicates,
                        SELECT_LEX *select_lex,
                        Query_result_update **result)
{
  bool res;
  DBUG_ENTER("mysql_multi_update");

  if (!(*result= new Query_result_update(select_lex->get_table_list(),
                                         select_lex->leaf_tables,
                                         fields, values,
                                         handle_duplicates)))
    DBUG_RETURN(true); /* purecov: inspected */

  DBUG_ASSERT(select_lex->having_cond() == NULL &&
              !select_lex->order_list.elements &&
              !select_lex->group_list.elements &&
              !select_lex->select_limit);

  res= handle_query(thd, thd->lex, *result,
                    SELECT_NO_JOIN_CACHE | SELECT_NO_UNLOCK |
                    OPTION_SETUP_TABLES_DONE,
                    OPTION_BUFFER_RESULT);

  DBUG_PRINT("info",("res: %d  report_error: %d",res, (int) thd->is_error()));
  res|= thd->is_error();
  if (unlikely(res))
  {
    /* If we had a another error reported earlier then this will be ignored */
    (*result)->send_error(ER_UNKNOWN_ERROR, ER(ER_UNKNOWN_ERROR));
    (*result)->abort_result_set();
  }
  DBUG_RETURN(res);
}


Query_result_update::Query_result_update(TABLE_LIST *table_list,
                                         TABLE_LIST *leaves_list,
                                         List<Item> *field_list,
                                         List<Item> *value_list,
                                    enum enum_duplicates handle_duplicates_arg)
  :all_tables(table_list), leaves(leaves_list), update_tables(0),
   tmp_tables(0), updated(0), found(0), fields(field_list),
   values(value_list), table_count(0), copy_field(0),
   handle_duplicates(handle_duplicates_arg), do_update(1), trans_safe(1),
   transactional_tables(0), error_handled(0), update_operations(NULL)
{}


/*
  Connect fields with tables and create list of tables that are updated
*/

int Query_result_update::prepare(List<Item> &not_used_values,
                                 SELECT_LEX_UNIT *lex_unit)
{
  SQL_I_List<TABLE_LIST> update;
  List_iterator_fast<Item> field_it(*fields);
  List_iterator_fast<Item> value_it(*values);
  DBUG_ENTER("Query_result_update::prepare");

  SELECT_LEX *const select= lex_unit->first_select();

  thd->count_cuted_fields= CHECK_FIELD_WARN;
  thd->cuted_fields=0L;
  THD_STAGE_INFO(thd, stage_updating_main_table);

  const table_map tables_to_update= get_table_map(fields);

  if (!tables_to_update)
  {
    my_message(ER_NO_TABLES_USED, ER(ER_NO_TABLES_USED), MYF(0));
    DBUG_RETURN(1);
  }

  /*
    We gather the set of columns read during evaluation of SET expression in
    TABLE::tmp_set by pointing TABLE::read_set to it and then restore it after
    setup_fields().
  */
  for (TABLE_LIST *tr= leaves; tr; tr= tr->next_leaf)
  {
    if (tables_to_update & tr->map())
    {
      TABLE *const table= tr->table;
      DBUG_ASSERT(table->read_set == &table->def_read_set);
      table->read_set= &table->tmp_set;
      bitmap_clear_all(table->read_set);
    }
    // Resolving may be needed for subsequent executions
    if (tr->check_option && !tr->check_option->fixed &&
        tr->check_option->fix_fields(thd, NULL))
      DBUG_RETURN(1);        /* purecov: inspected */
  }

  /*
    We have to check values after setup_tables to get covering_keys right in
    reference tables
  */

  int error= setup_fields(thd, Ref_ptr_array(), *values, SELECT_ACL, NULL,
                          false, false);

  for (TABLE_LIST *tr= leaves; tr; tr= tr->next_leaf)
  {
    if (tables_to_update & tr->map())
    {
      TABLE *const table= tr->table;
      table->read_set= &table->def_read_set;
      bitmap_union(table->read_set, &table->tmp_set);
      bitmap_clear_all(&table->tmp_set);
    }
  }
  
  if (error)
    DBUG_RETURN(1);    

  /*
    Check that table being updated is not being used in a subquery, but
    skip all tables of the UPDATE query block itself
  */
  select->exclude_from_table_unique_test= true;
  for (TABLE_LIST *tr= select->leaf_tables; tr; tr= tr->next_leaf)
  {
    if (tr->lock_type != TL_READ &&
        tr->lock_type != TL_READ_NO_INSERT)
    {
      TABLE_LIST *duplicate= unique_table(thd, tr, all_tables, 0);
      if (duplicate != NULL)
      {
        update_non_unique_table_error(all_tables, "UPDATE", duplicate);
        DBUG_RETURN(true);
      }
    }
  }
  /*
    Set exclude_from_table_unique_test value back to FALSE. It is needed for
    further check whether to use record cache.
  */
  select->exclude_from_table_unique_test= false;
  /*
    Save tables beeing updated in update_tables
    update_table->shared is position for table
    Don't use key read on tables that are updated
  */

  update.empty();
  uint leaf_table_count= 0;
  for (TABLE_LIST *tr= leaves; tr; tr= tr->next_leaf)
  {
    /* TODO: add support of view of join support */
    leaf_table_count++;
    if (tables_to_update & tr->map())
    {
      TABLE_LIST *dup= (TABLE_LIST*) thd->memdup(tr, sizeof(*dup));
      if (dup == NULL)
	DBUG_RETURN(1);

      TABLE *const table= tr->table;

      update.link_in_list(dup, &dup->next_local);
      tr->shared= dup->shared= table_count++;
      table->no_keyread=1;
      table->covering_keys.clear_all();
      table->pos_in_table_list= dup;
      if (table->triggers &&
          table->triggers->has_triggers(TRG_EVENT_UPDATE,
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
  }


  table_count=  update.elements;
  update_tables= update.first;

  tmp_tables = (TABLE**) thd->mem_calloc(sizeof(TABLE *) * table_count);
  tmp_table_param= new (thd->mem_root) Temp_table_param[table_count];
  fields_for_table= (List_item **) thd->alloc(sizeof(List_item *) *
					      table_count);
  values_for_table= (List_item **) thd->alloc(sizeof(List_item *) *
					      table_count);

  DBUG_ASSERT(update_operations == NULL);
  update_operations= (COPY_INFO**) thd->mem_calloc(sizeof(COPY_INFO*) *
                                               table_count);

  if (thd->is_error())
    DBUG_RETURN(1);
  for (uint i= 0; i < table_count; i++)
  {
    fields_for_table[i]= new List_item;
    values_for_table[i]= new List_item;
  }
  if (thd->is_error())
    DBUG_RETURN(1);

  /* Split fields into fields_for_table[] and values_by_table[] */

  Item *item;
  while ((item= field_it++))
  {
    Item_field *const field= down_cast<Item_field *>(item);
    Item *const value= value_it++;
    uint offset= field->table_ref->shared;
    fields_for_table[offset]->push_back(field);
    values_for_table[offset]->push_back(value);
  }
  if (thd->is_fatal_error)
    DBUG_RETURN(1);

  /* Allocate copy fields */
  uint max_fields= 0;
  for (uint i= 0; i < table_count; i++)
    set_if_bigger(max_fields, fields_for_table[i]->elements + leaf_table_count);
  copy_field= new Copy_field[max_fields];


  for (TABLE_LIST *ref= leaves; ref != NULL; ref= ref->next_leaf)
  {
    if (tables_to_update & ref->map())
    {
      const uint position= ref->shared;
      List<Item> *cols= fields_for_table[position];
      List<Item> *vals= values_for_table[position];
      TABLE *const table= ref->table;

      COPY_INFO *update=
        new (thd->mem_root) COPY_INFO(COPY_INFO::UPDATE_OPERATION, cols, vals);
      if (update == NULL ||
          update->add_function_default_columns(table, table->write_set))
        DBUG_RETURN(1);

      update_operations[position]= update;

      if ((table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ) != 0 &&
          update->function_defaults_apply(table))
      {
        /*
          A column is to be set to its ON UPDATE function default only if
          other columns of the row are changing. To know this, we must be able
          to compare the "before" and "after" value of those columns. Thus, we
          must read those columns:
        */
        bitmap_union(table->read_set, table->write_set);
      }
      /* All needed columns must be marked before prune_partitions(). */
      if (table->triggers && table->triggers->mark_fields(TRG_EVENT_UPDATE))
        DBUG_RETURN(true);
    }
  }

  DBUG_RETURN(thd->is_fatal_error != 0);
}


/*
  Check if table is safe to update on fly

  SYNOPSIS
    safe_update_on_fly()
    thd                 Thread handler
    join_tab            How table is used in join
    all_tables          List of tables

  NOTES
    We can update the first table in join on the fly if we know that
    a row in this table will never be read twice. This is true under
    the following conditions:

    - No column is both written to and read in SET expressions.

    - We are doing a table scan and the data is in a separate file (MyISAM) or
      if we don't update a clustered key.

    - We are doing a range scan and we don't update the scan key or
      the primary key for a clustered table handler.

    - Table is not joined to itself.

    This function gets information about fields to be updated from
    the TABLE::write_set bitmap.

  WARNING
    This code is a bit dependent of how make_join_readinfo() works.

    The field table->tmp_set is used for keeping track of which fields are
    read during evaluation of the SET expression.
    See Query_result_update::prepare.

  RETURN
    0		Not safe to update
    1		Safe to update
*/

static bool safe_update_on_fly(THD *thd, JOIN_TAB *join_tab,
                               TABLE_LIST *table_ref, TABLE_LIST *all_tables)
{
  TABLE *table= join_tab->table();
  if (unique_table(thd, table_ref, all_tables, 0))
    return 0;
  switch (join_tab->type()) {
  case JT_SYSTEM:
  case JT_CONST:
  case JT_EQ_REF:
    return TRUE;				// At most one matching row
  case JT_REF:
  case JT_REF_OR_NULL:
    return !is_key_used(table, join_tab->ref().key, table->write_set);
  case JT_ALL:
    if (bitmap_is_overlapping(&table->tmp_set, table->write_set))
      return FALSE;
    /* If range search on index */
    if (join_tab->quick())
      return !join_tab->quick()->is_keys_used(table->write_set);
    /* If scanning in clustered key */
    if ((table->file->ha_table_flags() & HA_PRIMARY_KEY_IN_READ_INDEX) &&
	table->s->primary_key < MAX_KEY)
      return !is_key_used(table, table->s->primary_key, table->write_set);
    return TRUE;
  default:
    break;					// Avoid compler warning
  }
  return FALSE;

}


/*
  Initialize table for multi table

  IMPLEMENTATION
    - Update first table in join on the fly, if possible
    - Create temporary tables to store changed values for all other tables
      that are updated (and main_table if the above doesn't hold).
*/

bool Query_result_update::initialize_tables(JOIN *join)
{
  TABLE_LIST *table_ref;
  DBUG_ENTER("initialize_tables");
  ASSERT_BEST_REF_IN_JOIN_ORDER(join);

  if ((thd->variables.option_bits & OPTION_SAFE_UPDATES) &&
      error_if_full_join(join))
    DBUG_RETURN(true);
  main_table= join->best_ref[0]->table();
  table_to_update= 0;

  /* Any update has at least one pair (field, value) */
  DBUG_ASSERT(fields->elements);
  /*
   Only one table may be modified by UPDATE of an updatable view.
   For an updatable view first_table_for_update indicates this
   table.
   For a regular multi-update it refers to some updated table.
  */ 
  TABLE_LIST *first_table_for_update= ((Item_field *)fields->head())->table_ref;

  /* Create a temporary table for keys to all tables, except main table */
  for (table_ref= update_tables; table_ref; table_ref= table_ref->next_local)
  {
    TABLE *table=table_ref->table;
    uint cnt= table_ref->shared;
    List<Item> temp_fields;
    ORDER     group;
    Temp_table_param *tmp_param;
    if (table->vfield &&
        validate_gc_assignment(thd, fields, values, table))
      DBUG_RETURN(0);

    if (thd->lex->is_ignore())
      table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
    if (table == main_table)			// First table in join
    {
      /*
        If there are at least two tables to update, t1 and t2, t1 being
        before t2 in the plan, we need to collect all fields of t1 which
        influence the selection of rows from t2. If those fields are also
        updated, it will not be possible to update t1 on-the-fly.
        Due to how the nested loop join algorithm works, when collecting
        we can ignore the condition attached to t1 - a row of t1 is read
        only one time.
      */
      if (update_tables->next_local)
      {
        for (uint i= 1; i < join->tables; ++i)
        {
          JOIN_TAB *tab= join->best_ref[i];
          if (tab->condition())
            tab->condition()->walk(&Item::add_field_to_set_processor,
                      Item::enum_walk(Item::WALK_POSTFIX | Item::WALK_SUBQUERY),
                      reinterpret_cast<uchar *>(main_table));
          /*
            On top of checking conditions, we need to check conditions
            referenced by index lookup on the following tables. They implement
            conditions too, but their corresponding search conditions might
            have been optimized away. The second table is an exception: even if
            rows are read from it using index lookup which references a column
            of main_table, the implementation of ref access will see the
            before-update value;
            consider this flow of a nested loop join:
            read a row from main_table and:
            - init ref access (cp_buffer_from_ref() in join_read_always_key()):
              copy referenced value from main_table into 2nd table's ref buffer
            - look up a first row in 2nd table (join_read_always_key)
              - if it joins, update row of main_table on the fly
            - look up a second row in 2nd table (join_read_next_same).
            Because cp_buffer_from_ref() is not called again, the before-update
            value of the row of main_table is still in the 2nd table's ref
            buffer. So the lookup is not influenced by the just-done update of
            main_table.
          */
          if (tab > join->join_tab + 1)
          {
            for (uint i= 0; i < tab->ref().key_parts; i++)
            {
              Item *ref_item= tab->ref().items[i];
              if ((table_ref->map() & ref_item->used_tables()) != 0)
                ref_item->walk(&Item::add_field_to_set_processor,
                      Item::enum_walk(Item::WALK_POSTFIX | Item::WALK_SUBQUERY),
                      reinterpret_cast<uchar *>(main_table));
            }
          }
        }
      }
      if (safe_update_on_fly(thd, join->best_ref[0], table_ref, all_tables))
      {
        table->mark_columns_needed_for_update(true/*mark_binlog_columns=true*/);
        table_to_update= table;			// Update table on the fly
	continue;
      }
    }
    table->mark_columns_needed_for_update(true/*mark_binlog_columns=true*/);
    /*
      enable uncacheable flag if we update a view with check option
      and check option has a subselect, otherwise, the check option
      can be evaluated after the subselect was freed as independent
      (See full_local in JOIN::join_free()).
    */
    if (table_ref->check_option && !join->select_lex->uncacheable)
    {
      SELECT_LEX_UNIT *tmp_unit;
      SELECT_LEX *sl;
      for (tmp_unit= join->select_lex->first_inner_unit();
           tmp_unit;
           tmp_unit= tmp_unit->next_unit())
      {
        for (sl= tmp_unit->first_select(); sl; sl= sl->next_select())
        {
          if (sl->master_unit()->item)
          {
            join->select_lex->uncacheable|= UNCACHEABLE_CHECKOPTION;
            goto loop_end;
          }
        }
      }
    }
loop_end:

    if (table_ref->table == first_table_for_update->table &&
        table_ref->check_option)
    {
      table_map unupdated_tables= table_ref->check_option->used_tables() &
                                  ~first_table_for_update->map();
      for (TABLE_LIST *tbl_ref =leaves;
           unupdated_tables && tbl_ref;
           tbl_ref= tbl_ref->next_leaf)
      {
        if (unupdated_tables & tbl_ref->map())
          unupdated_tables&= ~tbl_ref->map();
        else
          continue;
        if (unupdated_check_opt_tables.push_back(tbl_ref->table))
          DBUG_RETURN(1);
      }
    }

    tmp_param= tmp_table_param+cnt;

    /*
      Create a temporary table to store all fields that are changed for this
      table. The first field in the temporary table is a pointer to the
      original row so that we can find and update it. For the updatable
      VIEW a few following fields are rowids of tables used in the CHECK
      OPTION condition.
    */

    List_iterator_fast<TABLE> tbl_it(unupdated_check_opt_tables);
    TABLE *tbl= table;
    do
    {
      /*
        Signal each table (including tables referenced by WITH CHECK OPTION
        clause) for which we will store row position in the temporary table
        that we need a position to be read first.
      */
      tbl->prepare_for_position();

      Field_string *field= new Field_string(tbl->file->ref_length, 0,
                                            tbl->alias, &my_charset_bin);
      if (!field)
        DBUG_RETURN(1);
      field->init(tbl);
      /*
        The field will be converted to varstring when creating tmp table if
        table to be updated was created by mysql 4.1. Deny this.
      */
      field->can_alter_field_type= 0;
      Item_field *ifield= new Item_field((Field *) field);
      if (!ifield)
         DBUG_RETURN(1);
      ifield->maybe_null= 0;
      if (temp_fields.push_back(ifield))
        DBUG_RETURN(1);
    } while ((tbl= tbl_it++));

    temp_fields.concat(fields_for_table[cnt]);

    /* Make an unique key over the first field to avoid duplicated updates */
    memset(&group, 0, sizeof(group));
    group.direction= ORDER::ORDER_ASC;
    group.item= temp_fields.head_ref();

    tmp_param->quick_group=1;
    tmp_param->field_count=temp_fields.elements;
    tmp_param->group_parts=1;
    tmp_param->group_length= table->file->ref_length;
    /* small table, ignore SQL_BIG_TABLES */
    my_bool save_big_tables= thd->variables.big_tables; 
    thd->variables.big_tables= FALSE;
    tmp_tables[cnt]=create_tmp_table(thd, tmp_param, temp_fields,
                                     &group, 0, 0,
                                     TMP_TABLE_ALL_COLUMNS, HA_POS_ERROR, "");
    thd->variables.big_tables= save_big_tables;
    if (!tmp_tables[cnt])
      DBUG_RETURN(1);

    /*
      Pass a table triggers pointer (Table_trigger_dispatcher *) from
      the original table to the new temporary table. This pointer will be used
      inside the method Query_result_update::send_data() to determine temporary
      nullability flag for the temporary table's fields. It will be done before
      calling fill_record() to assign values to the temporary table's fields.
    */
    tmp_tables[cnt]->triggers= table->triggers;
    tmp_tables[cnt]->file->extra(HA_EXTRA_WRITE_CACHE);
  }
  DBUG_RETURN(0);
}


Query_result_update::~Query_result_update()
{
  TABLE_LIST *table;
  for (table= update_tables ; table; table= table->next_local)
  {
    table->table->no_cache= 0;
    if (thd->lex->is_ignore())
      table->table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
  }

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
  DBUG_ASSERT(trans_safe || !updated ||
              thd->get_transaction()->cannot_safely_rollback(
                Transaction_ctx::STMT));

  if (update_operations != NULL)
    for (uint i= 0; i < table_count; i++)
      delete update_operations[i];
}


bool Query_result_update::send_data(List<Item> &not_used_values)
{
  TABLE_LIST *cur_table;
  DBUG_ENTER("Query_result_update::send_data");

  for (cur_table= update_tables; cur_table; cur_table= cur_table->next_local)
  {
    TABLE *table= cur_table->table;
    uint offset= cur_table->shared;
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

    if (table == table_to_update)
    {
      table->status|= STATUS_UPDATED;
      store_record(table,record[1]);
      if (fill_record_n_invoke_before_triggers(thd, update_operations[offset],
                                               *fields_for_table[offset],
                                               *values_for_table[offset],
                                               table,
                                               TRG_EVENT_UPDATE, 0))
	DBUG_RETURN(1);

      /*
        Reset the table->auto_increment_field_not_null as it is valid for
        only one row.
      */
      table->auto_increment_field_not_null= FALSE;
      found++;
      int error= 0;
      if (!records_are_comparable(table) || compare_records(table))
      {
        update_operations[offset]->set_function_defaults(table);

        if ((error= cur_table->view_check_option(thd)) !=
            VIEW_CHECK_OK)
        {
          found--;
          if (error == VIEW_CHECK_SKIP)
            continue;
          else if (error == VIEW_CHECK_ERROR)
            DBUG_RETURN(1);
        }
        if (!updated++)
        {
          /*
            Inform the main table that we are going to update the table even
            while we may be scanning it.  This will flush the read cache
            if it's used.
          */
          main_table->file->extra(HA_EXTRA_PREPARE_FOR_UPDATE);
        }
        if ((error=table->file->ha_update_row(table->record[1],
                                              table->record[0])) &&
            error != HA_ERR_RECORD_IS_THE_SAME)
        {
          updated--;
          myf error_flags= MYF(0);
          if (table->file->is_fatal_error(error))
            error_flags|= ME_FATALERROR;

          table->file->print_error(error, error_flags);

          /* Errors could be downgraded to warning by IGNORE */
          if (thd->is_error())
            DBUG_RETURN(1);
        }
        else
        {
          if (error == HA_ERR_RECORD_IS_THE_SAME)
          {
            error= 0;
            updated--;
          }
          /* non-transactional or transactional table got modified   */
          /* either Query_result_update class' flag is raised in its branch */
          if (table->file->has_transactions())
            transactional_tables= TRUE;
          else
          {
            trans_safe= FALSE;
            thd->get_transaction()->mark_modified_non_trans_table(
              Transaction_ctx::STMT);
          }
        }
      }
      if (!error && table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_UPDATE,
                                            TRG_ACTION_AFTER, TRUE))
        DBUG_RETURN(1);
    }
    else
    {
      int error;
      TABLE *tmp_table= tmp_tables[offset];
      /*
       For updatable VIEW store rowid of the updated table and
       rowids of tables used in the CHECK OPTION condition.
      */
      uint field_num= 0;
      List_iterator_fast<TABLE> tbl_it(unupdated_check_opt_tables);
      TABLE *tbl= table;
      do
      {
        tbl->file->position(tbl->record[0]);
        memcpy((char*) tmp_table->visible_field_ptr()[field_num]->ptr,
               (char*) tbl->file->ref, tbl->file->ref_length);
        /*
         For outer joins a rowid field may have no NOT_NULL_FLAG,
         so we have to reset NULL bit for this field.
         (set_notnull() resets NULL bit only if available).
        */
        tmp_table->visible_field_ptr()[field_num]->set_notnull();
        field_num++;
      } while ((tbl= tbl_it++));

      /*
        If there are triggers in an original table the temporary table based on
        then enable temporary nullability for temporary table's fields.
      */
      if (tmp_table->triggers)
      {
        for (Field** modified_fields= tmp_table->visible_field_ptr() + 1 +
                                      unupdated_check_opt_tables.elements;
            *modified_fields; ++modified_fields)
        {
          (*modified_fields)->set_tmp_nullable();
        }
      }

      /* Store regular updated fields in the row. */
      fill_record(thd, tmp_table,
                  tmp_table->visible_field_ptr() +
                  1 + unupdated_check_opt_tables.elements,
                  *values_for_table[offset], NULL, NULL);

      /* Write row, ignoring duplicated updates to a row */
      error= tmp_table->file->ha_write_row(tmp_table->record[0]);
      if (error != HA_ERR_FOUND_DUPP_KEY && error != HA_ERR_FOUND_DUPP_UNIQUE)
      {
        if (error &&
            create_ondisk_from_heap(thd, tmp_table,
                                         tmp_table_param[offset].start_recinfo,
                                         &tmp_table_param[offset].recinfo,
                                         error, TRUE, NULL))
        {
          do_update= 0;
	  DBUG_RETURN(1);			// Not a table_is_full error
	}
        found++;
      }
    }
  }
  DBUG_RETURN(0);
}


void Query_result_update::send_error(uint errcode,const char *err)
{
  /* First send error what ever it is ... */
  my_error(errcode, MYF(0), err);
}



static void invalidate_update_tables(THD *thd, TABLE_LIST *update_tables)
{
  for (TABLE_LIST *tl= update_tables; tl != NULL; tl= tl->next_local)
  {
    query_cache.invalidate_single(thd, tl->updatable_base_table(), 1);
  }
}


void Query_result_update::abort_result_set()
{
  /* the error was handled or nothing deleted and no side effects return */
  if (error_handled ||
      (!thd->get_transaction()->cannot_safely_rollback(
        Transaction_ctx::STMT) && !updated))
    return;

  /* Something already updated so we have to invalidate cache */
  if (updated)
    invalidate_update_tables(thd, update_tables);

  /*
    If all tables that has been updated are trans safe then just do rollback.
    If not attempt to do remaining updates.
  */

  if (! trans_safe)
  {
    DBUG_ASSERT(thd->get_transaction()->cannot_safely_rollback(
      Transaction_ctx::STMT));
    if (do_update && table_count > 1)
    {
      /* Add warning here */
      /* 
         todo/fixme: do_update() is never called with the arg 1.
         should it change the signature to become argless?
      */
      (void) do_updates();
    }
  }
  if (thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT))
  {
    /*
      The query has to binlog because there's a modified non-transactional table
      either from the query's list or via a stored routine: bug#13270,23333
    */
    if (mysql_bin_log.is_open())
    {
      /*
        THD::killed status might not have been set ON at time of an error
        got caught and if happens later the killed error is written
        into repl event.
      */
      int errcode= query_error_code(thd, thd->killed == THD::NOT_KILLED);
      /* the error of binary logging is ignored */
      (void)thd->binlog_query(THD::ROW_QUERY_TYPE,
                              thd->query().str, thd->query().length,
                              transactional_tables, false, false, errcode);
    }
  }
  DBUG_ASSERT(trans_safe || !updated ||
              thd->get_transaction()->cannot_safely_rollback(
                Transaction_ctx::STMT));
}


int Query_result_update::do_updates()
{
  TABLE_LIST *cur_table;
  int local_error= 0;
  ha_rows org_updated;
  TABLE *table, *tmp_table;
  List_iterator_fast<TABLE> check_opt_it(unupdated_check_opt_tables);
  myf error_flags= MYF(0);                      /**< Flag for fatal errors */

  DBUG_ENTER("Query_result_update::do_updates");

  do_update= 0;					// Don't retry this function

  if (!found)
  {
    /*
      If the binary log is on, we still need to check
      if there are transactional tables involved. If
      there are mark the transactional_tables flag correctly.

      This flag determines whether the writes go into the
      transactional or non transactional cache, even if they
      do not change any table, they are still written into
      the binary log when the format is STMT or MIXED.
    */
    if(mysql_bin_log.is_open())
    {
      for (cur_table= update_tables; cur_table;
           cur_table= cur_table->next_local)
      {
        table = cur_table->table;
        transactional_tables= transactional_tables ||
                              table->file->has_transactions();
      }
    }
    DBUG_RETURN(0);
  }
  for (cur_table= update_tables; cur_table; cur_table= cur_table->next_local)
  {
    uint offset= cur_table->shared;

    table = cur_table->table;

    /*
      Always update the flag if - even if not updating the table,
      when the binary log is ON. This will allow the right binlog
      cache - stmt or trx cache - to be selected when logging
      innefective statementst to the binary log (in STMT or MIXED
      mode logging).
     */
    if (mysql_bin_log.is_open())
      transactional_tables= transactional_tables || table->file->has_transactions();

    if (table == table_to_update)
      continue;                                        // Already updated
    org_updated= updated;
    tmp_table= tmp_tables[cur_table->shared];
    tmp_table->file->extra(HA_EXTRA_CACHE);	// Change to read cache
    if ((local_error= table->file->ha_rnd_init(0)))
    {
      if (table->file->is_fatal_error(local_error))
        error_flags|= ME_FATALERROR;

      table->file->print_error(local_error, error_flags);
      goto err;
    }

    table->file->extra(HA_EXTRA_NO_CACHE);

    check_opt_it.rewind();
    while(TABLE *tbl= check_opt_it++)
    {
      if (tbl->file->ha_rnd_init(1))
        // No known handler error code present, print_error makes no sense
        goto err;
      tbl->file->extra(HA_EXTRA_CACHE);
    }

    /*
      Setup copy functions to copy fields from temporary table
    */
    List_iterator_fast<Item> field_it(*fields_for_table[offset]);
    Field **field= tmp_table->visible_field_ptr() +
                   1 + unupdated_check_opt_tables.elements; // Skip row pointers
    Copy_field *copy_field_ptr= copy_field, *copy_field_end;
    for ( ; *field ; field++)
    {
      Item_field *item= (Item_field* ) field_it++;
      (copy_field_ptr++)->set(item->field, *field, 0);
    }
    copy_field_end=copy_field_ptr;

    if ((local_error = tmp_table->file->ha_rnd_init(1)))
    {
      if (table->file->is_fatal_error(local_error))
        error_flags|= ME_FATALERROR;

      table->file->print_error(local_error, error_flags);
      goto err;
    }

    for (;;)
    {
      if (thd->killed && trans_safe)
        // No known handler error code present, print_error makes no sense
        goto err;
      if ((local_error=tmp_table->file->ha_rnd_next(tmp_table->record[0])))
      {
        if (local_error == HA_ERR_END_OF_FILE)
          break;
        if (local_error == HA_ERR_RECORD_DELETED)
          continue;                             // May happen on dup key
        if (table->file->is_fatal_error(local_error))
          error_flags|= ME_FATALERROR;

        table->file->print_error(local_error, error_flags);
        goto err;
      }

      /* call ha_rnd_pos() using rowids from temporary table */
      check_opt_it.rewind();
      TABLE *tbl= table;
      uint field_num= 0;
      do
      {
        if((local_error=
              tbl->file->ha_rnd_pos(tbl->record[0],
                                    (uchar *) tmp_table->visible_field_ptr()[field_num]->ptr)))
        {
          if (table->file->is_fatal_error(local_error))
            error_flags|= ME_FATALERROR;

          table->file->print_error(local_error, error_flags);
          goto err;
        }
        field_num++;
      } while((tbl= check_opt_it++));

      table->status|= STATUS_UPDATED;
      store_record(table,record[1]);

      /* Copy data from temporary table to current table */
      for (copy_field_ptr=copy_field;
	   copy_field_ptr != copy_field_end;
	   copy_field_ptr++)
        copy_field_ptr->invoke_do_copy(copy_field_ptr);

      // The above didn't update generated columns
      if (table->vfield &&
          update_generated_write_fields(table->write_set, table))
        goto err;

      if (table->triggers)
      {
        bool rc= table->triggers->process_triggers(thd, TRG_EVENT_UPDATE,
                                                   TRG_ACTION_BEFORE, true);

        // Trigger might have changed dependencies of generated columns
        if (!rc && table->vfield &&
            update_generated_write_fields(table->write_set, table))
          goto err;

        table->triggers->disable_fields_temporary_nullability();

        if (rc || check_record(thd, table->field))
          goto err;
      }

      if (!records_are_comparable(table) || compare_records(table))
      {
        update_operations[offset]->set_function_defaults(table);
        int error;
        if ((error= cur_table->view_check_option(thd)) !=
            VIEW_CHECK_OK)
        {
          if (error == VIEW_CHECK_SKIP)
            continue;
          else if (error == VIEW_CHECK_ERROR)
            // No known handler error code present, print_error makes no sense
            goto err;
        }
        local_error= table->file->ha_update_row(table->record[1],
                                                table->record[0]);
        if (!local_error)
          updated++;
        else if (local_error == HA_ERR_RECORD_IS_THE_SAME)
          local_error= 0;
        else
	{
          if (table->file->is_fatal_error(local_error))
            error_flags|= ME_FATALERROR;

          table->file->print_error(local_error, error_flags);
          /* Errors could be downgraded to warning by IGNORE */
          if (thd->is_error())
            goto err;
        }
      }

      if (!local_error && table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_UPDATE,
                                            TRG_ACTION_AFTER, TRUE))
        goto err;
    }

    if (updated != org_updated)
    {
      if (!table->file->has_transactions())
      {
        trans_safe= FALSE;				// Can't do safe rollback
        thd->get_transaction()->mark_modified_non_trans_table(
          Transaction_ctx::STMT);
      }
    }
    (void) table->file->ha_rnd_end();
    (void) tmp_table->file->ha_rnd_end();
    check_opt_it.rewind();
    while (TABLE *tbl= check_opt_it++)
        tbl->file->ha_rnd_end();

  }
  DBUG_RETURN(0);

err:
  if (table->file->inited)
    (void) table->file->ha_rnd_end();
  if (tmp_table->file->inited)
    (void) tmp_table->file->ha_rnd_end();
  check_opt_it.rewind();
  while (TABLE *tbl= check_opt_it++)
  {
    if (tbl->file->inited)
      (void) tbl->file->ha_rnd_end();
  }

  if (updated != org_updated)
  {
    if (table->file->has_transactions())
      transactional_tables= TRUE;
    else
    {
      trans_safe= FALSE;
      thd->get_transaction()->mark_modified_non_trans_table(
        Transaction_ctx::STMT);
    }
  }
  DBUG_RETURN(1);
}


/* out: 1 if error, 0 if success */

bool Query_result_update::send_eof()
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  ulonglong id;
  THD::killed_state killed_status= THD::NOT_KILLED;
  DBUG_ENTER("Query_result_update::send_eof");
  THD_STAGE_INFO(thd, stage_updating_reference_tables);

  /* 
     Does updates for the last n - 1 tables, returns 0 if ok;
     error takes into account killed status gained in do_updates()
  */
  int local_error= thd->is_error();
  if (!local_error)
    local_error = (table_count) ? do_updates() : 0;
  /*
    if local_error is not set ON until after do_updates() then
    later carried out killing should not affect binlogging.
  */
  killed_status= (local_error == 0)? THD::NOT_KILLED : thd->killed;
  THD_STAGE_INFO(thd, stage_end);

  /* We must invalidate the query cache before binlog writing and
  ha_autocommit_... */

  if (updated)
    invalidate_update_tables(thd, update_tables);

  /*
    Write the SQL statement to the binlog if we updated
    rows and we succeeded or if we updated some non
    transactional tables.
    
    The query has to binlog because there's a modified non-transactional table
    either from the query's list or via a stored routine: bug#13270,23333
  */

  if (local_error == 0 ||
      thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT))
  {
    if (mysql_bin_log.is_open())
    {
      int errcode= 0;
      if (local_error == 0)
        thd->clear_error();
      else
        errcode= query_error_code(thd, killed_status == THD::NOT_KILLED);
      if (thd->binlog_query(THD::ROW_QUERY_TYPE,
                            thd->query().str, thd->query().length,
                            transactional_tables, FALSE, FALSE, errcode))
      {
	local_error= 1;				// Rollback update
      }
    }
  }
  DBUG_ASSERT(trans_safe || !updated || 
              thd->get_transaction()->cannot_safely_rollback(
                Transaction_ctx::STMT));

  if (local_error != 0)
    error_handled= TRUE; // to force early leave from ::send_error()

  if (local_error > 0) // if the above log write did not fail ...
  {
    /* Safety: If we haven't got an error before (can happen in do_updates) */
    my_message(ER_UNKNOWN_ERROR, "An error occured in multi-table update",
	       MYF(0));
    DBUG_RETURN(TRUE);
  }

  id= thd->arg_of_last_insert_id_function ?
    thd->first_successful_insert_id_in_prev_stmt : 0;
  my_snprintf(buff, sizeof(buff), ER(ER_UPDATE_INFO),
              (long) found, (long) updated,
              (long) thd->get_stmt_da()->current_statement_cond_count());
  ::my_ok(thd, thd->get_protocol()->has_client_capability(CLIENT_FOUND_ROWS) ?
          found : updated, id, buff);
  DBUG_RETURN(FALSE);
}


/**
  Try to execute UPDATE as single-table UPDATE

  If the UPDATE statement can be executed as a single-table UPDATE,
  the do it. Otherwise defer its execution for the further
  execute_multi_table_update() call outside this function and set
  switch_to_multitable to "true".

  @param thd                            Current THD.
  @param [out] switch_to_multitable     True if the UPDATE statement can't
                                        be evaluated as single-table.
                                        Note: undefined if the function
                                        returns "true".

  @return true on error
  @return false otherwise.
*/
bool Sql_cmd_update::try_single_table_update(THD *thd,
                                             bool *switch_to_multitable)
{
  LEX *const lex= thd->lex;
  SELECT_LEX *const select_lex= lex->select_lex;
  SELECT_LEX_UNIT *const unit= lex->unit;
  TABLE_LIST *const first_table= select_lex->get_table_list();
  TABLE_LIST *const all_tables= first_table;

  if (update_precheck(thd, all_tables))
    return true;

  /*
    UPDATE IGNORE can be unsafe. We therefore use row based
    logging if mixed or row based logging is available.
    TODO: Check if the order of the output of the select statement is
    deterministic. Waiting for BUG#42415
  */
  if (lex->is_ignore())
    lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_UPDATE_IGNORE);

  DBUG_ASSERT(select_lex->offset_limit == 0);
  unit->set_limit(select_lex);
  MYSQL_UPDATE_START(const_cast<char*>(thd->query().str));

  // Need to open to check for multi-update
  if (open_tables_for_query(thd, all_tables, 0) ||
      mysql_update_prepare_table(thd, select_lex) ||
      run_before_dml_hook(thd))
    return true;

  if (!all_tables->is_multiple_tables())
  {
    /* Push ignore / strict error handler */
    Ignore_error_handler ignore_handler;
    Strict_error_handler strict_handler;
    if (thd->lex->is_ignore())
      thd->push_internal_handler(&ignore_handler);
    else if (thd->is_strict_mode())
      thd->push_internal_handler(&strict_handler);

    ha_rows found= 0, updated= 0;
    const bool res= mysql_update(thd, select_lex->item_list,
                                 update_value_list,
                                 unit->select_limit_cnt,
                                 lex->duplicates,
                                 &found, &updated);

    /* Pop ignore / strict error handler */
    if (thd->lex->is_ignore() || thd->is_strict_mode())
      thd->pop_internal_handler();
    MYSQL_UPDATE_DONE(res, found, updated);
    if (res)
      return true;
    else
    {
      *switch_to_multitable= false;
      return false;
    }
  }
  else
  {
    DBUG_ASSERT(all_tables->is_view());
    DBUG_PRINT("info", ("Switch to multi-update"));
    if (!thd->in_sub_stmt)
      thd->query_plan.set_query_plan(SQLCOM_UPDATE_MULTI, lex,
                                     !thd->stmt_arena->is_conventional());
    MYSQL_UPDATE_DONE(true, 0, 0);
    *switch_to_multitable= true;
    return false;
  }
}


bool Sql_cmd_update::execute_multi_table_update(THD *thd)
{
  bool res= false;
  LEX *const lex= thd->lex;
  SELECT_LEX *const select_lex= lex->select_lex;
  TABLE_LIST *const first_table= select_lex->get_table_list();
  TABLE_LIST *const all_tables= first_table;

#ifdef HAVE_REPLICATION
  /* have table map for update for multi-update statement (BUG#37051) */
  /*
    Save the thd->table_map_for_update value as a result of the
    Query_log_event::do_apply_event() function call --
    the mysql_multi_update_prepare() function will reset it below.
  */
  const bool have_table_map_for_update= thd->table_map_for_update;
#endif

  res= mysql_multi_update_prepare(thd);

#ifdef HAVE_REPLICATION
  /* Check slave filtering rules */
  if (unlikely(thd->slave_thread && !have_table_map_for_update))
  {
    if (all_tables_not_ok(thd, all_tables))
    {
      if (res!= 0)
      {
        res= 0;             /* don't care of prev failure  */
        thd->clear_error(); /* filters are of highest prior */
      }
      /* we warn the slave SQL thread */
      my_error(ER_SLAVE_IGNORED_TABLE, MYF(0));
      return res;
    }
    if (res)
      return res;
  }
  else
  {
#endif /* HAVE_REPLICATION */
    if (res)
      return res;
    if (check_readonly(thd, false) &&
        some_non_temp_table_to_be_updated(thd, all_tables))
    {
      err_readonly(thd);
      return res;
    }
#ifdef HAVE_REPLICATION
  }  /* unlikely */
#endif
  {
    /* Push ignore / strict error handler */
    Ignore_error_handler ignore_handler;
    Strict_error_handler strict_handler;
    if (thd->lex->is_ignore())
      thd->push_internal_handler(&ignore_handler);
    else if (thd->is_strict_mode())
      thd->push_internal_handler(&strict_handler);

    Query_result_update *result_obj;
    MYSQL_MULTI_UPDATE_START(const_cast<char*>(thd->query().str));
    res= mysql_multi_update(thd,
                            &select_lex->item_list,
                            &update_value_list,
                            lex->duplicates,
                            select_lex,
                            &result_obj);

    /* Pop ignore / strict error handler */
    if (thd->lex->is_ignore() || thd->is_strict_mode())
      thd->pop_internal_handler();

    if (result_obj)
    {
      MYSQL_MULTI_UPDATE_DONE(res, result_obj->num_found(),
                              result_obj->num_updated());
      res= FALSE; /* Ignore errors here */
      delete result_obj;
    }
    else
    {
      MYSQL_MULTI_UPDATE_DONE(1, 0, 0);
    }
  }
  return res;
}


bool Sql_cmd_update::execute(THD *thd)
{
  if (thd->lex->sql_command == SQLCOM_UPDATE_MULTI)
  {
    return multi_update_precheck(thd, thd->lex->select_lex->get_table_list()) ||
           execute_multi_table_update(thd);
  }

  bool switch_to_multitable;
  if (try_single_table_update(thd, &switch_to_multitable))
    return true;
  if (switch_to_multitable)
  {
    sql_command= SQLCOM_UPDATE_MULTI;
    return execute_multi_table_update(thd);
  }
  return false;
}


bool Sql_cmd_update::prepared_statement_test(THD *thd)
{
  DBUG_ASSERT(thd->lex->query_tables == thd->lex->select_lex->get_table_list());
  if (thd->lex->sql_command == SQLCOM_UPDATE)
  {
    int res= mysql_test_update(thd);
    /* mysql_test_update returns 2 if we need to switch to multi-update */
    if (res == 2)
      return select_like_stmt_cmd_test(thd, this, OPTION_SETUP_TABLES_DONE);
    else
      return MY_TEST(res);
  }
  else
  {
    return multi_update_precheck(thd, thd->lex->query_tables) ||
           select_like_stmt_cmd_test(thd, this, OPTION_SETUP_TABLES_DONE);
  }
}
