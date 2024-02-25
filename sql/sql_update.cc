/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

// Handle UPDATE queries (both single- and multi-table).

#include "sql/sql_update.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

#include "field_types.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "mem_root_deque.h"  // mem_root_deque
#include "my_alloc.h"
#include "my_base.h"
#include "my_bit.h"  // my_count_bits
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "prealloced_array.h"  // Prealloced_array
#include "scope_guard.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // check_grant, check_access
#include "sql/binlog.h"            // mysql_bin_log
#include "sql/debug_sync.h"        // DEBUG_SYNC
#include "sql/derror.h"            // ER_THD
#include "sql/field.h"             // Field
#include "sql/filesort.h"          // Filesort
#include "sql/handler.h"
#include "sql/item.h"            // Item
#include "sql/item_json_func.h"  // Item_json_func
#include "sql/item_subselect.h"  // Item_subselect
#include "sql/iterators/basic_row_iterators.h"
#include "sql/iterators/row_iterator.h"
#include "sql/iterators/timing_iterator.h"
#include "sql/iterators/update_rows_iterator.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/walk_access_paths.h"
#include "sql/key.h"  // is_key_used
#include "sql/key_spec.h"
#include "sql/locked_tables_list.h"
#include "sql/mdl.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"       // stage_... mysql_tmpdir
#include "sql/opt_explain.h"  // Modification_plan
#include "sql/opt_explain_format.h"
#include "sql/opt_trace.h"  // Opt_trace_object
#include "sql/opt_trace_context.h"
#include "sql/parse_tree_node_base.h"
#include "sql/partition_info.h"  // partition_info
#include "sql/protocol.h"
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/range_optimizer/partition_pruning.h"
#include "sql/range_optimizer/path_helpers.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/select_lex_visitor.h"
#include "sql/sql_array.h"
#include "sql/sql_base.h"  // check_record, fill_record
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_data_change.h"
#include "sql/sql_delete.h"
#include "sql/sql_error.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"  // build_equal_items, substitute_gc
#include "sql/sql_partition.h"  // partition_key_modified
#include "sql/sql_resolver.h"   // setup_order
#include "sql/sql_select.h"
#include "sql/sql_tmp_table.h"  // create_tmp_table
#include "sql/sql_view.h"       // check_key_in_view
#include "sql/system_variables.h"
#include "sql/table.h"                     // TABLE
#include "sql/table_trigger_dispatcher.h"  // Table_trigger_dispatcher
#include "sql/temp_table_param.h"
#include "sql/thd_raii.h"
#include "sql/transaction_info.h"
#include "sql/trigger_chain.h"
#include "sql/trigger_def.h"
#include "sql/visible_fields.h"
#include "template_utils.h"
#include "thr_lock.h"

class COND_EQUAL;

static bool prepare_partial_update(Opt_trace_context *trace,
                                   const mem_root_deque<Item *> &fields,
                                   const mem_root_deque<Item *> &values);

bool Sql_cmd_update::precheck(THD *thd) {
  DBUG_TRACE;

  if (!multitable) {
    if (check_one_table_access(thd, UPDATE_ACL, lex->query_tables)) return true;
  } else {
    /*
      Ensure that we have UPDATE or SELECT privilege for each table
      The exact privilege is checked in mysql_multi_update()
    */
    for (Table_ref *tr = lex->query_tables; tr; tr = tr->next_global) {
      /*
        "uses_materialization()" covers the case where a prepared statement is
        executed and a view is decided to be materialized during preparation.
        @todo: Check whether this properly handles the case when privileges
        for a view is revoked during execution of a prepared statement.
      */
      if (tr->is_derived() || tr->uses_materialization())
        tr->grant.privilege = SELECT_ACL;
      else {
        auto chk = [&](long want_access) {
          const bool ignore_errors = (want_access == UPDATE_ACL);
          return check_access(thd, want_access, tr->db, &tr->grant.privilege,
                              &tr->grant.m_internal, false, ignore_errors) ||
                 check_grant(thd, want_access, tr, false, 1, ignore_errors);
        };
        if (chk(UPDATE_ACL)) {
          // Verify that lock has not yet been acquired for request.
          assert(tr->mdl_request.ticket == nullptr);
          // If there is no UPDATE privilege on this table we want to avoid
          // acquiring SHARED_WRITE MDL. It is safe to change lock type to
          // SHARE_READ in such a case, since attempts to update column in
          // this table will be rejected by later column-level privilege
          // check.
          // If a prepared statement/stored procedure is re-executed after
          // the tables and privileges have been modified such that the
          // ps/sp will attempt to update the SR-locked table, a
          // reprepare/recompilation will restore the mdl request to SW.
          //
          // There are two possible cases in which we could try to update the SR
          // locked table, here, in theory:
          //
          // 1) There was no movement of columns between tables mentioned in
          // UPDATE statement.
          //    We didn't have privilege on the table which we try to update
          //    before but i was granted. This case is not relevant for PS, as
          //    in it we will get an error during prepare phase and no PS will
          //    be created. For SP first execution of statement will fail during
          //    prepare phase as well, however, SP will stay around, and the
          //    second execution attempt will just cause another prepare.
          // 2) Some columns has been moved between tables mentioned in UPDATE
          // statement,
          //    so the table which was read-only initially (and on which we
          //    didn't have UPDATE privilege then, so it was SR-locked), now
          //    needs to be updated. The change in table definition will be
          //    detected at open_tables() time and cause re-prepare in this
          //    case.
          tr->mdl_request.type = MDL_SHARED_READ;
          if (chk(SELECT_ACL)) return true;
        }
      }  // else
    }    // for
  }      // else
  return false;
}

bool Sql_cmd_update::check_privileges(THD *thd) {
  DBUG_TRACE;

  Query_block *const select = lex->query_block;

  if (check_all_table_privileges(thd)) return true;

  // @todo: replace individual calls for privilege checking with
  // Query_block::check_column_privileges(), but only when fields_list
  // is no longer reused for an UPDATE statement.

  if (select->m_current_table_nest &&
      check_privileges_for_join(thd, select->m_current_table_nest))
    return true;

  thd->want_privilege = SELECT_ACL;
  if (select->where_cond() != nullptr &&
      select->where_cond()->walk(&Item::check_column_privileges,
                                 enum_walk::PREFIX, pointer_cast<uchar *>(thd)))
    return true;

  if (check_privileges_for_list(thd, original_fields, UPDATE_ACL)) return true;

  if (check_privileges_for_list(thd, *update_value_list, SELECT_ACL))
    return true;

  for (ORDER *order = select->order_list.first; order; order = order->next) {
    if ((*order->item)
            ->walk(&Item::check_column_privileges, enum_walk::PREFIX,
                   pointer_cast<uchar *>(thd)))
      return true;
  }

  thd->want_privilege = SELECT_ACL;
  if (select->check_privileges_for_subqueries(thd)) return true;

  return false;
}

/**
   True if the table's input and output record buffers are comparable using
   compare_records(TABLE*).
 */
bool records_are_comparable(const TABLE *table) {
  return ((table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ) == 0) ||
         bitmap_is_subset(table->write_set, table->read_set);
}

/**
   Compares the input and output record buffers of the table to see if a row
   has changed. The algorithm iterates over updated columns and if they are
   nullable compares NULL bits in the buffer before comparing actual
   data. Special care must be taken to compare only the relevant NULL bits and
   mask out all others as they may be undefined. The storage engine will not
   and should not touch them.

   @param table The table to evaluate.

   @return true if row has changed.
   @return false otherwise.
*/
bool compare_records(const TABLE *table) {
  assert(records_are_comparable(table));

  if ((table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ) != 0) {
    /*
      Storage engine may not have read all columns of the record.  Fields
      (including NULL bits) not in the write_set may not have been read and
      can therefore not be compared.
    */
    for (Field **ptr = table->field; *ptr != nullptr; ptr++) {
      Field *field = *ptr;
      if (bitmap_is_set(table->write_set, field->field_index())) {
        if (field->is_nullable()) {
          uchar null_byte_index = field->null_offset();

          if (((table->record[0][null_byte_index]) & field->null_bit) !=
              ((table->record[1][null_byte_index]) & field->null_bit))
            return true;
        }
        if (field->cmp_binary_offset(table->s->rec_buff_length)) return true;
      }
    }
    return false;
  }

  /*
     The storage engine has read all columns, so it's safe to compare all bits
     including those not in the write_set. This is cheaper than the
     field-by-field comparison done above.
  */
  if (table->s->blob_fields + table->s->varchar_fields == 0)
    // Fixed-size record: do bitwise comparison of the records
    return cmp_record(table, record[1]);
  /* Compare null bits */
  if (memcmp(table->null_flags, table->null_flags + table->s->rec_buff_length,
             table->s->null_bytes))
    return true;  // Diff in NULL value
  /* Compare updated fields */
  for (Field **ptr = table->field; *ptr; ptr++) {
    if (bitmap_is_set(table->write_set, (*ptr)->field_index()) &&
        (*ptr)->cmp_binary_offset(table->s->rec_buff_length))
      return true;
  }
  return false;
}

/**
  Check that all fields are base table columns.
  Replace columns from views with base table columns.

  @param      thd              thread handler
  @param      items            Items for check

  @return false if success, true if error (OOM)
*/

bool Sql_cmd_update::make_base_table_fields(THD *thd,
                                            mem_root_deque<Item *> *items) {
  for (auto it = items->begin(); it != items->end(); ++it) {
    Item *item = *it;
    assert(!item->hidden);

    // Save original item for privilege checking
    original_fields.push_back(item);

    /*
      Make temporary copy of Item_field, to avoid influence of changing
      result_field on Item_ref which refer on this field
    */
    Item_field *const base_table_field = item->field_for_view_update();
    assert(base_table_field != nullptr);

    Item_field *const cloned_field = new Item_field(thd, base_table_field);
    if (cloned_field == nullptr) return true; /* purecov: inspected */

    *it = cloned_field;
    // WL#6570 remove-after-qa
    assert(thd->stmt_arena->is_regular() || !thd->lex->is_exec_started());
  }
  return false;
}

/**
  Check if all expressions in list are constant expressions

  @param[in] values List of expressions

  @retval true Only constant expressions
  @retval false At least one non-constant expression
*/

static bool check_constant_expressions(const mem_root_deque<Item *> &values) {
  DBUG_TRACE;

  for (Item *value : values) {
    if (!value->const_for_execution()) {
      DBUG_PRINT("exit", ("expression is not constant"));
      return false;
    }
  }
  DBUG_PRINT("exit", ("expression is constant"));
  return true;
}

/**
  Perform an update to a set of rows in a single table.

  @param thd     Thread handler

  @returns false if success, true if error
*/

bool Sql_cmd_update::update_single_table(THD *thd) {
  DBUG_TRACE;

  myf error_flags = MYF(0); /**< Flag for fatal errors */
  /*
    Most recent handler error
    =  1: Some non-handler error
    =  0: Success
    = -1: No more rows to process, or reached limit
  */
  int error = 0;

  Query_block *const query_block = lex->query_block;
  Query_expression *const unit = lex->unit;
  Table_ref *const table_list = query_block->get_table_list();
  Table_ref *const update_table_ref = table_list->updatable_base_table();
  TABLE *const table = update_table_ref->table;

  assert(table->pos_in_table_list == update_table_ref);

  const bool transactional_table = table->file->has_transactions();

  const bool has_update_triggers =
      table->triggers && table->triggers->has_update_triggers();

  const bool has_after_triggers =
      has_update_triggers &&
      table->triggers->has_triggers(TRG_EVENT_UPDATE, TRG_ACTION_AFTER);

  Opt_trace_context *const trace = &thd->opt_trace;

  if (unit->set_limit(thd, unit->global_parameters()))
    return true; /* purecov: inspected */

  ha_rows limit = unit->select_limit_cnt;
  const bool using_limit = limit != HA_POS_ERROR;

  if (limit == 0 && thd->lex->is_explain()) {
    Modification_plan plan(thd, MT_UPDATE, table, "LIMIT is zero", true, 0);
    bool err = explain_single_table_modification(thd, thd, &plan, query_block);
    return err;
  }

  // Used to track whether there are no rows that need to be read
  bool no_rows = limit == 0;

  THD::killed_state killed_status = THD::NOT_KILLED;
  assert(CountHiddenFields(query_block->fields) == 0);
  COPY_INFO update(COPY_INFO::UPDATE_OPERATION, &query_block->fields,
                   update_value_list);
  if (update.add_function_default_columns(table, table->write_set)) return true;

  const bool safe_update = thd->variables.option_bits & OPTION_SAFE_UPDATES;

  assert(!(table->all_partitions_pruned_away || m_empty_query));

  Item *conds = nullptr;
  ORDER *order = query_block->order_list.first;
  if (!no_rows && query_block->get_optimizable_conditions(thd, &conds, nullptr))
    return true; /* purecov: inspected */

  /*
    See if we can substitute expressions with equivalent generated
    columns in the WHERE and ORDER BY clauses of the UPDATE statement.
    It is unclear if this is best to do before or after the other
    substitutions performed by substitute_for_best_equal_field(). Do
    it here for now, to keep it consistent with how multi-table
    updates are optimized in JOIN::optimize().
  */
  if (conds || order)
    static_cast<void>(substitute_gc(thd, query_block, conds, nullptr, order));

  if (conds != nullptr) {
    if (table_list->check_option) {
      // See the explanation in multi-table UPDATE code path
      // (Query_result_update::prepare).
      table_list->check_option->walk(&Item::disable_constant_propagation,
                                     enum_walk::POSTFIX, nullptr);
    }
    COND_EQUAL *cond_equal = nullptr;
    Item::cond_result result;
    if (optimize_cond(thd, &conds, &cond_equal,
                      query_block->m_current_table_nest, &result))
      return true;

    if (result == Item::COND_FALSE) {
      no_rows = true;  // Impossible WHERE
      if (thd->lex->is_explain()) {
        Modification_plan plan(thd, MT_UPDATE, table, "Impossible WHERE", true,
                               0);
        bool err =
            explain_single_table_modification(thd, thd, &plan, query_block);
        return err;
      }
    }
    if (conds != nullptr) {
      conds = substitute_for_best_equal_field(thd, conds, cond_equal, nullptr);
      if (conds == nullptr) return true;

      conds->update_used_tables();
    }
  }

  /*
    Also try a second time after locking, to prune when subqueries and
    stored programs can be evaluated.
  */
  if (table->part_info && !no_rows) {
    if (prune_partitions(thd, table, query_block, conds))
      return true; /* purecov: inspected */
    if (table->all_partitions_pruned_away) {
      no_rows = true;

      if (thd->lex->is_explain()) {
        Modification_plan plan(thd, MT_UPDATE, table,
                               "No matching rows after partition pruning", true,
                               0);
        bool err =
            explain_single_table_modification(thd, thd, &plan, query_block);
        return err;
      }
      my_ok(thd);
      return false;
    }
  }
  // Initialize the cost model that will be used for this table
  table->init_cost_model(thd->cost_model());

  /* Update the table->file->stats.records number */
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  table->mark_columns_needed_for_update(thd,
                                        false /*mark_binlog_columns=false*/);

  AccessPath *range_scan = nullptr;
  join_type type = JT_UNKNOWN;

  auto cleanup = create_scope_guard([&range_scan, table] {
    destroy(range_scan);
    table->set_keyread(false);
    table->file->ha_index_or_rnd_end();
    free_io_cache(table);
    filesort_free_buffers(table, true);
  });

  if (conds &&
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN)) {
    table->file->cond_push(conds);
  }

  {  // Enter scope for optimizer trace wrapper
    Opt_trace_object wrapper(&thd->opt_trace);
    wrapper.add_utf8_table(update_table_ref);

    if (!no_rows && conds != nullptr) {
      Key_map keys_to_use(Key_map::ALL_BITS), needed_reg_dummy;
      MEM_ROOT temp_mem_root(key_memory_test_quick_select_exec,
                             thd->variables.range_alloc_block_size);
      no_rows = test_quick_select(
                    thd, thd->mem_root, &temp_mem_root, keys_to_use, 0, 0,
                    limit, safe_update, ORDER_NOT_RELEVANT, table,
                    /*skip_records_in_range=*/false, conds, &needed_reg_dummy,
                    table->force_index, query_block, &range_scan) < 0;
      if (thd->is_error()) return true;
    }
    if (no_rows) {
      if (thd->lex->is_explain()) {
        Modification_plan plan(thd, MT_UPDATE, table, "Impossible WHERE", true,
                               0);
        bool err =
            explain_single_table_modification(thd, thd, &plan, query_block);
        return err;
      }

      char buff[MYSQL_ERRMSG_SIZE];
      snprintf(buff, sizeof(buff), ER_THD(thd, ER_UPDATE_INFO), 0L, 0L,
               (long)thd->get_stmt_da()->current_statement_cond_count());
      my_ok(thd, 0, 0, buff);

      DBUG_PRINT("info", ("0 records updated"));
      return false;
    }
  }  // Ends scope for optimizer trace wrapper

  /* If running in safe sql mode, don't allow updates without keys */
  if (table->quick_keys.is_clear_all()) {
    thd->server_status |= SERVER_QUERY_NO_INDEX_USED;

    /*
      No safe update error will be returned if:
      1) Statement is an EXPLAIN OR
      2) LIMIT is present.

      Append the first warning (if any) to the error message. Allows the user
      to understand why index access couldn't be chosen.
    */
    if (!lex->is_explain() && safe_update && !using_limit) {
      my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE, MYF(0),
               thd->get_stmt_da()->get_first_condition_message());
      return true;
    }
  }
  if (query_block->has_ft_funcs() && init_ftfuncs(thd, query_block))
    return true; /* purecov: inspected */

  if (conds != nullptr) table->update_const_key_parts(conds);

  order = simple_remove_const(order, conds);
  bool need_sort;
  bool reverse = false;
  bool used_key_is_modified = false;
  uint used_index;
  {
    ORDER_with_src order_src(order, ESC_ORDER_BY, /*const_optimized=*/true);
    used_index = get_index_for_order(&order_src, table, limit, range_scan,
                                     &need_sort, &reverse);
    if (range_scan != nullptr) {
      // May have been changed by get_index_for_order().
      type = calc_join_type(range_scan);
    }
  }
  if (need_sort) {  // Assign table scan index to check below for modified key
                    // fields:
    used_index = table->file->key_used_on_scan;
  }
  if (used_index != MAX_KEY) {  // Check if we are modifying a key that we are
                                // used to search with:
    used_key_is_modified = is_key_used(table, used_index, table->write_set);
  } else if (range_scan) {
    /*
      select->range_scan != NULL and used_index == MAX_KEY happens for index
      merge and should be handled in a different way.
    */
    used_key_is_modified = (!unique_key_range(range_scan) &&
                            uses_index_on_fields(range_scan, table->write_set));
  }

  if (table->part_info)
    used_key_is_modified |= table->part_info->num_partitions_used() > 1 &&
                            partition_key_modified(table, table->write_set);

  const bool using_filesort = order && need_sort;

  table->mark_columns_per_binlog_row_image(thd);

  if (prepare_partial_update(trace, query_block->fields, *update_value_list))
    return true; /* purecov: inspected */

  if (table->setup_partial_update()) return true; /* purecov: inspected */

  ha_rows updated_rows = 0;
  ha_rows found_rows = 0;

  unique_ptr_destroy_only<Filesort> fsort;
  unique_ptr_destroy_only<RowIterator> iterator;

  {  // Start of scope for Modification_plan
    ha_rows rows;
    if (range_scan)
      rows = range_scan->num_output_rows();
    else if (!conds && !need_sort && limit != HA_POS_ERROR)
      rows = limit;
    else {
      update_table_ref->fetch_number_of_rows();
      rows = table->file->stats.records;
    }
    DEBUG_SYNC(thd, "before_single_update");
    Modification_plan plan(thd, MT_UPDATE, table, type, range_scan, conds,
                           used_index, limit,
                           (!using_filesort && (used_key_is_modified || order)),
                           using_filesort, used_key_is_modified, rows);
    DEBUG_SYNC(thd, "planned_single_update");
    if (thd->lex->is_explain()) {
      bool err =
          explain_single_table_modification(thd, thd, &plan, query_block);
      return err;
    }

    if (thd->lex->is_ignore()) table->file->ha_extra(HA_EXTRA_IGNORE_DUP_KEY);

    if (used_key_is_modified || order) {
      /*
        We can't update table directly;  We must first search after all
        matching rows before updating the table!
      */

      /* note: We avoid sorting if we sort on the used index */
      if (using_filesort) {
        /*
          Doing an ORDER BY;  Let filesort find and sort the rows we are going
          to update
          NOTE: filesort will call table->prepare_for_position()
        */
        JOIN join(thd, query_block);  // Only for holding examined_rows.
        AccessPath *path = create_table_access_path(
            thd, table, range_scan, /*table_ref=*/nullptr,
            /*position=*/nullptr, /*count_examined_rows=*/true);

        if (conds != nullptr) {
          path = NewFilterAccessPath(thd, path, conds);
        }

        // Force filesort to sort by position.
        fsort.reset(new (thd->mem_root) Filesort(
            thd, {table}, /*keep_buffers=*/false, order, limit,
            /*remove_duplicates=*/false,
            /*force_sort_rowids=*/true, /*unwrap_rollup=*/false));
        path = NewSortAccessPath(thd, path, fsort.get(), order,
                                 /*count_examined_rows=*/false);
        iterator = CreateIteratorFromAccessPath(
            thd, path, &join, /*eligible_for_batch_mode=*/true);
        // Prevent cleanup in JOIN::destroy() and in the cleanup condition
        // guard, to avoid double-destroy of the SortingIterator.
        table->sorting_iterator = nullptr;

        if (iterator == nullptr || iterator->Init()) return true;
        thd->inc_examined_row_count(join.examined_rows);

        /*
          Filesort has already found and selected the rows we want to update,
          so we don't need the where clause
        */
        destroy(range_scan);
        range_scan = nullptr;
        conds = nullptr;
      } else {
        /*
          We are doing a search on a key that is updated. In this case
          we go through the matching rows, save a pointer to them and
          update these in a separate loop based on the pointer. In the end,
          we get a result file that looks exactly like what filesort uses
          internally, which allows us to read from it
          using SortFileIndirectIterator.

          TODO: Find something less ugly.
         */
        Key_map covering_keys_for_cond;  // @todo - move this
        if (used_index < MAX_KEY && covering_keys_for_cond.is_set(used_index))
          table->set_keyread(true);

        table->prepare_for_position();
        table->file->try_semi_consistent_read(true);
        auto end_semi_consistent_read = create_scope_guard(
            [table] { table->file->try_semi_consistent_read(false); });

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

        AccessPath *path;
        if (used_index == MAX_KEY || range_scan) {
          path = create_table_access_path(thd, table, range_scan,
                                          /*table_ref=*/nullptr,
                                          /*position=*/nullptr,
                                          /*count_examined_rows=*/false);
        } else {
          empty_record(table);
          path = NewIndexScanAccessPath(thd, table, used_index,
                                        /*use_order=*/true, reverse,
                                        /*count_examined_rows=*/false);
        }

        iterator = CreateIteratorFromAccessPath(
            thd, path, /*join=*/nullptr, /*eligible_for_batch_mode=*/true);
        // Prevent cleanup in JOIN::destroy() and in the cleanup condition
        // guard, to avoid double-destroy of the SortingIterator.
        table->sorting_iterator = nullptr;

        if (iterator == nullptr || iterator->Init()) {
          return true;
        }

        THD_STAGE_INFO(thd, stage_searching_rows_for_update);
        ha_rows tmp_limit = limit;

        IO_CACHE *tempfile =
            (IO_CACHE *)my_malloc(key_memory_TABLE_sort_io_cache,
                                  sizeof(IO_CACHE), MYF(MY_FAE | MY_ZEROFILL));

        if (open_cached_file(tempfile, mysql_tmpdir, TEMP_PREFIX,
                             DISK_BUFFER_SIZE, MYF(MY_WME))) {
          my_free(tempfile);
          return true;
        }

        while (!(error = iterator->Read()) && !thd->killed) {
          assert(!thd->is_error());
          thd->inc_examined_row_count(1);

          if (conds != nullptr) {
            const bool skip_record = conds->val_int() == 0;
            if (thd->is_error()) {
              error = 1;
              /*
                Don't try unlocking the row if skip_record reported an error
                since in this case the transaction might have been rolled back
                already.
              */
              break;
            }
            if (skip_record) {
              table->file->unlock_row();
              continue;
            }
          }
          if (table->file->was_semi_consistent_read())
            continue; /* repeat the read of the same row if it still exists */

          table->file->position(table->record[0]);
          if (my_b_write(tempfile, table->file->ref, table->file->ref_length)) {
            error = 1; /* purecov: inspected */
            break;     /* purecov: inspected */
          }
          if (!--limit && using_limit) {
            error = -1;
            break;
          }
        }

        if (thd->killed && !error)  // Aborted
          error = 1;                /* purecov: inspected */
        limit = tmp_limit;
        end_semi_consistent_read.rollback();
        if (used_index < MAX_KEY && covering_keys_for_cond.is_set(used_index))
          table->set_keyread(false);
        table->file->ha_index_or_rnd_end();
        iterator.reset();

        // Change reader to use tempfile
        if (reinit_io_cache(tempfile, READ_CACHE, 0L, false, false))
          error = 1; /* purecov: inspected */

        if (error >= 0) {
          close_cached_file(tempfile);
          my_free(tempfile);
          return error > 0;
        }

        iterator = NewIterator<SortFileIndirectIterator>(
            thd, thd->mem_root, Mem_root_array<TABLE *>{table}, tempfile,
            /*ignore_not_found_rows=*/false, /*has_null_flags=*/false,
            /*examined_rows=*/nullptr);
        if (iterator->Init()) return true;

        destroy(range_scan);
        range_scan = nullptr;
        conds = nullptr;
      }
    } else {
      // No ORDER BY or updated key underway, so we can use a regular read.
      iterator =
          init_table_iterator(thd, table, range_scan,
                              /*table_ref=*/nullptr, /*position=*/nullptr,
                              /*ignore_not_found_rows=*/false,
                              /*count_examined_rows=*/false);
      if (iterator == nullptr) return true; /* purecov: inspected */
    }

    table->file->try_semi_consistent_read(true);
    auto end_semi_consistent_read = create_scope_guard(
        [table] { table->file->try_semi_consistent_read(false); });

    /*
      Generate an error (in TRADITIONAL mode) or warning
      when trying to set a NOT NULL field to NULL.
    */
    thd->check_for_truncated_fields = CHECK_FIELD_WARN;
    thd->num_truncated_fields = 0L;
    THD_STAGE_INFO(thd, stage_updating);

    bool will_batch;
    /// read_removal is only used by NDB storage engine
    bool read_removal = false;

    if (has_after_triggers) {
      /*
        The table has AFTER UPDATE triggers that might access to subject
        table and therefore might need update to be done immediately.
        So we turn-off the batching.
      */
      (void)table->file->ha_extra(HA_EXTRA_UPDATE_CANNOT_BATCH);
      will_batch = false;
    } else {
      // No after update triggers, attempt to start bulk update
      will_batch = !table->file->start_bulk_update();
    }
    if ((table->file->ha_table_flags() & HA_READ_BEFORE_WRITE_REMOVAL) &&
        !thd->lex->is_ignore() && !using_limit && !has_update_triggers &&
        range_scan && ::used_index(range_scan) != MAX_KEY &&
        check_constant_expressions(*update_value_list))
      read_removal = table->check_read_removal(::used_index(range_scan));

    // If the update is batched, we cannot do partial update, so turn it off.
    if (will_batch) table->cleanup_partial_update(); /* purecov: inspected */

    uint dup_key_found;

    while (true) {
      error = iterator->Read();
      if (error || thd->killed) break;
      thd->inc_examined_row_count(1);
      if (conds != nullptr) {
        const bool skip_record = conds->val_int() == 0;
        if (thd->is_error()) {
          error = 1;
          break;
        }
        if (skip_record) {
          table->file
              ->unlock_row();  // Row failed condition check, release lock
          thd->get_stmt_da()->inc_current_row_for_condition();
          continue;
        }
      }
      assert(!thd->is_error());

      if (table->file->was_semi_consistent_read())
        /*
          Reviewer: iterator is reading from the to-be-updated table or
          from a tmp file.
          In the latter case, if the condition of this if() is true,
          it is wrong to "continue"; indeed this will pick up the _next_ row of
          tempfile; it will not re-read-with-lock the current row of tempfile,
          as tempfile is not an InnoDB table and not doing semi consistent read.
          If that happens, we're potentially skipping a row which was found
          matching! OTOH, as the rowid was written to the tempfile, it means it
          matched and thus we have already re-read it in the tempfile-write loop
          above and thus locked it. So we shouldn't come here. How about adding
          an assertion that if reading from tmp file we shouldn't come here?
        */
        continue; /* repeat the read of the same row if it still exists */

      table->clear_partial_update_diffs();

      store_record(table, record[1]);
      bool is_row_changed = false;
      if (fill_record_n_invoke_before_triggers(
              thd, &update, query_block->fields, *update_value_list, table,
              TRG_EVENT_UPDATE, 0, false, &is_row_changed)) {
        error = 1;
        break;
      }
      found_rows++;

      if (is_row_changed) {
        /*
          Default function and default expression values are filled before
          evaluating the view check option. Check option on view using table(s)
          with default function and default expression breaks otherwise.

          It is safe to not invoke CHECK OPTION for VIEW if records are same.
          In this case the row is coming from the view and thus should satisfy
          the CHECK OPTION.
        */
        int check_result = table_list->view_check_option(thd);
        if (check_result != VIEW_CHECK_OK) {
          if (check_result == VIEW_CHECK_SKIP)
            continue;
          else if (check_result == VIEW_CHECK_ERROR) {
            error = 1;
            break;
          }
        }

        /*
          Existing rows in table should normally satisfy CHECK constraints. So
          it should be safe to check constraints only for rows that has really
          changed (i.e. after compare_records()).

          In future, once addition/enabling of CHECK constraints without their
          validation is supported, we might encounter old rows which do not
          satisfy CHECK constraints currently enabled. However, rejecting no-op
          updates to such invalid pre-existing rows won't make them valid and is
          probably going to be confusing for users. So it makes sense to stick
          to current behavior.
        */
        if (invoke_table_check_constraints(thd, table)) {
          if (thd->is_error()) {
            error = 1;
            break;
          }
          // continue when IGNORE clause is used.
          continue;
        }

        if (will_batch) {
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
          error = table->file->ha_bulk_update_row(
              table->record[1], table->record[0], &dup_key_found);
          limit += dup_key_found;
          updated_rows -= dup_key_found;
        } else {
          /* Non-batched update */
          error =
              table->file->ha_update_row(table->record[1], table->record[0]);
        }
        if (error == 0)
          updated_rows++;
        else if (error == HA_ERR_RECORD_IS_THE_SAME)
          error = 0;
        else {
          if (table->file->is_fatal_error(error)) error_flags |= ME_FATALERROR;

          table->file->print_error(error, error_flags);

          // The error can have been downgraded to warning by IGNORE.
          if (thd->is_error()) break;
        }
      }

      if (!error && has_after_triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_UPDATE,
                                            TRG_ACTION_AFTER, true)) {
        error = 1;
        break;
      }

      if (!--limit && using_limit) {
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
            ((error = table->file->exec_bulk_update(&dup_key_found)) ||
             dup_key_found)) {
          if (error) {
            /*
              ndbcluster is the only handler that returns an error at this
              juncture
            */
            assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER);
            if (table->file->is_fatal_error(error))
              error_flags |= ME_FATALERROR;

            table->file->print_error(error, error_flags);
            error = 1;
            break;
          }
          /* purecov: begin inspected */
          /*
            Either an error was found and we are ignoring errors or there were
            duplicate keys found with HA_IGNORE_DUP_KEY enabled. In both cases
            we need to correct the counters and continue the loop.
          */

          /*
            Note that NDB disables batching when duplicate keys are to be
            ignored. Any duplicate key found will result in an error returned
            above.
          */
          assert(false);
          limit = dup_key_found;  // limit is 0 when we get here so need to +
          updated_rows -= dup_key_found;
          /* purecov: end */
        } else {
          error = -1;  // Simulate end of file
          break;
        }
      }

      thd->get_stmt_da()->inc_current_row_for_condition();
      assert(!thd->is_error());
      if (thd->is_error()) {
        error = 1;
        break;
      }
    }
    end_semi_consistent_read.rollback();

    dup_key_found = 0;
    /*
      Caching the killed status to pass as the arg to query event constructor;
      The cached value can not change whereas the killed status can
      (externally) since this point and change of the latter won't affect
      binlogging.
      It's assumed that if an error was set in combination with an effective
      killed status then the error is due to killing.
    */
    killed_status = thd->killed;  // get the status of the atomic
    // simulated killing after the loop must be ineffective for binlogging
    DBUG_EXECUTE_IF("simulate_kill_bug27571",
                    { thd->killed = THD::KILL_QUERY; };);
    if (killed_status != THD::NOT_KILLED) error = 1;

    int loc_error;
    if (error && will_batch &&
        (loc_error = table->file->exec_bulk_update(&dup_key_found)))
    /*
      An error has occurred when a batched update was performed and returned
      an error indication. It cannot be an allowed duplicate key error since
      we require the batching handler to treat this as a normal behavior.

      Otherwise we simply remove the number of duplicate keys records found
      in the batched update.
    */
    {
      /* purecov: begin inspected */
      error_flags = MYF(0);
      if (table->file->is_fatal_error(loc_error)) error_flags |= ME_FATALERROR;

      table->file->print_error(loc_error, error_flags);
      error = 1;
      /* purecov: end */
    } else
      updated_rows -= dup_key_found;
    if (will_batch) table->file->end_bulk_update();

    if (read_removal) {
      /* Only handler knows how many records really was written */
      updated_rows = table->file->end_read_removal();
      if (!records_are_comparable(table)) found_rows = updated_rows;
    }

  }  // End of scope for Modification_plan

  if (!transactional_table && updated_rows > 0)
    thd->get_transaction()->mark_modified_non_trans_table(
        Transaction_ctx::STMT);

  iterator.reset();

  /*
    error < 0 means really no error at all: we processed all rows until the
    last one without error. error > 0 means an error (e.g. unique key
    violation and no IGNORE or REPLACE). error == 0 is also an error (if
    preparing the record or invoking before triggers fails). See
    ha_autocommit_or_rollback(error>=0) and return error>=0 below.
    Sometimes we want to binlog even if we updated no rows, in case user used
    it to be sure master and slave are in same state.
  */
  if ((error < 0) ||
      thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT)) {
    if (mysql_bin_log.is_open()) {
      int errcode = 0;
      if (error < 0)
        thd->clear_error();
      else
        errcode = query_error_code(thd, killed_status == THD::NOT_KILLED);

      if (thd->binlog_query(THD::ROW_QUERY_TYPE, thd->query().str,
                            thd->query().length, transactional_table, false,
                            false, errcode)) {
        error = 1;  // Rollback update
      }
    }
  }
  assert(transactional_table || updated_rows == 0 ||
         thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT));

  // If LAST_INSERT_ID(X) was used, report X
  const ulonglong id = thd->arg_of_last_insert_id_function
                           ? thd->first_successful_insert_id_in_prev_stmt
                           : 0;

  if (error < 0) {
    char buff[MYSQL_ERRMSG_SIZE];
    snprintf(buff, sizeof(buff), ER_THD(thd, ER_UPDATE_INFO), (long)found_rows,
             (long)updated_rows,
             (long)thd->get_stmt_da()->current_statement_cond_count());
    my_ok(thd,
          thd->get_protocol()->has_client_capability(CLIENT_FOUND_ROWS)
              ? found_rows
              : updated_rows,
          id, buff);
    DBUG_PRINT("info", ("%ld records updated", (long)updated_rows));
  }
  thd->check_for_truncated_fields = CHECK_FIELD_IGNORE;
  thd->current_found_rows = found_rows;

  assert(CountHiddenFields(*update_value_list) == 0);

  // Following test is disabled, as we get RQG errors that are hard to debug
  // assert((error >= 0) == thd->is_error());
  return error >= 0 || thd->is_error();
}

/***************************************************************************
  Update multiple tables from join
***************************************************************************/

/*
  Get table map for list of Item_field
*/

static table_map get_table_map(const mem_root_deque<Item *> &items) {
  table_map map = 0;

  for (Item *item : items) {
    map |= down_cast<Item_field *>(item)->used_tables();
  }
  DBUG_PRINT("info", ("table_map: 0x%08lx", (long)map));
  return map;
}

/// Returns the outermost (leftmost) table of a join.
static TABLE *GetOutermostTable(const JOIN *join) {
  // The hypergraph optimizer will need to get the table from the access path.
  // The old optimizer can usually find it in the access path too, except if the
  // outermost table is a const table, since const tables may not be visible in
  // the access path tree.
  if (!join->thd->lex->using_hypergraph_optimizer) {
    assert(join->qep_tab != nullptr);
    return join->qep_tab[0].table();
  }

  // This assumes WalkTablesUnderAccessPath() walks depth-first and
  // left-to-right, so that it finds the leftmost table first.
  TABLE *found_table = nullptr;
  WalkTablesUnderAccessPath(
      join->root_access_path(),
      [&found_table](TABLE *table) {
        if (found_table == nullptr) {
          found_table = table;
        }
        return found_table != nullptr;
      },
      /*include_pruned_tables=*/true);
  return found_table;
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
static bool unsafe_key_update(Table_ref *leaves, table_map tables_for_update) {
  Table_ref *tl = leaves;

  for (tl = leaves; tl; tl = tl->next_leaf) {
    if (tl->map() & tables_for_update) {
      TABLE *table1 = tl->table;
      bool primkey_clustered = (table1->file->primary_key_is_clustered() &&
                                table1->s->primary_key != MAX_KEY);

      bool table_partitioned = (table1->part_info != nullptr);

      if (!table_partitioned && !primkey_clustered) continue;

      for (Table_ref *tl2 = tl->next_leaf; tl2; tl2 = tl2->next_leaf) {
        /*
          Look at "next" tables only since all previous tables have
          already been checked
        */
        TABLE *table2 = tl2->table;
        if (tl2->map() & tables_for_update && table1->s == table2->s) {
          // A table is updated through two aliases
          if (table_partitioned &&
              (partition_key_modified(table1, table1->write_set) ||
               partition_key_modified(table2, table2->write_set))) {
            // Partitioned key is updated
            my_error(
                ER_MULTI_UPDATE_KEY_CONFLICT, MYF(0),
                tl->belong_to_view ? tl->belong_to_view->alias : tl->alias,
                tl2->belong_to_view ? tl2->belong_to_view->alias : tl2->alias);
            return true;
          }

          if (primkey_clustered) {
            // The primary key can cover multiple columns
            KEY key_info = table1->key_info[table1->s->primary_key];
            KEY_PART_INFO *key_part = key_info.key_part;
            KEY_PART_INFO *key_part_end =
                key_part + key_info.user_defined_key_parts;

            for (; key_part != key_part_end; ++key_part) {
              if (bitmap_is_set(table1->write_set, key_part->fieldnr - 1) ||
                  bitmap_is_set(table2->write_set, key_part->fieldnr - 1)) {
                // Clustered primary key is updated
                my_error(
                    ER_MULTI_UPDATE_KEY_CONFLICT, MYF(0),
                    tl->belong_to_view ? tl->belong_to_view->alias : tl->alias,
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

/// Check if a list of Items contains an Item whose type is JSON.
static bool has_json_columns(const mem_root_deque<Item *> &items) {
  for (Item *item : items) {
    if (item->data_type() == MYSQL_TYPE_JSON) return true;
  }
  return false;
}

/**
  Mark the columns that can possibly be updated in-place using partial update.

  Only JSON columns can be updated in-place, and only if all the updates of the
  column are on the form

      json_col = JSON_SET(json_col, ...)

      json_col = JSON_REPLACE(json_col, ...)

      json_col = JSON_REMOVE(json_col, ...)

  Even though a column is marked for partial update, it is not necessarily
  updated as a partial update during execution. It depends on the actual data
  in the column if it is possible to do it as a partial update. Also, for
  multi-table updates, it is only possible to perform partial updates in the
  first table of the join operation, and it is not determined until later (in
  Query_result_update::optimize()) which table it is.

  @param trace   the optimizer trace context
  @param fields  the fields that are updated by the update statement
  @param values  the values they are updated to
  @return false on success, true on error
*/
static bool prepare_partial_update(Opt_trace_context *trace,
                                   const mem_root_deque<Item *> &fields,
                                   const mem_root_deque<Item *> &values) {
  /*
    First check if we have any JSON columns. The only reason we do this, is to
    prevent writing an empty optimizer trace about partial update if there are
    no JSON columns.
  */
  if (!has_json_columns(fields)) return false;

  Opt_trace_object trace_partial_update(trace, "json_partial_update");
  Opt_trace_array trace_rejected(trace, "rejected_columns");

  using Field_array = Prealloced_array<const Field *, 8>;
  Field_array partial_update_fields(PSI_NOT_INSTRUMENTED);
  Field_array rejected_fields(PSI_NOT_INSTRUMENTED);
  auto field_it = VisibleFields(fields).begin();
  auto value_it = values.begin();
  for (; field_it != VisibleFields(fields).end() && value_it != values.end();) {
    Item *field_item = *field_it++;
    Item *value_item = *value_it++;
    // Only consider JSON fields for partial update for now.
    if (field_item->data_type() != MYSQL_TYPE_JSON) continue;

    const Field_json *field =
        down_cast<Field_json *>(down_cast<Item_field *>(field_item)->field);

    if (rejected_fields.count_unique(field) != 0) continue;

    /*
      Function object that adds the current column to the list of rejected
      columns, and possibly traces the rejection if optimizer tracing is
      enabled.
    */
    const auto reject_column = [&](const char *cause) {
      Opt_trace_object trace_obj(trace);
      trace_obj.add_utf8_table(field->table->pos_in_table_list);
      trace_obj.add_utf8("column", field->field_name);
      trace_obj.add_utf8("cause", cause);
      rejected_fields.insert_unique(field);
    };

    if ((field->table->file->ha_table_flags() & HA_BLOB_PARTIAL_UPDATE) == 0) {
      reject_column("Storage engine does not support partial update");
      continue;
    }

    if (!value_item->supports_partial_update(field)) {
      reject_column(
          "Updated using a function that does not support partial "
          "update, or source and target column differ");
      partial_update_fields.erase_unique(field);
      continue;
    }

    partial_update_fields.insert_unique(field);
  }

  if (partial_update_fields.empty()) return false;

  for (const Field *fld : partial_update_fields)
    if (fld->table->mark_column_for_partial_update(fld))
      return true; /* purecov: inspected */

  field_it = VisibleFields(fields).begin();
  value_it = values.begin();
  for (; field_it != VisibleFields(fields).end() && value_it != values.end();) {
    Item *field_item = *field_it++;
    Item *value_item = *value_it++;
    const Field *field = down_cast<Item_field *>(field_item)->field;
    if (field->table->is_marked_for_partial_update(field)) {
      auto json_field = down_cast<const Field_json *>(field);
      auto json_func = down_cast<Item_json_func *>(value_item);
      json_func->mark_for_partial_update(json_field);
    }
  }

  return false;
}

/**
  Decides if a single-table UPDATE/DELETE statement should switch to the
  multi-table code path, if there are subqueries which might benefit from
  semijoin or subquery materialization, and if no feature specific to the
  single-table path are used.

  @param thd         Thread handler
  @param select      Query block
  @param table_list  Table to modify
  @returns true if we should switch
 */
bool should_switch_to_multi_table_if_subqueries(const THD *thd,
                                                const Query_block *select,
                                                const Table_ref *table_list) {
  TABLE *t = table_list->updatable_base_table()->table;
  handler *h = t->file;
  // Secondary engine is never the target of updates here (updates are done
  // to the primary engine and then propagated):
  assert((h->ht->flags & HTON_IS_SECONDARY_ENGINE) == 0);
  // LIMIT, ORDER BY and read-before-write removal are not supported in
  // multi-table UPDATE/DELETE.
  if (select->is_ordered() || select->has_limit() ||
      (h->ha_table_flags() & HA_READ_BEFORE_WRITE_REMOVAL))
    return false;
  /*
    Search for subqueries. Subquery hints and optimizer_switch are taken into
    account. They can serve as a solution is a user really wants to use the
    single-table path, e.g. in case of regression.
  */
  for (Query_expression *unit = select->first_inner_query_expression(); unit;
       unit = unit->next_query_expression()) {
    if (unit->item && (unit->item->substype() == Item_subselect::IN_SUBS ||
                       unit->item->substype() == Item_subselect::EXISTS_SUBS)) {
      auto sub_query_block = unit->first_query_block();
      Subquery_strategy subq_mat = sub_query_block->subquery_strategy(thd);
      if (subq_mat == Subquery_strategy::CANDIDATE_FOR_IN2EXISTS_OR_MAT ||
          subq_mat == Subquery_strategy::SUBQ_MATERIALIZATION)
        return true;
      if (sub_query_block->semijoin_enabled(thd)) return true;
    }
  }
  return false;
}

bool Sql_cmd_update::prepare_inner(THD *thd) {
  DBUG_TRACE;

  Query_block *const select = lex->query_block;
  Table_ref *const table_list = select->get_table_list();

  Table_ref *single_table_updated = nullptr;

  const bool using_lock_tables = thd->locked_tables_mode != LTM_NONE;
  const bool is_single_table_syntax = !multitable;

  assert(select->fields.size() == select->num_visible_fields());
  assert(select->num_visible_fields() == update_value_list->size());

  Mem_root_array<Item_exists_subselect *> sj_candidates_local(thd->mem_root);

  Opt_trace_context *const trace = &thd->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_prepare(trace, "update_preparation");
  trace_prepare.add_select_number(select->select_number);

  if (!select->m_table_nest.empty())
    propagate_nullability(&select->m_table_nest, false);

  if (select->setup_tables(thd, table_list, false))
    return true; /* purecov: inspected */

  thd->want_privilege = SELECT_ACL;
  enum enum_mark_columns mark_used_columns_saved = thd->mark_used_columns;
  thd->mark_used_columns = MARK_COLUMNS_READ;
  if (select->derived_table_count || select->table_func_count) {
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
    if (select->resolve_placeholder_tables(thd, /*apply_semijoin=*/false))
      return true;
    /*
      @todo - This check is a bit primitive and ad-hoc. We have not yet analyzed
      the list of tables that are updated. Perhaps we should wait until that
      list is ready. In that case, we should check for UPDATE and SELECT
      privileges for tables that are updated and SELECT privileges for tables
      that are selected from. However, check_view_privileges() lacks
      functionality for detailed privilege checking.
    */
    if (select->check_view_privileges(thd, UPDATE_ACL, SELECT_ACL)) return true;
  }

  /*
    Updatability test is spread across several places:
    - Target table or view must be updatable (checked below)
    - A view has special requirements with respect to columns being updated
                                          (checked in check_key_in_view)
    - All updated columns must be from an updatable component of a view
                                          (checked in setup_fields)
    - Target table must not be same as one selected from
                                          (checked in unique_table)
  */

  if (!multitable) {
    // Single-table UPDATE, the table must be updatable:
    if (!table_list->is_updatable()) {
      my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "UPDATE");
      return true;
    }
    // Perform multi-table operation if table to be updated is multi-table view
    if (table_list->is_multiple_tables()) multitable = true;
  }

  // The hypergraph optimizer has a unified execution path for single-table and
  // multi-table UPDATE, and does not need to distinguish between the two. This
  // enables it to perform optimizations like sort avoidance and semi-join
  // flattening even if features specific to single-table UPDATE (that is, ORDER
  // BY and LIMIT) are used.
  if (lex->using_hypergraph_optimizer) {
    multitable = true;
  }

  if (!multitable && select->first_inner_query_expression() != nullptr &&
      should_switch_to_multi_table_if_subqueries(thd, select, table_list))
    multitable = true;

  if (multitable) select->set_sj_candidates(&sj_candidates_local);

  if (select->leaf_table_count >= 2 &&
      setup_natural_join_row_types(thd, select->m_current_table_nest,
                                   &select->context))
    return true;

  if (!multitable) {
    select->make_active_options(SELECT_NO_JOIN_CACHE, 0);

    // Identify the single table to be updated
    single_table_updated = table_list->updatable_base_table();
  } else {
    // At this point the update is known to be a multi-table operation.
    // Join buffering and hash join cannot be used as the update operations
    // assume nested loop join (see logic in safe_update_on_fly()).
    select->make_active_options(SELECT_NO_JOIN_CACHE | SELECT_NO_UNLOCK,
                                OPTION_BUFFER_RESULT);

    Prepared_stmt_arena_holder ps_holder(thd);
    result = new (thd->mem_root)
        Query_result_update(&select->fields, update_value_list);
    if (result == nullptr) return true; /* purecov: inspected */

    // The former is for the pre-iterator executor; the latter is for the
    // iterator executor.
    // TODO(sgunders): Get rid of this when we remove Query_result.
    select->set_query_result(result);
    select->master_query_expression()->set_query_result(result);
  }

  lex->allow_sum_func = 0;  // Query block cannot be aggregated

  if (select->setup_conds(thd)) return true;

  if (select->setup_base_ref_items(thd)) return true; /* purecov: inspected */

  if (setup_fields(thd, /*want_privilege=*/UPDATE_ACL,
                   /*allow_sum_func=*/false, /*split_sum_funcs=*/false,
                   /*column_update=*/true, /*typed_items=*/nullptr,
                   &select->fields, Ref_item_array()))
    return true;

  if (make_base_table_fields(thd, &select->fields))
    return true; /* purecov: inspected */

  /*
    Calculate map of tables that are updated based on resolved columns
    in the update field list.
  */
  thd->table_map_for_update = tables_for_update = get_table_map(select->fields);

  uint update_table_count_local = my_count_bits(tables_for_update);

  assert(update_table_count_local > 0);

  /*
    Some tables may be marked for update, even though they have no columns
    that are updated. Adjust "updating" flag based on actual updated columns.
  */
  for (Table_ref *tr = select->leaf_tables; tr; tr = tr->next_leaf) {
    if (tr->map() & tables_for_update) tr->set_updated();
    tr->updating = tr->is_updated();
  }

  if (setup_fields(thd, /*want_privilege=*/SELECT_ACL,
                   /*allow_sum_func=*/false, /*split_sum_funcs=*/false,
                   /*column_update=*/false, &select->fields, update_value_list,
                   Ref_item_array()))
    return true; /* purecov: inspected */

  thd->mark_used_columns = mark_used_columns_saved;

  if (select->resolve_limits(thd)) return true;

  if (!multitable) {
    // Add default values provided by a function, required for part. pruning
    // @todo consolidate with corresponding function in update_single_table()
    COPY_INFO update(COPY_INFO::UPDATE_OPERATION, &select->fields,
                     update_value_list);
    TABLE *table = single_table_updated->table;
    if (update.add_function_default_columns(table, table->write_set))
      return true; /* purecov: inspected */

    if ((table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ) != 0 &&
        update.function_defaults_apply(table))
      /*
        A column is to be set to its ON UPDATE function default only if other
        columns of the row are changing. To know this, we must be able to
        compare the "before" and "after" value of those columns
        (i.e. records_are_comparable() must be true below). Thus, we must read
        those columns:
      */
      // @todo - consolidate with Query_result_update::prepare()
      bitmap_union(table->read_set, table->write_set);

    // UPDATE operations requires full row from base table, disable covering key
    // @todo - Consolidate this with multi-table ops
    table->covering_keys.clear_all();

    /*
      This must be done before partition pruning, since prune_partitions()
      uses table->write_set to determine if locks can be pruned.
    */
    if (table->triggers && table->triggers->mark_fields(TRG_EVENT_UPDATE))
      return true;
  }

  for (Table_ref *tl = select->leaf_tables; tl; tl = tl->next_leaf) {
    if (tl->is_updated()) {
      // Cannot update a table if the storage engine does not support update.
      if (tl->table->file->ha_table_flags() & HA_UPDATE_NOT_SUPPORTED) {
        my_error(ER_ILLEGAL_HA, MYF(0), tl->table_name);
        return true;
      }

      if ((tl->table->vfield || tl->table->gen_def_fields_ptr != nullptr) &&
          validate_gc_assignment(select->fields, *update_value_list, tl->table))
        return true; /* purecov: inspected */

      // Mark all containing view references as updating
      for (Table_ref *ref = tl; ref != nullptr; ref = ref->referencing_view)
        ref->updating = true;

      // Check that table is unique, updatability has already been checked.
      if (select->first_execution && check_key_in_view(thd, tl, tl)) {
        my_error(ER_NON_UPDATABLE_TABLE, MYF(0), tl->top_table()->alias,
                 "UPDATE");
        return true;
      }

      DBUG_PRINT("info", ("setting table `%s` for update", tl->alias));
    } else {
      DBUG_PRINT("info", ("setting table `%s` for read-only", tl->alias));
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
      assert(tl->prelocking_placeholder == false);
      tl->set_lock({read_lock_type_for_table(thd, lex, tl, true), THR_DEFAULT});
      /* Update TABLE::lock_type accordingly. */
      if (!tl->is_placeholder() && !using_lock_tables)
        tl->table->reginfo.lock_type = tl->lock_descriptor().type;
    }
  }

  if (update_table_count_local > 1 &&
      unsafe_key_update(select->leaf_tables, tables_for_update))
    return true;

  /*
    Check that tables being updated are not used in a subquery, but
    skip all tables of the UPDATE query block itself
  */
  select->exclude_from_table_unique_test = true;

  for (Table_ref *tr = select->leaf_tables; tr; tr = tr->next_leaf) {
    if (tr->is_updated()) {
      Table_ref *duplicate = unique_table(tr, select->leaf_tables, false);
      if (duplicate != nullptr) {
        update_non_unique_table_error(select->leaf_tables, "UPDATE", duplicate);
        return true;
      }
    }
  }

  /*
    Set exclude_from_table_unique_test value back to false. It is needed for
    further check whether to use record cache.
  */
  select->exclude_from_table_unique_test = false;

  /* check single table update for view compound from several tables */
  for (Table_ref *tl = table_list; tl; tl = tl->next_local) {
    if (tl->is_merged()) {
      assert(tl->is_view_or_derived());
      Table_ref *for_update = nullptr;
      if (tl->check_single_table(&for_update, tables_for_update)) {
        my_error(ER_VIEW_MULTIUPDATE, MYF(0), tl->db, tl->table_name);
        return true;
      }
    }
  }

  /* @todo: downgrade the metadata locks here. */

  /*
    ORDER BY and LIMIT is only allowed for single table update, so check. Even
    through it looks syntactically like a single table update, it may still be
    a multi-table update hidden if we update a multi-table view.
  */
  if (!is_single_table_syntax || table_list->is_multiple_tables()) {
    if (select->order_list.elements) {
      my_error(ER_WRONG_USAGE, MYF(0), "UPDATE", "ORDER BY");
      return true;
    }
    if (select->select_limit) {
      my_error(ER_WRONG_USAGE, MYF(0), "UPDATE", "LIMIT");
      return true;
    }
  }
  if (select->order_list.first) {
    mem_root_deque<Item *> fields(thd->mem_root);
    if (setup_order(thd, select->base_ref_items, table_list, &fields,
                    select->order_list.first))
      return true;
  }

  assert(select->having_cond() == nullptr && select->group_list.elements == 0);

  if (select->has_ft_funcs() && setup_ftfuncs(thd, select))
    return true; /* purecov: inspected */

  if (select->query_result() &&
      select->query_result()->prepare(thd, select->fields, lex->unit))
    return true; /* purecov: inspected */

  Opt_trace_array trace_steps(trace, "steps");
  opt_trace_print_expanded_query(thd, select, &trace_wrapper);

  if (select->has_sj_candidates() && select->flatten_subqueries(thd))
    return true; /* purecov: inspected */

  select->set_sj_candidates(nullptr);

  if (select->apply_local_transforms(thd, true))
    return true; /* purecov: inspected */

  if (select->is_empty_query()) set_empty_query();

  select->master_query_expression()->set_prepared();

  assert(CountHiddenFields(*update_value_list) == 0);

  return false;
}

bool Sql_cmd_update::execute_inner(THD *thd) {
  // Binary logging happens at execution, and needs this information:
  thd->table_map_for_update = tables_for_update;

  if (is_empty_query()) {
    if (lex->is_explain()) {
      Modification_plan plan(thd, MT_UPDATE, /*table_arg=*/nullptr,
                             "No matching rows after partition pruning", true,
                             0);
      return explain_single_table_modification(thd, thd, &plan,
                                               lex->query_block);
    }
    my_ok(thd);
    return false;
  }
  return multitable ? Sql_cmd_dml::execute_inner(thd)
                    : update_single_table(thd);
}

/*
  Connect fields with tables and create list of tables that are updated
*/

bool Query_result_update::prepare(THD *thd, const mem_root_deque<Item *> &,
                                  Query_expression *u) {
  SQL_I_List<Table_ref> update_list;
  DBUG_TRACE;

  unit = u;

  Query_block *const select = unit->first_query_block();
  Table_ref *const leaves = select->leaf_tables;

  const table_map tables_to_update = get_table_map(*fields);

  for (Table_ref *tr = leaves; tr; tr = tr->next_leaf) {
    if (tr->check_option) {
      // Resolving may be needed for subsequent executions
      if (!tr->check_option->fixed &&
          tr->check_option->fix_fields(thd, nullptr))
        return true; /* purecov: inspected */
      /*
        Do not do cross-predicate column substitution in CHECK OPTION. Imagine
        that the view's CHECK OPTION is "a<b" and the query's WHERE is "a=1".
        By view merging, the former is injected into the latter, the total
        WHERE is now "a=1 AND a<b". If the total WHERE is then changed, by
        optimize_cond(), to "a=1 AND 1<b", this also changes the CHECK OPTION
        to "1<b"; and if the query was: "UPDATE v SET a=300 WHERE a=1" then
        "1<b" will pass, wrongly, while "a<b" maybe wouldn't have. The CHECK
        OPTION must remain intact.
        @todo When we can clone expressions, clone the CHECK OPTION, and
        remove this de-optimization.
      */
      tr->check_option->walk(&Item::disable_constant_propagation,
                             enum_walk::POSTFIX, nullptr);
    }
  }

  /*
    Save tables being updated in update_tables
    update_table->shared is position for table
  */

  update_list.clear();
  for (Table_ref *tr = leaves; tr; tr = tr->next_leaf) {
    /* TODO: add support of view of join support */
    if (tables_to_update & tr->map()) {
      auto dup = new (thd->mem_root) Table_ref(*tr);
      if (dup == nullptr) return true;

      update_list.link_in_list(dup, &dup->next_local);
      tr->shared = dup->shared = update_table_count++;

      // Avoid checking for duplicates for MyISAM merge tables
      dup->next_global = nullptr;

      // Set properties for table to be updated
      TABLE *const table = tr->table;
      table->no_keyread = true;
      table->covering_keys.clear_all();
      table->pos_in_table_list = dup;
    }
  }

  update_table_count = update_list.elements;
  update_tables = update_list.first;

  tmp_tables = (TABLE **)thd->mem_calloc(sizeof(TABLE *) * update_table_count);
  if (tmp_tables == nullptr) return true;
  tmp_table_param = new (thd->mem_root) Temp_table_param[update_table_count];
  if (tmp_table_param == nullptr) return true;
  fields_for_table = thd->mem_root->ArrayAlloc<List_item *>(update_table_count);
  if (fields_for_table == nullptr) return true;
  values_for_table = thd->mem_root->ArrayAlloc<List_item *>(update_table_count);
  if (values_for_table == nullptr) return true;

  assert(update_operations == nullptr);
  update_operations =
      (COPY_INFO **)thd->mem_calloc(sizeof(COPY_INFO *) * update_table_count);

  if (update_operations == nullptr) return true;
  for (uint i = 0; i < update_table_count; i++) {
    fields_for_table[i] = new (thd->mem_root) List_item(thd->mem_root);
    values_for_table[i] = new (thd->mem_root) List_item(thd->mem_root);
  }
  if (thd->is_error()) return true;

  /* Split fields into fields_for_table[] and values_by_table[] */

  auto field_it = fields->begin();
  auto value_it = values->begin();
  while (field_it != fields->end()) {
    Item_field *const field = down_cast<Item_field *>(*field_it++);
    Item *const value = *value_it++;
    assert(!field->hidden && !value->hidden);
    uint offset = field->table_ref->shared;
    fields_for_table[offset]->push_back(field);
    values_for_table[offset]->push_back(value);
  }
  if (thd->is_error()) return true;

  /* Allocate copy fields */
  size_t max_fields = 0;
  for (uint i = 0; i < update_table_count; i++)
    max_fields = std::max(max_fields, size_t(fields_for_table[i]->size() +
                                             select->leaf_table_count));
  copy_field = new (thd->mem_root) Copy_field[max_fields];

  for (Table_ref *ref = leaves; ref != nullptr; ref = ref->next_leaf) {
    if (tables_to_update & ref->map()) {
      const uint position = ref->shared;
      mem_root_deque<Item *> *cols = fields_for_table[position];
      mem_root_deque<Item *> *vals = values_for_table[position];
      TABLE *const table = ref->table;

      COPY_INFO *update = new (thd->mem_root)
          COPY_INFO(COPY_INFO::UPDATE_OPERATION, cols, vals);
      if (update == nullptr ||
          update->add_function_default_columns(table, table->write_set))
        return true;

      update_operations[position] = update;

      if ((table->file->ha_table_flags() & HA_PARTIAL_COLUMN_READ) != 0 &&
          update->function_defaults_apply(table)) {
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
        return true;
    }
  }

  assert(!thd->is_error());
  return false;
}

// Collects all columns of the given table that are referenced in join
// conditions. The given table is assumed to be the first (outermost) table in
// the join order. The referenced columns are collected into TABLE::tmp_set.
static void CollectColumnsReferencedInJoinConditions(
    const JOIN *join, const Table_ref *table_ref) {
  TABLE *table = table_ref->table;

  // Verify tmp_set is not in use.
  assert(bitmap_is_clear_all(&table->tmp_set));

  for (uint i = 1; i < join->tables; ++i) {
    const QEP_TAB *const tab = &join->qep_tab[i];
    if (!tab->position()) continue;
    if (tab->condition())
      tab->condition()->walk(&Item::add_field_to_set_processor,
                             enum_walk::SUBQUERY_POSTFIX,
                             pointer_cast<uchar *>(table));
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
      - init ref access (construct_lookup_ref() in RefIterator):
        copy referenced value from main_table into 2nd table's ref buffer
      - look up a first row in 2nd table (RefIterator::Read())
        - if it joins, update row of main_table on the fly
      - look up a second row in 2nd table (again RefIterator::Read()).
      Because construct_lookup_ref() is not called again, the
      before-update value of the row of main_table is still in the 2nd
      table's ref buffer. So the lookup is not influenced by the just-done
      update of main_table.
    */
    if (i > 1) {
      for (uint key_part_idx = 0; key_part_idx < tab->ref().key_parts;
           key_part_idx++) {
        Item *ref_item = tab->ref().items[key_part_idx];
        if ((table_ref->map() & ref_item->used_tables()) != 0)
          ref_item->walk(&Item::add_field_to_set_processor,
                         enum_walk::SUBQUERY_POSTFIX,
                         pointer_cast<uchar *>(table));
      }
    }
  }
}

/**
  Check if table is safe to update on the fly

  @param join_tab   Table (always the first one in the JOIN plan)
  @param table_ref  Table reference for 'join_tab'
  @param all_tables List of tables (updated or not)

  We can update the first table in join on the fly if we know that:
  - a row in this table will never be read twice (that depends, among other
  things, on the access method), and
  - updating this row won't influence the selection of rows in next tables.

  This function gets information about fields to be updated from the
  TABLE::write_set bitmap. And it gets information about which fields influence
  the selection of rows in next tables, from the TABLE::tmp_set bitmap.

  @returns true if it is safe to update on the fly.
*/

static bool safe_update_on_fly(const QEP_TAB *join_tab,
                               const Table_ref *table_ref,
                               Table_ref *all_tables) {
  TABLE *table = join_tab->table();

  // Check that the table is not joined to itself:
  if (unique_table(table_ref, all_tables, false)) return false;
  if (table->part_info &&
      // if there is risk for a row to move in a next partition, in which case
      // it may be read twice:
      table->part_info->num_partitions_used() > 1 &&
      partition_key_modified(table, table->write_set))
    return false;

  // If updating the table influences the selection of rows in next tables:
  if (bitmap_is_overlapping(&table->tmp_set, table->write_set)) return false;

  switch (join_tab->type()) {
    case JT_SYSTEM:
    case JT_CONST:
    case JT_EQ_REF:
      // At most one matching row, with a constant lookup value as this is the
      // first table. So this row won't be seen a second time; the iterator
      // won't even try a second read.
      return true;
    case JT_REF:
    case JT_REF_OR_NULL:
      // If the key is updated, it may change from non-NULL-constant to NULL,
      // so with JT_REF_OR_NULL, the row would be read twice.
      // For JT_REF, let's be defensive as we do not know how the engine behaves
      // if doing an index lookup on a changing index.
      return !is_key_used(table, join_tab->ref().key, table->write_set);
    case JT_RANGE:
    case JT_INDEX_MERGE:
      assert(join_tab->range_scan() != nullptr);
      // When scanning a range, it's possible to read a row twice if it moves
      // within the range:
      if (uses_index_on_fields(join_tab->range_scan(), table->write_set))
        return false;
      // If the index access is using some secondary key(s), and if the table
      // has a clustered primary key, modifying that key might affect the
      // functioning of the the secondary key(s), so fall through to check that.
      [[fallthrough]];
    case JT_ALL:
      assert(join_tab->type() != JT_ALL || join_tab->range_scan() == nullptr);
      // If using the clustered key under the cover:
      if ((table->file->ha_table_flags() & HA_PRIMARY_KEY_IN_READ_INDEX) &&
          table->s->primary_key < MAX_KEY)
        return !is_key_used(table, table->s->primary_key, table->write_set);
      return true;
    case JT_INDEX_SCAN:
      assert(false);  // cannot happen, due to "no_keyread" instruction.
    default:
      break;  // Avoid compiler warning
  }
  return false;
}

/// Adds a field for storing row IDs from "table" to "fields".
static bool AddRowIdAsTempTableField(THD *thd, TABLE *table,
                                     mem_root_deque<Item *> *fields) {
  /*
    Signal each table (including tables referenced by WITH CHECK OPTION
    clause) for which we will store row position in the temporary table
    that we need a position to be read first.
  */
  table->prepare_for_position();

  /*
    A tmp table is moved to InnoDB if it doesn't fit in memory,
    and InnoDB does not support fixed length string fields bigger
    than 1024 bytes, so use a variable length string field.
  */
  Field_varstring *field = new (thd->mem_root) Field_varstring(
      table->file->ref_length, false, table->alias, table->s, &my_charset_bin);
  if (field == nullptr) return true;
  field->init(table);
  Item_field *ifield = new (thd->mem_root) Item_field(field);
  if (ifield == nullptr) return true;
  ifield->set_nullable(false);
  return fields->push_back(ifield);
}

/// Stores the current row ID of "table" in the specified field of "tmp_table".
///
/// @param table The table to get a row ID from.
/// @param tmp_table The temporary table in which to store the row ID.
/// @param field_num The field of tmp_table in which to store the row ID.
/// @param hash_join_tables A map of all tables that are part of a hash join.
static void StoreRowId(TABLE *table, TABLE *tmp_table, int field_num,
                       table_map hash_join_tables) {
  // Hash joins have already copied the row ID from the join buffer into
  // table->file->ref. Nested loop joins have not, so we call position() to get
  // the row ID from the handler.
  if (!Overlaps(hash_join_tables, table->pos_in_table_list->map())) {
    table->file->position(table->record[0]);
  }
  tmp_table->visible_field_ptr()[field_num]->store(
      pointer_cast<const char *>(table->file->ref), table->file->ref_length,
      &my_charset_bin);

  /*
    For outer joins a rowid field may have no NOT_NULL_FLAG,
    so we have to reset NULL bit for this field.
    (set_notnull() resets NULL bit only if available).
  */
  tmp_table->visible_field_ptr()[field_num]->set_notnull();
}

/// Position the scan of "table" using the row ID stored in the specified field
/// of "tmp_table".
///
/// @param updated_table The table that is being updated.
/// @param table The table to position on a given row.
/// @param tmp_table The temporary table that holds the row ID.
/// @param field_num The field of tmp_table that holds the row ID.
/// @return True on error.
static bool PositionScanOnRow(TABLE *updated_table, TABLE *table,
                              TABLE *tmp_table, int field_num) {
  /*
    The row-id is after the "length bytes", and the storage
    engine knows its length. Pass the "data_ptr()" instead of
    the "field_ptr()" to ha_rnd_pos().
  */
  if (const int error = table->file->ha_rnd_pos(
          table->record[0],
          const_cast<uchar *>(
              tmp_table->visible_field_ptr()[field_num]->data_ptr()))) {
    myf error_flags = 0;
    if (updated_table->file->is_fatal_error(error)) {
      error_flags |= ME_FATALERROR;
    }

    updated_table->file->print_error(error, error_flags);
    return true;
  }
  return false;
}

/**
  Set up data structures for multi-table UPDATE

  IMPLEMENTATION
    - Update first table in join on the fly, if possible
    - Create temporary tables to store changed values for all other tables
      that are updated (and main_table if the above doesn't hold).
*/

bool Query_result_update::optimize() {
  DBUG_TRACE;

  Query_block *const select = unit->first_query_block();
  JOIN *const join = select->join;
  THD *thd = join->thd;

  Table_ref *leaves = select->leaf_tables;

  // Ensure table pointers are synced in repeated executions

  Table_ref *update_table = update_tables;
  for (Table_ref *tr = leaves; tr; tr = tr->next_leaf) {
    if (tr->is_updated()) {
      TABLE *const table = tr->table;

      update_table->table = table;
      table->pos_in_table_list = update_table;

      table->covering_keys.clear_all();
      if (table->triggers &&
          table->triggers->has_triggers(TRG_EVENT_UPDATE, TRG_ACTION_AFTER)) {
        /*
          The table has AFTER UPDATE triggers that might access to subject
          table and therefore might need update to be done immediately.
          So we turn-off the batching.
        */
        (void)table->file->ha_extra(HA_EXTRA_UPDATE_CANNOT_BATCH);
      }
      update_table = update_table->next_local;
    }
  }

  // Skip remaining optimization if no plan is generated
  if (select->join->zero_result_cause) return false;

  if (!thd->lex->is_explain() && CheckSqlSafeUpdate(thd, join)) {
    return true;
  }

  main_table = GetOutermostTable(join);
  assert(main_table != nullptr);

  AccessPath *root_path = join->root_access_path();
  assert(root_path->type == AccessPath::UPDATE_ROWS);
  const table_map immediate_update_tables =
      root_path->update_rows().immediate_tables;
  if (immediate_update_tables != 0) {
    assert(immediate_update_tables == main_table->pos_in_table_list->map());
    table_to_update = main_table;
  } else {
    table_to_update = nullptr;
  }

  if (prepare_partial_update(&thd->opt_trace, *fields, *values))
    return true; /* purecov: inspected */

  /* Any update has at least one pair (field, value) */
  assert(!fields->empty());

  // Allocate temporary table structs per execution (they use the mem_root)
  tmp_table_param = new (thd->mem_root) Temp_table_param[update_table_count];
  if (tmp_table_param == nullptr) return true;

  /*
   Only one table may be modified by UPDATE of an updatable view.
   For an updatable view first_table_for_update indicates this
   table.
   For a regular multi-update it refers to some updated table.
  */
  Table_ref *first_table_for_update =
      down_cast<Item_field *>(*VisibleFields(*fields).begin())->table_ref;

  /* Create a temporary table for keys to all tables, except main table */
  for (Table_ref *table_ref = update_tables; table_ref != nullptr;
       table_ref = table_ref->next_local) {
    TABLE *table = table_ref->table;
    uint cnt = table_ref->shared;
    mem_root_deque<Item *> temp_fields(thd->mem_root);
    ORDER *group = nullptr;
    Temp_table_param *tmp_param;

    if (thd->lex->is_ignore()) table->file->ha_extra(HA_EXTRA_IGNORE_DUP_KEY);
    if (table == main_table)  // First table in join
    {
      if (table == table_to_update) {
        assert(bitmap_is_clear_all(&table->tmp_set));
        table->mark_columns_needed_for_update(
            thd, true /*mark_binlog_columns=true*/);
        if (table->setup_partial_update()) return true; /* purecov: inspected */
        continue;
      }
      bitmap_clear_all(&main_table->tmp_set);
    }
    table->mark_columns_needed_for_update(thd,
                                          true /*mark_binlog_columns=true*/);

    if (table != table_to_update &&
        table->has_columns_marked_for_partial_update()) {
      Opt_trace_context *trace = &thd->opt_trace;
      if (trace->is_started()) {
        Opt_trace_object trace_wrapper(trace);
        Opt_trace_object trace_partial_update(trace, "json_partial_update");
        Opt_trace_object trace_rejected(trace, "rejected_table");
        trace_rejected.add_utf8_table(table->pos_in_table_list);
        trace_rejected.add_utf8("cause", "Table cannot be updated on the fly");
      }
    }

    /*
      enable uncacheable flag if we update a view with check option
      and check option has a subselect, otherwise, the check option
      can be evaluated after the subselect was freed as independent
      (See full_local in JOIN::join_free()).
    */
    if (table_ref->check_option && !select->uncacheable) {
      Query_expression *tmp_query_expression;
      Query_block *sl;
      for (tmp_query_expression = select->first_inner_query_expression();
           tmp_query_expression;
           tmp_query_expression =
               tmp_query_expression->next_query_expression()) {
        for (sl = tmp_query_expression->first_query_block(); sl;
             sl = sl->next_query_block()) {
          if (sl->master_query_expression()->item) {
            // Prevent early freeing in JOIN::join_free()
            select->uncacheable |= UNCACHEABLE_CHECKOPTION;
            goto loop_end;
          }
        }
      }
    }
  loop_end:

    if (table_ref->table == first_table_for_update->table &&
        table_ref->check_option) {
      table_map unupdated_tables = table_ref->check_option->used_tables() &
                                   ~first_table_for_update->map();
      for (Table_ref *tbl_ref = leaves; unupdated_tables && tbl_ref;
           tbl_ref = tbl_ref->next_leaf) {
        if (unupdated_tables & tbl_ref->map())
          unupdated_tables &= ~tbl_ref->map();
        else
          continue;
        if (unupdated_check_opt_tables.push_back(tbl_ref->table))
          return true; /* purecov: inspected */
      }
    }

    tmp_param = tmp_table_param + cnt;

    /*
      Create a temporary table to store all fields that are changed for this
      table. The first field in the temporary table is a pointer to the
      original row so that we can find and update it. For the updatable
      VIEW a few following fields are rowids of tables used in the CHECK
      OPTION condition.
    */

    if (AddRowIdAsTempTableField(thd, table, &temp_fields)) return true;
    for (TABLE &tbl : unupdated_check_opt_tables) {
      if (AddRowIdAsTempTableField(thd, &tbl, &temp_fields)) return true;
    }

    temp_fields.insert(temp_fields.end(), fields_for_table[cnt]->begin(),
                       fields_for_table[cnt]->end());

    group = new (thd->mem_root) ORDER;
    /* Make an unique key over the first field to avoid duplicated updates */
    group->direction = ORDER_ASC;
    group->item = &temp_fields.front();

    tmp_param->allow_group_via_temp_table = true;
    tmp_param->func_count = temp_fields.size();
    tmp_param->group_parts = 1;
    tmp_param->group_length = table->file->ref_length;
    tmp_tables[cnt] =
        create_tmp_table(thd, tmp_param, temp_fields, group, false, false,
                         TMP_TABLE_ALL_COLUMNS, HA_POS_ERROR, "");
    if (!tmp_tables[cnt]) return true;

    /*
      Pass a table triggers pointer (Table_trigger_dispatcher *) from
      the original table to the new temporary table. This pointer will be used
      inside the method Query_result_update::send_data() to determine temporary
      nullability flag for the temporary table's fields. It will be done before
      calling fill_record() to assign values to the temporary table's fields.
    */
    tmp_tables[cnt]->triggers = table->triggers;
    tmp_tables[cnt]->file->ha_index_init(0, false /*sorted*/);
  }
  return false;
}

bool Query_result_update::start_execution(THD *thd) {
  thd->check_for_truncated_fields = CHECK_FIELD_WARN;
  thd->num_truncated_fields = 0L;
  return false;
}

void Query_result_update::cleanup() {
  assert(CountHiddenFields(*values) == 0);
  for (Table_ref *tr = update_tables; tr != nullptr; tr = tr->next_local) {
    tr->table = nullptr;
  }
  if (tmp_tables) {
    for (uint cnt = 0; cnt < update_table_count; cnt++) {
      if (tmp_tables[cnt]) {
        /*
          Cleanup can get called without the send_eof() call, close
          the index if open.
        */
        tmp_tables[cnt]->file->ha_index_or_rnd_end();
        close_tmp_table(tmp_tables[cnt]);
        free_tmp_table(tmp_tables[cnt]);
        tmp_tables[cnt] = nullptr;
        tmp_table_param[cnt].cleanup();
      }
    }
  }
  tmp_table_param = nullptr;
  current_thd->check_for_truncated_fields =
      CHECK_FIELD_IGNORE;  // Restore this setting
  main_table = nullptr;
  // Reset state and statistics members:
  unupdated_check_opt_tables.clear();
}

bool Query_result_update::send_data(THD *, const mem_root_deque<Item *> &) {
  DBUG_TRACE;
  assert(false);  // UPDATE does not return any data.
  return false;
}

bool UpdateRowsIterator::DoImmediateUpdatesAndBufferRowIds(
    bool *trans_safe, bool *transactional_tables) {
  if (m_outermost_table->file->was_semi_consistent_read()) {
    // This will let the nested-loop iterator repeat the read of the same row if
    // it still exists.
    return false;
  }

  // For now, don't actually update anything in EXPLAIN ANALYZE. (If we enable
  // it, INSERT and DELETE should also be changed to have side effects when
  // running under EXPLAIN ANALYZE.)
  if (thd()->lex->is_explain_analyze) {
    return false;
  }

  for (Table_ref *cur_table = m_update_tables; cur_table != nullptr;
       cur_table = cur_table->next_local) {
    TABLE *table = cur_table->table;
    uint offset = cur_table->shared;
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
    if (table->has_null_row() || table->has_updated_row()) continue;

    if (table == m_immediate_table) {
      table->clear_partial_update_diffs();
      table->set_updated_row();
      store_record(table, record[1]);
      bool is_row_changed = false;
      if (fill_record_n_invoke_before_triggers(
              thd(), m_update_operations[offset], *m_fields_for_table[offset],
              *m_values_for_table[offset], table, TRG_EVENT_UPDATE, 0, false,
              &is_row_changed))
        return true;

      ++m_found_rows;
      int error = 0;
      if (is_row_changed) {
        if ((error = cur_table->view_check_option(thd())) != VIEW_CHECK_OK) {
          if (error == VIEW_CHECK_SKIP)
            continue;
          else if (error == VIEW_CHECK_ERROR)
            return true;
        }

        /*
          Existing rows in table should normally satisfy CHECK constraints. So
          it should be safe to check constraints only for rows that has really
          changed (i.e. after compare_records()).

          In future, once addition/enabling of CHECK constraints without their
          validation is supported, we might encounter old rows which do not
          satisfy CHECK constraints currently enabled. However, rejecting
          no-op updates to such invalid pre-existing rows won't make them
          valid and is probably going to be confusing for users. So it makes
          sense to stick to current behavior.
        */
        if (invoke_table_check_constraints(thd(), table)) {
          if (thd()->is_error()) return true;
          // continue when IGNORE clause is used.
          continue;
        }

        if (m_updated_rows == 0) {
          /*
            Inform the main table that we are going to update the table even
            while we may be scanning it.  This will flush the read cache
            if it's used.
          */
          m_outermost_table->file->ha_extra(HA_EXTRA_PREPARE_FOR_UPDATE);
        }

        ++m_updated_rows;
        if ((error = table->file->ha_update_row(table->record[1],
                                                table->record[0])) &&
            error != HA_ERR_RECORD_IS_THE_SAME) {
          --m_updated_rows;
          myf error_flags = MYF(0);
          if (table->file->is_fatal_error(error)) error_flags |= ME_FATALERROR;

          table->file->print_error(error, error_flags);

          /* Errors could be downgraded to warning by IGNORE */
          if (thd()->is_error()) return true;
        } else {
          if (error == HA_ERR_RECORD_IS_THE_SAME) {
            error = 0;
            --m_updated_rows;
          }
          /* non-transactional or transactional table got modified   */
          /* either Query_result_update class' flag is raised in its branch */
          if (table->file->has_transactions())
            *transactional_tables = true;
          else {
            *trans_safe = false;
            thd()->get_transaction()->mark_modified_non_trans_table(
                Transaction_ctx::STMT);
          }
        }
      }
      if (!error && table->triggers &&
          table->triggers->process_triggers(thd(), TRG_EVENT_UPDATE,
                                            TRG_ACTION_AFTER, true))
        return true;
    } else {
      int error;
      TABLE *tmp_table = m_tmp_tables[offset];
      /*
       For updatable VIEW store rowid of the updated table and
       rowids of tables used in the CHECK OPTION condition.
      */
      int field_num = 0;
      StoreRowId(table, tmp_table, field_num++, m_hash_join_tables);
      for (TABLE &tbl : m_unupdated_check_opt_tables) {
        StoreRowId(&tbl, tmp_table, field_num++, m_hash_join_tables);
      }

      /*
        If there are triggers in an original table the temporary table based on
        then enable temporary nullability for temporary table's fields.
      */
      if (tmp_table->triggers) {
        for (Field **modified_fields = tmp_table->visible_field_ptr() + 1 +
                                       m_unupdated_check_opt_tables.size();
             *modified_fields; ++modified_fields) {
          (*modified_fields)->set_tmp_nullable();
        }
      }

      /* Store regular updated fields in the row. */
      if (fill_record(thd(), tmp_table,
                      tmp_table->visible_field_ptr() + 1 +
                          m_unupdated_check_opt_tables.size(),
                      *m_values_for_table[offset], nullptr, nullptr, false)) {
        return true;
      }

      // check if a record exists with the same hash value
      if (!check_unique_constraint(tmp_table))
        return false;  // skip adding duplicate record to the temp table

      /* Write row, ignoring duplicated updates to a row */
      error = tmp_table->file->ha_write_row(tmp_table->record[0]);
      if (error != HA_ERR_FOUND_DUPP_KEY && error != HA_ERR_FOUND_DUPP_UNIQUE) {
        if (error && (create_ondisk_from_heap(
                          thd(), tmp_table, error, /*insert_last_record=*/true,
                          /*ignore_last_dup=*/true, /*is_duplicate=*/nullptr) ||
                      tmp_table->file->ha_index_init(0, false /*sorted*/))) {
          return true;  // Not a table_is_full error
        }
        ++m_found_rows;
      }
    }
  }
  return false;
}

bool UpdateRowsIterator::DoDelayedUpdates(bool *trans_safe,
                                          bool *transactional_tables) {
  int local_error = 0;
  ha_rows org_updated;
  TABLE *table, *tmp_table;
  myf error_flags = MYF(0); /**< Flag for fatal errors */

  DBUG_TRACE;

  if (m_found_rows == 0) {
    /*
      If the binary log is on, we still need to check
      if there are transactional tables involved. If
      there are mark the transactional_tables flag correctly.

      This flag determines whether the writes go into the
      transactional or non transactional cache, even if they
      do not change any table, they are still written into
      the binary log when the format is STMT or MIXED.
    */
    if (mysql_bin_log.is_open()) {
      for (Table_ref *cur_table = m_update_tables; cur_table != nullptr;
           cur_table = cur_table->next_local) {
        if (cur_table->table->file->has_transactions()) {
          *transactional_tables = true;
          break;
        }
      }
    }
    return false;
  }

  // All rows which we will now read must be updated and thus locked:
  m_outermost_table->file->try_semi_consistent_read(false);

  // If we're updating based on an outer join, the executor may have left some
  // rows in NULL row state. Reset them before we start looking at rows,
  // so that generated fields don't inadvertedly get NULL inputs.
  for (Table_ref *cur_table = m_update_tables; cur_table != nullptr;
       cur_table = cur_table->next_local) {
    cur_table->table->reset_null_row();
  }

  for (Table_ref *cur_table = m_update_tables; cur_table != nullptr;
       cur_table = cur_table->next_local) {
    uint offset = cur_table->shared;

    table = cur_table->table;

    /*
      Always update the flag if - even if not updating the table,
      when the binary log is ON. This will allow the right binlog
      cache - stmt or trx cache - to be selected when logging
      innefective statementst to the binary log (in STMT or MIXED
      mode logging).
     */
    if (mysql_bin_log.is_open())
      *transactional_tables |= table->file->has_transactions();

    if (table == m_immediate_table) continue;  // Already updated
    org_updated = m_updated_rows;
    tmp_table = m_tmp_tables[offset];
    table->file->ha_index_or_rnd_end();
    if ((local_error = table->file->ha_rnd_init(false))) {
      if (table->file->is_fatal_error(local_error))
        error_flags |= ME_FATALERROR;

      table->file->print_error(local_error, error_flags);
      goto err;
    }

    for (TABLE &tbl : m_unupdated_check_opt_tables) {
      tbl.file->ha_index_or_rnd_end();
      if (tbl.file->ha_rnd_init(true))
        // No known handler error code present, print_error makes no sense
        goto err;
    }

    /*
      Setup copy functions to copy fields from temporary table
    */
    auto field_it = m_fields_for_table[offset]->begin();
    Field **field = tmp_table->visible_field_ptr() + 1 +
                    m_unupdated_check_opt_tables.size();  // Skip row pointers
    Copy_field *copy_field_ptr = m_copy_fields, *copy_field_end;
    for (; *field; field++) {
      Item_field *item = down_cast<Item_field *>(*field_it++);
      (copy_field_ptr++)->set(item->field, *field);
    }
    copy_field_end = copy_field_ptr;

    // Before initializing for random scan, close the index opened for insert.
    tmp_table->file->ha_index_or_rnd_end();
    if ((local_error = tmp_table->file->ha_rnd_init(true))) {
      if (table->file->is_fatal_error(local_error))
        error_flags |= ME_FATALERROR;

      table->file->print_error(local_error, error_flags);
      goto err;
    }

    for (;;) {
      if (thd()->killed && *trans_safe)
        // No known handler error code present, print_error makes no sense
        goto err;
      if ((local_error = tmp_table->file->ha_rnd_next(tmp_table->record[0]))) {
        if (local_error == HA_ERR_END_OF_FILE) break;
        if (local_error == HA_ERR_RECORD_DELETED)
          continue;  // May happen on dup key
        if (table->file->is_fatal_error(local_error))
          error_flags |= ME_FATALERROR;

        table->file->print_error(local_error, error_flags);
        goto err;
      }

      /* call ha_rnd_pos() using rowids from temporary table */
      int field_num = 0;
      if (PositionScanOnRow(table, table, tmp_table, field_num++)) goto err;
      for (TABLE &tbl : m_unupdated_check_opt_tables) {
        if (PositionScanOnRow(table, &tbl, tmp_table, field_num++)) goto err;
      }

      table->set_updated_row();
      store_record(table, record[1]);

      /* Copy data from temporary table to current table */
      for (copy_field_ptr = m_copy_fields; copy_field_ptr != copy_field_end;
           copy_field_ptr++)
        copy_field_ptr->invoke_do_copy();

      if (thd()->is_error()) goto err;

      // The above didn't update generated columns
      if (table->vfield &&
          update_generated_write_fields(table->write_set, table))
        goto err;

      if (table->triggers) {
        bool rc = table->triggers->process_triggers(thd(), TRG_EVENT_UPDATE,
                                                    TRG_ACTION_BEFORE, true);

        // Trigger might have changed dependencies of generated columns
        Trigger_chain *tc =
            table->triggers->get_triggers(TRG_EVENT_UPDATE, TRG_ACTION_BEFORE);
        bool has_trigger_updated_fields =
            (tc && tc->has_updated_trigger_fields(table->write_set));
        if (!rc && table->vfield && has_trigger_updated_fields) {
          // Dont save old value while re-calculating generated fields.
          // Before image will already be saved in the first calculation.
          table->blobs_need_not_keep_old_value();
          if (update_generated_write_fields(table->write_set, table)) goto err;
        }

        table->triggers->disable_fields_temporary_nullability();

        if (rc || check_record(thd(), table->field)) goto err;
      }

      if (!records_are_comparable(table) || compare_records(table)) {
        /*
          This function does not call the fill_record_n_invoke_before_triggers
          which sets function defaults automagically. Hence calling
          set_function_defaults here explicitly to set the function defaults.
          Note that, in fill_record_n_invoke_before_triggers, function defaults
          are set before before-triggers are called; here, it's the opposite
          order.
        */
        if (m_update_operations[offset]->set_function_defaults(table)) {
          goto err;
        }
        /*
          It is safe to not invoke CHECK OPTION for VIEW if records are same.
          In this case the row is coming from the view and thus should satisfy
          the CHECK OPTION.
        */
        int error;
        if ((error = cur_table->view_check_option(thd())) != VIEW_CHECK_OK) {
          if (error == VIEW_CHECK_SKIP)
            continue;
          else if (error == VIEW_CHECK_ERROR)
            // No known handler error code present, print_error makes no sense
            goto err;
        }

        /*
          Existing rows in table should normally satisfy CHECK constraints. So
          it should be safe to check constraints only for rows that has really
          changed (i.e. after compare_records()).

          In future, once addition/enabling of CHECK constraints without their
          validation is supported, we might encounter old rows which do not
          satisfy CHECK constraints currently enabled. However, rejecting no-op
          updates to such invalid pre-existing rows won't make them valid and is
          probably going to be confusing for users. So it makes sense to stick
          to current behavior.
        */
        if (invoke_table_check_constraints(thd(), table)) {
          if (thd()->is_error()) goto err;
          // continue when IGNORE clause is used.
          continue;
        }

        local_error =
            table->file->ha_update_row(table->record[1], table->record[0]);
        if (!local_error)
          ++m_updated_rows;
        else if (local_error == HA_ERR_RECORD_IS_THE_SAME)
          local_error = 0;
        else {
          if (table->file->is_fatal_error(local_error))
            error_flags |= ME_FATALERROR;

          table->file->print_error(local_error, error_flags);
          /* Errors could be downgraded to warning by IGNORE */
          if (thd()->is_error()) goto err;
        }
      }

      if (!local_error && table->triggers &&
          table->triggers->process_triggers(thd(), TRG_EVENT_UPDATE,
                                            TRG_ACTION_AFTER, true))
        goto err;
    }

    if (m_updated_rows != org_updated) {
      if (!table->file->has_transactions()) {
        *trans_safe = false;  // Can't do safe rollback
        thd()->get_transaction()->mark_modified_non_trans_table(
            Transaction_ctx::STMT);
      }
    }
    (void)table->file->ha_rnd_end();
    (void)tmp_table->file->ha_rnd_end();
    for (TABLE &tbl : m_unupdated_check_opt_tables) {
      tbl.file->ha_rnd_end();
    }
  }
  return false;

err:
  if (table->file->inited) (void)table->file->ha_rnd_end();
  if (tmp_table->file->inited) (void)tmp_table->file->ha_rnd_end();
  for (TABLE &tbl : m_unupdated_check_opt_tables) {
    if (tbl.file->inited) (void)tbl.file->ha_rnd_end();
  }

  if (m_updated_rows != org_updated) {
    if (table->file->has_transactions())
      *transactional_tables = true;
    else {
      *trans_safe = false;
      thd()->get_transaction()->mark_modified_non_trans_table(
          Transaction_ctx::STMT);
    }
  }
  return true;
}

bool UpdateRowsIterator::Init() {
  if (m_source->Init()) return true;

  if (m_outermost_table != nullptr &&
      !Overlaps(m_hash_join_tables,
                m_outermost_table->pos_in_table_list->map())) {
    // As it's the first table in the join, and we're doing a nested loop join,
    // the table is the left argument of that nested loop join; thus, we can ask
    // for semi-consistent read.
    m_outermost_table->file->try_semi_consistent_read(true);
  }

  return false;
}

UpdateRowsIterator::~UpdateRowsIterator() {
  if (m_outermost_table != nullptr && m_outermost_table->is_created()) {
    m_outermost_table->file->try_semi_consistent_read(false);
  }
}

int UpdateRowsIterator::Read() {
  bool local_error = false;
  bool trans_safe = true;
  bool transactional_tables = false;

  // First process all the rows returned by the join. Update immediately the
  // tables that allow immediate delete, and buffer row IDs for the rows to
  // delete in the other tables.
  Diagnostics_area *const diagnostics = thd()->get_stmt_da();
  while (!local_error) {
    const int read_error = m_source->Read();
    DBUG_EXECUTE_IF("bug13822652_1", thd()->killed = THD::KILL_QUERY;);
    if (read_error > 0 || thd()->is_error()) {
      local_error = true;
    } else if (read_error < 0) {
      break;  // EOF
    } else if (thd()->killed) {
      thd()->send_kill_message();
      return 1;
    } else {
      local_error =
          DoImmediateUpdatesAndBufferRowIds(&trans_safe, &transactional_tables);
      diagnostics->inc_current_row_for_condition();
    }
  }

  const bool error_before_do_delayed_updates = local_error;

  // Do the delayed updates if no error occurred, or if the statement cannot be
  // rolled back (typically because a non-transactional table has been updated).
  if (!local_error ||
      thd()->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT)) {
    THD_STAGE_INFO(thd(), stage_updating_reference_tables);
    if (DoDelayedUpdates(&trans_safe, &transactional_tables)) {
      local_error = true;
    }
  }

  const THD::killed_state killed_status =
      !local_error ? THD::NOT_KILLED : thd()->killed.load();

  /*
    Write the SQL statement to the binlog if we updated
    rows and we succeeded or if we updated some non
    transactional tables.

    The query has to binlog because there's a modified non-transactional table
    either from the query's list or via a stored routine: bug#13270,23333
  */

  if (!local_error ||
      thd()->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT)) {
    if (mysql_bin_log.is_open()) {
      int errcode = 0;
      if (!local_error)
        thd()->clear_error();
      else
        errcode = query_error_code(thd(), killed_status == THD::NOT_KILLED);
      if (thd()->binlog_query(THD::ROW_QUERY_TYPE, thd()->query().str,
                              thd()->query().length, transactional_tables,
                              false, false, errcode)) {
        local_error = true;  // Rollback update
      }
    }
  }
  assert(
      trans_safe || m_updated_rows == 0 ||
      thd()->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT));

  // Safety: If we haven't got an error before (can happen in DoDelayedUpdates).
  if (local_error && !error_before_do_delayed_updates) {
    my_message(ER_UNKNOWN_ERROR, "An error occurred in multi-table update",
               MYF(0));
  }

  return local_error ? 1 : -1;
}

bool Query_result_update::send_eof(THD *thd) {
  char buff[STRING_BUFFER_USUAL_SIZE];
  const ulonglong id = thd->arg_of_last_insert_id_function
                           ? thd->first_successful_insert_id_in_prev_stmt
                           : 0;
  const UpdateRowsIterator *iterator =
      down_cast<UpdateRowsIterator *>(unit->root_iterator()->real_iterator());
  const ha_rows found_rows = iterator->found_rows();
  const ha_rows updated_rows = iterator->updated_rows();

  snprintf(buff, sizeof(buff), ER_THD(thd, ER_UPDATE_INFO), (long)found_rows,
           (long)updated_rows,
           (long)thd->get_stmt_da()->current_statement_cond_count());
  ::my_ok(thd,
          thd->get_protocol()->has_client_capability(CLIENT_FOUND_ROWS)
              ? found_rows
              : updated_rows,
          id, buff);
  return false;
}

bool Sql_cmd_update::accept(THD *thd, Select_lex_visitor *visitor) {
  Query_block *const select = thd->lex->query_block;
  // Update tables
  if (select->has_tables() &&
      accept_for_join(select->m_current_table_nest, visitor))
    return true;

  // Update list
  auto it_value = update_value_list->begin();
  auto it_column = select->visible_fields().begin();
  while (it_value != update_value_list->end() &&
         it_column != select->visible_fields().end()) {
    Item *value = *it_value++;
    Item *column = *it_column++;
    if (walk_item(column, visitor) || walk_item(value, visitor)) return true;
  }

  // Where clause
  if (select->where_cond() != nullptr &&
      walk_item(select->where_cond(), visitor))
    return true;

  // Order clause
  if (accept_for_order(select->order_list, visitor)) return true;

  // Limit clause
  if (select->has_limit()) {
    if (walk_item(select->select_limit, visitor)) return true;
  }
  return visitor->visit(select);
}

table_map GetImmediateUpdateTable(const JOIN *join, bool single_target) {
  if (join->zero_result_cause != nullptr) {
    // The entire statement was optimized away. Typically because of an
    // impossible WHERE clause.
    return 0;
  }

  // The hypergraph optimizer determines the immediate update tables during
  // planning, not after planning.
  assert(!join->thd->lex->using_hypergraph_optimizer);

  // In some cases, rows may be updated immediately as they are read from the
  // outermost table in the join.
  assert(join->qep_tab != nullptr);
  assert(join->tables > 0);
  const QEP_TAB *const first_table = &join->qep_tab[0];
  const Table_ref *const first_table_ref = first_table->table_ref;

  if (!first_table_ref->is_updated()) {
    return 0;
  }

  // If there are at least two tables to update, t1 and t2, t1 being before t2
  // in the plan, we need to collect all fields of t1 which influence the
  // selection of rows from t2. If those fields are also updated, it will not be
  // possible to update t1 on-the-fly.
  //
  // Fields are collected in table->tmp_set, which is then checked in
  // safe_update_on_fly().
  if (!single_target) {
    CollectColumnsReferencedInJoinConditions(join, first_table_ref);
  }

  const bool can_update_on_the_fly =
      safe_update_on_fly(first_table, first_table_ref, join->tables_list);

  // Restore the original state of table->tmp_set.
  bitmap_clear_all(&first_table_ref->table->tmp_set);

  return can_update_on_the_fly ? first_table_ref->map() : 0;
}

bool FinalizeOptimizationForUpdate(JOIN *join) {
  return down_cast<Query_result_update *>(join->query_block->query_result())
      ->optimize();
}

UpdateRowsIterator::UpdateRowsIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> source,
    TABLE *outermost_table, TABLE *immediate_table, Table_ref *update_tables,
    TABLE **tmp_tables, Copy_field *copy_fields,
    List<TABLE> unupdated_check_opt_tables, COPY_INFO **update_operations,
    mem_root_deque<Item *> **fields_for_table,
    mem_root_deque<Item *> **values_for_table,
    table_map tables_with_rowid_in_buffer)
    : RowIterator(thd),
      m_source(std::move(source)),
      m_outermost_table(outermost_table),
      m_immediate_table(immediate_table),
      m_update_tables(update_tables),
      m_tmp_tables(tmp_tables),
      m_copy_fields(copy_fields),
      m_unupdated_check_opt_tables(unupdated_check_opt_tables),
      m_update_operations(update_operations),
      m_fields_for_table(fields_for_table),
      m_values_for_table(values_for_table),
      m_hash_join_tables(tables_with_rowid_in_buffer) {}

unique_ptr_destroy_only<RowIterator> CreateUpdateRowsIterator(
    THD *thd, MEM_ROOT *mem_root, JOIN *join,
    unique_ptr_destroy_only<RowIterator> source) {
  return down_cast<Query_result_update *>(join->query_block->query_result())
      ->create_iterator(thd, mem_root, std::move(source));
}

unique_ptr_destroy_only<RowIterator> Query_result_update::create_iterator(
    THD *thd, MEM_ROOT *mem_root, unique_ptr_destroy_only<RowIterator> source) {
  return NewIterator<UpdateRowsIterator>(
      thd, mem_root, std::move(source), main_table, table_to_update,
      update_tables, tmp_tables, copy_field, unupdated_check_opt_tables,
      update_operations, fields_for_table, values_for_table,
      // The old optimizer does not use hash join in UPDATE statements.
      thd->lex->using_hypergraph_optimizer
          ? GetHashJoinTables(unit->root_access_path())
          : 0);
}
