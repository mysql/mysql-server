<<<<<<< HEAD
/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.
=======
/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

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

// Handle DELETE queries (both single- and multi-table).

#include "sql/sql_delete.h"

#include <assert.h>
#include <limits.h>
#include <sys/types.h>
#include <atomic>
#include <memory>
#include <utility>

#include "lex_string.h"
#include "mem_root_deque.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "scope_guard.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // check_table_access
#include "sql/binlog.h"            // mysql_bin_log
#include "sql/debug_sync.h"        // DEBUG_SYNC
#include "sql/filesort.h"          // Filesort
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/iterators/delete_rows_iterator.h"
#include "sql/iterators/row_iterator.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/walk_access_paths.h"
#include "sql/key_spec.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"       // stage_...
#include "sql/opt_explain.h"  // Modification_plan
#include "sql/opt_explain_format.h"
#include "sql/opt_trace.h"  // Opt_trace_object
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/query_result.h"
#include "sql/range_optimizer/partition_pruning.h"
#include "sql/range_optimizer/path_helpers.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/sql_base.h"  // update_non_unique_table_error
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_optimizer.h"  // optimize_cond, substitute_gc
#include "sql/sql_resolver.h"   // setup_order
#include "sql/sql_select.h"
#include "sql/sql_update.h"  // switch_to_multi_table_if_subqueries
#include "sql/sql_view.h"    // check_key_in_view
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/table_trigger_dispatcher.h"  // Table_trigger_dispatcher
#include "sql/thd_raii.h"
#include "sql/transaction_info.h"
#include "sql/trigger_def.h"
#include "sql/uniques.h"  // Unique

class COND_EQUAL;
class Item_exists_subselect;
class Opt_trace_context;
class Select_lex_visitor;

namespace {

class Query_result_delete final : public Query_result_interceptor {
 public:
  bool need_explain_interceptor() const override { return true; }
  bool send_data(THD *, const mem_root_deque<Item *> &) override {
    assert(false);  // DELETE does not return any data.
    return false;
  }
  bool send_eof(THD *thd) override {
    my_ok(thd, thd->get_row_count_func());
    return false;
  }
};

bool DeleteCurrentRowAndProcessTriggers(THD *thd, TABLE *table,
                                        bool invoke_before_triggers,
                                        bool invoke_after_triggers,
                                        ha_rows *deleted_rows) {
  if (invoke_before_triggers) {
    if (table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                          TRG_ACTION_BEFORE,
                                          /*old_row_is_record1=*/false)) {
      return true;
    }
  }

  if (const int delete_error = table->file->ha_delete_row(table->record[0]);
      delete_error != 0) {
    myf error_flags = MYF(0);
    if (table->file->is_fatal_error(delete_error)) {
      error_flags |= ME_FATALERROR;
    }
    table->file->print_error(delete_error, error_flags);

    // The IGNORE option may have downgraded the error from ha_delete_row
    // to a warning, so we need to check the error flag in the THD.
    return thd->is_error();
  }

  ++*deleted_rows;

  if (invoke_after_triggers) {
    if (table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                          TRG_ACTION_AFTER,
                                          /*old_row_is_record1=*/false)) {
      return true;
    }
  }

  return false;
}

}  // namespace

bool Sql_cmd_delete::precheck(THD *thd) {
  DBUG_TRACE;

  Table_ref *tables = lex->query_tables;

  if (!multitable) {
    if (check_one_table_access(thd, DELETE_ACL, tables)) return true;
  } else {
    Table_ref *aux_tables = delete_tables->first;
    Table_ref **save_query_tables_own_last = lex->query_tables_own_last;

    if (check_table_access(thd, SELECT_ACL, tables, false, UINT_MAX, false))
      return true;

    /*
      Since aux_tables list is not part of LEX::query_tables list we
      have to juggle with LEX::query_tables_own_last value to be able
      call check_table_access() safely.
    */
    lex->query_tables_own_last = nullptr;
    if (check_table_access(thd, DELETE_ACL, aux_tables, false, UINT_MAX,
                           false)) {
      lex->query_tables_own_last = save_query_tables_own_last;
      return true;
    }
    lex->query_tables_own_last = save_query_tables_own_last;
  }
  return false;
}

bool Sql_cmd_delete::check_privileges(THD *thd) {
  DBUG_TRACE;

  if (check_all_table_privileges(thd)) return true;

  if (lex->query_block->check_column_privileges(thd)) return true;

  return false;
}

/**
  Delete a set of rows from a single table.

  @param thd    Thread handler

  @returns false on success, true on error

  @note Like implementations of other DDL/DML in MySQL, this function
  relies on the caller to close the thread tables. This is done in the
  end of dispatch_command().
*/

bool Sql_cmd_delete::delete_from_single_table(THD *thd) {
  DBUG_TRACE;

  myf error_flags = MYF(0); /**< Flag for fatal errors */
  bool will_batch;
  /*
    Most recent handler error
    =  1: Some non-handler error
    =  0: Success
    = -1: No more rows to process, or reached limit
  */
  int error = 0;
  ha_rows deleted_rows = 0;
  bool reverse = false;
  /// read_removal is only used by NDB storage engine
  bool read_removal = false;
  bool need_sort = false;

  uint usable_index = MAX_KEY;
  Query_block *const query_block = lex->query_block;
  Query_expression *const unit = query_block->master_query_expression();
  ORDER *order = query_block->order_list.first;
  Table_ref *const table_list = query_block->get_table_list();
  THD::killed_state killed_status = THD::NOT_KILLED;
  THD::enum_binlog_query_type query_type = THD::ROW_QUERY_TYPE;

  const bool safe_update = thd->variables.option_bits & OPTION_SAFE_UPDATES;

  Table_ref *const delete_table_ref = table_list->updatable_base_table();
  TABLE *const table = delete_table_ref->table;

  const bool transactional_table = table->file->has_transactions();

  const bool has_delete_triggers =
      table->triggers && table->triggers->has_delete_triggers();

  const bool has_before_triggers =
      has_delete_triggers &&
      table->triggers->has_triggers(TRG_EVENT_DELETE, TRG_ACTION_BEFORE);
  const bool has_after_triggers =
      has_delete_triggers &&
      table->triggers->has_triggers(TRG_EVENT_DELETE, TRG_ACTION_AFTER);
  unit->set_limit(thd, query_block);

  AccessPath *range_scan = nullptr;
  join_type type = JT_UNKNOWN;

  auto cleanup = create_scope_guard([&range_scan, table] {
    destroy(range_scan);
    table->set_keyread(false);
    table->file->ha_index_or_rnd_end();
    free_io_cache(table);
    filesort_free_buffers(table, true);
  });

  ha_rows limit = unit->select_limit_cnt;
  const bool using_limit = limit != HA_POS_ERROR;

  if (limit == 0 && thd->lex->is_explain()) {
    Modification_plan plan(thd, MT_DELETE, table, "LIMIT is zero", true, 0);
    bool err = explain_single_table_modification(thd, thd, &plan, query_block);
    return err;
  }

  assert(!(table->all_partitions_pruned_away || m_empty_query));

  // Used to track whether there are no rows that need to be read
  bool no_rows =
      limit == 0 || is_empty_query() || table->all_partitions_pruned_away;

  Item *conds = nullptr;
  if (!no_rows && query_block->get_optimizable_conditions(thd, &conds, nullptr))
    return true; /* purecov: inspected */

  /*
    See if we can substitute expressions with equivalent generated
    columns in the WHERE and ORDER BY clauses of the DELETE statement.
    It is unclear if this is best to do before or after the other
    substitutions performed by substitute_for_best_equal_field(). Do
    it here for now, to keep it consistent with how multi-table
    deletes are optimized in JOIN::optimize().
  */
  if (conds || order)
<<<<<<< HEAD
    static_cast<void>(substitute_gc(thd, query_block, conds, nullptr, order));
=======
    static_cast<void>(substitute_gc(thd, select_lex, conds, NULL, order));

  QEP_TAB_standalone qep_tab_st;
  QEP_TAB &qep_tab = qep_tab_st.as_QEP_TAB();

  if (table->all_partitions_pruned_away) {
    /*
      All partitions were pruned away during preparation. Shortcut further
      processing by "no rows". If explaining, report the plan and bail out.
    */
    no_rows = true;

    if (lex->is_explain()) {
      Modification_plan plan(thd, MT_DELETE, table,
                             "No matching rows after partition pruning", true,
                             0);
      bool err = explain_single_table_modification(thd, &plan, select_lex);
      DBUG_RETURN(err);
    }
  }

  const bool const_cond = (!conds || conds->const_item());
  if (safe_update && const_cond) {
    // Safe mode is a runtime check, so apply it in execution and not prepare
    my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE, MYF(0));
    DBUG_RETURN(true);
<<<<<<< HEAD
  }
>>>>>>> pr/231

  const bool const_cond = conds == nullptr || conds->const_item();
  const bool const_cond_result = const_cond && (!conds || conds->val_int());
  if (thd->is_error())  // Error during val_int()
<<<<<<< HEAD
    return true;        /* purecov: inspected */
=======
    DBUG_RETURN(true);  /* purecov: inspected */
=======

  const_cond= (!conds || conds->const_item());
  const_cond_result= const_cond && (!conds || conds->val_int());
  if (thd->is_error())
  {
    /* Error evaluating val_int(). */
    DBUG_RETURN(TRUE);
  }
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  /*
    We are passing HA_EXTRA_IGNORE_DUP_KEY flag here to recreate query with
    IGNORE keyword within federated storage engine. If federated engine is
    removed in the future, use of HA_EXTRA_IGNORE_DUP_KEY and
    HA_EXTRA_NO_IGNORE_DUP_KEY flag should be removed from
    delete_from_single_table(), DeleteRowsIterator::Init() and
    handler::ha_reset().
  */
  if (lex->is_ignore()) table->file->ha_extra(HA_EXTRA_IGNORE_DUP_KEY);

  /*
    Test if the user wants to delete all rows and deletion doesn't have
    any side-effects (because of triggers), so we can use optimized
    handler::delete_all_rows() method.

    We can use delete_all_rows() if and only if:
    - There is no limit clause
    - The condition is constant
    - The row set is not empty
    - We allow new functions (not using option --skip-new)
    - If there is a condition, then it it produces a non-zero value
    - If the current command is DELETE FROM with no where clause, then:
      - We will not be binlogging this statement in row-based, and
      - there should be no delete triggers associated with the table.
  */
  if (!using_limit && const_cond_result && !no_rows &&
      !(specialflag & SPECIAL_NO_NEW_FUNC) &&
      ((!thd->is_current_stmt_binlog_format_row() ||  // not ROW binlog-format
        thd->is_current_stmt_binlog_disabled()) &&    // no binlog for this
                                                      // command
       !has_delete_triggers)) {
    /* Update the table->file->stats.records number */
    table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
    ha_rows const maybe_deleted = table->file->stats.records;

    Modification_plan plan(thd, MT_DELETE, table, "Deleting all rows", false,
                           maybe_deleted);
    if (lex->is_explain()) {
      bool err =
          explain_single_table_modification(thd, thd, &plan, query_block);
      return err;
    }

    /* Do not allow deletion of all records if safe_update is set. */
    if (safe_update) {
      my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE, MYF(0),
               thd->get_stmt_da()->get_first_condition_message());
      return true;
    }

    /* Do not allow deletion of all records if safe_update is set. */
    if (safe_update)
    {
      my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE, MYF(0),
               thd->get_stmt_da()->get_first_condition_message());
      DBUG_RETURN(true);
    }

    DBUG_PRINT("debug", ("Trying to use delete_all_rows()"));
    if (!(error = table->file->ha_delete_all_rows())) {
      /*
        As delete_all_rows() was used, we have to log it in statement format.
      */
      query_type = THD::STMT_QUERY_TYPE;
      error = -1;
      deleted_rows = maybe_deleted;
      goto cleanup;
    }
    if (error != HA_ERR_WRONG_COMMAND) {
      if (table->file->is_fatal_error(error)) error_flags |= ME_FATALERROR;

      table->file->print_error(error, error_flags);
      goto cleanup;
    }
    /* Handler didn't support fast delete; Delete rows one by one */
  }

  if (conds != nullptr) {
    COND_EQUAL *cond_equal = nullptr;
    Item::cond_result result;

    if (optimize_cond(thd, &conds, &cond_equal,
                      query_block->m_current_table_nest, &result))
      return true;
    if (result == Item::COND_FALSE)  // Impossible where
    {
      no_rows = true;

      if (lex->is_explain()) {
        Modification_plan plan(thd, MT_DELETE, table, "Impossible WHERE", true,
                               0);
        bool err =
            explain_single_table_modification(thd, thd, &plan, query_block);
        return err;
      }
    }
    if (conds) {
      conds = substitute_for_best_equal_field(thd, conds, cond_equal, nullptr);
      if (conds == nullptr) return true;

      conds->update_used_tables();
    }
  }

  /* Prune a second time to be able to prune on subqueries in WHERE clause. */
  if (table->part_info && !no_rows) {
    if (prune_partitions(thd, table, query_block, conds)) return true;
    if (table->all_partitions_pruned_away) {
      no_rows = true;
      if (lex->is_explain()) {
        Modification_plan plan(thd, MT_DELETE, table,
                               "No matching rows after partition pruning", true,
                               0);
        bool err =
            explain_single_table_modification(thd, thd, &plan, query_block);
        return err;
      }
      my_ok(thd, 0);
      return false;
    }
  }

  // Initialize the cost model that will be used for this table
  table->init_cost_model(thd->cost_model());

  /* Update the table->file->stats.records number */
  table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);

  table->covering_keys.clear_all();

  if (conds &&
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_ENGINE_CONDITION_PUSHDOWN)) {
    table->file->cond_push(conds);
  }

  {  // Enter scope for optimizer trace wrapper
    Opt_trace_object wrapper(&thd->opt_trace);
    wrapper.add_utf8_table(delete_table_ref);

    if (!no_rows && conds != nullptr) {
      Key_map keys_to_use(Key_map::ALL_BITS), needed_reg_dummy;
<<<<<<< HEAD
      MEM_ROOT temp_mem_root(key_memory_test_quick_select_exec,
                             thd->variables.range_alloc_block_size);
      no_rows = test_quick_select(
                    thd, thd->mem_root, &temp_mem_root, keys_to_use, 0, 0,
                    limit, safe_update, ORDER_NOT_RELEVANT, table,
                    /*skip_records_in_range=*/false, conds, &needed_reg_dummy,
                    table->force_index, query_block, &range_scan) < 0;
=======
      QUICK_SELECT_I *qck;
<<<<<<< HEAD
      no_rows = test_quick_select(thd, keys_to_use, 0, limit, safe_update,
                                  ORDER_NOT_RELEVANT, &qep_tab, conds,
                                  &needed_reg_dummy, &qck) < 0;
=======
      zero_rows= test_quick_select(thd, keys_to_use, 0, limit, safe_update,
                                   ORDER::ORDER_NOT_RELEVANT, &qep_tab,
                                   conds, &needed_reg_dummy, &qck,
                                   qep_tab.table()->force_index) < 0;
>>>>>>> upstream/cluster-7.6
      qep_tab.set_quick(qck);
>>>>>>> pr/231
    }
    if (thd->is_error())  // test_quick_select() has improper error propagation
      return true;

    if (no_rows) {
      if (lex->is_explain()) {
        Modification_plan plan(thd, MT_DELETE, table, "Impossible WHERE", true,
                               0);
        bool err =
            explain_single_table_modification(thd, thd, &plan, query_block);
        return err;
      }

      my_ok(thd, 0);
      return false;  // Nothing to delete
    }
  }  // Ends scope for optimizer trace wrapper

  /* If running in safe sql mode, don't allow updates without keys */
<<<<<<< HEAD
  if (table->quick_keys.is_clear_all()) {
    thd->server_status |= SERVER_QUERY_NO_INDEX_USED;
<<<<<<< HEAD
=======
    if (safe_update && !using_limit) {
      my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE, MYF(0));
=======
  if (table->quick_keys.is_clear_all())
  {
    thd->server_status|= SERVER_QUERY_NO_INDEX_USED;
>>>>>>> pr/231

    /*
      Safe update error isn't returned if:
      1) It is  an EXPLAIN statement OR
      2) LIMIT is present.

      Append the first warning (if any) to the error message. This allows the
      user to understand why index access couldn't be chosen.
    */
<<<<<<< HEAD
    if (!thd->lex->is_explain() && safe_update && !using_limit) {
      my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE, MYF(0),
               thd->get_stmt_da()->get_first_condition_message());
      return true;
=======
    if (!thd->lex->is_explain() && safe_update &&  !using_limit)
    {
      free_underlaid_joins(thd, select_lex);
      my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE, MYF(0),
               thd->get_stmt_da()->get_first_condition_message());
>>>>>>> upstream/cluster-7.6
      DBUG_RETURN(true);
>>>>>>> pr/231
    }
  }

  if (order) {
    if (conds != nullptr) table->update_const_key_parts(conds);
    order = simple_remove_const(order, conds);
    ORDER_with_src order_src(order, ESC_ORDER_BY);
    usable_index = get_index_for_order(&order_src, table, limit, range_scan,
                                       &need_sort, &reverse);
    if (range_scan != nullptr) {
      // May have been changed by get_index_for_order().
      type = calc_join_type(range_scan);
    }
  }

  // Reaching here only when table must be accessed
  assert(!no_rows);

  {
    ha_rows rows;
    if (range_scan)
      rows = range_scan->num_output_rows();
    else if (!conds && !need_sort && limit != HA_POS_ERROR)
      rows = limit;
    else {
      delete_table_ref->fetch_number_of_rows();
      rows = table->file->stats.records;
    }
    Modification_plan plan(thd, MT_DELETE, table, type, range_scan, conds,
                           usable_index, limit, false, need_sort, false, rows);
    DEBUG_SYNC(thd, "planned_single_delete");

    if (lex->is_explain()) {
      bool err =
          explain_single_table_modification(thd, thd, &plan, query_block);
      return err;
    }

    if (query_block->active_options() & OPTION_QUICK)
      (void)table->file->ha_extra(HA_EXTRA_QUICK);

    unique_ptr_destroy_only<Filesort> fsort;
    JOIN join(thd, query_block);  // Only for holding examined_rows.
    AccessPath *path;
    if (usable_index == MAX_KEY || range_scan) {
      path =
          create_table_access_path(thd, table, range_scan,
                                   /*table_ref=*/nullptr, /*position=*/nullptr,
                                   /*count_examined_rows=*/true);
    } else {
      empty_record(table);
      path = NewIndexScanAccessPath(thd, table, usable_index,
                                    /*use_order=*/true, reverse,
                                    /*count_examined_rows=*/false);
    }

    unique_ptr_destroy_only<RowIterator> iterator;
    if (need_sort) {
      assert(usable_index == MAX_KEY);

<<<<<<< HEAD
      if (conds != nullptr) {
        path = NewFilterAccessPath(thd, path, conds);
      }
=======
<<<<<<< HEAD
      Filesort fsort(&qep_tab, order, HA_POS_ERROR);
      DBUG_ASSERT(usable_index == MAX_KEY);
      table->sort_result.io_cache =
          (IO_CACHE *)my_malloc(key_memory_TABLE_sort_io_cache,
                                sizeof(IO_CACHE), MYF(MY_FAE | MY_ZEROFILL));
=======
      {
        Filesort fsort(&qep_tab, order, HA_POS_ERROR);
        assert(usable_index == MAX_KEY);
        table->sort.io_cache= (IO_CACHE *) my_malloc(key_memory_TABLE_sort_io_cache,
                                                     sizeof(IO_CACHE),
                                                     MYF(MY_FAE | MY_ZEROFILL));
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

      fsort.reset(new (thd->mem_root) Filesort(
          thd, {table}, /*keep_buffers=*/false, order, HA_POS_ERROR,
          /*remove_duplicates=*/false,
          /*force_sort_rowids=*/true, /*unwrap_rollup=*/false));
      path = NewSortAccessPath(thd, path, fsort.get(), order,
                               /*count_examined_rows=*/false);
      iterator = CreateIteratorFromAccessPath(thd, path, &join,
                                              /*eligible_for_batch_mode=*/true);
      // Prevent cleanup in JOIN::destroy() and in the cleanup condition guard,
      // to avoid double-destroy of the SortingIterator.
      table->sorting_iterator = nullptr;
      if (iterator == nullptr || iterator->Init()) return true;
      thd->inc_examined_row_count(join.examined_rows);

      /*
        Filesort has already found and selected the rows we want to delete,
        so we don't need the where clause
      */
      conds = nullptr;
    } else {
      iterator = CreateIteratorFromAccessPath(thd, path, &join,
                                              /*eligible_for_batch_mode=*/true);
      // Prevent cleanup in JOIN::destroy() and in the cleanup condition guard,
      // to avoid double-destroy of the SortingIterator.
      table->sorting_iterator = nullptr;
      if (iterator->Init()) return true;
    }

    if (query_block->has_ft_funcs() && init_ftfuncs(thd, query_block))
      return true; /* purecov: inspected */

    THD_STAGE_INFO(thd, stage_updating);

    if (has_after_triggers) {
      /*
        The table has AFTER DELETE triggers that might access to subject table
        and therefore might need delete to be done immediately. So we turn-off
        the batching.
      */
      (void)table->file->ha_extra(HA_EXTRA_DELETE_CANNOT_BATCH);
      will_batch = false;
    } else {
      // No after delete triggers, attempt to start bulk delete
      will_batch = !table->file->start_bulk_delete();
    }
    table->mark_columns_needed_for_delete(thd);
    if (thd->is_error()) return true;

    if ((table->file->ha_table_flags() & HA_READ_BEFORE_WRITE_REMOVAL) &&
        !using_limit && !has_delete_triggers && range_scan &&
        used_index(range_scan) != MAX_KEY)
      read_removal = table->check_read_removal(used_index(range_scan));

    assert(limit > 0);

    // The loop that reads rows and delete those that qualify

    while (!(error = iterator->Read()) && !thd->killed) {
      assert(!thd->is_error());
      thd->inc_examined_row_count(1);

      if (conds != nullptr) {
        const bool skip_record = conds->val_int() == 0;
        if (thd->is_error()) {
          error = 1;
          break;
        }
        if (skip_record) {
          // Row failed condition check, release lock
          table->file->unlock_row();
          continue;
        }
      }

      assert(!thd->is_error());

      if (DeleteCurrentRowAndProcessTriggers(thd, table, has_before_triggers,
                                             has_after_triggers,
                                             &deleted_rows)) {
        error = 1;
        break;
      }

      if (!--limit && using_limit) {
        error = -1;
        break;
      }
    }

    killed_status = thd->killed;
    if (killed_status != THD::NOT_KILLED || thd->is_error())
      error = 1;  // Aborted
    int loc_error;
    if (will_batch && (loc_error = table->file->end_bulk_delete())) {
      /* purecov: begin inspected */
      if (error != 1) {
        if (table->file->is_fatal_error(loc_error))
          error_flags |= ME_FATALERROR;

        table->file->print_error(loc_error, error_flags);
      }
      error = 1;
      /* purecov: end */
    }
    if (read_removal) {
      /* Only handler knows how many records were really written */
      deleted_rows = table->file->end_read_removal();
    }
    if (query_block->active_options() & OPTION_QUICK)
      (void)table->file->ha_extra(HA_EXTRA_NORMAL);
  }  // End of scope for Modification_plan

cleanup:
<<<<<<< HEAD
  assert(!lex->is_explain());
=======
<<<<<<< HEAD
  DBUG_ASSERT(!lex->is_explain());
=======
  assert(!thd->lex->describe);
  /*
    Invalidate the table in the query cache if something changed. This must
    be before binlog writing and ha_autocommit_...
  */
  if (deleted)
    query_cache.invalidate_single(thd, delete_table_ref, true);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

  if (!transactional_table && deleted_rows > 0)
    thd->get_transaction()->mark_modified_non_trans_table(
        Transaction_ctx::STMT);

  /* See similar binlogging code in sql_update.cc, for comments */
  if ((error < 0) ||
      thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT)) {
    if (mysql_bin_log.is_open()) {
      int errcode = 0;
      if (error < 0)
        thd->clear_error();
      else
        errcode = query_error_code(thd, killed_status == THD::NOT_KILLED);

      /*
        [binlog]: As we don't allow the use of 'handler:delete_all_rows()' when
        binlog_format == ROW, if 'handler::delete_all_rows()' was called
        we replicate statement-based; otherwise, 'ha_delete_row()' was used to
        delete specific rows which we might log row-based.
      */
      int log_result =
          thd->binlog_query(query_type, thd->query().str, thd->query().length,
                            transactional_table, false, false, errcode);

      if (log_result) {
        error = 1;
      }
    }
  }
<<<<<<< HEAD
  assert(transactional_table || deleted_rows == 0 ||
         thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT));
=======
<<<<<<< HEAD
  DBUG_ASSERT(
      transactional_table || deleted_rows == 0 ||
      thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT));
>>>>>>> pr/231
  if (error < 0) {
    my_ok(thd, deleted_rows);
    DBUG_PRINT("info", ("%ld records deleted", (long)deleted_rows));
  }
<<<<<<< HEAD
  return error > 0;
=======
  DBUG_RETURN(error > 0);
=======
  assert(transactional_table ||
         !deleted ||
         thd->get_transaction()->cannot_safely_rollback(
                                                        Transaction_ctx::STMT));
  free_underlaid_joins(thd, select_lex);
  if (error < 0)
  {
    my_ok(thd, deleted);
    DBUG_PRINT("info",("%ld records deleted",(long) deleted));
  }
  DBUG_RETURN(thd->is_error() || thd->killed);

exit_without_my_ok:
  free_underlaid_joins(thd, select_lex);
  table->set_keyread(false);
  DBUG_RETURN((err || thd->is_error() || thd->killed) ? 1 : 0);
}


/**
  Prepare items in DELETE statement

  @param thd        - thread handler

  @return false if success, true if error
*/

bool Sql_cmd_delete::mysql_prepare_delete(THD *thd)
{
  DBUG_ENTER("mysql_prepare_delete");

  List<Item> all_fields;
  SELECT_LEX *const select= thd->lex->select_lex;
  TABLE_LIST *const table_list= select->get_table_list();

  if (select->setup_tables(thd, table_list, false))
    DBUG_RETURN(true);            /* purecov: inspected */

  if (table_list->is_view() && select->resolve_derived(thd, false))
    DBUG_RETURN(true);            /* purecov: inspected */

  if (!table_list->is_updatable())
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "DELETE");
    DBUG_RETURN(true);
  }

  if (table_list->is_multiple_tables())
  {
    my_error(ER_VIEW_DELETE_MERGE_VIEW, MYF(0),
             table_list->view_db.str, table_list->view_name.str);
    DBUG_RETURN(TRUE);
  }

  TABLE_LIST *const delete_table_ref= table_list->updatable_base_table();

  thd->lex->allow_sum_func= 0;
  if (table_list->is_view() &&
      select->check_view_privileges(thd, DELETE_ACL, SELECT_ACL))
    DBUG_RETURN(true);

  ulong want_privilege_saved= thd->want_privilege;
  thd->want_privilege= SELECT_ACL;
  enum enum_mark_columns mark_used_columns_saved= thd->mark_used_columns;
  thd->mark_used_columns= MARK_COLUMNS_READ;

  if (select->setup_conds(thd))
    DBUG_RETURN(true);

  // check ORDER BY even if it can be ignored
  if (select->order_list.first)
  {
    TABLE_LIST   tables;
    List<Item>   fields;
    List<Item>   all_fields;

    tables.table = table_list->table;
    tables.alias = table_list->alias;

    assert(!select->group_list.elements);
    if (select->setup_ref_array(thd))
      DBUG_RETURN(true);                     /* purecov: inspected */
    if (setup_order(thd, select->ref_pointer_array, &tables,
                    fields, all_fields, select->order_list.first))
      DBUG_RETURN(true);
  }

  thd->want_privilege= want_privilege_saved;
  thd->mark_used_columns= mark_used_columns_saved;

  if (setup_ftfuncs(select))
    DBUG_RETURN(true);                       /* purecov: inspected */

  // check_key_in_view() may send an SQL note, but we only want it once.
  if (select->first_execution &&
      check_key_in_view(thd, table_list, delete_table_ref))
  {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "DELETE");
    DBUG_RETURN(true);
  }

  TABLE_LIST *const duplicate= unique_table(thd, delete_table_ref,
                                            table_list->next_global, false);
  if (duplicate)
  {
    update_non_unique_table_error(table_list, "DELETE", duplicate);
    DBUG_RETURN(true);
  }

  if (select->inner_refs_list.elements && select->fix_inner_refs(thd))
    DBUG_RETURN(true);                       /* purecov: inspected */

  if (select->apply_local_transforms(thd, false))
    DBUG_RETURN(true);

  DBUG_RETURN(false);
}


/***************************************************************************
  Delete multiple tables from join 
***************************************************************************/

#define MEM_STRIP_BUF_SIZE current_thd->variables.sortbuff_size

extern "C" int refpos_order_cmp(const void* arg, const void *a,const void *b)
{
  handler *file= (handler*)arg;
  return file->cmp_ref((const uchar*)a, (const uchar*)b);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
}

/**
  Prepare a DELETE statement
*/

bool Sql_cmd_delete::prepare_inner(THD *thd) {
  DBUG_TRACE;

  Query_block *const select = lex->query_block;
  Table_ref *const table_list = select->get_table_list();

  bool apply_semijoin;

  Mem_root_array<Item_exists_subselect *> sj_candidates_local(thd->mem_root);

  Opt_trace_context *const trace = &thd->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_prepare(trace, "delete_preparation");
  trace_prepare.add_select_number(select->select_number);
  Opt_trace_array trace_steps(trace, "steps");

  apply_semijoin = multitable;

  if (select->setup_tables(thd, table_list, false))
    return true; /* purecov: inspected */

  ulong want_privilege_saved = thd->want_privilege;
  thd->want_privilege = SELECT_ACL;
  enum enum_mark_columns mark_used_columns_saved = thd->mark_used_columns;
  thd->mark_used_columns = MARK_COLUMNS_READ;

  if (select->derived_table_count || select->table_func_count) {
    if (select->resolve_placeholder_tables(thd, apply_semijoin)) return true;

    if (select->check_view_privileges(thd, DELETE_ACL, SELECT_ACL)) return true;
  }

  /*
    Deletability test is spread across several places:
    - Target table or view must be updatable (checked below)
    - A view has special requirements with respect to keys
                                          (checked in check_key_in_view)
    - Target table must not be same as one selected from
                                          (checked in unique_table)
  */

  // Check the list of tables to be deleted from
  for (Table_ref *table_ref = table_list; table_ref;
       table_ref = table_ref->next_local) {
    // Skip tables that are only selected from
    if (!table_ref->updating) continue;

    // Cannot delete from a non-updatable view or derived table.
    if (!table_ref->is_updatable()) {
      my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_ref->alias, "DELETE");
      return true;
    }

    // DELETE does not allow deleting from multi-table views
    if (table_ref->is_multiple_tables()) {
      my_error(ER_VIEW_DELETE_MERGE_VIEW, MYF(0), table_ref->db,
               table_ref->table_name);
      return true;
    }

    if (check_key_in_view(thd, table_ref, table_ref->updatable_base_table())) {
      my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_ref->alias, "DELETE");
      return true;
    }

<<<<<<< HEAD
    // A view must be merged, and thus cannot have a TABLE
    assert(!table_ref->is_view() || table_ref->table == nullptr);

    // Cannot delete from a storage engine that does not support delete.
    Table_ref *base_table = table_ref->updatable_base_table();
    if (base_table->table->file->ha_table_flags() & HA_DELETE_NOT_SUPPORTED) {
      my_error(ER_ILLEGAL_HA, MYF(0), base_table->table_name);
      return true;
    }

    for (Table_ref *tr = base_table; tr != nullptr; tr = tr->referencing_view) {
      tr->updating = true;
    }
<<<<<<< HEAD

    // Table is deleted from, used for privilege checking during execution
    base_table->set_deleted();
  }

  // The hypergraph optimizer has a unified execution path for single-table and
  // multi-table DELETE, and does not need to distinguish between the two. This
  // enables it to perform optimizations like sort avoidance and semi-join
  // flattening even if features specific to single-table DELETE (that is, ORDER
  // BY and LIMIT) are used.
  if (lex->using_hypergraph_optimizer) {
    multitable = true;
  }

  if (!multitable && select->first_inner_query_expression() != nullptr &&
      should_switch_to_multi_table_if_subqueries(thd, select, table_list))
    multitable = true;

  if (multitable) {
    if (!select->m_table_nest.empty())
      propagate_nullability(&select->m_table_nest, false);

    Prepared_stmt_arena_holder ps_holder(thd);
    result = new (thd->mem_root) Query_result_delete;
    if (result == nullptr) return true; /* purecov: inspected */

    // The former is for the pre-iterator executor; the latter is for the
    // iterator executor.
    // TODO(sgunders): Get rid of this when we remove Query_result.
    select->set_query_result(result);
    select->master_query_expression()->set_query_result(result);

    select->make_active_options(SELECT_NO_JOIN_CACHE | SELECT_NO_UNLOCK,
                                OPTION_BUFFER_RESULT);
    select->set_sj_candidates(&sj_candidates_local);
  } else {
    select->make_active_options(0, 0);
=======
=======
    // A view must be merged, and thus cannot have a TABLE 
    assert(!table_ref->is_view() || table_ref->table == NULL);

    // Enable the following code if allowing LIMIT with multi-table DELETE
    assert(select->select_limit == 0);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }

  // Precompute and store the row types of NATURAL/USING joins.
  if (select->leaf_table_count >= 2 &&
      setup_natural_join_row_types(thd, select->m_current_table_nest,
                                   &select->context))
    return true;

  // Enable the following code if allowing LIMIT with multi-table DELETE
  assert(lex->sql_command == SQLCOM_DELETE || !select->has_limit());

  lex->allow_sum_func = 0;

  if (select->setup_conds(thd)) return true;

  assert(select->having_cond() == nullptr && select->group_list.elements == 0 &&
         select->offset_limit == nullptr);

  if (select->resolve_limits(thd)) return true; /* purecov: inspected */

  // check ORDER BY even if it can be ignored
  if (select->order_list.first) {
    Table_ref tables;

    tables.table = table_list->table;
    tables.alias = table_list->alias;

    assert(!select->group_list.elements);
    if (select->setup_base_ref_items(thd)) return true; /* purecov: inspected */
    if (setup_order(thd, select->base_ref_items, &tables, &select->fields,
                    select->order_list.first))
      return true;
  }

  thd->want_privilege = want_privilege_saved;
  thd->mark_used_columns = mark_used_columns_saved;

  if (select->has_ft_funcs() && setup_ftfuncs(thd, select))
    return true; /* purecov: inspected */

  /*
    Check tables to be deleted from for duplicate entries -
    must be done after conditions have been prepared.
  */
  select->exclude_from_table_unique_test = true;

  for (Table_ref *table_ref = table_list; table_ref;
       table_ref = table_ref->next_local) {
    if (!table_ref->is_deleted()) continue;
    /*
      Check that table from which we delete is not used somewhere
      inside subqueries/view.
    */
    Table_ref *duplicate = unique_table(table_ref->updatable_base_table(),
                                        lex->query_tables, false);
    if (duplicate) {
      update_non_unique_table_error(table_ref, "DELETE", duplicate);
      return true;
    }
  }

  select->exclude_from_table_unique_test = false;

  if (select->query_result() &&
      select->query_result()->prepare(thd, select->fields, lex->unit))
    return true; /* purecov: inspected */

  opt_trace_print_expanded_query(thd, select, &trace_wrapper);

  if (select->has_sj_candidates() && select->flatten_subqueries(thd))
    return true;

  select->set_sj_candidates(nullptr);

  if (select->apply_local_transforms(thd, true))
    return true; /* purecov: inspected */

  if (select->is_empty_query()) set_empty_query();

  select->master_query_expression()->set_prepared();

  return false;
}

/**
  Execute a DELETE statement.
*/
bool Sql_cmd_delete::execute_inner(THD *thd) {
  if (is_empty_query()) {
    if (lex->is_explain()) {
      Modification_plan plan(thd, MT_DELETE, /*table_arg=*/nullptr,
                             "No matching rows after partition pruning", true,
                             0);
      return explain_single_table_modification(thd, thd, &plan,
                                               lex->query_block);
    }
    my_ok(thd);
    return false;
  }
  return multitable ? Sql_cmd_dml::execute_inner(thd)
                    : delete_from_single_table(thd);
}

/***************************************************************************
  Delete multiple tables from join
***************************************************************************/

extern "C" int refpos_order_cmp(const void *arg, const void *a, const void *b) {
  const handler *file = static_cast<const handler *>(arg);
  return file->cmp_ref(static_cast<const uchar *>(a),
                       static_cast<const uchar *>(b));
}

DeleteRowsIterator::DeleteRowsIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> source, JOIN *join,
    table_map tables_to_delete_from, table_map immediate_tables)
    : RowIterator(thd),
      m_source(std::move(source)),
      m_join(join),
      m_tables_to_delete_from(tables_to_delete_from),
      m_immediate_tables(immediate_tables),
      // The old optimizer does not use hash join in DELETE statements.
      m_hash_join_tables(thd->lex->using_hypergraph_optimizer
                             ? GetHashJoinTables(join->root_access_path())
                             : 0),
      m_tempfiles(thd->mem_root),
      m_delayed_tables(thd->mem_root) {
  for (const Table_ref *tr = join->query_block->leaf_tables; tr != nullptr;
       tr = tr->next_leaf) {
    TABLE *const table = tr->table;
    const uint tableno = tr->tableno();
    if (!IsBitSet(tableno, tables_to_delete_from)) continue;

    // Record transactional tables that are deleted from.
    if (table->file->has_transactions()) {
      m_transactional_tables |= tr->map();
    }

    // Record which tables have delete triggers that need to be fired.
    if (table->triggers != nullptr) {
      if (table->triggers->has_triggers(TRG_EVENT_DELETE, TRG_ACTION_BEFORE)) {
        m_tables_with_before_triggers |= tr->map();
      }
      if (table->triggers->has_triggers(TRG_EVENT_DELETE, TRG_ACTION_AFTER)) {
        m_tables_with_after_triggers |= tr->map();
      }
    }
  }
}

void SetUpTablesForDelete(THD *thd, JOIN *join) {
  for (const Table_ref *tr = join->query_block->leaf_tables; tr != nullptr;
       tr = tr->next_leaf) {
    if (!tr->is_deleted()) continue;
    TABLE *table = tr->table;
    table->covering_keys.clear_all();
    table->prepare_for_position();
    table->mark_columns_needed_for_delete(thd);
  }

  THD_STAGE_INFO(thd, stage_deleting_from_main_table);
}

<<<<<<< HEAD
bool CheckSqlSafeUpdate(THD *thd, const JOIN *join) {
  if (!Overlaps(thd->variables.option_bits, OPTION_SAFE_UPDATES)) {
    return false;
=======
/**
  Optimize for deletion from one or more tables in a multi-table DELETE

  Function is called when the join order has been determined.
  Calculate which tables can be deleted from immediately and which tables
  must be delayed. Create objects for handling of delayed deletes.
*/

bool Query_result_delete::optimize() {
  DBUG_ENTER("Query_result_delete::optimize");

  SELECT_LEX *const select = unit->first_select();

  JOIN *const join = select->join;

  ASSERT_BEST_REF_IN_JOIN_ORDER(join);

<<<<<<< HEAD
=======
  SELECT_LEX *const select= unit->first_select();
  assert(join == select->join);

>>>>>>> upstream/cluster-7.6
  if ((thd->variables.option_bits & OPTION_SAFE_UPDATES) &&
      error_if_full_join(join))
    DBUG_RETURN(true);

  if (!(tempfiles =
            (Unique **)sql_calloc(sizeof(Unique *) * delete_table_count)))
    DBUG_RETURN(true); /* purecov: inspected */

  if (!(tables = (TABLE **)sql_calloc(sizeof(TABLE *) * delete_table_count)))
    DBUG_RETURN(true); /* purecov: inspected */

  bool delete_while_scanning = true;
  for (TABLE_LIST *tr = select->leaf_tables; tr; tr = tr->next_leaf) {
    if (!tr->updating) continue;
    delete_table_map |= tr->map();
    if (delete_while_scanning && unique_table(tr, join->tables_list, false)) {
      /*
        If the table being deleted from is also referenced in the query,
        defer delete so that the delete doesn't interfer with reading of this
        table.
      */
      delete_while_scanning = false;
    }
>>>>>>> pr/231
  }

  if (join->query_block->has_limit()) {
    return false;
  }

  bool full_scan = false;
  WalkAccessPaths(
      join->root_access_path(), join, WalkAccessPathPolicy::ENTIRE_QUERY_BLOCK,
      [&full_scan](const AccessPath *path, const JOIN *) {
        if (path->type == AccessPath::TABLE_SCAN) {
          full_scan |= path->table_scan().table->pos_in_table_list->updating;
        } else if (path->type == AccessPath::INDEX_SCAN) {
          full_scan |= path->index_scan().table->pos_in_table_list->updating;
        }
        return full_scan;
      });

  if (full_scan) {
    // Append the first warning (if any) to the error message. The warning may
    // give the user a hint as to why index access couldn't be chosen.
    my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE, MYF(0),
             thd->get_stmt_da()->get_first_condition_message());
    return true;
  }

  return false;
}

bool DeleteRowsIterator::Init() {
  if (CheckSqlSafeUpdate(thd(), m_join)) {
    return true;
  }

  if (m_source->Init()) {
    return true;
  }

  for (Table_ref *tr = m_join->query_block->leaf_tables; tr != nullptr;
       tr = tr->next_leaf) {
    TABLE *const table = tr->table;
    const uint tableno = tr->tableno();
    if (!IsBitSet(tableno, m_tables_to_delete_from)) continue;

    // We are going to delete from this table
    if (IsBitSet(tableno, m_tables_with_after_triggers)) {
      /*
        The table has AFTER DELETE triggers that might access the subject
        table and therefore might need delete to be done immediately.
        So we turn-off the batching.
      */
      (void)table->file->ha_extra(HA_EXTRA_DELETE_CANNOT_BATCH);
    }
    if (thd()->lex->is_ignore()) {
      table->file->ha_extra(HA_EXTRA_IGNORE_DUP_KEY);
    }
    if (thd()->is_error()) return true;

    // Set up a Unique object for each table whose delete operation is deferred.
    if (!IsBitSet(tableno, m_immediate_tables)) {
      auto tempfile = make_unique_destroy_only<Unique>(
          thd()->mem_root, refpos_order_cmp, table->file,
          table->file->ref_length, thd()->variables.sortbuff_size);
      if (tempfile == nullptr || m_tempfiles.push_back(std::move(tempfile)) ||
          m_delayed_tables.push_back(table)) {
        return true; /* purecov: inspected */
      }
    }
  }

  assert(!thd()->is_error());

  return false;
}

bool DeleteRowsIterator::DoImmediateDeletesAndBufferRowIds() {
  DBUG_TRACE;

  // For now, don't actually delete anything in EXPLAIN ANALYZE. (If we enable
  // it, INSERT and UPDATE should also be changed to have side effects when
  // running under EXPLAIN ANALYZE.)
  if (thd()->lex->is_explain_analyze) {
    return false;
  }

  int unique_counter = 0;

  for (Table_ref *tr = m_join->query_block->leaf_tables; tr != nullptr;
       tr = tr->next_leaf) {
    const table_map map = tr->map();

    // Check whether this table is being deleted from
    if (!Overlaps(map, m_tables_to_delete_from)) continue;

    const bool immediate = Overlaps(map, m_immediate_tables);

    TABLE *const table = tr->table;

    assert(immediate || table == m_delayed_tables[unique_counter]);

    /*
      If not doing immediate deletion, increment unique_counter and assign
      "tempfile" here, so that it is available when and if it is needed.
    */
    Unique *const tempfile =
        immediate ? nullptr : m_tempfiles[unique_counter++].get();

    // Check if using outer join and no row found, or row is already deleted
    if (table->has_null_row() || table->has_deleted_row()) continue;

    // Hash joins have already copied the row ID from the join buffer into
    // table->file->ref. Nested loop joins have not, so we call position() to
    // get the row ID from the handler.
    if (!Overlaps(map, m_hash_join_tables)) {
      table->file->position(table->record[0]);
    }

    if (immediate) {
      // Rows from this table can be deleted immediately
      const ha_rows last_deleted = m_deleted_rows;
      table->set_deleted_row();
      if (DeleteCurrentRowAndProcessTriggers(
              thd(), table, Overlaps(map, m_tables_with_before_triggers),
              Overlaps(map, m_tables_with_after_triggers), &m_deleted_rows)) {
        return true;
      }
      if (last_deleted != m_deleted_rows &&
          !Overlaps(map, m_transactional_tables)) {
        thd()->get_transaction()->mark_modified_non_trans_table(
            Transaction_ctx::STMT);
      }
    } else {
      // Save deletes in a Unique object, to be carried out later.
      m_has_delayed_deletes = true;
      if (tempfile->unique_add(table->file->ref)) {
        /* purecov: begin inspected */
        return true;
        /* purecov: end */
      }
    }
  }
  return false;
}

bool DeleteRowsIterator::DoDelayedDeletes() {
  DBUG_TRACE;
  assert(m_has_delayed_deletes);
  for (uint counter = 0; counter < m_delayed_tables.size(); counter++) {
    TABLE *const table = m_delayed_tables[counter];
    if (m_tempfiles[counter]->get(table)) return true;

    if (DoDelayedDeletesFromTable(table) || thd()->killed) {
      return true;
    }
  }
  return false;
}

bool DeleteRowsIterator::DoDelayedDeletesFromTable(TABLE *table) {
  DBUG_TRACE;

  /*
    Ignore any rows not found in reference tables as they may already have
    been deleted by foreign key handling
  */
  unique_ptr_destroy_only<RowIterator> iterator =
      init_table_iterator(thd(), table, /*ignore_not_found_rows=*/true,
                          /*count_examined_rows=*/false);
  if (iterator == nullptr) return true;

  const uint tableno = table->pos_in_table_list->tableno();
  const bool has_before_triggers =
      IsBitSet(tableno, m_tables_with_before_triggers);
  const bool has_after_triggers =
      IsBitSet(tableno, m_tables_with_after_triggers);

  const bool will_batch = !table->file->start_bulk_delete();
  const ha_rows last_deleted = m_deleted_rows;

  bool local_error = false;
  while (!thd()->killed) {
    const int read_error = iterator->Read();
    if (read_error == 0) {
      if (DeleteCurrentRowAndProcessTriggers(thd(), table, has_before_triggers,
                                             has_after_triggers,
                                             &m_deleted_rows)) {
        local_error = true;
        break;
      }
    } else if (read_error == -1) {
      break;  // EOF
    } else {
      local_error = true;
      break;
    }
  }

  if (will_batch) {
    const int bulk_error = table->file->end_bulk_delete();
    if (bulk_error != 0 && !local_error) {
      local_error = true;
      myf error_flags = MYF(0);
      if (table->file->is_fatal_error(bulk_error)) {
        error_flags |= ME_FATALERROR;
      }
      table->file->print_error(bulk_error, error_flags);
    }
  }
  if (last_deleted != m_deleted_rows &&
      !IsBitSet(tableno, m_transactional_tables)) {
    thd()->get_transaction()->mark_modified_non_trans_table(
        Transaction_ctx::STMT);
  }

  return local_error;
}

int DeleteRowsIterator::Read() {
  bool local_error = false;

  // First process all the rows returned by the join. Delete immediately from
  // the tables that allow immediate delete, and buffer row IDs for the rows to
  // delete in the other tables.
  while (true) {
    if (const int read_error = m_source->Read();
        read_error > 0 || thd()->is_error()) {
      local_error = true;
      break;
    } else if (read_error < 0) {
      break;  // EOF
    } else if (thd()->killed) {
      thd()->send_kill_message();
      return 1;
    } else {
      if (DoImmediateDeletesAndBufferRowIds()) {
        local_error = true;
        break;
      }
    }
  }

  // If rows were buffered for delayed deletion, and no error occurred, do the
  // delayed deletions.
  //
  // In case a non-transactional table has been modified, we do the delayed
  // deletes even if an error has been raised. (The rationale is probably that
  // the deletes already performed cannot be rolled back, so deleting the
  // corresponding rows in the other tables will make the state of the tables
  // more consistent.)
  if (m_has_delayed_deletes &&
      (!local_error || thd()->get_transaction()->has_modified_non_trans_table(
                           Transaction_ctx::STMT))) {
    THD_STAGE_INFO(thd(), stage_deleting_from_reference_tables);
    if (DoDelayedDeletes()) {
      local_error = true;
    }
  }

  const THD::killed_state killed_status =
      !local_error ? THD::NOT_KILLED : thd()->killed.load();

  if (!local_error ||
      thd()->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT)) {
    if (mysql_bin_log.is_open()) {
      int errcode = 0;
      if (!local_error)
        thd()->clear_error();
      else
        errcode = query_error_code(thd(), killed_status == THD::NOT_KILLED);
      thd()->thread_specific_used = true;
      if (thd()->binlog_query(
              THD::ROW_QUERY_TYPE, thd()->query().str, thd()->query().length,
              m_transactional_tables != 0, false, false, errcode) &&
          m_transactional_tables == m_tables_to_delete_from) {
        local_error = true;  // Log write failed: roll back the SQL statement
      }
    }
  }

  if (local_error) {
    return 1;
  } else {
    thd()->set_row_count_func(m_deleted_rows);
    return -1;
  }
}

bool Sql_cmd_delete::accept(THD *thd, Select_lex_visitor *visitor) {
  return thd->lex->unit->accept(visitor);
}

table_map GetImmediateDeleteTables(const JOIN *join, table_map delete_tables) {
  // The hypergraph optimizer determines the immediate delete tables during
  // planning, not after planning. The only time this function is called when
  // using the hypergraph optimizer is when there is an impossible WHERE clause,
  // in which case join order optimization is short-circuited. See
  // JOIN::create_access_paths_for_zero_rows().
  if (join->thd->lex->using_hypergraph_optimizer) {
    assert(join->root_access_path()->type == AccessPath::ZERO_ROWS);
    return 0;
  }

  for (Table_ref *tr = join->query_block->leaf_tables; tr != nullptr;
       tr = tr->next_leaf) {
    if (!tr->is_deleted()) continue;

    if (unique_table(tr, join->tables_list, false) != nullptr) {
      /*
        If the table being deleted from is also referenced in the query,
        defer delete so that the delete doesn't interfere with reading of this
        table.
      */
      return 0;
    }
  }

  /*
    In some cases, rows may be deleted from the first table(s) in the join order
    while performing the join operation when "delete_while_scanning" is true and
      1. deleting from one of the const tables, or
      2. deleting from the first non-const table
  */
<<<<<<< HEAD
  table_map candidate_tables = join->const_table_map;  // 1
  if (join->primary_tables > join->const_tables) {
    // Can be called in different stages after the join order has been
    // determined, so look into QEP_TAB or JOIN_TAB depending on which is
    // available in the current stage.
    const Table_ref *first_non_const;
    if (join->qep_tab != nullptr) {
      first_non_const = join->qep_tab[join->const_tables].table_ref;
=======
  table_map possible_tables = join->const_table_map;  // 1
  if (join->primary_tables > join->const_tables)
    possible_tables |=
        join->best_ref[join->const_tables]->table_ref->map();  // 2
  if (delete_while_scanning)
    delete_immediate = delete_table_map & possible_tables;

  // Set up a Unique object for each table whose delete operation is deferred:

  Unique **tempfile = tempfiles;
  TABLE **table_ptr = tables;
  for (uint i = 0; i < join->primary_tables; i++) {
    const table_map map = join->best_ref[i]->table_ref->map();

    if (!(map & delete_table_map & ~delete_immediate)) continue;

    TABLE *const table = join->best_ref[i]->table();
    if (!(*tempfile++ = new (*THR_MALLOC)
              Unique(refpos_order_cmp, (void *)table->file,
                     table->file->ref_length, thd->variables.sortbuff_size)))
      DBUG_RETURN(true); /* purecov: inspected */
    *(table_ptr++) = table;
  }
  assert(select == thd->lex->current_select());

  if (select->has_ft_funcs() && init_ftfuncs(thd, select)) DBUG_RETURN(true);

  DBUG_RETURN(thd->is_fatal_error != 0);
}

void Query_result_delete::cleanup() {
  // Cleanup only needed if result object has been prepared
  if (delete_table_count == 0) return;

  // Remove optimize structs for this operation.
  for (uint counter = 0; counter < delete_table_count; counter++) {
    if (tempfiles && tempfiles[counter]) destroy(tempfiles[counter]);
  }
  tempfiles = NULL;
  tables = NULL;
}

bool Query_result_delete::send_data(List<Item> &) {
  DBUG_ENTER("Query_result_delete::send_data");

  JOIN *const join = unit->first_select()->join;

<<<<<<< HEAD
  DBUG_ASSERT(thd->lex->current_select() == unit->first_select());
  int unique_counter = 0;
=======
  assert(thd->lex->current_select() == unit->first_select());
  int unique_counter= 0;
>>>>>>> upstream/cluster-7.6

  for (uint i = 0; i < join->primary_tables; i++) {
    const table_map map = join->qep_tab[i].table_ref->map();

    // Check whether this table is being deleted from
    if (!(map & delete_table_map)) continue;

    const bool immediate = map & delete_immediate;

    TABLE *const table = join->qep_tab[i].table();

    assert(immediate || table == tables[unique_counter]);

    /*
      If not doing immediate deletion, increment unique_counter and assign
      "tempfile" here, so that it is available when and if it is needed.
    */
    Unique *const tempfile = immediate ? NULL : tempfiles[unique_counter++];

    // Check if using outer join and no row found, or row is already deleted
    if (table->has_null_row() || table->has_deleted_row()) continue;

    table->file->position(table->record[0]);
    found_rows++;

    if (immediate) {
      // Rows from this table can be deleted immediately
      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_BEFORE, false))
        DBUG_RETURN(true);
      table->set_deleted_row();
      if (map & non_transactional_table_map) non_transactional_deleted = true;
      if (!(error = table->file->ha_delete_row(table->record[0]))) {
        deleted_rows++;
        if (!table->file->has_transactions())
          thd->get_transaction()->mark_modified_non_trans_table(
              Transaction_ctx::STMT);
        if (table->triggers &&
            table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                              TRG_ACTION_AFTER, false))
          DBUG_RETURN(true);
      } else {
        myf error_flags = MYF(0);
        if (table->file->is_fatal_error(error)) error_flags |= ME_FATALERROR;
        table->file->print_error(error, error_flags);

        /*
          If IGNORE option is used errors caused by ha_delete_row will
          be downgraded to warnings and don't have to stop the iteration.
        */
        if (thd->is_error()) DBUG_RETURN(true);

        /*
          If IGNORE keyword is used, then 'error' variable will have the error
          number which is ignored. Reset the 'error' variable if IGNORE is used.
          This is necessary to call my_ok().
        */
        error = 0;
      }
>>>>>>> pr/231
    } else {
      ASSERT_BEST_REF_IN_JOIN_ORDER(join);
      first_non_const = join->best_ref[join->const_tables]->table_ref;
    }
    candidate_tables |= first_non_const->map();  // 2
  }
<<<<<<< HEAD

  return delete_tables & candidate_tables;
=======
  DBUG_RETURN(false);
}

void Query_result_delete::send_error(uint errcode, const char *err) {
  DBUG_ENTER("Query_result_delete::send_error");

  /* First send error what ever it is ... */
  my_message(errcode, err, MYF(0));

  DBUG_VOID_RETURN;
}

void Query_result_delete::abort_result_set() {
  DBUG_ENTER("Query_result_delete::abort_result_set");

  /* the error was handled or nothing deleted and no side effects return */
  if (error_handled ||
      (!thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT) &&
       deleted_rows == 0))
    DBUG_VOID_RETURN;

  /*
    If rows from the first table only has been deleted and it is
    transactional, just do rollback.
    The same if all tables are transactional, regardless of where we are.
    In all other cases do attempt deletes ...
  */
  if (!delete_completed && non_transactional_deleted) {
    /*
      We have to execute the recorded do_deletes() and write info into the
      error log
    */
    error = 1;
    send_eof();
    assert(error_handled);
    DBUG_VOID_RETURN;
  }

  if (thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT)) {
    /*
       there is only side effects; to binlog with the error
    */
    if (mysql_bin_log.is_open()) {
      int errcode = query_error_code(thd, thd->killed == THD::NOT_KILLED);
      /* possible error of writing binary log is ignored deliberately */
      (void)thd->binlog_query(THD::ROW_QUERY_TYPE, thd->query().str,
                              thd->query().length, transactional_table_map != 0,
                              false, false, errcode);
    }
  }
  DBUG_VOID_RETURN;
}

/**
  Do delete from other tables.

  @retval 0 ok
  @retval 1 error
*/

int Query_result_delete::do_deletes() {
  DBUG_ENTER("Query_result_delete::do_deletes");
<<<<<<< HEAD
  DBUG_ASSERT(!delete_completed);

  DBUG_ASSERT(thd->lex->current_select() == unit->first_select());
  delete_completed = true;  // Mark operation as complete
  if (found_rows == 0) DBUG_RETURN(0);
=======
  assert(do_delete);

  assert(thd->lex->current_select() == unit->first_select());
  do_delete= false;                                 // Mark called
  if (!found)
    DBUG_RETURN(0);
>>>>>>> upstream/cluster-7.6

  for (uint counter = 0; counter < delete_table_count; counter++) {
    TABLE *const table = tables[counter];
    if (table == NULL) break;

    if (tempfiles[counter]->get(table)) DBUG_RETURN(1);

    int local_error = do_table_deletes(table);

    if (thd->killed && !local_error) DBUG_RETURN(1);

    if (local_error == -1)  // End of file
      local_error = 0;

    if (local_error) DBUG_RETURN(local_error);
  }
  DBUG_RETURN(0);
}

/**
   Implements the inner loop of nested-loops join within multi-DELETE
   execution.

   @param table The table from which to delete.

   @return Status code

   @retval  0 All ok.
   @retval  1 Triggers or handler reported error.
   @retval -1 End of file from handler.
*/
int Query_result_delete::do_table_deletes(TABLE *table) {
  myf error_flags = MYF(0); /**< Flag for fatal errors */
  int local_error = 0;
  READ_RECORD info;
  ha_rows last_deleted = deleted_rows;
  DBUG_ENTER("Query_result_delete::do_table_deletes");
  if (init_read_record(&info, thd, table, NULL, 0, 1, false)) DBUG_RETURN(1);
  /*
    Ignore any rows not found in reference tables as they may already have
    been deleted by foreign key handling
  */
  info.ignore_not_found_rows = 1;
  bool will_batch = !table->file->start_bulk_delete();
  while (!(local_error = info.read_record(&info)) && !thd->killed) {
    if (table->triggers &&
        table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                          TRG_ACTION_BEFORE, false)) {
      local_error = 1;
      break;
    }

    local_error = table->file->ha_delete_row(table->record[0]);
    if (local_error) {
      if (table->file->is_fatal_error(local_error))
        error_flags |= ME_FATALERROR;

      table->file->print_error(local_error, error_flags);
      /*
        If IGNORE option is used errors caused by ha_delete_row will
        be downgraded to warnings and don't have to stop the iteration.
      */
      if (thd->is_error()) break;
    }

    /*
      Increase the reported number of deleted rows only if no error occurred
      during ha_delete_row.
      Also, don't execute the AFTER trigger if the row operation failed.
    */
    if (!local_error) {
      deleted_rows++;
      if (table->pos_in_table_list->map() & non_transactional_table_map)
        non_transactional_deleted = true;

      if (table->triggers &&
          table->triggers->process_triggers(thd, TRG_EVENT_DELETE,
                                            TRG_ACTION_AFTER, false)) {
        local_error = 1;
        break;
      }
    }
  }
  if (will_batch) {
    int tmp_error = table->file->end_bulk_delete();
    if (tmp_error && !local_error) {
      local_error = tmp_error;
      if (table->file->is_fatal_error(local_error))
        error_flags |= ME_FATALERROR;

      table->file->print_error(local_error, error_flags);
    }
  }
  if (last_deleted != deleted_rows && !table->file->has_transactions())
    thd->get_transaction()->mark_modified_non_trans_table(
        Transaction_ctx::STMT);

  end_read_record(&info);

  DBUG_RETURN(local_error);
}

/**
  Send ok to the client

  The function has to perform all deferred deletes that have been queued up.

  @return false if success, true if error
*/

bool Query_result_delete::send_eof() {
  THD::killed_state killed_status = THD::NOT_KILLED;
  THD_STAGE_INFO(thd, stage_deleting_from_reference_tables);

  /* Does deletes for the last n - 1 tables, returns 0 if ok */
  int local_error = do_deletes();  // returns 0 if success

  /* compute a total error to know if something failed */
  local_error = local_error || error;
  killed_status = (local_error == 0) ? THD::NOT_KILLED : thd->killed.load();
  /* reset used flags */

  if ((local_error == 0) ||
      thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT)) {
    if (mysql_bin_log.is_open()) {
      int errcode = 0;
      if (local_error == 0)
        thd->clear_error();
      else
<<<<<<< HEAD
        errcode = query_error_code(thd, killed_status == THD::NOT_KILLED);
      if (thd->binlog_query(THD::ROW_QUERY_TYPE, thd->query().str,
                            thd->query().length, transactional_table_map != 0,
                            false, false, errcode) &&
          !non_transactional_table_map) {
        local_error = 1;  // Log write failed: roll back the SQL statement
=======
        errcode= query_error_code(thd, killed_status == THD::NOT_KILLED);
      thd->thread_specific_used= TRUE;
      if (thd->binlog_query(THD::ROW_QUERY_TYPE,
                            thd->query().str, thd->query().length,
                            transactional_table_map != 0, FALSE, FALSE,
                            errcode) &&
          !non_transactional_table_map)
      {
	local_error=1;  // Log write failed: roll back the SQL statement
>>>>>>> upstream/cluster-7.6
      }
    }
  }
  if (local_error != 0)
    error_handled = true;  // to force early leave from ::send_error()

  if (!local_error) {
    ::my_ok(thd, deleted_rows);
  }
  return 0;
>>>>>>> pr/231
}
<<<<<<< HEAD
=======


bool Sql_cmd_delete::execute(THD *thd)
{
  assert(thd->lex->sql_command == SQLCOM_DELETE);

  LEX *const lex= thd->lex;
  SELECT_LEX *const select_lex= lex->select_lex;
  SELECT_LEX_UNIT *const unit= lex->unit;
  TABLE_LIST *const first_table= select_lex->get_table_list();
  TABLE_LIST *const all_tables= first_table;

  if (delete_precheck(thd, all_tables))
    return true;
  assert(select_lex->offset_limit == 0);
  unit->set_limit(select_lex);

  /* Push ignore / strict error handler */
  Ignore_error_handler ignore_handler;
  Strict_error_handler strict_handler;
  if (thd->lex->is_ignore())
    thd->push_internal_handler(&ignore_handler);
  else if (thd->is_strict_mode())
    thd->push_internal_handler(&strict_handler);

  MYSQL_DELETE_START(const_cast<char*>(thd->query().str));
  bool res = mysql_delete(thd, unit->select_limit_cnt);
  MYSQL_DELETE_DONE(res, (ulong) thd->get_row_count_func());

  /* Pop ignore / strict error handler */
  if (thd->lex->is_ignore() || thd->is_strict_mode())
    thd->pop_internal_handler();

  return res;
}


bool Sql_cmd_delete_multi::execute(THD *thd)
{
  assert(thd->lex->sql_command == SQLCOM_DELETE_MULTI);

  bool res= false;
  LEX *const lex= thd->lex;
  SELECT_LEX *const select_lex= lex->select_lex;
  TABLE_LIST *const first_table= select_lex->get_table_list();
  TABLE_LIST *const all_tables= first_table;

  TABLE_LIST *aux_tables= thd->lex->auxiliary_table_list.first;
  uint del_table_count;
  Query_result_delete *del_result;

  if (multi_delete_precheck(thd, all_tables))
    return true;

  /* condition will be TRUE on SP re-excuting */
  if (select_lex->item_list.elements != 0)
    select_lex->item_list.empty();
  if (add_item_to_list(thd, new Item_null()))
    return true;

  THD_STAGE_INFO(thd, stage_init);
  if ((res= open_tables_for_query(thd, all_tables, 0)))
    return true;

  if (run_before_dml_hook(thd))
    return true;

  MYSQL_MULTI_DELETE_START(const_cast<char*>(thd->query().str));
  if (mysql_multi_delete_prepare(thd, &del_table_count))
  {
    MYSQL_MULTI_DELETE_DONE(1, 0);
    return true;
  }

  if (!thd->is_fatal_error &&
      (del_result= new Query_result_delete(aux_tables, del_table_count)))
  {
    assert(select_lex->having_cond() == NULL &&
           !select_lex->order_list.elements &&
           !select_lex->group_list.elements);

    Ignore_error_handler ignore_handler;
    Strict_error_handler strict_handler;
    if (thd->lex->is_ignore())
      thd->push_internal_handler(&ignore_handler);
    else if (thd->is_strict_mode())
      thd->push_internal_handler(&strict_handler);

    res= handle_query(thd, lex, del_result,
                      SELECT_NO_JOIN_CACHE |
                      SELECT_NO_UNLOCK |
                      OPTION_SETUP_TABLES_DONE,
                      OPTION_BUFFER_RESULT);

    if (thd->lex->is_ignore() || thd->is_strict_mode())
      thd->pop_internal_handler();

    if (res)
      del_result->abort_result_set();

    MYSQL_MULTI_DELETE_DONE(res, del_result->num_deleted());
    delete del_result;
  }
  else
  {
    res= true;                                // Error
    MYSQL_MULTI_DELETE_DONE(1, 0);
  }

  return res;
}
>>>>>>> upstream/cluster-7.6
