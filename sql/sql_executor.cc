/* Copyright (c) 2000, 2019, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @brief
  Query execution


  @defgroup Query_Executor  Query Executor
  @{
*/

#include "sql/sql_executor.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "field_types.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "map_helpers.h"
#include "mem_root_allocator.h"
#include "memory_debugging.h"
#include "my_alloc.h"
#include "my_bitmap.h"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_loglevel.h"
#include "my_macros.h"
#include "my_pointer_arithmetic.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/psi/psi_base.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "prealloced_array.h"
#include "sql/basic_row_iterators.h"
#include "sql/bka_iterator.h"
#include "sql/composite_iterators.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/enum_query_type.h"
#include "sql/field.h"
#include "sql/filesort.h"  // Filesort
#include "sql/handler.h"
#include "sql/hash_join_iterator.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_sum.h"  // Item_sum
#include "sql/json_dom.h"  // Json_wrapper
#include "sql/key.h"       // key_cmp
#include "sql/key_spec.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"  // stage_executing
#include "sql/nested_join.h"
#include "sql/opt_costmodel.h"
#include "sql/opt_explain_format.h"
#include "sql/opt_range.h"  // QUICK_SELECT_I
#include "sql/opt_trace.h"  // Opt_trace_object
#include "sql/opt_trace_context.h"
#include "sql/parse_tree_nodes.h"  // PT_frame
#include "sql/pfs_batch_mode.h"
#include "sql/protocol.h"
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/query_result.h"   // Query_result
#include "sql/record_buffer.h"  // Record_buffer
#include "sql/records.h"
#include "sql/ref_row_iterators.h"
#include "sql/row_iterator.h"
#include "sql/sort_param.h"
#include "sql/sorting_iterator.h"
#include "sql/sql_base.h"  // fill_record
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_cmd.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_join_buffer.h"  // CACHE_FIELD
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_select.h"
#include "sql/sql_tmp_table.h"  // create_tmp_table
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/table_function.h"
#include "sql/temp_table_param.h"  // Mem_root_vector
#include "sql/thr_malloc.h"
#include "sql/timing_iterator.h"
#include "sql/window.h"
#include "sql/window_lex.h"
#include "sql_string.h"
#include "tables_contained_in.h"
#include "template_utils.h"
#include "thr_lock.h"

using std::max;
using std::min;
using std::string;
using std::unique_ptr;
using std::vector;

static void return_zero_rows(JOIN *join, List<Item> &fields);
static int do_select(JOIN *join);

static enum_nested_loop_state evaluate_join_record(JOIN *join,
                                                   QEP_TAB *qep_tab);
static enum_nested_loop_state evaluate_null_complemented_join_record(
    JOIN *join, QEP_TAB *qep_tab);
static enum_nested_loop_state end_send(JOIN *join, QEP_TAB *qep_tab,
                                       bool end_of_records);
static enum_nested_loop_state end_write(JOIN *join, QEP_TAB *qep_tab,
                                        bool end_of_records);
static enum_nested_loop_state end_write_wf(JOIN *join, QEP_TAB *qep_tab,
                                           bool end_of_records);
static enum_nested_loop_state end_update(JOIN *join, QEP_TAB *qep_tab,
                                         bool end_of_records);

static int read_system(TABLE *table);
static int read_const(TABLE *table, TABLE_REF *ref);
static bool remove_dup_with_compare(THD *thd, TABLE *entry, Field **field,
                                    ulong offset, Item *having);
static bool remove_dup_with_hash_index(THD *thd, TABLE *table,
                                       Field **first_field,
                                       const size_t *field_lengths,
                                       size_t key_length, Item *having);
static int do_sj_reset(SJ_TMP_TABLE *sj_tbl);
static bool alloc_group_fields(JOIN *join, ORDER *group);
static void SetCostOnTableIterator(const Cost_model_server &cost_model,
                                   const POSITION *pos, bool is_after_filter,
                                   RowIterator *iterator);

/**
   Evaluates HAVING condition
   @returns true if TRUE, false if FALSE or NULL
   @note this uses val_int() and relies on the convention that val_int()
   returns 0 when the value is NULL.
*/
static bool having_is_true(Item *h) {
  if (h == nullptr) {
    DBUG_PRINT("info", ("no HAVING"));
    return true;
  }
  bool rc = h->val_int();
  DBUG_PRINT("info", ("HAVING is %d", (int)rc));
  return rc;
}

/// Maximum amount of space (in bytes) to allocate for a Record_buffer.
static constexpr size_t MAX_RECORD_BUFFER_SIZE = 128 * 1024;  // 128KB

string RefToString(const TABLE_REF &ref, const KEY *key, bool include_nulls) {
  string ret;

  const uchar *key_buff = ref.key_buff;

  for (unsigned key_part_idx = 0; key_part_idx < ref.key_parts;
       ++key_part_idx) {
    if (key_part_idx != 0) {
      ret += ", ";
    }
    const Field *field = key->key_part[key_part_idx].field;
    if (field->is_field_for_functional_index()) {
      // Do not print out the column name if the column represents a functional
      // index. Instead, print out the indexed expression.
      ret += ItemToString(field->gcol_info->expr_item);
    } else {
      DBUG_ASSERT(!field->is_hidden_from_user());
      ret += field->field_name;
    }
    ret += "=";
    ret += ItemToString(ref.items[key_part_idx]);

    // If we have ref_or_null access, find out if this keypart is the one that
    // is -or-NULL (there's always only a single one).
    if (include_nulls && key_buff == ref.null_ref_key) {
      ret += " or NULL";
    }
    key_buff += key->key_part[key_part_idx].store_length;
  }
  return ret;
}

/**
  Execute select, executor entry point.

  @todo
    When can we have here thd->net.report_error not zero?

  @note that EXPLAIN may come here (single-row derived table, uncorrelated
    scalar subquery in WHERE clause...).
*/

void JOIN::exec() {
  Opt_trace_context *const trace = &thd->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_exec(trace, "join_execution");
  trace_exec.add_select_number(select_lex->select_number);
  Opt_trace_array trace_steps(trace, "steps");
  List<Item> *columns_list = &fields_list;
  DBUG_TRACE;

  DBUG_ASSERT(select_lex == thd->lex->current_select());

  /*
    Check that we either
    - have no tables, or
    - have tables and have locked them, or
    - called for fake_select_lex, which may have temporary tables which do
      not need locking up front.
  */
  DBUG_ASSERT(!tables || thd->lex->is_query_tables_locked() ||
              select_lex == unit->fake_select_lex);

  THD_STAGE_INFO(thd, stage_executing);
  DEBUG_SYNC(thd, "before_join_exec");

  set_executed();

  if (prepare_result()) return;

  if (m_windows.elements > 0 && !m_windowing_steps) {
    // Initialize state of window functions as end_write_wf() will be shortcut
    for (Window &w : m_windows) {
      w.reset_all_wf_state();
    }
  }

  Query_result *const query_result = select_lex->query_result();

  do_send_rows = unit->select_limit_cnt > 0;

  if (!tables_list &&
      (tables || !select_lex->with_sum_func)) {  // Only test of functions
    /*
      We have to test for 'conds' here as the WHERE may not be constant
      even if we don't have any tables for prepared statements or if
      conds uses something like 'rand()'.

      Don't evaluate the having clause here. return_zero_rows() should
      be called only for cases where there are no matching rows after
      evaluating all conditions except the HAVING clause.
    */
    if (select_lex->cond_value != Item::COND_FALSE &&
        (!where_cond || where_cond->val_int())) {
      if (query_result->send_result_set_metadata(
              thd, *columns_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
        return;

      /*
        If the HAVING clause is either impossible or always true, then
        JOIN::having is set to NULL by optimize_cond.
        In this case JOIN::exec must check for JOIN::having_value, in the
        same way it checks for JOIN::cond_value.
      */
      if (((select_lex->having_value != Item::COND_FALSE) &&
           having_is_true(having_cond)) &&
          should_send_current_row() &&
          query_result->send_data(thd, fields_list))
        error = 1;
      else {
        error = (int)query_result->send_eof(thd);
        send_records = calc_found_rows ? 1 : thd->get_sent_row_count();
      }
      /* Query block (without union) always returns 0 or 1 row */
      thd->current_found_rows = send_records;
    } else {
      return_zero_rows(this, *columns_list);
    }
    return;
  }

  if (zero_result_cause) {
    return_zero_rows(this, *columns_list);
    return;
  }

  /*
    Initialize examined rows here because the values from all join parts
    must be accumulated in examined_row_count. Hence every join
    iteration must count from zero.
  */
  examined_rows = 0;

  /* XXX: When can we have here thd->is_error() not zero? */
  if (thd->is_error()) {
    error = thd->is_error();
    return;
  }

  DBUG_PRINT("info", ("%s", thd->proc_info));
  if (query_result->send_result_set_metadata(
          thd, *fields, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF)) {
    /* purecov: begin inspected */
    error = 1;
    return;
    /* purecov: end */
  }
  error = do_select(this);
  /* Accumulate the counts from all join iterations of all join parts. */
  thd->inc_examined_row_count(examined_rows);
  DBUG_PRINT("counts", ("thd->examined_row_count: %lu",
                        (ulong)thd->get_examined_row_count()));
}

bool JOIN::create_intermediate_table(QEP_TAB *const tab,
                                     List<Item> *tmp_table_fields,
                                     ORDER_with_src &tmp_table_group,
                                     bool save_sum_fields) {
  DBUG_TRACE;
  THD_STAGE_INFO(thd, stage_creating_tmp_table);
  const bool windowing = m_windows.elements > 0;
  /*
    Pushing LIMIT to the temporary table creation is not applicable
    when there is ORDER BY or GROUP BY or aggregate/window functions, because
    in all these cases we need all result rows.
  */
  ha_rows tmp_rows_limit =
      ((order == NULL || skip_sort_order) && !tmp_table_group && !windowing &&
       !select_lex->with_sum_func)
          ? m_select_limit
          : HA_POS_ERROR;

  tab->tmp_table_param = new (thd->mem_root) Temp_table_param(tmp_table_param);
  tab->tmp_table_param->skip_create_table = true;

  bool distinct_arg =
      select_distinct &&
      // GROUP BY is absent or has been done in a previous step
      !group_list &&
      // We can only do DISTINCT in last window's tmp table step
      (!windowing || (tab->tmp_table_param->m_window &&
                      tab->tmp_table_param->m_window->is_last()));

  TABLE *table =
      create_tmp_table(thd, tab->tmp_table_param, *tmp_table_fields,
                       tmp_table_group, distinct_arg, save_sum_fields,
                       select_lex->active_options(), tmp_rows_limit, "");
  if (!table) return true;
  tmp_table_param.using_outer_summary_function =
      tab->tmp_table_param->using_outer_summary_function;

  DBUG_ASSERT(tab->idx() > 0);
  tab[-1].next_select = sub_select_op;
  if (!(tab->op = new (thd->mem_root) QEP_tmp_table(tab))) goto err;

  tab->set_table(table);
  tab->set_temporary_table_deduplicates(distinct_arg ||
                                        tmp_table_group != nullptr);

  /**
    If this is a window's OUT table, any final DISTINCT, ORDER BY will lead to
    windows showing use of tmp table in the final windowing step, so no
    need to signal use of tmp table unless we are here for another tmp table.
  */
  if (!tab->tmp_table_param->m_window) {
    if (table->group)
      explain_flags.set(tmp_table_group.src, ESP_USING_TMPTABLE);
    else if (table->is_distinct || select_distinct)
      explain_flags.set(ESC_DISTINCT, ESP_USING_TMPTABLE);
    else {
      /*
        Try to find a reason for this table, to show in EXPLAIN.
        If there's no GROUP BY, no ORDER BY, no DISTINCT, it must be just a
        result buffer. If there's ORDER BY but there is also windowing
        then ORDER BY happens after windowing, and here we are before
        windowing, so the table is not for ORDER BY either.
      */
      if ((!group_list && (!order || windowing) && !select_distinct) ||
          (select_lex->active_options() &
           (SELECT_BIG_RESULT | OPTION_BUFFER_RESULT)))
        explain_flags.set(ESC_BUFFER_RESULT, ESP_USING_TMPTABLE);
    }
  }
  /* if group or order on first table, sort first */
  if (group_list && simple_group) {
    DBUG_PRINT("info", ("Sorting for group"));

    if (m_ordered_index_usage != ORDERED_INDEX_GROUP_BY &&
        add_sorting_to_table(const_tables, &group_list))
      goto err;

    if (alloc_group_fields(this, group_list)) goto err;
    if (make_sum_func_list(all_fields, fields_list, true)) goto err;
    const bool need_distinct =
        !(tab->quick() && tab->quick()->is_agg_loose_index_scan());
    if (prepare_sum_aggregators(sum_funcs, need_distinct)) goto err;
    if (setup_sum_funcs(thd, sum_funcs)) goto err;
    group_list = NULL;
  } else {
    if (make_sum_func_list(all_fields, fields_list, false)) goto err;
    const bool need_distinct =
        !(tab->quick() && tab->quick()->is_agg_loose_index_scan());
    if (prepare_sum_aggregators(sum_funcs, need_distinct)) goto err;
    if (setup_sum_funcs(thd, sum_funcs)) goto err;

    if (!group_list && !table->is_distinct && order && simple_order &&
        !m_windows_sort) {
      DBUG_PRINT("info", ("Sorting for order"));

      if (m_ordered_index_usage != ORDERED_INDEX_ORDER_BY &&
          add_sorting_to_table(const_tables, &order))
        goto err;
      order = NULL;
    }
  }
  return false;

err:
  if (table != NULL) {
    free_tmp_table(thd, table);
    tab->set_table(NULL);
  }
  return true;
}

/**
  Send all rollup levels higher than the current one to the client.

  @b SAMPLE
    @code
      SELECT a, b, c SUM(b) FROM t1 GROUP BY a,b WITH ROLLUP
  @endcode

  @param idx		Level we are on:
                        - 0 = Total sum level
                        - 1 = First group changed  (a)
                        - 2 = Second group changed (a,b)

  @returns false if success, true if error
*/

bool JOIN::rollup_send_data(uint idx) {
  uint save_slice = current_ref_item_slice;
  for (uint i = send_group_parts; i-- > idx;) {
    // Get references to sum functions in place
    copy_ref_item_slice(ref_items[REF_SLICE_ACTIVE], rollup.ref_item_arrays[i]);
    current_ref_item_slice = -1;  // as we switched to a not-numbered slice
    if (having_is_true(having_cond)) {
      if (send_records < unit->select_limit_cnt && should_send_current_row() &&
          select_lex->query_result()->send_data(thd, rollup.fields_list[i]))
        return true;
      send_records++;
    }
  }
  // Restore ref_items array
  set_ref_item_slice(save_slice);
  return false;
}

/**
  Checks if an item has a ROLLUP NULL which needs to be written to
  temp table.

  @param item         Item for which we need to detect if ROLLUP
                      NULL has to be written.

  @returns false if ROLLUP NULL need not be written for this item.
           true if it has to be written.
*/

bool has_rollup_result(Item *item) {
  if (item->type() == Item::NULL_RESULT_ITEM) return true;

  if (item->type() == Item::FUNC_ITEM) {
    for (uint i = 0; i < ((Item_func *)item)->arg_count; i++) {
      Item *real_item = ((Item_func *)item)->arguments()[i];
      while (real_item->type() == Item::REF_ITEM)
        real_item = *((down_cast<Item_ref *>(real_item))->ref);

      if (real_item->type() == Item::NULL_RESULT_ITEM)
        return true;
      else if (real_item->type() == Item::FUNC_ITEM &&
               has_rollup_result(real_item))
        return true;
    }
  }
  return false;
}

/**
  Write all rollup levels higher than the current one to a temp table.

  @b SAMPLE
    @code
      SELECT a, b, SUM(c) FROM t1 GROUP BY a,b WITH ROLLUP
  @endcode

  @param idx                 Level we are on:
                               - 0 = Total sum level
                               - 1 = First group changed  (a)
                               - 2 = Second group changed (a,b)
  @param qep_tab             temp table

  @returns false if success, true if error
*/

bool JOIN::rollup_write_data(uint idx, QEP_TAB *qep_tab) {
  uint save_slice = current_ref_item_slice;
  for (uint i = send_group_parts; i-- > idx;) {
    // Get references to sum functions in place
    copy_ref_item_slice(ref_items[REF_SLICE_ACTIVE], rollup.ref_item_arrays[i]);
    current_ref_item_slice = -1;  // as we switched to a not-numbered slice
    if (having_is_true(qep_tab->having)) {
      int write_error;
      for (Item &item : rollup.all_fields[i]) {
        /*
          Save the values of rollup expressions in the temporary table.
          Unless it is a literal NULL value, make sure there is actually
          a temporary table field created for it.
        */
        if ((item.type() == Item::NULL_RESULT_ITEM) ||
            (has_rollup_result(&item) && item.get_tmp_table_field() != nullptr))
          item.save_in_field(item.get_result_field(), true);
      }
      copy_sum_funcs(sum_funcs_end[i + 1], sum_funcs_end[i]);
      TABLE *table_arg = qep_tab->table();
      if ((write_error = table_arg->file->ha_write_row(table_arg->record[0]))) {
        if (create_ondisk_from_heap(thd, table_arg, write_error, false, NULL))
          return true;
      }
    }
  }
  set_ref_item_slice(save_slice);  // Restore ref_items array
  return false;
}

void JOIN::optimize_distinct() {
  for (int i = primary_tables - 1; i >= 0; --i) {
    QEP_TAB *last_tab = qep_tab + i;
    if (select_lex->select_list_tables & last_tab->table_ref->map()) break;
    last_tab->not_used_in_distinct = true;
  }

  /* Optimize "select distinct b from t1 order by key_part_1 limit #" */
  if (order && skip_sort_order) {
    /* Should already have been optimized away */
    DBUG_ASSERT(m_ordered_index_usage == ORDERED_INDEX_ORDER_BY);
    if (m_ordered_index_usage == ORDERED_INDEX_ORDER_BY) {
      order = NULL;
    }
  }
}

bool prepare_sum_aggregators(Item_sum **func_ptr, bool need_distinct) {
  Item_sum *func;
  DBUG_TRACE;
  while ((func = *(func_ptr++))) {
    if (func->set_aggregator(need_distinct && func->has_with_distinct()
                                 ? Aggregator::DISTINCT_AGGREGATOR
                                 : Aggregator::SIMPLE_AGGREGATOR))
      return true;
  }
  return false;
}

/******************************************************************************
  Code for calculating functions
******************************************************************************/

/**
  Call @c setup() for all sum functions.

  @param thd           thread handler
  @param func_ptr      sum function list

  @retval
    false  ok
  @retval
    true   error
*/

bool setup_sum_funcs(THD *thd, Item_sum **func_ptr) {
  Item_sum *func;
  DBUG_TRACE;
  while ((func = *(func_ptr++))) {
    if (func->aggregator_setup(thd)) return true;
  }
  return false;
}

void init_tmptable_sum_functions(Item_sum **func_ptr) {
  DBUG_TRACE;
  Item_sum *func;
  while ((func = *(func_ptr++))) func->reset_field();
}

/** Update record 0 in tmp_table from record 1. */

void update_tmptable_sum_func(Item_sum **func_ptr,
                              TABLE *tmp_table MY_ATTRIBUTE((unused))) {
  DBUG_TRACE;
  Item_sum *func;
  while ((func = *(func_ptr++))) func->update_field();
}

/** Copy result of sum functions to record in tmp_table. */

void copy_sum_funcs(Item_sum **func_ptr, Item_sum **end_ptr) {
  DBUG_TRACE;
  for (; func_ptr != end_ptr; func_ptr++) {
    if ((*func_ptr)->get_result_field() != nullptr) {
      (*func_ptr)->save_in_field((*func_ptr)->get_result_field(), true);
    }
  }
}

bool init_sum_functions(Item_sum **func_ptr, Item_sum **end_ptr) {
  for (; func_ptr != end_ptr; func_ptr++) {
    if ((*func_ptr)->reset_and_add()) return true;
  }
  /* If rollup, calculate the upper sum levels */
  for (; *func_ptr; func_ptr++) {
    if ((*func_ptr)->aggregator_add()) return true;
  }
  return false;
}

bool update_sum_func(Item_sum **func_ptr) {
  DBUG_TRACE;
  Item_sum *func;
  for (; (func = *func_ptr); func_ptr++)
    if (func->aggregator_add()) return true;
  return false;
}

/**
  Copy result of functions to record in tmp_table.

  Uses the thread pointer to check for errors in
  some of the val_xxx() methods called by the
  save_in_result_field() function.
  TODO: make the Item::val_xxx() return error code

  @param param     Copy functions of tmp table specified by param
  @param thd       pointer to the current thread for error checking
  @param type      type of function Items that need to be copied (used
                   w.r.t windowing functions).
  @retval
    false if OK
  @retval
    true on error
*/
bool copy_funcs(Temp_table_param *param, const THD *thd, Copy_func_type type) {
  DBUG_TRACE;
  if (!param->items_to_copy->size()) return false;

  Func_ptr_array *func_ptr = param->items_to_copy;
  uint end = func_ptr->size();
  for (uint i = 0; i < end; i++) {
    Func_ptr &func = func_ptr->at(i);
    Item *item = func.func();
    bool do_copy = false;
    switch (type) {
      case CFT_ALL:
        do_copy = true;
        break;
      case CFT_WF_FRAMING:
        do_copy = (item->m_is_window_function &&
                   down_cast<Item_sum *>(item)->framing());
        break;
      case CFT_WF_NON_FRAMING:
        do_copy = (item->m_is_window_function &&
                   !down_cast<Item_sum *>(item)->framing() &&
                   !down_cast<Item_sum *>(item)->needs_card());
        break;
      case CFT_WF_NEEDS_CARD:
        do_copy = (item->m_is_window_function &&
                   down_cast<Item_sum *>(item)->needs_card());
        break;
      case CFT_WF_USES_ONLY_ONE_ROW:
        do_copy = (item->m_is_window_function &&
                   down_cast<Item_sum *>(item)->uses_only_one_row());
        break;
      case CFT_HAS_NO_WF:
        do_copy = !item->m_is_window_function && !item->has_wf();
        break;
      case CFT_HAS_WF:
        do_copy = !item->m_is_window_function && item->has_wf();
        break;
      case CFT_WF:
        do_copy = item->m_is_window_function;
        break;
      case CFT_DEPENDING_ON_AGGREGATE:
        do_copy =
            item->has_aggregation() && item->type() != Item::SUM_FUNC_ITEM;
        break;
    }

    if (do_copy) {
      if (func.override_result_field() == nullptr) {
        item->save_in_field(item->get_result_field(),
                            /*no_conversions=*/true);
      } else {
        item->save_in_field(func.override_result_field(),
                            /*no_conversions=*/true);
      }
      /*
        Need to check the THD error state because Item::val_xxx() don't
        return error code, but can generate errors
        TODO: change it for a real status check when Item::val_xxx()
        are extended to return status code.
      */
      if (thd->is_error()) return true;
    }
  }
  return false;
}

/*
  end_select-compatible function that writes the record into a sjm temptable

  SYNOPSIS
    end_sj_materialize()
      join            The join
      join_tab        Last join table
      end_of_records  false <=> This call is made to pass another record
                                combination
                      true  <=> EOF (no action)

  DESCRIPTION
    This function is used by semi-join materialization to capture suquery's
    resultset and write it into the temptable (that is, materialize it).

  NOTE
    This function is used only for semi-join materialization. Non-semijoin
    materialization uses different mechanism.

  RETURN
    NESTED_LOOP_OK
    NESTED_LOOP_ERROR
*/

static enum_nested_loop_state end_sj_materialize(JOIN *join, QEP_TAB *qep_tab,
                                                 bool end_of_records) {
  int error;
  THD *thd = join->thd;
  Semijoin_mat_exec *sjm = qep_tab[-1].sj_mat_exec();
  DBUG_TRACE;
  if (!end_of_records) {
    TABLE *table = sjm->table;

    for (Item &item : sjm->sj_nest->nested_join->sj_inner_exprs) {
      if (item.is_null()) return NESTED_LOOP_OK;
    }
    fill_record(thd, table, table->visible_field_ptr(),
                sjm->sj_nest->nested_join->sj_inner_exprs, NULL, NULL, false);
    if (thd->is_error()) return NESTED_LOOP_ERROR; /* purecov: inspected */
    if (!check_unique_constraint(table)) return NESTED_LOOP_OK;
    if ((error = table->file->ha_write_row(table->record[0]))) {
      /* create_ondisk_from_heap will generate error if needed */
      if (!table->file->is_ignorable_error(error)) {
        if (create_ondisk_from_heap(thd, table, error, true, NULL))
          return NESTED_LOOP_ERROR; /* purecov: inspected */
        /* Initialize the index, since create_ondisk_from_heap does
           not replicate the earlier index initialization */
        if (table->hash_field) table->file->ha_index_init(0, false);
      }
    }
  }
  return NESTED_LOOP_OK;
}

/**
  Check appearance of new constant items in multiple equalities
  of a condition after reading a constant table.

    The function retrieves the cond condition and for each encountered
    multiple equality checks whether new constants have appeared after
    reading the constant (single row) table tab. If so it adjusts
    the multiple equality appropriately.

  @param thd        thread handler
  @param cond       condition whose multiple equalities are to be checked
  @param tab        constant table that has been read
*/

static bool update_const_equal_items(THD *thd, Item *cond, JOIN_TAB *tab) {
  if (!(cond->used_tables() & tab->table_ref->map())) return false;

  if (cond->type() == Item::COND_ITEM) {
    for (Item &item : *(down_cast<Item_cond *>(cond))->argument_list()) {
      if (update_const_equal_items(thd, &item, tab)) return true;
    }
  } else if (cond->type() == Item::FUNC_ITEM &&
             down_cast<Item_func *>(cond)->functype() ==
                 Item_func::MULT_EQUAL_FUNC) {
    Item_equal *item_equal = (Item_equal *)cond;
    bool contained_const = item_equal->get_const() != NULL;
    if (item_equal->update_const(thd)) return true;
    if (!contained_const && item_equal->get_const()) {
      /* Update keys for range analysis */
      Item_equal_iterator it(*item_equal);
      Item_field *item_field;
      while ((item_field = it++)) {
        const Field *field = item_field->field;
        JOIN_TAB *stat = field->table->reginfo.join_tab;
        Key_map possible_keys = field->key_start;
        possible_keys.intersect(field->table->keys_in_use_for_query);
        stat[0].const_keys.merge(possible_keys);
        stat[0].keys().merge(possible_keys);

        /*
          For each field in the multiple equality (for which we know that it
          is a constant) we have to find its corresponding key part, and set
          that key part in const_key_parts.
        */
        if (!possible_keys.is_clear_all()) {
          TABLE *const table = field->table;
          for (Key_use *use = stat->keyuse();
               use && use->table_ref == item_field->table_ref; use++) {
            if (possible_keys.is_set(use->key) &&
                table->key_info[use->key].key_part[use->keypart].field == field)
              table->const_key_parts[use->key] |= use->keypart_map;
          }
        }
      }
    }
  }
  return false;
}

/**
  For some reason, e.g. due to an impossible WHERE clause, the tables cannot
  possibly contain any rows that will be in the result. This function
  is used to return with a result based on no matching rows (i.e., an
  empty result or one row with aggregates calculated without using
  rows in the case of implicit grouping) before the execution of
  nested loop join.

  This function may evaluate the HAVING clause and is only meant for
  result sets that are empty due to an impossible HAVING clause. Do
  not use it if HAVING has already been evaluated.

  @param join    The join that does not produce a row
  @param fields  Fields in result
*/
static void return_zero_rows(JOIN *join, List<Item> &fields) {
  DBUG_TRACE;

  join->join_free();

  /* Update results for FOUND_ROWS */
  if (!join->send_row_on_empty_set()) {
    join->thd->current_found_rows = 0;
  }

  SELECT_LEX *const select = join->select_lex;
  THD *thd = join->thd;

  if (!(select->query_result()->send_result_set_metadata(
          thd, fields, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))) {
    bool send_error = false;
    if (join->send_row_on_empty_set()) {
      // Mark tables as containing only NULL values
      for (TABLE_LIST *table = select->leaf_tables; table;
           table = table->next_leaf)
        table->table->set_null_row();

      // Calculate aggregate functions for no rows

      /*
        Must notify all fields that there are no rows (not only those
        that will be returned) because join->having may refer to
        fields that are not part of the result columns.
       */
      for (Item &item : join->all_fields) {
        item.no_rows_in_result();
      }

      if (having_is_true(join->having_cond) && join->should_send_current_row())
        send_error = select->query_result()->send_data(thd, fields);
    }
    if (!send_error) select->query_result()->send_eof(thd);  // Should be safe
  }
}

/**
  @brief Setup write_func of QEP_tmp_table object

  @param tab QEP_TAB of a tmp table
  @param trace Opt_trace_object to add to
  @details
  Function sets up write_func according to how QEP_tmp_table object that
  is attached to the given join_tab will be used in the query.
*/

void setup_tmptable_write_func(QEP_TAB *tab, Opt_trace_object *trace) {
  DBUG_TRACE;
  JOIN *join = tab->join();
  TABLE *table = tab->table();
  QEP_tmp_table *op = (QEP_tmp_table *)tab->op;
  Temp_table_param *const tmp_tbl = tab->tmp_table_param;
  uint phase = tab->ref_item_slice;
  const char *description = nullptr;
  DBUG_ASSERT(table && op);

  if (table->group && tmp_tbl->sum_func_count &&
      !tmp_tbl->precomputed_group_by) {
    /*
      Note for MyISAM tmp tables: if uniques is true keys won't be
      created.
    */
    DBUG_ASSERT(phase < REF_SLICE_WIN_1);
    if (table->s->keys) {
      description = "continuously_update_group_row";
      op->set_write_func(end_update);
    }
  } else if (join->streaming_aggregation && !tmp_tbl->precomputed_group_by) {
    DBUG_ASSERT(phase < REF_SLICE_WIN_1);
    description = "write_group_row_when_complete";
    DBUG_PRINT("info", ("Using end_write_group"));
    op->set_write_func(end_write_group);
  } else {
    description = "write_all_rows";
    op->set_write_func(phase >= REF_SLICE_WIN_1 ? end_write_wf : end_write);
    if (tmp_tbl->precomputed_group_by) {
      Item_sum **func_ptr = join->sum_funcs;
      Item_sum *func;
      while ((func = *(func_ptr++))) {
        tmp_tbl->items_to_copy->push_back(Func_ptr(func));
      }
    }
  }
  if (description) trace->add_alnum("write_method", description);
}

/**
  @details
  Rows produced by a join sweep may end up in a temporary table or be sent
  to a client. Setup the function of the nested loop join algorithm which
  handles final fully constructed and matched records.

  @return
    end_select function to use. This function can't fail.
*/
Next_select_func JOIN::get_end_select_func() {
  DBUG_TRACE;
  /*
     Choose method for presenting result to user. Use end_send_group
     if the query requires grouping (has a GROUP BY clause and/or one or
     more aggregate functions). Use end_send if the query should not
     be grouped.
   */
  if (streaming_aggregation && !tmp_table_param.precomputed_group_by) {
    DBUG_PRINT("info", ("Using end_send_group"));
    return end_send_group;
  }
  DBUG_PRINT("info", ("Using end_send"));
  return end_send;
}

/**
  Find out how many bytes it takes to store the smallest prefix which
  covers all the columns that will be read from a table.

  @param qep_tab the table to read
  @return the size of the smallest prefix that covers all records to be
          read from the table
*/
static size_t record_prefix_size(const QEP_TAB *qep_tab) {
  const TABLE *table = qep_tab->table();

  /*
    Find the end of the last column that is read, or the beginning of
    the record if no column is read.

    We want the column that is physically last in table->record[0],
    which is not necessarily the column that is last in table->field.
    For example, virtual columns come at the end of the record, even
    if they are not at the end of table->field. This means we need to
    inspect all the columns in the read set and take the one with the
    highest end pointer.
  */
  uchar *prefix_end = table->record[0];  // beginning of record
  for (auto f = table->field, end = table->field + table->s->fields; f < end;
       ++f) {
    if (bitmap_is_set(table->read_set, (*f)->field_index))
      prefix_end = std::max(prefix_end, (*f)->ptr + (*f)->pack_length());
  }

  /*
    If this is an index merge, the primary key columns may be required
    for positioning in a later stage, even though they are not in the
    read_set here. Allocate space for them in case they are needed.
    Also allocate space for them for dynamic ranges, because they can
    switch to index merge for a subsequent scan.
  */
  if ((qep_tab->type() == JT_INDEX_MERGE || qep_tab->dynamic_range()) &&
      !table->s->is_missing_primary_key() &&
      (table->file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION)) {
    const KEY &key = table->key_info[table->s->primary_key];
    for (auto kp = key.key_part, end = kp + key.user_defined_key_parts;
         kp < end; ++kp) {
      const Field *f = table->field[kp->fieldnr - 1];
      /*
        If a key column comes after all the columns in the read set,
        extend the prefix to include the key column.
      */
      prefix_end = std::max(prefix_end, f->ptr + f->pack_length());
    }
  }

  return prefix_end - table->record[0];
}

/**
  Allocate a data buffer that the storage engine can use for fetching
  batches of records.

  A buffer is only allocated if ha_is_record_buffer_wanted() returns true
  for the handler, and the scan in question is of a kind that could be
  expected to benefit from fetching records in batches.

  @param tab the table to read
  @retval true if an error occurred when allocating the buffer
  @retval false if a buffer was successfully allocated, or if a buffer
  was not attempted allocated
*/
bool set_record_buffer(const QEP_TAB *tab) {
  if (tab == nullptr) return false;

  TABLE *const table = tab->table();

  DBUG_ASSERT(table->file->inited);
  DBUG_ASSERT(table->file->ha_get_record_buffer() == nullptr);

  // Skip temporary tables.
  if (tab->position() == nullptr) return false;

  // Don't allocate a buffer for loose index scan.
  if (tab->quick_optim() && tab->quick_optim()->is_loose_index_scan())
    return false;

  // Only create a buffer if the storage engine wants it.
  ha_rows max_rows = 0;
  if (!table->file->ha_is_record_buffer_wanted(&max_rows) || max_rows == 0)
    return false;

  // If we already have a buffer, reuse it.
  if (table->m_record_buffer.max_records() > 0) {
    /*
      Assume that the existing buffer has the shape we want. That is, the
      record size shouldn't change for a table during execution.
    */
    DBUG_ASSERT(table->m_record_buffer.record_size() ==
                record_prefix_size(tab));
    table->m_record_buffer.reset();
    table->file->ha_set_record_buffer(&table->m_record_buffer);
    return false;
  }

  // How many rows do we expect to fetch?
  double rows_to_fetch = tab->position()->rows_fetched;

  /*
    If this is the outer table of a join and there is a limit defined
    on the query block, adjust the buffer size accordingly.
  */
  const JOIN *const join = tab->join();
  if (tab->idx() == 0 && join->m_select_limit != HA_POS_ERROR) {
    /*
      Estimated number of rows returned by the join per qualifying row
      in the outer table.
    */
    double fanout = 1.0;
    for (uint i = 1; i < join->primary_tables; i++) {
      const auto p = join->qep_tab[i].position();
      fanout *= p->rows_fetched * p->filter_effect;
    }

    /*
      The number of qualifying rows to read from the outer table in
      order to reach the limit is limit / fanout. Divide by
      filter_effect to get the total number of qualifying and
      non-qualifying rows to fetch to reach the limit.
    */
    rows_to_fetch = std::min(rows_to_fetch, join->m_select_limit / fanout /
                                                tab->position()->filter_effect);
  }

  ha_rows rows_in_buffer = static_cast<ha_rows>(std::ceil(rows_to_fetch));

  // No need for a multi-row buffer if we don't expect multiple rows.
  if (rows_in_buffer <= 1) return false;

  /*
    How much space do we need to allocate for each record? Enough to
    hold all columns from the beginning and up to the last one in the
    read set. We don't need to allocate space for unread columns at
    the end of the record.
  */
  const size_t record_size = record_prefix_size(tab);

  // Do not allocate a buffer whose total size exceeds MAX_RECORD_BUFFER_SIZE.
  if (record_size > 0)
    rows_in_buffer =
        std::min<ha_rows>(MAX_RECORD_BUFFER_SIZE / record_size, rows_in_buffer);

  // Do not allocate space for more rows than the handler asked for.
  rows_in_buffer = std::min(rows_in_buffer, max_rows);

  const auto bufsize = Record_buffer::buffer_size(rows_in_buffer, record_size);
  const auto ptr = static_cast<uchar *>(table->in_use->alloc(bufsize));
  if (ptr == nullptr) return true; /* purecov: inspected */

  table->m_record_buffer = Record_buffer{rows_in_buffer, record_size, ptr};
  table->file->ha_set_record_buffer(&table->m_record_buffer);
  return false;
}

/**
  Split AND conditions into their constituent parts, recursively.
  Conditions that are not AND conditions are appended unchanged onto
  condition_parts. E.g. if you have ((a AND b) AND c), condition_parts
  will contain [a, b, c], plus whatever it contained before the call.
 */
static void ExtractConditions(Item *condition,
                              vector<Item *> *condition_parts) {
  if (condition == nullptr) {
    return;
  }
  if (condition->type() != Item::COND_ITEM ||
      down_cast<Item_cond *>(condition)->functype() !=
          Item_bool_func2::COND_AND_FUNC) {
    condition_parts->push_back(condition);
    return;
  }

  Item_cond_and *and_condition = down_cast<Item_cond_and *>(condition);
  for (Item &item : *and_condition->argument_list()) {
    ExtractConditions(&item, condition_parts);
  }
}

/**
  Return a new iterator that wraps "iterator" and that tests all of the given
  conditions (if any), ANDed together. If there are no conditions, just return
  the given iterator back.
 */
unique_ptr_destroy_only<RowIterator> PossiblyAttachFilterIterator(
    unique_ptr_destroy_only<RowIterator> iterator,
    const vector<Item *> &conditions, THD *thd) {
  if (conditions.empty()) {
    return iterator;
  }

  Item *condition = nullptr;
  if (conditions.size() == 1) {
    condition = conditions[0];
  } else {
    List<Item> items;
    for (Item *cond : conditions) {
      items.push_back(cond);
    }
    condition = new Item_cond_and(items);
    condition->quick_fix_field();
    condition->update_used_tables();
    condition->apply_is_true();
  }

  RowIterator *child_iterator = iterator.get();
  unique_ptr_destroy_only<RowIterator> filter_iterator =
      NewIterator<FilterIterator>(thd, move(iterator), condition);

  // Copy costs (we don't care about filter_effect here, even though we
  // should).
  filter_iterator->set_expected_rows(child_iterator->expected_rows());
  filter_iterator->set_estimated_cost(child_iterator->estimated_cost());

  return filter_iterator;
}

unique_ptr_destroy_only<RowIterator> CreateNestedLoopIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> left_iterator,
    unique_ptr_destroy_only<RowIterator> right_iterator, JoinType join_type,
    bool pfs_batch_mode) {
  if (join_type == JoinType::ANTI || join_type == JoinType::SEMI) {
    // This does not make sense as an optimization for anti- or semijoins.
    pfs_batch_mode = false;
  }

  return NewIterator<NestedLoopIterator>(thd, move(left_iterator),
                                         move(right_iterator), join_type,
                                         pfs_batch_mode);
}

static unique_ptr_destroy_only<RowIterator> CreateInvalidatorIterator(
    THD *thd, QEP_TAB *qep_tab, unique_ptr_destroy_only<RowIterator> iterator) {
  RowIterator *child_iterator = iterator.get();

  unique_ptr_destroy_only<RowIterator> invalidator =
      NewIterator<CacheInvalidatorIterator>(thd, move(iterator),
                                            qep_tab->table()->alias);

  // Copy costs.
  invalidator->set_expected_rows(child_iterator->expected_rows());
  invalidator->set_estimated_cost(child_iterator->estimated_cost());

  table_map deps = qep_tab->lateral_derived_tables_depend_on_me;
  for (QEP_TAB **tab2 = qep_tab->join()->map2qep_tab; deps;
       tab2++, deps >>= 1) {
    if (!(deps & 1)) continue;
    if ((*tab2)->invalidators == nullptr) {
      (*tab2)->invalidators = new (thd->mem_root)
          Mem_root_array<const CacheInvalidatorIterator *>(thd->mem_root);
    }
    (*tab2)->invalidators->push_back(
        down_cast<CacheInvalidatorIterator *>(invalidator->real_iterator()));
  }
  return invalidator;
}

static unique_ptr_destroy_only<RowIterator> PossiblyAttachFilterIterator(
    unique_ptr_destroy_only<RowIterator> iterator,
    const vector<PendingCondition> &conditions, THD *thd) {
  vector<Item *> stripped_conditions;
  for (const PendingCondition &cond : conditions) {
    stripped_conditions.push_back(cond.cond);
  }
  return PossiblyAttachFilterIterator(move(iterator), stripped_conditions, thd);
}

static Item_func_trig_cond *GetTriggerCondOrNull(Item *item) {
  if (item->type() == Item::FUNC_ITEM &&
      down_cast<Item_func *>(item)->functype() ==
          Item_bool_func2::TRIG_COND_FUNC) {
    return down_cast<Item_func_trig_cond *>(item);
  } else {
    return nullptr;
  }
}

enum CallingContext {
  TOP_LEVEL,
  DIRECTLY_UNDER_SEMIJOIN,
  DIRECTLY_UNDER_OUTER_JOIN,
  DIRECTLY_UNDER_WEEDOUT
};

/**
  For historical reasons, derived table materialization and temporary
  table materialization didn't specify the fields to materialize in the
  same way. Temporary table materialization used copy_fields() and
  copy_funcs() (also reused for aggregation; see the comments on
  AggregateIterator for the relation between aggregations and temporary
  tables) to get the data into the Field pointers of the temporary table
  to be written, storing the lists in copy_fields and items_to_copy.

  However, derived table materialization used JOIN::fields (which is a
  set of Item, not Field!) for the same purpose, calling fill_record()
  (which originally was meant for INSERT and UPDATE) instead. Thus, we
  have to rewrite one to the other, so that we can have only one
  MaterializeIterator. We choose to rewrite JOIN::fields to
  copy_fields/items_to_copy.

  TODO: The optimizer should output just one kind of structure directly.
 */
void ConvertItemsToCopy(List<Item> *items, Field **fields,
                        Temp_table_param *param, JOIN *join) {
  DBUG_ASSERT(param->items_to_copy == nullptr);

  const bool replaced_items_for_rollup =
      (join != nullptr && join->replaced_items_for_rollup);

  // All fields are to be copied.
  Func_ptr_array *copy_func =
      new (current_thd->mem_root) Func_ptr_array(current_thd->mem_root);
  Field **field_ptr = fields;
  for (Item &item : *items) {
    Item *real_item = item.real_item();
    if (real_item->type() == Item::FIELD_ITEM) {
      Field *from_field = (pointer_cast<Item_field *>(real_item))->field;
      Field *to_field = *field_ptr;
      param->copy_fields.emplace_back(to_field, from_field, /*save=*/true);

      // If any of the Item_null_result items are set to save in this field,
      // forward them to the new field instead. See below for the result fields
      // for the other items.
      if (replaced_items_for_rollup) {
        for (size_t rollup_level = 0; rollup_level < join->send_group_parts;
             ++rollup_level) {
          for (Item &item_r : join->rollup.fields_list[rollup_level]) {
            if (item_r.type() == Item::NULL_RESULT_ITEM &&
                item_r.get_result_field() == from_field) {
              item_r.set_result_field(to_field);
            }
          }
        }
      }
    } else if (item.real_item()->is_result_field()) {
      Field *from_field = item.real_item()->get_result_field();
      Field *to_field = *field_ptr;
      item.set_result_field(to_field);
      copy_func->push_back(Func_ptr(&item));

      // Similarly to above, set the right result field for any aggregates
      // that we might output as part of rollup.
      if (replaced_items_for_rollup && &item != real_item) {
        for (Item_sum **func_ptr = join->sum_funcs;
             func_ptr != join->sum_funcs_end[join->send_group_parts];
             ++func_ptr) {
          if ((*func_ptr)->get_result_field() == from_field) {
            (*func_ptr)->set_result_field(to_field);
          }
        }
      }
    } else {
      Func_ptr ptr(&item);
      ptr.set_override_result_field(*field_ptr);
      copy_func->push_back(ptr);
    }
    ++field_ptr;
  }
  param->items_to_copy = copy_func;

  if (replaced_items_for_rollup) {
    // Patch up the rollup items so that they save in the same field as
    // the ref would. This is required because we call save_in_result_field()
    // directly on each field in the rollup field list
    // (in AggregateIterator::Read), not on the Item_ref in join->fields.
    for (size_t rollup_level = 0; rollup_level < join->send_group_parts;
         ++rollup_level) {
      List_STL_Iterator<Item> item_it = join->fields->begin();
      for (Item &item : join->rollup.fields_list[rollup_level]) {
        // For cases where we need an Item_null_result, the field in
        // join->fields often does not have the right result field set.
        // However, the Item_null_result field does after we patched it
        // up earlier in the function.
        if (item.type() != Item::NULL_RESULT_ITEM) {
          item.set_result_field(item_it->get_result_field());
        }
        ++item_it;
      }
    }
  }
}

/** Similar to PendingCondition, but for cache invalidator iterators. */
struct PendingInvalidator {
  /**
    The table whose every (post-join) row invalidates one or more derived
    lateral tables.
   */
  QEP_TAB *qep_tab;
  int table_index_to_attach_to;  // -1 means “on the last possible outer join”.
};

/*
  There are three kinds of conditions stored into a table's QEP_TAB object:

  1. Join conditions (where not optimized into EQ_REF accesses or similar).
     These are attached as a condition on the rightmost table of the join;
     if it's an outer join, they are wrapped in a “not_null_compl”
     condition, to mark that they should not be applied to the NULL values
     synthesized when no row is found. These can be kept on the table, and
     we don't really need the not_null_compl wrapper as long as we don't
     move the condition up above the join (which we don't).

  2. WHERE predicates referring to the table, and possibly also one or more
     earlier tables in the join. These should normally be kept on the table,
     so we can discard rows as early as possible (but see next point).
     We should test these after the join conditions, though, as they may
     have side effects. Also note that these may be pushed below sort
     operations for efficiency -- in fact, they already have, so we should
     not try to re-apply them.

  3. Predicates like in #2 that are on the inner (right) side of a
     left join. These conditions must be moved _above_ the join, as they
     should also be tested for NULL-complemented rows the join may generate.
     E.g., for t1 LEFT JOIN t2 WHERE t1.x + t2.x > 3, the condition will be
     attached to t2's QEP_TAB, but needs to be attached above the join, or
     it would erroneously keep rows wherever t2 did not produce a
     (real) row. Such conditions are marked with a “found” trigger (in the
     old execution engine, which tested qep_tab->condition() both before and
     after the join, it would need to be exempt from the first test).

  4. Predicates that are #1 _and_ #3. These can happen with more complicated
     outer joins; e.g., with t1 LEFT JOIN ( t2 LEFT JOIN t3 ON <x> ) ON <y>,
     the <x> join condition (posted on t3) should be above one join but
     below the other.

  TODO: The optimizer should distinguish between before-join and
  after-join conditions to begin with, instead of us having to untangle
  it here.
 */
void SplitConditions(Item *condition, vector<Item *> *predicates_below_join,
                     vector<PendingCondition> *predicates_above_join) {
  vector<Item *> condition_parts;
  ExtractConditions(condition, &condition_parts);
  for (Item *item : condition_parts) {
    Item_func_trig_cond *trig_cond = GetTriggerCondOrNull(item);
    if (trig_cond != nullptr) {
      Item *inner_cond = trig_cond->arguments()[0];
      if (trig_cond->get_trig_type() == Item_func_trig_cond::FOUND_MATCH) {
        // A WHERE predicate on the table that needs to be pushed up above the
        // join (case #3 above). Push it up to above the last outer join.
        predicates_above_join->push_back(PendingCondition{inner_cond, -1});
      } else if (trig_cond->get_trig_type() ==
                 Item_func_trig_cond::IS_NOT_NULL_COMPL) {
        // It's a join condition, so it should nominally go directly onto the
        // table. If it _also_ has a FOUND_MATCH predicate, we are dealing
        // with case #4 above, and need to push it up to exactly the right
        // spot.
        //
        // There is a special exception here for anti-joins; see the code under
        // qep_tab->table()->reginfo.not_exists_optimize in ConnectJoins().
        Item_func_trig_cond *inner_trig_cond = GetTriggerCondOrNull(inner_cond);
        if (inner_trig_cond != nullptr) {
          Item *inner_inner_cond = inner_trig_cond->arguments()[0];
          predicates_above_join->push_back(
              PendingCondition{inner_inner_cond, inner_trig_cond->idx()});
        } else {
          predicates_below_join->push_back(inner_cond);
        }
      } else {
        predicates_below_join->push_back(item);
      }
    } else {
      predicates_below_join->push_back(item);
    }
  }
}

/**
  For a given duplicate weedout operation, figure out which tables are supposed
  to be deduplicated by it, and add those to unhandled_duplicates. (SJ_TMP_TABLE
  contains the deduplication key, which is exactly the complement of the tables
  to be deduplicated.)
 */
static void MarkUnhandledDuplicates(SJ_TMP_TABLE *weedout,
                                    plan_idx weedout_start,
                                    plan_idx weedout_end,
                                    qep_tab_map *unhandled_duplicates) {
  DBUG_ASSERT(weedout_start >= 0);
  DBUG_ASSERT(weedout_end >= 0);

  qep_tab_map weedout_range = TablesBetween(weedout_start, weedout_end);
  if (weedout->is_confluent) {
    // Confluent weedout doesn't have tabs or tabs_end set; it just implicitly
    // says none of the tables are allowed to produce duplicates.
  } else {
    // Remove all tables that are part of the key.
    for (SJ_TMP_TABLE_TAB *tab = weedout->tabs; tab != weedout->tabs_end;
         ++tab) {
      weedout_range &= ~tab->qep_tab->idx_map();
    }
  }
  *unhandled_duplicates |= weedout_range;
}

static unique_ptr_destroy_only<RowIterator> CreateWeedoutIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> iterator,
    SJ_TMP_TABLE *weedout_table) {
  if (weedout_table->is_confluent) {
    // A “confluent” weedout is one that deduplicates on all the
    // fields. If so, we can drop the complexity of the WeedoutIterator
    // and simply insert a LIMIT 1.
    return NewIterator<LimitOffsetIterator>(
        thd, move(iterator), /*limit=*/1, /*offset=*/0,
        /*count_all_rows=*/false, /*skipped_rows=*/nullptr);
  } else {
    return NewIterator<WeedoutIterator>(thd, move(iterator), weedout_table);
  }
}

static unique_ptr_destroy_only<RowIterator> CreateWeedoutIteratorForTables(
    THD *thd, const qep_tab_map tables_to_deduplicate, QEP_TAB *qep_tabs,
    uint primary_tables, unique_ptr_destroy_only<RowIterator> iterator) {
  Prealloced_array<SJ_TMP_TABLE_TAB, MAX_TABLES> sj_tabs(PSI_NOT_INSTRUMENTED);
  for (uint i = 0; i < primary_tables; ++i) {
    if (!ContainsTable(tables_to_deduplicate, i)) {
      SJ_TMP_TABLE_TAB sj_tab;
      sj_tab.qep_tab = &qep_tabs[i];
      sj_tabs.push_back(sj_tab);

      // See JOIN::add_sorting_to_table() for rationale.
      Filesort *filesort = qep_tabs[i].filesort;
      if (filesort != nullptr) {
        DBUG_ASSERT(filesort->m_sort_param.m_addon_fields_status ==
                    Addon_fields_status::unknown_status);
        filesort->m_force_sort_positions = true;
      }
    }
  }

  JOIN *join = qep_tabs[0].join();
  SJ_TMP_TABLE *sjtbl =
      create_sj_tmp_table(thd, join, &sj_tabs[0], &sj_tabs[0] + sj_tabs.size());
  return CreateWeedoutIterator(thd, move(iterator), sjtbl);
}

enum class Substructure { NONE, OUTER_JOIN, SEMIJOIN, WEEDOUT };

/**
  Given a range of tables (where we assume that we've already handled
  first_idx..(this_idx-1) as inner joins), figure out whether this is a
  semijoin, an outer join or a weedout. In general, the outermost structure
  wins; if we are in one of the rare cases where there are e.g. coincident
  outer- and semijoins, we do various forms of conflict resolution:

   - Unhandled weedouts will add elements to unhandled_duplicates
     (to be handled at the top level of the query).
   - Unhandled semijoins will either:
     * Set add_limit_1 to true, which means a LIMIT 1 iterator should
       be added, or
     * Add elements to unhandled_duplicates in situations that cannot
       be solved by a simple one-table, one-row LIMIT.

  If not returning NONE, substructure_end will also be filled with where this
  sub-join ends (exclusive).
 */
static Substructure FindSubstructure(
    QEP_TAB *qep_tabs, const plan_idx first_idx, const plan_idx this_idx,
    const plan_idx last_idx, CallingContext calling_context, bool *add_limit_1,
    plan_idx *substructure_end, qep_tab_map *unhandled_duplicates) {
  QEP_TAB *qep_tab = &qep_tabs[this_idx];
  bool is_outer_join =
      qep_tab->last_inner() != NO_PLAN_IDX && qep_tab->last_inner() < last_idx;
  plan_idx outer_join_end =
      qep_tab->last_inner() + 1;  // Only valid if is_outer_join.

  // See if this table marks the end of the left side of a semijoin.
  bool is_semijoin = false;
  plan_idx semijoin_end = NO_PLAN_IDX;
  for (plan_idx j = this_idx; j < last_idx; ++j) {
    if (qep_tabs[j].firstmatch_return == this_idx - 1) {
      is_semijoin = true;
      semijoin_end = j + 1;
      break;
    }
  }

  // Outer joins (or semijoins) wrapping a weedout is tricky,
  // especially in edge cases. If we have an outer join wrapping
  // a weedout, the outer join needs to be processed first.
  // But the weedout wins if it's strictly larger than the outer join.
  // However, a problem occurs if the weedout wraps two consecutive
  // outer joins (which can happen if the join optimizer interleaves
  // tables from different weedouts and needs to combine them into
  // one larger weedout). E.g., consider a join order such as
  //
  //   a LEFT JOIN (b,c) LEFT JOIN (d,e)
  //
  // where there is _also_ a weedout wrapping all four tables [b,e].
  // (Presumably, there were originally two weedouts b+e and c+d,
  // but due to reordering, they were combined into one.)
  // In this case, we have a non-hierarchical situation since the
  // (a,(b,c)) join only partially overlaps with the [b,e] weedout.
  //
  // We solve these non-hierarchical cases by punting them upwards;
  // we signal that they are simply not done by adding them to
  // unhandled_duplicates, and then drop the weedout. The top level
  // will then add a final weedout after all joins. In some cases,
  // it is possible to push the weedout further down than this,
  // but these cases are so marginal that it's not worth it.

  // See if this table starts a weedout operation.
  bool is_weedout = false;
  plan_idx weedout_end = NO_PLAN_IDX;
  if (qep_tab->starts_weedout() &&
      !(calling_context == DIRECTLY_UNDER_WEEDOUT && this_idx == first_idx)) {
    for (plan_idx j = this_idx; j < last_idx; ++j) {
      if (qep_tabs[j].check_weed_out_table == qep_tab->flush_weedout_table) {
        weedout_end = j + 1;
        break;
      }
    }
    if (weedout_end != NO_PLAN_IDX) {
      is_weedout = true;
    }
  }

  if (weedout_end > last_idx) {
    // See comment above.
    MarkUnhandledDuplicates(qep_tab->flush_weedout_table, this_idx, weedout_end,
                            unhandled_duplicates);
    is_weedout = false;
  }

  if (is_outer_join && is_weedout) {
    if (outer_join_end > weedout_end) {
      // Weedout will be handled at a lower recursion level.
      is_weedout = false;
    } else {
      // See comment above.
      MarkUnhandledDuplicates(qep_tab->flush_weedout_table, this_idx,
                              weedout_end, unhandled_duplicates);
      is_weedout = false;
    }
  }
  if (is_semijoin && is_weedout) {
    if (semijoin_end > weedout_end) {
      // Weedout will be handled at a lower recursion level.
      is_weedout = false;
    } else {
      // See comment above.
      MarkUnhandledDuplicates(qep_tab->flush_weedout_table, this_idx,
                              weedout_end, unhandled_duplicates);
      is_weedout = false;
    }
  }

  // Occasionally, a subslice may be designated as the right side of both a
  // semijoin _and_ an outer join. This is a fairly odd construction,
  // as it means exactly one row is generated no matter what (negating the
  // point of a semijoin in the first place), and typically happens as the
  // result of the join optimizer reordering tables that have no real bearing
  // on the query, such as ... WHERE t1 IN ( t2.i FROM t2 LEFT JOIN t3 )
  // with the ordering t2, t1, t3 (t3 will now be in such a situation).
  //
  // Nominally, these tables should be optimized away, but this is not the
  // right place for that, so we solve it by adding a LIMIT 1 and then
  // treating the slice as a normal outer join.
  *add_limit_1 = false;
  if (is_semijoin && is_outer_join) {
    if (semijoin_end == outer_join_end) {
      *add_limit_1 = true;
      is_semijoin = false;
    } else if (semijoin_end > outer_join_end) {
      // A special case of the special case; there might be more than one
      // outer join contained in this semijoin, e.g. A LEFT JOIN B LEFT JOIN C
      // where the combination B-C is _also_ the right side of a semijoin.
      // This forms a non-hierarchical structure and should be exceedingly rare,
      // so we handle it the same way we handle non-hierarchical weedout above,
      // ie., just by removing the added duplicates at the top of the query.
      *unhandled_duplicates |= TablesBetween(this_idx, semijoin_end);
      is_semijoin = false;
    }
  }

  // Yet another special case like the above; this is when we have a semijoin
  // and then a partially overlapping outer join that ends outside the semijoin.
  // E.g., A JOIN B JOIN C LEFT JOIN D, where A..C denotes a semijoin
  // (C has first match back to A).
  if (is_semijoin) {
    for (plan_idx i = this_idx; i < semijoin_end; ++i) {
      if (qep_tabs[i].last_inner() >= semijoin_end) {
        // Handle this semijoin as non-hierarchical weedout above.
        *unhandled_duplicates |= TablesBetween(this_idx, semijoin_end);
        is_semijoin = false;
        break;
      }
    }
  }

  // We may have detected both a semijoin and an outer join starting at
  // this table. Decide which one is the outermost that is not already
  // processed, so that we recurse in the right order.
  if (calling_context == DIRECTLY_UNDER_SEMIJOIN && this_idx == first_idx &&
      semijoin_end == last_idx) {
    is_semijoin = false;
  } else if (calling_context == DIRECTLY_UNDER_OUTER_JOIN &&
             this_idx == first_idx && outer_join_end == last_idx) {
    is_outer_join = false;
  }
  if (is_semijoin && is_outer_join) {
    DBUG_ASSERT(outer_join_end > semijoin_end);
    is_semijoin = false;
  }

  DBUG_ASSERT(is_semijoin + is_outer_join + is_weedout <= 1);

  if (is_semijoin) {
    *substructure_end = semijoin_end;
    return Substructure::SEMIJOIN;
  } else if (is_outer_join) {
    *substructure_end = outer_join_end;
    return Substructure::OUTER_JOIN;
  } else if (is_weedout) {
    *substructure_end = weedout_end;
    return Substructure::WEEDOUT;
  } else {
    *substructure_end = NO_PLAN_IDX;  // Not used.
    return Substructure::NONE;
  }
}

/// @cond Doxygen_is_confused
static unique_ptr_destroy_only<RowIterator> ConnectJoins(
    plan_idx first_idx, plan_idx last_idx, QEP_TAB *qep_tabs, THD *thd,
    CallingContext calling_context,
    vector<PendingCondition> *pending_conditions,
    vector<PendingInvalidator> *pending_invalidators,
    qep_tab_map *unhandled_duplicates);
/// @endcond

/**
  Get the RowIterator used for scanning the given table, with any required
  materialization operations done first.
 */
unique_ptr_destroy_only<RowIterator> GetTableIterator(THD *thd,
                                                      QEP_TAB *qep_tab,
                                                      QEP_TAB *qep_tabs) {
  unique_ptr_destroy_only<RowIterator> table_iterator;
  if (qep_tab->materialize_table == join_materialize_derived) {
    SELECT_LEX_UNIT *unit = qep_tab->table_ref->derived_unit();
    JOIN *subjoin = nullptr;
    Temp_table_param *tmp_table_param;
    int select_number;

    // If we have a single query block at the end of the QEP_TAB array,
    // it may contain aggregation that have already set up fields and items
    // to copy, and we need to pass those to MaterializeIterator, so reuse its
    // tmp_table_param. If not, make a new object, so that we don't
    // disturb the materialization going on inside our own query block.
    if (unit->is_simple()) {
      subjoin = unit->first_select()->join;
      tmp_table_param = &unit->first_select()->join->tmp_table_param;
      select_number = subjoin->select_lex->select_number;
    } else if (unit->fake_select_lex != nullptr) {
      // NOTE: subjoin here is never used, as ConvertItemsToCopy only uses it
      // for ROLLUP, and fake_select_lex can't have ROLLUP.
      subjoin = unit->fake_select_lex->join;
      tmp_table_param = &unit->fake_select_lex->join->tmp_table_param;
      select_number = unit->fake_select_lex->select_number;
    } else {
      tmp_table_param = new (thd->mem_root) Temp_table_param;
      select_number = unit->first_select()->select_number;
    }
    ConvertItemsToCopy(unit->get_field_list(),
                       qep_tab->table()->visible_field_ptr(), tmp_table_param,
                       subjoin);
    bool copy_fields_and_items_in_materialize = true;
    if (unit->is_simple()) {
      // See if AggregateIterator already does this for us.
      JOIN *join = unit->first_select()->join;
      copy_fields_and_items_in_materialize =
          !join->streaming_aggregation ||
          join->tmp_table_param.precomputed_group_by;
    }

    MaterializeIterator *materialize = nullptr;

    if (unit->unfinished_materialization()) {
      // The unit is a UNION capable of materializing directly into our result
      // table. This saves us from doing double materialization (first into
      // a UNION result table, then from there into our own).
      //
      // We will already have set up a unique index on the table if
      // required; see TABLE_LIST::setup_materialized_derived_tmp_table().
      table_iterator = NewIterator<MaterializeIterator>(
          thd, unit->release_query_blocks_to_materialize(), qep_tab->table(),
          move(qep_tab->iterator), qep_tab->table_ref->common_table_expr(),
          unit, /*subjoin=*/nullptr,
          /*ref_slice=*/-1, qep_tab->rematerialize, unit->select_limit_cnt);
      materialize =
          down_cast<MaterializeIterator *>(table_iterator->real_iterator());
      if (unit->offset_limit_cnt != 0) {
        // LIMIT is handled inside MaterializeIterator, but OFFSET is not.
        // SQL_CALC_FOUND_ROWS cannot occur in a derived table's definition.
        table_iterator = NewIterator<LimitOffsetIterator>(
            thd, move(table_iterator), unit->select_limit_cnt,
            unit->offset_limit_cnt, /*count_all_rows=*/false,
            /*skipped_rows=*/nullptr);
      }
    } else if (qep_tab->table_ref->common_table_expr() == nullptr &&
               qep_tab->rematerialize && qep_tab->using_table_scan()) {
      // We don't actually need the materialization for anything (we would
      // just reading the rows straight out from the table, never to be used
      // again), so we can just stream records directly over to the next
      // iterator. This saves both CPU time and memory (for the temporary
      // table).
      //
      // NOTE: Currently, qep_tab->rematerialize is true only for JSON_TABLE.
      // We could extend this to other situations, such as the leftmost
      // table of the join (assuming nested loop only). The test for CTEs is
      // also conservative; if the CTEs is defined within this join and used
      // only once, we could still stream without losing performance.
      table_iterator = NewIterator<StreamingIterator>(
          thd, unit->release_root_iterator(), &subjoin->tmp_table_param,
          qep_tab->table(), copy_fields_and_items_in_materialize);
    } else {
      table_iterator = NewIterator<MaterializeIterator>(
          thd, unit->release_root_iterator(), tmp_table_param, qep_tab->table(),
          move(qep_tab->iterator), qep_tab->table_ref->common_table_expr(),
          select_number, unit, /*subjoin=*/nullptr,
          /*ref_slice=*/-1, copy_fields_and_items_in_materialize,
          qep_tab->rematerialize, tmp_table_param->end_write_records);
      materialize =
          down_cast<MaterializeIterator *>(table_iterator->real_iterator());
    }

    if (!qep_tab->rematerialize) {
      if (qep_tab->invalidators != nullptr) {
        for (const CacheInvalidatorIterator *iterator :
             *qep_tab->invalidators) {
          materialize->AddInvalidator(iterator);
        }
      }
    }
  } else if (qep_tab->materialize_table == join_materialize_table_function) {
    table_iterator = NewIterator<MaterializedTableFunctionIterator>(
        thd, qep_tab->table_ref->table_function, qep_tab->table(),
        move(qep_tab->iterator));
  } else if (qep_tab->materialize_table == join_materialize_semijoin) {
    Semijoin_mat_exec *sjm = qep_tab->sj_mat_exec();

    // create_tmp_table() has already filled sjm->table_param.items_to_copy.
    // However, the structures there are not used by
    // join_materialize_semijoin, and don't have e.g. result fields set up
    // correctly, so we just clear it and create our own.
    sjm->table_param.items_to_copy = nullptr;
    ConvertItemsToCopy(&sjm->sj_nest->nested_join->sj_inner_exprs,
                       qep_tab->table()->visible_field_ptr(), &sjm->table_param,
                       qep_tab->join());

    int join_start = sjm->inner_table_index;
    int join_end = join_start + sjm->table_count;

    // Handle this subquery as a we would a completely separate join,
    // even though the tables are part of the same JOIN object
    // (so in effect, a “virtual join”).
    qep_tab_map unhandled_duplicates = 0;
    unique_ptr_destroy_only<RowIterator> subtree_iterator =
        ConnectJoins(join_start, join_end, qep_tabs, thd, TOP_LEVEL,
                     /*pending_conditions=*/nullptr,
                     /*pending_invalidators=*/nullptr, &unhandled_duplicates);

    // If there were any weedouts that we had to drop during ConnectJoins()
    // (ie., the join left some tables that were supposed to be deduplicated
    // but were not), handle them now at the end of the virtual join.
    if (unhandled_duplicates != 0) {
      subtree_iterator = CreateWeedoutIteratorForTables(
          thd, unhandled_duplicates, qep_tab, qep_tab->join()->primary_tables,
          move(subtree_iterator));
    }

    // Since materialized semijoins are based on ref access against the table,
    // and ref access has NULL = NULL (while IN expressions should not),
    // remove rows with NULLs in them here. This is only an optimization for IN
    // (since equality propagation will filter away NULLs on the other side),
    // but is required for NOT IN correctness.
    //
    // TODO: It could be possible to join this with an existing condition,
    // and possibly also in some cases when scanning each table.
    vector<Item *> not_null_conditions;
    for (Item &item : sjm->sj_nest->nested_join->sj_inner_exprs) {
      if (item.maybe_null) {
        Item *condition = new Item_func_isnotnull(&item);
        condition->quick_fix_field();
        condition->update_used_tables();
        condition->apply_is_true();
        not_null_conditions.push_back(condition);
      }
    }
    subtree_iterator = PossiblyAttachFilterIterator(move(subtree_iterator),
                                                    not_null_conditions, thd);

    bool copy_fields_and_items_in_materialize =
        true;  // We never have aggregation within semijoins.
    table_iterator = NewIterator<MaterializeIterator>(
        thd, move(subtree_iterator), &sjm->table_param, qep_tab->table(),
        move(qep_tab->iterator), /*cte=*/nullptr,
        qep_tab->join()->select_lex->select_number, /*unit=*/nullptr,
        qep_tab->join(),
        /*ref_slice=*/-1, copy_fields_and_items_in_materialize,
        qep_tab->rematerialize, sjm->table_param.end_write_records);

#ifndef DBUG_OFF
    // Make sure we clear this table out when the join is reset,
    // since its contents may depend on outer expressions.
    bool found = false;
    for (TABLE &sj_tmp_tab : qep_tab->join()->sj_tmp_tables) {
      if (&sj_tmp_tab == qep_tab->table()) {
        found = true;
        break;
      }
    }
    DBUG_ASSERT(found);
#endif
  } else {
    table_iterator = move(qep_tab->iterator);

    POSITION *pos = qep_tab->position();
    if (pos != nullptr) {
      SetCostOnTableIterator(*thd->cost_model(), pos, /*is_after_filter=*/false,
                             table_iterator.get());
    }

    // See if this is an information schema table that must be filled in before
    // we scan.
    if (qep_tab->table_ref->schema_table &&
        qep_tab->table_ref->schema_table->fill_table) {
      table_iterator.reset(new (thd->mem_root)
                               MaterializeInformationSchemaTableIterator(
                                   thd, qep_tab, move(table_iterator)));
    }
  }
  return table_iterator;
}

void SetCostOnTableIterator(const Cost_model_server &cost_model,
                            const POSITION *pos, bool is_after_filter,
                            RowIterator *iterator) {
  double num_rows_after_filtering = pos->rows_fetched * pos->filter_effect;
  if (is_after_filter) {
    iterator->set_expected_rows(num_rows_after_filtering);
  } else {
    iterator->set_expected_rows(pos->rows_fetched);
  }

  // Note that we don't try to adjust for the filtering here;
  // we estimate the same cost as the table itself.
  double cost =
      pos->read_cost + cost_model.row_evaluate_cost(num_rows_after_filtering);
  if (pos->prefix_rowcount <= 0.0) {
    iterator->set_estimated_cost(cost);
  } else {
    // Scale the estimated cost to being for one loop only, to match the
    // measured costs.
    iterator->set_estimated_cost(cost * num_rows_after_filtering /
                                 pos->prefix_rowcount);
  }
}

void SetCostOnNestedLoopIterator(const Cost_model_server &cost_model,
                                 const POSITION *pos_right,
                                 RowIterator *iterator) {
  if (pos_right == nullptr) {
    // No cost information.
    return;
  }

  DBUG_ASSERT(iterator->children().size() == 2);
  RowIterator *left = iterator->children()[0].iterator;
  RowIterator *right = iterator->children()[1].iterator;

  if (left->expected_rows() == -1.0 || right->expected_rows() == -1.0) {
    // Missing cost information on at least one child.
    return;
  }

  // Mirrors set_prefix_join_cost(), even though the cost calculation doesn't
  // make a lot of sense.
  double right_expected_rows_before_filter =
      pos_right->filter_effect > 0.0
          ? (right->expected_rows() / pos_right->filter_effect)
          : 0.0;
  double joined_rows =
      left->expected_rows() * right_expected_rows_before_filter;
  iterator->set_expected_rows(joined_rows * pos_right->filter_effect);
  iterator->set_estimated_cost(left->estimated_cost() + pos_right->read_cost +
                               cost_model.row_evaluate_cost(joined_rows));
}

void SetCostOnHashJoinIterator(const Cost_model_server &cost_model,
                               const POSITION *pos_right,
                               RowIterator *iterator) {
  if (pos_right == nullptr) {
    // No cost information.
    return;
  }

  DBUG_ASSERT(iterator->children().size() == 2);
  RowIterator *left = iterator->children()[1].iterator;
  RowIterator *right = iterator->children()[0].iterator;

  if (left->expected_rows() == -1.0 || right->expected_rows() == -1.0) {
    // Missing cost information on at least one child.
    return;
  }

  // Mirrors set_prefix_join_cost(), even though the cost calculation doesn't
  // make a lot of sense.
  double joined_rows = left->expected_rows() * right->expected_rows();
  iterator->set_expected_rows(joined_rows * pos_right->filter_effect);
  iterator->set_estimated_cost(left->estimated_cost() + pos_right->read_cost +
                               cost_model.row_evaluate_cost(joined_rows));
}

// Move all the hash join conditions from the vector "predicates" over to the
// vector "hash_join_conditions". Only join conditions that are suitable for
// hash join are moved. If there are any condition that has to be evaluated
// after the join (i.e. non equi-join conditions), they are placed in the vector
// "conditions_after_hash_join" so that they can be attached as filters after
// the join.
static void ExtractHashJoinConditions(
    const QEP_TAB *current_table, qep_tab_map left_tables,
    vector<Item *> *predicates, vector<Item_func_eq *> *hash_join_conditions,
    vector<Item *> *conditions_after_hash_join) {
  table_map left_tables_map = 0;
  for (QEP_TAB *qep_tab :
       TablesContainedIn(current_table->join(), left_tables)) {
    left_tables_map = left_tables_map | qep_tab->table_ref->map();
  }

  for (Item *item : *predicates) {
    if (item->type() != Item::FUNC_ITEM) {
      continue;
    }

    Item_func *func_item = down_cast<Item_func *>(item);
    if (func_item->functype() != Item_func::EQ_FUNC) {
      continue;
    }

    Item_func_eq *item_func_eq = down_cast<Item_func_eq *>(func_item);
    if (item_func_eq->has_any_hash_join_condition(left_tables_map,
                                                  *current_table)) {
      hash_join_conditions->emplace_back(item_func_eq);
    }
  }

  // Remove all hash join conditions from the vector "predicates".
  predicates->erase(remove_if(predicates->begin(), predicates->end(),
                              [&hash_join_conditions](const Item *item) {
                                return find(hash_join_conditions->begin(),
                                            hash_join_conditions->end(),
                                            item) !=
                                       hash_join_conditions->end();
                              }),
                    predicates->end());

  // See if any of the remaining conditions should be attached as filter after
  // the join. If so, place them in a separate vector.
  for (int i = predicates->size() - 1; i >= 0; --i) {
    Item *item = predicates->at(i);
    table_map used_tables = item->used_tables();
    if ((~current_table->table_ref->map() & used_tables) > 0) {
      conditions_after_hash_join->emplace_back(item);
      predicates->erase(predicates->begin() + i);
    }
  }
}

/**
  For a given slice of the table list, build up the iterator tree corresponding
  to the tables in that slice. It handles inner and outer joins, as well as
  semijoins (“first match”).

  The join tree in MySQL is generally a left-deep tree of inner joins,
  so we can start at the left, make an inner join against the next table,
  join the result of that against the next table, etc.. However, a given
  sub-slice of the table list can be designated as an outer join, by setting
  first_inner() and last_inner() on the first table of said slice. (It is also
  set in some, but not all, of the other tables in the slice.) If so, we call
  ourselves recursively with that slice, put it as the right (inner) arm of
  an outer join, and then continue with our inner join.

  Similarly, if a table N has set “first match” to table M (ie., jump back to
  table M whenever we see a non-filtered record in table N), then there is a
  subslice from [M+1,N] that we need to process recursively before putting it
  as the right side of a semijoin. Every semijoin can be implemented with a
  LIMIT 1, but for clarity and performance, we prefer to use a NestedLoopJoin
  with a special SEMI join type whenever possible. Sometimes, we have no choice,
  though (see the comments below). Note that we cannot use first_sj_inner() for
  detecting semijoins, as it is not updated when tables are reordered by the
  join optimizer. Outer joins and semijoins can nest, so we need to take some
  care to make sure that we pick the outermost structure to recurse on.

  Conditions are a bit tricky. Conceptually, SQL evaluates conditions only
  after all tables have been joined; however, for efficiency reasons, we want
  to evaluate them as early as possible. As long as we are only dealing with
  inner joins, this is as soon as we've read all tables participating in the
  condition, but for outer joins, we need to wait until the join has happened.
  See pending_conditions below.

  @param first_idx index of the first table in the slice we are creating a
    tree for (inclusive)
  @param last_idx index of the last table in the slice we are creating a
    tree for (exclusive)
  @param qep_tabs the full list of tables we are joining
  @param thd the THD to allocate the iterators on
  @param calling_context what situation we have immediately around is in the
    tree (ie., whether we are called to resolve the inner part of an outer
    join, a semijoin, etc.); mostly used to avoid infinite recursion where we
    would process e.g. the same semijoin over and over again
  @param pending_conditions if nullptr, we are not at the right (inner) side of
    any outer join and can evaluate conditions immediately. If not, we need to
    push any WHERE predicates to that vector and evaluate them only after joins.
  @param pending_invalidators similar to pending_conditions, but for tables
    that should have a CacheInvalidatorIterator synthesized for them;
    NULL-complemented rows must also invalidate materialized lateral derived
  tables.
  @param[out] unhandled_duplicates list of tables we should have deduplicated
    using duplicate weedout, but could not; append-only.
 */
static unique_ptr_destroy_only<RowIterator> ConnectJoins(
    plan_idx first_idx, plan_idx last_idx, QEP_TAB *qep_tabs, THD *thd,
    CallingContext calling_context,
    vector<PendingCondition> *pending_conditions,
    vector<PendingInvalidator> *pending_invalidators,
    qep_tab_map *unhandled_duplicates) {
  DBUG_ASSERT(last_idx > first_idx);
  DBUG_ASSERT((pending_conditions == nullptr) ==
              (pending_invalidators == nullptr));
  unique_ptr_destroy_only<RowIterator> iterator = nullptr;

  // A special case: If we are at the top but the first table is an outer
  // join, we implicitly have one or more const tables to the left side
  // of said join.
  bool is_top_level_outer_join =
      calling_context == TOP_LEVEL &&
      qep_tabs[first_idx].last_inner() != NO_PLAN_IDX;

  vector<PendingCondition> top_level_pending_conditions;
  vector<PendingInvalidator> top_level_pending_invalidators;
  if (is_top_level_outer_join) {
    iterator =
        NewIterator<FakeSingleRowIterator>(thd, /*examined_rows=*/nullptr);
    pending_conditions = &top_level_pending_conditions;
    pending_invalidators = &top_level_pending_invalidators;
  }

  // NOTE: i is advanced in one of two ways:
  //
  //  - If we have an inner join, it will be incremented near the bottom of the
  //    loop, as we can process inner join tables one by one.
  //  - If not (ie., we have an outer join or semijoin), we will process
  //    the sub-join recursively, and thus move it past the end of said
  //    sub-join.
  for (plan_idx i = first_idx; i < last_idx;) {
    bool add_limit_1;
    plan_idx substructure_end;
    Substructure substructure =
        FindSubstructure(qep_tabs, first_idx, i, last_idx, calling_context,
                         &add_limit_1, &substructure_end, unhandled_duplicates);

    QEP_TAB *qep_tab = &qep_tabs[i];
    if (substructure == Substructure::OUTER_JOIN ||
        substructure == Substructure::SEMIJOIN) {
      // Outer or semijoin, consisting of a subtree (possibly of only one
      // table), so we send the entire subtree down to a recursive invocation
      // and then join the returned root into our existing tree.
      unique_ptr_destroy_only<RowIterator> subtree_iterator;
      vector<PendingCondition> subtree_pending_conditions;
      vector<PendingInvalidator> subtree_pending_invalidators;
      if (substructure == Substructure::SEMIJOIN) {
        // Semijoins don't have special handling of WHERE, so simply recurse.
        subtree_iterator = ConnectJoins(
            i, substructure_end, qep_tabs, thd, DIRECTLY_UNDER_SEMIJOIN,
            pending_conditions, pending_invalidators, unhandled_duplicates);
      } else if (pending_conditions != nullptr) {
        // We are already on the right (inner) side of an outer join,
        // so we need to keep deferring WHERE predicates.
        subtree_iterator = ConnectJoins(
            i, substructure_end, qep_tabs, thd, DIRECTLY_UNDER_OUTER_JOIN,
            pending_conditions, pending_invalidators, unhandled_duplicates);

        // Pick out any conditions that should be directly above this join
        // (ie., the ON conditions for this specific join).
        for (auto it = pending_conditions->begin();
             it != pending_conditions->end();) {
          if (it->table_index_to_attach_to == int(i)) {
            subtree_pending_conditions.push_back(*it);
            it = pending_conditions->erase(it);
          } else {
            ++it;
          }
        }

        // Similarly, for invalidators.
        for (auto it = pending_invalidators->begin();
             it != pending_invalidators->end();) {
          if (it->table_index_to_attach_to == int(i)) {
            subtree_pending_invalidators.push_back(*it);
            it = pending_invalidators->erase(it);
          } else {
            ++it;
          }
        }
      } else {
        // We can check the WHERE predicates on this table right away
        // after the join (and similarly, set up invalidators).
        subtree_iterator =
            ConnectJoins(i, substructure_end, qep_tabs, thd,
                         DIRECTLY_UNDER_OUTER_JOIN, &subtree_pending_conditions,
                         &subtree_pending_invalidators, unhandled_duplicates);
      }

      JoinType join_type;
      if (qep_tab->table()->reginfo.not_exists_optimize) {
        // Similar to the comment on SplitConditions (see case #3), we can only
        // enable anti-join optimizations if we are not already on the right
        // (inner) side of another outer join. Otherwise, we would cause the
        // higher-up outer join to create NULL rows where there should be none.
        DBUG_ASSERT(substructure != Substructure::SEMIJOIN);
        join_type =
            (pending_conditions == nullptr) ? JoinType::ANTI : JoinType::OUTER;

        // Normally, a ”found” trigger means that the condition should be moved
        // up above some outer join (ie., it's a WHERE, not an ON condition).
        // However, there is one specific case where the optimizer sets up such
        // a trigger with the condition being _the same table as it's posted
        // on_, namely anti-joins used for NOT IN; here, a FALSE condition is
        // being used to specify that inner rows should pass by the join, but
        // they should inhibit the null-complemented row. (So in this case,
        // the anti-join is no longer just an optimization that can be ignored
        // as we rewrite into an outer join.) In this case, there's a condition
        // wrapped in “not_null_compl” and ”found”, with the trigger for both
        // being the same table as the condition is posted on.
        //
        // So, as a special exception, detect this case, removing these
        // conditions (as they would otherwise kill all of our output rows) and
        // use them to mark the join as _really_ anti-join, even when it's
        // within an outer join.
        for (auto it = subtree_pending_conditions.begin();
             it != subtree_pending_conditions.end();) {
          if (it->table_index_to_attach_to == int(i) &&
              it->cond->item_name.ptr() == antijoin_null_cond) {
            DBUG_ASSERT(nullptr != dynamic_cast<Item_func_false *>(it->cond));
            join_type = JoinType::ANTI;
            it = subtree_pending_conditions.erase(it);
          } else {
            ++it;
          }
        }
      } else {
        join_type = substructure == Substructure::SEMIJOIN ? JoinType::SEMI
                                                           : JoinType::OUTER;
      }

      // If the entire slice is a semijoin (e.g. because we are semijoined
      // against all the const tables, or because we're a semijoin within an
      // outer join), solve it by using LIMIT 1.
      //
      // If the entire slice is an outer join, we've solved that in a more
      // roundabout way; see is_top_level_outer_join above.
      if (iterator == nullptr) {
        DBUG_ASSERT(substructure == Substructure::SEMIJOIN);
        add_limit_1 = true;
      }

      if (add_limit_1) {
        subtree_iterator = NewIterator<LimitOffsetIterator>(
            thd, move(subtree_iterator), /*limit=*/1, /*offset=*/0,
            /*count_all_rows=*/false, /*skipped_rows=*/nullptr);
      }

      const bool pfs_batch_mode = qep_tab->pfs_batch_update(qep_tab->join()) &&
                                  join_type != JoinType::ANTI &&
                                  join_type != JoinType::SEMI;
      bool remove_duplicates_loose_scan = false;
      if (i != first_idx && qep_tabs[i - 1].do_loosescan() &&
          qep_tabs[i - 1].match_tab != i - 1) {
        QEP_TAB *prev_qep_tab = &qep_tabs[i - 1];
        DBUG_ASSERT(iterator != nullptr);

        KEY *key = prev_qep_tab->table()->key_info + prev_qep_tab->index();
        if (substructure == Substructure::SEMIJOIN) {
          iterator =
              NewIterator<NestedLoopSemiJoinWithDuplicateRemovalIterator>(
                  thd, move(iterator), move(subtree_iterator),
                  prev_qep_tab->table(), key, prev_qep_tab->loosescan_key_len);
          SetCostOnNestedLoopIterator(*thd->cost_model(), qep_tab->position(),
                                      iterator.get());
        } else {
          // We were originally in a semijoin, even if it didn't win in
          // FindSubstructure (LooseScan against multiple tables always puts
          // the non-first tables in FirstMatch), it was just overridden by
          // the outer join. In this case, we put duplicate removal after the
          // join (and any associated filtering), which is the safe option --
          // and in this case, it's no slower, since we'll be having a LIMIT 1
          // inserted anyway.
          DBUG_ASSERT(substructure == Substructure::OUTER_JOIN);
          remove_duplicates_loose_scan = true;

          iterator = NewIterator<NestedLoopIterator>(thd, move(iterator),
                                                     move(subtree_iterator),
                                                     join_type, pfs_batch_mode);
          SetCostOnNestedLoopIterator(*thd->cost_model(), qep_tab->position(),
                                      iterator.get());
        }
      } else if (iterator == nullptr) {
        DBUG_ASSERT(substructure == Substructure::SEMIJOIN);
        iterator = move(subtree_iterator);
      } else {
        iterator = NewIterator<NestedLoopIterator>(thd, move(iterator),
                                                   move(subtree_iterator),
                                                   join_type, pfs_batch_mode);
        SetCostOnNestedLoopIterator(*thd->cost_model(), qep_tab->position(),
                                    iterator.get());
      }

      iterator = PossiblyAttachFilterIterator(move(iterator),
                                              subtree_pending_conditions, thd);

      if (remove_duplicates_loose_scan) {
        QEP_TAB *prev_qep_tab = &qep_tabs[i - 1];
        KEY *key = prev_qep_tab->table()->key_info + prev_qep_tab->index();
        iterator = NewIterator<RemoveDuplicatesIterator>(
            thd, move(iterator), prev_qep_tab->table(), key,
            prev_qep_tab->loosescan_key_len);
      }

      // It's highly unlikely that we have more than one pending QEP_TAB here
      // (the most common case will be zero), so don't bother combining them
      // into one invalidator.
      for (const PendingInvalidator &invalidator :
           subtree_pending_invalidators) {
        iterator =
            CreateInvalidatorIterator(thd, invalidator.qep_tab, move(iterator));
      }

      i = substructure_end;
      continue;
    } else if (substructure == Substructure::WEEDOUT) {
      unique_ptr_destroy_only<RowIterator> subtree_iterator = ConnectJoins(
          i, substructure_end, qep_tabs, thd, DIRECTLY_UNDER_WEEDOUT,
          pending_conditions, pending_invalidators, unhandled_duplicates);
      RowIterator *child_iterator = subtree_iterator.get();
      subtree_iterator = CreateWeedoutIterator(thd, move(subtree_iterator),
                                               qep_tab->flush_weedout_table);

      // Copy costs (even though it makes no sense for the LIMIT 1 case).
      subtree_iterator->set_expected_rows(child_iterator->expected_rows());
      subtree_iterator->set_estimated_cost(child_iterator->estimated_cost());

      if (iterator == nullptr) {
        iterator = move(subtree_iterator);
      } else {
        iterator = NewIterator<NestedLoopIterator>(
            thd, move(iterator), move(subtree_iterator), JoinType::INNER,
            /*pfs_batch_mode=*/false);
        SetCostOnNestedLoopIterator(*thd->cost_model(), qep_tab->position(),
                                    iterator.get());
      }

      i = substructure_end;
      continue;
    }

    unique_ptr_destroy_only<RowIterator> table_iterator =
        GetTableIterator(thd, qep_tab, qep_tabs);
    MultiRangeRowIterator *mrr_iterator_ptr = nullptr;

    vector<Item *> predicates_below_join;
    vector<Item_func_eq *> hash_join_conditions;
    vector<Item *> conditions_after_hash_join;
    vector<PendingCondition> predicates_above_join;
    SplitConditions(qep_tab->condition(), &predicates_below_join,
                    &predicates_above_join);

    qep_tab_map left_tables = 0;

    // If this is a BNL, we should replace it with hash join. We did decide
    // during create_iterators that we actually can replace the BNL with a hash
    // join, so we don't bother checking any further that we actually can
    // replace the BNL with a hash join.
    const bool replace_with_hash_join =
        qep_tab->op != nullptr &&
        qep_tab->op->type() == QEP_operation::OT_CACHE &&
        down_cast<JOIN_CACHE *>(qep_tab->op)->cache_type() !=
            JOIN_CACHE::ALG_BKA;

    // We can always do BKA. The setup is very similar to hash join.
    const bool is_bka = qep_tab->op != nullptr &&
                        qep_tab->op->type() == QEP_operation::OT_CACHE &&
                        down_cast<JOIN_CACHE *>(qep_tab->op)->cache_type() ==
                            JOIN_CACHE::ALG_BKA;

    if (replace_with_hash_join || is_bka) {
      // Get the left tables of this join.
      left_tables |= TablesBetween(first_idx, i);

      if (is_bka) {
        table_iterator = NewIterator<MultiRangeRowIterator>(
            thd, qep_tab->join(), left_tables, qep_tab->cache_idx_cond,
            qep_tab->table(), qep_tab->copy_current_rowid, &qep_tab->ref(),
            qep_tab->position()->table->join_cache_flags);
        mrr_iterator_ptr =
            down_cast<MultiRangeRowIterator *>(table_iterator->real_iterator());
      } else {
        // All join conditions are now contained in "predicates_below_join". We
        // will now take all the hash join conditions (equi-join conditions) and
        // move them to a separate vector so we can attach them to the hash join
        // iterator later. Also, "predicates_below_join" might contain
        // conditions that should be applied after the join (for instance non
        // equi-join conditions). Put them in a separate vector, and attach them
        // as a filter after the hash join.
        ExtractHashJoinConditions(qep_tab, left_tables, &predicates_below_join,
                                  &hash_join_conditions,
                                  &conditions_after_hash_join);
      }
    }

    if (!qep_tab->condition_is_pushed_to_sort()) {  // See the comment on #2.
      double expected_rows = table_iterator->expected_rows();
      table_iterator = PossiblyAttachFilterIterator(move(table_iterator),
                                                    predicates_below_join, thd);
      POSITION *pos = qep_tab->position();
      if (expected_rows >= 0.0 && !predicates_below_join.empty() &&
          pos != nullptr) {
        SetCostOnTableIterator(*thd->cost_model(), pos,
                               /*is_after_filter=*/true, table_iterator.get());
      }
    }

    // Handle LooseScan that hits this specific table only.
    // Multi-table LooseScans will be handled by
    // NestedLoopSemiJoinWithDuplicateRemovalIterator
    // (which is essentially a semijoin NestedLoopIterator and
    // RemoveDuplicatesIterator in one).
    if (qep_tab->do_loosescan() && qep_tab->match_tab == i) {
      KEY *key = qep_tab->table()->key_info + qep_tab->index();
      table_iterator = NewIterator<RemoveDuplicatesIterator>(
          thd, move(table_iterator), qep_tab->table(), key,
          qep_tab->loosescan_key_len);
    }

    if (qep_tab->lateral_derived_tables_depend_on_me) {
      if (pending_invalidators != nullptr) {
        pending_invalidators->push_back(
            PendingInvalidator{qep_tab, /*table_index_to_attach_to=*/i});
      } else {
        table_iterator =
            CreateInvalidatorIterator(thd, qep_tab, move(table_iterator));
      }
    }

    if (iterator == nullptr) {
      // We are the first table in this join.
      iterator = move(table_iterator);
    } else {
      // We can only enable DISTINCT optimizations if we are not in the right
      // (inner) side of an outer join; since the filter is deferred, the limit
      // would have to be, too. Similarly, we the old executor can do these
      // optimizations for multiple tables, but it requires poking into global
      // state to see if later tables produced rows or not; we restrict
      // ourselves to the rightmost table, instead of trying to make iterators
      // look at nonlocal state.
      //
      // We don't lose correctness by not applying the limit, only performance
      // on some fairly rare queries (for for former: DISTINCT queries where we
      // outer-join in a table that we don't use in the select list, but filter
      // on one of the columns; for the latter: queries with multiple unused
      // tables).
      //
      // Note that if we are to attach a hash join iterator, we cannot add this
      // optimization, as it would limit the probe input to only one row before
      // the join condition is even applied. Same with BKA; we need to buffer
      // the entire input, since we don't know if there's a match until the join
      // has actually happened.
      //
      // TODO: Consider pushing this limit up the tree together with the filter.
      // Note that this would require some trickery to reset the filter for
      // each new row on the left side of the join, so it's probably not worth
      // it.
      if (qep_tab->not_used_in_distinct && pending_conditions == nullptr &&
          i == static_cast<plan_idx>(qep_tab->join()->primary_tables - 1) &&
          !add_limit_1 && !replace_with_hash_join && !is_bka) {
        table_iterator = NewIterator<LimitOffsetIterator>(
            thd, move(table_iterator), /*limit=*/1, /*offset=*/0,
            /*count_all_rows=*/false, /*skipped_rows=*/nullptr);
      }

      // Inner join this table to the existing tree.
      // Inner joins are always left-deep, so we can just attach the tables as
      // we find them.
      DBUG_ASSERT(qep_tab->last_inner() == NO_PLAN_IDX);

      if (is_bka) {
        const TABLE *table = qep_tab->table();
        const TABLE_REF *ref = &qep_tab->ref();
        const float rec_per_key =
            table->key_info[ref->key].records_per_key(ref->key_parts - 1);
        iterator = NewIterator<BKAIterator>(
            thd, qep_tab->join(), move(iterator), left_tables,
            move(table_iterator), thd->variables.join_buff_size,
            table->file->stats.mrr_length_per_rec, rec_per_key,
            mrr_iterator_ptr);
      } else if (replace_with_hash_join) {
        const bool has_grouping =
            qep_tab->join()->implicit_grouping || qep_tab->join()->grouped;

        const bool has_limit = qep_tab->join()->m_select_limit != HA_POS_ERROR;

        const bool has_order_by = qep_tab->join()->order.order != nullptr;

        // If we have a limit in the query, do not allow hash join to spill to
        // disk. The effect of this is that hash join will start producing
        // result rows a lot earlier, and thus hit the LIMIT a lot sooner.
        // Ideally, this should be decided during optimization.
        // There are however two situations where we always allow spill to disk,
        // and that is if we either have grouping or sorting in the query. In
        // those cases, the iterator above us will most likely consume the
        // entire result set anyways.
        const bool allow_spill_to_disk =
            !has_limit || has_grouping || has_order_by;

        // The numerically lower QEP_TAB is often (if not always) the smaller
        // input, so use that as the build input.
        iterator = NewIterator<HashJoinIterator>(
            thd, move(iterator), left_tables, move(table_iterator), qep_tab,
            thd->variables.join_buff_size, hash_join_conditions,
            allow_spill_to_disk);
        SetCostOnHashJoinIterator(*thd->cost_model(), qep_tab->position(),
                                  iterator.get());

        // Attach the conditions that must be evaluated after the join, such as
        // non equi-join conditions.
        iterator = PossiblyAttachFilterIterator(
            move(iterator), conditions_after_hash_join, thd);
      } else {
        iterator = CreateNestedLoopIterator(
            thd, move(iterator), move(table_iterator), JoinType::INNER,
            qep_tab->pfs_batch_update(qep_tab->join()));
        SetCostOnNestedLoopIterator(*thd->cost_model(), qep_tab->position(),
                                    iterator.get());
      }
    }
    ++i;

    // If we have any predicates that should be above an outer join,
    // send them upwards.
    for (PendingCondition &cond : predicates_above_join) {
      DBUG_ASSERT(pending_conditions != nullptr);
      pending_conditions->push_back(cond);
    }
  }
  if (is_top_level_outer_join) {
    iterator = PossiblyAttachFilterIterator(move(iterator),
                                            top_level_pending_conditions, thd);

    // We can't have any invalidators here, because there's no later table
    // to invalidate.
    DBUG_ASSERT(top_level_pending_invalidators.empty());
  }
  return iterator;
}

void JOIN::create_iterators() {
  DBUG_ASSERT(m_root_iterator == nullptr);

  // 1) Set up the basic RowIterators for accessing each specific table.
  //    This is needed even if we run in pre-iterator executor.
  create_table_iterators();

  if (select_lex->parent_lex->m_sql_cmd != nullptr &&
      select_lex->parent_lex->m_sql_cmd->using_secondary_storage_engine()) {
    return;
  }

  // 2) If supported by the implemented iterators, we also create the
  //    composite iterators combining the row from each table.
  unique_ptr_destroy_only<RowIterator> iterator =
      create_root_iterator_for_join();
  if (iterator == nullptr) {
    // The query is not supported by the iterator executor.
    DBUG_ASSERT(!select_lex->parent_lex->force_iterator_executor);
    return;
  }

  iterator = attach_iterators_for_having_and_limit(move(iterator));
  iterator->set_join_for_explain(this);
  m_root_iterator = move(iterator);
}

void JOIN::create_table_iterators() {
  for (unsigned table_idx = const_tables; table_idx < tables; ++table_idx) {
    QEP_TAB *qep_tab = &this->qep_tab[table_idx];
    if (qep_tab->position() == nullptr) {
      continue;
    }

    /*
      Create the specific RowIterators, including any specific
      RowIterator for the pushed queries.
    */
    qep_tab->pick_table_access_method();

    if (qep_tab->filesort) {
      unique_ptr_destroy_only<RowIterator> iterator = move(qep_tab->iterator);

      // Evaluate any conditions before sorting entire row set.
      if (qep_tab->condition()) {
        vector<Item *> predicates_below_join;
        vector<PendingCondition> predicates_above_join;
        SplitConditions(qep_tab->condition(), &predicates_below_join,
                        &predicates_above_join);

        iterator = PossiblyAttachFilterIterator(move(iterator),
                                                predicates_below_join, thd);
        qep_tab->mark_condition_as_pushed_to_sort();
      }

      // Wrap the chosen RowIterator in a SortingIterator, so that we get
      // sorted results out.
      qep_tab->iterator = NewIterator<SortingIterator>(
          qep_tab->join()->thd, qep_tab->filesort, move(iterator),
          &qep_tab->join()->examined_rows);
      qep_tab->table()->sorting_iterator =
          down_cast<SortingIterator *>(qep_tab->iterator->real_iterator());
    }
  }
}

unique_ptr_destroy_only<RowIterator> JOIN::create_root_iterator_for_join() {
  if (select_count) {
    return unique_ptr_destroy_only<RowIterator>(
        new (thd->mem_root) UnqualifiedCountIterator(thd, this));
  }

  struct MaterializeOperation {
    QEP_TAB *temporary_qep_tab;
    enum {
      MATERIALIZE,
      AGGREGATE_THEN_MATERIALIZE,
      AGGREGATE_INTO_TMP_TABLE,
      WINDOWING_FUNCTION
    } type;
  };
  vector<MaterializeOperation> final_materializations;

  // There are only three specific cases where we need to use the pre-iterator
  // executor:
  //
  //   1. We have a child query expression that needs to run in it.
  //   2. We have BNL that we cannot rewrite to hash join (non equi-join
  //      condition).
  //   3. We have join buffering (BNL/BKA) that is not an inner join
  //      (outer join, semijoin, or antijoin).
  //
  // If either #1, #2 or #3 is detected, revert to the pre-iterator executor.
  for (unsigned table_idx = const_tables; table_idx < tables; ++table_idx) {
    QEP_TAB *qep_tab = &this->qep_tab[table_idx];
    if (qep_tab->materialize_table == join_materialize_derived) {
      // If we have a derived table that can be processed by
      // the iterator executor, MaterializeIterator can deal with it.
      SELECT_LEX_UNIT *unit = qep_tab->table_ref->derived_unit();
      if (unit->root_iterator() == nullptr &&
          !unit->unfinished_materialization()) {
        // Runs in the pre-iterator executor.
        return nullptr;
      }
    }
    if (qep_tab->next_select == sub_select_op) {
      QEP_operation *op = qep_tab[1].op;
      if (op->type() != QEP_operation::OT_TMP_TABLE) {
        if (qep_tab[1].last_inner() != NO_PLAN_IDX ||
            qep_tab[1].firstmatch_return != NO_PLAN_IDX) {
          // Outer or semijoin. Not supported for BNL/BKA yet!
          return nullptr;
        }
        // See if it's possible to replace the BNL with a hash join,
        // or if it's BKA.
        const JOIN_CACHE *join_cache = down_cast<const JOIN_CACHE *>(op);
        if (!join_cache->can_be_replaced_with_hash_join() &&
            join_cache->cache_type() != JOIN_CACHE::ALG_BKA) {
          return nullptr;
        }
      } else {
        DBUG_ASSERT(op->type() == QEP_operation::OT_TMP_TABLE);
        QEP_tmp_table *tmp_op = down_cast<QEP_tmp_table *>(op);
        if (tmp_op->get_write_func() == end_write) {
          DBUG_ASSERT(need_tmp_before_win);
          final_materializations.push_back(MaterializeOperation{
              qep_tab + 1, MaterializeOperation::MATERIALIZE});
        } else if (tmp_op->get_write_func() == end_write_group) {
          final_materializations.push_back(MaterializeOperation{
              qep_tab + 1, MaterializeOperation::AGGREGATE_THEN_MATERIALIZE});
        } else if (tmp_op->get_write_func() == end_update) {
          final_materializations.push_back(MaterializeOperation{
              qep_tab + 1, MaterializeOperation::AGGREGATE_INTO_TMP_TABLE});
        } else if (tmp_op->get_write_func() == end_write_wf) {
          final_materializations.push_back(MaterializeOperation{
              qep_tab + 1, MaterializeOperation::WINDOWING_FUNCTION});
        }
      }
    }
  }

  // OK, so we're good. Go through the tables and make the join iterators.
  for (unsigned table_idx = const_tables; table_idx < tables; ++table_idx) {
    QEP_TAB *qep_tab = &this->qep_tab[table_idx];
    if (qep_tab->position() == nullptr) {
      continue;
    }

    // We don't use these in the iterator executor (except for figuring out
    // which conditions are join conditions and which are from WHERE),
    // so we remove them whenever we can. However, we don't prune them
    // entirely from the query tree, so they may be left within e.g.
    // e.g. sub-conditions of ORs. Open up the conditions so that we don't
    // have conditions that are disabled during execution.
    qep_tab->not_null_compl = true;
    qep_tab->found = true;
  }

  unique_ptr_destroy_only<RowIterator> iterator;
  if (select_lex->is_table_value_constructor) {
    best_rowcount = select_lex->row_value_list->size();
    iterator = NewIterator<TableValueConstructorIterator>(
        thd, &examined_rows, *select_lex->row_value_list, fields);
  } else if (const_tables == primary_tables) {
    // Only const tables, so add a fake single row to join in all
    // the const tables (only inner-joined tables are promoted to
    // const tables in the optimizer).
    iterator = NewIterator<FakeSingleRowIterator>(thd, &examined_rows);
    if (where_cond != nullptr) {
      iterator = PossiblyAttachFilterIterator(move(iterator),
                                              vector<Item *>{where_cond}, thd);
    }

    // Surprisingly enough, we can specify that the const tables are
    // to be dumped immediately to a temporary table. If we don't do this,
    // we risk that there are fields that are not copied correctly
    // (tmp_table_param contains copy_funcs we'd otherwise miss).
    if (const_tables > 0) {
      QEP_TAB *qep_tab = &this->qep_tab[const_tables];
      if (qep_tab[-1].next_select == sub_select_op) {
        // We don't support join buffering, but we do support temporary tables.
        QEP_operation *op = qep_tab->op;
        if (op->type() != QEP_operation::OT_TMP_TABLE) {
          return nullptr;
        }
        DBUG_ASSERT(down_cast<QEP_tmp_table *>(op)->get_write_func() ==
                    end_write);
        qep_tab->iterator.reset();
        join_setup_iterator(qep_tab);
        qep_tab->table()->alias = "<temporary>";
        iterator = NewIterator<MaterializeIterator>(
            thd, move(iterator), qep_tab->tmp_table_param, qep_tab->table(),
            move(qep_tab->iterator), /*cte=*/nullptr, select_lex->select_number,
            unit, this, qep_tab->ref_item_slice,
            /*copy_fields_and_items=*/true,
            /*rematerialize=*/true,
            qep_tab->tmp_table_param->end_write_records);
      }
    }
  } else {
    qep_tab_map unhandled_duplicates = 0;
    iterator = ConnectJoins(const_tables, primary_tables, qep_tab, thd,
                            TOP_LEVEL, nullptr, nullptr, &unhandled_duplicates);

    // If there were any weedouts that we had to drop during ConnectJoins()
    // (ie., the join left some tables that were supposed to be deduplicated
    // but were not), handle them now at the very end.
    if (unhandled_duplicates != 0) {
      iterator = CreateWeedoutIteratorForTables(
          thd, unhandled_duplicates, qep_tab, primary_tables, move(iterator));
    }
  }

  // Deal with any materialization happening at the end (typically for sorting,
  // grouping or distinct).
  for (MaterializeOperation materialize_op : final_materializations) {
    QEP_TAB *qep_tab = materialize_op.temporary_qep_tab;

    if (materialize_op.type ==
        MaterializeOperation::AGGREGATE_THEN_MATERIALIZE) {
      // Aggregate as we go, with output into a temporary table.
      // (We can also aggregate as we go after the materialization step;
      // see below. We won't be aggregating twice, though.)
      if (qep_tab->tmp_table_param->precomputed_group_by) {
        DBUG_ASSERT(rollup.state == ROLLUP::STATE_NONE);
        iterator = NewIterator<PrecomputedAggregateIterator>(
            thd, move(iterator), this, qep_tab->tmp_table_param,
            qep_tab->ref_item_slice);
      } else {
        iterator = NewIterator<AggregateIterator>(
            thd, move(iterator), this, qep_tab->tmp_table_param,
            qep_tab->ref_item_slice, rollup.state != ROLLUP::STATE_NONE);
      }
    }

    // Attach HAVING if needed (it's put on the QEP_TAB and not on the JOIN if
    // we have a temporary table) and we've done all aggregation.
    //
    // FIXME: If the HAVING condition is an alias (a MySQL-specific extension),
    // it could be evaluated twice; once for the condition, and again for the
    // copying into the table. This was originally partially fixed by moving
    // the HAVING into qep_tab->condition() instead, although this makes the
    // temporary table larger than it needs to be, and is not a legal case in
    // the presence of SELECT DISTINCT. (The main.having test has a few tests
    // for this.) Later, it was completely fixed for the old executor,
    // by evaluating the filter against the temporary table row (switching
    // slices), although the conditional move into qep_tab->condition(),
    // which was obsolete for the old executor after said fix, was never
    // removed. See if we can get this fixed in the new executor as well,
    // and then remove the code that moves HAVING onto qep_tab->condition().
    if (qep_tab->having != nullptr &&
        materialize_op.type != MaterializeOperation::AGGREGATE_INTO_TMP_TABLE) {
      iterator =
          NewIterator<FilterIterator>(thd, move(iterator), qep_tab->having);
    }

    // Sorting comes after the materialization (which we're about to add),
    // and should be shown as such. Prevent join_setup_iterator
    // from adding it to the result iterator; we'll add it ourselves below.
    //
    // Note that this would break the query if run by the old executor!
    Filesort *filesort = qep_tab->filesort;
    qep_tab->filesort = nullptr;

    Filesort *dup_filesort = nullptr;
    bool limit_1_for_dup_filesort = false;

    // The pre-iterator executor does duplicate removal by going into the
    // temporary table and actually deleting records, using a hash table for
    // smaller tables and an O(n²) algorithm for large tables. This kind of
    // deletion is not cleanly representable in the iterator model, so we do it
    // using a duplicate-removing filesort instead, which has a straight-up
    // O(n log n) cost.
    if (qep_tab->needs_duplicate_removal) {
      bool all_order_fields_used;
      ORDER *order = create_order_from_distinct(
          thd, ref_items[qep_tab->ref_item_slice], this->order, fields_list,
          /*skip_aggregates=*/false, /*convert_bit_fields_to_long=*/false,
          &all_order_fields_used);
      if (order == nullptr) {
        // Only const fields.
        limit_1_for_dup_filesort = true;
      } else {
        bool force_sort_positions = false;
        if (all_order_fields_used) {
          // The ordering for DISTINCT already gave us the right sort order,
          // so no need to sort again.
          filesort = nullptr;
        } else if (filesort != nullptr && !filesort->using_addon_fields()) {
          // We have the rather unusual situation here that we have two sorts
          // directly after each other, with no temporary table in-between,
          // and filesort expects to be able to refer to rows by their position.
          // Usually, the sort for DISTINCT would be a superset of the sort for
          // ORDER BY, but not always (e.g. when sorting by some expression),
          // so we could end up in a situation where the first sort is by addon
          // fields and the second one is by positions.
          //
          // Thus, in this case, we force the first sort to be by positions,
          // so that the result comes from SortFileIndirectIterator or
          // SortBufferIndirectIterator. These will both position the cursor
          // on the underlying temporary table correctly before returning it,
          // so that the successive filesort will save the right position
          // for the row.
          force_sort_positions = true;
        }

        dup_filesort = new (thd->mem_root) Filesort(
            thd, qep_tab, order, HA_POS_ERROR, /*force_stable_sort=*/false,
            /*remove_duplicates=*/true, force_sort_positions);
      }
    }

    qep_tab->iterator.reset();
    join_setup_iterator(qep_tab);

    qep_tab->table()->alias = "<temporary>";

    if (materialize_op.type == MaterializeOperation::WINDOWING_FUNCTION) {
      if (qep_tab->tmp_table_param->m_window->needs_buffering()) {
        iterator = NewIterator<BufferingWindowingIterator>(
            thd, move(iterator), qep_tab->tmp_table_param, this,
            qep_tab->ref_item_slice);
      } else {
        iterator = NewIterator<WindowingIterator>(
            thd, move(iterator), qep_tab->tmp_table_param, this,
            qep_tab->ref_item_slice);
      }
      if (!qep_tab->tmp_table_param->m_window_short_circuit) {
        iterator = NewIterator<MaterializeIterator>(
            thd, move(iterator), qep_tab->tmp_table_param, qep_tab->table(),
            move(qep_tab->iterator), /*cte=*/nullptr, select_lex->select_number,
            unit, this,
            /*ref_slice=*/-1, /*copy_fields_and_items_in_materialize=*/false,
            qep_tab->rematerialize, tmp_table_param.end_write_records);
      }
    } else if (materialize_op.type ==
               MaterializeOperation::AGGREGATE_INTO_TMP_TABLE) {
      iterator = NewIterator<TemptableAggregateIterator>(
          thd, move(iterator), qep_tab->tmp_table_param, qep_tab->table(),
          move(qep_tab->iterator), select_lex, this, qep_tab->ref_item_slice);
      if (qep_tab->having != nullptr) {
        iterator =
            NewIterator<FilterIterator>(thd, move(iterator), qep_tab->having);
      }
    } else {
      // MATERIALIZE or AGGREGATE_THEN_MATERIALIZE.
      bool copy_fields_and_items =
          (materialize_op.type !=
           MaterializeOperation::AGGREGATE_THEN_MATERIALIZE);

      // If we don't need the row IDs, and don't have some sort of deduplication
      // (e.g. for GROUP BY) on the table, filesort can take in the data
      // directly, without going through a temporary table.
      //
      // If there are two sorts, we need row IDs if either one of them needs it.
      // Above, we've set up so that the innermost sort (for DISTINCT) always
      // needs row IDs if the outermost (for ORDER BY) does. The other way is
      // fine, though; if the innermost needs row IDs but the outermost doesn't,
      // then we can use row IDs here (ie., no streaming) but drop them in the
      // outer sort. Thus, we check the using_addon_fields() flag on the
      // innermost.
      //
      // TODO: If the sort order is suitable (or extendable), we could take over
      // the deduplicating responsibilities of the temporary table and activate
      // this mode even if qep_tab->temporary_table_deduplicates() is set.
      Filesort *first_sort = dup_filesort != nullptr ? dup_filesort : filesort;
      if (first_sort != nullptr && first_sort->using_addon_fields() &&
          !qep_tab->temporary_table_deduplicates()) {
        iterator = NewIterator<StreamingIterator>(
            thd, move(iterator), qep_tab->tmp_table_param, qep_tab->table(),
            copy_fields_and_items);
      } else {
        iterator = NewIterator<MaterializeIterator>(
            thd, move(iterator), qep_tab->tmp_table_param, qep_tab->table(),
            move(qep_tab->iterator), /*cte=*/nullptr, select_lex->select_number,
            unit, this, qep_tab->ref_item_slice, copy_fields_and_items,
            /*rematerialize=*/true,
            qep_tab->tmp_table_param->end_write_records);
      }

      // NOTE: There's no need to call join->add_materialize_iterator(),
      // as this iterator always rematerializes anyway.
    }

    if (qep_tab->condition() != nullptr) {
      iterator = NewIterator<FilterIterator>(thd, move(iterator),
                                             qep_tab->condition());
      qep_tab->mark_condition_as_pushed_to_sort();
    }

    if (limit_1_for_dup_filesort) {
      iterator = NewIterator<LimitOffsetIterator>(
          thd, move(iterator), /*select_limit_cnt=*/1, /*offset_limit_cnt=*/0,
          /*count_all_rows=*/false, /*skipped_rows=*/nullptr);
    } else if (dup_filesort != nullptr) {
      iterator = NewIterator<SortingIterator>(thd, dup_filesort, move(iterator),
                                              &examined_rows);
      qep_tab->table()->duplicate_removal_iterator =
          down_cast<SortingIterator *>(iterator->real_iterator());
    }
    if (filesort != nullptr) {
      iterator = NewIterator<SortingIterator>(thd, filesort, move(iterator),
                                              &examined_rows);
      qep_tab->table()->sorting_iterator =
          down_cast<SortingIterator *>(iterator->real_iterator());
    }
  }

  // See if we need to aggregate data in the final step. Note that we can
  // _not_ rely on streaming_aggregation, as it can be changed from false
  // to true during optimization, and depending on when it was set, it could
  // either mean to aggregate into a temporary table or aggregate on final
  // send.
  bool do_aggregate;
  if (primary_tables == 0 && tmp_tables == 0) {
    // We can't check qep_tab since there's no table, but in this specific case,
    // it is safe to call get_end_select_func() at this point.
    do_aggregate = (get_end_select_func() == end_send_group);
  } else {
    // Note that tmp_table_param.precomputed_group_by can be set even if we
    // don't actually have any grouping (e.g., make_tmp_tables_info() does this
    // even if there are no temporary tables made).
    do_aggregate = (qep_tab[primary_tables + tmp_tables - 1].next_select ==
                    end_send_group) ||
                   ((grouped || group_optimized_away) &&
                    tmp_table_param.precomputed_group_by);
  }
  if (do_aggregate) {
    // Aggregate as we go, with output into a special slice of the same table.
    DBUG_ASSERT(streaming_aggregation || tmp_table_param.precomputed_group_by);
#ifndef DBUG_OFF
    for (MaterializeOperation materialize_op : final_materializations) {
      DBUG_ASSERT(materialize_op.type !=
                  MaterializeOperation::AGGREGATE_THEN_MATERIALIZE);
    }
#endif
    if (tmp_table_param.precomputed_group_by) {
      iterator = NewIterator<PrecomputedAggregateIterator>(
          thd, move(iterator), this, &tmp_table_param,
          REF_SLICE_ORDERED_GROUP_BY);
      DBUG_ASSERT(rollup.state == ROLLUP::STATE_NONE);
    } else {
      iterator = NewIterator<AggregateIterator>(
          thd, move(iterator), this, &tmp_table_param,
          REF_SLICE_ORDERED_GROUP_BY, rollup.state != ROLLUP::STATE_NONE);
    }
  }

  return iterator;
}

unique_ptr_destroy_only<RowIterator>
JOIN::attach_iterators_for_having_and_limit(
    unique_ptr_destroy_only<RowIterator> iterator) {
  // Attach HAVING and LIMIT if needed.
  // NOTE: We can have HAVING even without GROUP BY, although it's not very
  // useful.
  if (having_cond != nullptr) {
    iterator = NewIterator<FilterIterator>(thd, move(iterator), having_cond);
  }

  // Note: For select_count, LIMIT 0 is handled in JOIN::optimize() for the
  // common case, but not for CALC_FOUND_ROWS. OFFSET also isn't handled there.
  if (unit->select_limit_cnt != HA_POS_ERROR || unit->offset_limit_cnt != 0) {
    iterator = NewIterator<LimitOffsetIterator>(
        thd, move(iterator), unit->select_limit_cnt, unit->offset_limit_cnt,
        calc_found_rows, &send_records);
  }

  return iterator;
}

// Used only in the specific, odd case of a UNION between a non-iterator
// and an iterator query block.
static int ExecuteIteratorQuery(JOIN *join) {
  // The outermost LimitOffsetIterator, if any, will increment send_records for
  // each record skipped by OFFSET. This is needed because LIMIT 50 OFFSET 10
  // with no SQL_CALC_FOUND_ROWS is defined to return 60, not 50 (even though
  // it's not necessarily the most useful definition).
  join->send_records = 0;

  join->thd->get_stmt_da()->reset_current_row_for_condition();
  if (join->root_iterator()->Init()) {
    return 1;
  }

  PFSBatchMode pfs_batch_mode(join->root_iterator());
  for (;;) {
    int error = join->root_iterator()->Read();

    DBUG_EXECUTE_IF("bug13822652_1", join->thd->killed = THD::KILL_QUERY;);

    if (error > 0 || (join->thd->is_error()))  // Fatal error
      return 1;
    else if (error < 0)
      break;
    else if (join->thd->killed)  // Aborted by user
    {
      join->thd->send_kill_message();
      return -1;
    }

    ++join->send_records;

    if (join->select_lex->query_result()->send_data(join->thd, *join->fields)) {
      return 1;
    }
    join->thd->get_stmt_da()->inc_current_row_for_condition();
  }
  return 0;
}

/**
  Make a join of all tables and write it on socket or to table.

  @retval
    0  if ok
  @retval
    1  if error is sent
  @retval
    -1  if error should be sent
*/

static int do_select(JOIN *join) {
  int rc = 0;
  enum_nested_loop_state error = NESTED_LOOP_OK;
  DBUG_TRACE;

  join->send_records = 0;
  THD *thd = join->thd;

  if (join->root_iterator() != nullptr) {
    error =
        ExecuteIteratorQuery(join) == 0 ? NESTED_LOOP_OK : NESTED_LOOP_ERROR;
  } else if (join->select_count) {
    QEP_TAB *qep_tab = join->qep_tab;
    error = end_send_count(join, qep_tab);
  } else if (join->plan_is_const() && !join->need_tmp_before_win) {
    // Special code for dealing with queries that don't need to
    // read any tables.

    Next_select_func end_select = join->get_end_select_func();
    /*
      HAVING will be checked after processing aggregate functions,
      But WHERE should checkd here (we alredy have read tables)

      @todo: consider calling end_select instead of duplicating code
    */
    if (!join->where_cond || join->where_cond->val_int()) {
      // HAVING will be checked by end_select
      error = (*end_select)(join, 0, false);
      if (error >= NESTED_LOOP_OK) error = (*end_select)(join, 0, true);

      // This is a special case because const-only plans don't go through
      // iterators, which would normally be responsible for incrementing
      // examined_rows.
      join->examined_rows++;
      DBUG_ASSERT(join->examined_rows <= 1);
    } else if (join->send_row_on_empty_set()) {
      table_map save_nullinfo = 0;

      // Calculate aggregate functions for no rows
      for (Item &item : *join->fields) {
        item.no_rows_in_result();
      }

      /*
        Mark tables as containing only NULL values for processing
        the HAVING clause and for send_data().
        Calculate a set of tables for which NULL values need to be restored
        after sending data.
      */
      if (join->clear_fields(&save_nullinfo))
        error = NESTED_LOOP_ERROR;
      else {
        if (having_is_true(join->having_cond) &&
            join->should_send_current_row())
          rc = join->select_lex->query_result()->send_data(thd, *join->fields);

        // Restore NULL values if needed.
        if (save_nullinfo) join->restore_fields(save_nullinfo);
      }
    }
    /*
      An error can happen when evaluating the conds
      (the join condition and piece of where clause
      relevant to this join table).
    */
    if (thd->is_error()) error = NESTED_LOOP_ERROR;
  } else {
    // Pre-iterator query execution path.
    DBUG_ASSERT(join->primary_tables);

    QEP_TAB *qep_tab = join->qep_tab + join->const_tables;
    error = join->first_select(join, qep_tab, false);
    if (error >= NESTED_LOOP_OK)
      error = join->first_select(join, qep_tab, true);
  }

  thd->current_found_rows = join->send_records;
  /*
    For "order by with limit", we cannot rely on send_records, but need
    to use the rowcount read originally into the join_tab applying the
    filesort. There cannot be any post-filtering conditions, nor any
    following join_tabs in this case, so this rowcount properly represents
    the correct number of qualifying rows.
  */
  if (join->qep_tab && join->order) {
    // Save # of found records prior to cleanup
    QEP_TAB *sort_tab;
    uint const_tables = join->const_tables;

    // Take record count from first non constant table or from last tmp table
    if (join->tmp_tables > 0)
      sort_tab = &join->qep_tab[join->primary_tables + join->tmp_tables - 1];
    else {
      DBUG_ASSERT(!join->plan_is_const());
      sort_tab = &join->qep_tab[const_tables];
    }
    if (sort_tab->filesort && join->calc_found_rows &&
        sort_tab->filesort->sortorder &&
        sort_tab->filesort->limit != HA_POS_ERROR) {
      thd->current_found_rows = sort_tab->records();
    }
  }

  if (error != NESTED_LOOP_OK) rc = -1;

  if (!join->select_lex->is_recursive() ||
      join->select_lex->master_unit()->got_all_recursive_rows) {
    /*
      The following will unlock all cursors if the command wasn't an
      update command
    */
    join->join_free();  // Unlock all cursors
    if (error == NESTED_LOOP_OK) {
      /*
        Sic: this branch works even if rc != 0, e.g. when
        send_data above returns an error.
      */
      if (join->select_lex->query_result()->send_eof(thd))
        rc = 1;  // Don't send error
      DBUG_PRINT("info", ("%ld records output", (long)join->send_records));
    }
  }

  rc = thd->is_error() ? -1 : rc;
#ifndef DBUG_OFF
  if (rc) {
    DBUG_PRINT("error", ("Error: do_select() failed"));
  }
#endif
  return rc;
}

/**
  @brief Accumulate full or partial join result in operation and send
  operation's result further.

  @param join  pointer to the structure providing all context info for the query
  @param qep_tab the QEP_TAB object to which the operation is attached
  @param end_of_records  true <=> all records were accumulated, send them
  further

  @details
  This function accumulates records, one by one, in QEP operation's buffer by
  calling op->put_record(). When there is no more records to save, in this
  case the end_of_records argument == true, function tells QEP operation to
  send records further by calling op->send_records().
  When all records are sent this function passes 'end_of_records' signal
  further by calling sub_select() with end_of_records argument set to
  true. After that op->end_send() is called to tell QEP operation that
  it could end internal buffer scan.

  @note
  This function is not expected to be called when dynamic range scan is
  used to scan join_tab because join cache is disabled for such scan
  and range scans aren't used for tmp tables.
  @see setup_join_buffering
  For caches the function implements the algorithmic schema for both
  Blocked Nested Loop Join and Batched Key Access Join. The difference can
  be seen only at the level of of the implementation of the put_record and
  send_records virtual methods for the cache object associated with the
  join_tab.

  @return
    return one of enum_nested_loop_state.
*/

enum_nested_loop_state sub_select_op(JOIN *join, QEP_TAB *qep_tab,
                                     bool end_of_records) {
  DBUG_TRACE;

  if (join->thd->killed) {
    /* The user has aborted the execution of the query */
    join->thd->send_kill_message();
    return NESTED_LOOP_KILLED;
  }

  enum_nested_loop_state rc;
  QEP_operation *op = qep_tab->op;

  /* This function cannot be called if qep_tab has no associated operation */
  DBUG_ASSERT(op != NULL);
  if (end_of_records) {
    rc = op->end_send();
    if (rc >= NESTED_LOOP_OK) rc = sub_select(join, qep_tab, end_of_records);
    return rc;
  }
  if (qep_tab->prepare_scan()) return NESTED_LOOP_ERROR;

  /*
    setup_join_buffering() disables join buffering if QS_DYNAMIC_RANGE is
    enabled.
  */
  DBUG_ASSERT(!qep_tab->dynamic_range());

  rc = op->put_record();

  return rc;
}

/**
  Retrieve records ends with a given beginning from the result of a join.

    For a given partial join record consisting of records from the tables
    preceding the table join_tab in the execution plan, the function
    retrieves all matching full records from the result set and
    send them to the result set stream.

  @note
    The function effectively implements the  final (n-k) nested loops
    of nested loops join algorithm, where k is the ordinal number of
    the join_tab table and n is the total number of tables in the join query.
    It performs nested loops joins with all conjunctive predicates from
    the where condition pushed as low to the tables as possible.
    E.g. for the query
    @code
      SELECT * FROM t1,t2,t3
      WHERE t1.a=t2.a AND t2.b=t3.b AND t1.a BETWEEN 5 AND 9
    @endcode
    the predicate (t1.a BETWEEN 5 AND 9) will be pushed to table t1,
    given the selected plan prescribes to nest retrievals of the
    joined tables in the following order: t1,t2,t3.
    A pushed down predicate are attached to the table which it pushed to,
    at the field join_tab->cond.
    When executing a nested loop of level k the function runs through
    the rows of 'join_tab' and for each row checks the pushed condition
    attached to the table.
    If it is false the function moves to the next row of the
    table. If the condition is true the function recursively executes (n-k-1)
    remaining embedded nested loops.
    The situation becomes more complicated if outer joins are involved in
    the execution plan. In this case the pushed down predicates can be
    checked only at certain conditions.
    Suppose for the query
    @code
      SELECT * FROM t1 LEFT JOIN (t2,t3) ON t3.a=t1.a
      WHERE t1>2 AND (t2.b>5 OR t2.b IS NULL)
    @endcode
    the optimizer has chosen a plan with the table order t1,t2,t3.
    The predicate P1=t1>2 will be pushed down to the table t1, while the
    predicate P2=(t2.b>5 OR t2.b IS NULL) will be attached to the table
    t2. But the second predicate can not be unconditionally tested right
    after a row from t2 has been read. This can be done only after the
    first row with t3.a=t1.a has been encountered.
    Thus, the second predicate P2 is supplied with a guarded value that are
    stored in the field 'found' of the first inner table for the outer join
    (table t2). When the first row with t3.a=t1.a for the  current row
    of table t1  appears, the value becomes true. For now on the predicate
    is evaluated immediately after the row of table t2 has been read.
    When the first row with t3.a=t1.a has been encountered all
    conditions attached to the inner tables t2,t3 must be evaluated.
    Only when all of them are true the row is sent to the output stream.
    If not, the function returns to the lowest nest level that has a false
    attached condition.
    The predicates from on expressions are also pushed down. If in the
    the above example the on expression were (t3.a=t1.a AND t2.a=t1.a),
    then t1.a=t2.a would be pushed down to table t2, and without any
    guard.
    If after the run through all rows of table t2, the first inner table
    for the outer join operation, it turns out that no matches are
    found for the current row of t1, then current row from table t1
    is complemented by nulls  for t2 and t3. Then the pushed down predicates
    are checked for the composed row almost in the same way as it had
    been done for the first row with a match. The only difference is
    the predicates from on expressions are not checked.

  @par
  @b IMPLEMENTATION
  @par
    The function forms output rows for a current partial join of k
    tables tables recursively.
    For each partial join record ending with a certain row from
    join_tab it calls sub_select that builds all possible matching
    tails from the result set.
    To be able  check predicates conditionally items of the class
    Item_func_trig_cond are employed.
    An object of  this class is constructed from an item of class COND
    and a pointer to a guarding boolean variable.
    When the value of the guard variable is true the value of the object
    is the same as the value of the predicate, otherwise it's just returns
    true.

    Testing predicates at the optimal time can be tricky, especially for
    outer joins. Consider the following query:

    @code
        SELECT * FROM t1
                      LEFT JOIN
                      (t2 JOIN t3 ON t2.a=t3.a)
                      ON t1.a=t2.a
           WHERE t2.b=5 OR t2.b IS NULL
    @endcode

    (The OR ... IS NULL is solely so that the outer join can not be rewritten
    to an inner join.)

    Suppose the chosen execution plan dictates the order t1,t2,t3,
    and suppose that we have found a row t1 and are scanning t2.
    We cannot filter rows from t2 as we see them, as the LEFT JOIN needs
    to know that there existed at least one (t2,t3) tuple matching t1,
    so that it should not synthesize a NULL-complemented row.

    However, once we have a matching t3, we can activate the predicate
    (t2.b=5 OR t2.b IS NULL). (Note that it does not refer to t3 at all.)
    If it fails, we should immediately stop scanning t3 and go back to
    scanning t2 (or in general, arbitrarily early), which is done by setting
    the field 'return_tab' of the JOIN.

    Now consider a similar but more complex case:

    @code
        SELECT * FROM t1
                      LEFT JOIN
                      (t2, t3 LEFT JOIN (t4,t5) ON t5.a=t3.a)
                      ON t4.a=t2.a
           WHERE (t2.b=5 OR t2.b IS NULL) AND (t4.b=2 OR t4.b IS NULL)
    @endcode

    In order not to re-evaluate the predicates that were already evaluated
    as attached pushed down predicates, a pointer to the the first
    most inner unmatched table is maintained in join_tab->first_unmatched.
    Thus, when the first row from t5 with t5.a=t3.a is found
    this pointer for t5 is changed from t4 to t2.

    @par
    @b STRUCTURE @b NOTES
    @par
    join_tab->first_unmatched points always backwards to the first inner
    table of the embedding nested join, if any.

  @param join      pointer to the structure providing all context info for
                   the query
  @param qep_tab  the first next table of the execution plan to be retrieved
  @param end_of_records  true when we need to perform final steps of retreival

  @return
    return one of enum_nested_loop_state, except NESTED_LOOP_NO_MORE_ROWS.
*/

enum_nested_loop_state sub_select(JOIN *join, QEP_TAB *const qep_tab,
                                  bool end_of_records) {
  DBUG_TRACE;

  TABLE *const table = qep_tab->table();

  /*
    Enable the items which one should use if one wants to evaluate anything
    (e.g. functions in WHERE, HAVING) involving columns of this table.
  */
  Switch_ref_item_slice slice_switch(join, qep_tab->ref_item_slice);

  if (end_of_records) {
    enum_nested_loop_state nls =
        (*qep_tab->next_select)(join, qep_tab + 1, end_of_records);

    return nls;
  }

  if (qep_tab->prepare_scan()) return NESTED_LOOP_ERROR;

  if (qep_tab->starts_weedout()) {
    do_sj_reset(qep_tab->flush_weedout_table);
  }

  const plan_idx qep_tab_idx = qep_tab->idx();
  join->return_tab = qep_tab_idx;
  qep_tab->not_null_compl = true;
  qep_tab->found_match = false;

  if (qep_tab->last_inner() != NO_PLAN_IDX) {
    /* qep_tab is the first inner table for an outer join operation. */

    /* Set initial state of guard variables for this table.*/
    qep_tab->found = false;

    /* Set first_unmatched for the last inner table of this group */
    QEP_AT(qep_tab, last_inner()).first_unmatched = qep_tab_idx;
  }
  if (qep_tab->do_firstmatch() || qep_tab->do_loosescan()) {
    /*
      qep_tab is the first table of a LooseScan range, or has a "jump"
      address in a FirstMatch range.
      Reset the matching for this round of execution.
    */
    QEP_AT(qep_tab, match_tab).found_match = false;
  }

  join->thd->get_stmt_da()->reset_current_row_for_condition();

  enum_nested_loop_state rc = NESTED_LOOP_OK;

  bool in_first_read = true;
  const bool is_recursive_ref = qep_tab->table_ref->is_recursive_reference();
  // Init these 3 variables even if used only if is_recursive_ref is true.
  const ha_rows *recursive_row_count = nullptr;
  ha_rows recursive_row_count_start = 0;
  bool count_iterations = false;

  if (is_recursive_ref) {
    // See also Recursive_executor's documentation
    if (join->unit->got_all_recursive_rows) return rc;
    // The recursive CTE algorithm requires a table scan.
    DBUG_ASSERT(qep_tab->type() == JT_ALL);
    in_first_read = !table->file->inited;
    /*
      Tmp table which we're reading is bound to this result, and we'll be
      checking its row count frequently:
    */
    recursive_row_count =
        join->unit->recursive_result(join->select_lex)->row_count();
    // How many rows we have already read; defines start of iteration.
    recursive_row_count_start = qep_tab->m_fetched_rows;
    // Execution of fake_select_lex doesn't count for the user:
    count_iterations = join->select_lex != join->unit->fake_select_lex;
  }

  // NOTE: If we are reading from a SortingIterator, it will set up batch mode
  // by itself, so don't activate it here. (It won't be activated when reading
  // the records back, though, only during the sort itself.)
  const bool pfs_batch_update =
      qep_tab->filesort == nullptr && qep_tab->pfs_batch_update(join);
  if (pfs_batch_update) table->file->start_psi_batch_mode();

  RowIterator *iterator = qep_tab->iterator.get();
  while (rc == NESTED_LOOP_OK && join->return_tab >= qep_tab_idx) {
    int error;

    if (is_recursive_ref && qep_tab->m_fetched_rows >=
                                *recursive_row_count) {  // We have read all
                                                         // that's in the tmp
                                                         // table: signal EOF.
      error = -1;
      break;
    }

    if (in_first_read) {
      in_first_read = false;
      if (iterator->Init()) {
        rc = NESTED_LOOP_ERROR;
        break;
      }
    }
    error = iterator->Read();

    DBUG_EXECUTE_IF("bug13822652_1", join->thd->killed = THD::KILL_QUERY;);

    if (error > 0 || (join->thd->is_error()))  // Fatal error
      rc = NESTED_LOOP_ERROR;
    else if (error < 0)
      break;
    else if (join->thd->killed)  // Aborted by user
    {
      join->thd->send_kill_message();
      rc = NESTED_LOOP_KILLED;
    } else {
      qep_tab->m_fetched_rows++;
      if (is_recursive_ref &&
          qep_tab->m_fetched_rows == recursive_row_count_start + 1) {
        /*
          We have just read one row further than the set of rows of the
          iteration, so we have actually just entered a new iteration.
        */
        if (count_iterations &&
            ++join->recursive_iteration_count >
                join->thd->variables.cte_max_recursion_depth) {
          my_error(ER_CTE_MAX_RECURSION_DEPTH, MYF(0),
                   join->recursive_iteration_count);
          rc = NESTED_LOOP_ERROR;
          break;
        }
        // This new iteration sees the rows made by the previous one:
        recursive_row_count_start = *recursive_row_count;
      }
      if (qep_tab->rowid_status == NEED_TO_CALL_POSITION_FOR_ROWID) {
        table->file->position(table->record[0]);
      }
      rc = evaluate_join_record(join, qep_tab);
    }
  }

  if (rc == NESTED_LOOP_OK && qep_tab->last_inner() != NO_PLAN_IDX &&
      !qep_tab->found)
    rc = evaluate_null_complemented_join_record(join, qep_tab);

  if (pfs_batch_update) table->file->end_psi_batch_mode();

  return rc;
}

void QEP_TAB::refresh_lateral() {
  /*
    See if some lateral derived table further down on the execution path,
    depends on us. If so, mark it for rematerialization.
    Note that if this lateral DT depends on only const tables, the function
    does nothing as it's not called for const tables; however, the lateral DT
    is materialized once in its prepare_scan() like for a non-lateral DT.
    todo: could this dependency-map idea be reused to decrease the amount of
    execution for JSON_TABLE too? For now, JSON_TABLE is rematerialized every
    time we're about to read it.
  */
  JOIN *j = join();
  DBUG_ASSERT(j->has_lateral && lateral_derived_tables_depend_on_me);
  auto deps = lateral_derived_tables_depend_on_me;
  for (QEP_TAB **tab2 = j->map2qep_tab; deps; tab2++, deps >>= 1) {
    if (deps & 1) (*tab2)->rematerialize = true;
  }
}

/**
  @brief Prepare table to be scanned.

  @details This function is the place to do any work on the table that
  needs to be done before table can be scanned. Currently it
  materializes derived tables and semi-joined subqueries,
  binds buffer for current rowid and removes duplicates if needed.

  @returns false - Ok, true  - error
*/

bool QEP_TAB::prepare_scan() {
  // Check whether materialization is required.
  if (!materialize_table) return false;

  if (table()->materialized) {
    if (!rematerialize) return false;
    if (table()->empty_result_table()) return true;
  }

  // Materialize table prior to reading it
  if ((*materialize_table)(this)) return true;

  if (table_ref && table_ref->is_derived() &&
      table_ref->derived_unit()->m_lateral_deps)
    // no further materialization, unless dependencies change
    rematerialize = false;

  // Bind to the rowid buffer managed by the TABLE object.
  if (copy_current_rowid) copy_current_rowid->bind_buffer(table()->file->ref);

  table()->set_not_started();

  if (needs_duplicate_removal && remove_duplicates()) return true;

  return false;
}

/**
  SemiJoinDuplicateElimination: Weed out duplicate row combinations

  SYNPOSIS
    do_sj_dups_weedout()
      thd    Thread handle
      sjtbl  Duplicate weedout table

  DESCRIPTION
    Try storing current record combination of outer tables (i.e. their
    rowids) in the temporary table. This records the fact that we've seen
    this record combination and also tells us if we've seen it before.

  RETURN
    -1  Error
    1   The row combination is a duplicate (discard it)
    0   The row combination is not a duplicate (continue)
*/

int do_sj_dups_weedout(THD *thd, SJ_TMP_TABLE *sjtbl) {
  int error;
  SJ_TMP_TABLE_TAB *tab = sjtbl->tabs;
  SJ_TMP_TABLE_TAB *tab_end = sjtbl->tabs_end;

  DBUG_TRACE;

  if (sjtbl->is_confluent) {
    if (sjtbl->have_confluent_row)
      return 1;
    else {
      sjtbl->have_confluent_row = true;
      return 0;
    }
  }

  uchar *ptr = sjtbl->tmp_table->visible_field_ptr()[0]->ptr;
  // Put the rowids tuple into table->record[0]:
  // 1. Store the length
  if (((Field_varstring *)(sjtbl->tmp_table->visible_field_ptr()[0]))
          ->length_bytes == 1) {
    *ptr = (uchar)(sjtbl->rowid_len + sjtbl->null_bytes);
    ptr++;
  } else {
    int2store(ptr, sjtbl->rowid_len + sjtbl->null_bytes);
    ptr += 2;
  }

  // 2. Zero the null bytes
  uchar *const nulls_ptr = ptr;
  if (sjtbl->null_bytes) {
    memset(ptr, 0, sjtbl->null_bytes);
    ptr += sjtbl->null_bytes;
  }

  // 3. Put the rowids
  for (uint i = 0; tab != tab_end; tab++, i++) {
    handler *h = tab->qep_tab->table()->file;
    if (tab->qep_tab->table()->is_nullable() &&
        tab->qep_tab->table()->has_null_row()) {
      /* It's a NULL-complemented row */
      *(nulls_ptr + tab->null_byte) |= tab->null_bit;
      memset(ptr + tab->rowid_offset, 0, h->ref_length);
    } else {
      /* Copy the rowid value */
      memcpy(ptr + tab->rowid_offset, h->ref, h->ref_length);
    }
  }

  if (!check_unique_constraint(sjtbl->tmp_table)) return 1;
  error = sjtbl->tmp_table->file->ha_write_row(sjtbl->tmp_table->record[0]);
  if (error) {
    /* If this is a duplicate error, return immediately */
    if (sjtbl->tmp_table->file->is_ignorable_error(error)) return 1;
    /*
      Other error than duplicate error: Attempt to create a temporary table.
    */
    bool is_duplicate;
    if (create_ondisk_from_heap(thd, sjtbl->tmp_table, error, true,
                                &is_duplicate))
      return -1;
    return is_duplicate ? 1 : 0;
  }
  return 0;
}

/**
  SemiJoinDuplicateElimination: Reset the temporary table
*/

static int do_sj_reset(SJ_TMP_TABLE *sj_tbl) {
  DBUG_TRACE;
  if (sj_tbl->tmp_table) {
    int rc = sj_tbl->tmp_table->empty_result_table();
    if (sj_tbl->tmp_table->hash_field)
      sj_tbl->tmp_table->file->ha_index_init(0, false);
    return rc;
  }
  sj_tbl->have_confluent_row = false;
  return 0;
}

/**
  @brief Process one row of the nested loop join.

  This function will evaluate parts of WHERE/ON clauses that are
  applicable to the partial row on hand and in case of success
  submit this row to the next level of the nested loop.
  join_tab->return_tab may be modified to cause a return to a previous
  join_tab.

  @param  join     The join object
  @param  qep_tab The most inner qep_tab being processed

  @return Nested loop state
*/

static enum_nested_loop_state evaluate_join_record(JOIN *join,
                                                   QEP_TAB *const qep_tab) {
  bool not_used_in_distinct = qep_tab->not_used_in_distinct;
  ha_rows found_records = join->found_records;
  Item *condition = qep_tab->condition();
  const plan_idx qep_tab_idx = qep_tab->idx();
  bool found = true;
  DBUG_TRACE;
  DBUG_PRINT("enter", ("join: %p join_tab index: %d table: %s cond: %p", join,
                       static_cast<int>(qep_tab_idx), qep_tab->table()->alias,
                       condition));

  if (condition) {
    found = condition->val_int();

    if (join->thd->killed) {
      join->thd->send_kill_message();
      return NESTED_LOOP_KILLED;
    }

    /* check for errors evaluating the condition */
    if (join->thd->is_error()) return NESTED_LOOP_ERROR;
  }
  if (found) {
    /*
      There is no condition on this join_tab or the attached pushed down
      condition is true => a match is found.
    */
    while (qep_tab->first_unmatched != NO_PLAN_IDX && found) {
      /*
        The while condition is always false if join_tab is not
        the last inner join table of an outer join operation.
      */
      QEP_TAB *first_unmatched = &QEP_AT(qep_tab, first_unmatched);
      /*
        Mark that a match for the current row of the outer table is found.
        This activates WHERE clause predicates attached the inner tables of
        the outer join.
      */
      first_unmatched->found = true;
      for (QEP_TAB *tab = first_unmatched; tab <= qep_tab; tab++) {
        /*
          Check all predicates that have just been activated.

          Actually all predicates non-guarded by first_unmatched->found
          will be re-evaluated again. It could be fixed, but, probably,
          it's not worth doing now.

          not_exists_optimize has been created from a
          condition containing 'is_null'. This 'is_null'
          predicate is still present on any 'tab' with
          'not_exists_optimize'. Furthermore, the usual rules
          for condition guards also applies for
          'not_exists_optimize' -> When 'is_null==false' we
          know all cond. guards are open and we can apply
          the 'not_exists_optimize'.
        */
        DBUG_ASSERT(
            !(tab->table()->reginfo.not_exists_optimize && !tab->condition()));

        if (tab->condition() && !tab->condition()->val_int()) {
          /* The condition attached to table tab is false */

          if (tab->table()->reginfo.not_exists_optimize) {
            /*
              When not_exists_optimizer is set and a matching row is found, the
              outer row should be excluded from the result set: no need to
              explore this record, thus we don't call the next_select.
              And, no need to explore other following records of 'tab', so we
              set join->return_tab.
              As we set join_tab->found above, evaluate_join_record() at the
              upper level will not yield a NULL-complemented record.
              Note that the calculation below can set return_tab to -1
              i.e. PRE_FIRST_PLAN_IDX.
            */
            join->return_tab = qep_tab_idx - 1;
            return NESTED_LOOP_OK;
          }

          if (tab == qep_tab)
            found = false;
          else {
            /*
              Set a return point if rejected predicate is attached
              not to the last table of the current nest level.
            */
            join->return_tab = tab->idx();
            return NESTED_LOOP_OK;
          }
        }
        /* check for errors evaluating the condition */
        if (join->thd->is_error()) return NESTED_LOOP_ERROR;
      }
      /*
        Check whether join_tab is not the last inner table
        for another embedding outer join.
      */
      plan_idx f_u = first_unmatched->first_upper();
      if (f_u != NO_PLAN_IDX && join->qep_tab[f_u].last_inner() != qep_tab_idx)
        f_u = NO_PLAN_IDX;
      qep_tab->first_unmatched = f_u;
    }

    plan_idx return_tab = join->return_tab;

    if (qep_tab->finishes_weedout() && found) {
      int res = do_sj_dups_weedout(join->thd, qep_tab->check_weed_out_table);
      if (res == -1)
        return NESTED_LOOP_ERROR;
      else if (res == 1)
        found = false;
    } else if (qep_tab->do_loosescan() &&
               QEP_AT(qep_tab, match_tab).found_match) {
      /*
         Loosescan algorithm requires an access method that gives 'sorted'
         retrieval of keys, or an access method that provides only one
         row (which is inherently sorted).
         EQ_REF and LooseScan may happen if dependencies in subquery (e.g.,
         outer join) prevents table pull-out.
       */
      DBUG_ASSERT(qep_tab->use_order() || qep_tab->type() == JT_EQ_REF);

      /*
         Previous row combination for duplicate-generating range,
         generated a match.  Compare keys of this row and previous row
         to determine if this is a duplicate that should be skipped.
       */
      if (key_cmp(qep_tab->table()->key_info[qep_tab->index()].key_part,
                  qep_tab->loosescan_buf, qep_tab->loosescan_key_len))
        /*
           Keys do not match.
           Reset found_match for last table of duplicate-generating range,
           to avoid comparing keys until a new match has been found.
        */
        QEP_AT(qep_tab, match_tab).found_match = false;
      else
        found = false;
    }

    /*
      It was not just a return to lower loop level when one
      of the newly activated predicates is evaluated as false
      (See above join->return_tab= tab).
    */

    if (found) {
      enum enum_nested_loop_state rc;
      // A match is found for the current partial join prefix.
      qep_tab->found_match = true;
      if (unlikely(qep_tab->lateral_derived_tables_depend_on_me))
        qep_tab->refresh_lateral();

      rc = (*qep_tab->next_select)(join, qep_tab + 1, false);

      if (rc != NESTED_LOOP_OK) return rc;

      /* check for errors evaluating the condition */
      if (join->thd->is_error()) return NESTED_LOOP_ERROR;

      if (qep_tab->do_loosescan() && QEP_AT(qep_tab, match_tab).found_match) {
        /*
           A match was found for a duplicate-generating range of a semijoin.
           Copy key to be able to determine whether subsequent rows
           will give duplicates that should be skipped.
        */
        KEY *key = qep_tab->table()->key_info + qep_tab->index();
        key_copy(qep_tab->loosescan_buf, qep_tab->table()->record[0], key,
                 qep_tab->loosescan_key_len);
      } else if (qep_tab->do_firstmatch() &&
                 QEP_AT(qep_tab, match_tab).found_match) {
        /*
          We should return to join_tab->firstmatch_return after we have
          enumerated all the suffixes for current prefix row combination
        */
        return_tab = std::min(return_tab, qep_tab->firstmatch_return);
      }

      /*
        Test if this was a SELECT DISTINCT query on a table that
        was not in the field list;  In this case we can abort if
        we found a row, as no new rows can be added to the result.
      */
      if (not_used_in_distinct && found_records != join->found_records)
        return_tab = std::min(return_tab, plan_idx(qep_tab_idx - 1));

      join->return_tab = std::min(join->return_tab, return_tab);
    } else {
      if (qep_tab->not_null_compl) {
        /* a NULL-complemented row is not in a table so cannot be locked */
        qep_tab->iterator->UnlockRow();
      }
    }
  } else {
    /*
      The condition pushed down to the table join_tab rejects all rows
      with the beginning coinciding with the current partial join.
    */
    if (qep_tab->not_null_compl) qep_tab->iterator->UnlockRow();
  }
  return NESTED_LOOP_OK;
}

/**

  @details
    Construct a NULL complimented partial join record and feed it to the next
    level of the nested loop. This function is used in case we have
    an OUTER join and no matching record was found.
*/

static enum_nested_loop_state evaluate_null_complemented_join_record(
    JOIN *join, QEP_TAB *qep_tab) {
  /*
    The table join_tab is the first inner table of a outer join operation
    and no matches has been found for the current outer row.
  */
  QEP_TAB *first_inner_tab = qep_tab;
  QEP_TAB *last_inner_tab = &QEP_AT(qep_tab, last_inner());

  DBUG_TRACE;

  bool matching = true;
  enum_nested_loop_state rc = NESTED_LOOP_OK;

  for (; qep_tab <= last_inner_tab; qep_tab++) {
    // Make sure that the rowid buffer is bound, duplicates weedout needs it
    if (qep_tab->copy_current_rowid &&
        !qep_tab->copy_current_rowid->buffer_is_bound())
      qep_tab->copy_current_rowid->bind_buffer(qep_tab->table()->file->ref);

    /* Change the the values of guard predicate variables. */
    qep_tab->found = true;
    qep_tab->not_null_compl = false;
    // Outer row is complemented by null values for each field from inner tables
    qep_tab->table()->set_null_row();
    if (qep_tab->starts_weedout() && qep_tab > first_inner_tab) {
      // sub_select() has not performed a reset for this table.
      do_sj_reset(qep_tab->flush_weedout_table);
    }
    /* Check all attached conditions for inner table rows. */
    if (qep_tab->condition() && !qep_tab->condition()->val_int()) {
      if (join->thd->killed) {
        join->thd->send_kill_message();
        return NESTED_LOOP_KILLED;
      }

      /* check for errors */
      if (join->thd->is_error()) return NESTED_LOOP_ERROR;

      matching = false;
      break;
    }
  }
  if (matching) {
    qep_tab = last_inner_tab;
    /*
      From the point of view of the rest of execution, this record matches
      (it has been built and satisfies conditions, no need to do more evaluation
      on it). See similar code in evaluate_join_record().
    */
    plan_idx f_u = QEP_AT(qep_tab, first_unmatched).first_upper();
    if (f_u != NO_PLAN_IDX && join->qep_tab[f_u].last_inner() != qep_tab->idx())
      f_u = NO_PLAN_IDX;
    qep_tab->first_unmatched = f_u;
    /*
      The row complemented by nulls satisfies all conditions
      attached to inner tables.
      Finish evaluation of record and send it to be joined with
      remaining tables.
      Note that evaluate_join_record will re-evaluate the condition attached
      to the last inner table of the current outer join. This is not deemed to
      have a significant performance impact.
    */
    rc = evaluate_join_record(join, qep_tab);
  }
  for (QEP_TAB *tab = first_inner_tab; tab <= last_inner_tab; tab++) {
    tab->table()->reset_null_row();
    // Restore NULL bits saved when reading row, @see EQRefIterator()
    if (tab->type() == JT_EQ_REF) tab->table()->restore_null_flags();
  }

  return rc;
}

/*****************************************************************************
  The different ways to read a record
  Returns -1 if row was not found, 0 if row was found and 1 on errors
*****************************************************************************/

/** Help function when we get some an error from the table handler. */

int report_handler_error(TABLE *table, int error) {
  if (error == HA_ERR_END_OF_FILE || error == HA_ERR_KEY_NOT_FOUND) {
    table->set_no_row();
    return -1;  // key not found; ok
  }
  /*
    Do not spam the error log with these temporary errors:
       LOCK_DEADLOCK LOCK_WAIT_TIMEOUT TABLE_DEF_CHANGED
    Also skip printing to error log if the current thread has been killed.
  */
  if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT &&
      error != HA_ERR_TABLE_DEF_CHANGED && !table->in_use->killed)
    LogErr(ERROR_LEVEL, ER_READING_TABLE_FAILED, error, table->s->path.str);
  table->file->print_error(error, MYF(0));
  return 1;
}

/**
  Initialize an index scan and the record buffer to use in the scan.

  @param qep_tab the table to read
  @param file    the handler to initialize
  @param idx     the index to use
  @param sorted  use the sorted order of the index
  @retval true   if an error occurred
  @retval false  on success
*/
static bool init_index_and_record_buffer(const QEP_TAB *qep_tab, handler *file,
                                         uint idx, bool sorted) {
  if (file->inited) return false;  // OK, already initialized

  int error = file->ha_index_init(idx, sorted);
  if (error != 0) {
    (void)report_handler_error(qep_tab->table(), error);
    return true;
  }

  return set_record_buffer(qep_tab);
}

int safe_index_read(QEP_TAB *tab) {
  int error;
  TABLE *table = tab->table();
  if ((error = table->file->ha_index_read_map(
           table->record[0], tab->ref().key_buff,
           make_prev_keypart_map(tab->ref().key_parts), HA_READ_KEY_EXACT)))
    return report_handler_error(table, error);
  return 0;
}

/**
   Reads content of constant table
   @param tab  table
   @param pos  position of table in query plan
   @retval 0   ok, one row was found or one NULL-complemented row was created
   @retval -1  ok, no row was found and no NULL-complemented row was created
   @retval 1   error
*/

int join_read_const_table(JOIN_TAB *tab, POSITION *pos) {
  int error;
  DBUG_TRACE;
  TABLE *table = tab->table();
  THD *const thd = tab->join()->thd;
  table->const_table = true;
  DBUG_ASSERT(!thd->is_error());

  if (table->reginfo.lock_type >= TL_WRITE_ALLOW_WRITE) {
    const enum_sql_command sql_command = tab->join()->thd->lex->sql_command;
    if (sql_command == SQLCOM_UPDATE_MULTI ||
        sql_command == SQLCOM_DELETE_MULTI) {
      /*
        In a multi-UPDATE, if we represent "depends on" with "->", we have:
        "what columns to read (read_set)" ->
        "whether table will be updated on-the-fly or with tmp table" ->
        "whether to-be-updated columns are used by access path"
        "access path to table (range, ref, scan...)" ->
        "query execution plan" ->
        "what tables are const" ->
        "reading const tables" ->
        "what columns to read (read_set)".
        To break this loop, we always read all columns of a constant table if
        it is going to be updated.
        Another case is in multi-UPDATE and multi-DELETE, when the table has a
        trigger: bits of columns needed by the trigger are turned on in
        result->optimize(), which has not yet been called when we do
        the reading now, so we must read all columns.
      */
      bitmap_set_all(table->read_set);
      /* Virtual generated columns must be writable */
      for (Field **vfield_ptr = table->vfield; vfield_ptr && *vfield_ptr;
           vfield_ptr++)
        bitmap_set_bit(table->write_set, (*vfield_ptr)->field_index);
      table->file->column_bitmaps_signal();
    }
  }

  if (tab->type() == JT_SYSTEM)
    error = read_system(table);
  else {
    if (!table->key_read && table->covering_keys.is_set(tab->ref().key) &&
        !table->no_keyread &&
        (int)table->reginfo.lock_type <= (int)TL_READ_HIGH_PRIORITY) {
      table->set_keyread(true);
      tab->set_index(tab->ref().key);
    }
    error = read_const(table, &tab->ref());
    table->set_keyread(false);
  }

  if (error) {
    // Promote error to fatal if an actual error was reported
    if (thd->is_error()) error = 1;
    /* Mark for EXPLAIN that the row was not found */
    pos->filter_effect = 1.0;
    pos->rows_fetched = 0.0;
    pos->prefix_rowcount = 0.0;
    pos->ref_depend_map = 0;
    if (!tab->table_ref->outer_join || error > 0) return error;
  }

  if (tab->join_cond() && !table->has_null_row()) {
    // We cannot handle outer-joined tables with expensive join conditions here:
    DBUG_ASSERT(!tab->join_cond()->is_expensive());
    if (tab->join_cond()->val_int() == 0) table->set_null_row();
  }

  /* Check appearance of new constant items in Item_equal objects */
  JOIN *const join = tab->join();
  if (join->where_cond && update_const_equal_items(thd, join->where_cond, tab))
    return 1;
  TABLE_LIST *tbl;
  for (tbl = join->select_lex->leaf_tables; tbl; tbl = tbl->next_leaf) {
    TABLE_LIST *embedded;
    TABLE_LIST *embedding = tbl;
    do {
      embedded = embedding;
      if (embedded->join_cond_optim() &&
          update_const_equal_items(thd, embedded->join_cond_optim(), tab))
        return 1;
      embedding = embedded->embedding;
    } while (embedding &&
             embedding->nested_join->join_list.front() == embedded);
  }

  return 0;
}

/**
  Read a constant table when there is at most one matching row, using a table
  scan.

  @param table			Table to read

  @retval  0  Row was found
  @retval  -1 Row was not found
  @retval  1  Got an error (other than row not found) during read
*/
static int read_system(TABLE *table) {
  int error;
  if (!table->is_started())  // If first read
  {
    if ((error = table->file->ha_read_first_row(table->record[0],
                                                table->s->primary_key))) {
      if (error != HA_ERR_END_OF_FILE)
        return report_handler_error(table, error);
      table->set_null_row();
      empty_record(table);  // Make empty record
      return -1;
    }
    store_record(table, record[1]);
  } else if (table->has_row() && table->is_nullable()) {
    /*
      Row buffer contains a row, but it may have been partially overwritten
      by a null-extended row. Restore the row from the saved copy.
      @note this branch is currently unused.
    */
    DBUG_ASSERT(false);
    table->set_found_row();
    restore_record(table, record[1]);
  }

  return table->has_row() ? 0 : -1;
}

ConstIterator::ConstIterator(THD *thd, TABLE *table, TABLE_REF *table_ref,
                             ha_rows *examined_rows)
    : TableRowIterator(thd, table),
      m_ref(table_ref),
      m_examined_rows(examined_rows) {}

bool ConstIterator::Init() {
  m_first_record_since_init = true;
  return false;
}

/**
  Read a constant table when there is at most one matching row, using an
  index lookup.

  @retval 0  Row was found
  @retval -1 Row was not found
  @retval 1  Got an error (other than row not found) during read
*/

int ConstIterator::Read() {
  if (!m_first_record_since_init) {
    return -1;
  }
  m_first_record_since_init = false;
  int err = read_const(table(), m_ref);
  if (err == 0 && m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  table()->const_table = true;
  return err;
}

vector<string> ConstIterator::DebugString() const {
  DBUG_ASSERT(table()->file->pushed_idx_cond == nullptr);
  DBUG_ASSERT(table()->file->pushed_cond == nullptr);
  return {string("Constant row from ") + table()->alias};
}

static int read_const(TABLE *table, TABLE_REF *ref) {
  int error;
  DBUG_TRACE;

  if (!table->is_started())  // If first read
  {
    /* Perform "Late NULLs Filtering" (see internals manual for explanations) */
    if (ref->impossible_null_ref() ||
        construct_lookup_ref(table->in_use, table, ref))
      error = HA_ERR_KEY_NOT_FOUND;
    else {
      error = table->file->ha_index_read_idx_map(
          table->record[0], ref->key, ref->key_buff,
          make_prev_keypart_map(ref->key_parts), HA_READ_KEY_EXACT);
    }
    if (error) {
      if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE) {
        const int ret = report_handler_error(table, error);
        return ret;
      }
      table->set_no_row();
      table->set_null_row();
      empty_record(table);
      return -1;
    }
    /*
      read_const() may be called several times inside a nested loop join.
      Save record in case it is needed when table is in "started" state.
    */
    store_record(table, record[1]);
  } else if (table->has_row() && table->is_nullable()) {
    /*
      Row buffer contains a row, but it may have been partially overwritten
      by a null-extended row. Restore the row from the saved copy.
    */
    table->set_found_row();
    restore_record(table, record[1]);
  }
  return table->has_row() ? 0 : -1;
}

EQRefIterator::EQRefIterator(THD *thd, TABLE *table, TABLE_REF *ref,
                             bool use_order, ha_rows *examined_rows)
    : TableRowIterator(thd, table),
      m_ref(ref),
      m_use_order(use_order),
      m_examined_rows(examined_rows) {}

/**
  Read row using unique key: eq_ref access method implementation

  @details
    This is the "read_first" function for the eq_ref access method.
    The difference from ref access function is that it has a one-element
    lookup cache, maintained in record[0]. Since the eq_ref access method
    will always return the same row, it is not necessary to read the row
    more than once, regardless of how many times it is needed in execution.
    This cache element is used when a row is needed after it has been read once,
    unless a key conversion error has occurred, or the cache has been disabled.

  @retval  0 - Ok
  @retval -1 - Row not found
  @retval  1 - Error
*/

bool EQRefIterator::Init() {
  if (!table()->file->inited) {
    DBUG_ASSERT(!m_use_order);  // Don't expect sort req. for single row.
    int error = table()->file->ha_index_init(m_ref->key, m_use_order);
    if (error) {
      PrintError(error);
      return true;
    }
  }

  m_first_record_since_init = true;

  return false;
}

/**
  Read row using unique key: eq_ref access method implementation

  @details
    The difference from RefIterator is that it has a one-element
    lookup cache, maintained in record[0]. Since the eq_ref access method
    will always return the same row, it is not necessary to read the row
    more than once, regardless of how many times it is needed in execution.
    This cache element is used when a row is needed after it has been read once,
    unless a key conversion error has occurred, or the cache has been disabled.

  @retval  0 - Ok
  @retval -1 - Row not found
  @retval  1 - Error
*/

int EQRefIterator::Read() {
  if (!m_first_record_since_init) {
    return -1;
  }
  m_first_record_since_init = false;

  /*
    Calculate if needed to read row. Always needed if
    - no rows read yet, or
    - table has a pushed condition, or
    - cache is disabled, or
    - previous lookup caused error when calculating key.
  */
  bool read_row = !table()->is_started() || table()->file->pushed_cond ||
                  m_ref->disable_cache || m_ref->key_err;
  if (!read_row)
    // Last lookup found a row, copy its key to secondary buffer
    memcpy(m_ref->key_buff2, m_ref->key_buff, m_ref->key_length);

  // Create new key for lookup
  m_ref->key_err = construct_lookup_ref(table()->in_use, table(), m_ref);
  if (m_ref->key_err) {
    table()->set_no_row();
    return -1;
  }

  // Re-use current row if keys are equal
  if (!read_row &&
      memcmp(m_ref->key_buff2, m_ref->key_buff, m_ref->key_length) != 0)
    read_row = true;

  if (read_row) {
    /*
       Moving away from the current record. Unlock the row
       in the handler if it did not match the partial WHERE.
     */
    if (table()->has_row() && m_ref->use_count == 0)
      table()->file->unlock_row();

    /*
      Perform "Late NULLs Filtering" (see internals manual for explanations)

      As EQRefIterator effectively implements a one row cache of last
      fetched row, the NULLs filtering cant be done until after the cache
      key has been checked and updated, and row locks maintained.
    */
    if (m_ref->impossible_null_ref()) {
      DBUG_PRINT("info", ("EQRefIterator null_rejected"));
      table()->set_no_row();
      return -1;
    }

    int error = table()->file->ha_index_read_map(
        table()->record[0], m_ref->key_buff,
        make_prev_keypart_map(m_ref->key_parts), HA_READ_KEY_EXACT);
    if (error) {
      return HandleError(error);
    }

    m_ref->use_count = 1;
    table()->save_null_flags();
  } else if (table()->has_row()) {
    DBUG_ASSERT(!table()->has_null_row());
    table()->restore_null_flags();
    m_ref->use_count++;
  }

  if (table()->has_row() && m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return table()->has_row() ? 0 : -1;
}

/**
  Since EQRefIterator may buffer a record, do not unlock
  it if it was not used in this invocation of EQRefIterator::Read().
  Only count locks, thus remembering if the record was left unused,
  and unlock already when pruning the current value of
  TABLE_REF buffer.
  @sa EQRefIterator::Read()
*/

void EQRefIterator::UnlockRow() {
  DBUG_ASSERT(m_ref->use_count);
  if (m_ref->use_count) m_ref->use_count--;
}

vector<string> EQRefIterator::DebugString() const {
  const KEY *key = &table()->key_info[m_ref->key];
  string str = string("Single-row index lookup on ") + table()->alias +
               " using " + key->name + " (" +
               RefToString(*m_ref, key, /*include_nulls=*/false) + ")";
  if (table()->file->pushed_idx_cond != nullptr) {
    str += ", with index condition: " +
           ItemToString(table()->file->pushed_idx_cond);
  }
  str += table()->file->explain_extra();
  return {str};
}

PushedJoinRefIterator::PushedJoinRefIterator(THD *thd, TABLE *table,
                                             TABLE_REF *ref, bool use_order,
                                             ha_rows *examined_rows)
    : TableRowIterator(thd, table),
      m_ref(ref),
      m_use_order(use_order),
      m_examined_rows(examined_rows) {}

bool PushedJoinRefIterator::Init() {
  DBUG_ASSERT(!m_use_order);  // Pushed child can't be sorted

  if (!table()->file->inited) {
    int error = table()->file->ha_index_init(m_ref->key, m_use_order);
    if (error) {
      PrintError(error);
      return true;
    }
  }

  m_first_record_since_init = true;
  return false;
}

int PushedJoinRefIterator::Read() {
  if (m_first_record_since_init) {
    m_first_record_since_init = false;

    /* Perform "Late NULLs Filtering" (see internals manual for explanations) */
    if (m_ref->impossible_null_ref()) {
      table()->set_no_row();
      DBUG_PRINT("info", ("PushedJoinRefIterator::Read() null_rejected"));
      return -1;
    }

    if (construct_lookup_ref(thd(), table(), m_ref)) {
      table()->set_no_row();
      return -1;
    }

    // 'read' itself is a NOOP:
    //  handler::ha_index_read_pushed() only unpack the prefetched row and
    //  set 'status'
    int error = table()->file->ha_index_read_pushed(
        table()->record[0], m_ref->key_buff,
        make_prev_keypart_map(m_ref->key_parts));
    if (error) {
      return HandleError(error);
    }
  } else {
    int error = table()->file->ha_index_next_pushed(table()->record[0]);
    if (error) {
      return HandleError(error);
    }
  }
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

vector<string> PushedJoinRefIterator::DebugString() const {
  DBUG_ASSERT(table()->file->pushed_idx_cond == nullptr);
  const KEY *key = &table()->key_info[m_ref->key];
  return {string("Index lookup on ") + table()->alias + " using " + key->name +
          " (" + RefToString(*m_ref, key, /*include_nulls=*/false) + ")" +
          table()->file->explain_extra()};
}

template <bool Reverse>
RefIterator<Reverse>::RefIterator(THD *thd, TABLE *table, TABLE_REF *ref,
                                  bool use_order, QEP_TAB *qep_tab,
                                  ha_rows *examined_rows)
    : TableRowIterator(thd, table),
      m_ref(ref),
      m_use_order(use_order),
      m_qep_tab(qep_tab),
      m_examined_rows(examined_rows) {}

template <bool Reverse>
bool RefIterator<Reverse>::Init() {
  m_first_record_since_init = true;
  return init_index_and_record_buffer(m_qep_tab, m_qep_tab->table()->file,
                                      m_ref->key, m_use_order);
}

template <bool Reverse>
vector<string> RefIterator<Reverse>::DebugString() const {
  const KEY *key = &table()->key_info[m_ref->key];
  string str = string("Index lookup on ") + table()->alias + " using " +
               key->name + " (" +
               RefToString(*m_ref, key, /*include_nulls=*/false);
  if (Reverse) {
    str += "; iterate backwards";
  }
  str += ")";
  if (table()->file->pushed_idx_cond != nullptr) {
    str += ", with index condition: " +
           ItemToString(table()->file->pushed_idx_cond);
  }
  str += table()->file->explain_extra();
  return {str};
}

// Doxygen gets confused by the explicit specializations.

//! @cond
template <>
int RefIterator<false>::Read() {  // Forward read.
  if (m_first_record_since_init) {
    m_first_record_since_init = false;

    /*
      a = b can never return true if a or b is NULL, so if we're asked
      to do such a lookup, we can say there won't be a match without even
      checking the index. This is “late NULLs filtering” (as opposed to
      “early NULLs filtering”, which propagates the IS NOT NULL constraint
      further back to the other table so we don't even get the request).
      See the internals manual for more details.
     */
    if (m_ref->impossible_null_ref()) {
      DBUG_PRINT("info", ("RefIterator null_rejected"));
      table()->set_no_row();
      return -1;
    }
    if (construct_lookup_ref(thd(), table(), m_ref)) {
      table()->set_no_row();
      return -1;
    }
    int error = table()->file->ha_index_read_map(
        table()->record[0], m_ref->key_buff,
        make_prev_keypart_map(m_ref->key_parts), HA_READ_KEY_EXACT);
    if (error) {
      return HandleError(error);
    }
  } else {
    int error = table()->file->ha_index_next_same(
        table()->record[0], m_ref->key_buff, m_ref->key_length);
    if (error) {
      return HandleError(error);
    }
  }
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

/**
  This function is used when optimizing away ORDER BY in
  SELECT * FROM t1 WHERE a=1 ORDER BY a DESC,b DESC.
*/
template <>
int RefIterator<true>::Read() {  // Reverse read.
  if (m_first_record_since_init) {
    m_first_record_since_init = false;

    /*
      a = b can never return true if a or b is NULL, so if we're asked
      to do such a lookup, we can say there won't be a match without even
      checking the index. This is “late NULLs filtering” (as opposed to
      “early NULLs filtering”, which propagates the IS NOT NULL constraint
      further back to the other table so we don't even get the request).
      See the internals manual for more details.
     */
    if (m_ref->impossible_null_ref()) {
      DBUG_PRINT("info", ("RefIterator null_rejected"));
      table()->set_no_row();
      return -1;
    }
    if (construct_lookup_ref(thd(), table(), m_ref)) {
      table()->set_no_row();
      return -1;
    }
    int error = table()->file->ha_index_read_last_map(
        table()->record[0], m_ref->key_buff,
        make_prev_keypart_map(m_ref->key_parts));
    if (error) {
      return HandleError(error);
    }
  } else {
    /*
      Using ha_index_prev() for reading records from the table can cause
      performance issues if used in combination with ICP. The ICP code
      in the storage engine does not know when to stop reading from the
      index and a call to ha_index_prev() might cause the storage engine
      to read to the beginning of the index if no qualifying record is
      found.
     */
    DBUG_ASSERT(table()->file->pushed_idx_cond == NULL);
    int error = table()->file->ha_index_prev(table()->record[0]);
    if (error) {
      return HandleError(error);
    }
    if (key_cmp_if_same(table(), m_ref->key_buff, m_ref->key,
                        m_ref->key_length)) {
      table()->set_no_row();
      return -1;
    }
  }
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}
//! @endcond

DynamicRangeIterator::DynamicRangeIterator(THD *thd, TABLE *table,
                                           QEP_TAB *qep_tab,
                                           ha_rows *examined_rows)
    : TableRowIterator(thd, table),
      m_qep_tab(qep_tab),
      m_examined_rows(examined_rows) {}

bool DynamicRangeIterator::Init() {
  // The range optimizer generally expects this to be set.
  thd()->lex->set_current_select(m_qep_tab->join()->select_lex);

  Opt_trace_context *const trace = &thd()->opt_trace;
  const bool disable_trace =
      m_quick_traced_before &&
      !trace->feature_enabled(Opt_trace_context::DYNAMIC_RANGE);
  Opt_trace_disable_I_S disable_trace_wrapper(trace, disable_trace);

  m_quick_traced_before = true;

  Opt_trace_object wrapper(trace);
  Opt_trace_object trace_table(trace, "rows_estimation_per_outer_row");
  trace_table.add_utf8_table(m_qep_tab->table_ref);

  Key_map needed_reg_dummy;
  QUICK_SELECT_I *old_qck = m_qep_tab->quick();
  QUICK_SELECT_I *qck;
  DEBUG_SYNC(thd(), "quick_not_created");
  const int rc = test_quick_select(thd(), m_qep_tab->keys(),
                                   0,  // empty table map
                                   HA_POS_ERROR,
                                   false,  // don't force quick range
                                   ORDER_NOT_RELEVANT, m_qep_tab,
                                   m_qep_tab->condition(), &needed_reg_dummy,
                                   &qck, m_qep_tab->table()->force_index);
  if (thd()->is_error())  // @todo consolidate error reporting of
                          // test_quick_select
    return true;
  DBUG_ASSERT(old_qck == NULL || old_qck != qck);
  m_qep_tab->set_quick(qck);

  /*
    EXPLAIN CONNECTION is used to understand why a query is currently taking
    so much time. So it makes sense to show what the execution is doing now:
    is it a table scan or a range scan? A range scan on which index.
    So: below we want to change the type and quick visible in EXPLAIN, and for
    that, we need to take mutex and change type and quick_optim.
  */

  DEBUG_SYNC(thd(), "quick_created_before_mutex");

  thd()->lock_query_plan();
  m_qep_tab->set_type(qck ? calc_join_type(qck->get_type()) : JT_ALL);
  m_qep_tab->set_quick_optim();
  thd()->unlock_query_plan();

  delete old_qck;
  DEBUG_SYNC(thd(), "quick_droped_after_mutex");

  // Clear out and destroy any old iterators before we start constructing
  // new ones, since they may share the same memory in the union.
  m_iterator.reset();

  if (rc == -1) {
    return false;
  }

  if (qck) {
    m_iterator = NewIterator<IndexRangeScanIterator>(
        thd(), table(), qck, m_qep_tab, m_examined_rows);
  } else {
    m_iterator = NewIterator<TableScanIterator>(thd(), table(), m_qep_tab,
                                                m_examined_rows);
  }
  return m_iterator->Init();
}

int DynamicRangeIterator::Read() {
  if (m_iterator == nullptr) {
    return -1;
  } else {
    return m_iterator->Read();
  }
}

vector<string> DynamicRangeIterator::DebugString() const {
  // TODO: Convert QUICK_SELECT_I to RowIterator so that we can get
  // better outputs here (similar to dbug_dump()), although it might
  // get tricky when there are many alternatives.
  string str = string("Index range scan on ") + table()->alias +
               " (re-planned for each iteration)";
  if (table()->file->pushed_idx_cond != nullptr) {
    str += ", with index condition: " +
           ItemToString(table()->file->pushed_idx_cond);
  }
  str += table()->file->explain_extra();
  return {str};
}

/**
  @brief Prepare table for reading rows and read first record.
  @details
    Prior to reading the table following tasks are done, (in the order of
    execution):
      .) derived tables are materialized
      .) pre-iterator executor only: duplicates removed (tmp tables only)
      .) table is sorted with filesort (both non-tmp and tmp tables)
    After this have been done this function resets quick select, if it's
    present, sets up table reading functions, and reads first record.

  @retval
    0   Ok
  @retval
    -1   End of records
  @retval
    1   Error
*/

void join_setup_iterator(QEP_TAB *tab) {
  bool using_table_scan;
  tab->iterator =
      create_table_iterator(tab->join()->thd, NULL, tab, false,
                            /*ignore_not_found_rows=*/false,
                            /*examined_rows=*/nullptr, &using_table_scan);
  tab->set_using_table_scan(using_table_scan);

  if (tab->filesort) {
    unique_ptr_destroy_only<RowIterator> iterator = move(tab->iterator);

    if (tab->condition()) {
      iterator = NewIterator<FilterIterator>(tab->join()->thd, move(iterator),
                                             tab->condition());
    }

    // Wrap the chosen RowIterator in a SortingIterator, so that we get
    // sorted results out.
    tab->iterator = NewIterator<SortingIterator>(tab->join()->thd,
                                                 tab->filesort, move(iterator),
                                                 &tab->join()->examined_rows);
    tab->table()->sorting_iterator =
        down_cast<SortingIterator *>(tab->iterator->real_iterator());
  }
}

/*
  This helper function materializes derived table/view and then calls
  read_first_record function to set up access to the materialized table.
*/

int join_materialize_table_function(QEP_TAB *tab) {
  TABLE_LIST *const table = tab->table_ref;
  DBUG_ASSERT(table->table_function);

  (void)table->table_function->fill_result_table();

  return table->table->in_use->is_error() ? NESTED_LOOP_ERROR : NESTED_LOOP_OK;
}

/*
  This helper function materializes derived table/view and then calls
  read_first_record function to set up access to the materialized table.
*/

int join_materialize_derived(QEP_TAB *tab) {
  THD *const thd = tab->table()->in_use;
  TABLE_LIST *const derived = tab->table_ref;

  DBUG_ASSERT(derived->uses_materialization() && !tab->table()->materialized);

  if (derived->materializable_is_const())  // Has been materialized by optimizer
    return NESTED_LOOP_OK;

  bool res = derived->materialize_derived(thd);
  res |= derived->cleanup_derived(thd);
  DEBUG_SYNC(thd, "after_materialize_derived");
  return res ? NESTED_LOOP_ERROR : NESTED_LOOP_OK;
}

/*
  Helper function for materialization of a semi-joined subquery.

  @param tab JOIN_TAB referencing a materialized semi-join table

  @return Nested loop state
*/

int join_materialize_semijoin(QEP_TAB *tab) {
  DBUG_TRACE;

  Semijoin_mat_exec *const sjm = tab->sj_mat_exec();

  QEP_TAB *const first = tab->join()->qep_tab + sjm->inner_table_index;
  QEP_TAB *const last = first + (sjm->table_count - 1);
  /*
    Set up the end_sj_materialize function after the last inner table,
    so that generated rows are inserted into the materialized table.
  */
  last->next_select = end_sj_materialize;
  last->set_sj_mat_exec(sjm);  // TODO: This violates comment for sj_mat_exec!
  if (tab->table()->hash_field) tab->table()->file->ha_index_init(0, false);
  int rc;
  if ((rc = sub_select(tab->join(), first, false)) < 0) return rc;
  if ((rc = sub_select(tab->join(), first, true)) < 0) return rc;
  if (tab->table()->hash_field) tab->table()->file->ha_index_or_rnd_end();

  last->next_select = NULL;
  last->set_sj_mat_exec(NULL);

#if !defined(DBUG_OFF) || defined(HAVE_VALGRIND)
  // Fields of inner tables should not be read anymore:
  for (QEP_TAB *t = first; t <= last; t++) {
    // Rows may persist across executions for these types:
    if (t->type() == JT_EQ_REF || t->type() == JT_CONST ||
        t->type() == JT_SYSTEM)
      continue;
    TABLE *const inner_table = t->table();
    TRASH(inner_table->record[0], inner_table->s->reclength);
  }
#endif

  tab->table()->materialized = true;
  return NESTED_LOOP_OK;
}

/**
  Check if access to this JOIN_TAB has to retrieve rows
  in sorted order as defined by the ordered index
  used to access this table.
*/
bool QEP_TAB::use_order() const {
  /*
    No need to require sorted access for single row reads
    being performed by const- or EQ_REF-accessed tables.
  */
  if (type() == JT_EQ_REF || type() == JT_CONST || type() == JT_SYSTEM)
    return false;

  /*
    First non-const table requires sorted results
    if ORDER or GROUP BY use ordered index.
  */
  if ((uint)idx() == join()->const_tables &&
      join()->m_ordered_index_usage != JOIN::ORDERED_INDEX_VOID)
    return true;

  /*
    LooseScan strategy for semijoin requires sorted
    results even if final result is not to be sorted.
  */
  if (position()->sj_strategy == SJ_OPT_LOOSE_SCAN) return true;

  /* Fall through: Results don't have to be sorted */
  return false;
}

FullTextSearchIterator::FullTextSearchIterator(THD *thd, TABLE *table,
                                               TABLE_REF *ref, bool use_order,
                                               ha_rows *examined_rows)
    : TableRowIterator(thd, table),
      m_ref(ref),
      m_use_order(use_order),
      m_examined_rows(examined_rows) {}

FullTextSearchIterator::~FullTextSearchIterator() {
  table()->file->ha_index_or_rnd_end();
}

bool FullTextSearchIterator::Init() {
  if (!table()->file->inited) {
    int error = table()->file->ha_index_init(m_ref->key, m_use_order);
    if (error) {
      PrintError(error);
      return true;
    }
  }
  table()->file->ft_init();
  return false;
}

int FullTextSearchIterator::Read() {
  int error = table()->file->ha_ft_read(table()->record[0]);
  if (error) {
    return HandleError(error);
  }
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

vector<string> FullTextSearchIterator::DebugString() const {
  DBUG_ASSERT(table()->file->pushed_idx_cond == nullptr);
  const KEY *key = &table()->key_info[m_ref->key];
  return {string("Indexed full text search on ") + table()->alias + " using " +
          key->name + " (" + RefToString(*m_ref, key, /*include_nulls=*/false) +
          ")" + table()->file->explain_extra()};
}

/**
  Reading of key with key reference and one part that may be NULL.
*/

RefOrNullIterator::RefOrNullIterator(THD *thd, TABLE *table, TABLE_REF *ref,
                                     bool use_order, QEP_TAB *qep_tab,
                                     ha_rows *examined_rows)
    : TableRowIterator(thd, table),
      m_ref(ref),
      m_use_order(use_order),
      m_qep_tab(qep_tab),
      m_examined_rows(examined_rows) {}

bool RefOrNullIterator::Init() {
  m_reading_first_row = true;
  *m_ref->null_ref_key = false;
  return init_index_and_record_buffer(m_qep_tab, m_qep_tab->table()->file,
                                      m_ref->key, m_use_order);
}

int RefOrNullIterator::Read() {
  if (m_reading_first_row && !*m_ref->null_ref_key) {
    /* Perform "Late NULLs Filtering" (see internals manual for explanations)
     */
    if (m_ref->impossible_null_ref() ||
        construct_lookup_ref(thd(), table(), m_ref)) {
      // Skip searching for non-NULL rows; go straight to NULL rows.
      *m_ref->null_ref_key = true;
    }
  }

  int error;
  if (m_reading_first_row) {
    m_reading_first_row = false;
    error = table()->file->ha_index_read_map(
        table()->record[0], m_ref->key_buff,
        make_prev_keypart_map(m_ref->key_parts), HA_READ_KEY_EXACT);
  } else {
    error = table()->file->ha_index_next_same(
        table()->record[0], m_ref->key_buff, m_ref->key_length);
  }

  if (error == 0) {
    if (m_examined_rows != nullptr) {
      ++*m_examined_rows;
    }
    return 0;
  } else if (error == HA_ERR_END_OF_FILE || error == HA_ERR_KEY_NOT_FOUND) {
    if (!*m_ref->null_ref_key) {
      // No more non-NULL rows; try again with NULL rows.
      *m_ref->null_ref_key = true;
      m_reading_first_row = true;
      return Read();
    } else {
      // Real EOF.
      table()->set_no_row();
      return -1;
    }
  } else {
    return HandleError(error);
  }
}

vector<string> RefOrNullIterator::DebugString() const {
  const KEY *key = &table()->key_info[m_ref->key];
  string str = string("Index lookup on ") + table()->alias + " using " +
               key->name + " (" +
               RefToString(*m_ref, key, /*include_nulls=*/true) + ")";
  if (table()->file->pushed_idx_cond != nullptr) {
    str += ", with index condition: " +
           ItemToString(table()->file->pushed_idx_cond);
  }
  str += table()->file->explain_extra();
  return {str};
}

AlternativeIterator::AlternativeIterator(
    THD *thd, TABLE *table, QEP_TAB *qep_tab, ha_rows *examined_rows,
    unique_ptr_destroy_only<RowIterator> source, TABLE_REF *ref)
    : RowIterator(thd),
      m_ref(ref),
      m_source_iterator(std::move(source)),
      m_table_scan_iterator(
          NewIterator<TableScanIterator>(thd, table, qep_tab, examined_rows)),
      m_table(table) {
  for (unsigned key_part_idx = 0; key_part_idx < ref->key_parts;
       ++key_part_idx) {
    bool *cond_guard = ref->cond_guards[key_part_idx];
    if (cond_guard != nullptr) {
      m_applicable_cond_guards.push_back(cond_guard);
    }
  }
  DBUG_ASSERT(!m_applicable_cond_guards.empty());
}

bool AlternativeIterator::Init() {
  m_iterator = m_source_iterator.get();
  for (bool *cond_guard : m_applicable_cond_guards) {
    if (!*cond_guard) {
      m_iterator = m_table_scan_iterator.get();
      break;
    }
  }

  if (m_iterator != m_last_iterator_inited) {
    m_table->file->ha_index_or_rnd_end();
    m_last_iterator_inited = m_iterator;
  }

  return m_iterator->Init();
}

vector<string> AlternativeIterator::DebugString() const {
  const TABLE *table =
      down_cast<TableScanIterator *>(m_table_scan_iterator->real_iterator())
          ->table();
  const KEY *key = &table->key_info[m_ref->key];
  string ret = "Alternative plans for IN subquery: Index lookup unless ";
  if (m_applicable_cond_guards.size() > 1) {
    ret += " any of (";
  }
  bool first = true;
  for (unsigned key_part_idx = 0; key_part_idx < m_ref->key_parts;
       ++key_part_idx) {
    if (m_ref->cond_guards[key_part_idx] == nullptr) {
      continue;
    }
    if (!first) {
      ret += ", ";
    }
    first = false;
    ret += key->key_part[key_part_idx].field->field_name;
  }
  if (m_applicable_cond_guards.size() > 1) {
    ret += ")";
  }
  ret += " IS NULL";
  return {ret};
}

/**
  Pick the appropriate access method functions

  Sets the functions for the selected table access method
*/

void QEP_TAB::pick_table_access_method() {
  DBUG_ASSERT(table());
  // Only some access methods support reversed access:
  DBUG_ASSERT(!m_reversed_access || type() == JT_REF ||
              type() == JT_INDEX_SCAN);
  TABLE_REF *used_ref = nullptr;

  const TABLE *pushed_root = table()->file->member_of_pushed_join();
  const bool is_pushed_child = (pushed_root && pushed_root != table());
  // A 'pushed_child' has to be a REF type
  DBUG_ASSERT(!is_pushed_child || type() == JT_REF || type() == JT_EQ_REF);

  switch (type()) {
    case JT_REF:
      if (is_pushed_child) {
        DBUG_ASSERT(!m_reversed_access);
        iterator = NewIterator<PushedJoinRefIterator>(
            join()->thd, table(), &ref(), use_order(), &join()->examined_rows);
      } else if (m_reversed_access) {
        iterator = NewIterator<RefIterator<true>>(join()->thd, table(), &ref(),
                                                  use_order(), this,
                                                  &join()->examined_rows);
      } else {
        iterator = NewIterator<RefIterator<false>>(join()->thd, table(), &ref(),
                                                   use_order(), this,
                                                   &join()->examined_rows);
      }
      used_ref = &ref();
      break;

    case JT_REF_OR_NULL:
      iterator = NewIterator<RefOrNullIterator>(join()->thd, table(), &ref(),
                                                use_order(), this,
                                                &join()->examined_rows);
      used_ref = &ref();
      break;

    case JT_CONST:
      iterator = NewIterator<ConstIterator>(join()->thd, table(), &ref(),
                                            &join()->examined_rows);
      break;

    case JT_EQ_REF:
      if (is_pushed_child) {
        iterator = NewIterator<PushedJoinRefIterator>(
            join()->thd, table(), &ref(), use_order(), &join()->examined_rows);
      } else {
        iterator = NewIterator<EQRefIterator>(
            join()->thd, table(), &ref(), use_order(), &join()->examined_rows);
      }
      used_ref = &ref();
      break;

    case JT_FT:
      iterator = NewIterator<FullTextSearchIterator>(
          join()->thd, table(), &ref(), use_order(), &join()->examined_rows);
      used_ref = &ref();
      break;

    case JT_INDEX_SCAN:
      if (m_reversed_access) {
        iterator = NewIterator<IndexScanIterator<true>>(
            join()->thd, table(), index(), use_order(), this,
            &join()->examined_rows);
      } else {
        iterator = NewIterator<IndexScanIterator<false>>(
            join()->thd, table(), index(), use_order(), this,
            &join()->examined_rows);
      }
      break;
    case JT_ALL:
    case JT_RANGE:
    case JT_INDEX_MERGE:
      if (using_dynamic_range) {
        iterator = NewIterator<DynamicRangeIterator>(join()->thd, table(), this,
                                                     &join()->examined_rows);
      } else {
        iterator =
            create_table_iterator(join()->thd, NULL, this, false,
                                  /*ignore_not_found_rows=*/false,
                                  &join()->examined_rows, &m_using_table_scan);
      }
      break;
    default:
      DBUG_ASSERT(0);
      break;
  }

  /*
    If we have an item like <expr> IN ( SELECT f2 FROM t2 ), and we were not
    able to rewrite it into a semijoin, the optimizer may rewrite it into
    EXISTS ( SELECT 1 FROM t2 WHERE f2=<expr> LIMIT 1 ) (ie., pushing down the
    value into the subquery), using a REF or REF_OR_NULL scan on t2 if possible.
    This happens in Item_in_subselect::select_in_like_transformer() and the
    functions it calls.

    However, if <expr> evaluates to NULL, this transformation is incorrect,
    and the transformation used should instead be to

      EXISTS ( SELECT 1 FROM t2 LIMIT 1 ) ? NULL : FALSE.

    Thus, in the case of nullable <expr>, the rewriter inserts so-called
    “condition guards” (pointers to bool saying whether <expr> was NULL or not,
    for each part of <expr> if it contains multiple columns). These condition
    guards do two things:

      1. They disable the pushed-down WHERE clauses.
      2. They change the REF/REF_OR_NULL accesses to table scans.

    We don't need to worry about #1 here, but #2 needs to be dealt with,
    as it changes the plan. We solve it by inserting an AlternativeIterator
    that chooses between two sub-iterators at execution time, based on the
    condition guard in question.

    Note that ideally, we'd plan a completely separate plan for the NULL case,
    as there might be e.g. a different index we could scan on, or even a
    different optimal join order. (Note, however, that for the case of multiple
    columns in the expression, we could get 2^N different plans.) However, given
    that most cases are now handled by semijoins and not in2exists at all,
    we don't need to jump through every possible hoop to optimize these cases.
   */
  if (used_ref != nullptr) {
    for (unsigned key_part_idx = 0; key_part_idx < used_ref->key_parts;
         ++key_part_idx) {
      if (used_ref->cond_guards[key_part_idx] != nullptr) {
        DBUG_ASSERT(!is_pushed_child);
        // At least one condition guard is relevant, so we need to use
        // the AlternativeIterator.
        iterator = NewIterator<AlternativeIterator>(join()->thd, table(), this,
                                                    &join()->examined_rows,
                                                    move(iterator), used_ref);
        break;
      }
    }
  }
}

/*****************************************************************************
  DESCRIPTION
    Functions that end one nested loop iteration. Different functions
    are used to support GROUP BY clause and to redirect records
    to a table (e.g. in case of SELECT into a temporary table) or to the
    network client.
    See the enum_nested_loop_state enumeration for the description of return
    values.
*****************************************************************************/

/* ARGSUSED */
static enum_nested_loop_state end_send(JOIN *join, QEP_TAB *qep_tab,
                                       bool end_of_records) {
  DBUG_TRACE;
  /*
    When all tables are const this function is called with jointab == NULL.
    This function shouldn't be called for the first join_tab as it needs
    to get fields from previous tab.

    Note that qep_tab may be one past the last of qep_tab! So don't read its
    pointed content. But you can read qep_tab[-1] then.
  */
  DBUG_ASSERT(qep_tab == NULL || qep_tab > join->qep_tab);
  THD *thd = join->thd;

  if (!end_of_records) {
    int error;
    int sliceno;
    if (qep_tab) {
      if (qep_tab - 1 == join->ref_slice_immediately_before_group_by) {
        // Read Items from pseudo-table REF_SLICE_ORDERED_GROUP_BY
        sliceno = REF_SLICE_ORDERED_GROUP_BY;
      } else {
        sliceno = qep_tab[-1].ref_item_slice;
      }
    } else {
      // All-constant tables; no change of slice
      sliceno = join->current_ref_item_slice;
    }
    Switch_ref_item_slice slice_switch(join, sliceno);
    List<Item> *fields = join->get_current_fields();
    if (join->tables &&
        // In case filesort has been used and zeroed quick():
        (join->qep_tab[0].quick_optim() &&
         join->qep_tab[0].quick_optim()->is_loose_index_scan())) {
      // Copy non-aggregated fields when loose index scan is used.
      if (copy_fields(&join->tmp_table_param, thd))
        return NESTED_LOOP_ERROR; /* purecov: inspected */
    }
    // Filter HAVING if not done earlier
    if (!having_is_true(join->having_cond))
      return NESTED_LOOP_OK;  // Didn't match having
    error = 0;
    if (join->should_send_current_row())
      error = join->select_lex->query_result()->send_data(thd, *fields);
    if (error) return NESTED_LOOP_ERROR; /* purecov: inspected */

    ++join->send_records;
    thd->get_stmt_da()->inc_current_row_for_condition();
    if (join->send_records >= join->unit->select_limit_cnt &&
        !join->do_send_rows) {
      /*
        If we have used Priority Queue for optimizing order by with limit,
        then stop here, there are no more records to consume.
        When this optimization is used, end_send is called on the next
        join_tab.
      */
      if (join->order && join->calc_found_rows && qep_tab > join->qep_tab &&
          qep_tab[-1].filesort && qep_tab[-1].filesort->using_pq) {
        DBUG_PRINT("info", ("filesort NESTED_LOOP_QUERY_LIMIT"));
        return NESTED_LOOP_QUERY_LIMIT;
      }
    }
    if (join->send_records >= join->unit->select_limit_cnt &&
        join->do_send_rows) {
      if (join->calc_found_rows) {
        join->do_send_rows = false;
        if (join->unit->fake_select_lex)
          join->unit->fake_select_lex->select_limit = 0;
        return NESTED_LOOP_OK;
      }
      return NESTED_LOOP_QUERY_LIMIT;  // Abort nicely
    } else if (join->send_records >= join->fetch_limit) {
      /*
        There is a server side cursor and all rows for
        this fetch request are sent.
      */
      return NESTED_LOOP_CURSOR_LIMIT;
    }
  }
  return NESTED_LOOP_OK;
}

/**
  Get exact count of rows in all tables. When this is called, at least one
  table's SE doesn't include HA_COUNT_ROWS_INSTANT.

    @param qep_tab      List of qep_tab in this JOIN.
    @param table_count  Count of qep_tab in the JOIN.
    @param error [out]  Return any possible error. Else return 0

    @returns
      Cartesian product of count of the rows in all tables if success
      0 if error.

  @note The "error" parameter is required for the sake of testcases like the
        one in innodb-wl6742.test:272. Earlier if an error was raised by
        ha_records, it wasn't handled by get_exact_record_count. Instead it was
        just allowed to go to the execution phase, where end_send_group would
        see the same error and raise it.

        But with the new function 'end_send_count' in the execution phase,
        such an error should be properly returned so that it can be raised.
*/
ulonglong get_exact_record_count(QEP_TAB *qep_tab, uint table_count,
                                 int *error) {
  ulonglong count = 1;
  QEP_TAB *qt;

  for (uint i = 0; i < table_count; i++) {
    ha_rows tmp = 0;
    qt = qep_tab + i;

    if (qt->type() == JT_ALL || (qt->index() == qt->table()->s->primary_key &&
                                 qt->table()->file->primary_key_is_clustered()))
      *error = qt->table()->file->ha_records(&tmp);
    else
      *error = qt->table()->file->ha_records(&tmp, qt->index());
    if (*error != 0) {
      (void)report_handler_error(qt->table(), *error);
      return 0;
    }
    count *= tmp;
  }
  *error = 0;
  return count;
}

enum_nested_loop_state end_send_count(JOIN *join, QEP_TAB *qep_tab) {
  List_iterator_fast<Item> it(join->all_fields);
  Item *item;
  int error = 0;
  THD *thd = join->thd;

  while ((item = it++)) {
    if (item->type() == Item::SUM_FUNC_ITEM &&
        (((Item_sum *)item))->sum_func() == Item_sum::COUNT_FUNC) {
      ulonglong count =
          get_exact_record_count(qep_tab, join->primary_tables, &error);
      if (error) return NESTED_LOOP_ERROR;

      ((Item_sum_count *)item)->make_const((longlong)count);
    }
  }

  /*
    Copy non-aggregated items in the result set.
    Handles queries like:
    SET @s =1;
    SELECT @s, COUNT(*) FROM t1;
  */
  if (copy_fields(&join->tmp_table_param, thd)) return NESTED_LOOP_ERROR;

  if (having_is_true(join->having_cond) && join->should_send_current_row()) {
    if (join->select_lex->query_result()->send_data(thd, *join->fields))
      return NESTED_LOOP_ERROR;
    join->send_records++;
  }

  return NESTED_LOOP_OK;
}

/* ARGSUSED */
enum_nested_loop_state end_send_group(JOIN *join, QEP_TAB *qep_tab,
                                      bool end_of_records) {
  int idx = -1;
  enum_nested_loop_state ok_code = NESTED_LOOP_OK;
  DBUG_TRACE;
  THD *thd = join->thd;

  List<Item> *fields;
  if (qep_tab) {
    DBUG_ASSERT(qep_tab - 1 == join->ref_slice_immediately_before_group_by);
    fields = &join->tmp_fields_list[REF_SLICE_ORDERED_GROUP_BY];
  } else
    fields = join->fields;

  /*
    (1) Haven't seen a first row yet
    (2) Have seen all rows
    (3) GROUP expression are different from previous row's
  */
  if (!join->seen_first_record ||                                     // (1)
      end_of_records ||                                               // (2)
      (idx = update_item_cache_if_changed(join->group_fields)) >= 0)  // (3)
  {
    if (!join->group_sent &&
        (join->seen_first_record ||
         (end_of_records && !join->grouped && !join->group_optimized_away))) {
      if (idx < (int)join->send_group_parts) {
        /*
          As GROUP expressions have changed, we now send forward the group
          of the previous row.
          While end_write_group() has a real tmp table as output,
          end_send_group() has a pseudo-table, made of a list of Item_copy
          items (created by setup_copy_fields()) which are accessible through
          REF_SLICE_ORDERED_GROUP_BY. This is equivalent to one row where the
          current group is accumulated. The creation of a new group in the
          pseudo-table happens in this function (call to
          init_sum_functions()); the update of an existing group also happens
          in this function (call to update_sum_func()); the reading of an
          existing group happens right below.
          As we are now reading from pseudo-table REF_SLICE_ORDERED_GROUP_BY, we
          switch to this slice; we should not have switched when calculating
          group expressions in update_item_cache_if_changed() above; indeed
          these group expressions need the current row of the input table, not
          what is in this slice (which is generally the last completed group so
          is based on some previous row of the input table).
        */
        Switch_ref_item_slice slice_switch(join, REF_SLICE_ORDERED_GROUP_BY);
        DBUG_ASSERT(fields == join->get_current_fields());
        int error = 0;
        {
          table_map save_nullinfo = 0;
          if (!join->seen_first_record) {
            // Calculate aggregate functions for no rows
            for (Item &item : *fields) {
              item.no_rows_in_result();
            }

            /*
              Mark tables as containing only NULL values for processing
              the HAVING clause and for send_data().
              Calculate a set of tables for which NULL values need to
              be restored after sending data.
            */
            if (join->clear_fields(&save_nullinfo))
              return NESTED_LOOP_ERROR; /* purecov: inspected */
          }
          if (!having_is_true(join->having_cond))
            error = -1;  // Didn't satisfy having
          else {
            if (join->should_send_current_row())
              error = join->select_lex->query_result()->send_data(thd, *fields);
            join->send_records++;
            thd->get_stmt_da()->inc_current_row_for_condition();
            join->group_sent = true;
          }
          if (join->rollup.state != ROLLUP::STATE_NONE && error <= 0) {
            if (join->rollup_send_data((uint)(idx + 1))) error = 1;
          }
          // Restore NULL values if needed.
          if (save_nullinfo) join->restore_fields(save_nullinfo);
        }
        if (error > 0) return NESTED_LOOP_ERROR; /* purecov: inspected */
        if (end_of_records) return NESTED_LOOP_OK;
        if (join->send_records >= join->unit->select_limit_cnt &&
            join->do_send_rows) {
          if (!join->calc_found_rows)
            return NESTED_LOOP_QUERY_LIMIT;  // Abort nicely
          join->do_send_rows = false;
          join->unit->select_limit_cnt = HA_POS_ERROR;
        } else if (join->send_records >= join->fetch_limit) {
          /*
            There is a server side cursor and all rows
            for this fetch request are sent.
          */
          /*
            Preventing code duplication. When finished with the group reset
            the group functions and copy_fields. We fall through. bug #11904
          */
          ok_code = NESTED_LOOP_CURSOR_LIMIT;
        }
      }
    } else {
      if (end_of_records) return NESTED_LOOP_OK;
      join->seen_first_record = true;
      // Initialize the cache of GROUP expressions with this 1st row's values
      (void)(update_item_cache_if_changed(join->group_fields));
    }
    if (idx < (int)join->send_group_parts) {
      /*
        This branch is executed also for cursors which have finished their
        fetch limit - the reason for ok_code.

        As GROUP expressions have changed, initialize the new group:
        (1) copy non-aggregated expressions (they're constant over the group)
        (2) and reset group aggregate functions.

        About (1): some expressions to copy are not Item_fields and they are
        copied by copy_fields() which evaluates them (see
        param->grouped_expressions, set up in setup_copy_fields()). Thus,
        copy_fields() can evaluate functions. One of them, F2, may reference
        another one F1, example: SELECT expr AS F1 ... GROUP BY ... HAVING
        F2(F1)<=2 . Assume F1 and F2 are not aggregate functions. Then they are
        calculated by copy_fields() when starting a new group, i.e. here. As F2
        uses an alias to F1, F1 is calculated first; F2 must use that value (not
        evaluate expr again, as expr may not be deterministic), so F2 uses a
        reference (Item_ref) to the already-computed value of F1; that value is
        in Item_copy part of REF_SLICE_ORDERED_GROUP_BY. So, we switch to that
        slice.
      */
      Switch_ref_item_slice slice_switch(join, REF_SLICE_ORDERED_GROUP_BY);
      if (copy_fields(&join->tmp_table_param, thd))  // (1)
        return NESTED_LOOP_ERROR;
      if (init_sum_functions(join->sum_funcs,
                             join->sum_funcs_end[idx + 1]))  //(2)
        return NESTED_LOOP_ERROR;
      join->group_sent = false;
      return ok_code;
    }
  }
  if (update_sum_func(join->sum_funcs)) return NESTED_LOOP_ERROR;
  return NESTED_LOOP_OK;
}

static bool cmp_field_value(Field *field, ptrdiff_t diff) {
  DBUG_ASSERT(field);
  /*
    Records are different when:
    1) NULL flags aren't the same
    2) length isn't the same
    3) data isn't the same
  */
  const bool value1_isnull = field->is_real_null();
  const bool value2_isnull = field->is_real_null(diff);

  if (value1_isnull != value2_isnull)  // 1
    return true;
  if (value1_isnull) return false;  // Both values are null, no need to proceed.

  const size_t value1_length = field->data_length();
  const size_t value2_length = field->data_length(diff);

  if (field->type() == MYSQL_TYPE_JSON) {
    Field_json *json_field = down_cast<Field_json *>(field);

    // Fetch the JSON value on the left side of the comparison.
    Json_wrapper left_wrapper;
    if (json_field->val_json(&left_wrapper))
      return true; /* purecov: inspected */

    // Fetch the JSON value on the right side of the comparison.
    Json_wrapper right_wrapper;
    json_field->ptr += diff;
    bool err = json_field->val_json(&right_wrapper);
    json_field->ptr -= diff;
    if (err) return true; /* purecov: inspected */

    return (left_wrapper.compare(right_wrapper) != 0);
  }

  // Trailing space can't be skipped and length is different
  if (!field->is_text_key_type() && value1_length != value2_length)  // 2
    return true;

  if (field->cmp_max(field->ptr, field->ptr + diff,  // 3
                     std::max(value1_length, value2_length)))
    return true;

  return false;
}

/**
  Compare GROUP BY in from tmp table's record[0] and record[1]

  @returns
    true  records are different
    false records are the same
*/

static bool group_rec_cmp(ORDER *group, uchar *rec0, uchar *rec1) {
  DBUG_TRACE;
  ptrdiff_t diff = rec1 - rec0;

  for (ORDER *grp = group; grp; grp = grp->next) {
    Field *field = grp->field_in_tmp_table;
    if (cmp_field_value(field, diff)) return true;
  }
  return false;
}

/**
  Compare GROUP BY in from tmp table's record[0] and record[1]

  @returns
    true  records are different
    false records are the same
*/

static bool table_rec_cmp(TABLE *table) {
  DBUG_TRACE;
  ptrdiff_t diff = table->record[1] - table->record[0];
  Field **fields = table->visible_field_ptr();

  for (uint i = 0; i < table->visible_field_count(); i++) {
    Field *field = fields[i];
    if (cmp_field_value(field, diff)) return true;
  }
  return false;
}

/**
  Generate hash for a field

  @returns generated hash
*/

ulonglong unique_hash(const Field *field, ulonglong *hash_val) {
  const uchar *pos, *end;
  uint64 seed1 = 0, seed2 = 4;
  ulonglong crc = *hash_val;

  if (field->is_null()) {
    /*
      Change crc in a way different from an empty string or 0.
      (This is an optimisation;  The code will work even if
      this isn't done)
    */
    crc = ((crc << 8) + 511 + (crc >> (8 * sizeof(ha_checksum) - 8)));
    goto finish;
  }

  pos = field->get_ptr();
  end = pos + field->data_length();

  if (field->type() == MYSQL_TYPE_JSON) {
    const Field_json *json_field = down_cast<const Field_json *>(field);

    crc = json_field->make_hash_key(*hash_val);
  } else if (field->key_type() == HA_KEYTYPE_TEXT ||
             field->key_type() == HA_KEYTYPE_VARTEXT1 ||
             field->key_type() == HA_KEYTYPE_VARTEXT2) {
    field->charset()->coll->hash_sort(field->charset(), (const uchar *)pos,
                                      field->data_length(), &seed1, &seed2);
    crc ^= seed1;
  } else
    while (pos != end)
      crc = ((crc << 8) + (*pos++)) + (crc >> (8 * sizeof(ha_checksum) - 8));
finish:
  *hash_val = crc;
  return crc;
}

/**
  Generate hash for unique constraint according to group-by list.

  This reads the values of the GROUP BY expressions from fields so assumes
  those expressions have been computed and stored into fields of a temporary
  table; in practice this means that copy_fields() and copy_funcs() must have
  been called.
*/

static ulonglong unique_hash_group(ORDER *group) {
  DBUG_TRACE;
  ulonglong crc = 0;

  for (ORDER *ord = group; ord; ord = ord->next) {
    Field *field = ord->field_in_tmp_table;
    DBUG_ASSERT(field);
    unique_hash(field, &crc);
  }

  return crc;
}

/* Generate hash for unique_constraint for all visible fields of a table */

static ulonglong unique_hash_fields(TABLE *table) {
  ulonglong crc = 0;
  Field **fields = table->visible_field_ptr();

  for (uint i = 0; i < table->visible_field_count(); i++)
    unique_hash(fields[i], &crc);

  return crc;
}

/**
  Check unique_constraint.

  @details Calculates record's hash and checks whether the record given in
  table->record[0] is already present in the tmp table.

  @param table JOIN_TAB of tmp table to check

  @note This function assumes record[0] is already filled by the caller.
  Depending on presence of table->group, it's or full list of table's fields
  are used to calculate hash.

  @returns
    false same record was found
    true  record wasn't found
*/

bool check_unique_constraint(TABLE *table) {
  ulonglong hash;

  if (!table->hash_field) return true;

  if (table->no_keyread) return true;

  if (table->group)
    hash = unique_hash_group(table->group);
  else
    hash = unique_hash_fields(table);
  table->hash_field->store(hash, true);
  int res =
      table->file->ha_index_read_map(table->record[1], table->hash_field->ptr,
                                     HA_WHOLE_KEY, HA_READ_KEY_EXACT);
  while (!res) {
    // Check whether records are the same.
    if (!(table->group
              ? group_rec_cmp(table->group, table->record[0], table->record[1])
              : table_rec_cmp(table)))
      return false;  // skip it
    res = table->file->ha_index_next_same(table->record[1],
                                          table->hash_field->ptr, sizeof(hash));
  }
  return true;
}

/**
  Minion for reset_framing_wf_states and reset_non_framing_wf_state, q.v.

  @param func_ptr     the set of functions
  @param framing      true if we want to reset for framing window functions
*/
static inline void reset_wf_states(Func_ptr_array *func_ptr, bool framing) {
  for (auto it : *func_ptr) {
    (void)it.func()->walk(&Item::reset_wf_state, enum_walk::POSTFIX,
                          (uchar *)&framing);
  }
}
/**
  Walk the function calls and reset any framing window function's window state.

  @param func_ptr   an array of function call items which might represent
                    or contain window function calls
*/
static inline void reset_framing_wf_states(Func_ptr_array *func_ptr) {
  reset_wf_states(func_ptr, true);
}

/**
  Walk the function calls and reset any non-framing window function's window
  state.

  @param func_ptr   an array of function call items which might represent
                    or contain window function calls
 */
static inline void reset_non_framing_wf_state(Func_ptr_array *func_ptr) {
  reset_wf_states(func_ptr, false);
}

/**
  Save a window frame buffer to frame buffer temporary table.

  @param thd      The current thread
  @param w        The current window
  @param rowno    The rowno in the current partition (1-based)
*/
static bool buffer_record_somewhere(THD *thd, Window *w, int64 rowno) {
  DBUG_TRACE;
  TABLE *const t = w->frame_buffer();
  uchar *record = t->record[0];

  DBUG_ASSERT(rowno != Window::FBC_FIRST_IN_NEXT_PARTITION);
  DBUG_ASSERT(t->is_created());

  if (!t->file->inited) {
    /*
      On the frame buffer table, t->file, we do several things in the
      windowing code:
      - read a row by position,
      - read rows after that row,
      - write a row,
      - find the position of a just-written row, if it's first in partition.
      To prepare for reads, we initialize a scan once for all with
      ha_rnd_init(), with argument=true as we'll use ha_rnd_next().
      To read a row, we use ha_rnd_pos() or ha_rnd_next().
      To write, we use ha_write_row().
      To find the position of a just-written row, we are in the following
      conditions:
      - the written row is first of its partition
      - before writing it, we have processed the previous partition, and that
      process ended with a read of the previous partition's last row
      - so, before the write, the read cursor is already positioned on that
      last row.
      Then we do the write; the new row goes after the last row; then
      ha_rnd_next() reads the row after the last row, i.e. reads the written
      row. Then position() gives the position of the written row.
    */
    int rc = t->file->ha_rnd_init(true);
    if (rc != 0) {
      t->file->print_error(rc, MYF(0));
      return true;
    }
  }

  int error = t->file->ha_write_row(record);
  w->set_frame_buffer_total_rows(w->frame_buffer_total_rows() + 1);

  constexpr size_t first_in_partition = static_cast<size_t>(
      Window_retrieve_cached_row_reason::FIRST_IN_PARTITION);

  if (error) {
    /* If this is a duplicate error, return immediately */
    if (t->file->is_ignorable_error(error)) return true;

    /* Other error than duplicate error: Attempt to create a temporary table. */
    bool is_duplicate;
    if (create_ondisk_from_heap(thd, t, error, true, &is_duplicate)) return -1;

    DBUG_ASSERT(t->s->db_type() == innodb_hton);
    if (t->file->ha_rnd_init(true)) return true; /* purecov: inspected */

    /*
      Reset all hints since they all pertain to the in-memory file, not the
      new on-disk one.
    */
    for (size_t i = first_in_partition;
         i < Window::FRAME_BUFFER_POSITIONS_CARD +
                 w->opt_nth_row().m_offsets.size() +
                 w->opt_lead_lag().m_offsets.size();
         i++) {
      void *r = (*THR_MALLOC)->Alloc(t->file->ref_length);
      if (r == nullptr) return true;
      w->m_frame_buffer_positions[i].m_position = static_cast<uchar *>(r);
      w->m_frame_buffer_positions[i].m_rowno = -1;
    }

    if ((w->m_tmp_pos.m_position =
             (uchar *)(*THR_MALLOC)->Alloc(t->file->ref_length)) == nullptr)
      return true;

    w->m_frame_buffer_positions[first_in_partition].m_rowno = 1;
    /*
      The auto-generated primary key of the first row is 1. Our offset is
      also one-based, so we can use w->frame_buffer_partition_offset() "as is"
      to construct the position.
    */
    encode_innodb_position(
        w->m_frame_buffer_positions[first_in_partition].m_position,
        t->file->ref_length, w->frame_buffer_partition_offset());

    return is_duplicate ? true : false;
  }

  /* Save position in frame buffer file of first row in a partition */
  if (rowno == 1) {
    if (w->m_frame_buffer_positions.empty()) {
      w->m_frame_buffer_positions.init(thd->mem_root);
      /* lazy initialization of positions remembered */
      for (uint i = 0; i < Window::FRAME_BUFFER_POSITIONS_CARD +
                               w->opt_nth_row().m_offsets.size() +
                               w->opt_lead_lag().m_offsets.size();
           i++) {
        void *r = (*THR_MALLOC)->Alloc(t->file->ref_length);
        if (r == nullptr) return true;
        Window::Frame_buffer_position p(static_cast<uchar *>(r), -1);
        w->m_frame_buffer_positions.push_back(p);
      }

      if ((w->m_tmp_pos.m_position =
               (uchar *)(*THR_MALLOC)->Alloc(t->file->ref_length)) == nullptr)
        return true;
    }

    // Do a read to establish scan position, then get it
    error = t->file->ha_rnd_next(record);
    t->file->position(record);
    std::memcpy(w->m_frame_buffer_positions[first_in_partition].m_position,
                t->file->ref, t->file->ref_length);
    w->m_frame_buffer_positions[first_in_partition].m_rowno = 1;
    w->set_frame_buffer_partition_offset(w->frame_buffer_total_rows());
  }

  return false;
}

/**
  If we cannot evaluate all window functions for a window on the fly, buffer the
  current row for later processing by process_buffered_windowing_record.

  @param thd                Current thread
  @param param              The temporary table parameter

  @param[in,out] new_partition If input is not nullptr:
                            sets the bool pointed to to true if a new partition
                            was found and there was a previous partition; if
                            so the buffering of the first row in new
                            partition isn't done and must be repeated
                            later: we save away the row as rowno
                            FBC_FIRST_IN_NEXT_PARTITION, then fetch it back
                            later, cf. end_write_wf.
                            If input is nullptr, this is the "later" call to
                            buffer the first row of the new partition:
                            buffer the row.
  @return true if error.
*/
bool buffer_windowing_record(THD *thd, Temp_table_param *param,
                             bool *new_partition) {
  DBUG_TRACE;
  Window *w = param->m_window;

  if (copy_fields(w->frame_buffer_param(), thd)) return true;

  if (new_partition != nullptr) {
    const bool first_partition = w->partition_rowno() == 0;
    w->check_partition_boundary();

    if (!first_partition && w->partition_rowno() == 1) {
      *new_partition = true;
      w->save_special_record(Window::FBC_FIRST_IN_NEXT_PARTITION,
                             w->frame_buffer());
      return false;
    }
  }

  /*
    The record is now ready in TABLE and can be saved. The window
    function(s) on the window have not yet been evaluated, but
    will be evaluated when we read frame rows back, before the end wf result
    (usually ready in the last read when the last frame row has been read back)
    can be produced. E.g. SUM(i): we save away all rows in partition.
    We read back rows in current row's frame, producing the total SUM in the
    last read back row. That value for SUM will then be used for the current row
    output.
  */

  if (w->needs_restore_input_row()) {
    w->save_special_record(Window::FBC_LAST_BUFFERED_ROW, w->frame_buffer());
  }

  if (buffer_record_somewhere(thd, w, w->partition_rowno())) return true;

  w->set_last_rowno_in_cache(w->partition_rowno());

  return false;
}

/**
  Read row rowno from frame buffer tmp file using cached row positions to
  minimize positioning work.
*/
static bool read_frame_buffer_row(int64 rowno, Window *w,
#ifndef DBUG_OFF
                                  bool for_nth_value)
#else
                                  bool for_nth_value MY_ATTRIBUTE((unused)))
#endif
{
  int use_idx = 0;  // closest prior position found, a priori 0 (row 1)
  int diff = w->last_rowno_in_cache();  // maximum a priori
  TABLE *t = w->frame_buffer();

  // Find the saved position closest to where we want to go
  for (int i = w->m_frame_buffer_positions.size() - 1; i >= 0; i--) {
    auto cand = w->m_frame_buffer_positions[i];
    if (cand.m_rowno == -1 || cand.m_rowno > rowno) continue;

    if (rowno - cand.m_rowno < diff) {
      /* closest so far */
      diff = rowno - cand.m_rowno;
      use_idx = i;
    }
  }

  auto cand = &w->m_frame_buffer_positions[use_idx];

  int error =
      t->file->ha_rnd_pos(w->frame_buffer()->record[0], cand->m_position);
  if (error) {
    t->file->print_error(error, MYF(0));
    return true;
  }

  if (rowno > cand->m_rowno) {
    /*
      The saved position didn't correspond exactly to where we want to go, but
      is located one or more rows further out on the file, so read next to move
      forward to desired row.
    */
    const int64 cnt = rowno - cand->m_rowno;

    /*
      We should have enough location hints to normally need only one extra read.
      If we have just switched to INNODB due to MEM overflow, a rescan is
      required, so skip assert if we have INNODB.
    */
    DBUG_ASSERT(w->frame_buffer()->s->db_type()->db_type == DB_TYPE_INNODB ||
                cnt <= 1 ||
                // unless we have a frame beyond the current row, 1. time
                // in which case we need to do some scanning...
                (w->last_row_output() == 0 &&
                 w->frame()->m_from->m_border_type == WBT_VALUE_FOLLOWING) ||
                // or unless we are search for NTH_VALUE, which can be in the
                // middle of a frame, and with RANGE frames it can jump many
                // positions from one frame to the next with optimized eval
                // strategy
                for_nth_value);

    for (int i = 0; i < cnt; i++) {
      error = t->file->ha_rnd_next(t->record[0]);
      if (error) {
        t->file->print_error(error, MYF(0));
        return true;
      }
    }
  }

  return false;
}

#if !defined(DBUG_OFF)
inline static void dbug_allow_write_all_columns(
    Temp_table_param *param, std::map<TABLE *, my_bitmap_map *> &map) {
  for (auto &copy_field : param->copy_fields) {
    TABLE *const t = copy_field.from_field()->table;
    if (t != nullptr) {
      auto it = map.find(t);
      if (it == map.end())
        map.insert(it, std::pair<TABLE *, my_bitmap_map *>(
                           t, dbug_tmp_use_all_columns(t, t->write_set)));
    }
  }
}

inline static void dbug_restore_all_columns(
    std::map<TABLE *, my_bitmap_map *> &map) {
  auto func = [](std::pair<TABLE *const, my_bitmap_map *> &e) {
    dbug_tmp_restore_column_map(e.first->write_set, e.second);
  };

  std::for_each(map.begin(), map.end(), func);
}
#endif

/**
  Bring back buffered data to the record of qep_tab-1 [1], and optionally
  execute copy_fields() to the OUT table.

  [1] This is not always the case. For the first window, if we have no
  PARTITION BY or ORDER BY in the window, and there is more than one table
  in the join, the logical input can consist of more than one table
  (qep_tab-1 .. qep_tab-n), so the record accordingly.

  This method works by temporarily reversing the "normal" direction of the field
  copying.

  Also make a note of the position of the record we retrieved in the window's
  m_frame_buffer_positions to be able to optimize succeeding retrievals.

  @param thd       The current thread
  @param w         The current window
  @param out_param OUT table; if not nullptr, does copy_fields() to OUT
  @param rowno     The row number (in the partition) to set up
  @param reason    What kind of row to retrieve
  @param fno       Used with NTH_VALUE and LEAD/LAG to specify which
                   window function's position cache to use, i.e. what index
                   of m_frame_buffer_positions to update. For the second
                   LEAD/LAG window function in a query, the index would be
                   REA_MISC_POSITIONS (reason) + \<no of NTH functions\> + 2.

  @return true on error
*/
bool bring_back_frame_row(THD *thd, Window *w, Temp_table_param *out_param,
                          int64 rowno, Window_retrieve_cached_row_reason reason,
                          int fno) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("rowno: %" PRId64 " reason: %d fno: %d", rowno,
                       static_cast<int>(reason), fno));
  DBUG_ASSERT(reason == Window_retrieve_cached_row_reason::MISC_POSITIONS ||
              fno == 0);

  uchar *fb_rec = w->frame_buffer()->record[0];

  DBUG_ASSERT(rowno != 0);

  /*
    If requested row is the last we fetched from FB and copied to OUT, we
    don't need to fetch and copy again.
    Because "reason", "fno" may differ from the last call which fetched the
    row, we still do the updates of w.m_frame_buffer_positions even if
    do_fetch=false.
  */
  bool do_fetch;

  if (rowno == Window::FBC_FIRST_IN_NEXT_PARTITION) {
    do_fetch = true;
    w->restore_special_record(rowno, fb_rec);
  } else if (rowno == Window::FBC_LAST_BUFFERED_ROW) {
    do_fetch = w->row_has_fields_in_out_table() != w->last_rowno_in_cache();
    if (do_fetch) w->restore_special_record(rowno, fb_rec);
  } else {
    DBUG_ASSERT(reason != Window_retrieve_cached_row_reason::WONT_UPDATE_HINT);
    do_fetch = w->row_has_fields_in_out_table() != rowno;

    if (do_fetch &&
        read_frame_buffer_row(
            rowno, w,
            reason == Window_retrieve_cached_row_reason::MISC_POSITIONS))
      return true;

    /* Got row rowno in record[0], remember position */
    const TABLE *const t = w->frame_buffer();
    t->file->position(fb_rec);
    std::memcpy(
        w->m_frame_buffer_positions[static_cast<int>(reason) + fno].m_position,
        t->file->ref, t->file->ref_length);
    w->m_frame_buffer_positions[static_cast<int>(reason) + fno].m_rowno = rowno;
  }

  if (!do_fetch) return false;

  Temp_table_param *const fb_info = w->frame_buffer_param();

#if !defined(DBUG_OFF)
  /*
    Since we are copying back a row from the frame buffer to the input table's
    buffer, we will be copying into fields that are not necessarily marked as
    writeable. To eliminate problems with ASSERT_COLUMN_MARKED_FOR_WRITE, we
    set all fields writeable. This is only
    applicable in debug builds, since ASSERT_COLUMN_MARKED_FOR_WRITE is debug
    only.
  */
  std::map<TABLE *, my_bitmap_map *> saved_map;
  dbug_allow_write_all_columns(fb_info, saved_map);
#endif

  /*
    Do the inverse of copy_fields to get the row's fields back to the input
    table from the frame buffer.
  */
  bool rc = copy_fields(fb_info, thd, true);

#if !defined(DBUG_OFF)
  dbug_restore_all_columns(saved_map);
#endif

  if (!rc) {
    if (out_param) {
      if (copy_fields(out_param, thd)) return true;
      // fields are in IN and in OUT
      if (rowno >= 1) w->set_row_has_fields_in_out_table(rowno);
    } else
      // we only wrote IN record, so OUT and IN are inconsistent
      w->set_row_has_fields_in_out_table(0);
  }

  return rc;
}

/**
  Save row special_rowno in table t->record[0] to an in-memory copy for later
  restoration.
*/
void Window::save_special_record(uint64 special_rowno, TABLE *t) {
  DBUG_PRINT("info", ("save_special_record: %" PRIu64, special_rowno));
  size_t l = t->s->reclength;
  DBUG_ASSERT(m_special_rows_cache_max_length >= l);  // check room.
  // From negative enum, get proper array index:
  int idx = FBC_FIRST_KEY - special_rowno;
  m_special_rows_cache_length[idx] = l;
  std::memcpy(m_special_rows_cache + idx * m_special_rows_cache_max_length,
              t->record[0], l);
}

/**
  Restore row special_rowno into record from in-memory copy. Any fields not
  the result of window functions are not used, but they do tag along here
  (unnecessary copying..). BLOBs: have storage in result_field of Item
  for the window function although the pointer is copied here. The
  result field storage is stable across reads from the frame buffer, so safe.
*/
void Window::restore_special_record(uint64 special_rowno, uchar *record) {
  DBUG_PRINT("info", ("restore_special_record: %" PRIu64, special_rowno));
  int idx = FBC_FIRST_KEY - special_rowno;
  size_t l = m_special_rows_cache_length[idx];
  std::memcpy(record,
              m_special_rows_cache + idx * m_special_rows_cache_max_length, l);
  // Sometimes, "record" points to IN record
  set_row_has_fields_in_out_table(0);
}

/**
  Process window functions that need partition cardinality
*/
static bool process_wfs_needing_card(
    THD *thd, Temp_table_param *param, const Window::st_nth &have_nth_value,
    const Window::st_lead_lag &have_lead_lag, const int64 current_row,
    Window *w, Window_retrieve_cached_row_reason current_row_reason) {
  w->set_rowno_being_visited(current_row);

  // Reset state for LEAD/LAG functions
  if (!have_lead_lag.m_offsets.empty()) w->reset_lead_lag();

  // This also handles LEAD(.., 0)
  if (copy_funcs(param, thd, CFT_WF_NEEDS_CARD)) return true;

  if (!have_lead_lag.m_offsets.empty()) {
    int fno = 0;
    const int nths = have_nth_value.m_offsets.size();

    for (auto &ll : have_lead_lag.m_offsets) {
      const int64 rowno_to_visit = current_row - ll.m_rowno;

      if (rowno_to_visit == current_row)
        continue;  // Already processed above above

      /*
        Note that this value can be outside partition, even negative: if so,
        the default will applied, if any is provided.
      */
      w->set_rowno_being_visited(rowno_to_visit);

      if (rowno_to_visit >= 1 && rowno_to_visit <= w->last_rowno_in_cache()) {
        if (bring_back_frame_row(
                thd, w, param, rowno_to_visit,
                Window_retrieve_cached_row_reason::MISC_POSITIONS,
                nths + fno++))
          return true;
      }

      if (copy_funcs(param, thd, CFT_WF_NEEDS_CARD)) return true;
    }
    /* Bring back the fields for the output row */
    if (bring_back_frame_row(thd, w, param, current_row, current_row_reason))
      return true;
  }

  return false;
}

/**
  While there are more unprocessed rows ready to process given the current
  partition/frame state, process such buffered rows by evaluating/aggregating
  the window functions defined over this window on the current frame, moving
  the frame if required.

  This method contains the main execution time logic of the evaluation
  window functions if we need buffering for one or more of the window functions
  defined on the window.

  Moving (sliding) frames can be executed using a naive or optimized strategy
  for aggregate window functions, like SUM or AVG (but not MAX, or MIN).
  In the naive approach, for each row considered for processing from the buffer,
  we visit all the rows defined in the frame for that row, essentially leading
  to N*M complexity, where N is the number of rows in the result set, and M is
  the number for rows in the frame. This can be slow for large frames,
  obviously, so we can choose an optimized evaluation strategy using inversion.
  This means that when rows leave the frame as we move it forward, we re-use
  the previous aggregate state, but compute the *inverse* function to eliminate
  the contribution to the aggregate by the row(s) leaving the frame, and then
  use the normal aggregate function to add the contribution of the rows moving
  into the frame. The present method contains code paths for both strategies.

  For integral data types, this is safe in the sense that the result will be the
  same if no overflow occurs during normal evaluation. For floating numbers,
  optimizing in this way may lead to different results, so it is not done by
  default, cf the session variable "windowing_use_high_precision".

  Since the evaluation strategy is chosen based on the "most difficult" window
  function defined on the window, we must also be able to evaluate
  non-aggregates like ROW_NUMBER, NTILE, FIRST_VALUE in the code path of the
  optimized aggregates, so there is redundant code for those in the naive and
  optimized code paths. Note that NTILE forms a class of its own of the
  non-aggregates: it needs two passes over the partition's rows since the
  cardinality is needed to compute it. Furthermore, FIRST_VALUE and LAST_VALUE
  heed the frames, but they are not aggregates.

  The is a special optimized code path for *static aggregates*: when the window
  frame is the default, e.g. the entire partition and there is no ORDER BY
  specified, the value of the framing window functions, i.e. SUM, AVG,
  FIRST_VALUE, LAST_VALUE can be evaluated once and for all and saved when
  we visit and evaluate the first row of the partition. For later rows we
  restore the aggregate values and just fill in the other fields and evaluate
  non-framing window functions for the row.

  The code paths both for naive execution and optimized execution differ
  depending on whether we have ROW or RANGE boundaries in a explicit frame.

  A word on BLOBs. Below we make copies of rows into the frame buffer.
  This is a temporary table, so BLOBs get copied in the normal way.

  Sometimes we save records containing already computed framing window
  functions away into memory only: is the lifetime of the referenced BLOBs long
  enough? We have two cases:

  BLOB results from wfs: Any BLOB results will reside in the copies in result
  fields of the Items ready for the out file, so they no longer need any BLOB
  memory read from the frame buffer tmp file.

  BLOB fields not evaluated by wfs: Any other BLOB field will be copied as
  well, and would not have life-time past the next read from the frame buffer,
  but they are never used since we fill in the fields from the current row
  after evaluation of the window functions, so we don't need to make special
  copies of such BLOBs. This can be (and was) tested by shredding any BLOBs
  deallocated by InnoDB at the next read.

  We also save away in memory the next record of the next partition while
  processing the current partition. Any blob there will have its storage from
  the read of the input file, but we won't be touching that for reading again
  until after we start processing the next partition and save the saved away
  next partition row to the frame buffer.

  Note that the logic of this function is centered around the window, not
  around the window function. It is about putting rows in a partition,
  in a frame, in a set of peers, and passing this information to all window
  functions attached to this window; each function looks at the partition,
  frame, or peer set in its own particular way (for example RANK looks at the
  partition, SUM looks at the frame).

  @param thd                    Current thread
  @param param                  Current temporary table
  @param new_partition_or_eof   True if (we are about to start a new partition
                                and there was a previous partition) or eof
  @param[out] output_row_ready  True if there is a row record ready to write
                                to the out table

  @return true if error
*/
bool process_buffered_windowing_record(THD *thd, Temp_table_param *param,
                                       const bool new_partition_or_eof,
                                       bool *output_row_ready) {
  DBUG_TRACE;
  /**
    The current window
  */
  Window &w = *param->m_window;

  /**
    The frame
  */
  const PT_frame *f = w.frame();

  *output_row_ready = false;

  /**
    This is the row we are currently considering for processing and getting
    ready for output, cf. output_row_ready.
  */
  const int64 current_row = w.last_row_output() + 1;

  /**
    This is the row number of the last row we have buffered so far.
  */
  const int64 last_rowno_in_cache = w.last_rowno_in_cache();

  if (current_row > last_rowno_in_cache)  // already sent all buffered rows
    return false;

  /**
    If true, use code path for static aggregates
  */
  const bool static_aggregate = w.static_aggregates();

  /**
    If true, use code path for ROW bounds with optimized strategy
  */
  const bool row_optimizable = w.optimizable_row_aggregates();

  /**
    If true, use code path for RANGE bounds with optimized strategy
  */
  const bool range_optimizable = w.optimizable_range_aggregates();

  // These three strategies are mutually exclusive:
  DBUG_ASSERT((static_aggregate + row_optimizable + range_optimizable) <= 1);

  /**
    We need to evaluate FIRST_VALUE, or optimized MIN/MAX
  */
  const bool have_first_value = w.opt_first_row();

  /**
    We need to evaluate LAST_VALUE, or optimized MIN/MAX
  */
  const bool have_last_value = w.opt_last_row();

  /**
    We need to evaluate NTH_VALUE
  */
  const Window::st_nth &have_nth_value = w.opt_nth_row();

  /**
    We need to evaluate LEAD/LAG rows
  */

  const Window::st_lead_lag &have_lead_lag = w.opt_lead_lag();

  /**
    True if an inversion optimization strategy is used. For common
    code paths.
  */
  const bool optimizable = (row_optimizable || range_optimizable);

  /**
    RANGE was specified as the bounds unit for the frame
  */
  const bool range_frame = f->m_unit == WFU_RANGE;

  const bool range_to_current_row =
      range_frame && f->m_to->m_border_type == WBT_CURRENT_ROW;

  const bool range_from_first_to_current_row =
      range_to_current_row &&
      f->m_from->m_border_type == WBT_UNBOUNDED_PRECEDING;
  /**
    UNBOUNDED FOLLOWING was specified for the frame
  */
  bool unbounded_following = false;

  /**
    Row_number of the first row in the frame. Invariant: lower_limit >= 1
    after initialization.
  */
  int64 lower_limit = 1;

  /**
    Row_number of the logically last row to be computed in the frame, may be
    higher than the number of rows in the partition. The actual highest row
    number is computed later, see upper below.
  */
  int64 upper_limit = 0;

  /**
    needs peerset of current row to evaluate a wf for the current row.
  */
  bool needs_peerset = w.needs_peerset();

  /**
    needs the last peer of the current row within a frame.
  */
  const bool needs_last_peer_in_frame = w.needs_last_peer_in_frame();

  DBUG_PRINT("enter", ("current_row: %" PRId64 ", new_partition_or_eof: %d",
                       current_row, new_partition_or_eof));

  /* Compute lower_limit, upper_limit and possibly unbounded_following */
  if (f->m_unit == WFU_RANGE) {
    lower_limit = w.first_rowno_in_range_frame();
    /*
      For RANGE frame, we first buffer all the rows in the partition due to the
      need to find last peer before first can be processed. This can be
      optimized,
      FIXME.
    */
    upper_limit = INT64_MAX;
  } else {
    DBUG_ASSERT(f->m_unit == WFU_ROWS);
    bool lower_within_limits = true;
    /* Determine lower border */
    int64 border =
        f->m_from->border() != nullptr ? f->m_from->border()->val_int() : 0;
    switch (f->m_from->m_border_type) {
      case WBT_CURRENT_ROW:
        lower_limit = current_row;
        break;
      case WBT_VALUE_PRECEDING:
        /*
          Example: 1 PRECEDING and current row== 2 => 1
                                   current row== 1 => 1
                                   current row== 3 => 2
        */
        lower_limit = std::max<int64>(current_row - border, 1);
        break;
      case WBT_VALUE_FOLLOWING:
        /*
          Example: 1 FOLLOWING and current row== 2 => 3
                                   current row== 1 => 2
                                   current row== 3 => 4
        */
        if (border <= (std::numeric_limits<int64>::max() - current_row))
          lower_limit = current_row + border;
        else {
          lower_within_limits = false;
          lower_limit = INT64_MAX;
        }
        break;
      case WBT_UNBOUNDED_PRECEDING:
        lower_limit = 1;
        break;
      case WBT_UNBOUNDED_FOLLOWING:
        DBUG_ASSERT(false);
        break;
    }

    /* Determine upper border */
    border = f->m_to->border() != nullptr ? f->m_to->border()->val_int() : 0;
    {
      switch (f->m_to->m_border_type) {
        case WBT_CURRENT_ROW:
          // we always have enough cache
          upper_limit = current_row;
          break;
        case WBT_VALUE_PRECEDING:
          upper_limit = current_row - border;
          break;
        case WBT_VALUE_FOLLOWING:
          if (border <= (std::numeric_limits<longlong>::max() - current_row))
            upper_limit = current_row + border;
          else {
            upper_limit = INT64_MAX;
            /*
              If both the border specifications are beyond numeric limits,
              the window frame is empty.
            */
            if (f->m_from->m_border_type == WBT_VALUE_FOLLOWING &&
                !lower_within_limits) {
              lower_limit = INT64_MAX;
              upper_limit = INT64_MAX - 1;
            }
          }
          break;
        case WBT_UNBOUNDED_FOLLOWING:
          unbounded_following = true;
          upper_limit = INT64_MAX;  // need whole partition
          break;
        case WBT_UNBOUNDED_PRECEDING:
          DBUG_ASSERT(false);
          break;
      }
    }
  }

  /*
    Determine if, given our current read and buffering state, we have enough
    buffered rows to compute an output row.

    Example: ROWS BETWEEN 1 PRECEDING and 3 FOLLOWING

    State:
    +---+-------------------------------+
    |   | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
    +---+-------------------------------+
    ^    1?         ^
    lower      last_rowno_in_cache
    (0)             (4)

    This state means:

    We have read 4 rows, cf. value of last_rowno_in_cache.
    We can now process row 1 since both lower (1-1=0) and upper (1+3=4) are less
    than or equal to 4, the last row in the cache so far.

    We can not process row 2 since: !(4 >= 2 + 3) and we haven't seen the last
    row in partition which means that the frame may not be full yet.

    If we have a window function that needs to know the partition cardinality,
    we also must buffer all records of the partition before processing.
  */

  if (!((lower_limit <= last_rowno_in_cache &&
         upper_limit <= last_rowno_in_cache &&
         !w.needs_card()) || /* we have cached enough rows */
        new_partition_or_eof /* we have cached all rows */))
    return false;  // We haven't read enough rows yet, so return

  w.set_rowno_in_partition(current_row);

  /*
    By default, we must:
    - if we are the first row of a partition, reset values for both
    non-framing and framing WFs
    - reset values for framing WFs (new current row = new frame = new
    values for WFs).

    Both resettings require restoring the row from the FB. And, as we have
    restored this row, we use this opportunity to compute non-framing
    does-not-need-card functions.

    The meaning of if statements below is that in some cases, we can avoid
    this default behaviour.

    For example, if we have static framing WFs, and this is not the
    partition's first row: the previous row's framing-WF values should be
    reused without change, so all the above resetting must be skipped;
    so row restoration isn't immediately needed; that and the computation of
    non-framing functions is then done in another later block of code.
    Likewise, if we have framing WFs with inversion, and it's not the
    first row of the partition, we must skip the resetting of framing WFs.
  */
  if (!static_aggregate || current_row == 1) {
    /*
      We need to reset functions. As part of it, their comparators need to
      update themselves to use the new row as base line. So, restore it:
    */
    if (bring_back_frame_row(thd, &w, param, current_row,
                             Window_retrieve_cached_row_reason::CURRENT))
      return true;

    if (current_row == 1)  // new partition
      reset_non_framing_wf_state(param->items_to_copy);
    if (!optimizable || current_row == 1)  // new frame
    {
      reset_framing_wf_states(param->items_to_copy);
    }  // else we remember state and update it for row 2..N

    /* E.g. ROW_NUMBER, RANK, DENSE_RANK */
    if (copy_funcs(param, thd, CFT_WF_NON_FRAMING)) return true;
    if (!optimizable || current_row == 1) {
      /*
        So far frame is empty; set up a flag which makes framing WFs set
        themselves to NULL in OUT.
      */
      w.set_do_copy_null(true);
      if (copy_funcs(param, thd, CFT_WF_FRAMING)) return true;
      w.set_do_copy_null(false);
    }  // else aggregates keep value of previous row, and we'll do inversion
  }

  if (range_frame) {
    /* establish current row as base-line for RANGE computation */
    w.reset_order_by_peer_set();
  }

  bool first_row_in_range_frame_seen = false;

  /**
    For optimized strategy we want to save away the previous aggregate result
    and reuse in later round by inversion. This keeps track of whether we
    managed to compute results for this current row (result are "primed"), so we
    can use inversion in later rows. Cf Window::m_aggregates_primed.
  */
  bool optimizable_primed = false;

  /**
    Possible adjustment of the logical upper_limit: no rows exist beyond
    last_rowno_in_cache.
  */
  const int64 upper = min(upper_limit, last_rowno_in_cache);

  /*
    Optimization: we evaluate the peer set of the current row potentially
    several times. Window functions like CUME_DIST sets needs_peerset and is
    evaluated last, so if any other wf evaluation led to finding the peer set
    of the current row, make a note of it, so we can skip doing it twice.
  */
  bool have_peers_current_row = false;

  if ((static_aggregate && current_row == 1) ||   // skip for row > 1
      (optimizable && !w.aggregates_primed()) ||  // skip for 2..N in frame
      (!static_aggregate && !optimizable))        // normal: no skip
  {
    // Compute and output current_row.
    int64 rowno;        ///< iterates over rows in a frame
    int64 skipped = 0;  ///< RANGE: # of visited rows seen before the frame

    for (rowno = lower_limit; rowno <= upper; rowno++) {
      if (optimizable) optimizable_primed = true;

      /*
        Set window frame state before computing framing window function.
        'n' is the number of row #rowno relative to the beginning of the
        frame, 1-based.
      */
      const int64 n = rowno - lower_limit + 1 - skipped;

      w.set_rowno_in_frame(n);
      w.set_rowno_being_visited(rowno);

      const Window_retrieve_cached_row_reason reason =
          (n == 1 ? Window_retrieve_cached_row_reason::FIRST_IN_FRAME
                  : Window_retrieve_cached_row_reason::LAST_IN_FRAME);
      /*
        Hint maintenance: we will normally read past last row in frame, so
        prepare to resurrect that hint once we do.
      */
      w.save_pos(reason);

      /* Set up the non-wf fields for aggregating to the output row. */
      if (bring_back_frame_row(thd, &w, param, rowno, reason)) return true;

      if (range_frame) {
        if (w.before_frame()) {
          skipped++;
          continue;
        }
        if (w.after_frame()) {
          w.set_last_rowno_in_range_frame(rowno - 1);

          if (!first_row_in_range_frame_seen)
            // empty frame, optimize starting point for next row
            w.set_first_rowno_in_range_frame(rowno);
          w.restore_pos(reason);
          break;
        }  // else: row is within range, process

        if (!first_row_in_range_frame_seen) {
          /*
            Optimize starting point for next row: monotonic increase in frame
            bounds
          */
          first_row_in_range_frame_seen = true;
          w.set_first_rowno_in_range_frame(rowno);
        }
      }

      /*
        Compute framing WFs. For ROWS frame, "upper" is exactly the frame's
        last row; but for the case of RANGE
        we can't be sure that this is indeed the last row, but we must make a
        pessimistic assumption. If it is not the last, the final row
        calculation, if any, as for AVG, will be repeated for the next peer
        row(s).
        For optimized MIN/MAX [1], we do this to make sure we have a non-NULL
        last value (if one exists) for the initial frame.
      */
      const bool setstate =
          (rowno == upper || range_frame || have_last_value /* [1] */);
      if (setstate)
        w.set_is_last_row_in_frame(true);  // temporary state for next call

      // Accumulate frame's row into WF's value for current_row:
      if (copy_funcs(param, thd, CFT_WF_FRAMING)) return true;

      if (setstate) w.set_is_last_row_in_frame(false);  // undo temporary state
    }

    if (range_frame || rowno > upper)  // no more rows in partition
    {
      if (range_frame) {
        if (!first_row_in_range_frame_seen) {
          /*
            Empty frame: optimize starting point for next row: monotonic
            increase in frame bounds
          */
          w.set_first_rowno_in_range_frame(rowno);
        }
      }
      w.set_last_rowno_in_range_frame(rowno - 1);
      if (range_to_current_row) {
        w.set_last_rowno_in_peerset(w.last_rowno_in_range_frame());
        have_peers_current_row = true;
      }
    }  // else: we already set it before breaking out of loop
  }

  /*
    While the block above was for the default execution method, below we have
    alternative blocks for optimized methods: static framing WFs and
    inversion, when current_row isn't first; i.e. we can use the previous
    row's value of framing WFs as a base.
    In the row buffer of OUT, after the previous row was emitted, these values
    of framing WFs are still present, as no copy_funcs(CFT_WF_FRAMING) was run
    for our new row yet.
  */
  if (static_aggregate && current_row != 1) {
    /* Set up the correct non-wf fields for copying to the output row */
    if (bring_back_frame_row(thd, &w, param, current_row,
                             Window_retrieve_cached_row_reason::CURRENT))
      return true;

    /* E.g. ROW_NUMBER, RANK, DENSE_RANK */
    if (copy_funcs(param, thd, CFT_WF_NON_FRAMING)) return true;
  } else if (row_optimizable && w.aggregates_primed()) {
    /*
      Rows 2..N in partition: we still have state from previous current row's
      frame computation, now adjust by subtracting row 1 in frame (lower_limit)
      and adding new, if any, final frame row
    */
    const bool remove_previous_first_row =
        (lower_limit > 1 && lower_limit - 1 <= last_rowno_in_cache);
    const bool new_last_row =
        (upper_limit <= upper &&
         !unbounded_following /* all added when primed */);
    const int64 rn_in_frame = upper - lower_limit + 1;

    /* possibly subtract: early in partition there may not be any */
    if (remove_previous_first_row) {
      /*
        Check if the row leaving the frame is the last row in the peerset
        within a frame. If true, set is_last_row_in_peerset_within_frame
        to true.
        Used by JSON_OBJECTAGG to remove the key/value pair only
        when it is the last row having that key value.
      */
      if (needs_last_peer_in_frame) {
        int64 rowno = lower_limit - 1;
        bool is_last_row_in_peerset = true;
        if (rowno < upper) {
          if (bring_back_frame_row(
                  thd, &w, param, rowno,
                  Window_retrieve_cached_row_reason::LAST_IN_PEERSET))
            return true;
          // Establish current row as base-line for peer set.
          w.reset_order_by_peer_set();
          /*
            Check if the next row is a peer to this row. If not
            set current row as the last row in peerset within
            frame.
          */
          rowno++;
          if (rowno < upper) {
            if (bring_back_frame_row(
                    thd, &w, param, rowno,
                    Window_retrieve_cached_row_reason::LAST_IN_PEERSET))
              return true;
            // Compare only the first order by item.
            if (!w.in_new_order_by_peer_set(false))
              is_last_row_in_peerset = false;
          }
        }
        if (is_last_row_in_peerset)
          w.set_is_last_row_in_peerset_within_frame(true);
      }

      if (bring_back_frame_row(
              thd, &w, param, lower_limit - 1,
              Window_retrieve_cached_row_reason::FIRST_IN_FRAME))
        return true;

      w.set_inverse(true);
      if (!new_last_row) {
        w.set_rowno_in_frame(rn_in_frame);
        if (rn_in_frame > 0)
          w.set_is_last_row_in_frame(true);  // do final comp., e.g. div in AVG

        if (copy_funcs(param, thd, CFT_WF_FRAMING)) return true;

        w.set_is_last_row_in_frame(false);  // undo temporary states
      } else {
        if (copy_funcs(param, thd, CFT_WF_FRAMING)) return true;
      }

      w.set_is_last_row_in_peerset_within_frame(false);
      w.set_inverse(false);
    }

    if (have_first_value && (lower_limit <= last_rowno_in_cache)) {
      // We have seen first row of frame, FIRST_VALUE can be computed:
      if (bring_back_frame_row(
              thd, &w, param, lower_limit,
              Window_retrieve_cached_row_reason::FIRST_IN_FRAME))
        return true;

      w.set_rowno_in_frame(1);

      /*
        Framing WFs which accumulate (SUM, COUNT, AVG) shouldn't accumulate
        this row again as they have done so already. Evaluate only
        X_VALUE/MIN/MAX.
      */
      if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW)) return true;
    }

    if (have_last_value && !new_last_row) {
      // We have seen last row of frame, LAST_VALUE can be computed:
      if (bring_back_frame_row(
              thd, &w, param, upper,
              Window_retrieve_cached_row_reason::LAST_IN_FRAME))
        return true;

      w.set_rowno_in_frame(rn_in_frame);

      if (rn_in_frame > 0) w.set_is_last_row_in_frame(true);

      if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW)) return true;

      w.set_is_last_row_in_frame(false);
    }

    if (!have_nth_value.m_offsets.empty()) {
      int fno = 0;
      for (auto nth : have_nth_value.m_offsets) {
        if (lower_limit + nth.m_rowno - 1 <= upper) {
          if (bring_back_frame_row(
                  thd, &w, param, lower_limit + nth.m_rowno - 1,
                  Window_retrieve_cached_row_reason::MISC_POSITIONS, fno++))
            return true;

          w.set_rowno_in_frame(nth.m_rowno);

          if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW)) return true;
        }
      }
    }

    if (new_last_row)  // Add new last row to framing WF's value
    {
      if (bring_back_frame_row(
              thd, &w, param, upper,
              Window_retrieve_cached_row_reason::LAST_IN_FRAME))
        return true;

      w.set_rowno_in_frame(upper - lower_limit + 1)
          .set_is_last_row_in_frame(true);  // temporary states for next copy
      w.set_rowno_being_visited(upper);

      if (copy_funcs(param, thd, CFT_WF_FRAMING)) return true;

      w.set_is_last_row_in_frame(false);  // undo temporary states
    }
  } else if (range_optimizable && w.aggregates_primed()) {
    /*
      Peer sets 2..N in partition: we still have state from previous current
      row's frame computation, now adjust by possibly subtracting rows no
      longer in frame and possibly adding new rows now within range.
    */
    const int64 prev_last_rowno_in_frame = w.last_rowno_in_range_frame();
    const int64 prev_first_rowno_in_frame = w.first_rowno_in_range_frame();

    /*
      As an optimization, if:
      - RANGE frame specification ends at CURRENT ROW and
      - current_row belongs to frame of previous row,
      then both rows are peers, so have the same frame: nothing changes.
    */
    if (range_to_current_row && current_row >= prev_first_rowno_in_frame &&
        current_row <= prev_last_rowno_in_frame) {
      // Peer set should already have been determined:
      DBUG_ASSERT(w.last_rowno_in_peerset() >= current_row);
      have_peers_current_row = true;
    } else {
      /**
         Whether we know the start of the frame yet. The a priori setting is
         inherited from the previous current row.
      */
      bool found_first =
          (prev_first_rowno_in_frame <= prev_last_rowno_in_frame);
      int64 new_first_rowno_in_frame = prev_first_rowno_in_frame;  // a priori

      int64 inverted = 0;  // Number of rows inverted when moving frame
      int64 rowno;         // Partition relative, loop counter

      if (range_from_first_to_current_row) {
        /*
          No need to locate frame's start, it's first row of partition. No
          need to recompute FIRST_VALUE, it's same as for previous row.
          So we just have to accumulate new rows.
        */
        DBUG_ASSERT(current_row > prev_last_rowno_in_frame &&
                    lower_limit == 1 && prev_first_rowno_in_frame == 1 &&
                    found_first);
      } else {
        for (rowno = lower_limit;
             (rowno <= upper &&
              prev_first_rowno_in_frame <= prev_last_rowno_in_frame);
             rowno++) {
          /* Set up the non-wf fields for aggregating to the output row. */
          if (bring_back_frame_row(
                  thd, &w, param, rowno,
                  Window_retrieve_cached_row_reason::FIRST_IN_FRAME))
            return true;

          if (w.before_frame()) {
            w.set_inverse(true)
                .
                /*
                  The next setting sets the logical last row number in the frame
                  after inversion, so that final actions can do the right thing,
                  e.g.  AVG needs to know the updated cardinality. The
                  aggregates consults m_rowno_in_frame for that, so set it
                  accordingly.
                */
                set_rowno_in_frame(prev_last_rowno_in_frame -
                                   prev_first_rowno_in_frame + 1 - ++inverted)
                .set_is_last_row_in_frame(true);  // pessimistic assumption

            // Set the current row as the last row in the peerset.
            w.set_is_last_row_in_peerset_within_frame(true);

            /*
              It may be that rowno is not in previous frame; for example if
              column id contains 1, 3, 4 and 5 and frame is RANGE BETWEEN 2
              FOLLOWING AND 2 FOLLOWING: we process id=1, frame of id=1 is
              id=3; then we process id=3: id=3 is before frame (and was in
              previous frame), id=4 is before frame too (and was not in
              previous frame); so id=3 only should be inverted:
            */
            if (rowno >= prev_first_rowno_in_frame &&
                rowno <= prev_last_rowno_in_frame) {
              if (copy_funcs(param, thd, CFT_WF_FRAMING)) return true;
            }

            w.set_inverse(false).set_is_last_row_in_frame(false);
            w.set_is_last_row_in_peerset_within_frame(false);
            found_first = false;
          } else {
            if (w.after_frame()) {
              found_first = false;
            } else {
              w.set_first_rowno_in_range_frame(rowno);
              found_first = true;
              new_first_rowno_in_frame = rowno;
              w.set_rowno_in_frame(1);
            }

            break;
          }
        }

        if ((have_first_value || have_last_value) &&
            (rowno <= last_rowno_in_cache) && found_first) {
          /*
             We have FIRST_VALUE or LAST_VALUE and have a new first row; make it
             last also until we find something better.
          */
          w.set_is_last_row_in_frame(true);
          w.set_rowno_being_visited(rowno);

          if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW)) return true;
          w.set_is_last_row_in_frame(false);

          if (have_last_value && w.last_rowno_in_range_frame() > rowno) {
            /* Set up the non-wf fields for aggregating to the output row. */
            if (bring_back_frame_row(
                    thd, &w, param, w.last_rowno_in_range_frame(),
                    Window_retrieve_cached_row_reason::LAST_IN_FRAME))
              return true;

            w.set_rowno_in_frame(w.last_rowno_in_range_frame() -
                                 w.first_rowno_in_range_frame() + 1)
                .set_is_last_row_in_frame(true);
            w.set_rowno_being_visited(w.last_rowno_in_range_frame());
            if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW)) return true;
            w.set_is_last_row_in_frame(false);
          }
        }
      }

      /*
        We last evaluated last_rowno_in_range_frame for the previous current
        row. Now evaluate over any new rows within range of the current row.
      */
      const int64 first = w.last_rowno_in_range_frame() + 1;
      bool row_added = false;

      for (rowno = first; rowno <= upper; rowno++) {
        w.save_pos(Window_retrieve_cached_row_reason::LAST_IN_FRAME);
        if (bring_back_frame_row(
                thd, &w, param, rowno,
                Window_retrieve_cached_row_reason::LAST_IN_FRAME))
          return true;

        if (w.before_frame()) {
          if (!found_first) new_first_rowno_in_frame++;
          continue;
        } else if (w.after_frame()) {
          w.set_last_rowno_in_range_frame(rowno - 1);
          if (!found_first) w.set_first_rowno_in_range_frame(rowno);
          /*
            We read one row too far, so reinstate previous hint for last in
            frame. We will likely be reading the last row in frame
            again in for next current row, and then we will need the hint.
          */
          w.restore_pos(Window_retrieve_cached_row_reason::LAST_IN_FRAME);
          break;
        }  // else: row is within range, process

        const int64 rowno_in_frame = rowno - new_first_rowno_in_frame + 1;

        if (rowno_in_frame == 1 && !found_first) {
          found_first = true;
          w.set_first_rowno_in_range_frame(rowno);
          // Found the first row in this range frame. Make a note in the hint.
          w.copy_pos(Window_retrieve_cached_row_reason::LAST_IN_FRAME,
                     Window_retrieve_cached_row_reason::FIRST_IN_FRAME);
        }
        w.set_rowno_in_frame(rowno_in_frame)
            .set_is_last_row_in_frame(true);  // pessimistic assumption
        w.set_rowno_being_visited(rowno);

        if (copy_funcs(param, thd, CFT_WF_FRAMING)) return true;

        w.set_is_last_row_in_frame(false);  // undo temporary states
        row_added = true;
      }

      if (rowno > upper && row_added)
        w.set_last_rowno_in_range_frame(rowno - 1);

      if (range_to_current_row) {
        w.set_last_rowno_in_peerset(w.last_rowno_in_range_frame());
        have_peers_current_row = true;
      }

      if (found_first && !have_nth_value.m_offsets.empty()) {
        // frame is non-empty, so we might find NTH_VALUE
        DBUG_ASSERT(w.first_rowno_in_range_frame() <=
                    w.last_rowno_in_range_frame());
        int fno = 0;
        for (auto nth : have_nth_value.m_offsets) {
          const int64 row_to_get =
              w.first_rowno_in_range_frame() + nth.m_rowno - 1;
          if (row_to_get <= w.last_rowno_in_range_frame()) {
            if (bring_back_frame_row(
                    thd, &w, param, row_to_get,
                    Window_retrieve_cached_row_reason::MISC_POSITIONS, fno++))
              return true;

            w.set_rowno_in_frame(nth.m_rowno);

            if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW)) return true;
          }
        }
      }

      // We have empty frame, maintain invariant
      if (!found_first) {
        DBUG_ASSERT(!row_added);
        w.set_first_rowno_in_range_frame(w.last_rowno_in_range_frame() + 1);
      }
    }
  }

  /* We need the peer of the current row to evaluate the row. */
  if (needs_peerset && !have_peers_current_row) {
    int64 first = current_row;

    if (current_row != 1) first = w.last_rowno_in_peerset() + 1;

    if (current_row >= first) {
      int64 rowno;
      for (rowno = current_row; rowno <= last_rowno_in_cache; rowno++) {
        if (bring_back_frame_row(
                thd, &w, param, rowno,
                Window_retrieve_cached_row_reason::LAST_IN_PEERSET))
          return true;

        if (rowno == current_row) {
          /* establish current row as base-line for peer set */
          w.reset_order_by_peer_set();
          w.set_last_rowno_in_peerset(current_row);
        } else if (w.in_new_order_by_peer_set()) {
          w.set_last_rowno_in_peerset(rowno - 1);
          break;  // we have accumulated all rows in the peer set
        }
      }
      if (rowno > last_rowno_in_cache)
        w.set_last_rowno_in_peerset(last_rowno_in_cache);
    }
  }

  if (optimizable && optimizable_primed) w.set_aggregates_primed(true);

  if (bring_back_frame_row(thd, &w, param, current_row,
                           Window_retrieve_cached_row_reason::CURRENT))
    return true;

  /* NTILE and other non-framing wfs */
  if (w.needs_card()) {
    /* Set up the non-wf fields for aggregating to the output row. */
    if (process_wfs_needing_card(thd, param, have_nth_value, have_lead_lag,
                                 current_row, &w,
                                 Window_retrieve_cached_row_reason::CURRENT))
      return true;
  }

  if (w.is_last() && copy_funcs(param, thd, CFT_HAS_WF)) return true;
  *output_row_ready = true;
  w.set_last_row_output(current_row);
  DBUG_PRINT("info", ("sent row: %" PRId64, current_row));

  return false;
}

/**
  The last step in a series of windows do not need to write a tmp file
  if both a) and b) holds:

   a) no SELECT DISTINCT
   b) no final ORDER BY

  that have not been eliminated. If the condition is true, we send the data
  direct over the protocol to save the trip back and from the tmp file
*/
static inline enum_nested_loop_state write_or_send_row(
    JOIN *join, QEP_TAB *const qep_tab, TABLE *const table,
    Temp_table_param *const out_tbl) {
  if (out_tbl->m_window_short_circuit) {
    if (join->send_records >= join->unit->select_limit_cnt)
      return NESTED_LOOP_QUERY_LIMIT;
    enum_nested_loop_state nls =
        (*qep_tab->next_select)(join, qep_tab + 1, false);
    return nls;
  }
  int error;
  if ((error = table->file->ha_write_row(table->record[0]))) {
    if (table->file->is_ignorable_error(error)) return NESTED_LOOP_OK;

    /*
      - Convert to disk-based table,
      - and setup index access over hash field; that is usually done by
      QEP_tmp_table::prepare_tmp_table() but we may have a set of buffered
      rows to write before such function is executed.
    */
    if (create_ondisk_from_heap(join->thd, table, error, true, NULL) ||
        (table->hash_field && table->file->ha_index_init(0, false)))
      return NESTED_LOOP_ERROR;  // Not a table_is_full error
  }

  if (++qep_tab->send_records >= out_tbl->end_write_records &&
      join->do_send_rows) {
    if (!join->calc_found_rows) return NESTED_LOOP_QUERY_LIMIT;
    join->do_send_rows = false;
    join->unit->select_limit_cnt = HA_POS_ERROR;
    return NESTED_LOOP_OK;
  }
  join->thd->get_stmt_da()->inc_current_row_for_condition();

  return NESTED_LOOP_OK;
}

/* ARGSUSED */
static enum_nested_loop_state end_write(JOIN *join, QEP_TAB *const qep_tab,
                                        bool end_of_records) {
  DBUG_TRACE;

  TABLE *const table = qep_tab->table();

  if (join->thd->killed)  // Aborted by user
  {
    join->thd->send_kill_message();
    return NESTED_LOOP_KILLED; /* purecov: inspected */
  }
  if (!end_of_records) {
    Temp_table_param *const tmp_tbl = qep_tab->tmp_table_param;
    Switch_ref_item_slice slice_switch(join, qep_tab->ref_item_slice);
    DBUG_ASSERT(qep_tab - 1 != join->ref_slice_immediately_before_group_by);

    if (copy_fields_and_funcs(tmp_tbl, join->thd))
      return NESTED_LOOP_ERROR; /* purecov: inspected */

    if (having_is_true(qep_tab->having)) {
      int error;
      join->found_records++;

      if (!check_unique_constraint(table)) goto end;  // skip it

      if ((error = table->file->ha_write_row(table->record[0]))) {
        if (table->file->is_ignorable_error(error)) goto end;
        if (create_ondisk_from_heap(join->thd, table, error, true, NULL))
          return NESTED_LOOP_ERROR;  // Not a table_is_full error
      }
      if (++qep_tab->send_records >= tmp_tbl->end_write_records &&
          join->do_send_rows) {
        if (!join->calc_found_rows) return NESTED_LOOP_QUERY_LIMIT;
        join->do_send_rows = false;
        join->unit->select_limit_cnt = HA_POS_ERROR;
        return NESTED_LOOP_OK;
      }
      join->thd->get_stmt_da()->inc_current_row_for_condition();
    }
  }
end:
  return NESTED_LOOP_OK;
}

/* ARGSUSED */

/**
  Similar to end_write, but used in the windowing tmp table steps
*/
static enum_nested_loop_state end_write_wf(JOIN *join, QEP_TAB *const qep_tab,
                                           bool end_of_records) {
  DBUG_TRACE;
  THD *const thd = join->thd;

  if (thd->killed)  // Aborted by user
  {
    thd->send_kill_message();
    return NESTED_LOOP_KILLED; /* purecov: inspected */
  }

  Temp_table_param *const out_tbl = qep_tab->tmp_table_param;

  /**
    If we don't need to buffer rows to evaluate the window functions, execution
    is simple, see logic below. In that case we can just evaluate the
    window functions as we go here, similar to the non windowing flow,
    cf. copy_funcs below and in end_write.

    If we do need buffering, though, we buffer the row here.  Next, we enter a
    loop calling process_buffered_windowing_record and conditionally write (or
    send) the row onward.  That is, if process_buffered_windowing_record was
    able to complete evaluation of a row (cf. output_row_ready), including its
    window functions given how much has already been buffered, we do the write
    (or send), else we exit, and postpone evaluation and writing till we have
    enough rows in the buffer.

    When we have read a full partition (or reach EOF), we evaluate any remaining
    rows. Note that since we have to read one row past the current partition to
    detect that that previous row was indeed the last row in a partition, we
    need to re-establish the first row of the next partition when we are done
    processing the current one. This is because the record will be overwritten
    (many times) during evaluation of window functions in the current partition.

    Usually [1], for window execution we have two or three tmp tables per
    windowing step involved:

    - The input table, corresponding to qep_tab-1. Holds (possibly sorted)
      records ready for windowing, sorted on expressions concatenated from
      any PARTITION BY and ORDER BY clauses.

    - The output table, corresponding to qep_tab: where we write the evaluated
      records from this step. Note that we may optimize away this last write if
      we have no final ORDER BY or DISTINCT, see write_or_send_row.

    - If we have buffering, the frame buffer, held by
      Window::m_frame_buffer[_param]

    [1] This is not always the case. For the first window, if we have no
    PARTITION BY or ORDER BY in the window, and there is more than one table
    in the join, the logical input can consist of more than one table
    (qep_tab-1 .. qep_tab-n).

    The first thing we do in this function, is:
    we copy fields from IN to OUT (copy_fields), and evaluate non-WF functions
    (copy_funcs): those functions then read their arguments from IN and store
    their result into their result_field which is a field in OUT.
    We then evaluate any HAVING, on OUT table.
    The next steps depend on if we have a FB (Frame Buffer) or not.

    (a) If we have no FB, we immediately calculate the WFs over the OUT row,
    store their value in OUT row, and pass control to next plan operator
    (write_or_send_row) - we're done.

    (b) If we have a FB, let's take SUM(A+FLOOR(B)) OVER (ROWS 2 FOLLOWING) as
    example. Above, we have stored A and the result of FLOOR in OUT. Now we
    buffer (save) the row into the FB: for that, we copy field A from IN to
    FB, and FLOOR's result_field from OUT to FB; a single copy_fields() call
    handles both copy jobs.
    Then we look at the rows we have buffered and may realize that we have
    enough of the frame to calculate SUM for a certain row (not necessarily
    the one we just buffered; might be an earlier row, in our example it is
    the row which is 2 rows above the buffered row). If we do, to calculate
    WFs, we bring back the frame's rows; which is done by:
    first copying field A and FLOOR's result_field in directions
    opposite to above (using one copy_fields), then copying field A from IN to
    OUT, thus getting in OUT all that SUM needs (A and FLOOR), then giving
    that OUT row to SUM (SUM will then add the row's value to its total; that
    happens in copy_funcs). After we have done that on all rows of the frame,
    we have the values of SUM ready in OUT, we also restore the row which owns
    this SUM value, in the same way as we restored the frame's rows, and
    we pass control to next plan operator (write_or_send_row) - we're done for
    this row. However, when the next plan operator is done and we regain
    control, we loop to check if we can calculate one more row with the frame
    we have, and if so, we do. Until we can't calculate any more row in which
    case we're back to just buffering.

    @todo If we have buffering, for fields (not result_field of non-WF
    functions), we do:
    copy_fields IN->OUT, copy_fields IN->FB (buffering phase), and later
    (restoration phase): copy_fields FB->IN, copy_fields IN->OUT.
    The copy_fields IN->OUT before buffering, is useless as the OUT values
    will not be used (they'll be overwritten). We have two possible
    alternative improvements, any of which would avoid one copying:
    - remove this copy_fields (the buffering-phase IN->OUT)
    - keep it but change the rest to: OUT->FB, FB->OUT; that eliminates the
    restoration-phase IN->OUT; this design would be in line with what is done
    for result_field of non-WF functions.
  */
  Window *const win = out_tbl->m_window;
  const bool window_buffering = win->needs_buffering();

  if (end_of_records && !window_buffering) return NESTED_LOOP_OK;

  /*
    All evaluations of functions, done in process_buffered_windowing_record()
    and copy_funcs(), are using values of the out table, so we must use its
    slice:
  */
  Switch_ref_item_slice slice_switch(join, qep_tab->ref_item_slice);
  DBUG_ASSERT(qep_tab - 1 != join->ref_slice_immediately_before_group_by &&
              qep_tab != join->ref_slice_immediately_before_group_by);

  TABLE *const table = qep_tab->table();
  if (window_buffering) {
    bool new_partition = false;
    if (!end_of_records) {
      /*
        This saves the values of non-WF functions for the row. For example,
        1+t.a.
      */
      if (copy_fields_and_funcs(out_tbl, thd, CFT_HAS_NO_WF))
        return NESTED_LOOP_ERROR; /* purecov: inspected */

      if (!having_is_true(qep_tab->having))
        goto end;  // Didn't match having, skip it

      if (buffer_windowing_record(thd, out_tbl, &new_partition))
        return NESTED_LOOP_ERROR;

      join->found_records++;
    }

  repeat:
    while (true) {
      bool output_row_ready = false;
      if (process_buffered_windowing_record(
              thd, out_tbl, new_partition || end_of_records, &output_row_ready))
        return NESTED_LOOP_ERROR;

      if (!output_row_ready) break;

      if (!check_unique_constraint(table))  // In case of SELECT DISTINCT
        continue;                           // skip it

      enum_nested_loop_state result;
      if ((result = write_or_send_row(join, qep_tab, table, out_tbl)))
        return result;  // Not a table_is_full error

      if (thd->killed)  // Aborted by user
      {
        thd->send_kill_message();
        return NESTED_LOOP_KILLED;
      }
    }

    if (new_partition) {
      /*
        We didn't really buffer this row yet since, we found a partition
        change so we had to finalize the previous partition first.
        Bring back saved row for next partition.
      */
      if (bring_back_frame_row(
              thd, win, out_tbl, Window::FBC_FIRST_IN_NEXT_PARTITION,
              Window_retrieve_cached_row_reason::WONT_UPDATE_HINT))
        return NESTED_LOOP_ERROR;

      /*
        copy_funcs(CFT_NON_WF) is not necessary: a non-WF function was
        calculated and saved in OUT, then this OUT column was copied to
        special record, then restored to OUT column.
      */

      win->reset_partition_state();
      if (buffer_windowing_record(thd, out_tbl,
                                  nullptr /* first in new partition */))
        return NESTED_LOOP_ERROR;
      new_partition = false;
      goto repeat;
    }
    if (!end_of_records && win->needs_restore_input_row()) {
      /*
        Reestablish last row read from input table in case it is needed again
        before reading a new row. May be necessary if this is the first window
        following after a join, cf. the caching presumption in
        EQRefIterator. This logic can be removed if we move to copying
        between out tmp record and frame buffer record, instead of involving the
        in record. FIXME.
      */
      if (bring_back_frame_row(
              thd, win, nullptr /* no copy to OUT */,
              Window::FBC_LAST_BUFFERED_ROW,
              Window_retrieve_cached_row_reason::WONT_UPDATE_HINT))
        return NESTED_LOOP_ERROR;
    }
  } else {
    if (copy_fields_and_funcs(out_tbl, thd, CFT_HAS_NO_WF))
      return NESTED_LOOP_ERROR; /* purecov: inspected */

    if (!having_is_true(qep_tab->having))
      goto end;  // Didn't match having, skip it

    win->check_partition_boundary();

    if (copy_funcs(out_tbl, thd, CFT_WF))
      return NESTED_LOOP_ERROR; /* purecov: inspected */

    if (win->is_last() && copy_funcs(out_tbl, thd, CFT_HAS_WF))
      return NESTED_LOOP_ERROR; /* purecov: inspected */

    join->found_records++;

    if (!check_unique_constraint(table))  // In case of SELECT DISTINCT
      goto end;                           // skip it

    DBUG_PRINT("info", ("end_write: writing record at %p", table->record[0]));

    enum_nested_loop_state result;
    if ((result = write_or_send_row(join, qep_tab, table, out_tbl)))
      return result;  // Not a table_is_full error
  }
end:
  return NESTED_LOOP_OK;
}

/* ARGSUSED */
/** Group by searching after group record and updating it if possible. */

static enum_nested_loop_state end_update(JOIN *join, QEP_TAB *const qep_tab,
                                         bool end_of_records) {
  TABLE *const table = qep_tab->table();
  ORDER *group;
  int error;
  bool group_found = false;
  DBUG_TRACE;

  if (end_of_records) return NESTED_LOOP_OK;
  if (join->thd->killed)  // Aborted by user
  {
    join->thd->send_kill_message();
    return NESTED_LOOP_KILLED; /* purecov: inspected */
  }

  Temp_table_param *const tmp_tbl = qep_tab->tmp_table_param;
  join->found_records++;

  // See comment below.
  DBUG_ASSERT(tmp_tbl->grouped_expressions.size() == 0);

  if (copy_fields(tmp_tbl, join->thd))  // Groups are copied twice.
    return NESTED_LOOP_ERROR;           /* purecov: inspected */

  /* Make a key of group index */
  if (table->hash_field) {
    /*
      We need to call to copy_funcs here in order to get correct value for
      hash_field. However, this call isn't needed so early when hash_field
      isn't used as it would cause unnecessary additional evaluation of
      functions to be copied when 2nd and further records in group are
      found.
    */
    if (copy_funcs(tmp_tbl, join->thd))
      return NESTED_LOOP_ERROR; /* purecov: inspected */
    if (!check_unique_constraint(table)) group_found = true;
  } else {
    for (group = table->group; group; group = group->next) {
      Item *item = *group->item;
      item->save_org_in_field(group->field_in_tmp_table);
      /* Store in the used key if the field was 0 */
      if (item->maybe_null)
        group->buff[-1] = (char)group->field_in_tmp_table->is_null();
    }
    const uchar *key = tmp_tbl->group_buff;
    if (!table->file->ha_index_read_map(table->record[1], key, HA_WHOLE_KEY,
                                        HA_READ_KEY_EXACT))
      group_found = true;
  }
  if (group_found) {
    /* Update old record */
    restore_record(table, record[1]);
    update_tmptable_sum_func(join->sum_funcs, table);
    if ((error =
             table->file->ha_update_row(table->record[1], table->record[0]))) {
      // Old and new records are the same, ok to ignore
      if (error == HA_ERR_RECORD_IS_THE_SAME) return NESTED_LOOP_OK;
      table->file->print_error(error, MYF(0)); /* purecov: inspected */
      return NESTED_LOOP_ERROR;                /* purecov: inspected */
    }
    return NESTED_LOOP_OK;
  }

  /*
    Why, unlike in other end_* functions, do we advance the slice here and not
    before copy_fields()?
    Because of the evaluation of *group->item above: if we do it with this tmp
    table's slice, *group->item points to the field materializing the
    expression, which hasn't been calculated yet. We could force the missing
    calculation by doing copy_funcs() before evaluating *group->item; but
    then, for a group made of N rows, we might be doing N evaluations of
    another function when only one would suffice (like the '*' in
    "SELECT a, a*a ... GROUP BY a": only the first/last row of the group,
    needs to evaluate a*a).

    The assertion on tmp_tbl->grouped_expressions.size() is to make sure
    copy_fields() doesn't suffer from the late switching.
  */
  Switch_ref_item_slice slice_switch(join, qep_tab->ref_item_slice);
  DBUG_ASSERT(qep_tab - 1 != join->ref_slice_immediately_before_group_by &&
              qep_tab != join->ref_slice_immediately_before_group_by);

  /*
    Copy null bits from group key to table
    We can't copy all data as the key may have different format
    as the row data (for example as with VARCHAR keys)
  */
  if (!table->hash_field) {
    KEY_PART_INFO *key_part;
    for (group = table->group, key_part = table->key_info[0].key_part; group;
         group = group->next, key_part++) {
      // Field null indicator is located one byte ahead of field value.
      // @todo - check if this NULL byte is really necessary for grouping
      if (key_part->null_bit)
        memcpy(table->record[0] + key_part->offset - 1, group->buff - 1, 1);
    }
    /* See comment on copy_funcs above. */

    if (copy_funcs(tmp_tbl, join->thd))
      return NESTED_LOOP_ERROR; /* purecov: inspected */
  }
  init_tmptable_sum_functions(join->sum_funcs);
  if ((error = table->file->ha_write_row(table->record[0]))) {
    if (create_ondisk_from_heap(join->thd, table, error, false, NULL))
      return NESTED_LOOP_ERROR;  // Not a table_is_full error
    /* Change method to update rows */
    if ((error = table->file->ha_index_init(0, false))) {
      table->file->print_error(error, MYF(0));
      return NESTED_LOOP_ERROR;
    }
  }
  qep_tab->send_records++;
  return NESTED_LOOP_OK;
}

enum_nested_loop_state end_write_group(JOIN *join, QEP_TAB *const qep_tab,
                                       bool end_of_records) {
  TABLE *table = qep_tab->table();
  int idx = -1;
  DBUG_TRACE;

  if (join->thd->killed) {  // Aborted by user
    join->thd->send_kill_message();
    return NESTED_LOOP_KILLED; /* purecov: inspected */
  }
  /*
    (1) Haven't seen a first row yet
    (2) Have seen all rows
    (3) GROUP expression are different from previous row's
  */
  if (!join->seen_first_record ||                                     // (1)
      end_of_records ||                                               // (2)
      (idx = update_item_cache_if_changed(join->group_fields)) >= 0)  // (3)
  {
    Temp_table_param *const tmp_tbl = qep_tab->tmp_table_param;
    if (join->seen_first_record || (end_of_records && !join->grouped)) {
      if (idx < (int)join->send_group_parts) {
        /*
          As GROUP expressions have changed, we now send forward the group
          of the previous row.
        */
        Switch_ref_item_slice slice_switch(join, qep_tab->ref_item_slice);
        DBUG_ASSERT(qep_tab - 1 !=
                        join->ref_slice_immediately_before_group_by &&
                    qep_tab != join->ref_slice_immediately_before_group_by);
        {
          table_map save_nullinfo = 0;
          if (!join->seen_first_record) {
            // Calculate aggregate functions for no rows
            for (Item &item : *join->get_current_fields()) {
              item.no_rows_in_result();
            }

            /*
              Mark tables as containing only NULL values for ha_write_row().
              Calculate a set of tables for which NULL values need to
              be restored after sending data.
            */
            if (join->clear_fields(&save_nullinfo))
              return NESTED_LOOP_ERROR; /* purecov: inspected */
          }
          copy_sum_funcs(join->sum_funcs,
                         join->sum_funcs_end[join->send_group_parts]);
          if (having_is_true(qep_tab->having)) {
            int error = table->file->ha_write_row(table->record[0]);
            if (error &&
                create_ondisk_from_heap(join->thd, table, error, false, NULL))
              return NESTED_LOOP_ERROR;
          }
          if (join->rollup.state != ROLLUP::STATE_NONE) {
            if (join->rollup_write_data((uint)(idx + 1), qep_tab))
              return NESTED_LOOP_ERROR;
          }
          // Restore NULL values if needed.
          if (save_nullinfo) join->restore_fields(save_nullinfo);
        }
        if (end_of_records) return NESTED_LOOP_OK;
      }
    } else {
      if (end_of_records) return NESTED_LOOP_OK;
      join->seen_first_record = true;

      // Initialize the cache of GROUP expressions with this 1st row's values
      (void)(update_item_cache_if_changed(join->group_fields));
    }
    if (idx < (int)join->send_group_parts) {
      /*
        As GROUP expressions have changed, initialize the new group:
        (1) copy non-aggregated expressions (they're constant over the group)
        (2) and reset group aggregate functions.

        About (1): some expressions to copy are not Item_fields and they are
        copied by copy_fields() which evaluates them (see
        param->grouped_expressions, set up in setup_copy_fields()). Thus,
        copy_fields() can evaluate functions. One of them, F2, may reference
        another one F1, example: SELECT expr AS F1 ... GROUP BY ... HAVING
        F2(F1)<=2 . Assume F1 and F2 are not aggregate functions. Then they are
        calculated by copy_fields() when starting a new group, i.e. here. As F2
        uses an alias to F1, F1 is calculated first; F2 must use that value (not
        evaluate expr again, as expr may not be deterministic), so F2 uses a
        reference (Item_ref) to the already-computed value of F1; that value is
        in Item_copy part of REF_SLICE_ORDERED_GROUP_BY. So, we switch to that
        slice.
      */
      Switch_ref_item_slice slice_switch(join, qep_tab->ref_item_slice);
      if (copy_fields_and_funcs(tmp_tbl, join->thd))  // (1)
        return NESTED_LOOP_ERROR;
      if (init_sum_functions(join->sum_funcs,
                             join->sum_funcs_end[idx + 1]))  //(2)
        return NESTED_LOOP_ERROR;
      return NESTED_LOOP_OK;
    }
  }
  if (update_sum_func(join->sum_funcs)) return NESTED_LOOP_ERROR;
  return NESTED_LOOP_OK;
}

/*****************************************************************************
  Remove duplicates from tmp table
  This should be recoded to add a unique index to the table and remove
  duplicates
  Table is a locked single thread table
  fields is the number of fields to check (from the end)
*****************************************************************************/

static bool compare_record(TABLE *table, Field **ptr) {
  for (; *ptr; ptr++) {
    if ((*ptr)->cmp_offset(table->s->rec_buff_length)) return true;
  }
  return false;
}

static bool copy_blobs(Field **ptr) {
  for (; *ptr; ptr++) {
    if ((*ptr)->flags & BLOB_FLAG)
      if (((Field_blob *)(*ptr))->copy()) return true;  // Error
  }
  return false;
}

static void free_blobs(Field **ptr) {
  for (; *ptr; ptr++) {
    if ((*ptr)->flags & BLOB_FLAG) ((Field_blob *)(*ptr))->mem_free();
  }
}

/**
  For a set of fields, compute how many bytes their respective sort keys need.

  @param first_field         Array of fields, terminated by nullptr.
  @param[out] field_lengths  The computed sort buffer length for each field.
    Must be allocated by the caller.

  @retval The total number of bytes needed, sans extra alignment.

  @note
    This assumes that Field::sort_length() is constant for each field.
*/

static size_t compute_field_lengths(Field **first_field,
                                    size_t *field_lengths) {
  Field **field;
  size_t *field_length;
  size_t total_length = 0;
  for (field = first_field, field_length = field_lengths; *field;
       ++field, ++field_length) {
    size_t length = (*field)->sort_length();
    const CHARSET_INFO *cs = (*field)->sort_charset();
    length = cs->coll->strnxfrmlen(cs, length);

    if ((*field)->sort_key_is_varlen()) {
      // Make room for the length.
      length += sizeof(uint32);
    }

    *field_length = length;
    total_length += length;
  }
  return total_length;
}

bool QEP_TAB::remove_duplicates() {
  bool error;
  DBUG_ASSERT(this - 1 != join()->ref_slice_immediately_before_group_by &&
              this != join()->ref_slice_immediately_before_group_by);
  THD *thd = join()->thd;
  DBUG_TRACE;

  DBUG_ASSERT(join()->tmp_tables > 0 && table()->s->tmp_table != NO_TMP_TABLE);

  TABLE *const tbl = table();

  tbl->reginfo.lock_type = TL_WRITE;

  Opt_trace_object trace_wrapper(&thd->opt_trace);
  trace_wrapper.add("eliminating_duplicates_from_table_in_plan_at_position",
                    idx());

  // How many saved fields there is in list
  uint field_count = tbl->s->fields - tmp_table_param->hidden_field_count;
  DBUG_ASSERT((int)field_count >= 0);

  if (!field_count && !join()->calc_found_rows &&
      !having) {  // only const items with no OPTION_FOUND_ROWS
    join()->unit->select_limit_cnt = 1;  // Only send first row
    needs_duplicate_removal = false;
    return false;
  }
  Field **first_field = tbl->field + tbl->s->fields - field_count;

  size_t *field_lengths =
      (size_t *)my_malloc(key_memory_hash_index_key_buffer,
                          field_count * sizeof(*field_lengths), MYF(MY_WME));
  if (field_lengths == nullptr) return true;

  size_t key_length = compute_field_lengths(first_field, field_lengths);

  free_io_cache(tbl);  // Safety
  tbl->file->info(HA_STATUS_VARIABLE);
  constexpr int HASH_OVERHEAD = 16;  // Very approximate.
  if (!tbl->s->blob_fields &&
      (tbl->s->db_type() == temptable_hton || tbl->s->db_type() == heap_hton ||
       ((ALIGN_SIZE(key_length) + HASH_OVERHEAD) * tbl->file->stats.records <
        join()->thd->variables.sortbuff_size)))
    error = remove_dup_with_hash_index(thd, tbl, first_field, field_lengths,
                                       key_length, having);
  else {
    ulong offset =
        field_count
            ? tbl->field[tbl->s->fields - field_count]->offset(tbl->record[0])
            : 0;
    error = remove_dup_with_compare(thd, tbl, first_field, offset, having);
  }

  my_free(field_lengths);

  free_blobs(first_field);
  needs_duplicate_removal = false;
  return error;
}

static bool remove_dup_with_compare(THD *thd, TABLE *table, Field **first_field,
                                    ulong offset, Item *having) {
  handler *file = table->file;
  char *org_record, *new_record;
  uchar *record;
  int error;
  ulong reclength = table->s->reclength - offset;
  DBUG_TRACE;

  org_record = (char *)(record = table->record[0]) + offset;
  new_record = (char *)table->record[1] + offset;

  if ((error = file->ha_rnd_init(true))) goto err;
  error = file->ha_rnd_next(record);
  for (;;) {
    if (thd->killed) {
      thd->send_kill_message();
      error = 0;
      goto err;
    }
    if (error) {
      if (error == HA_ERR_RECORD_DELETED) {
        error = file->ha_rnd_next(record);
        continue;
      }
      if (error == HA_ERR_END_OF_FILE) break;
      goto err;
    }
    if (!having_is_true(having)) {
      if ((error = file->ha_delete_row(record))) goto err;
      error = file->ha_rnd_next(record);
      continue;
    }
    if (copy_blobs(first_field)) {
      error = 0;
      goto err;
    }
    memcpy(new_record, org_record, reclength);

    /* Read through rest of file and mark duplicated rows deleted */
    bool found = false;
    for (;;) {
      if ((error = file->ha_rnd_next(record))) {
        if (error == HA_ERR_RECORD_DELETED) continue;
        if (error == HA_ERR_END_OF_FILE) break;
        goto err;
      }
      if (compare_record(table, first_field) == 0) {
        if ((error = file->ha_delete_row(record))) goto err;
      } else if (!found) {
        found = true;
        file->position(record);  // Remember position
      }
    }
    if (!found) break;  // End of file
    /* Restart search on next row */
    error = file->ha_rnd_pos(record, file->ref);
  }

  return false;
err:
  if (file->inited) (void)file->ha_rnd_end();
  if (error) file->print_error(error, MYF(0));
  return true;
}

/**
  Generate a hash index for each row to quickly find duplicate rows.

  @note
    Note that this will not work on tables with blobs!
*/

static bool remove_dup_with_hash_index(THD *thd, TABLE *table,
                                       Field **first_field,
                                       const size_t *field_lengths,
                                       size_t key_length, Item *having) {
  uchar *record = table->record[0];
  int error;
  handler *file = table->file;
  DBUG_TRACE;

  MEM_ROOT mem_root(key_memory_hash_index_key_buffer, 32768);
  mem_root_unordered_set<std::string> hash(&mem_root);
  hash.reserve(file->stats.records);

  std::unique_ptr<uchar[]> key_buffer(new uchar[key_length]);
  if ((error = file->ha_rnd_init(true))) goto err;
  for (;;) {
    uchar *key_pos = key_buffer.get();
    if (thd->killed) {
      thd->send_kill_message();
      error = 0;
      goto err;
    }
    if ((error = file->ha_rnd_next(record))) {
      if (error == HA_ERR_RECORD_DELETED) continue;
      if (error == HA_ERR_END_OF_FILE) break;
      goto err;
    }
    if (!having_is_true(having)) {
      if ((error = file->ha_delete_row(record))) goto err;
      continue;
    }

    /* copy fields to key buffer */
    const size_t *field_length = field_lengths;
    for (Field **ptr = first_field; *ptr; ++ptr, ++field_length) {
      if ((*ptr)->sort_key_is_varlen()) {
        size_t len = (*ptr)->make_sort_key(key_pos + sizeof(uint32),
                                           *field_length - sizeof(uint32));
        int4store(key_pos, len);
        key_pos += sizeof(uint32) + len;
      } else {
        size_t len MY_ATTRIBUTE((unused)) =
            (*ptr)->make_sort_key(key_pos, *field_length);
        DBUG_ASSERT(len == *field_length);
        key_pos += *field_length;
      }
    }

    if (!hash.insert(std::string(key_buffer.get(), key_pos)).second) {
      // Duplicated record found; remove the row.
      if ((error = file->ha_delete_row(record))) goto err;
    }
  }

  (void)file->ha_rnd_end();
  return false;

err:
  if (file->inited) (void)file->ha_rnd_end();
  if (error) file->print_error(error, MYF(0));
  return true;
}

bool construct_lookup_ref(THD *thd, TABLE *table, TABLE_REF *ref) {
  enum enum_check_fields save_check_for_truncated_fields =
      thd->check_for_truncated_fields;
  thd->check_for_truncated_fields = CHECK_FIELD_IGNORE;
  my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
  bool result = false;

  for (uint part_no = 0; part_no < ref->key_parts; part_no++) {
    store_key *s_key = ref->key_copy[part_no];
    if (!s_key) continue;

    /*
      copy() can return STORE_KEY_OK even when there are errors so need to
      check thd->is_error().
      @todo This is due to missing handling of error return value from
      Field::store().
    */
    if (s_key->copy() != store_key::STORE_KEY_OK || thd->is_error()) {
      result = true;
      break;
    }
  }
  thd->check_for_truncated_fields = save_check_for_truncated_fields;
  dbug_tmp_restore_column_map(table->write_set, old_map);
  return result;
}

/**
  allocate group fields or take prepared (cached).

  @param main_join   join of current select
  @param curr_join   current join (join of current select or temporary copy
                     of it)

  @retval
    0   ok
  @retval
    1   failed
*/

bool make_group_fields(JOIN *main_join, JOIN *curr_join) {
  DBUG_TRACE;
  if (main_join->group_fields_cache.elements) {
    curr_join->group_fields = main_join->group_fields_cache;
    curr_join->streaming_aggregation = true;
  } else {
    if (alloc_group_fields(curr_join, curr_join->group_list)) return true;
    main_join->group_fields_cache = curr_join->group_fields;
  }
  return false;
}

/**
  Get a list of buffers for saveing last group.

  Groups are saved in reverse order for easyer check loop.
*/

static bool alloc_group_fields(JOIN *join, ORDER *group) {
  if (group) {
    for (; group; group = group->next) {
      Cached_item *tmp = new_Cached_item(join->thd, *group->item);
      if (!tmp || join->group_fields.push_front(tmp)) return true;
    }
  }
  join->streaming_aggregation = true; /* Mark for do_select */
  return false;
}

/*
  Test if a single-row cache of items changed, and update the cache.

  @details Test if a list of items that typically represents a result
  row has changed. If the value of some item changed, update the cached
  value for this item.

  @param list list of <item, cached_value> pairs stored as Cached_item.

  @return -1 if no item changed
  @return index of the first item that changed
*/

int update_item_cache_if_changed(List<Cached_item> &list) {
  DBUG_TRACE;
  List_iterator<Cached_item> li(list);
  int idx = -1, i;
  Cached_item *buff;

  for (i = (int)list.elements - 1; (buff = li++); i--) {
    if (buff->cmp()) idx = i;
  }
  DBUG_PRINT("info", ("idx: %d", idx));
  return idx;
}

/**
  Sets up caches for holding the values of non-aggregated expressions. The
  values are saved at the start of every new group.

  This code path is used in the cases when aggregation can be performed
  without a temporary table. Why it still uses a Temp_table_param is a
  mystery.

  Only FIELD_ITEM:s and FUNC_ITEM:s needs to be saved between groups.
  Change old item_field to use a new field with points at saved fieldvalue
  This function is only called before use of send_result_set_metadata.

  @param all_fields                  all fields list; should really be const,
                                       but Item does not always respect
                                       constness
  @param num_select_elements         number of elements in select item list
  @param thd                         THD pointer
  @param [in,out] param              temporary table parameters
  @param [out] ref_item_array        array of pointers to top elements of field
                                       list
  @param [out] res_selected_fields   new list of items of select item list
  @param [out] res_all_fields        new list of all items

  @todo
    In most cases this result will be sent to the user.
    This should be changed to use copy_int or copy_real depending
    on how the value is to be used: In some cases this may be an
    argument in a group function, like: IF(ISNULL(col),0,COUNT(*))

  @returns false if success, true if error
*/

bool setup_copy_fields(List<Item> &all_fields, size_t num_select_elements,
                       THD *thd, Temp_table_param *param,
                       Ref_item_array ref_item_array,
                       List<Item> *res_selected_fields,
                       List<Item> *res_all_fields) {
  DBUG_TRACE;

  res_selected_fields->empty();
  res_all_fields->empty();
  size_t border = all_fields.size() - num_select_elements;
  Mem_root_vector<Item_copy *> extra_funcs(
      Mem_root_allocator<Item_copy *>(thd->mem_root));

  param->grouped_expressions.clear();
  DBUG_ASSERT(param->copy_fields.empty());

  try {
    param->grouped_expressions.reserve(all_fields.size());
    param->copy_fields.reserve(param->field_count);
    extra_funcs.reserve(border);
  } catch (std::bad_alloc &) {
    return true;
  }

  List_iterator_fast<Item> li(all_fields);
  Item *pos;
  for (size_t i = 0; (pos = li++); i++) {
    Item *real_pos = pos->real_item();
    if (real_pos->type() == Item::FIELD_ITEM) {
      Item_field *item = new Item_field(thd, ((Item_field *)real_pos));
      if (item == nullptr) return true;
      if (pos->type() == Item::REF_ITEM) {
        /* preserve the names of the ref when dereferncing */
        Item_ref *ref = (Item_ref *)pos;
        item->db_name = ref->db_name;
        item->table_name = ref->table_name;
        item->item_name = ref->item_name;
      }
      pos = item;
      if (item->field->flags & BLOB_FLAG) {
        Item_copy *item_copy = Item_copy::create(pos);
        if (item_copy == nullptr) return true;
        pos = item_copy;
        /*
          Item_copy_string::copy for function can call
          Item_copy_string::val_int for blob via Item_ref.
          But if Item_copy_string::copy for blob isn't called before,
          it's value will be wrong
          so let's insert Item_copy_string for blobs in the beginning of
          copy_funcs
          (to see full test case look at having.test, BUG #4358)
        */
        param->grouped_expressions.push_back(item_copy);
      } else {
        DBUG_ASSERT(param->field_count > param->copy_fields.size());
        param->copy_fields.emplace_back(thd->mem_root, item);

        /*
          Even though the field doesn't point into field->table->record[0], we
          must still link it to 'table' through field->table because that's an
          existing way to access some type info (e.g. nullability from
          table->nullable).
        */
      }
    } else if (((real_pos->type() == Item::FUNC_ITEM ||
                 real_pos->type() == Item::SUBSELECT_ITEM ||
                 real_pos->type() == Item::CACHE_ITEM ||
                 real_pos->type() == Item::COND_ITEM) &&
                !real_pos->has_aggregation() &&
                !real_pos->has_rollup_expr())) {  // Save for send fields
      pos = real_pos;
      /* TODO:
         In most cases this result will be sent to the user.
         This should be changed to use copy_int or copy_real depending
         on how the value is to be used: In some cases this may be an
         argument in a group function, like: IF(ISNULL(col),0,COUNT(*))
      */
      Item_copy *item_copy = Item_copy::create(pos);
      if (item_copy == nullptr) return true;
      pos = item_copy;
      if (i < border)  // HAVING, ORDER and GROUP BY
        extra_funcs.push_back(item_copy);
      else
        param->grouped_expressions.push_back(item_copy);
    }
    res_all_fields->push_back(pos);
    ref_item_array[((i < border) ? all_fields.size() - i - 1 : i - border)] =
        pos;
  }

  List_iterator_fast<Item> itr(*res_all_fields);
  for (size_t i = 0; i < border; i++) itr++;
  itr.sublist(*res_selected_fields, num_select_elements);
  /*
    Put elements from HAVING, ORDER BY and GROUP BY last to ensure that any
    reference used in these will resolve to a item that is already calculated
  */
  param->grouped_expressions.insert(param->grouped_expressions.end(),
                                    extra_funcs.begin(), extra_funcs.end());
  return false;
}

/**
  Make a copy of all simple SELECT'ed fields.

  This is done at the start of a new group so that we can retrieve
  these later when the group changes. It is also used in materialization,
  to copy the values into the temporary table's fields.

  @param param     Represents the current temporary file being produced
  @param thd       The current thread
  @param reverse_copy   If true, copies fields *back* from the frame buffer
                        tmp table to the input table's buffer,
                        cf. #bring_back_frame_row.

  @returns false if OK, true on error.
*/

bool copy_fields(Temp_table_param *param, const THD *thd, bool reverse_copy) {
  DBUG_TRACE;

  DBUG_PRINT("enter", ("for param %p", param));
  for (Copy_field &ptr : param->copy_fields) ptr.invoke_do_copy(reverse_copy);

  if (thd->is_error()) return true;

  for (Item_copy *item : param->grouped_expressions) {
    if (item->copy(thd)) return true;
  }
  return false;
}

bool copy_fields_and_funcs(Temp_table_param *param, const THD *thd,
                           Copy_func_type type) {
  if (copy_fields(param, thd)) return true;
  if (param->items_to_copy != nullptr) {
    if (copy_funcs(param, thd, type)) return true;
  }
  return false;
}

/**
  Change all funcs and sum_funcs to fields in tmp table, and create
  new list of all items.

  @param all_fields                  all fields list; should really be const,
                                       but Item does not always respect
                                       constness
  @param num_select_elements         number of elements in select item list
  @param thd                         THD pointer
  @param [out] ref_item_array        array of pointers to top elements of filed
  list
  @param [out] res_selected_fields   new list of items of select item list
  @param [out] res_all_fields        new list of all items

  @returns false if success, true if error
*/

bool change_to_use_tmp_fields(List<Item> &all_fields,
                              size_t num_select_elements, THD *thd,
                              Ref_item_array ref_item_array,
                              List<Item> *res_selected_fields,
                              List<Item> *res_all_fields) {
  DBUG_TRACE;

  res_selected_fields->empty();
  res_all_fields->empty();

  List_iterator_fast<Item> li(all_fields);
  size_t border = all_fields.size() - num_select_elements;
  Item *item;
  for (size_t i = 0; (item = li++); i++) {
    Item *item_field;
    Field *field;
    if (item->has_aggregation() && item->type() != Item::SUM_FUNC_ITEM)
      item_field = item;
    else if (item->type() == Item::FIELD_ITEM)
      item_field = item->get_tmp_table_item(thd);
    else if (item->type() == Item::FUNC_ITEM &&
             ((Item_func *)item)->functype() == Item_func::SUSERVAR_FUNC) {
      field = item->get_tmp_table_field();
      if (field != NULL) {
        /*
          Replace "@:=<expression>" with "@:=<tmp table column>". Otherwise, we
          would re-evaluate <expression>, and if expression were a subquery,
          this would access already-unlocked tables.
        */
        Item_func_set_user_var *suv =
            new Item_func_set_user_var(thd, (Item_func_set_user_var *)item);
        Item_field *new_field = new Item_field(field);
        if (!suv || !new_field) return true;  // Fatal error
        List<Item> list;
        list.push_back(new_field);
        suv->set_arguments(list, true);
        item_field = suv;
      } else
        item_field = item;
    } else if ((field = item->get_tmp_table_field())) {
      if (item->type() == Item::SUM_FUNC_ITEM && field->table->group) {
        item_field = down_cast<Item_sum *>(item)->result_item(field);
        DBUG_ASSERT(item_field != nullptr);
      } else {
        item_field = new (thd->mem_root) Item_field(field);
        if (item_field == nullptr) return true;
      }
      if (item->real_item()->type() != Item::FIELD_ITEM) field->orig_table = 0;
      item_field->item_name = item->item_name;
      if (item->type() == Item::REF_ITEM) {
        Item_field *ifield = (Item_field *)item_field;
        Item_ref *iref = (Item_ref *)item;
        ifield->table_name = iref->table_name;
        ifield->db_name = iref->db_name;
      }
#ifndef DBUG_OFF
      if (!item_field->item_name.is_set()) {
        char buff[256];
        String str(buff, sizeof(buff), &my_charset_bin);
        str.length(0);
        item->print(thd, &str, QT_ORDINARY);
        item_field->item_name.copy(str.ptr(), str.length());
      }
#endif
    } else
      item_field = item;

    res_all_fields->push_back(item_field);
    /*
      Cf. comment explaining the reordering going on below in
      similar section of change_refs_to_tmp_fields
    */
    ref_item_array[((i < border) ? all_fields.size() - i - 1 : i - border)] =
        item_field;
    item_field->set_orig_field(item->get_orig_field());
  }

  List_iterator_fast<Item> itr(*res_all_fields);
  for (size_t i = 0; i < border; i++) itr++;
  itr.sublist(*res_selected_fields, num_select_elements);
  return false;
}

/**
  Change all sum_func refs to fields to point at fields in tmp table.
  Change all funcs to be fields in tmp table.

  @param all_fields                  all fields list; should really be const,
                                       but Item does not always respect
                                       constness
  @param num_select_elements         number of elements in select item list
  @param thd                         THD pointer
  @param [out] ref_item_array        array of pointers to top elements of filed
  list
  @param [out] res_selected_fields   new list of items of select item list
  @param [out] res_all_fields        new list of all items

  @returns false if success, true if error
*/

bool change_refs_to_tmp_fields(List<Item> &all_fields,
                               size_t num_select_elements, THD *thd,
                               Ref_item_array ref_item_array,
                               List<Item> *res_selected_fields,
                               List<Item> *res_all_fields) {
  DBUG_TRACE;
  res_selected_fields->empty();
  res_all_fields->empty();

  List_iterator_fast<Item> li(all_fields);
  size_t border = all_fields.size() - num_select_elements;
  Item *item;
  for (size_t i = 0; (item = li++); i++) {
    /*
      Below we create "new_item" using get_tmp_table_item
      based on all_fields[i] and assign them to res_all_fields[i].

      The new items are also put into ref_item_array, but in another order,
      cf the diagram below.

      Example of the population of ref_item_array, ref_all_fields and
      res_selected_fields based on all_fields:

      res_all_fields             res_selected_fields
         |                          |
         V                          V
       +--+   +--+   +--+   +--+   +--+   +--+          +--+
       |0 |-->|  |-->|  |-->|3 |-->|4 |-->|  |--> .. -->|9 |
       +--+   +--+   +--+   +--+   +--+   +--+          +--+
                              |     |
        ,------------->--------\----/
        |                       |
      +-^-+---+---+---+---+---#-^-+---+---+---+
      |   |   |   |   |   |   #   |   |   |   | ref_item_array
      +---+---+---+---+---+---#---+---+---+---+
        4   5   6   7   8   9   3   2   1   0   position in all_fields list
                                                similar to ref_all_fields pos
      all_fields.elements == 10      border == 4
      (visible) elements == 6

      i==0   ->   afe-0-1 == 9     i==4 -> 4-4 == 0
      i==1   ->   afe-1-1 == 8      :
      i==2   ->   afe-2-1 == 7
      i==3   ->   afe-3-1 == 6     i==9 -> 9-4 == 5
    */
    Item *new_item = item->get_tmp_table_item(thd);
    res_all_fields->push_back(new_item);
    ref_item_array[((i < border) ? all_fields.size() - i - 1 : i - border)] =
        new_item;
  }

  List_iterator_fast<Item> itr(*res_all_fields);
  for (size_t i = 0; i < border; i++) itr++;
  itr.sublist(*res_selected_fields, num_select_elements);

  return thd->is_fatal_error();
}

/**
  Clear all result fields. Non-aggregated fields are set to NULL,
  aggregated fields are set to their special "clear" value.

  Result fields can be fields from input tables, field values generated
  by sum functions and literal values.

  This is used when no rows are found during grouping: for FROM clause, a
  result row of all NULL values will be output; then SELECT list expressions
  get evaluated. E.g. SUM() will be NULL (the special "clear" value) and thus
  SUM() IS NULL will be true.

  @note Setting field values for input tables is a destructive operation,
        since it overwrite the NULL value flags with 1 bits. Rows from
        const tables are never re-read, hence their NULL value flags must
        be saved by this function and later restored by JOIN::restore_fields().
        This is generally not necessary for non-const tables, since field
        values are overwritten when new rows are read.

  @param[out] save_nullinfo Map of tables whose fields were set to NULL,
                            and for which NULL values must be restored.
                            Should be set to all zeroes on entry to function.

  @returns false if success, true if error
*/

bool JOIN::clear_fields(table_map *save_nullinfo) {
  // Set all column values from all input tables to NULL.
  for (uint tableno = 0; tableno < primary_tables; tableno++) {
    QEP_TAB *const tab = qep_tab + tableno;
    TABLE *const table = tab->table_ref->table;
    if (!table->has_null_row()) {
      *save_nullinfo |= tab->table_ref->map();
      if (table->const_table) table->save_null_flags();
      table->set_null_row();  // All fields are NULL
    }
  }
  if (copy_fields(&tmp_table_param, thd)) return true;

  if (sum_funcs) {
    Item_sum *func, **func_ptr = sum_funcs;
    while ((func = *(func_ptr++))) func->clear();
  }
  return false;
}

/**
  Restore all result fields for all tables specified in save_nullinfo.

  @param save_nullinfo Set of tables for which restore is necessary.

  @note Const tables must have their NULL value flags restored,
        @see JOIN::clear_fields().
*/
void JOIN::restore_fields(table_map save_nullinfo) {
  DBUG_ASSERT(save_nullinfo);

  for (uint tableno = 0; tableno < primary_tables; tableno++) {
    QEP_TAB *const tab = qep_tab + tableno;
    if (save_nullinfo & tab->table_ref->map()) {
      TABLE *const table = tab->table_ref->table;
      if (table->const_table) table->restore_null_flags();
      table->reset_null_row();
    }
  }
}

/****************************************************************************
  QEP_tmp_table implementation
****************************************************************************/

/**
  @brief Instantiate tmp table and start index scan if necessary
  @todo Tmp table always would be created, even for empty result. Extend
        executor to avoid tmp table creation when no rows were written
        into tmp table.
  @return
    true  error
    false ok
*/

bool QEP_tmp_table::prepare_tmp_table() {
  DBUG_TRACE;
  Temp_table_param *const tmp_tbl = qep_tab->tmp_table_param;

  /*
    Window final tmp file optimization: we skip actually writing to the
    tmp file, so no need to physically create it.
  */
  if (tmp_tbl->m_window_short_circuit) return false;

  TABLE *table = qep_tab->table();
  JOIN *join = qep_tab->join();
  int rc = 0;

  if (!table->is_created()) {
    if (instantiate_tmp_table(join->thd, table)) return true;
    empty_record(table);
  }
  /* If it wasn't already, start index scan for grouping using table index. */
  if (!table->file->inited &&
      ((table->group && tmp_tbl->sum_func_count && table->s->keys) ||
       table->hash_field))
    rc = table->file->ha_index_init(0, false);
  else {
    /* Start index scan in scanning mode */
    rc = table->file->ha_rnd_init(true);
  }
  if (rc) {
    table->file->print_error(rc, MYF(0));
    return true;
  }

  return false;
}

/**
  @brief Prepare table if necessary and call write_func to save record

  @param end_of_records The end_of_record signal to pass to the writer

  @return return one of enum_nested_loop_state.
*/

enum_nested_loop_state QEP_tmp_table::put_record(bool end_of_records) {
  // Lasy tmp table creation/initialization
  if (!qep_tab->table()->file->inited && prepare_tmp_table())
    return NESTED_LOOP_ERROR;
  enum_nested_loop_state rc =
      (*write_func)(qep_tab->join(), qep_tab, end_of_records);
  return rc;
}

/**
  @brief Finish rnd/index scan after accumulating records, switch ref_array,
         and send accumulated records further.
  @return return one of enum_nested_loop_state.
*/

enum_nested_loop_state QEP_tmp_table::end_send() {
  enum_nested_loop_state rc = NESTED_LOOP_OK;
  TABLE *table = qep_tab->table();
  JOIN *join = qep_tab->join();

  // All records were stored, send them further
  int tmp, new_errno = 0;

  if ((rc = put_record(true)) < NESTED_LOOP_OK) return rc;

  if ((tmp = table->file->ha_index_or_rnd_end())) {
    DBUG_PRINT("error", ("ha_index_or_rnd_end() failed"));
    new_errno = tmp;
  }
  if (new_errno) {
    table->file->print_error(new_errno, MYF(0));
    return NESTED_LOOP_ERROR;
  }
  table->reginfo.lock_type = TL_UNLOCK;

  if (join->m_windows.elements > 0)
    join->thd->get_stmt_da()->reset_current_row_for_condition();

  Temp_table_param *const tmp_tbl = qep_tab->tmp_table_param;

  /**
    Window final tmp file optimization:
    rows have already been sent from end_write, so just return.
  */
  if (tmp_tbl->m_window_short_circuit) return NESTED_LOOP_OK;

  Switch_ref_item_slice slice_switch(join, qep_tab->ref_item_slice);

  bool in_first_read = true;
  while (rc == NESTED_LOOP_OK) {
    if (in_first_read) {
      in_first_read = false;

      if (qep_tab->needs_duplicate_removal && qep_tab->remove_duplicates()) {
        rc = NESTED_LOOP_ERROR;
        break;
      }

      // The same temporary table can be used multiple times (with different
      // data, e.g. for a dependent subquery). To avoid leaks, we need to make
      // sure we clean up any existing streams here, as join_setup_iterator
      // assumes the memory is unused.
      qep_tab->iterator.reset();

      join_setup_iterator(qep_tab);
      if (qep_tab->iterator->Init()) {
        rc = NESTED_LOOP_ERROR;
        break;
      }
    }

    int error = qep_tab->iterator->Read();
    if (error > 0 || (join->thd->is_error()))  // Fatal error
      rc = NESTED_LOOP_ERROR;
    else if (error < 0)
      break;
    else if (join->thd->killed)  // Aborted by user
    {
      join->thd->send_kill_message();
      rc = NESTED_LOOP_KILLED;
    } else
      rc = evaluate_join_record(join, qep_tab);
  }

  // Finish rnd scn after sending records
  if (table->file->inited) table->file->ha_rnd_end();

  return rc;
}

/******************************************************************************
  Code for pfs_batch_update
******************************************************************************/

bool QEP_TAB::pfs_batch_update(JOIN *join) const {
  /*
    Use PFS batch mode unless
     1. tab is not an inner-most table, or
     2. a table has eq_ref or const access type, or
     3. this tab contains a subquery that accesses one or more tables
  */

  return !((join->qep_tab + join->primary_tables - 1) != this ||  // 1
           this->type() == JT_EQ_REF ||                           // 2
           this->type() == JT_CONST || this->type() == JT_SYSTEM ||
           (condition() && condition()->has_subquery()));  // 3
}

/**
  @} (end of group Query_Executor)
*/

vector<string> UnqualifiedCountIterator::DebugString() const {
  return {"Count rows in " + string(m_join->qep_tab->table()->alias)};
}

int UnqualifiedCountIterator::Read() {
  if (!m_has_row) {
    return -1;
  }

  for (Item &item : m_join->all_fields) {
    if (item.type() == Item::SUM_FUNC_ITEM &&
        down_cast<Item_sum &>(item).sum_func() == Item_sum::COUNT_FUNC) {
      int error;
      ulonglong count = get_exact_record_count(m_join->qep_tab,
                                               m_join->primary_tables, &error);
      if (error) return 1;

      down_cast<Item_sum_count &>(item).make_const(
          static_cast<longlong>(count));
    }
  }

  // If we are outputting to a temporary table, we need to copy the results
  // into it here. It is also used for nonaggregated items, even when there are
  // no temporary tables involved.
  if (copy_fields_and_funcs(&m_join->tmp_table_param, m_join->thd)) {
    return 1;
  }

  m_has_row = false;
  return 0;
}

int ZeroRowsAggregatedIterator::Read() {
  if (!m_has_row) {
    return -1;
  }

  // Mark tables as containing only NULL values
  for (TABLE_LIST *table = m_join->select_lex->leaf_tables; table;
       table = table->next_leaf) {
    table->table->set_null_row();
  }

  // Calculate aggregate functions for no rows

  /*
    Must notify all fields that there are no rows (not only those
    that will be returned) because join->having may refer to
    fields that are not part of the result columns.
   */
  for (Item &item : m_join->all_fields) {
    item.no_rows_in_result();
  }

  m_has_row = false;
  if (m_examined_rows != nullptr) {
    ++*m_examined_rows;
  }
  return 0;
}

TableValueConstructorIterator::TableValueConstructorIterator(
    THD *thd, ha_rows *examined_rows, const List<List<Item>> &row_value_list,
    List<Item> *join_fields)
    : RowIterator(thd),
      m_examined_rows(examined_rows),
      m_row_value_list(row_value_list),
      m_output_refs(join_fields) {}

bool TableValueConstructorIterator::Init() {
  m_row_it = m_row_value_list.begin();
  return false;
}

int TableValueConstructorIterator::Read() {
  if (*m_examined_rows == m_row_value_list.size()) return -1;

  // If the TVC has a single row, we don't create Item_values_column reference
  // objects during resolving. We will instead use the single row directly from
  // SELECT_LEX::item_list, such that we don't have to change references here.
  if (m_row_value_list.size() != 1) {
    List_STL_Iterator<Item> output_refs_it = m_output_refs->begin();
    for (const Item &value : *m_row_it) {
      Item_values_column &ref =
          down_cast<Item_values_column &>(*output_refs_it);
      ++output_refs_it;

      // Ideally we would not be casting away constness here. However, as the
      // evaluation of Item objects during execution is not const (i.e. none of
      // the val methods are const), the reference contained in a
      // Item_values_column object cannot be const.
      ref.set_value(const_cast<Item *>(&value));
    }
    ++m_row_it;
  }

  ++*m_examined_rows;
  return 0;
}
