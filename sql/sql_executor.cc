/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "my_config.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "binary_log_types.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "map_helpers.h"
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
#include "mysql/service_mysql_alloc.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/enum_query_type.h"
#include "sql/field.h"
#include "sql/filesort.h"  // Filesort
#include "sql/handler.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_sum.h"  // Item_sum
#include "sql/json_dom.h"  // Json_wrapper
#include "sql/key.h"       // key_cmp
#include "sql/key_spec.h"
#include "sql/log.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"  // stage_executing
#include "sql/nested_join.h"
#include "sql/opt_explain_format.h"
#include "sql/opt_range.h"  // QUICK_SELECT_I
#include "sql/opt_trace.h"  // Opt_trace_object
#include "sql/opt_trace_context.h"
#include "sql/parse_tree_nodes.h"  // PT_frame
#include "sql/protocol.h"
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/query_result.h"   // Query_result
#include "sql/record_buffer.h"  // Record_buffer
#include "sql/sql_base.h"       // fill_record
#include "sql/sql_bitmap.h"
#include "sql/sql_error.h"
#include "sql/sql_join_buffer.h"  // CACHE_FIELD
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_select.h"
#include "sql/sql_show.h"  // get_schema_tables_result
#include "sql/sql_sort.h"
#include "sql/sql_tmp_table.h"  // create_tmp_table
#include "sql/system_variables.h"
#include "sql/table_function.h"
#include "sql/thr_malloc.h"
#include "sql/window.h"
#include "sql/window_lex.h"
#include "sql_string.h"
#include "template_utils.h"
#include "thr_lock.h"

using std::max;
using std::min;

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
static void copy_sum_funcs(Item_sum **func_ptr, Item_sum **end_ptr);

static int read_system(TABLE *table);
static int join_read_const(QEP_TAB *tab);
static int read_const(TABLE *table, TABLE_REF *ref);
static int join_read_key(QEP_TAB *tab);
static int join_read_always_key(QEP_TAB *tab);
static int join_no_more_records(READ_RECORD *info);
static int join_read_next(READ_RECORD *info);
static int join_read_next_same(READ_RECORD *info);
static int join_read_prev(READ_RECORD *info);
static int join_ft_read_first(QEP_TAB *tab);
static int join_ft_read_next(READ_RECORD *info);
static int join_read_always_key_or_null(QEP_TAB *tab);
static int join_read_next_same_or_null(READ_RECORD *info);
static int create_sort_index(THD *thd, JOIN *join, QEP_TAB *tab);
static bool remove_dup_with_compare(THD *thd, TABLE *entry, Field **field,
                                    ulong offset, Item *having);
static bool remove_dup_with_hash_index(THD *thd, TABLE *table,
                                       Field **first_field,
                                       const size_t *field_lengths,
                                       size_t key_length, Item *having);
static int join_read_linked_first(QEP_TAB *tab);
static int join_read_linked_next(READ_RECORD *info);
static int do_sj_reset(SJ_TMP_TABLE *sj_tbl);
static bool alloc_group_fields(JOIN *join, ORDER *group);

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

void Temp_table_param::cleanup(void) {
  destroy_array(copy_field, field_count);
  copy_field = NULL;
  copy_field_end = NULL;
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
  DBUG_ENTER("JOIN::exec");

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

  if (prepare_result()) DBUG_VOID_RETURN;

  if (m_windows.elements > 0 && !m_windowing_steps) {
    // Initialize state of window functions as end_write_wf() will be shortcut
    List_iterator<Window> li(m_windows);
    Window *w;
    while ((w = li++)) w->reset_all_wf_state();
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
              *columns_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
        DBUG_VOID_RETURN;

      /*
        If the HAVING clause is either impossible or always true, then
        JOIN::having is set to NULL by optimize_cond.
        In this case JOIN::exec must check for JOIN::having_value, in the
        same way it checks for JOIN::cond_value.
      */
      if (((select_lex->having_value != Item::COND_FALSE) &&
           having_is_true(having_cond)) &&
          do_send_rows && query_result->send_data(fields_list))
        error = 1;
      else {
        error = (int)query_result->send_eof();
        send_records = calc_found_rows ? 1 : thd->get_sent_row_count();
      }
      /* Query block (without union) always returns 0 or 1 row */
      thd->current_found_rows = send_records;
    } else {
      return_zero_rows(this, *columns_list);
    }
    DBUG_VOID_RETURN;
  }

  if (zero_result_cause) {
    return_zero_rows(this, *columns_list);
    DBUG_VOID_RETURN;
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
    DBUG_VOID_RETURN;
  }

  THD_STAGE_INFO(thd, stage_sending_data);
  DBUG_PRINT("info", ("%s", thd->proc_info));
  if (query_result->send_result_set_metadata(
          *fields, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF)) {
    /* purecov: begin inspected */
    error = 1;
    DBUG_VOID_RETURN;
    /* purecov: end */
  }
  error = do_select(this);
  /* Accumulate the counts from all join iterations of all join parts. */
  thd->inc_examined_row_count(examined_rows);
  DBUG_PRINT("counts", ("thd->examined_row_count: %lu",
                        (ulong)thd->get_examined_row_count()));

  DBUG_VOID_RETURN;
}

bool JOIN::create_intermediate_table(QEP_TAB *const tab,
                                     List<Item> *tmp_table_fields,
                                     ORDER_with_src &tmp_table_group,
                                     bool save_sum_fields) {
  DBUG_ENTER("JOIN::create_intermediate_table");
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
  if (!table) DBUG_RETURN(true);
  tmp_table_param.using_outer_summary_function =
      tab->tmp_table_param->using_outer_summary_function;

  DBUG_ASSERT(tab->idx() > 0);
  tab[-1].next_select = sub_select_op;
  if (!(tab->op = new (thd->mem_root) QEP_tmp_table(tab))) goto err;

  tab->set_table(table);

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
    THD_STAGE_INFO(thd, stage_sorting_for_group);

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
      THD_STAGE_INFO(thd, stage_sorting_for_order);

      if (m_ordered_index_usage != ORDERED_INDEX_ORDER_BY &&
          add_sorting_to_table(const_tables, &order))
        goto err;
      order = NULL;
    }
  }
  DBUG_RETURN(false);

err:
  if (table != NULL) {
    free_tmp_table(thd, table);
    tab->set_table(NULL);
  }
  DBUG_RETURN(true);
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
    copy_ref_item_slice(ref_items[REF_SLICE_BASE], rollup.ref_item_arrays[i]);
    current_ref_item_slice = -1;  // as we switched to a not-numbered slice
    if (having_is_true(having_cond)) {
      if (send_records < unit->select_limit_cnt && do_send_rows &&
          select_lex->query_result()->send_data(rollup.fields_list[i]))
        return true;
      send_records++;
    }
  }
  // Restore ref_items array
  set_ref_item_slice(save_slice);
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
  @param table_arg           Reference to temp table

  @returns false if success, true if error
*/

bool JOIN::rollup_write_data(uint idx, TABLE *table_arg) {
  uint save_slice = current_ref_item_slice;
  for (uint i = send_group_parts; i-- > idx;) {
    // Get references to sum functions in place
    copy_ref_item_slice(ref_items[REF_SLICE_BASE], rollup.ref_item_arrays[i]);
    current_ref_item_slice = -1;  // as we switched to a not-numbered slice
    if (having_is_true(having_cond)) {
      int write_error;
      Item *item;
      List_iterator_fast<Item> it(rollup.all_fields[i]);
      while ((item = it++)) {
        if ((item->type() == Item::NULL_ITEM ||
             item->type() == Item::NULL_RESULT_ITEM) &&
            item->is_result_field())
          item->save_in_result_field(1);
      }
      copy_sum_funcs(sum_funcs_end[i + 1], sum_funcs_end[i]);
      if ((write_error = table_arg->file->ha_write_row(table_arg->record[0]))) {
        if (create_ondisk_from_heap(
                thd, table_arg, tmp_table_param.start_recinfo,
                &tmp_table_param.recinfo, write_error, false, NULL))
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
  DBUG_ENTER("prepare_sum_aggregators");
  while ((func = *(func_ptr++))) {
    if (func->set_aggregator(need_distinct && func->has_with_distinct()
                                 ? Aggregator::DISTINCT_AGGREGATOR
                                 : Aggregator::SIMPLE_AGGREGATOR))
      DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
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
  DBUG_ENTER("setup_sum_funcs");
  while ((func = *(func_ptr++))) {
    if (func->aggregator_setup(thd)) DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}

static void init_tmptable_sum_functions(Item_sum **func_ptr) {
  DBUG_ENTER("init_tmptable_sum_functions");
  Item_sum *func;
  while ((func = *(func_ptr++))) func->reset_field();
  DBUG_VOID_RETURN;
}

/** Update record 0 in tmp_table from record 1. */

static void update_tmptable_sum_func(Item_sum **func_ptr,
                                     TABLE *tmp_table MY_ATTRIBUTE((unused))) {
  DBUG_ENTER("update_tmptable_sum_func");
  Item_sum *func;
  while ((func = *(func_ptr++))) func->update_field();
  DBUG_VOID_RETURN;
}

/** Copy result of sum functions to record in tmp_table. */

static void copy_sum_funcs(Item_sum **func_ptr, Item_sum **end_ptr) {
  DBUG_ENTER("copy_sum_funcs");
  for (; func_ptr != end_ptr; func_ptr++) (*func_ptr)->save_in_result_field(1);
  DBUG_VOID_RETURN;
}

static bool init_sum_functions(Item_sum **func_ptr, Item_sum **end_ptr) {
  for (; func_ptr != end_ptr; func_ptr++) {
    if ((*func_ptr)->reset_and_add()) return 1;
  }
  /* If rollup, calculate the upper sum levels */
  for (; *func_ptr; func_ptr++) {
    if ((*func_ptr)->aggregator_add()) return 1;
  }
  return 0;
}

static bool update_sum_func(Item_sum **func_ptr) {
  DBUG_ENTER("update_sum_func");
  Item_sum *func;
  for (; (func = *func_ptr); func_ptr++)
    if (func->aggregator_add()) DBUG_RETURN(1);
  DBUG_RETURN(0);
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
  DBUG_ENTER("copy_funcs");
  if (!param->items_to_copy->size()) DBUG_RETURN(false);

  Func_ptr_array *func_ptr = param->items_to_copy;
  uint end = func_ptr->size();
  for (uint i = 0; i < end; i++) {
    Item *f = func_ptr->at(i).func();
    bool do_copy = false;
    switch (type) {
      case CFT_ALL:
        do_copy = true;
        break;
      case CFT_WF_FRAMING:
        do_copy =
            (f->m_is_window_function && down_cast<Item_sum *>(f)->framing());
        break;
      case CFT_WF_NON_FRAMING:
        do_copy =
            (f->m_is_window_function && !down_cast<Item_sum *>(f)->framing() &&
             !down_cast<Item_sum *>(f)->needs_card());
        break;
      case CFT_WF_NEEDS_CARD:
        do_copy =
            (f->m_is_window_function && down_cast<Item_sum *>(f)->needs_card());
        break;
      case CFT_WF_USES_ONLY_ONE_ROW:
        do_copy = (f->m_is_window_function &&
                   down_cast<Item_sum *>(f)->uses_only_one_row());
        break;
      case CFT_NON_WF:
        do_copy = !f->m_is_window_function;
        if (do_copy)  // copying an expression of a WF would be wrong:
          DBUG_ASSERT(!f->has_wf());
        break;
      case CFT_WF:
        do_copy = f->m_is_window_function;
        break;
    }

    if (do_copy) {
      f->save_in_result_field(1);
      /*
        Need to check the THD error state because Item::val_xxx() don't
        return error code, but can generate errors
        TODO: change it for a real status check when Item::val_xxx()
        are extended to return status code.
      */
      if (thd->is_error()) DBUG_RETURN(true);
    }
  }
  DBUG_RETURN(false);
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
  DBUG_ENTER("end_sj_materialize");
  if (!end_of_records) {
    TABLE *table = sjm->table;

    List_iterator<Item> it(sjm->sj_nest->nested_join->sj_inner_exprs);
    Item *item;
    while ((item = it++)) {
      if (item->is_null()) DBUG_RETURN(NESTED_LOOP_OK);
    }
    fill_record(thd, table, table->visible_field_ptr(),
                sjm->sj_nest->nested_join->sj_inner_exprs, NULL, NULL);
    if (thd->is_error())
      DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
    if (!check_unique_constraint(table)) DBUG_RETURN(NESTED_LOOP_OK);
    if ((error = table->file->ha_write_row(table->record[0]))) {
      /* create_ondisk_from_heap will generate error if needed */
      if (!table->file->is_ignorable_error(error)) {
        if (create_ondisk_from_heap(thd, table, sjm->table_param.start_recinfo,
                                    &sjm->table_param.recinfo, error, true,
                                    NULL))
          DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
        /* Initialize the index, since create_ondisk_from_heap does
           not replicate the earlier index initialization */
        if (table->hash_field) table->file->ha_index_init(0, false);
      }
    }
  }
  DBUG_RETURN(NESTED_LOOP_OK);
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
    List<Item> *cond_list = ((Item_cond *)cond)->argument_list();
    List_iterator_fast<Item> li(*cond_list);
    Item *item;
    while ((item = li++)) {
      if (update_const_equal_items(thd, item, tab)) return true;
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
        Field *field = item_field->field;
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
  DBUG_ENTER("return_zero_rows");

  join->join_free();

  /* Update results for FOUND_ROWS */
  if (!join->send_row_on_empty_set()) {
    join->thd->current_found_rows = 0;
  }

  SELECT_LEX *const select = join->select_lex;

  if (!(select->query_result()->send_result_set_metadata(
          fields, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))) {
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
      List_iterator_fast<Item> it(join->all_fields);
      Item *item;
      while ((item = it++)) item->no_rows_in_result();

      if (having_is_true(join->having_cond))
        send_error = select->query_result()->send_data(fields);
    }
    if (!send_error) select->query_result()->send_eof();  // Should be safe
  }
  DBUG_VOID_RETURN;
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
  DBUG_ENTER("setup_tmptable_write_func");
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
  } else if (join->sort_and_group && !tmp_tbl->precomputed_group_by) {
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
        tmp_tbl->items_to_copy->push_back(func);
      }
    }
  }
  if (description) trace->add_alnum("write_method", description);
  DBUG_VOID_RETURN;
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
  DBUG_ENTER("get_end_select_func");
  /*
     Choose method for presenting result to user. Use end_send_group
     if the query requires grouping (has a GROUP BY clause and/or one or
     more aggregate functions). Use end_send if the query should not
     be grouped.
   */
  if (sort_and_group && !tmp_table_param.precomputed_group_by) {
    DBUG_PRINT("info", ("Using end_send_group"));
    DBUG_RETURN(end_send_group);
  }
  DBUG_PRINT("info", ("Using end_send"));
  DBUG_RETURN(end_send);
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
static bool set_record_buffer(const QEP_TAB *tab) {
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
  DBUG_ENTER("do_select");

  join->send_records = 0;

  if (join->plan_is_const()) {
    DBUG_ASSERT(!join->need_tmp_before_win);
    Next_select_func end_select = join->get_end_select_func();
    /*
      HAVING will be checked after processing aggregate functions,
      But WHERE should checkd here (we alredy have read tables)

      @todo: consider calling end_select instead of duplicating code
    */
    if (!join->where_cond || join->where_cond->val_int()) {
      // HAVING will be checked by end_select
      error = (*end_select)(join, 0, 0);
      if (error >= NESTED_LOOP_OK) error = (*end_select)(join, 0, 1);

      /*
        If we don't go through evaluate_join_record(), do the counting
        here.  join->send_records is increased on success in end_send(),
        so we don't touch it here.
      */
      join->examined_rows++;
      DBUG_ASSERT(join->examined_rows <= 1);
    } else if (join->send_row_on_empty_set()) {
      table_map save_nullinfo = 0;

      // Calculate aggregate functions for no rows
      List_iterator_fast<Item> it(*join->fields);
      Item *item;
      while ((item = it++)) item->no_rows_in_result();

      /*
        Mark tables as containing only NULL values for processing
        the HAVING clause and for send_data().
        Calculate a set of tables for which NULL values need to be restored
        after sending data.
      */
      if (join->clear_fields(&save_nullinfo))
        error = NESTED_LOOP_ERROR;
      else {
        if (having_is_true(join->having_cond))
          rc = join->select_lex->query_result()->send_data(*join->fields);

        // Restore NULL values if needed.
        if (save_nullinfo) join->restore_fields(save_nullinfo);
      }
    }
    /*
      An error can happen when evaluating the conds
      (the join condition and piece of where clause
      relevant to this join table).
    */
    if (join->thd->is_error()) error = NESTED_LOOP_ERROR;
  } else {
    QEP_TAB *qep_tab = join->qep_tab + join->const_tables;
    DBUG_ASSERT(join->primary_tables);
    error = join->first_select(join, qep_tab, 0);
    if (error >= NESTED_LOOP_OK) error = join->first_select(join, qep_tab, 1);
  }

  join->thd->current_found_rows = join->send_records;
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
      join->thd->current_found_rows = sort_tab->records();
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
      if (join->select_lex->query_result()->send_eof())
        rc = 1;  // Don't send error
      DBUG_PRINT("info", ("%ld records output", (long)join->send_records));
    }
  }

  rc = join->thd->is_error() ? -1 : rc;
#ifndef DBUG_OFF
  if (rc) {
    DBUG_PRINT("error", ("Error: do_select() failed"));
  }
#endif
  DBUG_RETURN(rc);
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
  DBUG_ENTER("sub_select_op");

  if (join->thd->killed) {
    /* The user has aborted the execution of the query */
    join->thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED);
  }

  enum_nested_loop_state rc;
  QEP_operation *op = qep_tab->op;

  /* This function cannot be called if qep_tab has no associated operation */
  DBUG_ASSERT(op != NULL);
  if (end_of_records) {
    rc = op->end_send();
    if (rc >= NESTED_LOOP_OK) rc = sub_select(join, qep_tab, end_of_records);
    DBUG_RETURN(rc);
  }
  if (qep_tab->prepare_scan()) DBUG_RETURN(NESTED_LOOP_ERROR);

  /*
    setup_join_buffering() disables join buffering if QS_DYNAMIC_RANGE is
    enabled.
  */
  DBUG_ASSERT(!qep_tab->dynamic_range());

  rc = op->put_record();

  DBUG_RETURN(rc);
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
    To carry out a return to a nested loop level of join table t the pointer
    to t is remembered in the field 'return_tab' of the join structure.
    Consider the following query:
    @code
        SELECT * FROM t1,
                      LEFT JOIN
                      (t2, t3 LEFT JOIN (t4,t5) ON t5.a=t3.a)
                      ON t4.a=t2.a
           WHERE (t2.b=5 OR t2.b IS NULL) AND (t4.b=2 OR t4.b IS NULL)
    @endcode
    Suppose the chosen execution plan dictates the order t1,t2,t3,t4,t5
    and suppose for a given joined rows from tables t1,t2,t3 there are
    no rows in the result set yet.
    When first row from t5 that satisfies the on condition
    t5.a=t3.a is found, the pushed down predicate t4.b=2 OR t4.b IS NULL
    becomes 'activated', as well the predicate t4.a=t2.a. But
    the predicate (t2.b=5 OR t2.b IS NULL) can not be checked until
    t4.a=t2.a becomes true.
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
  DBUG_ENTER("sub_select");

  TABLE *const table = qep_tab->table();

  /*
    Enable the items which one should use if one wants to evaluate anything
    (e.g. functions in WHERE, HAVING) involving columns of this table.
  */
  Switch_ref_item_slice slice_switch(join, qep_tab->ref_item_slice);

  if (end_of_records) {
    enum_nested_loop_state nls =
        (*qep_tab->next_select)(join, qep_tab + 1, end_of_records);

    DBUG_RETURN(nls);
  }
  READ_RECORD *info = &qep_tab->read_record;

  if (qep_tab->prepare_scan()) DBUG_RETURN(NESTED_LOOP_ERROR);

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
    if (join->unit->got_all_recursive_rows) DBUG_RETURN(rc);
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

  const bool pfs_batch_update = qep_tab->pfs_batch_update(join);
  if (pfs_batch_update) table->file->start_psi_batch_mode();

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
      error = (*qep_tab->read_first_record)(qep_tab);
    } else
      error = info->read_record(info);

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
      if (qep_tab->keep_current_rowid) table->file->position(table->record[0]);
      rc = evaluate_join_record(join, qep_tab);
    }
  }

  if (rc == NESTED_LOOP_OK && qep_tab->last_inner() != NO_PLAN_IDX &&
      !qep_tab->found)
    rc = evaluate_null_complemented_join_record(join, qep_tab);

  if (pfs_batch_update) table->file->end_psi_batch_mode();

  DBUG_RETURN(rc);
}

/**
  @brief Prepare table to be scanned.

  @details This function is the place to do any work on the table that
  needs to be done before table can be scanned. Currently it
  only materialized derived tables and semi-joined subqueries and binds
  buffer for current rowid.

  @returns false - Ok, true  - error
*/

bool QEP_TAB::prepare_scan() {
  // Check whether materialization is required.
  if (!materialize_table || (table()->materialized && !rematerialize))
    return false;

  // Materialize table prior to reading it
  if ((*materialize_table)(this)) return true;

  // Bind to the rowid buffer managed by the TABLE object.
  if (copy_current_rowid) copy_current_rowid->bind_buffer(table()->file->ref);

  table()->set_not_started();

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
  SJ_TMP_TABLE::TAB *tab = sjtbl->tabs;
  SJ_TMP_TABLE::TAB *tab_end = sjtbl->tabs_end;

  DBUG_ENTER("do_sj_dups_weedout");

  if (sjtbl->is_confluent) {
    if (sjtbl->have_confluent_row)
      DBUG_RETURN(1);
    else {
      sjtbl->have_confluent_row = true;
      DBUG_RETURN(0);
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

  if (!check_unique_constraint(sjtbl->tmp_table)) DBUG_RETURN(1);
  error = sjtbl->tmp_table->file->ha_write_row(sjtbl->tmp_table->record[0]);
  if (error) {
    /* If this is a duplicate error, return immediately */
    if (sjtbl->tmp_table->file->is_ignorable_error(error)) DBUG_RETURN(1);
    /*
      Other error than duplicate error: Attempt to create a temporary table.
    */
    bool is_duplicate;
    if (create_ondisk_from_heap(thd, sjtbl->tmp_table, sjtbl->start_recinfo,
                                &sjtbl->recinfo, error, true, &is_duplicate))
      DBUG_RETURN(-1);
    DBUG_RETURN(is_duplicate ? 1 : 0);
  }
  DBUG_RETURN(0);
}

/**
  SemiJoinDuplicateElimination: Reset the temporary table
*/

static int do_sj_reset(SJ_TMP_TABLE *sj_tbl) {
  DBUG_ENTER("do_sj_reset");
  if (sj_tbl->tmp_table) {
    int rc = sj_tbl->tmp_table->file->ha_delete_all_rows();
    DBUG_RETURN(rc);
  }
  sj_tbl->have_confluent_row = false;
  DBUG_RETURN(0);
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
  DBUG_ENTER("evaluate_join_record");
  DBUG_PRINT("enter", ("join: %p join_tab index: %d table: %s cond: %p", join,
                       static_cast<int>(qep_tab_idx), qep_tab->table()->alias,
                       condition));

  if (condition) {
    found = condition->val_int();

    if (join->thd->killed) {
      join->thd->send_kill_message();
      DBUG_RETURN(NESTED_LOOP_KILLED);
    }

    /* check for errors evaluating the condition */
    if (join->thd->is_error()) DBUG_RETURN(NESTED_LOOP_ERROR);
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
            DBUG_RETURN(NESTED_LOOP_OK);
          }

          if (tab == qep_tab)
            found = 0;
          else {
            /*
              Set a return point if rejected predicate is attached
              not to the last table of the current nest level.
            */
            join->return_tab = tab->idx();
            DBUG_RETURN(NESTED_LOOP_OK);
          }
        }
        /* check for errors evaluating the condition */
        if (join->thd->is_error()) DBUG_RETURN(NESTED_LOOP_ERROR);
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
        DBUG_RETURN(NESTED_LOOP_ERROR);
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
    join->examined_rows++;
    DBUG_PRINT("counts", ("evaluate_join_record join->examined_rows++: %lu",
                          (ulong)join->examined_rows));

    if (found) {
      enum enum_nested_loop_state rc;
      // A match is found for the current partial join prefix.
      qep_tab->found_match = true;

      rc = (*qep_tab->next_select)(join, qep_tab + 1, 0);

      join->thd->get_stmt_da()->inc_current_row_for_condition();
      if (rc != NESTED_LOOP_OK) DBUG_RETURN(rc);

      /* check for errors evaluating the condition */
      if (join->thd->is_error()) DBUG_RETURN(NESTED_LOOP_ERROR);

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
        set_if_smaller(return_tab, qep_tab->firstmatch_return);
      }

      /*
        Test if this was a SELECT DISTINCT query on a table that
        was not in the field list;  In this case we can abort if
        we found a row, as no new rows can be added to the result.
      */
      if (not_used_in_distinct && found_records != join->found_records)
        set_if_smaller(return_tab, qep_tab_idx - 1);

      set_if_smaller(join->return_tab, return_tab);
    } else {
      join->thd->get_stmt_da()->inc_current_row_for_condition();
      if (qep_tab->not_null_compl) {
        /* a NULL-complemented row is not in a table so cannot be locked */
        qep_tab->read_record.unlock_row(qep_tab);
      }
    }
  } else {
    /*
      The condition pushed down to the table join_tab rejects all rows
      with the beginning coinciding with the current partial join.
    */
    join->examined_rows++;
    join->thd->get_stmt_da()->inc_current_row_for_condition();
    if (qep_tab->not_null_compl) qep_tab->read_record.unlock_row(qep_tab);
  }
  DBUG_RETURN(NESTED_LOOP_OK);
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

  DBUG_ENTER("evaluate_null_complemented_join_record");

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
        DBUG_RETURN(NESTED_LOOP_KILLED);
      }

      /* check for errors */
      if (join->thd->is_error()) DBUG_RETURN(NESTED_LOOP_ERROR);

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
    // Restore NULL bits saved when reading row, @see join_read_key()
    if (tab->type() == JT_EQ_REF) tab->table()->restore_null_flags();
  }

  DBUG_RETURN(rc);
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
  DBUG_ENTER("join_read_const_table");
  TABLE *table = tab->table();
  table->const_table = 1;

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
    /* Mark for EXPLAIN that the row was not found */
    pos->filter_effect = 1.0;
    pos->rows_fetched = 0.0;
    pos->prefix_rowcount = 0.0;
    pos->ref_depend_map = 0;
    if (!tab->table_ref->outer_join || error > 0) DBUG_RETURN(error);
  }

  if (tab->join_cond() && !table->has_null_row()) {
    // We cannot handle outer-joined tables with expensive join conditions here:
    DBUG_ASSERT(!tab->join_cond()->is_expensive());
    if (tab->join_cond()->val_int() == 0) table->set_null_row();
  }

  /* Check appearance of new constant items in Item_equal objects */
  JOIN *const join = tab->join();
  THD *const thd = join->thd;
  if (join->where_cond && update_const_equal_items(thd, join->where_cond, tab))
    DBUG_RETURN(1);
  TABLE_LIST *tbl;
  for (tbl = join->select_lex->leaf_tables; tbl; tbl = tbl->next_leaf) {
    TABLE_LIST *embedded;
    TABLE_LIST *embedding = tbl;
    do {
      embedded = embedding;
      if (embedded->join_cond_optim() &&
          update_const_equal_items(thd, embedded->join_cond_optim(), tab))
        DBUG_RETURN(1);
      embedding = embedded->embedding;
    } while (embedding && embedding->nested_join->join_list.head() == embedded);
  }

  DBUG_RETURN(0);
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

/**
  Read a constant table when there is at most one matching row, using an
  index lookup.

  @param tab			Table to read

  @retval 0  Row was found
  @retval -1 Row was not found
  @retval 1  Got an error (other than row not found) during read
*/

static int join_read_const(QEP_TAB *tab) {
  return read_const(tab->table(), &tab->ref());
}

static int read_const(TABLE *table, TABLE_REF *ref) {
  int error;
  DBUG_ENTER("read_const");

  if (!table->is_started())  // If first read
  {
    if (cp_buffer_from_ref(table->in_use, table, ref))
      error = HA_ERR_KEY_NOT_FOUND;
    else {
      error = table->file->ha_index_read_idx_map(
          table->record[0], ref->key, ref->key_buff,
          make_prev_keypart_map(ref->key_parts), HA_READ_KEY_EXACT);
    }
    if (error) {
      if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE) {
        const int ret = report_handler_error(table, error);
        DBUG_RETURN(ret);
      }
      table->set_no_row();
      table->set_null_row();
      empty_record(table);
      DBUG_RETURN(-1);
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
  DBUG_RETURN(table->has_row() ? 0 : -1);
}

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

  @param tab   JOIN_TAB of the accessed table

  @retval  0 - Ok
  @retval -1 - Row not found
  @retval  1 - Error
*/

static int join_read_key(QEP_TAB *tab) {
  TABLE *const table = tab->table();
  TABLE_REF *table_ref = &tab->ref();
  int error;

  if (!table->file->inited) {
    DBUG_ASSERT(!tab->use_order());  // Don't expect sort req. for single row.
    if ((error =
             table->file->ha_index_init(table_ref->key, tab->use_order()))) {
      (void)report_handler_error(table, error);
      return 1;
    }
  }

  /*
    We needn't do "Late NULLs Filtering" because eq_ref is restricted to
    indices on NOT NULL columns (see create_ref_for_key()).
  */

  /*
    Calculate if needed to read row. Always needed if
    - no rows read yet, or
    - cache is disabled, or
    - previous lookup caused error when calculating key.
  */
  bool read_row =
      !table->is_started() || table_ref->disable_cache || table_ref->key_err;
  if (!read_row)
    // Last lookup found a row, copy its key to secondary buffer
    memcpy(table_ref->key_buff2, table_ref->key_buff, table_ref->key_length);

  // Create new key for lookup
  table_ref->key_err = cp_buffer_from_ref(table->in_use, table, table_ref);
  if (table_ref->key_err) {
    table->set_no_row();
    return -1;
  }

  // Re-use current row if keys are equal
  if (!read_row && memcmp(table_ref->key_buff2, table_ref->key_buff,
                          table_ref->key_length) != 0)
    read_row = true;

  if (read_row) {
    /*
       Moving away from the current record. Unlock the row
       in the handler if it did not match the partial WHERE.
     */
    if (table->has_row() && table_ref->use_count == 0)
      table->file->unlock_row();

    error = table->file->ha_index_read_map(
        table->record[0], table_ref->key_buff,
        make_prev_keypart_map(table_ref->key_parts), HA_READ_KEY_EXACT);
    if (error) return report_handler_error(table, error);

    table_ref->use_count = 1;
    table->save_null_flags();
  } else if (table->has_row()) {
    DBUG_ASSERT(!table->has_null_row());
    table_ref->use_count++;
  }

  return table->has_row() ? 0 : -1;
}

/**
  Since join_read_key may buffer a record, do not unlock
  it if it was not used in this invocation of join_read_key().
  Only count locks, thus remembering if the record was left unused,
  and unlock already when pruning the current value of
  TABLE_REF buffer.
  @sa join_read_key()
*/

static void join_read_key_unlock_row(QEP_TAB *tab) {
  DBUG_ASSERT(tab->ref().use_count);
  if (tab->ref().use_count) tab->ref().use_count--;
}

/**
  Read a table *assumed* to be included in execution of a pushed join.
  This is the counterpart of join_read_key() / join_read_always_key()
  for child tables in a pushed join.

  When the table access is performed as part of the pushed join,
  all 'linked' child colums are prefetched together with the parent row.
  The handler will then only format the row as required by MySQL and set
  table status accordingly.

  However, there may be situations where the prepared pushed join was not
  executed as assumed. It is the responsibility of the handler to handle
  these situation by letting @c ha_index_read_pushed() then effectively do a
  plain old' index_read_map(..., HA_READ_KEY_EXACT);

  @param tab			Table to read

  @retval
    0	Row was found
  @retval
    -1   Row was not found
  @retval
    1   Got an error (other than row not found) during read
*/
static int join_read_linked_first(QEP_TAB *tab) {
  int error;
  TABLE *table = tab->table();
  DBUG_ENTER("join_read_linked_first");

  DBUG_ASSERT(!tab->use_order());  // Pushed child can't be sorted

  if (!table->file->inited &&
      (error = table->file->ha_index_init(tab->ref().key, tab->use_order()))) {
    (void)report_handler_error(table, error);
    DBUG_RETURN(error);
  }

  /* Perform "Late NULLs Filtering" (see internals manual for explanations) */
  if (tab->ref().impossible_null_ref()) {
    table->set_no_row();
    DBUG_PRINT("info", ("join_read_linked_first null_rejected"));
    DBUG_RETURN(-1);
  }

  if (cp_buffer_from_ref(tab->join()->thd, table, &tab->ref())) {
    table->set_no_row();
    DBUG_RETURN(-1);
  }

  // 'read' itself is a NOOP:
  //  handler::ha_index_read_pushed() only unpack the prefetched row and
  //  set 'status'
  error = table->file->ha_index_read_pushed(
      table->record[0], tab->ref().key_buff,
      make_prev_keypart_map(tab->ref().key_parts));
  if (error) {
    const int ret = report_handler_error(table, error);
    DBUG_RETURN(ret);
  }
  DBUG_RETURN(0);
}

static int join_read_linked_next(READ_RECORD *info) {
  TABLE *table = info->table;
  DBUG_ENTER("join_read_linked_next");

  int error = table->file->ha_index_next_pushed(table->record[0]);
  if (error) {
    const int ret = report_handler_error(table, error);
    DBUG_RETURN(ret);
  }
  DBUG_RETURN(error);
}

/*
  ref access method implementation: "read_first" function

  SYNOPSIS
    join_read_always_key()
      tab  JOIN_TAB of the accessed table

  DESCRIPTION
    This is "read_first" function for the "ref" access method.

    The function must leave the index initialized when it returns.
    ref_or_null access implementation depends on that.

  RETURN
    0  - Ok
   -1  - Row not found
    1  - Error
*/

static int join_read_always_key(QEP_TAB *tab) {
  int error;
  TABLE *table = tab->table();

  /* Initialize the index first */
  if (init_index_and_record_buffer(tab, table->file, tab->ref().key,
                                   tab->use_order()))
    return 1;

  /* Perform "Late NULLs Filtering" (see internals manual for explanations) */
  TABLE_REF *ref = &tab->ref();
  if (ref->impossible_null_ref()) {
    DBUG_PRINT("info", ("join_read_always_key null_rejected"));
    table->set_no_row();
    return -1;
  }

  if (cp_buffer_from_ref(tab->join()->thd, table, ref)) {
    table->set_no_row();
    return -1;
  }
  if ((error = table->file->ha_index_read_map(
           table->record[0], tab->ref().key_buff,
           make_prev_keypart_map(tab->ref().key_parts), HA_READ_KEY_EXACT)))
    return report_handler_error(table, error);

  return 0;
}

/**
  This function is used when optimizing away ORDER BY in
  SELECT * FROM t1 WHERE a=1 ORDER BY a DESC,b DESC.
*/

int join_read_last_key(QEP_TAB *tab) {
  int error;
  TABLE *table = tab->table();

  if (init_index_and_record_buffer(tab, table->file, tab->ref().key,
                                   tab->use_order()))
    return 1; /* purecov: inspected */
  if (cp_buffer_from_ref(tab->join()->thd, table, &tab->ref())) {
    table->set_no_row();
    return -1;
  }
  if ((error = table->file->ha_index_read_last_map(
           table->record[0], tab->ref().key_buff,
           make_prev_keypart_map(tab->ref().key_parts))))
    return report_handler_error(table, error);

  return 0;
}

/* ARGSUSED */
static int join_no_more_records(READ_RECORD *info MY_ATTRIBUTE((unused))) {
  return -1;
}

static int join_read_next_same(READ_RECORD *info) {
  int error;
  TABLE *table = info->table;
  QEP_TAB *tab = table->reginfo.qep_tab;

  if ((error = table->file->ha_index_next_same(
           table->record[0], tab->ref().key_buff, tab->ref().key_length)))
    return report_handler_error(table, error);

  return 0;
}

int join_read_prev_same(READ_RECORD *info) {
  int error;
  TABLE *table = info->table;
  QEP_TAB *tab = table->reginfo.qep_tab;

  /*
    Using ha_index_prev() for reading records from the table can cause
    performance issues if used in combination with ICP. The ICP code
    in the storage engine does not know when to stop reading from the
    index and a call to ha_index_prev() might cause the storage engine
    to read to the beginning of the index if no qualifying record is
    found.
  */
  DBUG_ASSERT(table->file->pushed_idx_cond == NULL);

  if ((error = table->file->ha_index_prev(table->record[0])))
    return report_handler_error(table, error);
  if (key_cmp_if_same(table, tab->ref().key_buff, tab->ref().key,
                      tab->ref().key_length)) {
    table->set_no_row();
    error = -1;
  }
  return error;
}

int join_init_quick_read_record(QEP_TAB *tab) {
  /*
    This is for QS_DYNAMIC_RANGE, i.e., "Range checked for each
    record". The trace for the range analysis below this point will
    be printed with different ranges for every record to the left of
    this table in the join.
  */

  THD *const thd = tab->join()->thd;
  Opt_trace_context *const trace = &thd->opt_trace;
  const bool disable_trace =
      tab->quick_traced_before &&
      !trace->feature_enabled(Opt_trace_context::DYNAMIC_RANGE);
  Opt_trace_disable_I_S disable_trace_wrapper(trace, disable_trace);

  tab->quick_traced_before = true;

  Opt_trace_object wrapper(trace);
  Opt_trace_object trace_table(trace, "rows_estimation_per_outer_row");
  trace_table.add_utf8_table(tab->table_ref);

  /*
    If this join tab was read through a QUICK for the last record
    combination from earlier tables, deleting that quick will close the
    index. Otherwise, we need to close the index before the next join
    iteration starts because the handler object might be reused by a different
    access strategy.
  */
  if (!tab->quick() && (tab->table()->file->inited != handler::NONE))
    tab->table()->file->ha_index_or_rnd_end();

  Key_map needed_reg_dummy;
  QUICK_SELECT_I *old_qck = tab->quick();
  QUICK_SELECT_I *qck;
  DEBUG_SYNC(thd, "quick_not_created");
  const int rc = test_quick_select(thd, tab->keys(),
                                   0,  // empty table map
                                   HA_POS_ERROR,
                                   false,  // don't force quick range
                                   ORDER_NOT_RELEVANT, tab, tab->condition(),
                                   &needed_reg_dummy, &qck);
  if (thd->is_error())  // @todo consolidate error reporting of
                        // test_quick_select
    return 1;
  DBUG_ASSERT(old_qck == NULL || old_qck != qck);
  tab->set_quick(qck);

  /*
    EXPLAIN CONNECTION is used to understand why a query is currently taking
    so much time. So it makes sense to show what the execution is doing now:
    is it a table scan or a range scan? A range scan on which index.
    So: below we want to change the type and quick visible in EXPLAIN, and for
    that, we need to take mutex and change type and quick_optim.
  */

  DEBUG_SYNC(thd, "quick_created_before_mutex");

  thd->lock_query_plan();
  tab->set_type(qck ? calc_join_type(qck->get_type()) : JT_ALL);
  tab->set_quick_optim();
  thd->unlock_query_plan();

  delete old_qck;
  DEBUG_SYNC(thd, "quick_droped_after_mutex");

  return (rc == -1) ? -1 : /* No possible records */
             join_init_read_record(tab);
}

int read_first_record_seq(QEP_TAB *tab) {
  if (tab->read_record.table->file->ha_rnd_init(1)) return 1;
  return (*tab->read_record.read_record)(&tab->read_record);
}

/**
  @brief Prepare table for reading rows and read first record.
  @details
    Prior to reading the table following tasks are done, (in the order of
    execution):
      .) derived tables are materialized
      .) duplicates removed (tmp tables only)
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

int join_init_read_record(QEP_TAB *tab) {
  int error;

  if (tab->needs_duplicate_removal &&
      tab->remove_duplicates())  // Remove duplicates.
    return 1;
  if (tab->filesort && tab->sort_table())  // Sort table.
    return 1;

  /*
    Only attempt to allocate a record buffer the first time the handler is
    initialized.
  */
  const bool first_init = !tab->table()->file->inited;

  if (tab->quick() && (error = tab->quick()->reset())) {
    /* Ensures error status is propageted back to client */
    (void)report_handler_error(tab->table(), error);
    return 1;
  }
  if (init_read_record(&tab->read_record, tab->join()->thd, NULL, tab, 1,
                       false))
    return 1;

  if (first_init && tab->table()->file->inited && set_record_buffer(tab))
    return 1; /* purecov: inspected */

  return (*tab->read_record.read_record)(&tab->read_record);
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
  res |= derived->cleanup_derived();
  DEBUG_SYNC(thd, "after_materialize_derived");
  return res ? NESTED_LOOP_ERROR : NESTED_LOOP_OK;
}

/*
  Helper function for materialization of a semi-joined subquery.

  @param tab JOIN_TAB referencing a materialized semi-join table

  @return Nested loop state
*/

int join_materialize_semijoin(QEP_TAB *tab) {
  DBUG_ENTER("join_materialize_semijoin");

  Semijoin_mat_exec *const sjm = tab->sj_mat_exec();

  QEP_TAB *const first = tab->join()->qep_tab + sjm->inner_table_index;
  QEP_TAB *const last = first + (sjm->table_count - 1);
  /*
    Set up the end_sj_materialize function after the last inner table,
    so that generated rows are inserted into the materialized table.
  */
  last->next_select = end_sj_materialize;
  last->set_sj_mat_exec(sjm);  // TODO: This violates comment for sj_mat_exec!
  if (tab->table()->hash_field) tab->table()->file->ha_index_init(0, 0);
  int rc;
  if ((rc = sub_select(tab->join(), first, false)) < 0) DBUG_RETURN(rc);
  if ((rc = sub_select(tab->join(), first, true)) < 0) DBUG_RETURN(rc);
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
  DBUG_RETURN(NESTED_LOOP_OK);
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

/*
  Helper function for sorting table with filesort.
*/

bool QEP_TAB::sort_table() {
  DBUG_ENTER("QEP_TAB::sort_table");
  DBUG_PRINT("info", ("Sorting for index"));
  THD_STAGE_INFO(join()->thd, stage_creating_sort_index);
  DBUG_ASSERT(join()->m_ordered_index_usage !=
              (filesort->order == join()->order
                   ? JOIN::ORDERED_INDEX_ORDER_BY
                   : JOIN::ORDERED_INDEX_GROUP_BY));
  const bool rc = create_sort_index(join()->thd, join(), this) != 0;
  /*
    Filesort has filtered rows already (see skip_record() in
    find_all_keys()): so we can simply scan the cache, so have to set
    quick=NULL.
    But if we do this, we still need to delete the quick, now or later. We
    cannot do it now: the dtor of quick_index_merge would do free_io_cache,
    but the cache has to remain, because scan will read from it.
    So we delay deletion: we just let the "quick" continue existing in
    "quick_optim"; double benefit:
    - EXPLAIN will show the "quick_optim"
    - it will be deleted late enough.

    There is an exception to the reasoning above. If the filtering condition
    contains a condition triggered by Item_func_trig_cond::FOUND_MATCH
    (i.e. QEP_TAB is inner to an outer join), the trigger variable is still
    false at this stage, so the condition evaluated to true in skip_record()
    and did not filter rows. In that case, we leave the condition in place for
    the next stage (evaluate_join_record()). We can still delete the QUICK as
    triggered conditions don't use that.
    If you wonder how we can come here for such inner table: it can happen if
    the outer table is constant (so the inner one is first-non-const) and a
    window function requires sorting.
  */
  set_quick(NULL);
  if (!is_inner_table_of_outer_join()) set_condition(NULL);
  DBUG_RETURN(rc);
}

int join_read_first(QEP_TAB *tab) {
  int error;
  TABLE *table = tab->table();
  if (table->covering_keys.is_set(tab->index()) && !table->no_keyread)
    table->set_keyread(true);
  tab->read_record.table = table;
  tab->read_record.record = table->record[0];
  tab->read_record.read_record = join_read_next;

  if (init_index_and_record_buffer(tab, table->file, tab->index(),
                                   tab->use_order()))
    return 1;
  if ((error = table->file->ha_index_first(tab->table()->record[0])))
    return report_handler_error(table, error);

  return 0;
}

static int join_read_next(READ_RECORD *info) {
  int error;
  if ((error = info->table->file->ha_index_next(info->record)))
    return report_handler_error(info->table, error);
  return 0;
}

int join_read_last(QEP_TAB *tab) {
  TABLE *table = tab->table();
  int error;
  if (table->covering_keys.is_set(tab->index()) && !table->no_keyread)
    table->set_keyread(true);
  tab->read_record.read_record = join_read_prev;
  tab->read_record.table = table;
  tab->read_record.record = table->record[0];
  if (init_index_and_record_buffer(tab, table->file, tab->index(),
                                   tab->use_order()))
    return 1; /* purecov: inspected */
  if ((error = table->file->ha_index_last(table->record[0])))
    return report_handler_error(table, error);
  return 0;
}

static int join_read_prev(READ_RECORD *info) {
  int error;
  if ((error = info->table->file->ha_index_prev(info->record)))
    return report_handler_error(info->table, error);
  return 0;
}

static int join_ft_read_first(QEP_TAB *tab) {
  int error;
  TABLE *table = tab->table();

  if (!table->file->inited &&
      (error = table->file->ha_index_init(tab->ref().key, tab->use_order()))) {
    (void)report_handler_error(table, error);
    return 1;
  }
  table->file->ft_init();

  if ((error = table->file->ha_ft_read(table->record[0])))
    return report_handler_error(table, error);
  return 0;
}

static int join_ft_read_next(READ_RECORD *info) {
  int error;
  if ((error = info->table->file->ha_ft_read(info->table->record[0])))
    return report_handler_error(info->table, error);
  return 0;
}

/**
  Reading of key with key reference and one part that may be NULL.
*/

static int join_read_always_key_or_null(QEP_TAB *tab) {
  int res;

  /* First read according to key which is NOT NULL */
  *tab->ref().null_ref_key = 0;  // Clear null byte
  if ((res = join_read_always_key(tab)) >= 0) return res;

  /* Then read key with null value */
  *tab->ref().null_ref_key = 1;  // Set null byte
  return safe_index_read(tab);
}

static int join_read_next_same_or_null(READ_RECORD *info) {
  int error;
  if ((error = join_read_next_same(info)) >= 0) return error;
  QEP_TAB *tab = info->table->reginfo.qep_tab;

  /* Test if we have already done a read after null key */
  if (*tab->ref().null_ref_key) return -1;  // All keys read
  *tab->ref().null_ref_key = 1;             // Set null byte
  return safe_index_read(tab);              // then read null keys
}

/**
  Pick the appropriate access method functions

  Sets the functions for the selected table access method

  @param      join_tab             JOIN_TAB for this QEP_TAB

  @todo join_init_read_record/join_read_(last|first) set
  tab->read_record.read_record internally. Do the same in other first record
  reading functions.
*/

void QEP_TAB::pick_table_access_method(const JOIN_TAB *join_tab) {
  ASSERT_BEST_REF_IN_JOIN_ORDER(join());
  DBUG_ASSERT(join_tab == join()->best_ref[idx()]);
  DBUG_ASSERT(table());
  DBUG_ASSERT(read_first_record == NULL);
  // Only some access methods support reversed access:
  DBUG_ASSERT(!join_tab->reversed_access || type() == JT_REF ||
              type() == JT_INDEX_SCAN);
  // Fall through to set default access functions:
  switch (type()) {
    case JT_REF:
      if (join_tab->reversed_access) {
        read_first_record = join_read_last_key;
        read_record.read_record = join_read_prev_same;
      } else {
        read_first_record = join_read_always_key;
        read_record.read_record = join_read_next_same;
      }
      break;

    case JT_REF_OR_NULL:
      read_first_record = join_read_always_key_or_null;
      read_record.read_record = join_read_next_same_or_null;
      break;

    case JT_CONST:
      read_first_record = join_read_const;
      read_record.read_record = join_no_more_records;
      break;

    case JT_EQ_REF:
      read_first_record = join_read_key;
      read_record.read_record = join_no_more_records;
      read_record.unlock_row = join_read_key_unlock_row;
      break;

    case JT_FT:
      read_first_record = join_ft_read_first;
      read_record.read_record = join_ft_read_next;
      break;

    case JT_INDEX_SCAN:
      read_first_record =
          join_tab->reversed_access ? join_read_last : join_read_first;
      break;
    case JT_ALL:
    case JT_RANGE:
    case JT_INDEX_MERGE:
      read_first_record = (join_tab->use_quick == QS_DYNAMIC_RANGE)
                              ? join_init_quick_read_record
                              : join_init_read_record;
      break;
    default:
      DBUG_ASSERT(0);
      break;
  }
}

/**
  Install the appropriate 'linked' access method functions
  if this part of the join have been converted to pushed join.
*/

void QEP_TAB::set_pushed_table_access_method(void) {
  DBUG_ENTER("set_pushed_table_access_method");
  DBUG_ASSERT(table());

  /**
    Setup modified access function for children of pushed joins.
  */
  const TABLE *pushed_root = table()->file->root_of_pushed_join();
  if (pushed_root && pushed_root != table()) {
    /**
      Is child of a pushed join operation:
      Replace access functions with its linked counterpart.
      ... Which is effectively a NOOP as the row is already fetched
      together with the root of the linked operation.
     */
    DBUG_PRINT("info", ("Modifying table access method for '%s'",
                        table()->s->table_name.str));
    DBUG_ASSERT(type() != JT_REF_OR_NULL);
    read_first_record = join_read_linked_first;
    read_record.read_record = join_read_linked_next;
    // Use the default unlock_row function
    read_record.unlock_row = rr_unlock_row;
  }
  DBUG_VOID_RETURN;
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
  DBUG_ENTER("end_send");
  /*
    When all tables are const this function is called with jointab == NULL.
    This function shouldn't be called for the first join_tab as it needs
    to get fields from previous tab.

    Note that qep_tab may be one past the last of qep_tab! So don't read its
    pointed content. But you can read qep_tab[-1] then.
  */
  DBUG_ASSERT(qep_tab == NULL || qep_tab > join->qep_tab);

  if (!end_of_records) {
    int error;
    int sliceno;
    if (qep_tab) {
      if (qep_tab - 1 == join->before_ref_item_slice_tmp3) {
        // Read Items from pseudo-table REF_SLICE_TMP3
        sliceno = REF_SLICE_TMP3;
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
      if (copy_fields(&join->tmp_table_param, join->thd))
        DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
    }
    // Filter HAVING if not done earlier
    if (!having_is_true(join->having_cond))
      DBUG_RETURN(NESTED_LOOP_OK);  // Didn't match having
    error = 0;
    if (join->do_send_rows)
      error = join->select_lex->query_result()->send_data(*fields);
    if (error) DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */

    ++join->send_records;
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
        DBUG_RETURN(NESTED_LOOP_QUERY_LIMIT);
      }
    }
    if (join->send_records >= join->unit->select_limit_cnt &&
        join->do_send_rows) {
      if (join->calc_found_rows) {
        QEP_TAB *first = &join->qep_tab[0];
        if ((join->primary_tables == 1) && !join->sort_and_group &&
            !join->send_group_parts && !join->having_cond &&
            !first->condition() && !(first->quick()) &&
            (first->table()->file->ha_table_flags() &
             HA_STATS_RECORDS_IS_EXACT) &&
            (first->ref().key < 0)) {
          /* Join over all rows in table;  Return number of found rows */
          TABLE *table = first->table();

          if (table->unique_result.has_result()) {
            join->send_records = table->unique_result.found_records;
          }
          if (table->sort_result.has_result()) {
            /* Using filesort */
            join->send_records = table->sort_result.found_records;
          } else {
            table->file->info(HA_STATUS_VARIABLE);
            join->send_records = table->file->stats.records;
          }
        } else {
          join->do_send_rows = 0;
          if (join->unit->fake_select_lex)
            join->unit->fake_select_lex->select_limit = 0;
          DBUG_RETURN(NESTED_LOOP_OK);
        }
      }
      DBUG_RETURN(NESTED_LOOP_QUERY_LIMIT);  // Abort nicely
    } else if (join->send_records >= join->fetch_limit) {
      /*
        There is a server side cursor and all rows for
        this fetch request are sent.
      */
      DBUG_RETURN(NESTED_LOOP_CURSOR_LIMIT);
    }
  }
  DBUG_RETURN(NESTED_LOOP_OK);
}

/* ARGSUSED */
enum_nested_loop_state end_send_group(JOIN *join, QEP_TAB *qep_tab,
                                      bool end_of_records) {
  int idx = -1;
  enum_nested_loop_state ok_code = NESTED_LOOP_OK;
  DBUG_ENTER("end_send_group");

  List<Item> *fields;
  if (qep_tab) {
    DBUG_ASSERT(qep_tab - 1 == join->before_ref_item_slice_tmp3);
    fields = &join->tmp_fields_list[REF_SLICE_TMP3];
  } else
    fields = join->fields;

  /*
    (1) Haven't seen a first row yet
    (2) Have seen all rows
    (3) GROUP expression are different from previous row's
  */
  if (!join->first_record ||                                        // (1)
      end_of_records ||                                             // (2)
      (idx = test_if_item_cache_changed(join->group_fields)) >= 0)  // (3)
  {
    if (!join->group_sent &&
        (join->first_record ||
         (end_of_records && !join->grouped && !join->group_optimized_away))) {
      if (idx < (int)join->send_group_parts) {
        /*
          As GROUP expressions have changed, we now send forward the group
          of the previous row.
          While end_write_group() has a real tmp table as output,
          end_send_group() has a pseudo-table, made of a list of Item_copy
          items (created by setup_copy_fields()) which are accessible through
          REF_SLICE_TMP3. This is equivalent to one row where the current
          group is accumulated. The creation of a new group in the
          pseudo-table happens in this function (call to
          init_sum_functions()); the update of an existing group also happens
          in this function (call to update_sum_func()); the reading of an
          existing group happens right below.
          As we are now reading from pseudo-table REF_SLICE_TMP3, we switch to
          this slice; we should not have switched when calculating group
          expressions in test_if_item_cache_changed() above; indeed these
          group expressions need the current row of the input table, not what
          is in this slice (which is generally the last completed group so is
          based on some previous row of the input table).
        */
        Switch_ref_item_slice slice_switch(join, REF_SLICE_TMP3);
        DBUG_ASSERT(fields == join->get_current_fields());
        int error = 0;
        {
          table_map save_nullinfo = 0;
          if (!join->first_record) {
            // Calculate aggregate functions for no rows
            List_iterator_fast<Item> it(*fields);
            Item *item;

            while ((item = it++)) item->no_rows_in_result();

            /*
              Mark tables as containing only NULL values for processing
              the HAVING clause and for send_data().
              Calculate a set of tables for which NULL values need to
              be restored after sending data.
            */
            if (join->clear_fields(&save_nullinfo))
              DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
          }
          if (!having_is_true(join->having_cond))
            error = -1;  // Didn't satisfy having
          else {
            if (join->do_send_rows)
              error = join->select_lex->query_result()->send_data(*fields);
            join->send_records++;
            join->group_sent = true;
          }
          if (join->rollup.state != ROLLUP::STATE_NONE && error <= 0) {
            if (join->rollup_send_data((uint)(idx + 1))) error = 1;
          }
          // Restore NULL values if needed.
          if (save_nullinfo) join->restore_fields(save_nullinfo);
        }
        if (error > 0) DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
        if (end_of_records) DBUG_RETURN(NESTED_LOOP_OK);
        if (join->send_records >= join->unit->select_limit_cnt &&
            join->do_send_rows) {
          if (!join->calc_found_rows)
            DBUG_RETURN(NESTED_LOOP_QUERY_LIMIT);  // Abort nicely
          join->do_send_rows = 0;
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
      if (end_of_records) DBUG_RETURN(NESTED_LOOP_OK);
      join->first_record = 1;
      // Initialize the cache of GROUP expressions with this 1st row's values
      (void)(test_if_item_cache_changed(join->group_fields));
    }
    if (idx < (int)join->send_group_parts) {
      /*
        This branch is executed also for cursors which have finished their
        fetch limit - the reason for ok_code.

        As GROUP expressions have changed, initialize the new group:
        (1) copy non-aggregated expressions (they're constant over the group)
        (2) and reset group aggregate functions.

        About (1): some expressions to copy are not Item_fields and they are
        copied by copy_fields() which evaluates them (see param->copy_funcs,
        set up in setup_copy_fields()).
        Thus, copy_fields() can evaluate functions. One of them, F2, may
        reference another one F1, example:
        SELECT expr AS F1 ... GROUP BY ... HAVING F2(F1)<=2 .
        Assume F1 and F2 are not aggregate functions.
        Then they are calculated by copy_fields() when starting a new group,
        i.e. here.
        As F2 uses an alias to F1, F1 is calculated first;
        F2 must use that value (not evaluate expr again, as expr may not be
        deterministic), so F2 uses a reference (Item_ref) to the
        already-computed value of F1; that value is in Item_copy part of
        REF_SLICE_TMP3. So, we switch to that slice.
      */
      Switch_ref_item_slice slice_switch(join, REF_SLICE_TMP3);
      if (copy_fields(&join->tmp_table_param, join->thd))  // (1)
        DBUG_RETURN(NESTED_LOOP_ERROR);
      if (init_sum_functions(join->sum_funcs,
                             join->sum_funcs_end[idx + 1]))  //(2)
        DBUG_RETURN(NESTED_LOOP_ERROR);
      join->group_sent = false;
      DBUG_RETURN(ok_code);
    }
  }
  if (update_sum_func(join->sum_funcs)) DBUG_RETURN(NESTED_LOOP_ERROR);
  DBUG_RETURN(NESTED_LOOP_OK);
}

static bool cmp_field_value(Field *field, my_ptrdiff_t diff) {
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
  DBUG_ENTER("group_rec_cmp");
  my_ptrdiff_t diff = rec1 - rec0;

  for (ORDER *grp = group; grp; grp = grp->next) {
    Field *field = grp->field_in_tmp_table;
    if (cmp_field_value(field, diff)) DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}

/**
  Compare GROUP BY in from tmp table's record[0] and record[1]

  @returns
    true  records are different
    false records are the same
*/

static bool table_rec_cmp(TABLE *table) {
  DBUG_ENTER("table_rec_cmp");
  my_ptrdiff_t diff = table->record[1] - table->record[0];
  Field **fields = table->visible_field_ptr();

  for (uint i = 0; i < table->visible_field_count(); i++) {
    Field *field = fields[i];
    if (cmp_field_value(field, diff)) DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}

/**
  Generate hash for a field

  @returns generated hash
*/

ulonglong unique_hash(Field *field, ulonglong *hash_val) {
  uchar *pos, *end;
  ulong seed1 = 0, seed2 = 4;
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

  field->get_ptr(&pos);
  end = pos + field->data_length();

  if (field->type() == MYSQL_TYPE_JSON) {
    Field_json *json_field = down_cast<Field_json *>(field);

    crc = json_field->make_hash_key(hash_val);
  } else if (field->key_type() == HA_KEYTYPE_TEXT ||
             field->key_type() == HA_KEYTYPE_VARTEXT1 ||
             field->key_type() == HA_KEYTYPE_VARTEXT2) {
    field->charset()->coll->hash_sort(field->charset(), (const uchar *)pos,
                                      field->data_length(), &seed1, &seed2);
    crc ^= seed1;
  } else
    while (pos != end)
      crc = ((crc << 8) + (((uchar) * (uchar *)pos++))) +
            (crc >> (8 * sizeof(ha_checksum) - 8));
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
  DBUG_ENTER("unique_hash_group");
  ulonglong crc = 0;

  for (ORDER *ord = group; ord; ord = ord->next) {
    Field *field = ord->field_in_tmp_table;
    DBUG_ASSERT(field);
    unique_hash(field, &crc);
  }

  DBUG_RETURN(crc);
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
    if (!(table->is_distinct ? table_rec_cmp(table)
                             : group_rec_cmp(table->group, table->record[0],
                                             table->record[1])))
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
    (void)it.func()->walk(&Item::reset_wf_state,
                          Item::enum_walk(Item::WALK_POSTFIX),
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
  Dirty trick to be able to copy fields *back* from the frame buffer tmp table
  to the input table's buffer, cf. #bring_back_frame_row.

  @param param  represents the frame buffer tmp file
*/
static void swap_copy_field_direction(const Temp_table_param *param) {
  Copy_field *ptr = param->copy_field;
  Copy_field *end = param->copy_field_end;

  for (; ptr < end; ptr++) ptr->swap_direction();
}

/**
  Save a window frame buffer to frame buffer temporary table.

  @param thd      The current thread
  @param w        The current window
  @param rowno    The rowno in the current partition (1-based)
*/
static bool buffer_record_somewhere(THD *thd, Window *w, int64 rowno) {
  DBUG_ENTER("buffer_record_somewhere");
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
      DBUG_RETURN(true);
    }
  }

  int error = t->file->ha_write_row(record);
  w->set_frame_buffer_total_rows(w->frame_buffer_total_rows() + 1);

  if (error) {
    /* If this is a duplicate error, return immediately */
    if (t->file->is_ignorable_error(error)) DBUG_RETURN(1);

    /* Other error than duplicate error: Attempt to create a temporary table. */
    bool is_duplicate;
    if (create_ondisk_from_heap(thd, t, w->frame_buffer_param()->start_recinfo,
                                &w->frame_buffer_param()->recinfo, error, true,
                                &is_duplicate))
      DBUG_RETURN(-1);

    DBUG_ASSERT(t->s->db_type() == innodb_hton);
    if (t->file->ha_rnd_init(true)) return true; /* purecov: inspected */

    /*
      Reset all hints since they all pertain to the in-memory file, not the
      new on-disk one.
    */
    for (uint i = Window::REA_FIRST_IN_PARTITION;
         i < Window::FRAME_BUFFER_POSITIONS_CARD +
                 w->opt_nth_row().m_offsets.size() +
                 w->opt_lead_lag().m_offsets.size();
         i++) {
      void *r = sql_alloc(t->file->ref_length);
      if (r == nullptr) DBUG_RETURN(true);
      w->m_frame_buffer_positions[i].m_position = static_cast<uchar *>(r);
      w->m_frame_buffer_positions[i].m_rowno = -1;
    }

    if ((w->m_tmp_pos.m_position = (uchar *)sql_alloc(t->file->ref_length)) ==
        nullptr)
      DBUG_RETURN(true);

    w->m_frame_buffer_positions[Window::REA_FIRST_IN_PARTITION].m_rowno = 1;
    /*
      The auto-generated primary key of the first row is 1. Our offset is
      also one-based, so we can use w->frame_buffer_partition_offset() "as is"
      to construct the position.
    */
    encode_innodb_position(
        w->m_frame_buffer_positions[Window::REA_FIRST_IN_PARTITION].m_position,
        t->file->ref_length, w->frame_buffer_partition_offset());

    DBUG_RETURN(is_duplicate ? 1 : 0);
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
        void *r = sql_alloc(t->file->ref_length);
        if (r == nullptr) DBUG_RETURN(true);
        Window::Frame_buffer_position p(static_cast<uchar *>(r), -1);
        w->m_frame_buffer_positions.push_back(p);
      }

      if ((w->m_tmp_pos.m_position = (uchar *)sql_alloc(t->file->ref_length)) ==
          nullptr)
        DBUG_RETURN(true);
    }

    // Do a read to establish scan position, then get it
    error = t->file->ha_rnd_next(record);
    t->file->position(record);
    std::memcpy(
        w->m_frame_buffer_positions[Window::REA_FIRST_IN_PARTITION].m_position,
        t->file->ref, t->file->ref_length);
    w->m_frame_buffer_positions[Window::REA_FIRST_IN_PARTITION].m_rowno = 1;
    w->set_frame_buffer_partition_offset(w->frame_buffer_total_rows());
  }

  DBUG_RETURN(false);
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
static bool buffer_windowing_record(THD *thd, Temp_table_param *param,
                                    bool *new_partition) {
  DBUG_ENTER("buffer_windowing_record");
  Window *w = param->m_window;

  if (copy_fields(w->frame_buffer_param(), thd)) DBUG_RETURN(true);

  if (new_partition != nullptr) {
    const bool first_partition = w->partition_rowno() == 0;
    w->check_partition_boundary();

    if (!first_partition && w->partition_rowno() == 1) {
      *new_partition = true;
      w->save_special_record(Window::FBC_FIRST_IN_NEXT_PARTITION,
                             w->frame_buffer());
      DBUG_RETURN(false);
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

  if (buffer_record_somewhere(thd, w, w->partition_rowno())) DBUG_RETURN(true);

  w->set_last_rowno_in_cache(w->partition_rowno());

  DBUG_RETURN(false);
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
  Copy_field *ptr = param->copy_field;
  Copy_field *const end = param->copy_field_end;

  while (ptr < end) {
    TABLE *const t = ptr->from_field()->table;
    if (t != nullptr) {
      auto it = map.find(t);
      if (it == map.end())
        map.insert(it, std::pair<TABLE *, my_bitmap_map *>(
                           t, dbug_tmp_use_all_columns(t, t->write_set)));
    }
    ptr++;
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
static bool bring_back_frame_row(THD *thd, Window &w,
                                 Temp_table_param *out_param, int64 rowno,
                                 enum Window::retrieve_cached_row_reason reason,
                                 int fno = 0) {
  DBUG_ENTER("bring_back_frame_row");
  DBUG_PRINT("enter", ("rowno: %lld reason: %d fno: %d", rowno, reason, fno));
  DBUG_ASSERT(reason == Window::REA_MISC_POSITIONS || fno == 0);

  uchar *fb_rec = w.frame_buffer()->record[0];

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
    w.restore_special_record(rowno, fb_rec);
  } else if (rowno == Window::FBC_LAST_BUFFERED_ROW) {
    do_fetch = w.row_has_fields_in_out_table() != w.last_rowno_in_cache();
    if (do_fetch) w.restore_special_record(rowno, fb_rec);
  } else {
    DBUG_ASSERT(reason != Window::REA_WONT_UPDATE_HINT);
    do_fetch = w.row_has_fields_in_out_table() != rowno;

    if (do_fetch &&
        read_frame_buffer_row(rowno, &w, reason == Window::REA_MISC_POSITIONS))
      DBUG_RETURN(true);

    /* Got row rowno in record[0], remember position */
    const TABLE *const t = w.frame_buffer();
    t->file->position(fb_rec);
    std::memcpy(w.m_frame_buffer_positions[reason + fno].m_position,
                t->file->ref, t->file->ref_length);
    w.m_frame_buffer_positions[reason + fno].m_rowno = rowno;
  }

  w.set_diagnostics_rowno(thd->get_stmt_da(), rowno);

  if (!do_fetch) DBUG_RETURN(false);

  Temp_table_param *const fb_info = w.frame_buffer_param();

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
  swap_copy_field_direction(fb_info);

  bool rc = copy_fields(fb_info, thd);

  swap_copy_field_direction(fb_info);  // reset original direction

#if !defined(DBUG_OFF)
  dbug_restore_all_columns(saved_map);
#endif

  if (!rc) {
    if (out_param) {
      if (copy_fields(out_param, thd)) DBUG_RETURN(true);
      // fields are in IN and in OUT
      if (rowno >= 1) w.set_row_has_fields_in_out_table(rowno);
    } else
      // we only wrote IN record, so OUT and IN are inconsistent
      w.set_row_has_fields_in_out_table(0);
  }

  DBUG_RETURN(rc);
}

/**
  Save row special_rowno in table t->record[0] to an in-memory copy for later
  restoration.
*/
void Window::save_special_record(uint64 special_rowno, TABLE *t) {
  DBUG_PRINT("info", ("save_special_record: %llu", special_rowno));
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
  DBUG_PRINT("info", ("restore_special_record: %llu", special_rowno));
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
bool process_wfs_needing_card(
    THD *thd, Temp_table_param *param, const Window::st_nth &have_nth_value,
    const Window::st_lead_lag &have_lead_lag, const int64 current_row,
    Window &w, enum Window::retrieve_cached_row_reason current_row_reason) {
  w.set_rowno_being_visited(current_row);

  // Reset state for LEAD/LAG functions
  if (!have_lead_lag.m_offsets.empty()) w.reset_lead_lag();

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
      w.set_rowno_being_visited(rowno_to_visit);

      if (rowno_to_visit >= 1 && rowno_to_visit <= w.last_rowno_in_cache()) {
        if (bring_back_frame_row(thd, w, param, rowno_to_visit,
                                 Window::REA_MISC_POSITIONS, nths + fno++))
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
static bool process_buffered_windowing_record(THD *thd, Temp_table_param *param,
                                              const bool new_partition_or_eof,
                                              bool *output_row_ready) {
  DBUG_ENTER("process_buffered_windowing_record");
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
    DBUG_RETURN(false);

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

  DBUG_PRINT("enter", ("current_row: %lld, new_partition_or_eof: %d",
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
    int sign = 1;
    /* Determine lower border */
    switch (f->m_from->m_border_type) {
      case WBT_CURRENT_ROW:
        lower_limit = current_row;
        break;
      case WBT_VALUE_FOLLOWING:
        sign = -1;
        /* fall through */
      case WBT_VALUE_PRECEDING:
        /*
          Example: 1 PRECEDING and current row== 2 => 1
                                   current row== 1 => 1
                                   current row== 3 => 2
        */
        lower_limit =
            max(current_row - f->m_from->border()->val_int() * sign, 1ll);
        break;
      case WBT_UNBOUNDED_PRECEDING:
        lower_limit = 1;
        break;
      case WBT_UNBOUNDED_FOLLOWING:
        DBUG_ASSERT(false);
        break;
    }

    /* Determine upper border */
    {
      int64 sign = 1;
      switch (f->m_to->m_border_type) {
        case WBT_CURRENT_ROW:
          // we always have enough cache
          upper_limit = current_row;
          break;
        case WBT_VALUE_PRECEDING:
          sign = -1;
          /* fall through */
        case WBT_VALUE_FOLLOWING:
          upper_limit = current_row + f->m_to->border()->val_int() * sign;
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
    DBUG_RETURN(false);  // We haven't read enough rows yet, so return

  w.set_rowno_in_partition(current_row);
  w.set_diagnostics_rowno(thd->get_stmt_da(), current_row);

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
    if (bring_back_frame_row(thd, w, param, current_row, Window::REA_CURRENT))
      DBUG_RETURN(true);

    if (current_row == 1)  // new partition
      reset_non_framing_wf_state(param->items_to_copy);
    if (!optimizable || current_row == 1)  // new frame
    {
      reset_framing_wf_states(param->items_to_copy);
    }  // else we remember state and update it for row 2..N

    /* E.g. ROW_NUMBER, RANK, DENSE_RANK */
    if (copy_funcs(param, thd, CFT_WF_NON_FRAMING)) DBUG_RETURN(true);
    if (!optimizable || current_row == 1) {
      /*
        So far frame is empty; set up a flag which makes framing WFs set
        themselves to NULL in OUT.
      */
      w.set_do_copy_null(true);
      if (copy_funcs(param, thd, CFT_WF_FRAMING)) DBUG_RETURN(true);
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

      const Window::retrieve_cached_row_reason reason =
          (n == 1 ? Window::REA_FIRST_IN_FRAME : Window::REA_LAST_IN_FRAME);
      /*
        Hint maintenance: we will normally read past last row in frame, so
        prepare to resurrect that hint once we do.
      */
      w.save_pos(reason);

      /* Set up the non-wf fields for aggregating to the output row. */
      if (bring_back_frame_row(thd, w, param, rowno, reason)) DBUG_RETURN(true);

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
      if (copy_funcs(param, thd, CFT_WF_FRAMING)) DBUG_RETURN(true);

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
    if (bring_back_frame_row(thd, w, param, current_row, Window::REA_CURRENT))
      DBUG_RETURN(true);

    /* E.g. ROW_NUMBER, RANK, DENSE_RANK */
    if (copy_funcs(param, thd, CFT_WF_NON_FRAMING)) DBUG_RETURN(true);
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
      if (bring_back_frame_row(thd, w, param, lower_limit - 1,
                               Window::REA_FIRST_IN_FRAME))
        DBUG_RETURN(true);

      w.set_inverse(true);
      if (!new_last_row) {
        w.set_rowno_in_frame(rn_in_frame);
        if (rn_in_frame > 0)
          w.set_is_last_row_in_frame(true);  // do final comp., e.g. div in AVG

        if (copy_funcs(param, thd, CFT_WF_FRAMING)) DBUG_RETURN(true);

        w.set_is_last_row_in_frame(false);  // undo temporary states
      } else {
        if (copy_funcs(param, thd, CFT_WF_FRAMING)) DBUG_RETURN(true);
      }

      w.set_inverse(false);
    }

    if (have_first_value && (lower_limit <= last_rowno_in_cache)) {
      // We have seen first row of frame, FIRST_VALUE can be computed:
      if (bring_back_frame_row(thd, w, param, lower_limit,
                               Window::REA_FIRST_IN_FRAME))
        DBUG_RETURN(true);

      w.set_rowno_in_frame(1);

      /*
        Framing WFs which accumulate (SUM, COUNT, AVG) shouldn't accumulate
        this row again as they have done so already. Evaluate only
        X_VALUE/MIN/MAX.
      */
      if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW)) DBUG_RETURN(true);
    }

    if (have_last_value && !new_last_row) {
      // We have seen last row of frame, LAST_VALUE can be computed:
      if (bring_back_frame_row(thd, w, param, upper, Window::REA_LAST_IN_FRAME))
        DBUG_RETURN(true);

      w.set_rowno_in_frame(rn_in_frame);

      if (rn_in_frame > 0) w.set_is_last_row_in_frame(true);

      if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW)) DBUG_RETURN(true);

      w.set_is_last_row_in_frame(false);
    }

    if (!have_nth_value.m_offsets.empty()) {
      int fno = 0;
      for (auto nth : have_nth_value.m_offsets) {
        if (lower_limit + nth.m_rowno - 1 <= upper) {
          if (bring_back_frame_row(thd, w, param, lower_limit + nth.m_rowno - 1,
                                   Window::REA_MISC_POSITIONS, fno++))
            DBUG_RETURN(true);

          w.set_rowno_in_frame(nth.m_rowno);

          if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW))
            DBUG_RETURN(true);
        }
      }
    }

    if (new_last_row)  // Add new last row to framing WF's value
    {
      if (bring_back_frame_row(thd, w, param, upper, Window::REA_LAST_IN_FRAME))
        DBUG_RETURN(true);

      w.set_rowno_in_frame(upper - lower_limit + 1)
          .set_is_last_row_in_frame(true);  // temporary states for next copy
      w.set_rowno_being_visited(upper);

      if (copy_funcs(param, thd, CFT_WF_FRAMING)) DBUG_RETURN(true);

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
          if (bring_back_frame_row(thd, w, param, rowno,
                                   Window::REA_FIRST_IN_FRAME))
            DBUG_RETURN(true);

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

            if (copy_funcs(param, thd, CFT_WF_FRAMING)) DBUG_RETURN(true);

            w.set_inverse(false).set_is_last_row_in_frame(false);
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

          if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW))
            DBUG_RETURN(true);
          w.set_is_last_row_in_frame(false);

          if (have_last_value && w.last_rowno_in_range_frame() > rowno) {
            /* Set up the non-wf fields for aggregating to the output row. */
            if (bring_back_frame_row(thd, w, param,
                                     w.last_rowno_in_range_frame(),
                                     Window::REA_LAST_IN_FRAME))
              DBUG_RETURN(true);

            w.set_rowno_in_frame(w.last_rowno_in_range_frame() -
                                 w.first_rowno_in_range_frame() + 1)
                .set_is_last_row_in_frame(true);
            w.set_rowno_being_visited(w.last_rowno_in_range_frame());
            if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW))
              DBUG_RETURN(true);
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
        w.save_pos(Window::REA_LAST_IN_FRAME);
        if (bring_back_frame_row(thd, w, param, rowno,
                                 Window::REA_LAST_IN_FRAME))
          DBUG_RETURN(true);

        if (w.before_frame()) {
          if (!found_first) new_first_rowno_in_frame++;
          continue;
        } else if (w.after_frame()) {
          w.set_last_rowno_in_range_frame(rowno - 1);
          if (!found_first) w.set_first_rowno_in_range_frame(rowno);
          if (!range_to_current_row) {
            /*
              If range_to_current_row, this first row out of frame will be
              necessary soon, so keep its position.
            */
            w.restore_pos(Window::REA_LAST_IN_FRAME);
          }
          break;
        }  // else: row is within range, process

        const int64 rowno_in_frame = rowno - new_first_rowno_in_frame + 1;

        if (rowno_in_frame == 1 && !found_first) {
          found_first = true;
          w.set_first_rowno_in_range_frame(rowno);
          // Found the first row in this range frame. Make a note in the hint.
          w.copy_pos(Window::REA_LAST_IN_FRAME, Window::REA_FIRST_IN_FRAME);
        }
        w.set_rowno_in_frame(rowno_in_frame)
            .set_is_last_row_in_frame(true);  // pessimistic assumption
        w.set_rowno_being_visited(rowno);

        if (copy_funcs(param, thd, CFT_WF_FRAMING)) DBUG_RETURN(true);

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
            if (bring_back_frame_row(thd, w, param, row_to_get,
                                     Window::REA_MISC_POSITIONS, fno++))
              DBUG_RETURN(true);

            w.set_rowno_in_frame(nth.m_rowno);

            if (copy_funcs(param, thd, CFT_WF_USES_ONLY_ONE_ROW))
              DBUG_RETURN(true);
          }
        }
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
        if (bring_back_frame_row(thd, w, param, rowno,
                                 Window::REA_LAST_IN_PEERSET))
          DBUG_RETURN(true);

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

  if (bring_back_frame_row(thd, w, param, current_row, Window::REA_CURRENT))
    DBUG_RETURN(true);

  /* NTILE and other non-framing wfs */
  if (w.needs_card()) {
    /* Set up the non-wf fields for aggregating to the output row. */
    if (process_wfs_needing_card(thd, param, have_nth_value, have_lead_lag,
                                 current_row, w, Window::REA_CURRENT))
      DBUG_RETURN(true);
  }

  *output_row_ready = true;
  w.set_last_row_output(current_row);
  DBUG_PRINT("info", ("sent row: %lld", current_row));

  DBUG_RETURN(false);
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
    if (create_ondisk_from_heap(join->thd, table, out_tbl->start_recinfo,
                                &out_tbl->recinfo, error, true, NULL) ||
        (table->hash_field && table->file->ha_index_init(0, 0)))
      return NESTED_LOOP_ERROR;  // Not a table_is_full error
  }

  if (++qep_tab->send_records >= out_tbl->end_write_records &&
      join->do_send_rows) {
    if (!join->calc_found_rows) return NESTED_LOOP_QUERY_LIMIT;
    join->do_send_rows = 0;
    join->unit->select_limit_cnt = HA_POS_ERROR;
    return NESTED_LOOP_OK;
  }

  return NESTED_LOOP_OK;
}

/* ARGSUSED */
static enum_nested_loop_state end_write(JOIN *join, QEP_TAB *const qep_tab,
                                        bool end_of_records) {
  DBUG_ENTER("end_write");

  TABLE *const table = qep_tab->table();

  if (join->thd->killed)  // Aborted by user
  {
    join->thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED); /* purecov: inspected */
  }
  if (!end_of_records) {
    Temp_table_param *const tmp_tbl = qep_tab->tmp_table_param;
    Switch_ref_item_slice slice_switch(join, qep_tab->ref_item_slice);
    DBUG_ASSERT(qep_tab - 1 != join->before_ref_item_slice_tmp3);

    if (copy_fields(tmp_tbl, join->thd))
      DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
    if (copy_funcs(tmp_tbl, join->thd))
      DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */

    if (having_is_true(qep_tab->having)) {
      int error;
      join->found_records++;

      if (!check_unique_constraint(table)) goto end;  // skip it

      if ((error = table->file->ha_write_row(table->record[0]))) {
        if (table->file->is_ignorable_error(error)) goto end;
        if (create_ondisk_from_heap(join->thd, table, tmp_tbl->start_recinfo,
                                    &tmp_tbl->recinfo, error, true, NULL))
          DBUG_RETURN(NESTED_LOOP_ERROR);  // Not a table_is_full error
      }
      if (++qep_tab->send_records >= tmp_tbl->end_write_records &&
          join->do_send_rows) {
        if (!join->calc_found_rows) DBUG_RETURN(NESTED_LOOP_QUERY_LIMIT);
        join->do_send_rows = 0;
        join->unit->select_limit_cnt = HA_POS_ERROR;
        DBUG_RETURN(NESTED_LOOP_OK);
      }
    }
  }
end:
  DBUG_RETURN(NESTED_LOOP_OK);
}

/* ARGSUSED */

/**
  Similar to end_write, but used in the windowing tmp table steps
*/
static enum_nested_loop_state end_write_wf(JOIN *join, QEP_TAB *const qep_tab,
                                           bool end_of_records) {
  DBUG_ENTER("end_write_wf");
  THD *const thd = join->thd;

  if (thd->killed)  // Aborted by user
  {
    thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED); /* purecov: inspected */
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

  if (end_of_records && !window_buffering) DBUG_RETURN(NESTED_LOOP_OK);

  /*
    All evaluations of functions, done in process_buffered_windowing_record()
    and copy_funcs(), are using values of the out table, so we must use its
    slice:
  */
  Switch_ref_item_slice slice_switch(join, qep_tab->ref_item_slice);
  DBUG_ASSERT(qep_tab - 1 != join->before_ref_item_slice_tmp3 &&
              qep_tab != join->before_ref_item_slice_tmp3);

  TABLE *const table = qep_tab->table();
  if (window_buffering) {
    bool new_partition = false;
    if (!end_of_records) {
      if (copy_fields(out_tbl, thd))
        DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */

      /*
        This saves the values of non-WF functions for the row. For example,
        1+t.a. But also 1+LEAD. Even though at this point we lack data to
        compute LEAD; the saved value is thus incorrect; later, when the row
        is fully computable, we will re-evaluate the CFT_NON_WF to get a
        correct value for 1+LEAD.
      */
      if (copy_funcs(out_tbl, thd, CFT_NON_WF))
        DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */

      if (!having_is_true(qep_tab->having))
        goto end;  // Didn't match having, skip it

      if (buffer_windowing_record(thd, out_tbl, &new_partition))
        DBUG_RETURN(NESTED_LOOP_ERROR);

      join->found_records++;
    }

  repeat:
    while (true) {
      bool output_row_ready = false;
      if (process_buffered_windowing_record(
              thd, out_tbl, new_partition || end_of_records, &output_row_ready))
        DBUG_RETURN(NESTED_LOOP_ERROR);

      if (!output_row_ready) break;

      if (!check_unique_constraint(table))  // In case of SELECT DISTINCT
        continue;                           // skip it

      enum_nested_loop_state result;
      if ((result = write_or_send_row(join, qep_tab, table, out_tbl)))
        DBUG_RETURN(result);  // Not a table_is_full error

      if (thd->killed)  // Aborted by user
      {
        thd->send_kill_message();
        DBUG_RETURN(NESTED_LOOP_KILLED);
      }
    }

    if (new_partition) {
      /*
        We didn't really buffer this row yet since, we found a partition
        change so we had to finalize the previous partition first.
        Bring back saved row for next partition.
      */
      if (bring_back_frame_row(thd, *win, out_tbl,
                               Window::FBC_FIRST_IN_NEXT_PARTITION,
                               Window::REA_WONT_UPDATE_HINT))
        DBUG_RETURN(NESTED_LOOP_ERROR);

      /*
        copy_funcs(CFT_NON_WF) is not necessary: a non-WF function was
        calculated and saved in OUT, then this OUT column was copied to
        special record, then restored to OUT column.
      */

      win->reset_partition_state();
      if (buffer_windowing_record(thd, out_tbl,
                                  nullptr /* first in new partition */))
        DBUG_RETURN(NESTED_LOOP_ERROR);
      new_partition = false;
      goto repeat;
    }
    if (!end_of_records && win->needs_restore_input_row()) {
      /*
        Reestablish last row read from input table in case it is needed again
        before reading a new row. May be necessary if this is the first window
        following after a join, cf. the caching presumption in join_read_key.
        This logic can be removed if we move to copying between out
        tmp record and frame buffer record, instead of involving the in
        record. FIXME.
      */
      if (bring_back_frame_row(thd, *win, nullptr /* no copy to OUT */,
                               Window::FBC_LAST_BUFFERED_ROW,
                               Window::REA_WONT_UPDATE_HINT))
        DBUG_RETURN(NESTED_LOOP_ERROR);
    }
  } else {
    if (copy_fields(out_tbl, thd))
      DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */

    if (copy_funcs(out_tbl, thd, CFT_NON_WF))
      DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */

    if (!having_is_true(qep_tab->having))
      goto end;  // Didn't match having, skip it

    win->check_partition_boundary();

    if (copy_funcs(out_tbl, thd, CFT_WF))
      DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */

    join->found_records++;

    if (!check_unique_constraint(table))  // In case of SELECT DISTINCT
      goto end;                           // skip it

    DBUG_PRINT("info", ("end_write: writing record at %p", table->record[0]));

    enum_nested_loop_state result;
    if ((result = write_or_send_row(join, qep_tab, table, out_tbl)))
      DBUG_RETURN(result);  // Not a table_is_full error
  }
end:
  DBUG_RETURN(NESTED_LOOP_OK);
}

/* ARGSUSED */
/** Group by searching after group record and updating it if possible. */

static enum_nested_loop_state end_update(JOIN *join, QEP_TAB *const qep_tab,
                                         bool end_of_records) {
  TABLE *const table = qep_tab->table();
  ORDER *group;
  int error;
  bool group_found = false;
  DBUG_ENTER("end_update");

  if (end_of_records) DBUG_RETURN(NESTED_LOOP_OK);
  if (join->thd->killed)  // Aborted by user
  {
    join->thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED); /* purecov: inspected */
  }

  Temp_table_param *const tmp_tbl = qep_tab->tmp_table_param;
  join->found_records++;

  DBUG_ASSERT(tmp_tbl->copy_funcs.elements == 0);  // See comment below.

  if (copy_fields(tmp_tbl, join->thd))  // Groups are copied twice.
    DBUG_RETURN(NESTED_LOOP_ERROR);     /* purecov: inspected */

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
      DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
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
      if (error == HA_ERR_RECORD_IS_THE_SAME) DBUG_RETURN(NESTED_LOOP_OK);
      table->file->print_error(error, MYF(0)); /* purecov: inspected */
      DBUG_RETURN(NESTED_LOOP_ERROR);          /* purecov: inspected */
    }
    DBUG_RETURN(NESTED_LOOP_OK);
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

    The assertion on tmp_tbl->copy_funcs is to make sure copy_fields() doesn't
    suffer from the late switching.
  */
  Switch_ref_item_slice slice_switch(join, qep_tab->ref_item_slice);
  DBUG_ASSERT(qep_tab - 1 != join->before_ref_item_slice_tmp3 &&
              qep_tab != join->before_ref_item_slice_tmp3);

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
      DBUG_RETURN(NESTED_LOOP_ERROR); /* purecov: inspected */
  }
  init_tmptable_sum_functions(join->sum_funcs);
  if ((error = table->file->ha_write_row(table->record[0]))) {
    if (create_ondisk_from_heap(join->thd, table, tmp_tbl->start_recinfo,
                                &tmp_tbl->recinfo, error, false, NULL))
      DBUG_RETURN(NESTED_LOOP_ERROR);  // Not a table_is_full error
    /* Change method to update rows */
    if ((error = table->file->ha_index_init(0, 0))) {
      table->file->print_error(error, MYF(0));
      DBUG_RETURN(NESTED_LOOP_ERROR);
    }
  }
  qep_tab->send_records++;
  DBUG_RETURN(NESTED_LOOP_OK);
}

/* ARGSUSED */
enum_nested_loop_state end_write_group(JOIN *join, QEP_TAB *const qep_tab,
                                       bool end_of_records) {
  TABLE *table = qep_tab->table();
  int idx = -1;
  DBUG_ENTER("end_write_group");

  if (join->thd->killed) {  // Aborted by user
    join->thd->send_kill_message();
    DBUG_RETURN(NESTED_LOOP_KILLED); /* purecov: inspected */
  }
  if (!join->first_record || end_of_records ||
      (idx = test_if_item_cache_changed(join->group_fields)) >= 0) {
    Temp_table_param *const tmp_tbl = qep_tab->tmp_table_param;
    if (join->first_record || (end_of_records && !join->grouped)) {
      int send_group_parts = join->send_group_parts;
      if (idx < send_group_parts) {
        Switch_ref_item_slice slice_switch(join, qep_tab->ref_item_slice);
        DBUG_ASSERT(qep_tab - 1 != join->before_ref_item_slice_tmp3 &&
                    qep_tab != join->before_ref_item_slice_tmp3);
        table_map save_nullinfo = 0;
        if (!join->first_record) {
          // Calculate aggregate functions for no rows
          List_iterator_fast<Item> it(*join->get_current_fields());
          Item *item;
          while ((item = it++)) item->no_rows_in_result();

          /*
            Mark tables as containing only NULL values for ha_write_row().
            Calculate a set of tables for which NULL values need to
            be restored after sending data.
          */
          if (join->clear_fields(&save_nullinfo))
            DBUG_RETURN(NESTED_LOOP_ERROR);
        }
        copy_sum_funcs(join->sum_funcs, join->sum_funcs_end[send_group_parts]);
        if (having_is_true(qep_tab->having)) {
          int error = table->file->ha_write_row(table->record[0]);
          if (error &&
              create_ondisk_from_heap(join->thd, table, tmp_tbl->start_recinfo,
                                      &tmp_tbl->recinfo, error, false, NULL))
            DBUG_RETURN(NESTED_LOOP_ERROR);
        }
        if (join->rollup.state != ROLLUP::STATE_NONE) {
          if (join->rollup_write_data((uint)(idx + 1), table))
            DBUG_RETURN(NESTED_LOOP_ERROR);
        }
        // Restore NULL values if needed.
        if (save_nullinfo) join->restore_fields(save_nullinfo);

        if (end_of_records) DBUG_RETURN(NESTED_LOOP_OK);
      }
    } else {
      if (end_of_records) DBUG_RETURN(NESTED_LOOP_OK);
      join->first_record = 1;

      (void)(test_if_item_cache_changed(join->group_fields));
    }
    if (idx < (int)join->send_group_parts) {
      Switch_ref_item_slice slice_switch(join, qep_tab->ref_item_slice);
      if (copy_fields(tmp_tbl, join->thd)) DBUG_RETURN(NESTED_LOOP_ERROR);
      if (copy_funcs(tmp_tbl, join->thd)) DBUG_RETURN(NESTED_LOOP_ERROR);
      if (init_sum_functions(join->sum_funcs, join->sum_funcs_end[idx + 1]))
        DBUG_RETURN(NESTED_LOOP_ERROR);
      DBUG_RETURN(NESTED_LOOP_OK);
    }
  }
  if (update_sum_func(join->sum_funcs)) DBUG_RETURN(NESTED_LOOP_ERROR);
  DBUG_RETURN(NESTED_LOOP_OK);
}

/*
  If not selecting by given key, create an index how records should be read

  SYNOPSIS
   create_sort_index()
     thd		Thread handler
     join		Join with table to sort
     order		How table should be sorted
     filesort_limit	Max number of rows that needs to be sorted
     select_limit	Max number of rows in final output
                        Used to decide if we should use index or not
  IMPLEMENTATION
   - If there is an index that can be used, the first non-const join_tab in
     'join' is modified to use this index.
   - If no index, create with filesort() an index file that can be used to
     retrieve rows in order (should be done with 'read_record').
     The sorted data is stored in tab->table() and will be freed when calling
     free_io_cache(tab->table()).

  RETURN VALUES
    0		ok
    -1		Some fatal error
    1		No records
*/

static int create_sort_index(THD *thd, JOIN *join, QEP_TAB *tab) {
  ha_rows examined_rows, found_rows, returned_rows;
  TABLE *table;
  bool status;
  Filesort *fsort = tab->filesort;
  DBUG_ENTER("create_sort_index");

  // One row, no need to sort. make_tmp_tables_info should already handle this.
  DBUG_ASSERT(!join->plan_is_const() && fsort);
  table = tab->table();

  table->sort_result.io_cache =
      (IO_CACHE *)my_malloc(key_memory_TABLE_sort_io_cache, sizeof(IO_CACHE),
                            MYF(MY_WME | MY_ZEROFILL));

  // If table has a range, move it to select
  if (tab->quick() && tab->ref().key >= 0) {
    if (tab->type() != JT_REF_OR_NULL && tab->type() != JT_FT) {
      DBUG_ASSERT(tab->type() == JT_REF || tab->type() == JT_EQ_REF);
      // Update ref value
      if ((cp_buffer_from_ref(thd, table, &tab->ref()) && thd->is_fatal_error))
        goto err;  // out of memory
    }
  }

  /* Fill schema tables with data before filesort if it's necessary */
  if ((join->select_lex->active_options() & OPTION_SCHEMA_TABLE) &&
      get_schema_tables_result(join, PROCESSED_BY_CREATE_SORT_INDEX))
    goto err;

  if (table->s->tmp_table)
    table->file->info(HA_STATUS_VARIABLE);  // Get record count
  status = filesort(thd, fsort, tab->keep_current_rowid, &examined_rows,
                    &found_rows, &returned_rows);
  table->sort_result.found_records = returned_rows;
  tab->set_records(found_rows);  // For SQL_CALC_ROWS
  tab->join()->examined_rows += examined_rows;
  table->set_keyread(false);  // Restore if we used indexes
  if (tab->type() == JT_FT)
    table->file->ft_end();
  else
    table->file->ha_index_or_rnd_end();
  DBUG_RETURN(status);
err:
  DBUG_RETURN(-1);
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
    if ((*ptr)->cmp_offset(table->s->rec_buff_length)) return 1;
  }
  return 0;
}

static bool copy_blobs(Field **ptr) {
  for (; *ptr; ptr++) {
    if ((*ptr)->flags & BLOB_FLAG)
      if (((Field_blob *)(*ptr))->copy()) return 1;  // Error
  }
  return 0;
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
  DBUG_ASSERT(this - 1 != join()->before_ref_item_slice_tmp3 &&
              this != join()->before_ref_item_slice_tmp3);
  THD *thd = join()->thd;
  DBUG_ENTER("remove_duplicates");

  DBUG_ASSERT(join()->tmp_tables > 0 && table()->s->tmp_table != NO_TMP_TABLE);
  THD_STAGE_INFO(thd, stage_removing_duplicates);

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
    DBUG_RETURN(false);
  }
  Field **first_field = tbl->field + tbl->s->fields - field_count;

  size_t *field_lengths =
      (size_t *)my_malloc(key_memory_hash_index_key_buffer,
                          field_count * sizeof(*field_lengths), MYF(MY_WME));
  if (field_lengths == nullptr) DBUG_RETURN(true);

  size_t key_length = compute_field_lengths(first_field, field_lengths);

  free_io_cache(tbl);  // Safety
  tbl->file->info(HA_STATUS_VARIABLE);
  constexpr int HASH_OVERHEAD = 16;  // Very approximate.
  if (tbl->s->db_type() == temptable_hton || tbl->s->db_type() == heap_hton ||
      (!tbl->s->blob_fields &&
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
  DBUG_RETURN(error);
}

static bool remove_dup_with_compare(THD *thd, TABLE *table, Field **first_field,
                                    ulong offset, Item *having) {
  handler *file = table->file;
  char *org_record, *new_record;
  uchar *record;
  int error;
  ulong reclength = table->s->reclength - offset;
  DBUG_ENTER("remove_dup_with_compare");

  org_record = (char *)(record = table->record[0]) + offset;
  new_record = (char *)table->record[1] + offset;

  if ((error = file->ha_rnd_init(1))) goto err;
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
    bool found = 0;
    for (;;) {
      if ((error = file->ha_rnd_next(record))) {
        if (error == HA_ERR_RECORD_DELETED) continue;
        if (error == HA_ERR_END_OF_FILE) break;
        goto err;
      }
      if (compare_record(table, first_field) == 0) {
        if ((error = file->ha_delete_row(record))) goto err;
      } else if (!found) {
        found = 1;
        file->position(record);  // Remember position
      }
    }
    if (!found) break;  // End of file
    /* Restart search on next row */
    error = file->ha_rnd_pos(record, file->ref);
  }

  DBUG_RETURN(false);
err:
  if (file->inited) (void)file->ha_rnd_end();
  if (error) file->print_error(error, MYF(0));
  DBUG_RETURN(true);
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
  DBUG_ENTER("remove_dup_with_hash_index");

  MEM_ROOT mem_root(key_memory_hash_index_key_buffer, 32768);
  memroot_unordered_set<std::string> hash(&mem_root);
  hash.reserve(file->stats.records);

  std::unique_ptr<uchar[]> key_buffer(new uchar[key_length]);
  if ((error = file->ha_rnd_init(1))) goto err;
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
  DBUG_RETURN(false);

err:
  if (file->inited) (void)file->ha_rnd_end();
  if (error) file->print_error(error, MYF(0));
  DBUG_RETURN(true);
}

bool cp_buffer_from_ref(THD *thd, TABLE *table, TABLE_REF *ref) {
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
  DBUG_ENTER("make_group_fields");
  if (main_join->group_fields_cache.elements) {
    curr_join->group_fields = main_join->group_fields_cache;
    curr_join->sort_and_group = 1;
  } else {
    if (alloc_group_fields(curr_join, curr_join->group_list)) DBUG_RETURN(1);
    main_join->group_fields_cache = curr_join->group_fields;
  }
  DBUG_RETURN(0);
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
  join->sort_and_group = 1; /* Mark for do_select */
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

int test_if_item_cache_changed(List<Cached_item> &list) {
  DBUG_ENTER("test_if_item_cache_changed");
  List_iterator<Cached_item> li(list);
  int idx = -1, i;
  Cached_item *buff;

  for (i = (int)list.elements - 1; (buff = li++); i--) {
    if (buff->cmp()) idx = i;
  }
  DBUG_PRINT("info", ("idx: %d", idx));
  DBUG_RETURN(idx);
}

/**
  Setup copy_fields to save fields at start of new group.

  Setup copy_fields to save fields at start of new group

  Only FIELD_ITEM:s and FUNC_ITEM:s needs to be saved between groups.
  Change old item_field to use a new field with points at saved fieldvalue
  This function is only called before use of send_result_set_metadata.

  @param thd                   THD pointer
  @param param                 temporary table parameters
  @param ref_item_array        array of pointers to top elements of filed list
  @param res_selected_fields   new list of items of select item list
  @param res_all_fields        new list of all items
  @param elements              number of elements in select item list
  @param all_fields            all fields list

  @todo
    In most cases this result will be sent to the user.
    This should be changed to use copy_int or copy_real depending
    on how the value is to be used: In some cases this may be an
    argument in a group function, like: IF(ISNULL(col),0,COUNT(*))

  @returns false if success, true if error
*/

bool setup_copy_fields(THD *thd, Temp_table_param *param,
                       Ref_item_array ref_item_array,
                       List<Item> &res_selected_fields,
                       List<Item> &res_all_fields, uint elements,
                       List<Item> &all_fields) {
  DBUG_ENTER("setup_copy_fields");

  Item *pos;
  List_iterator_fast<Item> li(all_fields);
  Copy_field *copy = NULL;
  Copy_field *copy_start MY_ATTRIBUTE((unused));
  res_selected_fields.empty();
  res_all_fields.empty();
  List_iterator_fast<Item> itr(res_all_fields);
  List<Item> extra_funcs;
  uint i, border = all_fields.elements - elements;

  if (param->field_count && !(copy = param->copy_field = new (*THR_MALLOC)
                                  Copy_field[param->field_count]))
    goto err2;

  param->copy_funcs.empty();
  copy_start = copy;
  for (i = 0; (pos = li++); i++) {
    Field *field;
    uchar *tmp;
    Item *real_pos = pos->real_item();
    if (real_pos->type() == Item::FIELD_ITEM) {
      Item_field *item;
      if (!(item = new Item_field(thd, ((Item_field *)real_pos)))) goto err;
      if (pos->type() == Item::REF_ITEM) {
        /* preserve the names of the ref when dereferncing */
        Item_ref *ref = (Item_ref *)pos;
        item->db_name = ref->db_name;
        item->table_name = ref->table_name;
        item->item_name = ref->item_name;
      }
      pos = item;
      if (item->field->flags & BLOB_FLAG) {
        if (!(pos = Item_copy::create(pos))) goto err;
        /*
          Item_copy_string::copy for function can call
          Item_copy_string::val_int for blob via Item_ref.
          But if Item_copy_string::copy for blob isn't called before,
          it's value will be wrong
          so let's insert Item_copy_string for blobs in the beginning of
          copy_funcs
          (to see full test case look at having.test, BUG #4358)
        */
        if (param->copy_funcs.push_front(pos)) goto err;
      } else {
        /*
           set up save buffer and change result_field to point at
           saved value
        */
        field = item->field;
        item->result_field = field->new_field(thd->mem_root, field->table, 1);
        /*
          We need to allocate one extra byte for null handling.
        */
        if (!(tmp = static_cast<uchar *>(sql_alloc(field->pack_length() + 1))))
          goto err;
        if (copy) {
          DBUG_ASSERT(param->field_count > (uint)(copy - copy_start));
          copy->set(tmp, item->result_field);
          item->result_field->move_field(copy->to_ptr, copy->to_null_ptr, 1);

          /*
            We have created a new Item_field; its field points into the
            previous table; its result_field points into a memory area
            (REF_SLICE_TMP3) which represents the pseudo-tmp-table from where
            aggregates' values can be read. So does 'field'.
            A Copy_field manages copying from 'field' to the memory area.
          */
          item->field = item->result_field;
          /*
            Even though the field doesn't point into field->table->record[0], we
            must still link it to 'table' through field->table because that's an
            existing way to access some type info (e.g. nullability from
            table->nullable).
          */
          copy++;
        }
      }
    } else if ((real_pos->type() == Item::FUNC_ITEM ||
                real_pos->type() == Item::SUBSELECT_ITEM ||
                real_pos->type() == Item::CACHE_ITEM ||
                real_pos->type() == Item::COND_ITEM) &&
               !real_pos->has_aggregation()) {  // Save for send fields
      pos = real_pos;
      /* TODO:
         In most cases this result will be sent to the user.
         This should be changed to use copy_int or copy_real depending
         on how the value is to be used: In some cases this may be an
         argument in a group function, like: IF(ISNULL(col),0,COUNT(*))
      */
      if (!(pos = Item_copy::create(pos))) goto err;
      if (i < border)  // HAVING, ORDER and GROUP BY
      {
        if (extra_funcs.push_back(pos)) goto err;
      } else if (param->copy_funcs.push_back(pos))
        goto err;
    }
    res_all_fields.push_back(pos);
    ref_item_array[((i < border) ? all_fields.elements - i - 1 : i - border)] =
        pos;
  }
  param->copy_field_end = copy;

  for (i = 0; i < border; i++) itr++;
  itr.sublist(res_selected_fields, elements);
  /*
    Put elements from HAVING, ORDER BY and GROUP BY last to ensure that any
    reference used in these will resolve to a item that is already calculated
  */
  param->copy_funcs.concat(&extra_funcs);

  DBUG_RETURN(0);

err:
  destroy_array(param->copy_field, param->field_count);
  param->copy_field = nullptr;
err2:
  DBUG_RETURN(true);
}

/**
  Make a copy of all simple SELECT'ed items.

  This is done at the start of a new group so that we can retrieve
  these later when the group changes.

  @param param     Represents the current temporary file being produced
  @param thd       The current thread

  @returns false if OK, true on error.
*/

bool copy_fields(Temp_table_param *param, const THD *thd) {
  DBUG_ENTER("copy_fields");
  Copy_field *ptr = param->copy_field;
  Copy_field *end = param->copy_field_end;

  DBUG_ASSERT((ptr != NULL && end >= ptr) || (ptr == NULL && end == NULL));
  DBUG_PRINT("enter", ("for param %p", param));
  for (; ptr < end; ptr++) ptr->invoke_do_copy(ptr);

  List_iterator_fast<Item> it(param->copy_funcs);
  Item_copy *item;
  bool is_error = thd->is_error();
  while (!is_error && (item = (Item_copy *)it++)) is_error = item->copy(thd);

  DBUG_RETURN(is_error);
}

/**
  Change all funcs and sum_funcs to fields in tmp table, and create
  new list of all items.

  @param thd                   THD pointer
  @param ref_item_array        array of pointers to top elements of filed list
  @param res_selected_fields   new list of items of select item list
  @param res_all_fields        new list of all items
  @param elements              number of elements in select item list
  @param all_fields            all fields list

  @returns false if success, true if error
*/

bool change_to_use_tmp_fields(THD *thd, Ref_item_array ref_item_array,
                              List<Item> &res_selected_fields,
                              List<Item> &res_all_fields, uint elements,
                              List<Item> &all_fields) {
  List_iterator_fast<Item> it(all_fields);
  Item *item_field, *item;
  DBUG_ENTER("change_to_use_tmp_fields");

  res_selected_fields.empty();
  res_all_fields.empty();

  uint border = all_fields.elements - elements;
  for (uint i = 0; (item = it++); i++) {
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
        if (!suv || !new_field) DBUG_RETURN(true);  // Fatal error
        List<Item> list;
        list.push_back(new_field);
        suv->set_arguments(list, true);
        item_field = suv;
      } else
        item_field = item;
    } else if ((field = item->get_tmp_table_field())) {
      if (item->type() == Item::SUM_FUNC_ITEM && field->table->group)
        item_field = ((Item_sum *)item)->result_item(field);
      else
        item_field = (Item *)new Item_field(field);
      if (!item_field) DBUG_RETURN(true);  // Fatal error

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
        item->print(&str, QT_ORDINARY);
        item_field->item_name.copy(str.ptr(), str.length());
      }
#endif
    } else
      item_field = item;

    res_all_fields.push_back(item_field);
    /*
      Cf. comment explaining the reordering going on below in
      similar section of change_refs_to_tmp_fields
    */
    ref_item_array[((i < border) ? all_fields.elements - i - 1 : i - border)] =
        item_field;
    item_field->set_orig_field(item->get_orig_field());
  }

  List_iterator_fast<Item> itr(res_all_fields);
  for (uint i = 0; i < border; i++) itr++;
  itr.sublist(res_selected_fields, elements);
  DBUG_RETURN(false);
}

/**
  Change all sum_func refs to fields to point at fields in tmp table.
  Change all funcs to be fields in tmp table.

  @param thd                   THD pointer
  @param ref_item_array        array of pointers to top elements of filed list
  @param res_selected_fields   new list of items of select item list
  @param res_all_fields        new list of all items
  @param elements              number of elements in select item list
  @param all_fields            all fields list

  @returns false if success, true if error
*/

bool change_refs_to_tmp_fields(THD *thd, Ref_item_array ref_item_array,
                               List<Item> &res_selected_fields,
                               List<Item> &res_all_fields, uint elements,
                               List<Item> &all_fields) {
  DBUG_ENTER("change_refs_to_tmp_fields");
  List_iterator_fast<Item> it(all_fields);
  Item *item, *new_item;
  res_selected_fields.empty();
  res_all_fields.empty();

  uint border = all_fields.elements - elements;
  for (uint i = 0; (item = it++); i++) {
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
    res_all_fields.push_back(new_item = item->get_tmp_table_item(thd));
    ref_item_array[((i < border) ? all_fields.elements - i - 1 : i - border)] =
        new_item;
  }

  List_iterator_fast<Item> itr(res_all_fields);
  for (uint i = 0; i < border; i++) itr++;
  itr.sublist(res_selected_fields, elements);

  DBUG_RETURN(thd->is_fatal_error);
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
  DBUG_ENTER("QEP_tmp_table::prepare_tmp_table");
  Temp_table_param *const tmp_tbl = qep_tab->tmp_table_param;

  /*
    Window final tmp file optimization: we skip actually writing to the
    tmp file, so no need to physically create it.
  */
  if (tmp_tbl->m_window_short_circuit) DBUG_RETURN(false);

  TABLE *table = qep_tab->table();
  JOIN *join = qep_tab->join();
  int rc = 0;

  if (!table->is_created()) {
    if (instantiate_tmp_table(join->thd, table, tmp_tbl->keyinfo,
                              tmp_tbl->start_recinfo, &tmp_tbl->recinfo,
                              join->select_lex->active_options(),
                              join->thd->variables.big_tables))
      DBUG_RETURN(true);
    empty_record(table);
  }
  /* If it wasn't already, start index scan for grouping using table index. */
  if (!table->file->inited &&
      ((table->group && tmp_tbl->sum_func_count && table->s->keys) ||
       table->hash_field))
    rc = table->file->ha_index_init(0, 0);
  else {
    /* Start index scan in scanning mode */
    rc = table->file->ha_rnd_init(true);
  }
  if (rc) {
    table->file->print_error(rc, MYF(0));
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
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
    int error;
    if (in_first_read) {
      in_first_read = false;
      error = join_init_read_record(qep_tab);
    } else
      error = qep_tab->read_record.read_record(&qep_tab->read_record);

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

bool QEP_TAB::pfs_batch_update(JOIN *join) {
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
