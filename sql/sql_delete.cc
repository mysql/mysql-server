/* Copyright (c) 2000, 2021, Oracle and/or its affiliates.

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
#include "sql/iterators/row_iterator.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
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
  /// Pointers to temporary files used for delayed deletion of rows
  Mem_root_array<unique_ptr_destroy_only<Unique>> tempfiles;
  /// Pointers to table objects matching tempfiles
  Mem_root_array<TABLE *> tables;
  /// True if at least one row has been buffered for delayed deletion.
  bool has_buffered_rows{false};
  /// True if a DELETE statement involving non-transactional tables failed
  /// before all rows had been processed.
  bool non_transactional_delete_aborted{false};
  /// Number of rows deleted
  ha_rows deleted_rows{0};
  /// Map of all tables to delete rows from
  table_map delete_table_map{0};
  /// Map of tables to delete from immediately
  table_map delete_immediate{0};
  // Map of transactional tables to be deleted from
  table_map transactional_table_map{0};
  /// Map of tables with before triggers.
  table_map tables_with_before_triggers{0};
  /// Map of tables with after triggers.
  table_map tables_with_after_triggers{0};
  /// True if the full delete operation is complete
  bool delete_completed{false};
  /*
     error handling (rollback and binlogging) can happen in send_eof()
     so that afterward send_error() needs to find out that.
  */
  bool error_handled{false};

 public:
  explicit Query_result_delete(THD *thd)
      : Query_result_interceptor(),
        tempfiles(thd->mem_root),
        tables(thd->mem_root) {}
  bool need_explain_interceptor() const override { return true; }
  bool prepare(THD *thd, const mem_root_deque<Item *> &list,
               Query_expression *u) override;
  bool send_data(THD *thd, const mem_root_deque<Item *> &items) override;
  void send_error(THD *thd, uint errcode, const char *err) override;
  bool optimize() override;
  bool start_execution(THD *) override {
    delete_completed = false;
    return false;
  }
  bool send_eof(THD *thd) override;
  void abort_result_set(THD *thd) override;
  void cleanup(THD *thd) override;

 private:
  bool do_deletes(THD *thd);
  bool do_table_deletes(THD *thd, TABLE *table);
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

  TABLE_LIST *tables = lex->query_tables;

  if (!multitable) {
    if (check_one_table_access(thd, DELETE_ACL, tables)) return true;
  } else {
    TABLE_LIST *aux_tables = delete_tables->first;
    TABLE_LIST **save_query_tables_own_last = lex->query_tables_own_last;

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
  TABLE_LIST *const table_list = query_block->get_table_list();
  THD::killed_state killed_status = THD::NOT_KILLED;
  THD::enum_binlog_query_type query_type = THD::ROW_QUERY_TYPE;

  const bool safe_update = thd->variables.option_bits & OPTION_SAFE_UPDATES;

  TABLE_LIST *const delete_table_ref = table_list->updatable_base_table();
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
    static_cast<void>(substitute_gc(thd, query_block, conds, nullptr, order));

  const bool const_cond = conds == nullptr || conds->const_item();
  const bool const_cond_result = const_cond && (!conds || conds->val_int());
  if (thd->is_error())  // Error during val_int()
    return true;        /* purecov: inspected */
  /*
    We are passing HA_EXTRA_IGNORE_DUP_KEY flag here to recreate query with
    IGNORE keyword within federated storage engine. If federated engine is
    removed in the future, use of HA_EXTRA_IGNORE_DUP_KEY and
    HA_EXTRA_NO_IGNORE_DUP_KEY flag should be removed from
    delete_from_single_table(), Query_result_delete::optimize() and
    Query_result_delete::cleanup().
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

    if (optimize_cond(thd, &conds, &cond_equal, query_block->join_list,
                      &result))
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
      MEM_ROOT temp_mem_root(key_memory_test_quick_select_exec,
                             thd->variables.range_alloc_block_size);
      no_rows = test_quick_select(
                    thd, thd->mem_root, &temp_mem_root, keys_to_use, 0, 0,
                    limit, safe_update, ORDER_NOT_RELEVANT, table,
                    /*skip_records_in_range=*/false, conds, &needed_reg_dummy,
                    table->force_index, query_block, &range_scan) < 0;
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
  if (table->quick_keys.is_clear_all()) {
    thd->server_status |= SERVER_QUERY_NO_INDEX_USED;

    /*
      Safe update error isn't returned if:
      1) It is  an EXPLAIN statement OR
      2) LIMIT is present.

      Append the first warning (if any) to the error message. This allows the
      user to understand why index access couldn't be chosen.
    */
    if (!thd->lex->is_explain() && safe_update && !using_limit) {
      my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE, MYF(0),
               thd->get_stmt_da()->get_first_condition_message());
      return true;
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
      rows = range_scan->num_output_rows;
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

      if (conds != nullptr) {
        path = NewFilterAccessPath(thd, path, conds);
      }

      fsort.reset(new (thd->mem_root) Filesort(
          thd, {table}, /*keep_buffers=*/false, order, HA_POS_ERROR,
          /*remove_duplicates=*/false,
          /*force_sort_positions=*/true, /*unwrap_rollup=*/false));
      path = NewSortAccessPath(thd, path, fsort.get(),
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
  assert(!lex->is_explain());

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
  assert(transactional_table || deleted_rows == 0 ||
         thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT));
  if (error < 0) {
    my_ok(thd, deleted_rows);
    DBUG_PRINT("info", ("%ld records deleted", (long)deleted_rows));
  }
  return error > 0;
}

/**
  Prepare a DELETE statement
*/

bool Sql_cmd_delete::prepare_inner(THD *thd) {
  DBUG_TRACE;

  Query_block *const select = lex->query_block;
  TABLE_LIST *const table_list = select->get_table_list();

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
  for (TABLE_LIST *table_ref = table_list; table_ref;
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

    // A view must be merged, and thus cannot have a TABLE
    assert(!table_ref->is_view() || table_ref->table == nullptr);

    // Cannot delete from a storage engine that does not support delete.
    TABLE_LIST *base_table = table_ref->updatable_base_table();
    if (base_table->table->file->ha_table_flags() & HA_DELETE_NOT_SUPPORTED) {
      my_error(ER_ILLEGAL_HA, MYF(0), base_table->table_name);
      return true;
    }

    for (TABLE_LIST *tr = base_table; tr != nullptr;
         tr = tr->referencing_view) {
      tr->updating = true;
    }

    // Table is deleted from, used for privilege checking during execution
    base_table->set_deleted();
  }

  if (!multitable && select->first_inner_query_expression() != nullptr &&
      should_switch_to_multi_table_if_subqueries(thd, select, table_list))
    multitable = true;

  if (multitable) {
    if (!select->top_join_list.empty())
      propagate_nullability(&select->top_join_list, false);

    Prepared_stmt_arena_holder ps_holder(thd);
    result = new (thd->mem_root) Query_result_delete(thd);
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
  }

  // Precompute and store the row types of NATURAL/USING joins.
  if (select->leaf_table_count >= 2 &&
      setup_natural_join_row_types(thd, select->join_list, &select->context))
    return true;

  // Enable the following code if allowing LIMIT with multi-table DELETE
  assert(sql_command_code() == SQLCOM_DELETE ||
         select->select_limit == nullptr);

  lex->allow_sum_func = 0;

  if (select->setup_conds(thd)) return true;

  assert(select->having_cond() == nullptr && select->group_list.elements == 0 &&
         select->offset_limit == nullptr);

  if (select->resolve_limits(thd)) return true; /* purecov: inspected */

  // check ORDER BY even if it can be ignored
  if (select->order_list.first) {
    TABLE_LIST tables;

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

  for (TABLE_LIST *table_ref = table_list; table_ref;
       table_ref = table_ref->next_local) {
    if (!table_ref->is_deleted()) continue;
    /*
      Check that table from which we delete is not used somewhere
      inside subqueries/view.
    */
    TABLE_LIST *duplicate = unique_table(table_ref->updatable_base_table(),
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

namespace {

bool Query_result_delete::prepare(THD *thd, const mem_root_deque<Item *> &,
                                  Query_expression *u) {
  DBUG_TRACE;
  unit = u;

  for (TABLE_LIST *tr = u->first_query_block()->leaf_tables; tr;
       tr = tr->next_leaf) {
    if (!tr->is_deleted()) continue;

    delete_table_map |= tr->map();

    // Record transactional tables that are deleted from:
    if (tr->table->file->has_transactions())
      transactional_table_map |= tr->map();

    // Record which tables have delete triggers that need to be fired.
    if (tr->table->triggers != nullptr) {
      if (tr->table->triggers->has_triggers(TRG_EVENT_DELETE,
                                            TRG_ACTION_BEFORE)) {
        tables_with_before_triggers |= tr->map();
      }
      if (tr->table->triggers->has_triggers(TRG_EVENT_DELETE,
                                            TRG_ACTION_AFTER)) {
        tables_with_after_triggers |= tr->map();
      }
    }
  }

  THD_STAGE_INFO(thd, stage_deleting_from_main_table);
  return false;
}

/**
  Optimize for deletion from one or more tables in a multi-table DELETE

  Function is called when the join order has been determined.
  Calculate which tables can be deleted from immediately and which tables
  must be delayed. Create objects for handling of delayed deletes.
*/

bool Query_result_delete::optimize() {
  DBUG_TRACE;

  Query_block *const select = unit->first_query_block();

  JOIN *const join = select->join;
  THD *thd = join->thd;

  if ((thd->variables.option_bits & OPTION_SAFE_UPDATES) &&
      error_if_full_join(join))
    return true;

  for (TABLE_LIST *tr = select->leaf_tables; tr != nullptr;
       tr = tr->next_leaf) {
    TABLE *const table = tr->table;
    const table_map map = tr->map();
    if (!(map & delete_table_map)) continue;

    // We are going to delete from this table
    // Don't use record cache
    table->no_cache = true;
    table->covering_keys.clear_all();
    if (table->triggers &&
        table->triggers->has_triggers(TRG_EVENT_DELETE, TRG_ACTION_AFTER)) {
      /*
        The table has AFTER DELETE triggers that might access the subject
        table and therefore might need delete to be done immediately.
        So we turn-off the batching.
      */
      (void)table->file->ha_extra(HA_EXTRA_DELETE_CANNOT_BATCH);
    }
    if (thd->lex->is_ignore()) table->file->ha_extra(HA_EXTRA_IGNORE_DUP_KEY);
    table->prepare_for_position();
    table->mark_columns_needed_for_delete(thd);
    if (thd->is_error()) return true;
  }

  delete_immediate = GetImmediateDeleteTables(join, delete_table_map);

  // Set up a Unique object for each table whose delete operation is deferred:

  for (TABLE_LIST *tr = select->leaf_tables; tr != nullptr;
       tr = tr->next_leaf) {
    const table_map map = tr->map();

    if (!(map & delete_table_map & ~delete_immediate)) continue;

    TABLE *const table = tr->table;
    auto tempfile = make_unique_destroy_only<Unique>(
        thd->mem_root, refpos_order_cmp, table->file, table->file->ref_length,
        thd->variables.sortbuff_size);
    if (tempfile == nullptr || tempfiles.push_back(move(tempfile)) ||
        tables.push_back(table)) {
      return true; /* purecov: inspected */
    }
  }

  assert(!thd->is_error());

  return false;
}

void Query_result_delete::cleanup(THD *) {
  // Remove optimize structs for this operation.
  tempfiles.clear();
  tables.clear();
  // Reset state and statistics members:
  error_handled = false;
  non_transactional_delete_aborted = false;
  has_buffered_rows = false;
  deleted_rows = 0;
}

bool Query_result_delete::send_data(THD *thd, const mem_root_deque<Item *> &) {
  DBUG_TRACE;

  int unique_counter = 0;

  for (TABLE_LIST *tr = unit->first_query_block()->leaf_tables; tr != nullptr;
       tr = tr->next_leaf) {
    const table_map map = tr->map();

    // Check whether this table is being deleted from
    if (!(map & delete_table_map)) continue;

    const bool immediate = map & delete_immediate;

    TABLE *const table = tr->table;

    assert(immediate || table == tables[unique_counter]);

    /*
      If not doing immediate deletion, increment unique_counter and assign
      "tempfile" here, so that it is available when and if it is needed.
    */
    Unique *const tempfile =
        immediate ? nullptr : tempfiles[unique_counter++].get();

    // Check if using outer join and no row found, or row is already deleted
    if (table->has_null_row() || table->has_deleted_row()) continue;

    table->file->position(table->record[0]);

    if (immediate) {
      // Rows from this table can be deleted immediately
      const ha_rows last_deleted = deleted_rows;
      table->set_deleted_row();
      if (DeleteCurrentRowAndProcessTriggers(
              thd, table, Overlaps(map, tables_with_before_triggers),
              Overlaps(map, tables_with_after_triggers), &deleted_rows)) {
        return true;
      }
      if (last_deleted != deleted_rows &&
          !Overlaps(map, transactional_table_map)) {
        thd->get_transaction()->mark_modified_non_trans_table(
            Transaction_ctx::STMT);
      }
    } else {
      // Save deletes in a Unique object, to be carried out later.
      has_buffered_rows = true;
      if (tempfile->unique_add(table->file->ref)) {
        /* purecov: begin inspected */
        return true;
        /* purecov: end */
      }
    }
  }
  return false;
}

void Query_result_delete::send_error(THD *, uint errcode, const char *err) {
  DBUG_TRACE;

  /* First send error what ever it is ... */
  my_message(errcode, err, MYF(0));
}

void Query_result_delete::abort_result_set(THD *thd) {
  DBUG_TRACE;

  /* the error was handled or nothing deleted and no side effects return */
  if (error_handled ||
      (!thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT) &&
       deleted_rows == 0))
    return;

  /*
    If rows from the first table only has been deleted and it is
    transactional, just do rollback.
    The same if all tables are transactional, regardless of where we are.
    In all other cases do attempt deletes ...
  */
  if (!delete_completed && thd->get_transaction()->has_modified_non_trans_table(
                               Transaction_ctx::STMT)) {
    // Execute the recorded do_deletes() and write info into the error log
    non_transactional_delete_aborted = true;
    send_eof(thd);
    assert(error_handled);
    return;
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
}

/**
  Do delete from other tables.

  @return true on error
*/

bool Query_result_delete::do_deletes(THD *thd) {
  DBUG_TRACE;
  assert(!delete_completed);

  delete_completed = true;  // Mark operation as complete
  if (!has_buffered_rows) return false;

  for (uint counter = 0; counter < tables.size(); counter++) {
    TABLE *const table = tables[counter];
    if (tempfiles[counter]->get(table)) return true;

    if (do_table_deletes(thd, table) || thd->killed) {
      return true;
    }
  }
  return false;
}

/**
   Implements the inner loop of nested-loops join within multi-DELETE
   execution.

   @param thd   Thread handle.
   @param table The table from which to delete.

   @return true on error
*/
bool Query_result_delete::do_table_deletes(THD *thd, TABLE *table) {
  DBUG_TRACE;

  /*
    Ignore any rows not found in reference tables as they may already have
    been deleted by foreign key handling
  */
  unique_ptr_destroy_only<RowIterator> iterator =
      init_table_iterator(thd, table, /*ignore_not_found_rows=*/true,
                          /*count_examined_rows=*/false);
  if (iterator == nullptr) return true;

  const uint tableno = table->pos_in_table_list->tableno();
  const bool has_before_triggers =
      IsBitSet(tableno, tables_with_before_triggers);
  const bool has_after_triggers = IsBitSet(tableno, tables_with_after_triggers);

  const bool will_batch = !table->file->start_bulk_delete();
  const ha_rows last_deleted = deleted_rows;

  bool local_error = false;
  while (!thd->killed) {
    const int read_error = iterator->Read();
    if (read_error == 0) {
      if (DeleteCurrentRowAndProcessTriggers(thd, table, has_before_triggers,
                                             has_after_triggers,
                                             &deleted_rows)) {
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
  if (last_deleted != deleted_rows &&
      !IsBitSet(tableno, transactional_table_map)) {
    thd->get_transaction()->mark_modified_non_trans_table(
        Transaction_ctx::STMT);
  }

  return local_error;
}

/**
  Send ok to the client

  The function has to perform all deferred deletes that have been queued up.

  @return false if success, true if error
*/

bool Query_result_delete::send_eof(THD *thd) {
  THD_STAGE_INFO(thd, stage_deleting_from_reference_tables);

  /* Does deletes for the last n - 1 tables, returns 0 if ok */
  bool local_error = do_deletes(thd);

  /* compute a total error to know if something failed */
  local_error = local_error || non_transactional_delete_aborted;

  const THD::killed_state killed_status =
      !local_error ? THD::NOT_KILLED : thd->killed.load();
  /* reset used flags */

  if (!local_error ||
      thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT)) {
    if (mysql_bin_log.is_open()) {
      int errcode = 0;
      if (!local_error)
        thd->clear_error();
      else
        errcode = query_error_code(thd, killed_status == THD::NOT_KILLED);
      thd->thread_specific_used = true;
      if (thd->binlog_query(THD::ROW_QUERY_TYPE, thd->query().str,
                            thd->query().length, transactional_table_map != 0,
                            false, false, errcode) &&
          transactional_table_map == delete_table_map) {
        local_error = true;  // Log write failed: roll back the SQL statement
      }
    }
  }
  if (local_error)
    error_handled = true;  // to force early leave from ::send_error()

  if (!local_error && !thd->is_error()) {
    ::my_ok(thd, deleted_rows);
  }
  return thd->is_error();
}

}  // namespace

bool Sql_cmd_delete::accept(THD *thd, Select_lex_visitor *visitor) {
  return thd->lex->unit->accept(visitor);
}

table_map GetImmediateDeleteTables(const JOIN *join, table_map delete_tables) {
  // The hypergraph optimizer determines the immediate delete tables during
  // planning, not after planning.
  assert(!join->thd->lex->using_hypergraph_optimizer);

  for (TABLE_LIST *tr = join->query_block->leaf_tables; tr != nullptr;
       tr = tr->next_leaf) {
    if (!tr->is_deleted()) continue;

    if (unique_table(tr, join->tables_list, false) != nullptr) {
      /*
        If the table being deleted from is also referenced in the query,
        defer delete so that the delete doesn't interfer with reading of this
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
  table_map candidate_tables = join->const_table_map;  // 1
  if (join->primary_tables > join->const_tables) {
    // Can be called in different stages after the join order has been
    // determined, so look into QEP_TAB or JOIN_TAB depending on which is
    // available in the current stage.
    const TABLE_LIST *first_non_const;
    if (join->qep_tab != nullptr) {
      first_non_const = join->qep_tab[join->const_tables].table_ref;
    } else {
      ASSERT_BEST_REF_IN_JOIN_ORDER(join);
      first_non_const = join->best_ref[join->const_tables]->table_ref;
    }
    candidate_tables |= first_non_const->map();  // 2
  }

  return delete_tables & candidate_tables;
}
