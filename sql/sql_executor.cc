<<<<<<< HEAD
/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.
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

/**
  @file

  @brief
  Query execution

  @defgroup Query_Executor  Query Executor
  @{
*/

#include "sql/sql_executor.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "field_types.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "mem_root_deque.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_byteorder.h"
#include "my_checksum.h"
#include "my_dbug.h"
#include "my_loglevel.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"
#include "prealloced_array.h"
#include "sql-common/json_dom.h"  // Json_wrapper
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/filesort.h"  // Filesort
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_sum.h"  // Item_sum
#include "sql/iterators/sorting_iterator.h"
#include "sql/iterators/timing_iterator.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/cost_model.h"
#include "sql/join_optimizer/join_optimizer.h"
#include "sql/join_optimizer/materialize_path_parameters.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/join_optimizer/walk_access_paths.h"
#include "sql/join_type.h"
#include "sql/key.h"  // key_cmp
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"  // stage_executing
#include "sql/nested_join.h"
#include "sql/opt_costmodel.h"
#include "sql/opt_explain_format.h"
#include "sql/opt_trace.h"  // Opt_trace_object
#include "sql/query_options.h"
#include "sql/record_buffer.h"  // Record_buffer
#include "sql/sort_param.h"
#include "sql/sql_array.h"  // Bounds_checked_array
#include "sql/sql_base.h"   // fill_record
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_delete.h"
#include "sql/sql_executor.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_resolver.h"
#include "sql/sql_select.h"
#include "sql/sql_tmp_table.h"  // create_tmp_table
#include "sql/sql_update.h"
#include "sql/table.h"
#include "sql/temp_table_param.h"
#include "sql/visible_fields.h"
#include "sql/window.h"
#include "tables_contained_in.h"
#include "template_utils.h"
#include "thr_lock.h"

using std::make_pair;
using std::max;
using std::min;
using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;

static int read_system(TABLE *table);
static bool alloc_group_fields(JOIN *join, ORDER *group);

/// Maximum amount of space (in bytes) to allocate for a Record_buffer.
static constexpr size_t MAX_RECORD_BUFFER_SIZE = 128 * 1024;  // 128KB

string RefToString(const Index_lookup &ref, const KEY *key,
                   bool include_nulls) {
  string ret;

<<<<<<< HEAD
  if (ref.keypart_hash != nullptr) {
    assert(!include_nulls);
    ret = key->key_part[0].field->field_name;
    ret += "=hash(";
    for (unsigned key_part_idx = 0; key_part_idx < ref.key_parts;
         ++key_part_idx) {
      if (key_part_idx != 0) {
        ret += ", ";
=======
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

  assert(select_lex == thd->lex->current_select());

  /*
    Check that we either
    - have no tables, or
    - have tables and have locked them, or
    - called for fake_select_lex, which may have temporary tables which do
      not need locking up front.
  */
  assert(!tables || thd->lex->is_query_tables_locked() ||
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
>>>>>>> pr/231
      }
      ret += ItemToString(ref.items[key_part_idx]);
    }
    ret += ")";
    return ret;
  }

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
      assert(!field->is_hidden_by_system());
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

bool JOIN::create_intermediate_table(
    QEP_TAB *const tab, const mem_root_deque<Item *> &tmp_table_fields,
    ORDER_with_src &tmp_table_group, bool save_sum_fields) {
  DBUG_TRACE;
  THD_STAGE_INFO(thd, stage_creating_tmp_table);
  const bool windowing = m_windows.elements > 0;
  /*
    Pushing LIMIT to the temporary table creation is not applicable
    when there is ORDER BY or GROUP BY or aggregate/window functions, because
    in all these cases we need all result rows.
  */
  ha_rows tmp_rows_limit =
      ((order.empty() || skip_sort_order) && tmp_table_group.empty() &&
       !windowing && !query_block->with_sum_func)
          ? m_select_limit
          : HA_POS_ERROR;

  tab->tmp_table_param =
      new (thd->mem_root) Temp_table_param(thd->mem_root, tmp_table_param);
  tab->tmp_table_param->skip_create_table = true;

  bool distinct_arg =
      select_distinct &&
      // GROUP BY is absent or has been done in a previous step
      group_list.empty() &&
      // We can only do DISTINCT in last window's tmp table step
      (!windowing || (tab->tmp_table_param->m_window &&
                      tab->tmp_table_param->m_window->is_last()));

  TABLE *table =
      create_tmp_table(thd, tab->tmp_table_param, tmp_table_fields,
                       tmp_table_group.order, distinct_arg, save_sum_fields,
                       query_block->active_options(), tmp_rows_limit, "");
  if (!table) return true;
  tmp_table_param.using_outer_summary_function =
      tab->tmp_table_param->using_outer_summary_function;

<<<<<<< HEAD
  assert(tab->idx() > 0);
=======
<<<<<<< HEAD
  DBUG_ASSERT(tab->idx() > 0);
  tab[-1].next_select = sub_select_op;
  if (!(tab->op = new (thd->mem_root) QEP_tmp_table(tab))) goto err;
=======
  assert(tab->idx() > 0);
  tab[-1].next_select= sub_select_op;
  if (!(tab->op= new (thd->mem_root) QEP_tmp_table(tab)))
    goto err;
>>>>>>> upstream/cluster-7.6

>>>>>>> pr/231
  tab->set_table(table);

  /**
    If this is a window's OUT table, any final DISTINCT, ORDER BY will lead to
    windows showing use of tmp table in the final windowing step, so no
    need to signal use of tmp table unless we are here for another tmp table.
  */
  if (!tab->tmp_table_param->m_window) {
    if (table->group)
      explain_flags.set(tmp_table_group.src, ESP_USING_TMPTABLE);
    else if (table->s->is_distinct || select_distinct)
      explain_flags.set(ESC_DISTINCT, ESP_USING_TMPTABLE);
    else {
      /*
        Try to find a reason for this table, to show in EXPLAIN.
        If there's no GROUP BY, no ORDER BY, no DISTINCT, it must be just a
        result buffer. If there's ORDER BY but there is also windowing
        then ORDER BY happens after windowing, and here we are before
        windowing, so the table is not for ORDER BY either.
      */
      if ((group_list.empty() && (order.empty() || windowing) &&
           !select_distinct) ||
          (query_block->active_options() &
           (SELECT_BIG_RESULT | OPTION_BUFFER_RESULT)))
        explain_flags.set(ESC_BUFFER_RESULT, ESP_USING_TMPTABLE);
    }
  }
  /* if group or order on first table, sort first */
  if (!group_list.empty() && simple_group) {
    DBUG_PRINT("info", ("Sorting for group"));

    if (m_ordered_index_usage != ORDERED_INDEX_GROUP_BY &&
        add_sorting_to_table(const_tables, &group_list,
                             /*sort_before_group=*/true))
      goto err;

    if (alloc_group_fields(this, group_list.order)) goto err;
    if (make_sum_func_list(*fields, true)) goto err;
    const bool need_distinct =
        !(tab->range_scan() &&
          tab->range_scan()->type == AccessPath::GROUP_INDEX_SKIP_SCAN);
    if (prepare_sum_aggregators(sum_funcs, need_distinct)) goto err;
    if (setup_sum_funcs(thd, sum_funcs)) goto err;
    group_list.clean();
  } else {
    if (make_sum_func_list(*fields, false)) goto err;
    const bool need_distinct =
        !(tab->range_scan() &&
          tab->range_scan()->type == AccessPath::GROUP_INDEX_SKIP_SCAN);
    if (prepare_sum_aggregators(sum_funcs, need_distinct)) goto err;
    if (setup_sum_funcs(thd, sum_funcs)) goto err;

    // In many cases, we can resolve ORDER BY for a query, if requested, by
    // sorting this temporary table. However, we cannot do so if the sort is
    // disturbed by additional rows from rollup or different sorting from
    // window functions. Also, if this temporary table is doing deduplication,
    // sorting is not added here, but once the correct ref_slice is set up in
    // make_tmp_tables_info().
    if (group_list.empty() && !table->s->is_distinct && !order.empty() &&
        simple_order && rollup_state == RollupState::NONE && !m_windows_sort) {
      DBUG_PRINT("info", ("Sorting for order"));

      if (m_ordered_index_usage != ORDERED_INDEX_ORDER_BY &&
          add_sorting_to_table(const_tables, &order,
                               /*sort_before_group=*/false))
        goto err;
      order.clean();
    }
  }
  return false;

err:
  if (table != nullptr) {
    close_tmp_table(table);
    free_tmp_table(table);
    tab->set_table(nullptr);
  }
  return true;
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
  item = item->real_item();

  if (is_rollup_group_wrapper(item) &&
      down_cast<Item_rollup_group_item *>(item)->rollup_null()) {
    return true;
  }

  if (item->type() == Item::CACHE_ITEM) {
    return has_rollup_result(down_cast<Item_cache *>(item)->example);
  } else if (item->type() == Item::FUNC_ITEM) {
    Item_func *item_func = down_cast<Item_func *>(item);
    for (uint i = 0; i < item_func->arg_count; i++) {
      if (has_rollup_result(item_func->arguments()[i])) return true;
    }
  } else if (item->type() == Item::COND_ITEM) {
    for (Item &arg : *down_cast<Item_cond *>(item)->argument_list()) {
      if (has_rollup_result(&arg)) return true;
    }
  }

  return false;
}

bool is_rollup_group_wrapper(Item *item) {
  return item->type() == Item::FUNC_ITEM &&
         down_cast<Item_func *>(item)->functype() ==
             Item_func::ROLLUP_GROUP_ITEM_FUNC;
}

Item *unwrap_rollup_group(Item *item) {
  if (is_rollup_group_wrapper(item)) {
    return down_cast<Item_rollup_group_item *>(item)->inner_item();
  } else {
    return item;
  }
}

void JOIN::optimize_distinct() {
  for (int i = primary_tables - 1; i >= 0; --i) {
    QEP_TAB *last_tab = qep_tab + i;
    if (query_block->select_list_tables & last_tab->table_ref->map()) break;
    last_tab->not_used_in_distinct = true;
  }

  /* Optimize "select distinct b from t1 order by key_part_1 limit #" */
  if (!order.empty() && skip_sort_order) {
    /* Should already have been optimized away */
<<<<<<< HEAD
    assert(m_ordered_index_usage == ORDERED_INDEX_ORDER_BY);
    if (m_ordered_index_usage == ORDERED_INDEX_ORDER_BY) {
      order.clean();
=======
<<<<<<< HEAD
    DBUG_ASSERT(m_ordered_index_usage == ORDERED_INDEX_ORDER_BY);
    if (m_ordered_index_usage == ORDERED_INDEX_ORDER_BY) {
      order = NULL;
=======
    assert(ordered_index_usage == ordered_index_order_by);
    if (ordered_index_usage == ordered_index_order_by)
    {
      order= NULL;
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
    }
  }
}

bool prepare_sum_aggregators(Item_sum **sum_funcs, bool need_distinct) {
  for (Item_sum **item = sum_funcs; *item != nullptr; ++item) {
    if ((*item)->set_aggregator(need_distinct && (*item)->has_with_distinct()
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
                              TABLE *tmp_table [[maybe_unused]]) {
  DBUG_TRACE;
  Item_sum *func;
  while ((func = *(func_ptr++))) func->update_field();
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
  if (param->items_to_copy == nullptr) {
    return false;
  }

  for (const Func_ptr &func : *param->items_to_copy) {
    if (func.should_copy(type)) {
      func.func()->save_in_field_no_error_check(func.result_field(),
                                                /*no_conversions=*/true);
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
    bool contained_const = item_equal->const_arg() != nullptr;
    if (item_equal->update_const(thd)) return true;
    if (!contained_const && item_equal->const_arg()) {
      /* Update keys for range analysis */
      for (Item_field &item_field : item_equal->get_fields()) {
        const Field *field = item_field.field;
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
               use && use->table_ref == item_field.table_ref; use++) {
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
  @brief Setup write_func of QEP_tmp_table object

  @param tab QEP_TAB of a tmp table
  @param trace Opt_trace_object to add to
  @details
  Function sets up write_func according to how QEP_tmp_table object that
  is attached to the given join_tab will be used in the query.
*/

<<<<<<< HEAD
void setup_tmptable_write_func(QEP_TAB *tab, Opt_trace_object *trace) {
  DBUG_TRACE;
  JOIN *join = tab->join();
  TABLE *table = tab->table();
  Temp_table_param *const tmp_tbl = tab->tmp_table_param;
  uint phase = tab->ref_item_slice;
  const char *description = nullptr;
<<<<<<< HEAD
  assert(table);
=======
  DBUG_ASSERT(table && op);
=======
void setup_tmptable_write_func(QEP_TAB *tab)
{
  JOIN *join= tab->join();
  TABLE *table= tab->table();
  QEP_tmp_table *op= (QEP_tmp_table *)tab->op;
  Temp_table_param *const tmp_tbl= tab->tmp_table_param;

  assert(table && op);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

  if (table->group && tmp_tbl->sum_func_count &&
      !tmp_tbl->precomputed_group_by) {
    /*
      Note for MyISAM tmp tables: if uniques is true keys won't be
      created.
    */
    assert(phase < REF_SLICE_WIN_1);
    if (table->s->keys) {
      description = "continuously_update_group_row";
      tab->op_type = QEP_TAB::OT_AGGREGATE_INTO_TMP_TABLE;
    }
  } else if (join->streaming_aggregation && !tmp_tbl->precomputed_group_by) {
    assert(phase < REF_SLICE_WIN_1);
    description = "write_group_row_when_complete";
    DBUG_PRINT("info", ("Using end_write_group"));
    tab->op_type = QEP_TAB::OT_AGGREGATE_THEN_MATERIALIZE;

    for (Item_sum **func_ptr = join->sum_funcs; *func_ptr != nullptr;
         ++func_ptr) {
      tmp_tbl->items_to_copy->push_back(
          Func_ptr(*func_ptr, (*func_ptr)->get_result_field()));
    }
  } else {
    description = "write_all_rows";
    tab->op_type = (phase >= REF_SLICE_WIN_1 ? QEP_TAB::OT_WINDOWING_FUNCTION
                                             : QEP_TAB::OT_MATERIALIZE);
    if (tmp_tbl->precomputed_group_by) {
      for (Item_sum **func_ptr = join->sum_funcs; *func_ptr != nullptr;
           ++func_ptr) {
        tmp_tbl->items_to_copy->push_back(
            Func_ptr(*func_ptr, (*func_ptr)->get_result_field()));
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
    end_query_block function to use. This function can't fail.
*/
QEP_TAB::enum_op_type JOIN::get_end_select_func() {
  DBUG_TRACE;
  /*
     Choose method for presenting result to user. Use end_send_group
     if the query requires grouping (has a GROUP BY clause and/or one or
     more aggregate functions). Use end_send if the query should not
     be grouped.
   */
  if (streaming_aggregation && !tmp_table_param.precomputed_group_by) {
    DBUG_PRINT("info", ("Using end_send_group"));
    return QEP_TAB::OT_AGGREGATE;
  }
  DBUG_PRINT("info", ("Using end_send"));
  return QEP_TAB::OT_NONE;
}

/**
  Find out how many bytes it takes to store the smallest prefix which
  covers all the columns that will be read from a table.

  @param table the table to read
  @return the size of the smallest prefix that covers all records to be
          read from the table
*/
static size_t record_prefix_size(const TABLE *table) {
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
  const uchar *prefix_end = table->record[0];  // beginning of record
  for (auto f = table->field, end = table->field + table->s->fields; f < end;
       ++f) {
    if (bitmap_is_set(table->read_set, (*f)->field_index()))
      prefix_end = std::max<const uchar *>(
          prefix_end, (*f)->field_ptr() + (*f)->pack_length());
  }

  /*
    If this is an index merge, the primary key columns may be required
    for positioning in a later stage, even though they are not in the
    read_set here. Allocate space for them in case they are needed.
  */
  if (!table->s->is_missing_primary_key() &&
      (table->file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION)) {
    const KEY &key = table->key_info[table->s->primary_key];
    for (auto kp = key.key_part, end = kp + key.user_defined_key_parts;
         kp < end; ++kp) {
      const Field *f = table->field[kp->fieldnr - 1];
      /*
        If a key column comes after all the columns in the read set,
        extend the prefix to include the key column.
      */
      prefix_end = std::max(prefix_end, f->field_ptr() + f->pack_length());
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

  @param table the table to read
  @param expected_rows_to_fetch number of rows the optimizer thinks
    we will be reading out of the table
  @retval true if an error occurred when allocating the buffer
  @retval false if a buffer was successfully allocated, or if a buffer
  was not attempted allocated
*/
bool set_record_buffer(TABLE *table, double expected_rows_to_fetch) {
  assert(table->file->inited);
  assert(table->file->ha_get_record_buffer() == nullptr);

  // Skip temporary tables, those with no estimates, or if we don't
  // expect multiple rows.
  if (expected_rows_to_fetch <= 1.0) return false;

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
    assert(table->m_record_buffer.record_size() == record_prefix_size(table));
    table->m_record_buffer.reset();
    table->file->ha_set_record_buffer(&table->m_record_buffer);
    return false;
  }

  ha_rows rows_in_buffer =
      static_cast<ha_rows>(std::ceil(expected_rows_to_fetch));

  /*
    How much space do we need to allocate for each record? Enough to
    hold all columns from the beginning and up to the last one in the
    read set. We don't need to allocate space for unread columns at
    the end of the record.
  */
  const size_t record_size = record_prefix_size(table);

  // Do not allocate a buffer whose total size exceeds MAX_RECORD_BUFFER_SIZE.
  if (record_size > 0)
    rows_in_buffer =
        std::min<ha_rows>(MAX_RECORD_BUFFER_SIZE / record_size, rows_in_buffer);

  // Do not allocate space for more rows than the handler asked for.
  rows_in_buffer = std::min(rows_in_buffer, max_rows);

  const auto bufsize = Record_buffer::buffer_size(rows_in_buffer, record_size);
  const auto ptr = pointer_cast<uchar *>(current_thd->alloc(bufsize));
  if (ptr == nullptr) return true; /* purecov: inspected */

  table->m_record_buffer = Record_buffer{rows_in_buffer, record_size, ptr};
  table->file->ha_set_record_buffer(&table->m_record_buffer);
  return false;
}

<<<<<<< HEAD
bool ExtractConditions(Item *condition,
                       Mem_root_array<Item *> *condition_parts) {
  return WalkConjunction(condition, [condition_parts](Item *item) {
    return condition_parts->push_back(item);
  });
=======
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
<<<<<<< HEAD
      DBUG_ASSERT(join->examined_rows <= 1);
    } else if (join->send_row_on_empty_set()) {
      table_map save_nullinfo = 0;
=======
      assert(join->examined_rows <= 1);
    }
    else if (join->send_row_on_empty_set())
    {
      table_map save_nullinfo= 0;
      /*
        If this is a subquery, we need to save and later restore
        the const table NULL info before clearing the tables
        because the following executions of the subquery do not
        reevaluate constant fields. @see save_const_null_info
        and restore_const_null_info
      */
      if (join->select_lex->master_unit()->item && join->const_tables)
        save_const_null_info(join, &save_nullinfo);
>>>>>>> upstream/cluster-7.6

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
<<<<<<< HEAD
    if (join->thd->is_error()) error = NESTED_LOOP_ERROR;
  } else {
    QEP_TAB *qep_tab = join->qep_tab + join->const_tables;
    DBUG_ASSERT(join->primary_tables);
    error = join->first_select(join, qep_tab, 0);
    if (error >= NESTED_LOOP_OK) error = join->first_select(join, qep_tab, 1);
=======
    if (join->thd->is_error())
      error= NESTED_LOOP_ERROR;
  }
  else
  {
    QEP_TAB *qep_tab= join->qep_tab + join->const_tables;
    assert(join->primary_tables);
    error= join->first_select(join,qep_tab,0);
    if (error >= NESTED_LOOP_OK)
      error= join->first_select(join,qep_tab,1);
>>>>>>> upstream/cluster-7.6
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
<<<<<<< HEAD
      sort_tab = &join->qep_tab[join->primary_tables + join->tmp_tables - 1];
    else {
      DBUG_ASSERT(!join->plan_is_const());
      sort_tab = &join->qep_tab[const_tables];
=======
      sort_tab= &join->qep_tab[join->primary_tables + join->tmp_tables - 1];
    else
    {
      assert(!join->plan_is_const());
      sort_tab= &join->qep_tab[const_tables];
>>>>>>> upstream/cluster-7.6
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
<<<<<<< HEAD

  rc = join->thd->is_error() ? -1 : rc;
#ifndef DBUG_OFF
  if (rc) {
    DBUG_PRINT("error", ("Error: do_select() failed"));
=======
  if (error == NESTED_LOOP_OK)
  {
    /*
      Sic: this branch works even if rc != 0, e.g. when
      send_data above returns an error.
    */
    if (join->select_lex->query_result()->send_eof())
      rc= 1;                                  // Don't send error
    DBUG_PRINT("info",("%ld records output", (long) join->send_records));
  }
  else
    rc= -1;
#ifndef NDEBUG
  if (rc)
  {
    DBUG_PRINT("error",("Error: do_select() failed"));
>>>>>>> upstream/cluster-7.6
  }
#endif
  DBUG_RETURN(rc);
>>>>>>> pr/231
}

/**
  See if “path” has any MRR nodes; if so, we cannot optimize them away
  in PossiblyAttachFilter(), as the BKA iterator expects there to be a
  corresponding MRR iterator. (This is a very rare case, so all we care about
  is that it should not crash.)
 */
static bool ContainsAnyMRRPaths(AccessPath *path) {
  bool any_mrr_paths = false;
  WalkAccessPaths(path, /*join=*/nullptr,
                  WalkAccessPathPolicy::STOP_AT_MATERIALIZATION,
                  [&any_mrr_paths](const AccessPath *sub_path, const JOIN *) {
                    if (sub_path->type == AccessPath::MRR) {
                      any_mrr_paths = true;
                      return true;
                    } else {
                      return false;
                    }
                  });
  return any_mrr_paths;
}

Item *CreateConjunction(List<Item> *items) {
  if (items->size() == 0) {
    return nullptr;
  }
<<<<<<< HEAD
  if (items->size() == 1) {
    return items->head();
  }
  Item_cond_and *condition = new Item_cond_and(*items);
  condition->quick_fix_field();
  condition->update_used_tables();
  condition->apply_is_true();
  return condition;
=======

  enum_nested_loop_state rc;
  QEP_operation *op = qep_tab->op;

  /* This function cannot be called if qep_tab has no associated operation */
<<<<<<< HEAD
  DBUG_ASSERT(op != NULL);
  if (end_of_records) {
    rc = op->end_send();
    if (rc >= NESTED_LOOP_OK) rc = sub_select(join, qep_tab, end_of_records);
=======
  assert(op != NULL);

  if (end_of_records)
  {
    rc= op->end_send();
    if (rc >= NESTED_LOOP_OK)
      rc= sub_select(join, qep_tab, end_of_records);
>>>>>>> upstream/cluster-7.6
    DBUG_RETURN(rc);
  }
  if (qep_tab->prepare_scan()) DBUG_RETURN(NESTED_LOOP_ERROR);

  /*
    setup_join_buffering() disables join buffering if QS_DYNAMIC_RANGE is
    enabled.
  */
  assert(!qep_tab->dynamic_range());

  rc = op->put_record();

  DBUG_RETURN(rc);
>>>>>>> pr/231
}

/**
  Return a new iterator that wraps "iterator" and that tests all of the given
  conditions (if any), ANDed together. If there are no conditions, just return
  the given iterator back.
 */
AccessPath *PossiblyAttachFilter(AccessPath *path,
                                 const vector<Item *> &conditions, THD *thd,
                                 table_map *conditions_depend_on_outer_tables) {
  // See if any of the sub-conditions are known to be always false,
  // and filter out any conditions that are known to be always true.
  List<Item> items;
  for (Item *cond : conditions) {
    if (cond->const_item()) {
      if (cond->val_int() == 0) {
        if (ContainsAnyMRRPaths(path)) {
          // Keep the condition. See comment on ContainsAnyMRRPaths().
          items.push_back(cond);
        } else {
          return NewZeroRowsAccessPath(thd, path, "Impossible filter");
        }
      } else {
        // Known to be always true, so skip it.
      }
    } else {
      items.push_back(cond);
    }
  }

  Item *condition = CreateConjunction(&items);
  if (condition == nullptr) {
    return path;
  }
  *conditions_depend_on_outer_tables |= condition->used_tables();

  AccessPath *filter_path = NewFilterAccessPath(thd, path, condition);

  // NOTE: We don't care about filter_effect here, even though we should.
  CopyBasicProperties(*path, filter_path);

  return filter_path;
}

AccessPath *CreateNestedLoopAccessPath(THD *thd, AccessPath *outer,
                                       AccessPath *inner, JoinType join_type,
                                       bool pfs_batch_mode) {
  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::NESTED_LOOP_JOIN;
  path->nested_loop_join().outer = outer;
  path->nested_loop_join().inner = inner;
  path->nested_loop_join().join_type = join_type;
  if (join_type == JoinType::ANTI || join_type == JoinType::SEMI) {
    // This does not make sense as an optimization for anti- or semijoins.
    path->nested_loop_join().pfs_batch_mode = false;
  } else {
    path->nested_loop_join().pfs_batch_mode = pfs_batch_mode;
  }
  return path;
}

static AccessPath *NewInvalidatorAccessPathForTable(
    THD *thd, AccessPath *path, QEP_TAB *qep_tab,
    plan_idx table_index_to_invalidate) {
  AccessPath *invalidator =
      NewInvalidatorAccessPath(thd, path, qep_tab->table()->alias);

  // Copy costs.
  invalidator->set_num_output_rows(path->num_output_rows());
  invalidator->cost = path->cost;

  QEP_TAB *tab2 = &qep_tab->join()->qep_tab[table_index_to_invalidate];
  if (tab2->invalidators == nullptr) {
    tab2->invalidators =
        new (thd->mem_root) Mem_root_array<const AccessPath *>(thd->mem_root);
  }
  tab2->invalidators->push_back(invalidator);
  return invalidator;
}

static table_map ConvertQepTabMapToTableMap(JOIN *join, qep_tab_map tables) {
  table_map map = 0;
  for (QEP_TAB *tab : TablesContainedIn(join, tables)) {
    map |= tab->table_ref->map();
  }
  return map;
}

AccessPath *CreateBKAAccessPath(THD *thd, JOIN *join, AccessPath *outer_path,
                                qep_tab_map left_tables, AccessPath *inner_path,
                                qep_tab_map right_tables, TABLE *table,
                                Table_ref *table_list, Index_lookup *ref,
                                JoinType join_type) {
  table_map left_table_map = ConvertQepTabMapToTableMap(join, left_tables);
  table_map right_table_map = ConvertQepTabMapToTableMap(join, right_tables);

  // If the BKA join condition (the “ref”) references fields that are outside
  // what we have available for this join, it is because they were
  // substituted by multi-equalities earlier (which assumes the
  // pre-iterator executor, which goes outside-in and not inside-out),
  // so find those multi-equalities and rewrite the fields back.
  for (uint part_no = 0; part_no < ref->key_parts; ++part_no) {
    Item *item = ref->items[part_no];
    if (item->type() == Item::FUNC_ITEM || item->type() == Item::COND_ITEM) {
      Item_func *func_item = down_cast<Item_func *>(item);
      if (func_item->functype() == Item_func::EQ_FUNC) {
        bool found = false;
        down_cast<Item_func_eq *>(func_item)
            ->ensure_multi_equality_fields_are_available(
                left_table_map, right_table_map, /*replace=*/true, &found);
      }
    } else if (item->type() == Item::FIELD_ITEM) {
      bool dummy;
      Item_equal *item_eq = find_item_equal(
          table_list->cond_equal, down_cast<Item_field *>(item), &dummy);
      if (item_eq == nullptr) {
        // Didn't come from a multi-equality.
        continue;
      }
      bool found = false;
      find_and_adjust_equal_fields(item, left_table_map, /*replace=*/true,
                                   &found);
    }
  }

  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::BKA_JOIN;
  path->bka_join().outer = outer_path;
  path->bka_join().inner = inner_path;
  path->bka_join().join_type = join_type;
  path->bka_join().mrr_length_per_rec = table->file->stats.mrr_length_per_rec;
  path->bka_join().rec_per_key =
      table->key_info[ref->key].records_per_key(ref->key_parts - 1);

  // Will be set later if we get a weedout access path as parent.
  path->bka_join().store_rowids = false;
  path->bka_join().tables_to_get_rowid_for = 0;

  return path;
}

static AccessPath *PossiblyAttachFilter(
    AccessPath *path, const vector<PendingCondition> &conditions, THD *thd,
    table_map *conditions_depend_on_outer_tables) {
  vector<Item *> stripped_conditions;
  for (const PendingCondition &cond : conditions) {
    stripped_conditions.push_back(cond.cond);
  }
  return PossiblyAttachFilter(path, stripped_conditions, thd,
                              conditions_depend_on_outer_tables);
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

/**
  For historical reasons, derived table materialization and temporary
  table materialization didn't specify the fields to materialize in the
  same way. Temporary table materialization used copy_funcs() to get the data
  into the Field pointers of the temporary table to be written, storing the
  lists in items_to_copy. (Originally, there was also copy_fields(), but it is
  no longer used for this purpose.)

  However, derived table materialization used JOIN::fields (which is a
  set of Item, not Field!) for the same purpose, calling fill_record()
  (which originally was meant for INSERT and UPDATE) instead. Thus, we
  have to rewrite one to the other, so that we can have only one
  MaterializeIterator. We choose to rewrite JOIN::fields to
  items_to_copy.

  TODO: The optimizer should output just one kind of structure directly.
 */
void ConvertItemsToCopy(const mem_root_deque<Item *> &items, Field **fields,
                        Temp_table_param *param) {
  assert(param->items_to_copy == nullptr);

  // All fields are to be copied.
  Func_ptr_array *copy_func =
      new (current_thd->mem_root) Func_ptr_array(current_thd->mem_root);
  Field **field_ptr = fields;
  for (Item *item : VisibleFields(items)) {
    copy_func->push_back(Func_ptr(item, *field_ptr++));
  }
  param->items_to_copy = copy_func;
}

/// @param item The item we want to see if is a join condition.
/// @param qep_tab The table we are joining in.
/// @returns true if 'item' is a join condition for a join involving the given
///   table (both equi-join and non-equi-join condition).
static bool IsJoinCondition(const Item *item, const QEP_TAB *qep_tab) {
  table_map used_tables = item->used_tables();
  if ((~qep_tab->table_ref->map() & used_tables) != 0) {
    // This is a join condition (either equi-join or non-equi-join).
    return true;
  }

  return false;
}

/// @returns the innermost condition of a nested trigger condition. If the item
///   is not a trigger condition, the item itself is returned.
static Item *GetInnermostCondition(Item *item) {
  Item_func_trig_cond *trig_cond = GetTriggerCondOrNull(item);
  while (trig_cond != nullptr) {
    item = trig_cond->arguments()[0];
    trig_cond = GetTriggerCondOrNull(item);
  }

  return item;
}

// Check if fields for a condition are available when joining the
// the given set of tables.
// Calls ensure_multi_equality_fields_are_available() to help.
static bool CheckIfFieldsAvailableForCond(Item *item, table_map build_tables,
                                          table_map probe_tables) {
  if (is_function_of_type(item, Item_func::EQ_FUNC)) {
    Item_func_eq *eq_func = down_cast<Item_func_eq *>(item);
    bool found = false;
    // Tries to find a suitable equal field for fields in the condition within
    // the available tables.
    eq_func->ensure_multi_equality_fields_are_available(
        build_tables, probe_tables, /*replace=*/false, &found);
    return found;
  } else if (item->type() == Item::COND_ITEM) {
    Item_cond *cond = down_cast<Item_cond *>(item);
    for (Item &cond_item : *cond->argument_list()) {
      if (!CheckIfFieldsAvailableForCond(&cond_item, build_tables,
                                         probe_tables))
        return false;
    }
    return true;
  } else {
    table_map used_tables = item->used_tables();
    return (Overlaps(used_tables, build_tables) &&
            Overlaps(used_tables, probe_tables) &&
            IsSubset(used_tables, build_tables | probe_tables));
  }
}

// Determine if a join condition attached to a table needs to be handled by the
// hash join iterator created for that table, or if it needs to be moved up to
// where the semijoin iterator is created (if there is more than one table on
// the inner side of a semijoin).

// If the fields in the condition are available within the join between the
// inner tables, we attach the condition to the current table. Otherwise, we
// attach it to the table where the semijoin iterator will be created.
static void AttachSemiJoinCondition(Item *join_cond,
                                    vector<PendingCondition> *join_conditions,
                                    QEP_TAB *current_table,
                                    qep_tab_map left_tables,
                                    plan_idx semi_join_table_idx) {
  table_map build_table_map = ConvertQepTabMapToTableMap(
      current_table->join(), current_table->idx_map());
  table_map probe_table_map =
      ConvertQepTabMapToTableMap(current_table->join(), left_tables);
  if (CheckIfFieldsAvailableForCond(join_cond, build_table_map,
                                    probe_table_map)) {
    join_conditions->push_back(
        PendingCondition{join_cond, current_table->idx()});
  } else {
    join_conditions->push_back(
        PendingCondition{join_cond, semi_join_table_idx});
  }
}

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

Special case:
    If we are on the inner side of a semijoin with only one table, any
    condition attached to this table is lifted up to where the semijoin
    iterator would be created. If we have more than one table on the inner
    side of a semijoin, and if conditions attached to these tables are
    lifted up to the semijoin iterator, we do not create good plans.
    Therefore, for such a case, we take special care to try and attach
    the condition to the correct hash join iterator. To do the same, we
    find if the fields in a join condition are available within the join
    created for the current table. If the fields are available, we attach the
    condition to the hash join iterator created for the current table.
    We make use of "semi_join_table_idx" to know where the semijoin iterator
    would be created and "left_tables" to know the tables that are available
    for the join that will be created for the current table.
    Note that, as of now, for mysql, we do not enable join buffering thereby
    not enabling hash joins when a semijoin has more than one table on
    its inner side. However, we enable it for secondary engines.

  TODO: The optimizer should distinguish between before-join and
  after-join conditions to begin with, instead of us having to untangle
  it here.
 */
void SplitConditions(Item *condition, QEP_TAB *current_table,
                     vector<Item *> *predicates_below_join,
                     vector<PendingCondition> *predicates_above_join,
                     vector<PendingCondition> *join_conditions,
                     plan_idx semi_join_table_idx, qep_tab_map left_tables) {
  Mem_root_array<Item *> condition_parts(*THR_MALLOC);
  ExtractConditions(condition, &condition_parts);
  for (Item *item : condition_parts) {
    Item_func_trig_cond *trig_cond = GetTriggerCondOrNull(item);
    if (trig_cond != nullptr) {
      Item *inner_cond = trig_cond->arguments()[0];
      if (trig_cond->get_trig_type() == Item_func_trig_cond::FOUND_MATCH) {
        // A WHERE predicate on the table that needs to be pushed up above the
        // join (case #3 above).
        predicates_above_join->push_back(
            PendingCondition{inner_cond, trig_cond->idx()});
      } else if (trig_cond->get_trig_type() ==
                 Item_func_trig_cond::IS_NOT_NULL_COMPL) {
        // It's a join condition, so it should nominally go directly onto the
        // table. If it _also_ has a FOUND_MATCH predicate, we are dealing
        // with case #4 above, and need to push it up to exactly the right
        // spot.
        //
        // There is a special exception here for antijoins; see the code under
        // qep_tab->table()->reginfo.not_exists_optimize in ConnectJoins().
        Item_func_trig_cond *inner_trig_cond = GetTriggerCondOrNull(inner_cond);
        if (inner_trig_cond != nullptr) {
          // Note that we can have a condition inside multiple levels of a
          // trigger condition. We want the innermost condition, as we really do
          // not care about trigger conditions after this point.
          Item *inner_inner_cond = GetInnermostCondition(inner_trig_cond);
          if (join_conditions != nullptr) {
            // If join_conditions is set, it indicates that we are on the right
            // side of an outer join that will be executed using hash join. The
            // condition must be moved to the point where the hash join iterator
            // is created, so the condition can be attached to the iterator.
            join_conditions->push_back(
                PendingCondition{inner_inner_cond, trig_cond->idx()});
          } else {
            predicates_above_join->push_back(
                PendingCondition{inner_inner_cond, inner_trig_cond->idx()});
          }
        } else {
          if (join_conditions != nullptr) {
            // Similar to the left join above: If join_conditions is set,
            // it indicates that we are on the inner side of an antijoin (we are
            // dealing with the NOT IN side in the below example), and the
            // antijoin will be executed using hash join:
            //
            //   SELECT * FROM t1 WHERE t1.col1 NOT IN (SELECT t2.col1 FROM t2);
            //
            // In this case, the condition must be moved up to the outer side
            // where the hash join iterator is created, so it can be attached
            // to the iterator.
            if (semi_join_table_idx == NO_PLAN_IDX) {
              join_conditions->push_back(
                  PendingCondition{inner_cond, trig_cond->idx()});
            }
            // Or, we might be on the inner side of a semijoin. In this case,
            // we move the condition to where the semijoin hash iterator is
            // created. However if we have more than one table on the inner
            // side of the semijoin, then we first check if it can be attached
            // to the hash join iterator of the inner join (provided the fields
            // in the condition are available within the join). If not, move it
            // upto where semijoin hash iterator is created.
            else if (current_table->idx() == semi_join_table_idx) {
              join_conditions->push_back(
                  PendingCondition{inner_cond, semi_join_table_idx});
            } else {
              AttachSemiJoinCondition(inner_cond, join_conditions,
                                      current_table, left_tables,
                                      semi_join_table_idx);
            }
          } else {
            predicates_below_join->push_back(inner_cond);
          }
        }
      } else {
        predicates_below_join->push_back(item);
      }
    } else {
      if (join_conditions != nullptr && IsJoinCondition(item, current_table) &&
          semi_join_table_idx != NO_PLAN_IDX) {
        // We are on the inner side of a semijoin, and the item we are
        // looking at is a join condition. In addition, the join will be
        // executed using hash join. Move the condition up where the hash join
        // iterator is created.
        // If we have only one table on the inner side of a semijoin,
        // we attach the condition to the semijoin iterator.
        if (current_table->idx() == semi_join_table_idx) {
          join_conditions->push_back(
              PendingCondition{item, semi_join_table_idx});
        } else {
          // In case we have more than one table on the inner side of a
          // semijoin, conditions will be attached to the inner hash join
          // iterator only if the fields present in the condition are
          // available within the join. Else, condition is moved up to where
          // the semijoin hash iterator is created.
          AttachSemiJoinCondition(item, join_conditions, current_table,
                                  left_tables, semi_join_table_idx);
        }
      } else {
        // All other conditions (both join condition and filters) will be looked
        // at while creating the iterator for this table.
        predicates_below_join->push_back(item);
      }
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
  assert(weedout_start >= 0);
  assert(weedout_end >= 0);

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

static AccessPath *CreateWeedoutOrLimitAccessPath(THD *thd, AccessPath *path,
                                                  SJ_TMP_TABLE *weedout_table) {
  if (weedout_table->is_confluent) {
    // A “confluent” weedout is one that deduplicates on all the
    // fields. If so, we can drop the complexity of the WeedoutIterator
    // and simply insert a LIMIT 1.
    return NewLimitOffsetAccessPath(thd, path,
                                    /*limit=*/1, /*offset=*/0,
                                    /*count_all_rows=*/false,
                                    /*reject_multiple_rows=*/false,
                                    /*send_records_override=*/nullptr);
  } else {
    AccessPath *weedout_path = NewWeedoutAccessPath(thd, path, weedout_table);
    FindTablesToGetRowidFor(weedout_path);
    return weedout_path;
  }
}

static AccessPath *NewWeedoutAccessPathForTables(
    THD *thd, const qep_tab_map tables_to_deduplicate, QEP_TAB *qep_tabs,
    uint primary_tables, AccessPath *path) {
  Prealloced_array<SJ_TMP_TABLE_TAB, MAX_TABLES> sj_tabs(PSI_NOT_INSTRUMENTED);
  for (uint i = 0; i < primary_tables; ++i) {
    if (!ContainsTable(tables_to_deduplicate, i)) {
      SJ_TMP_TABLE_TAB sj_tab;
      sj_tab.qep_tab = &qep_tabs[i];
      sj_tabs.push_back(sj_tab);

      // See JOIN::add_sorting_to_table() for rationale.
      Filesort *filesort = qep_tabs[i].filesort;
      if (filesort != nullptr) {
        if (filesort->m_sort_param.m_addon_fields_status !=
            Addon_fields_status::unknown_status) {
          // This can happen in the exceptional case that there's an extra
          // weedout added after-the-fact due to nonhierarchical weedouts
          // (see FindSubstructure for details). Note that our caller will
          // call FindTablesToGetRowidFor() if needed, which should overwrite
          // the previous (now wrong) decision there.
          filesort->clear_addon_fields();
        }
        filesort->m_force_sort_rowids = true;
        // Since we changed our mind about whether the SORT path below us should
        // use row IDs, update it to make EXPLAIN display correct information.
        WalkAccessPaths(path, /*join=*/nullptr,
                        WalkAccessPathPolicy::STOP_AT_MATERIALIZATION,
                        [filesort](AccessPath *subpath, const JOIN *) {
                          if (subpath->type == AccessPath::SORT &&
                              subpath->sort().filesort == filesort) {
                            subpath->sort().force_sort_rowids = true;
                            return true;
                          }
                          return false;
                        });
      }
    }
  }

  JOIN *join = qep_tabs[0].join();
  SJ_TMP_TABLE *sjtbl =
      create_sj_tmp_table(thd, join, &sj_tabs[0], &sj_tabs[0] + sj_tabs.size());
  return CreateWeedoutOrLimitAccessPath(thd, path, sjtbl);
}

enum class Substructure { NONE, OUTER_JOIN, SEMIJOIN, WEEDOUT };

/**
  Given a range of tables (where we assume that we've already handled
  first_idx..(this_idx-1) as inner joins), figure out whether this is a
  semijoin, an outer join or a weedout. In general, the outermost structure
  wins; if we are in one of the rare cases where there are e.g. coincident
  (first match) semijoins and weedouts, we do various forms of conflict
  resolution:

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

  *add_limit_1 = false;
  if (is_outer_join && is_weedout) {
    if (outer_join_end > weedout_end) {
      // Weedout will be handled at a lower recursion level.
      is_weedout = false;
    } else {
      if (qep_tab->flush_weedout_table->is_confluent) {
        // We have the case where the right side of an outer join is a confluent
        // weedout. The weedout will return at most one row, so replace the
        // weedout with LIMIT 1.
        *add_limit_1 = true;
      } else {
        // See comment above.
        MarkUnhandledDuplicates(qep_tab->flush_weedout_table, this_idx,
                                weedout_end, unhandled_duplicates);
      }
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
  if (is_semijoin && is_outer_join) {
    if (semijoin_end == outer_join_end) {
      *add_limit_1 = true;
      is_semijoin = false;
    } else if (semijoin_end > outer_join_end) {
      // A special case of the special case; there might be more than one
      // outer join contained in this semijoin, e.g. A LEFT JOIN B LEFT JOIN C
      // where the combination B-C is _also_ the right side of a semijoin.
      // The join optimizer should not produce this.
      assert(false);
    }
  }

  // Yet another special case like the above; this is when we have a semijoin
  // and then a partially overlapping outer join that ends outside the semijoin.
  // E.g., A JOIN B JOIN C LEFT JOIN D, where A..C denotes a semijoin
  // (C has first match back to A). Verify that it cannot happen.
  if (is_semijoin) {
    for (plan_idx i = this_idx; i < semijoin_end; ++i) {
      assert(qep_tabs[i].last_inner() < semijoin_end);
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
    assert(outer_join_end > semijoin_end);
    is_semijoin = false;
  }

  assert(is_semijoin + is_outer_join + is_weedout <= 1);

  if (is_semijoin) {
    *substructure_end = semijoin_end;
    return Substructure::SEMIJOIN;
  }
  if (is_outer_join) {
    *substructure_end = outer_join_end;
    return Substructure::OUTER_JOIN;
  }
  if (is_weedout) {
    *substructure_end = weedout_end;
    return Substructure::WEEDOUT;
  }
  *substructure_end = NO_PLAN_IDX;  // Not used.
  return Substructure::NONE;
}

static bool IsTableScan(AccessPath *path) {
  if (path->type == AccessPath::FILTER) {
    return IsTableScan(path->filter().child);
  }
  return path->type == AccessPath::TABLE_SCAN;
}

AccessPath *GetAccessPathForDerivedTable(THD *thd, QEP_TAB *qep_tab,
                                         AccessPath *table_path) {
  return GetAccessPathForDerivedTable(
      thd, qep_tab->table_ref, qep_tab->table(), qep_tab->rematerialize,
      qep_tab->invalidators, /*need_rowid=*/false, table_path);
}

/**
   Recalculate the cost of 'path'.
   @param path the access path for which we update the cost numbers.
   @param outer_query_block the query block to which 'path belongs.
*/
static void RecalculateTablePathCost(AccessPath *path,
                                     const Query_block &outer_query_block) {
  switch (path->type) {
    case AccessPath::FILTER: {
      const AccessPath &child = *path->filter().child;
      path->set_num_output_rows(child.num_output_rows());
      path->init_cost = child.init_cost;

      const FilterCost filterCost =
          EstimateFilterCost(current_thd, path->num_output_rows(),
                             path->filter().condition, &outer_query_block);

      path->cost = child.cost + (path->filter().materialize_subqueries
                                     ? filterCost.cost_if_materialized
                                     : filterCost.cost_if_not_materialized);
    } break;

    case AccessPath::SORT:
      EstimateSortCost(path);
      break;

    case AccessPath::LIMIT_OFFSET:
      EstimateLimitOffsetCost(path);
      break;

    case AccessPath::DELETE_ROWS:
      EstimateDeleteRowsCost(path);
      break;

    case AccessPath::UPDATE_ROWS:
      EstimateUpdateRowsCost(path);
      break;

    case AccessPath::STREAM:
      EstimateStreamCost(path);
      break;

    case AccessPath::MATERIALIZE:
      EstimateMaterializeCost(current_thd, path);
      break;

    default:
      assert(false);
  }
}

AccessPath *MoveCompositeIteratorsFromTablePath(
    AccessPath *path, const Query_block &outer_query_block) {
  assert(path->cost >= 0.0);
  AccessPath *table_path = path->materialize().table_path;
  AccessPath *bottom_of_table_path = nullptr;
  // For EXPLAIN, we recalculate the cost to reflect the new order of
  // AccessPath objects.
  const bool explain = current_thd->lex->is_explain();
  Prealloced_array<AccessPath *, 4> ancestor_paths{PSI_NOT_INSTRUMENTED};

  const auto scan_functor = [&bottom_of_table_path, &ancestor_paths, path,
                             explain](AccessPath *sub_path, const JOIN *) {
    switch (sub_path->type) {
      case AccessPath::TABLE_SCAN:
      case AccessPath::REF:
      case AccessPath::REF_OR_NULL:
      case AccessPath::EQ_REF:
      case AccessPath::ALTERNATIVE:
      case AccessPath::CONST_TABLE:
      case AccessPath::INDEX_SCAN:
      case AccessPath::INDEX_RANGE_SCAN:
        // We found our real bottom.
        path->materialize().table_path = sub_path;
        if (explain) {
          EstimateMaterializeCost(current_thd, path);
        }
        return true;
      default:
        // New possible bottom, so keep going.
        bottom_of_table_path = sub_path;
        ancestor_paths.push_back(sub_path);
        return false;
    }
  };
  WalkAccessPaths(table_path, /*join=*/nullptr,
                  WalkAccessPathPolicy::ENTIRE_TREE, scan_functor);
  if (bottom_of_table_path != nullptr) {
    if (bottom_of_table_path->type == AccessPath::ZERO_ROWS) {
      // There's nothing to materialize for ZERO_ROWS, so we can drop the
      // entire MATERIALIZE node.
      return bottom_of_table_path;
    }
    if (explain) {
      EstimateMaterializeCost(current_thd, path);
    }

    // This isn't strictly accurate, but helps propagate information
    // better throughout the tree nevertheless.
    CopyBasicProperties(*path, table_path);

    switch (bottom_of_table_path->type) {
      case AccessPath::FILTER:
        bottom_of_table_path->filter().child = path;
        break;
      case AccessPath::SORT:
        bottom_of_table_path->sort().child = path;
        break;
      case AccessPath::LIMIT_OFFSET:
        bottom_of_table_path->limit_offset().child = path;
        break;
      case AccessPath::DELETE_ROWS:
        bottom_of_table_path->delete_rows().child = path;
        break;
      case AccessPath::UPDATE_ROWS:
        bottom_of_table_path->update_rows().child = path;
        break;

      // It's a bit odd to have STREAM and MATERIALIZE nodes
      // inside table_path, but it happens when we have UNION with
      // with ORDER BY on nondeterministic predicates, or INSERT
      // which requires buffering. It should be safe move it
      // out of table_path nevertheless.
      case AccessPath::STREAM:
        bottom_of_table_path->stream().child = path;
        break;
      case AccessPath::MATERIALIZE:
        assert(bottom_of_table_path->materialize().param->query_blocks.size() ==
               1);
        bottom_of_table_path->materialize()
            .param->query_blocks[0]
            .subquery_path = path;
        break;
      default:
        assert(false);
    }

    path = table_path;
  }

  if (explain) {
    // Update cost from the bottom an up, so that the cost of each path
    // includes the cost of its descendants.
    for (auto ancestor = ancestor_paths.end() - 1;
         ancestor >= ancestor_paths.begin(); ancestor--) {
      RecalculateTablePathCost(*ancestor, outer_query_block);
    }
  }

  return path;
}

AccessPath *GetAccessPathForDerivedTable(
    THD *thd, Table_ref *table_ref, TABLE *table, bool rematerialize,
    Mem_root_array<const AccessPath *> *invalidators, bool need_rowid,
    AccessPath *table_path) {
  if (table_ref->access_path_for_derived != nullptr) {
    return table_ref->access_path_for_derived;
  }

  Query_expression *query_expression = table_ref->derived_query_expression();
  JOIN *subjoin = nullptr;
  Temp_table_param *tmp_table_param;
  int select_number;

  // If we have a single query block at the end of the QEP_TAB array,
  // it may contain aggregation that have already set up fields and items
  // to copy, and we need to pass those to MaterializeIterator, so reuse its
  // tmp_table_param. If not, make a new object, so that we don't
  // disturb the materialization going on inside our own query block.
  if (query_expression->is_simple()) {
    subjoin = query_expression->first_query_block()->join;
    select_number = query_expression->first_query_block()->select_number;
    tmp_table_param = &subjoin->tmp_table_param;
  } else if (query_expression->set_operation()->m_is_materialized) {
    // NOTE: subjoin here is never used, as ConvertItemsToCopy only uses it
    // for ROLLUP, and simple table can't have ROLLUP.
    Query_block *const qb = query_expression->set_operation()->query_block();
    subjoin = qb->join;
    tmp_table_param = &subjoin->tmp_table_param;
    select_number = qb->select_number;
  } else {
    tmp_table_param = new (thd->mem_root) Temp_table_param;
    select_number = query_expression->first_query_block()->select_number;
  }
  ConvertItemsToCopy(*query_expression->get_field_list(),
                     table->visible_field_ptr(), tmp_table_param);

  AccessPath *path;

  if (query_expression->unfinished_materialization()) {
    // The query expression is a UNION capable of materializing directly into
    // our result table. This saves us from doing double materialization (first
    // into a UNION result table, then from there into our own).
    //
    // We will already have set up a unique index on the table if
    // required; see Table_ref::setup_materialized_derived_tmp_table().
    path = NewMaterializeAccessPath(
        thd, query_expression->release_query_blocks_to_materialize(),
        invalidators, table, table_path, table_ref->common_table_expr(),
        query_expression,
        /*ref_slice=*/-1, rematerialize, query_expression->select_limit_cnt,
        query_expression->offset_limit_cnt == 0
            ? query_expression->m_reject_multiple_rows
            : false);
    EstimateMaterializeCost(thd, path);
    path = MoveCompositeIteratorsFromTablePath(
        path, *query_expression->outer_query_block());
    if (query_expression->offset_limit_cnt != 0) {
      // LIMIT is handled inside MaterializeIterator, but OFFSET is not.
      // SQL_CALC_FOUND_ROWS cannot occur in a derived table's definition.
      path = NewLimitOffsetAccessPath(
          thd, path, query_expression->select_limit_cnt,
          query_expression->offset_limit_cnt,
          /*count_all_rows=*/false, query_expression->m_reject_multiple_rows,
          /*send_records_override=*/nullptr);
    }
  } else if (table_ref->common_table_expr() == nullptr && rematerialize &&
             IsTableScan(table_path) && !need_rowid) {
    // We don't actually need the materialization for anything (we would
    // just be reading the rows straight out from the table, never to be used
    // again), so we can just stream records directly over to the next
    // iterator. This saves both CPU time and memory (for the temporary
    // table).
    //
    // NOTE: Currently, rematerialize is true only for JSON_TABLE. (In the
    // hypergraph optimizer, it is also true for lateral derived tables.)
    // We could extend this to other situations, such as the leftmost
    // table of the join (assuming nested loop only). The test for CTEs is
    // also conservative; if the CTE is defined within this join and used
    // only once, we could still stream without losing performance.
    path = NewStreamingAccessPath(thd, query_expression->root_access_path(),
                                  subjoin, &subjoin->tmp_table_param, table,
                                  /*ref_slice=*/-1);
    CopyBasicProperties(*query_expression->root_access_path(), path);
    path->ordering_state = 0;  // Different query block, so ordering is reset.
  } else {
    JOIN *join = query_expression->is_set_operation()
                     ? nullptr
                     : query_expression->first_query_block()->join;
    path = NewMaterializeAccessPath(
        thd,
        SingleMaterializeQueryBlock(thd, query_expression->root_access_path(),
                                    select_number, join,
                                    /*copy_items=*/true, tmp_table_param),
        invalidators, table, table_path, table_ref->common_table_expr(),
        query_expression,
        /*ref_slice=*/-1, rematerialize, tmp_table_param->end_write_records,
        query_expression->m_reject_multiple_rows);
    EstimateMaterializeCost(thd, path);
    path = MoveCompositeIteratorsFromTablePath(
        path, *query_expression->outer_query_block());
  }

  path->cost_before_filter = path->cost;
  path->num_output_rows_before_filter = path->num_output_rows();

  table_ref->access_path_for_derived = path;
  return path;
}

/**
  Get the RowIterator used for scanning the given table, with any required
  materialization operations done first.
 */
AccessPath *GetTableAccessPath(THD *thd, QEP_TAB *qep_tab, QEP_TAB *qep_tabs) {
  AccessPath *table_path;
  if (qep_tab->materialize_table == QEP_TAB::MATERIALIZE_DERIVED) {
    table_path =
        GetAccessPathForDerivedTable(thd, qep_tab, qep_tab->access_path());
  } else if (qep_tab->materialize_table ==
             QEP_TAB::MATERIALIZE_TABLE_FUNCTION) {
    table_path = NewMaterializedTableFunctionAccessPath(
        thd, qep_tab->table(), qep_tab->table_ref->table_function,
        qep_tab->access_path());
  } else if (qep_tab->materialize_table == QEP_TAB::MATERIALIZE_SEMIJOIN) {
    Semijoin_mat_exec *sjm = qep_tab->sj_mat_exec();

    // create_tmp_table() has already filled sjm->table_param.items_to_copy.
    // However, the structures there are not used by
    // join_materialize_semijoin, and don't have e.g. result fields set up
    // correctly, so we just clear it and create our own.
    sjm->table_param.items_to_copy = nullptr;
    ConvertItemsToCopy(sjm->sj_nest->nested_join->sj_inner_exprs,
                       qep_tab->table()->visible_field_ptr(),
                       &sjm->table_param);

    int join_start = sjm->inner_table_index;
    int join_end = join_start + sjm->table_count;

    // Handle this subquery as a we would a completely separate join,
    // even though the tables are part of the same JOIN object
    // (so in effect, a “virtual join”).
    qep_tab_map unhandled_duplicates = 0;
    table_map conditions_depend_on_outer_tables = 0;
    vector<PendingInvalidator> pending_invalidators;
    AccessPath *subtree_path = ConnectJoins(
        /*upper_first_idx=*/NO_PLAN_IDX, join_start, join_end, qep_tabs, thd,
        TOP_LEVEL,
        /*pending_conditions=*/nullptr, &pending_invalidators,
        /*pending_join_conditions=*/nullptr, &unhandled_duplicates,
        &conditions_depend_on_outer_tables);

    // If there were any weedouts that we had to drop during ConnectJoins()
    // (ie., the join left some tables that were supposed to be deduplicated
    // but were not), handle them now at the end of the virtual join.
    if (unhandled_duplicates != 0) {
      subtree_path = NewWeedoutAccessPathForTables(
          thd, unhandled_duplicates, qep_tab, qep_tab->join()->primary_tables,
          subtree_path);
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
    for (Item *item : sjm->sj_nest->nested_join->sj_inner_exprs) {
      if (item->is_nullable()) {
        Item *condition = new Item_func_isnotnull(item);
        condition->quick_fix_field();
        condition->update_used_tables();
        condition->apply_is_true();
        not_null_conditions.push_back(condition);
      }
    }
    subtree_path = PossiblyAttachFilter(subtree_path, not_null_conditions, thd,
                                        &conditions_depend_on_outer_tables);

    bool copy_items_in_materialize =
        true;  // We never have windowing functions within semijoins.
    table_path = NewMaterializeAccessPath(
        thd,
        SingleMaterializeQueryBlock(
            thd, subtree_path, qep_tab->join()->query_block->select_number,
            qep_tab->join(), copy_items_in_materialize, &sjm->table_param),
        qep_tab->invalidators, qep_tab->table(), qep_tab->access_path(),
        /*cte=*/nullptr,
        /*query_expression=*/nullptr,
        /*ref_slice=*/-1, qep_tab->rematerialize,
        sjm->table_param.end_write_records,
        /*reject_multiple_rows=*/false);
    EstimateMaterializeCost(thd, table_path);

#ifndef NDEBUG
    // Make sure we clear this table out when the join is reset,
    // since its contents may depend on outer expressions.
    bool found = false;
    for (TABLE &sj_tmp_tab : qep_tab->join()->sj_tmp_tables) {
      if (&sj_tmp_tab == qep_tab->table()) {
        found = true;
        break;
      }
    }
    assert(found);
#endif
  } else {
    table_path = qep_tab->access_path();

    // See if this is an information schema table that must be filled in before
    // we scan.
    if (qep_tab->table_ref->schema_table &&
        qep_tab->table_ref->schema_table->fill_table) {
      table_path = NewMaterializeInformationSchemaTableAccessPath(
          thd, table_path, qep_tab->table_ref, qep_tab->condition());
    }
  }
  return table_path;
}

void SetCostOnTableAccessPath(const Cost_model_server &cost_model,
                              const POSITION *pos, bool is_after_filter,
                              AccessPath *path) {
  double num_rows_after_filtering = pos->rows_fetched * pos->filter_effect;
  if (is_after_filter) {
    path->set_num_output_rows(num_rows_after_filtering);
  } else {
    path->set_num_output_rows(pos->rows_fetched);
  }

  // Note that we don't try to adjust for the filtering here;
  // we estimate the same cost as the table itself.
  double cost =
      pos->read_cost + cost_model.row_evaluate_cost(num_rows_after_filtering);
  if (pos->prefix_rowcount <= 0.0) {
    path->cost = cost;
  } else {
    // Scale the estimated cost to being for one loop only, to match the
    // measured costs.
    path->cost = cost * num_rows_after_filtering / pos->prefix_rowcount;
  }
}

void SetCostOnNestedLoopAccessPath(const Cost_model_server &cost_model,
                                   const POSITION *pos_inner,
                                   AccessPath *path) {
  if (pos_inner == nullptr) {
    // No cost information.
    return;
  }

  AccessPath *outer, *inner;
  if (path->type == AccessPath::NESTED_LOOP_SEMIJOIN_WITH_DUPLICATE_REMOVAL) {
    outer = path->nested_loop_semijoin_with_duplicate_removal().outer;
    inner = path->nested_loop_semijoin_with_duplicate_removal().inner;
  } else {
    assert(path->type == AccessPath::NESTED_LOOP_JOIN);
    outer = path->nested_loop_join().outer;
    inner = path->nested_loop_join().inner;
  }

  if (outer->num_output_rows() == -1.0 || inner->num_output_rows() == -1.0) {
    // Missing cost information on at least one child.
    return;
  }

  // Mirrors set_prefix_join_cost(), even though the cost calculation doesn't
  // make a lot of sense.
  double inner_expected_rows_before_filter =
      pos_inner->filter_effect > 0.0
          ? (inner->num_output_rows() / pos_inner->filter_effect)
          : 0.0;
  double joined_rows =
      outer->num_output_rows() * inner_expected_rows_before_filter;
  path->set_num_output_rows(joined_rows * pos_inner->filter_effect);
  path->cost = outer->cost + pos_inner->read_cost +
               cost_model.row_evaluate_cost(joined_rows);
}

void SetCostOnHashJoinAccessPath(const Cost_model_server &cost_model,
                                 const POSITION *pos_outer, AccessPath *path) {
  if (pos_outer == nullptr) {
    // No cost information.
    return;
  }

  AccessPath *outer = path->hash_join().outer;
  AccessPath *inner = path->hash_join().inner;

  if (outer->num_output_rows() == -1.0 || inner->num_output_rows() == -1.0) {
    // Missing cost information on at least one child.
    return;
  }

  // Mirrors set_prefix_join_cost(), even though the cost calculation doesn't
  // make a lot of sense.
  double joined_rows = outer->num_output_rows() * inner->num_output_rows();
  path->set_num_output_rows(joined_rows * pos_outer->filter_effect);
  path->cost = inner->cost + pos_outer->read_cost +
               cost_model.row_evaluate_cost(joined_rows);
}

static bool ConditionIsAlwaysTrue(Item *item) {
  return item->const_item() && item->val_bool();
}

// Returns true if the item refers to only one side of the join. This is used to
// determine whether an equi-join conditions need to be attached as an "extra"
// condition (pure join conditions must refer to both sides of the join).
static bool ItemRefersToOneSideOnly(Item *item, table_map left_side,
                                    table_map right_side) {
  item->update_used_tables();
  const table_map item_used_tables = item->used_tables();

  if ((left_side & item_used_tables) == 0 ||
      (right_side & item_used_tables) == 0) {
    return true;
  }
  return false;
}

// Create a hash join iterator with the given build and probe input. We will
// move conditions from the argument "join_conditions" into two separate lists;
// one list for equi-join conditions that will be used as normal join conditions
// in hash join, and one list for non-equi-join conditions that will be attached
// as "extra" conditions in hash join. The "extra" conditions are conditions
// that must be evaluated after the hash table lookup, but _before_ returning a
// row. Conditions that are not moved will be attached as filters after the
// join. Note that we only attach conditions as "extra" conditions if the join
// type is not inner join. This gives us more fine-grained output from EXPLAIN
// ANALYZE, where we can see whether the condition was expensive.
// This information is lost when we attach conditions as extra conditions inside
// hash join.
//
// The function will also determine whether hash join is allowed to spill to
// disk. In general, we reject spill to disk if the query has a LIMIT and no
// aggregation or grouping. See comments inside the function for justification.
static AccessPath *CreateHashJoinAccessPath(
    THD *thd, QEP_TAB *qep_tab, AccessPath *build_path,
    qep_tab_map build_tables, AccessPath *probe_path, qep_tab_map probe_tables,
    JoinType join_type, vector<Item *> *join_conditions,
    table_map *conditions_depend_on_outer_tables) {
  table_map left_table_map =
      ConvertQepTabMapToTableMap(qep_tab->join(), probe_tables);
  table_map right_table_map =
      ConvertQepTabMapToTableMap(qep_tab->join(), build_tables);

  // Move out equi-join conditions and non-equi-join conditions, so we can
  // attach them as join condition and extra conditions in hash join.
  vector<HashJoinCondition> hash_join_conditions;
  vector<Item *> hash_join_extra_conditions;

  for (Item *outer_item : *join_conditions) {
    // We can encounter conditions that are AND'ed together (i.e. a condition
    // that originally was Item_cond_and inside a Item_trig_cond).
    Mem_root_array<Item *> condition_parts(thd->mem_root);
    ExtractConditions(outer_item, &condition_parts);
    for (Item *inner_item : condition_parts) {
      if (ConditionIsAlwaysTrue(inner_item)) {
        // The optimizer may leave conditions that are always 'true'. These have
        // no effect on the query, so we ignore them. Ideally, the optimizer
        // should not attach these conditions in the first place.
        continue;
      }

      // See if this is an equi-join condition.
      if (inner_item->type() == Item::FUNC_ITEM ||
          inner_item->type() == Item::COND_ITEM) {
        Item_func *func_item = down_cast<Item_func *>(inner_item);

        if (func_item->functype() == Item_func::EQ_FUNC) {
          bool found = false;
          down_cast<Item_func_eq *>(func_item)
              ->ensure_multi_equality_fields_are_available(
                  left_table_map, right_table_map, /*replace=*/true, &found);
        }

        if (func_item->contains_only_equi_join_condition() &&
            !ItemRefersToOneSideOnly(func_item, left_table_map,
                                     right_table_map)) {
          Item_eq_base *join_condition = down_cast<Item_eq_base *>(func_item);
          // Join conditions with items that returns row values (subqueries or
          // row value expression) are set up with multiple child comparators,
          // one for each column in the row. As long as the row contains only
          // one column, use it as a join condition. If it has more than one
          // column, attach it as an extra condition. Note that join conditions
          // that does not return row values are not set up with any child
          // comparators, meaning that get_child_comparator_count() will return
          // 0.
          if (join_condition->get_comparator()->get_child_comparator_count() <
              2) {
            // Make a hash join condition for this equality comparison.
            // This may entail allocating type cast nodes; see the comments
            // on HashJoinCondition for more details.
            hash_join_conditions.emplace_back(join_condition, thd->mem_root);
            continue;
          }
        }
      }
      // It was not.
      hash_join_extra_conditions.push_back(inner_item);
    }
  }

  // For any conditions for which HashJoinCondition decided only to store the
  // hash in the key, we need to re-check.
  for (const HashJoinCondition &cond : hash_join_conditions) {
    if (!cond.store_full_sort_key()) {
      hash_join_extra_conditions.push_back(cond.join_condition());
    }
  }

  if (join_type == JoinType::INNER) {
    // For inner join, attach the extra conditions as filters after the join.
    // This gives us more detailed output in EXPLAIN ANALYZE since we get an
    // instrumented FilterIterator on top of the join.
    *join_conditions = std::move(hash_join_extra_conditions);
  } else {
    join_conditions->clear();

    // The join condition could contain conditions that can be pushed down into
    // the right side, e.g. “t1 LEFT JOIN t2 ON t2.x > 3” (or simply
    // “ON FALSE”). For inner joins, the optimizer will have pushed these down
    // to the right tables, but it is not capable of doing so for outer joins.
    // As a band-aid, we identify these and push them down onto the build
    // iterator. This isn't ideal (they will not e.g. give rise to index
    // lookups, and if there are multiple tables, we don't push the condition
    // as far down as we should), but it should give reasonable speedups for
    // many common cases.
    vector<Item *> build_conditions;
    for (auto cond_it = hash_join_extra_conditions.begin();
         cond_it != hash_join_extra_conditions.end();) {
      Item *cond = *cond_it;
      if ((cond->used_tables() & (left_table_map | RAND_TABLE_BIT)) == 0) {
        build_conditions.push_back(cond);
        cond_it = hash_join_extra_conditions.erase(cond_it);
      } else {
        *conditions_depend_on_outer_tables |= cond->used_tables();
        ++cond_it;
      }
    }
    build_path = PossiblyAttachFilter(build_path, build_conditions, thd,
                                      conditions_depend_on_outer_tables);
  }

  // If we have a degenerate semijoin or antijoin (ie., no join conditions),
  // we only need a single row from the inner side.
  if ((join_type == JoinType::SEMI || join_type == JoinType::ANTI) &&
      hash_join_conditions.empty() && hash_join_extra_conditions.empty()) {
    build_path = NewLimitOffsetAccessPath(thd, build_path,
                                          /*limit=*/1, /*offset=*/0,
                                          /*count_all_rows=*/false,
                                          /*reject_multiple_rows=*/false,
                                          /*send_records_override=*/nullptr);
  }

  const JOIN *join = qep_tab->join();
  const bool has_grouping = join->implicit_grouping || join->grouped;

  const bool has_limit = join->m_select_limit != HA_POS_ERROR;

  const bool has_order_by = join->order.order != nullptr;

  // If we have a limit in the query, do not allow hash join to spill to
  // disk. The effect of this is that hash join will start producing
  // result rows a lot earlier, and thus hit the LIMIT a lot sooner.
  // Ideally, this should be decided during optimization.
  // There are however two situations where we always allow spill to disk,
  // and that is if we either have grouping or sorting in the query. In
  // those cases, the iterator above us will most likely consume the
  // entire result set anyways.
  const bool allow_spill_to_disk = !has_limit || has_grouping || has_order_by;

  RelationalExpression *expr = new (thd->mem_root) RelationalExpression(thd);
  expr->left = expr->right =
      nullptr;  // Only used in the hypergraph join optimizer.
  switch (join_type) {
    case JoinType::ANTI:
      expr->type = RelationalExpression::ANTIJOIN;
      break;
    case JoinType::INNER:
      expr->type = RelationalExpression::INNER_JOIN;
      break;
    case JoinType::OUTER:
      expr->type = RelationalExpression::LEFT_JOIN;
      break;
    case JoinType::SEMI:
      expr->type = RelationalExpression::SEMIJOIN;
      break;
    case JoinType::FULL_OUTER:
      expr->type = RelationalExpression::FULL_OUTER_JOIN;
      break;
  }
  for (Item *item : hash_join_extra_conditions) {
    expr->join_conditions.push_back(item);
  }
  for (const HashJoinCondition &condition : hash_join_conditions) {
    expr->equijoin_conditions.push_back(condition.join_condition());
  }

  // Go through the equijoin conditions and check that all of them still
  // refer to tables that exist. If some table was pruned away due to
  // being replaced by ZeroRowsAccessPath, but the equijoin condition still
  // refers to it, it could become degenerate: The only rows it could ever
  // see would be NULL-complemented rows, which would never match.
  // In this case, we can remove the entire build path (ie., propagate the
  // zero-row property to our own join).
  //
  // We also remove the join conditions, to avoid using time on extracting their
  // hash values. (Also, Item_eq_base::append_join_key_for_hash_join has an
  // assert that this case should never happen, so it would trigger.)
  const table_map probe_used_tables =
      GetUsedTableMap(probe_path, /*include_pruned_tables=*/false);
  const table_map build_used_tables =
      GetUsedTableMap(build_path, /*include_pruned_tables=*/false);
  for (const HashJoinCondition &condition : hash_join_conditions) {
    if ((!condition.left_uses_any_table(probe_used_tables) &&
         !condition.right_uses_any_table(probe_used_tables)) ||
        (!condition.left_uses_any_table(build_used_tables) &&
         !condition.right_uses_any_table(build_used_tables))) {
      if (build_path->type != AccessPath::ZERO_ROWS) {
        string cause = "Join condition " +
                       ItemToString(condition.join_condition()) +
                       " requires pruned table";
        build_path = NewZeroRowsAccessPath(
            thd, build_path, strdup_root(thd->mem_root, cause.c_str()));
      }
      expr->equijoin_conditions.clear();
      break;
    }
  }

  JoinPredicate *pred = new (thd->mem_root) JoinPredicate;
  pred->expr = expr;

  AccessPath *path = new (thd->mem_root) AccessPath;
  path->type = AccessPath::HASH_JOIN;
  path->hash_join().outer = probe_path;
  path->hash_join().inner = build_path;
  path->hash_join().join_predicate = pred;
  path->hash_join().allow_spill_to_disk = allow_spill_to_disk;
  // Will be set later if we get a weedout access path as parent.
  path->hash_join().store_rowids = false;
  path->hash_join().rewrite_semi_to_inner = false;
  path->hash_join().tables_to_get_rowid_for = 0;

  SetCostOnHashJoinAccessPath(*thd->cost_model(), qep_tab->position(), path);

  return path;
}

// Move all the join conditions from the vector "predicates" over to the
// vector "join_conditions", while filters are untouched. This is done so that
// we can attach the join conditions directly to the hash join iterator. Further
// separation into equi-join and non-equi-join conditions will be done inside
// CreateHashJoinAccessPath().
static void ExtractJoinConditions(const QEP_TAB *current_table,
                                  vector<Item *> *predicates,
                                  vector<Item *> *join_conditions) {
  vector<Item *> real_predicates;
  for (Item *item : *predicates) {
    if (IsJoinCondition(item, current_table)) {
      join_conditions->emplace_back(item);
    } else {
      real_predicates.emplace_back(item);
    }
  }

  *predicates = std::move(real_predicates);
}

static bool UseHashJoin(QEP_TAB *qep_tab) {
  return qep_tab->op_type == QEP_TAB::OT_BNL;
}

static bool UseBKA(QEP_TAB *qep_tab) {
  if (qep_tab->op_type != QEP_TAB::OT_BKA) {
    // Not BKA.
    return false;
  }

  // Similar to QueryMixesOuterBKAAndBNL(), if we have an outer join BKA
  // that contains multiple tables on the right side, we will not have a
  // left-deep tree, which we cannot handle at this point.
  if (qep_tab->last_inner() != NO_PLAN_IDX &&
      qep_tab->last_inner() != qep_tab->idx()) {
    // More than one table on the right side of an outer join, so not
    // left-deep.
    return false;
  }
  return true;
}

// Having a non-BKA join on the right side of an outer BKA join causes problems
// for the matched-row signaling from MultiRangeRowIterator to BKAIterator;
// rows could be found just fine, but not go through the join filter (and thus
// not be marked as matched in BKAIterator), creating extra NULLs.
//
// The only way this can happen is when we get a hash join on the inside of an
// outer BKA join (otherwise, the join tree will be left-deep). If this
// happens, we simply turn off both BKA and hash join handling for the query;
// it is a very rare situation, and the slowdown should be acceptable.
// (Only turning off BKA helps somewhat, but MultiRangeRowIterator also cannot
// be on the inside of a hash join, so we need to turn off BNL as well.)
static bool QueryMixesOuterBKAAndBNL(JOIN *join) {
  bool has_outer_bka = false;
  bool has_bnl = false;
  for (uint i = join->const_tables; i < join->primary_tables; ++i) {
    QEP_TAB *qep_tab = &join->qep_tab[i];
    if (UseHashJoin(qep_tab)) {
      has_bnl = true;
    } else if (qep_tab->op_type == QEP_TAB::OT_BKA &&
               qep_tab->last_inner() != NO_PLAN_IDX) {
      has_outer_bka = true;
    }
  }
  return has_bnl && has_outer_bka;
}

static bool InsideOuterOrAntiJoin(QEP_TAB *qep_tab) {
  return qep_tab->last_inner() != NO_PLAN_IDX;
}

void PickOutConditionsForTableIndex(int table_idx,
                                    vector<PendingCondition> *from,
                                    vector<PendingCondition> *to) {
  for (auto it = from->begin(); it != from->end();) {
    if (it->table_index_to_attach_to == table_idx) {
      to->push_back(*it);
      it = from->erase(it);
    } else {
      ++it;
    }
  }
}

void PickOutConditionsForTableIndex(int table_idx,
                                    vector<PendingCondition> *from,
                                    vector<Item *> *to) {
  for (auto it = from->begin(); it != from->end();) {
    if (it->table_index_to_attach_to == table_idx) {
      to->push_back(it->cond);
      it = from->erase(it);
    } else {
      ++it;
    }
  }
}

AccessPath *FinishPendingOperations(
    THD *thd, AccessPath *path, QEP_TAB *remove_duplicates_loose_scan_qep_tab,
    const vector<PendingCondition> &pending_conditions,
    table_map *conditions_depend_on_outer_tables) {
  path = PossiblyAttachFilter(path, pending_conditions, thd,
                              conditions_depend_on_outer_tables);

  if (remove_duplicates_loose_scan_qep_tab != nullptr) {
    QEP_TAB *const qep_tab =
        remove_duplicates_loose_scan_qep_tab;  // For short.
    KEY *key = qep_tab->table()->key_info + qep_tab->index();
    AccessPath *old_path = path;
    path = NewRemoveDuplicatesOnIndexAccessPath(
        thd, path, qep_tab->table(), key, qep_tab->loosescan_key_len);
    CopyBasicProperties(*old_path, path);  // We have nothing better.
  }

  return path;
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

  @param upper_first_idx gives us the first table index of the other side of the
    join. Only valid if we are inside a substructure (outer join, semijoin or
    antijoin). I.e., if we are processing the right side of the query
    't1 LEFT JOIN t2', upper_first_idx gives us the table index of 't1'. Used by
    hash join to determine the table map for each side of the join.
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
  @param pending_invalidators a global list of CacheInvalidatorIterators we
    need to emit, but cannot yet due to pending outer joins. Note that unlike
    pending_conditions and pending_join_conditions, this is never nullptr,
    and is always the same pointer when recursing within the same JOIN.
  @param pending_join_conditions if not nullptr, we are at the inner side of
    semijoin/antijoin. The join iterator is created at the outer side, so any
    join conditions at the inner side needs to be pushed to this vector so that
    they can be attached to the join iterator. Note that this is currently only
    used by hash join.
  @param[out] unhandled_duplicates list of tables we should have deduplicated
    using duplicate weedout, but could not; append-only.
  @param[out] conditions_depend_on_outer_tables For each condition we have
    applied on the inside of these iterators, their dependent tables are
    appended to this set. Thus, if conditions_depend_on_outer_tables contain
    something from outside the tables covered by [first_idx,last_idx)
    (ie., after translation from QEP_TAB indexes to table indexes), we cannot
    use a hash join, since the returned iterator depends on seeing outer rows
    when evaluating its conditions.
 */
AccessPath *ConnectJoins(plan_idx upper_first_idx, plan_idx first_idx,
                         plan_idx last_idx, QEP_TAB *qep_tabs, THD *thd,
                         CallingContext calling_context,
                         vector<PendingCondition> *pending_conditions,
                         vector<PendingInvalidator> *pending_invalidators,
                         vector<PendingCondition> *pending_join_conditions,
                         qep_tab_map *unhandled_duplicates,
                         table_map *conditions_depend_on_outer_tables) {
  assert(last_idx > first_idx);
  AccessPath *path = nullptr;

  // A special case: If we are at the top but the first table is an outer
  // join, we implicitly have one or more const tables to the left side
  // of said join.
  bool is_top_level_outer_join =
      calling_context == TOP_LEVEL &&
      qep_tabs[first_idx].last_inner() != NO_PLAN_IDX;

  vector<PendingCondition> top_level_pending_conditions;
  vector<PendingCondition> top_level_pending_join_conditions;
  if (is_top_level_outer_join) {
    path = NewFakeSingleRowAccessPath(thd, /*count_examined_rows=*/false);
    pending_conditions = &top_level_pending_conditions;
    pending_join_conditions = &top_level_pending_join_conditions;
  }

  // NOTE: i is advanced in one of two ways:
  //
  //  - If we have an inner join, it will be incremented near the bottom of the
  //    loop, as we can process inner join tables one by one.
  //  - If not (ie., we have an outer join or semijoin), we will process
  //    the sub-join recursively, and thus move it past the end of said
  //    sub-join.
  for (plan_idx i = first_idx; i < last_idx;) {
    // See if there are any invalidators we couldn't output before
    // (typically on a lower recursion level), but that are in-scope now.
    // It's highly unlikely that we have more than one pending table here
    // (the most common case will be zero), so don't bother combining them
    // into one invalidator.
    for (auto it = pending_invalidators->begin();
         it != pending_invalidators->end();) {
      if (it->table_index_to_invalidate < last_idx) {
        assert(path != nullptr);
        path = NewInvalidatorAccessPathForTable(thd, path, it->qep_tab,
                                                it->table_index_to_invalidate);
        it = pending_invalidators->erase(it);
      } else {
        ++it;
      }
    }

    if (is_top_level_outer_join && i == qep_tabs[first_idx].last_inner() + 1) {
      // Finished the top level outer join.
      path = FinishPendingOperations(
          thd, path, /*remove_duplicates_loose_scan_qep_tab=*/nullptr,
          top_level_pending_conditions, conditions_depend_on_outer_tables);

      is_top_level_outer_join = false;
      pending_conditions = nullptr;
      pending_join_conditions = nullptr;
    }

    bool add_limit_1;
    plan_idx substructure_end;
    Substructure substructure =
        FindSubstructure(qep_tabs, first_idx, i, last_idx, calling_context,
                         &add_limit_1, &substructure_end, unhandled_duplicates);

    // Get the index of the table where semijoin hash iterator would be created.
    // Used in placing the join conditions attached to the tables that are on
    // the inner side of a semijoin correctly.
    plan_idx semi_join_table_idx = NO_PLAN_IDX;
    if (calling_context == DIRECTLY_UNDER_SEMIJOIN &&
        qep_tabs[last_idx - 1].firstmatch_return != NO_PLAN_IDX) {
      semi_join_table_idx = qep_tabs[last_idx - 1].firstmatch_return + 1;
    }

    QEP_TAB *qep_tab = &qep_tabs[i];
    if (substructure == Substructure::OUTER_JOIN ||
        substructure == Substructure::SEMIJOIN) {
      qep_tab_map left_tables = TablesBetween(first_idx, i);
      qep_tab_map right_tables = TablesBetween(i, substructure_end);

      // Outer or semijoin, consisting of a subtree (possibly of only one
      // table), so we send the entire subtree down to a recursive invocation
      // and then join the returned root into our existing tree.
      AccessPath *subtree_path;
      vector<PendingCondition> subtree_pending_conditions;
      vector<PendingCondition> subtree_pending_join_conditions;
      table_map conditions_depend_on_outer_tables_subtree = 0;
      if (substructure == Substructure::SEMIJOIN) {
        // Semijoins don't have special handling of WHERE, so simply recurse.
        if (UseHashJoin(qep_tab) &&
            !QueryMixesOuterBKAAndBNL(qep_tab->join())) {
          // We must move any join conditions inside the subtructure up to this
          // level so that they can be attached to the hash join iterator.
          subtree_path = ConnectJoins(
              first_idx, i, substructure_end, qep_tabs, thd,
              DIRECTLY_UNDER_SEMIJOIN, &subtree_pending_conditions,
              pending_invalidators, &subtree_pending_join_conditions,
              unhandled_duplicates, &conditions_depend_on_outer_tables_subtree);
        } else {
          // Send in "subtree_pending_join_conditions", so that any semijoin
          // conditions are moved up to this level, where they will be attached
          // as conditions to the hash join iterator.
          subtree_path = ConnectJoins(
              first_idx, i, substructure_end, qep_tabs, thd,
              DIRECTLY_UNDER_SEMIJOIN, pending_conditions, pending_invalidators,
              &subtree_pending_join_conditions, unhandled_duplicates,
              &conditions_depend_on_outer_tables_subtree);
        }
      } else if (pending_conditions != nullptr) {
        // We are already on the right (inner) side of an outer join,
        // so we need to keep deferring WHERE predicates.
        subtree_path = ConnectJoins(
            first_idx, i, substructure_end, qep_tabs, thd,
            DIRECTLY_UNDER_OUTER_JOIN, pending_conditions, pending_invalidators,
            pending_join_conditions, unhandled_duplicates,
            &conditions_depend_on_outer_tables_subtree);

        // Pick out any conditions that should be directly above this join
        // (ie., the ON conditions for this specific join).
        PickOutConditionsForTableIndex(i, pending_conditions,
                                       &subtree_pending_conditions);

        // Similarly, for join conditions.
        if (pending_join_conditions != nullptr) {
          PickOutConditionsForTableIndex(i, pending_join_conditions,
                                         &subtree_pending_join_conditions);
        }
      } else {
        // We can check the WHERE predicates on this table right away
        // after the join (and similarly, set up invalidators).
        subtree_path = ConnectJoins(
            first_idx, i, substructure_end, qep_tabs, thd,
            DIRECTLY_UNDER_OUTER_JOIN, &subtree_pending_conditions,
            pending_invalidators, &subtree_pending_join_conditions,
            unhandled_duplicates, &conditions_depend_on_outer_tables_subtree);
      }
      *conditions_depend_on_outer_tables |=
          conditions_depend_on_outer_tables_subtree;

      JoinType join_type;
      if (qep_tab->table()->reginfo.not_exists_optimize) {
        // Similar to the comment on SplitConditions (see case #3), we can only
        // enable antijoin optimizations if we are not already on the right
        // (inner) side of another outer join. Otherwise, we would cause the
        // higher-up outer join to create NULL rows where there should be none.
        assert(substructure != Substructure::SEMIJOIN);
        join_type =
            (pending_conditions == nullptr) ? JoinType::ANTI : JoinType::OUTER;

        // Normally, a ”found” trigger means that the condition should be moved
        // up above some outer join (ie., it's a WHERE, not an ON condition).
        // However, there is one specific case where the optimizer sets up such
        // a trigger with the condition being _the same table as it's posted
        // on_, namely antijoins used for NOT IN; here, a FALSE condition is
        // being used to specify that inner rows should pass by the join, but
        // they should inhibit the null-complemented row. (So in this case,
        // the antijoin is no longer just an optimization that can be ignored
        // as we rewrite into an outer join.) In this case, there's a condition
        // wrapped in “not_null_compl” and ”found”, with the trigger for both
        // being the same table as the condition is posted on.
        //
        // So, as a special exception, detect this case, removing these
        // conditions (as they would otherwise kill all of our output rows) and
        // use them to mark the join as _really_ antijoin, even when it's
        // within an outer join.
        for (auto it = subtree_pending_conditions.begin();
             it != subtree_pending_conditions.end();) {
          if (it->table_index_to_attach_to == int(i) &&
              it->cond->item_name.ptr() == antijoin_null_cond) {
            assert(nullptr != dynamic_cast<Item_func_false *>(it->cond));
            join_type = JoinType::ANTI;
            it = subtree_pending_conditions.erase(it);
          } else {
            ++it;
          }
        }

        // Do the same for antijoin-marking conditions.
        for (auto it = subtree_pending_join_conditions.begin();
             it != subtree_pending_join_conditions.end();) {
          if (it->table_index_to_attach_to == int(i) &&
              it->cond->item_name.ptr() == antijoin_null_cond) {
            assert(nullptr != dynamic_cast<Item_func_false *>(it->cond));
            join_type = JoinType::ANTI;
            it = subtree_pending_join_conditions.erase(it);
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
      if (path == nullptr) {
        assert(substructure == Substructure::SEMIJOIN);
        add_limit_1 = true;
      }

      if (add_limit_1) {
        subtree_path = NewLimitOffsetAccessPath(
            thd, subtree_path,
            /*limit=*/1, /*offset=*/0,
            /*count_all_rows=*/false, /*reject_multiple_rows=*/false,
            /*send_records_override=*/nullptr);
      }

      const bool pfs_batch_mode = qep_tab->pfs_batch_update(qep_tab->join()) &&
                                  join_type != JoinType::ANTI &&
                                  join_type != JoinType::SEMI;

      // See documentation for conditions_depend_on_outer_tables in
      // the function comment. Note that this cannot happen for inner joins
      // (join conditions can always be pulled up for them), so we do not
      // replicate this check for inner joins below.
      const bool right_side_depends_on_outer =
          Overlaps(conditions_depend_on_outer_tables_subtree,
                   ConvertQepTabMapToTableMap(qep_tab->join(), left_tables));

      bool remove_duplicates_loose_scan = false;
      if (i != first_idx && qep_tabs[i - 1].do_loosescan() &&
          qep_tabs[i - 1].match_tab != i - 1) {
        QEP_TAB *prev_qep_tab = &qep_tabs[i - 1];
        assert(path != nullptr);

        KEY *key = prev_qep_tab->table()->key_info + prev_qep_tab->index();
        if (substructure == Substructure::SEMIJOIN) {
          path = NewNestedLoopSemiJoinWithDuplicateRemovalAccessPath(
              thd, path, subtree_path, prev_qep_tab->table(), key,
              prev_qep_tab->loosescan_key_len);
          SetCostOnNestedLoopAccessPath(*thd->cost_model(), qep_tab->position(),
                                        path);
        } else {
          // We were originally in a semijoin, even if it didn't win in
          // FindSubstructure (LooseScan against multiple tables always puts
          // the non-first tables in FirstMatch), it was just overridden by
          // the outer join. In this case, we put duplicate removal after the
          // join (and any associated filtering), which is the safe option --
          // and in this case, it's no slower, since we'll be having a LIMIT 1
          // inserted anyway.
          assert(substructure == Substructure::OUTER_JOIN);
          remove_duplicates_loose_scan = true;

          path = CreateNestedLoopAccessPath(thd, path, subtree_path, join_type,
                                            pfs_batch_mode);
          SetCostOnNestedLoopAccessPath(*thd->cost_model(), qep_tab->position(),
                                        path);
        }
      } else if (path == nullptr) {
        assert(substructure == Substructure::SEMIJOIN);
        path = subtree_path;
      } else if (((UseHashJoin(qep_tab) && !right_side_depends_on_outer) ||
                  UseBKA(qep_tab)) &&
                 !QueryMixesOuterBKAAndBNL(qep_tab->join())) {
        // Join conditions that were inside the substructure are placed in the
        // vector 'subtree_pending_join_conditions'. Find out which of these
        // conditions that should be attached to this table, and attach them
        // to the hash join iterator.
        vector<Item *> join_conditions;
        PickOutConditionsForTableIndex(i, &subtree_pending_join_conditions,
                                       &join_conditions);

        if (UseBKA(qep_tab)) {
          path = CreateBKAAccessPath(thd, qep_tab->join(), path, left_tables,
                                     subtree_path, right_tables,
                                     qep_tab->table(), qep_tab->table_ref,
                                     &qep_tab->ref(), join_type);
        } else {
          path = CreateHashJoinAccessPath(
              thd, qep_tab, subtree_path, right_tables, path, left_tables,
              join_type, &join_conditions, conditions_depend_on_outer_tables);
        }

        path = PossiblyAttachFilter(path, join_conditions, thd,
                                    conditions_depend_on_outer_tables);
      } else {
        // Normally, subtree_pending_join_conditions should be empty when we
        // create a nested loop iterator. However, in the case where we thought
        // we would be making a hash join but changed our minds (due to
        // right_side_depends_on_outer), there may be conditions there.
        // Similar to hash join above, pick out those conditions and add them
        // here.
        vector<Item *> join_conditions;
        PickOutConditionsForTableIndex(i, &subtree_pending_join_conditions,
                                       &join_conditions);
        subtree_path = PossiblyAttachFilter(subtree_path, join_conditions, thd,
                                            conditions_depend_on_outer_tables);

        path = CreateNestedLoopAccessPath(thd, path, subtree_path, join_type,
                                          pfs_batch_mode);
        SetCostOnNestedLoopAccessPath(*thd->cost_model(), qep_tab->position(),
                                      path);
      }

      QEP_TAB *remove_duplicates_loose_scan_qep_tab =
          remove_duplicates_loose_scan ? &qep_tabs[i - 1] : nullptr;
      path = FinishPendingOperations(
          thd, path, remove_duplicates_loose_scan_qep_tab,
          subtree_pending_conditions, conditions_depend_on_outer_tables);

      i = substructure_end;
      continue;
    } else if (substructure == Substructure::WEEDOUT) {
      AccessPath *subtree_path = ConnectJoins(
          first_idx, i, substructure_end, qep_tabs, thd, DIRECTLY_UNDER_WEEDOUT,
          pending_conditions, pending_invalidators, pending_join_conditions,
          unhandled_duplicates, conditions_depend_on_outer_tables);
      AccessPath *child_path = subtree_path;
      subtree_path = CreateWeedoutOrLimitAccessPath(
          thd, subtree_path, qep_tab->flush_weedout_table);

      // Copy costs (even though it makes no sense for the LIMIT 1 case).
      CopyBasicProperties(*child_path, subtree_path);

      if (path == nullptr) {
        path = subtree_path;
      } else {
        path =
            CreateNestedLoopAccessPath(thd, path, subtree_path, JoinType::INNER,
                                       /*pfs_batch_mode=*/false);
        SetCostOnNestedLoopAccessPath(*thd->cost_model(), qep_tab->position(),
                                      path);
      }

      i = substructure_end;
      continue;
    } else if (qep_tab->do_loosescan() && qep_tab->match_tab != i &&
               path != nullptr) {
      // Multi-table loose scan is generally handled by other parts of the code
      // (FindSubstructure() returns SEMIJOIN on the next table, since they will
      // have first match set), but we need to make sure there is only one table
      // on NestedLoopSemiJoinWithDuplicateRemovalIterator's left (outer) side.
      // Since we're not at the first table, we would be collecting a join
      // in “iterator” if we just kept on going, so we need to create a separate
      // tree by recursing here.
      AccessPath *subtree_path = ConnectJoins(
          first_idx, i, qep_tab->match_tab + 1, qep_tabs, thd, TOP_LEVEL,
          pending_conditions, pending_invalidators, pending_join_conditions,
          unhandled_duplicates, conditions_depend_on_outer_tables);

      path =
          CreateNestedLoopAccessPath(thd, path, subtree_path, JoinType::INNER,
                                     /*pfs_batch_mode=*/false);
      SetCostOnNestedLoopAccessPath(*thd->cost_model(), qep_tab->position(),
                                    path);
      i = qep_tab->match_tab + 1;
      continue;
    }

    AccessPath *table_path = GetTableAccessPath(thd, qep_tab, qep_tabs);

    qep_tab_map right_tables = qep_tab->idx_map();
    qep_tab_map left_tables = 0;

    // Get the left side tables of this join.
    if (InsideOuterOrAntiJoin(qep_tab)) {
      left_tables |= TablesBetween(upper_first_idx, first_idx);
    } else {
      left_tables |= TablesBetween(first_idx, i);
    }

    // If this is a BNL, we should replace it with hash join. We did decide
    // during create_access_paths that we actually can replace the BNL with a
    // hash join, so we don't bother checking any further that we actually can
    // replace the BNL with a hash join.
    const bool replace_with_hash_join =
        UseHashJoin(qep_tab) && !QueryMixesOuterBKAAndBNL(qep_tab->join());

    vector<Item *> predicates_below_join;
    vector<Item *> join_conditions;
    vector<PendingCondition> predicates_above_join;

    // If we are on the inner side of a semi-/antijoin, pending_join_conditions
    // will be set. If the join should be executed using hash join,
    // SplitConditions() will put all join conditions in
    // pending_join_conditions. These conditions will later be attached to the
    // hash join iterator when we are done handling the inner side.
    SplitConditions(qep_tab->condition(), qep_tab, &predicates_below_join,
                    &predicates_above_join,
                    replace_with_hash_join ? pending_join_conditions : nullptr,
                    semi_join_table_idx, left_tables);

    // We can always do BKA. The setup is very similar to hash join.
    const bool is_bka =
        UseBKA(qep_tab) && !QueryMixesOuterBKAAndBNL(qep_tab->join());

    if (is_bka) {
      Index_lookup &ref = qep_tab->ref();

      table_path =
          NewMRRAccessPath(thd, qep_tab->table(), &ref,
                           qep_tab->position()->table->join_cache_flags);
      SetCostOnTableAccessPath(*thd->cost_model(), qep_tab->position(),
                               /*is_after_filter=*/false, table_path);

      for (unsigned key_part_idx = 0; key_part_idx < ref.key_parts;
           ++key_part_idx) {
        *conditions_depend_on_outer_tables |=
            ref.items[key_part_idx]->used_tables();
      }
    } else if (replace_with_hash_join) {
      // We will now take all the join conditions (both equi- and
      // non-equi-join conditions) and move them to a separate vector so we
      // can attach them to the hash join iterator later. Conditions that
      // should be attached after the join remain in "predicates_below_join"
      // (i.e. filters).
      ExtractJoinConditions(qep_tab, &predicates_below_join, &join_conditions);
    }

    if (!qep_tab->condition_is_pushed_to_sort()) {  // See the comment on #2.
      double expected_rows = table_path->num_output_rows();
      table_path = PossiblyAttachFilter(table_path, predicates_below_join, thd,
                                        conditions_depend_on_outer_tables);
      POSITION *pos = qep_tab->position();
      if (expected_rows >= 0.0 && !predicates_below_join.empty() &&
          pos != nullptr) {
        SetCostOnTableAccessPath(*thd->cost_model(), pos,
                                 /*is_after_filter=*/true, table_path);
      }
    } else {
      *conditions_depend_on_outer_tables |= qep_tab->condition()->used_tables();
    }

    // Handle LooseScan that hits this specific table only.
    // Multi-table LooseScans will be handled by
    // NestedLoopSemiJoinWithDuplicateRemovalIterator
    // (which is essentially a semijoin NestedLoopIterator and
    // RemoveDuplicatesOnIndexIterator in one).
    if (qep_tab->do_loosescan() && qep_tab->match_tab == i) {
      KEY *key = qep_tab->table()->key_info + qep_tab->index();
      AccessPath *old_path = table_path;
      table_path = NewRemoveDuplicatesOnIndexAccessPath(
          thd, table_path, qep_tab->table(), key, qep_tab->loosescan_key_len);
      CopyBasicProperties(*old_path, table_path);  // We have nothing better.
    }

    // If there are lateral derived tables that depend on this table,
    // output invalidators to clear them when we output a new row.
    for (plan_idx table_idx :
         BitsSetIn(qep_tab->lateral_derived_tables_depend_on_me)) {
      if (table_idx < last_idx) {
        table_path = NewInvalidatorAccessPathForTable(thd, table_path, qep_tab,
                                                      table_idx);
      } else {
        // The table to invalidate belongs to a higher outer join nest,
        // which means that we cannot emit the invalidator right away --
        // the outer join we are a part of could be emitting NULL-complemented
        // rows that also need to invalidate the cache in question.
        // We'll deal with them in as soon as we get into the same join nest.
        // (But if we deal with them later than that, it might be too late!)
        pending_invalidators->push_back(PendingInvalidator{
            qep_tab, /*table_index_to_attach_to=*/table_idx});
      }
    }

    if (path == nullptr) {
      // We are the first table in this join.
      path = table_path;
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
        table_path = NewLimitOffsetAccessPath(
            thd, table_path, /*limit=*/1,
            /*offset=*/0,
            /*count_all_rows=*/false, /*reject_multiple_rows=*/false,
            /*send_records_override=*/nullptr);
      }

      // Inner join this table to the existing tree.
      // Inner joins are always left-deep, so we can just attach the tables as
      // we find them.
      assert(qep_tab->last_inner() == NO_PLAN_IDX);

      if (is_bka) {
        path = CreateBKAAccessPath(thd, qep_tab->join(), path, left_tables,
                                   table_path, right_tables, qep_tab->table(),
                                   qep_tab->table_ref, &qep_tab->ref(),
                                   JoinType::INNER);
      } else if (replace_with_hash_join) {
        // The numerically lower QEP_TAB is often (if not always) the smaller
        // input, so use that as the build input.
        if (pending_join_conditions != nullptr)
          PickOutConditionsForTableIndex(i, pending_join_conditions,
                                         &join_conditions);
        path = CreateHashJoinAccessPath(thd, qep_tab, path, left_tables,
                                        table_path, right_tables,
                                        JoinType::INNER, &join_conditions,
                                        conditions_depend_on_outer_tables);

        // Attach any remaining non-equi-join conditions as a filter after the
        // join.
        path = PossiblyAttachFilter(path, join_conditions, thd,
                                    conditions_depend_on_outer_tables);
      } else {
        path = CreateNestedLoopAccessPath(
            thd, path, table_path, JoinType::INNER,
            qep_tab->pfs_batch_update(qep_tab->join()));
        SetCostOnNestedLoopAccessPath(*thd->cost_model(), qep_tab->position(),
                                      path);
      }
    }
    ++i;

    // If we have any predicates that should be above an outer join,
    // send them upwards.
    for (PendingCondition &cond : predicates_above_join) {
      assert(pending_conditions != nullptr);
      pending_conditions->push_back(cond);
    }
  }
  if (is_top_level_outer_join) {
    assert(last_idx == qep_tabs[first_idx].last_inner() + 1);
    path = FinishPendingOperations(
        thd, path, /*remove_duplicates_loose_scan_qep_tab=*/nullptr,
        top_level_pending_conditions, conditions_depend_on_outer_tables);
  }
  return path;
}

static table_map get_update_or_delete_target_tables(const JOIN *join) {
  table_map target_tables = 0;

  for (const Table_ref *tr = join->query_block->leaf_tables; tr != nullptr;
       tr = tr->next_leaf) {
    if (tr->updating) {
      target_tables |= tr->map();
    }
  }

  return target_tables;
}

// If this is the top-level query block of a multi-table UPDATE or multi-table
// DELETE statement, wrap the path in an UPDATE_ROWS or DELETE_ROWS path.
AccessPath *JOIN::attach_access_path_for_update_or_delete(AccessPath *path) {
  if (thd->lex->m_sql_cmd == nullptr) {
    // It is not an UPDATE or DELETE statement.
    return path;
  }

  if (query_block->outer_query_block() != nullptr) {
    // It is not the top-level query block.
    return path;
  }

  const enum_sql_command command = thd->lex->m_sql_cmd->sql_command_code();

  // Single-table update or delete does not use access paths and iterators in
  // the old optimizer. (The hypergraph optimizer uses a unified code path for
  // single-table and multi-table, and always identifies itself as MULTI, so
  // these asserts hold for both optimizers.)
  assert(command != SQLCOM_UPDATE);
  assert(command != SQLCOM_DELETE);

  if (command == SQLCOM_UPDATE_MULTI) {
    const table_map target_tables = get_update_or_delete_target_tables(this);
    path = NewUpdateRowsAccessPath(
        thd, path, target_tables,
        GetImmediateUpdateTable(this, IsSingleBitSet(target_tables)));
  } else if (command == SQLCOM_DELETE_MULTI) {
    const table_map target_tables = get_update_or_delete_target_tables(this);
    path =
        NewDeleteRowsAccessPath(thd, path, target_tables,
                                GetImmediateDeleteTables(this, target_tables));
    EstimateDeleteRowsCost(path);
  }

  return path;
}

void JOIN::create_access_paths() {
  assert(m_root_access_path == nullptr);

  AccessPath *path = create_root_access_path_for_join();
  path = attach_access_paths_for_having_and_limit(path);
  path = attach_access_path_for_update_or_delete(path);

  m_root_access_path = path;
}

// Disable eq_ref caching. This is done for streaming aggregation because
// EQRefIterator's cache assumes table->record[0] is unmodified between two
// calls to Read(), but AggregateIterator may have changed it in the meantime
// when switching between groups.
//
// TODO(khatlen): Caching could be left enabled if a STREAM access path is added
// just below the AGGREGATE access path. The hypergraph optimizer does that, but
// adding intermediate temporary tables is harder to do with the old optimizer,
// so we just disable caching for now.
static void DisableEqRefCache(AccessPath *path) {
  WalkAccessPaths(path, /*join=*/nullptr,
                  WalkAccessPathPolicy::STOP_AT_MATERIALIZATION,
                  [](AccessPath *subpath, const JOIN *) {
                    if (subpath->type == AccessPath::EQ_REF) {
                      subpath->eq_ref().ref->disable_cache = true;
                    }
                    return false;
                  });
}

AccessPath *JOIN::create_root_access_path_for_join() {
  if (select_count) {
    return NewUnqualifiedCountAccessPath(thd);
  }

  // OK, so we're good. Go through the tables and make the join access paths.
  AccessPath *path = nullptr;
  if (query_block->is_table_value_constructor) {
    best_rowcount = query_block->row_value_list->size();
    path = NewTableValueConstructorAccessPath(thd);
    path->set_num_output_rows(query_block->row_value_list->size());
    path->cost = 0.0;
    path->init_cost = 0.0;
  } else if (const_tables == primary_tables) {
    // Only const tables, so add a fake single row to join in all
    // the const tables (only inner-joined tables are promoted to
    // const tables in the optimizer).
    path = NewFakeSingleRowAccessPath(thd, /*count_examined_rows=*/true);
    qep_tab_map conditions_depend_on_outer_tables = 0;
    if (where_cond != nullptr) {
      path = PossiblyAttachFilter(path, vector<Item *>{where_cond}, thd,
                                  &conditions_depend_on_outer_tables);
    }

    // Surprisingly enough, we can specify that the const tables are
    // to be dumped immediately to a temporary table. If we don't do this,
    // we risk that there are fields that are not copied correctly
    // (tmp_table_param contains copy_funcs we'd otherwise miss).
    if (const_tables > 0) {
      QEP_TAB *qep_tab = &this->qep_tab[const_tables];
      if (qep_tab->op_type == QEP_TAB::OT_MATERIALIZE) {
        qep_tab->table()->alias = "<temporary>";
        AccessPath *table_path = create_table_access_path(
            thd, qep_tab->table(), qep_tab->range_scan(), qep_tab->table_ref,
            qep_tab->position(),
            /*count_examined_rows=*/false);
        path = NewMaterializeAccessPath(
            thd,
            SingleMaterializeQueryBlock(
                thd, path, query_block->select_number, this,
                /*copy_items=*/true, qep_tab->tmp_table_param),
            qep_tab->invalidators, qep_tab->table(), table_path,
            /*cte=*/nullptr, query_expression(), qep_tab->ref_item_slice,
            /*rematerialize=*/true, qep_tab->tmp_table_param->end_write_records,
            /*reject_multiple_rows=*/false);
        EstimateMaterializeCost(thd, path);
      }
    }
  } else {
    qep_tab_map unhandled_duplicates = 0;
    qep_tab_map conditions_depend_on_outer_tables = 0;
    vector<PendingInvalidator> pending_invalidators;
    path = ConnectJoins(
        /*upper_first_idx=*/NO_PLAN_IDX, const_tables, primary_tables, qep_tab,
        thd, TOP_LEVEL, nullptr, &pending_invalidators,
        /*pending_join_conditions=*/nullptr, &unhandled_duplicates,
        &conditions_depend_on_outer_tables);

    // If there were any weedouts that we had to drop during ConnectJoins()
    // (ie., the join left some tables that were supposed to be deduplicated
    // but were not), handle them now at the very end.
    if (unhandled_duplicates != 0) {
      AccessPath *const child = path;
      path = NewWeedoutAccessPathForTables(thd, unhandled_duplicates, qep_tab,
                                           primary_tables, child);

      CopyBasicProperties(*child, path);
    }
  }

  // Deal with any materialization happening at the end (typically for
  // sorting, grouping or distinct).
  for (unsigned table_idx = const_tables + 1; table_idx <= tables;
       ++table_idx) {
    QEP_TAB *qep_tab = &this->qep_tab[table_idx];
    if (qep_tab->op_type != QEP_TAB::OT_MATERIALIZE &&
        qep_tab->op_type != QEP_TAB::OT_AGGREGATE_THEN_MATERIALIZE &&
        qep_tab->op_type != QEP_TAB::OT_AGGREGATE_INTO_TMP_TABLE &&
        qep_tab->op_type != QEP_TAB::OT_WINDOWING_FUNCTION) {
      continue;
    }
    if (qep_tab->op_type == QEP_TAB::OT_AGGREGATE_THEN_MATERIALIZE) {
      // Aggregate as we go, with output into a temporary table.
      // (We can also aggregate as we go after the materialization step;
      // see below. We won't be aggregating twice, though.)
      if (!qep_tab->tmp_table_param->precomputed_group_by) {
        DisableEqRefCache(path);
        path = NewAggregateAccessPath(thd, path,
                                      rollup_state != RollupState::NONE);
        EstimateAggregateCost(path, query_block);
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
        qep_tab->op_type != QEP_TAB::OT_AGGREGATE_INTO_TMP_TABLE) {
      path = NewFilterAccessPath(thd, path, qep_tab->having);
    }

    // Sorting comes after the materialization (which we're about to add),
    // and should be shown as such.
    Filesort *filesort = qep_tab->filesort;
    ORDER *filesort_order = qep_tab->filesort_pushed_order;

    Filesort *dup_filesort = nullptr;
    ORDER *dup_filesort_order = nullptr;
    bool limit_1_for_dup_filesort = false;

    // The pre-iterator executor did duplicate removal by going into the
    // temporary table and actually deleting records, using a hash table for
    // smaller tables and an O(n²) algorithm for large tables. This kind of
    // deletion is not cleanly representable in the iterator model, so we do it
    // using a duplicate-removing filesort instead, which has a straight-up
    // O(n log n) cost.
    if (qep_tab->needs_duplicate_removal) {
      bool all_order_fields_used;

      // If there's an ORDER BY on the query, it needs to be heeded in the
      // re-sort for DISTINCT. Note that the global ORDER BY could be pushed
      // to the first table, so we need to check there, too.
      ORDER *desired_order = this->order.order;
      if (desired_order == nullptr &&
          this->qep_tab[0].filesort_pushed_order != nullptr) {
        desired_order = this->qep_tab[0].filesort_pushed_order;
      }

      // If we don't have ROLLUP, we prefer to use query_block->fields,
      // so that we can see if fields belong to const tables or not
      // (which, in rare cases, can remove the requirement for a sort).
      //
      // But if we have ROLLUP, the rollup group wrappers will have been
      // removed from the base list (in change_to_use_tmp_fields_except_sums()),
      // since that is to be used for materialization, and we need to use the
      // actual field list instead.
      mem_root_deque<Item *> *select_list =
          (rollup_state == RollupState::NONE) ? &query_block->fields : fields;

      ORDER *order = create_order_from_distinct(
          thd, ref_items[qep_tab->ref_item_slice], desired_order, select_list,
          /*skip_aggregates=*/false, /*convert_bit_fields_to_long=*/false,
          &all_order_fields_used);
      if (order == nullptr) {
        // Only const fields.
        limit_1_for_dup_filesort = true;
      } else {
        bool force_sort_rowids = false;
        if (all_order_fields_used) {
          // The ordering for DISTINCT already gave us the right sort order,
          // so no need to sort again.
          //
          // TODO(sgunders): If there are elements in desired_order that are not
          // in fields_list, consider whether it would be cheaper to add them on
          // the end to avoid the second lsort, even though it would make the
          // first one more expensive. See e.g. main.distinct for a case.
          desired_order = nullptr;
          filesort = nullptr;
        } else if (filesort != nullptr && !filesort->using_addon_fields()) {
          // We have the rather unusual situation here that we have two sorts
          // directly after each other, with no temporary table in-between,
          // and filesort expects to be able to refer to rows by their row ID.
          // Usually, the sort for DISTINCT would be a superset of the sort for
          // ORDER BY, but not always (e.g. when sorting by some expression),
          // so we could end up in a situation where the first sort is by addon
          // fields and the second one is by positions.
          //
          // Thus, in this case, we force the first sort to use row IDs,
          // so that the result comes from SortFileIndirectIterator or
          // SortBufferIndirectIterator. These will both position the cursor
          // on the underlying temporary table correctly before returning it,
          // so that the successive filesort will save the right row ID
          // for the row.
          force_sort_rowids = true;
        }

        // Switch to the right slice if applicable, so that we fetch out the
        // correct items from order_arg.
        Switch_ref_item_slice slice_switch(this, qep_tab->ref_item_slice);
        dup_filesort = new (thd->mem_root) Filesort(
            thd, {qep_tab->table()}, /*keep_buffers=*/false, order,
            HA_POS_ERROR, /*remove_duplicates=*/true, force_sort_rowids,
            /*unwrap_rollup=*/false);
        dup_filesort_order = order;

        if (desired_order != nullptr && filesort == nullptr) {
          // We picked up the desired order from the first table, but we cannot
          // reuse its Filesort object, as it would get the wrong slice and
          // potentially addon fields. Create a new one.
          filesort = new (thd->mem_root) Filesort(
              thd, {qep_tab->table()}, /*keep_buffers=*/false, desired_order,
              HA_POS_ERROR, /*remove_duplicates=*/false, force_sort_rowids,
              /*unwrap_rollup=*/false);
          filesort_order = desired_order;
        }
      }
    }

    AccessPath *table_path =
        create_table_access_path(thd, qep_tab->table(), qep_tab->range_scan(),
                                 qep_tab->table_ref, qep_tab->position(),
                                 /*count_examined_rows=*/false);
    qep_tab->table()->alias = "<temporary>";

    if (qep_tab->op_type == QEP_TAB::OT_WINDOWING_FUNCTION) {
      path = NewWindowAccessPath(
          thd, path, qep_tab->tmp_table_param->m_window,
          qep_tab->tmp_table_param, qep_tab->ref_item_slice,
          qep_tab->tmp_table_param->m_window->needs_buffering());
      if (!qep_tab->tmp_table_param->m_window->short_circuit()) {
        path = NewMaterializeAccessPath(
            thd,
            SingleMaterializeQueryBlock(
                thd, path, query_block->select_number, this,
                /*copy_items=*/false, qep_tab->tmp_table_param),
            qep_tab->invalidators, qep_tab->table(), table_path,
            /*cte=*/nullptr, query_expression(),
            /*ref_slice=*/-1,
            /*rematerialize=*/true, tmp_table_param.end_write_records,
            /*reject_multiple_rows=*/false);
        EstimateMaterializeCost(thd, path);
      }
    } else if (qep_tab->op_type == QEP_TAB::OT_AGGREGATE_INTO_TMP_TABLE) {
      path = NewTemptableAggregateAccessPath(
          thd, path, qep_tab->tmp_table_param, qep_tab->table(), table_path,
          qep_tab->ref_item_slice);
      if (qep_tab->having != nullptr) {
        path = NewFilterAccessPath(thd, path, qep_tab->having);
      }
    } else {
      assert(qep_tab->op_type == QEP_TAB::OT_MATERIALIZE ||
             qep_tab->op_type == QEP_TAB::OT_AGGREGATE_THEN_MATERIALIZE);

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
      // this mode even if MaterializeIsDoingDeduplication() is set.
      Filesort *first_sort = dup_filesort != nullptr ? dup_filesort : filesort;
      AccessPath *old_path = path;
      if (first_sort != nullptr && first_sort->using_addon_fields() &&
          !MaterializeIsDoingDeduplication(qep_tab->table())) {
        path = NewStreamingAccessPath(
            thd, path, /*join=*/this, qep_tab->tmp_table_param,
            qep_tab->table(), qep_tab->ref_item_slice);
        CopyBasicProperties(*old_path, path);
      } else {
        path = NewMaterializeAccessPath(
            thd,
            SingleMaterializeQueryBlock(thd, path, query_block->select_number,
                                        this, /*copy_items=*/true,
                                        qep_tab->tmp_table_param),
            qep_tab->invalidators, qep_tab->table(), table_path,
            /*cte=*/nullptr, query_expression(), qep_tab->ref_item_slice,
            /*rematerialize=*/true, qep_tab->tmp_table_param->end_write_records,
            /*reject_multiple_rows=*/false);
        EstimateMaterializeCost(thd, path);
      }
    }

    if (qep_tab->condition() != nullptr) {
      path = NewFilterAccessPath(thd, path, qep_tab->condition());
      qep_tab->mark_condition_as_pushed_to_sort();
    }

    if (limit_1_for_dup_filesort) {
      path = NewLimitOffsetAccessPath(thd, path, /*limit=*/1,
                                      /*offset=*/0,
                                      /*count_all_rows=*/false,
                                      /*reject_multiple_rows=*/false,
                                      /*send_records_override=*/nullptr);
    } else if (dup_filesort != nullptr) {
      path = NewSortAccessPath(thd, path, dup_filesort, dup_filesort_order,
                               /*count_examined_rows=*/true);
    }
    if (filesort != nullptr) {
      path = NewSortAccessPath(thd, path, filesort, filesort_order,
                               /*count_examined_rows=*/true);
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
    do_aggregate = (get_end_select_func() == QEP_TAB::OT_AGGREGATE);
  } else {
    // Note that tmp_table_param.precomputed_group_by can be set even if we
    // don't actually have any grouping (e.g., make_tmp_tables_info() does this
    // even if there are no temporary tables made).
    do_aggregate = (qep_tab[primary_tables + tmp_tables].op_type ==
                    QEP_TAB::OT_AGGREGATE) ||
                   ((grouped || group_optimized_away) &&
                    tmp_table_param.precomputed_group_by);
  }
  if (do_aggregate) {
    // Aggregate as we go, with output into a special slice of the same table.
    assert(streaming_aggregation || tmp_table_param.precomputed_group_by);
#ifndef NDEBUG
    for (unsigned table_idx = const_tables; table_idx < tables; ++table_idx) {
      assert(qep_tab[table_idx].op_type !=
             QEP_TAB::OT_AGGREGATE_THEN_MATERIALIZE);
    }
#endif
    if (!tmp_table_param.precomputed_group_by) {
      DisableEqRefCache(path);
      path =
          NewAggregateAccessPath(thd, path, rollup_state != RollupState::NONE);
      EstimateAggregateCost(path, query_block);
    }
  }

  return path;
}

AccessPath *JOIN::attach_access_paths_for_having_and_limit(AccessPath *path) {
  // Attach HAVING and LIMIT if needed.
  // NOTE: We can have HAVING even without GROUP BY, although it's not very
  // useful.
  // We don't currently bother with materializing subqueries
  // in HAVING, as they should be rare.
  if (having_cond != nullptr) {
    AccessPath *old_path = path;
    path = NewFilterAccessPath(thd, path, having_cond);
    CopyBasicProperties(*old_path, path);
    if (thd->lex->using_hypergraph_optimizer) {
      // We cannot call EstimateFilterCost() in the pre-hypergraph optimizer,
      // as on repeated execution of a prepared query, the condition may contain
      // references to subqueries that are destroyed and not re-optimized yet.
      const FilterCost filter_cost = EstimateFilterCost(
          thd, path->num_output_rows(), having_cond, query_block);

      path->cost += filter_cost.cost_if_not_materialized;
      path->init_cost += filter_cost.init_cost_if_not_materialized;
    }
  }

  // Note: For select_count, LIMIT 0 is handled in JOIN::optimize() for the
  // common case, but not for CALC_FOUND_ROWS. OFFSET also isn't handled there.
  if (query_expression()->select_limit_cnt != HA_POS_ERROR ||
      query_expression()->offset_limit_cnt != 0) {
    path = NewLimitOffsetAccessPath(
        thd, path, query_expression()->select_limit_cnt,
        query_expression()->offset_limit_cnt, calc_found_rows, false,
        /*send_records_override=*/nullptr);
  }

  return path;
}

void JOIN::create_access_paths_for_index_subquery() {
  QEP_TAB *first_qep_tab = &qep_tab[0];
  AccessPath *path = first_qep_tab->access_path();
  if (first_qep_tab->condition() != nullptr) {
    path = NewFilterAccessPath(thd, path, first_qep_tab->condition());
  }

  Table_ref *const tl = qep_tab->table_ref;
  if (tl && tl->uses_materialization()) {
    if (tl->is_table_function()) {
      path = NewMaterializedTableFunctionAccessPath(thd, first_qep_tab->table(),
                                                    tl->table_function, path);
    } else {
      path = GetAccessPathForDerivedTable(thd, first_qep_tab,
                                          first_qep_tab->access_path());
    }
  }

  path = attach_access_paths_for_having_and_limit(path);
  m_root_access_path = path;
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
    if (sjtbl->have_confluent_row) return 1;
    sjtbl->have_confluent_row = true;
    return 0;
  }

  uchar *ptr = sjtbl->tmp_table->visible_field_ptr()[0]->field_ptr();
  // Put the rowids tuple into table->record[0]:
  // 1. Store the length
  if (sjtbl->tmp_table->visible_field_ptr()[0]->get_length_bytes() == 1) {
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
    if (create_ondisk_from_heap(thd, sjtbl->tmp_table, error,
                                /*insert_last_record=*/true,
                                /*ignore_last_dup=*/true, &is_duplicate))
      return -1;
    return is_duplicate ? 1 : 0;
  }
<<<<<<< HEAD
  return 0;
=======
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
<<<<<<< HEAD
        DBUG_ASSERT(
            !(tab->table()->reginfo.not_exists_optimize && !tab->condition()));
=======
        assert(!(tab->table()->reginfo.not_exists_optimize &&
                 !tab->condition()));
>>>>>>> upstream/cluster-7.6

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
<<<<<<< HEAD
       */
      DBUG_ASSERT(qep_tab->use_order() || qep_tab->type() == JT_EQ_REF);
=======
       */  
      assert(qep_tab->use_order() || qep_tab->type() == JT_EQ_REF);
>>>>>>> upstream/cluster-7.6

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
>>>>>>> pr/231
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
       LOCK_DEADLOCK LOCK_WAIT_TIMEOUT TABLE_DEF_CHANGED LOCK_NOWAIT
    Also skip printing to error log if the current thread has been killed.
  */
  if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT &&
      error != HA_ERR_TABLE_DEF_CHANGED && error != HA_ERR_NO_WAIT_LOCK &&
      !current_thd->killed)
    LogErr(ERROR_LEVEL, ER_READING_TABLE_FAILED, error, table->s->path.str);
  table->file->print_error(error, MYF(0));
  return 1;
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
  assert(!thd->is_error());

  if (table->reginfo.lock_type >= TL_WRITE_ALLOW_WRITE) {
    const enum_sql_command sql_command = tab->join()->thd->lex->sql_command;
    if (sql_command == SQLCOM_UPDATE_MULTI || sql_command == SQLCOM_UPDATE ||
        sql_command == SQLCOM_DELETE_MULTI || sql_command == SQLCOM_DELETE) {
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
        bitmap_set_bit(table->write_set, (*vfield_ptr)->field_index());
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
<<<<<<< HEAD
    assert(!tab->join_cond()->is_expensive());
    if (tab->join_cond()->val_int() == 0) table->set_null_row();
    if (thd->is_error()) return 1;
=======
<<<<<<< HEAD
    DBUG_ASSERT(!tab->join_cond()->is_expensive());
    if (tab->join_cond()->val_int() == 0) table->set_null_row();
=======
    assert(!tab->join_cond()->is_expensive());
    if (tab->join_cond()->val_int() == 0)
      table->set_null_row();
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }

  /* Check appearance of new constant items in Item_equal objects */
  JOIN *const join = tab->join();
  if (join->where_cond && update_const_equal_items(thd, join->where_cond, tab))
    return 1;
  Table_ref *tbl;
  for (tbl = join->query_block->leaf_tables; tbl; tbl = tbl->next_leaf) {
    Table_ref *embedded;
    Table_ref *embedding = tbl;
    do {
      embedded = embedding;
      if (embedded->join_cond_optim() &&
          update_const_equal_items(thd, embedded->join_cond_optim(), tab))
        return 1;
      embedding = embedded->embedding;
    } while (embedding && embedding->nested_join->m_tables.front() == embedded);
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
    if (!(error = table->file->ha_rnd_init(true))) {
      while ((error = table->file->ha_rnd_next(table->record[0])) ==
             HA_ERR_RECORD_DELETED) {
      }  // skip deleted row
         // We leave the cursor open, see why in read_const()
    }
    if (error) {
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
    assert(false);
    table->set_found_row();
    restore_record(table, record[1]);
  }

  return table->has_row() ? 0 : -1;
}

int read_const(TABLE *table, Index_lookup *ref) {
  int error;
  DBUG_TRACE;

  if (!table->is_started())  // If first read
  {
    /* Perform "Late NULLs Filtering" (see internals manual for explanations) */
    if (ref->impossible_null_ref() || construct_lookup(current_thd, table, ref))
      error = HA_ERR_KEY_NOT_FOUND;
    else {
      error = table->file->ha_index_init(ref->key, false);
      if (!error) {
        error = table->file->ha_index_read_map(
            table->record[0], ref->key_buff,
            make_prev_keypart_map(ref->key_parts), HA_READ_KEY_EXACT);
      }
      /*
        We leave the cursor open (no ha_index_end()).
        Indeed, this may be a statement which wants to modify the constant table
        (e.g. multi-table UPDATE/DELETE); then it will later call
        update_row() and/or position()&rnd_pos() (the latter case would be
        to get the row's id, store it in a temporary table and, in a second
        pass, find the row again to update it).
        For update_row() or position() to work, the cursor must still be
        positioned on the row; it is logical and some engines
        enforce it (see assert(m_table) in ha_perfschema::position()).
        So we do not close it. It will be closed by JOIN::cleanup().
      */
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
<<<<<<< HEAD
=======
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

<<<<<<< HEAD
  if (!table->file->inited) {
    DBUG_ASSERT(!tab->use_order());  // Don't expect sort req. for single row.
    if ((error =
             table->file->ha_index_init(table_ref->key, tab->use_order()))) {
      (void)report_handler_error(table, error);
=======
  if (!table->file->inited)
  {
    /*
      Disable caching for inner table of outer join, since setting the NULL
      property on the table will overwrite NULL bits and hence destroy the
      current row for later use as a cached row.
    */
    if (tab->table_ref->is_inner_table_of_outer_join())
      table_ref->disable_cache= true;
    assert(!tab->use_order()); //Don't expect sort req. for single row.
    if ((error= table->file->ha_index_init(table_ref->key, tab->use_order())))
    {
      (void) report_handler_error(table, error);
>>>>>>> upstream/cluster-7.6
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
<<<<<<< HEAD

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
=======
  else if (table->status == 0)
  {
    assert(table_ref->has_record);
>>>>>>> upstream/cluster-7.6
    table_ref->use_count++;
  }

>>>>>>> pr/231
  return table->has_row() ? 0 : -1;
}

/**
<<<<<<< HEAD
=======
  Since join_read_key may buffer a record, do not unlock
  it if it was not used in this invocation of join_read_key().
  Only count locks, thus remembering if the record was left unused,
  and unlock already when pruning the current value of
  TABLE_REF buffer.
  @sa join_read_key()
*/

<<<<<<< HEAD
static void join_read_key_unlock_row(QEP_TAB *tab) {
  DBUG_ASSERT(tab->ref().use_count);
  if (tab->ref().use_count) tab->ref().use_count--;
=======
void
join_read_key_unlock_row(QEP_TAB *tab)
{
  assert(tab->ref().use_count);
  if (tab->ref().use_count)
    tab->ref().use_count--;
>>>>>>> upstream/cluster-7.6
}

/**
  Rows from const tables are read once but potentially used
  multiple times during execution of a query.
  Ensure such rows are never unlocked during query execution.
*/

void
join_const_unlock_row(QEP_TAB *tab)
{
  assert(tab->type() == JT_CONST);
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

<<<<<<< HEAD
  DBUG_ASSERT(!tab->use_order());  // Pushed child can't be sorted

=======
  assert(!tab->use_order()); // Pushed child can't be sorted
>>>>>>> upstream/cluster-7.6
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
  assert(table->file->pushed_idx_cond == NULL);

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
<<<<<<< HEAD
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
=======
  const int rc= test_quick_select(thd,
                                  tab->keys(),
                                  0,          // empty table map
                                  HA_POS_ERROR,
                                  false,      // don't force quick range
                                  ORDER::ORDER_NOT_RELEVANT, tab,
                                  tab->condition(), &needed_reg_dummy, &qck,
                                  tab->table()->force_index);
  assert(old_qck == NULL || old_qck != qck) ;
>>>>>>> upstream/cluster-7.6
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

  if (tab->distinct && tab->remove_duplicates())  // Remove duplicates.
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
  if (init_read_record(&tab->read_record, tab->join()->thd, NULL, tab, 1, 1,
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

<<<<<<< HEAD
  (void)table->table_function->fill_result_table();
=======
  assert(derived->uses_materialization() && !tab->materialized);
>>>>>>> upstream/cluster-7.6

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

#if !defined(NDEBUG) || defined(HAVE_VALGRIND)
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
>>>>>>> pr/231
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

<<<<<<< HEAD
AccessPath *QEP_TAB::access_path() {
  assert(table());
  // Only some access methods support reversed access:
  assert(!m_reversed_access || type() == JT_REF || type() == JT_INDEX_SCAN);
  Index_lookup *used_ref = nullptr;
  AccessPath *path = nullptr;

=======
/*
  Helper function for sorting table with filesort.
*/

bool QEP_TAB::sort_table() {
  DBUG_ENTER("QEP_TAB::sort_table");
  DBUG_PRINT("info", ("Sorting for index"));
  THD_STAGE_INFO(join()->thd, stage_creating_sort_index);
<<<<<<< HEAD
  DBUG_ASSERT(join()->m_ordered_index_usage !=
              (filesort->order == join()->order
                   ? JOIN::ORDERED_INDEX_ORDER_BY
                   : JOIN::ORDERED_INDEX_GROUP_BY));
  const bool rc = create_sort_index(join()->thd, join(), this) != 0;
=======
  assert(join()->ordered_index_usage != (filesort->order == join()->order ?
                                         JOIN::ordered_index_order_by :
                                         JOIN::ordered_index_group_by));
  const bool rc= create_sort_index(join()->thd, join(), this) != 0;
>>>>>>> upstream/cluster-7.6
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
  assert(join_tab == join()->best_ref[idx()]);
  assert(table());
  assert(read_first_record == NULL);
  // Only some access methods support reversed access:
  assert(!join_tab->reversed_access || type() == JT_REF ||
         type() == JT_INDEX_SCAN);
  // Fall through to set default access functions:
>>>>>>> pr/231
  switch (type()) {
    case JT_REF:
      // May later change to a PushedJoinRefAccessPath if 'pushed'
      path = NewRefAccessPath(join()->thd, table(), &ref(), use_order(),
                              m_reversed_access,
                              /*count_examined_rows=*/true);
      used_ref = &ref();
      break;

    case JT_REF_OR_NULL:
      path = NewRefOrNullAccessPath(join()->thd, table(), &ref(), use_order(),
                                    /*count_examined_rows=*/true);
      used_ref = &ref();
      break;

<<<<<<< HEAD
    case JT_CONST:
      path = NewConstTableAccessPath(join()->thd, table(), &ref(),
                                     /*count_examined_rows=*/true);
      break;
=======
  case JT_CONST:
    read_first_record= join_read_const;
    read_record.read_record= join_no_more_records;
    read_record.unlock_row= join_const_unlock_row;
    break;
>>>>>>> upstream/cluster-7.6

    case JT_EQ_REF:
      // May later change to a PushedJoinRefAccessPath if 'pushed'
      path = NewEQRefAccessPath(join()->thd, table(), &ref(),
                                /*count_examined_rows=*/true);
      used_ref = &ref();
      break;

    case JT_FT:
      path = NewFullTextSearchAccessPath(
          join()->thd, table(), &ref(), ft_func(), use_order(),
          ft_func()->get_hints()->get_limit() != HA_POS_ERROR,
          /*count_examined_rows=*/true);
      used_ref = &ref();
      break;

<<<<<<< HEAD
    case JT_INDEX_SCAN:
      path = NewIndexScanAccessPath(join()->thd, table(), index(), use_order(),
                                    m_reversed_access,
                                    /*count_examined_rows=*/true);
      break;
    case JT_ALL:
    case JT_RANGE:
    case JT_INDEX_MERGE:
      if (using_dynamic_range) {
        path = NewDynamicIndexRangeScanAccessPath(join()->thd, table(), this,
                                                  /*count_examined_rows=*/true);
      } else {
        path = create_table_access_path(join()->thd, table(), range_scan(),
                                        table_ref, position(),
                                        /*count_examined_rows=*/true);
      }
      break;
    default:
      assert(false);
      break;
=======
  case JT_INDEX_SCAN:
    read_first_record= join_tab->reversed_access ?
      join_read_last : join_read_first;
    break;
  case JT_ALL:
  case JT_RANGE:
  case JT_INDEX_MERGE:
    read_first_record= (join_tab->use_quick == QS_DYNAMIC_RANGE) ?
      join_init_quick_read_record : join_init_read_record;
    break;
  default:
    assert(0);
    break;
>>>>>>> upstream/cluster-7.6
  }

<<<<<<< HEAD
  if (position() != nullptr) {
    SetCostOnTableAccessPath(*join()->thd->cost_model(), position(),
                             /*is_after_filter=*/false, path);
  }
=======
/**
  Install the appropriate 'linked' access method functions
  if this part of the join have been converted to pushed join.
*/

void QEP_TAB::set_pushed_table_access_method(void) {
  DBUG_ENTER("set_pushed_table_access_method");
  assert(table());

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
<<<<<<< HEAD
    DBUG_ASSERT(type() != JT_REF_OR_NULL);
    read_first_record = join_read_linked_first;
    read_record.read_record = join_read_linked_next;
=======
    assert(type() != JT_REF_OR_NULL);
    read_first_record= join_read_linked_first;
    read_record.read_record= join_read_linked_next;
>>>>>>> upstream/cluster-7.6
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
<<<<<<< HEAD
  DBUG_ASSERT(qep_tab == NULL || qep_tab > join->qep_tab);
=======
  assert(qep_tab == NULL || qep_tab > join->qep_tab);
  //TODO pass fields via argument
  List<Item> *fields= qep_tab ? qep_tab[-1].fields : join->fields;
>>>>>>> upstream/cluster-7.6

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
>>>>>>> pr/231

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
        // At least one condition guard is relevant, so we need to use
        // the AlternativeIterator.
        AccessPath *table_scan_path = NewTableScanAccessPath(
            join()->thd, table(), /*count_examined_rows=*/true);
        table_scan_path->set_num_output_rows(table()->file->stats.records);
        table_scan_path->cost = table()->file->table_scan_cost().total_cost();
        path = NewAlternativeAccessPath(join()->thd, path, table_scan_path,
                                        used_ref);
        break;
      }
    }
  }

  if (filesort) {
    // Evaluate any conditions before sorting entire row set.
    if (condition()) {
      vector<Item *> predicates_below_join;
      vector<PendingCondition> predicates_above_join;
      SplitConditions(condition(), this, &predicates_below_join,
                      &predicates_above_join,
                      /*join_conditions=*/nullptr,
                      /*semi_join_table_idx=*/NO_PLAN_IDX, /*left_tables=*/0);

      table_map conditions_depend_on_outer_tables = 0;
      path = PossiblyAttachFilter(path, predicates_below_join, join()->thd,
                                  &conditions_depend_on_outer_tables);
      mark_condition_as_pushed_to_sort();
    }

    // Wrap the chosen RowIterator in a SortingIterator, so that we get
    // sorted results out.
    path = NewSortAccessPath(join()->thd, path, filesort, filesort_pushed_order,
                             /*count_examined_rows=*/true);
  }

  // If we wrapped the table path in for example a sort or a filter, add cost to
  // the wrapping path too.
  if (path->num_output_rows() == -1 && position() != nullptr) {
    SetCostOnTableAccessPath(*join()->thd->cost_model(), position(),
                             /*is_after_filter=*/false, path);
  }

  return path;
}

<<<<<<< HEAD
static bool cmp_field_value(Field *field, ptrdiff_t diff) {
  assert(field);
=======
<<<<<<< HEAD
static bool cmp_field_value(Field *field, my_ptrdiff_t diff) {
  DBUG_ASSERT(field);
=======
static bool cmp_field_value(Field *field, my_ptrdiff_t diff)
{
  assert(field);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
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
    json_field->move_field_offset(diff);
    bool err = json_field->val_json(&right_wrapper);
    json_field->move_field_offset(-diff);
    if (err) return true; /* purecov: inspected */

    return (left_wrapper.compare(right_wrapper) != 0);
  }

  // Trailing space can't be skipped and length is different
  if (!field->is_text_key_type() && value1_length != value2_length)  // 2
    return true;

  if (field->cmp_max(field->field_ptr(), field->field_ptr() + diff,  // 3
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

  if (field->type() == MYSQL_TYPE_JSON) {
    const Field_json *json_field = down_cast<const Field_json *>(field);

    crc = json_field->make_hash_key(*hash_val);
  } else if (field->key_type() == HA_KEYTYPE_TEXT ||
             field->key_type() == HA_KEYTYPE_VARTEXT1 ||
             field->key_type() == HA_KEYTYPE_VARTEXT2) {
    const uchar *data_ptr = field->data_ptr();
    // Do not pass nullptr to hash function: undefined behaviour.
    if (field->data_length() == 0 && data_ptr == nullptr) {
      data_ptr = pointer_cast<const uchar *>(const_cast<char *>(""));
    }
    field->charset()->coll->hash_sort(field->charset(), data_ptr,
                                      field->data_length(), &seed1, &seed2);
    crc ^= seed1;
  } else {
    const uchar *pos = field->data_ptr();
    const uchar *end = pos + field->data_length();
    while (pos != end)
      crc = ((crc << 8) + (*pos++)) + (crc >> (8 * sizeof(ha_checksum) - 8));
  }
finish:
  *hash_val = crc;
  return crc;
}

/**
  Generate hash for unique constraint according to group-by list.

  This reads the values of the GROUP BY expressions from fields so assumes
  those expressions have been computed and stored into fields of a temporary
  table; in practice this means that copy_funcs() must have been called.
*/

static ulonglong unique_hash_group(ORDER *group) {
  DBUG_TRACE;
  ulonglong crc = 0;

<<<<<<< HEAD
  for (ORDER *ord = group; ord; ord = ord->next) {
    Field *field = ord->field_in_tmp_table;
<<<<<<< HEAD
    assert(field);
=======
    DBUG_ASSERT(field);
=======
  for (ORDER *ord= group; ord ; ord= ord->next)
  {
    Item *item= *(ord->item);
    field= item->get_tmp_table_field();
    assert(field);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
    unique_hash(field, &crc);
  }

  return crc;
}

/**
  Generate hash for unique_constraint for all visible fields of a table
  @param table the table for which we want a hash of its fields
  @return the hash value
*/
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
  int res = table->file->ha_index_read_map(table->record[1],
                                           table->hash_field->field_ptr(),
                                           HA_WHOLE_KEY, HA_READ_KEY_EXACT);
  while (res == 0) {
    // Check whether records are the same.
    if (!(table->group
              ? group_rec_cmp(table->group, table->record[0], table->record[1])
              : table_rec_cmp(table))) {
      return false;  // skip it
    }
    res = table->file->ha_index_next_same(
        table->record[1], table->hash_field->field_ptr(), sizeof(hash));
  }
  return true;
}

<<<<<<< HEAD
bool construct_lookup(THD *thd, TABLE *table, Index_lookup *ref) {
=======
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
<<<<<<< HEAD
  if ((error = table->file->ha_write_row(table->record[0]))) {
    if (create_ondisk_from_heap(join->thd, table, tmp_tbl->start_recinfo,
                                &tmp_tbl->recinfo, error, false, NULL))
      DBUG_RETURN(NESTED_LOOP_ERROR);  // Not a table_is_full error
=======
  if ((error=table->file->ha_write_row(table->record[0])))
  {
    /*
      If the error is HA_ERR_FOUND_DUPP_KEY and the grouping involves a
      TIMESTAMP field, throw a meaningfull error to user with the actual
      reason and the workaround. I.e, "Grouping on temporal is
      non-deterministic for timezones having DST. Please consider switching
      to UTC for this query". This is a temporary measure until we implement
      WL#13148 (Do all internal handling TIMESTAMP in UTC timezone), which
      will make such problem impossible.
    */
    if (error == HA_ERR_FOUND_DUPP_KEY)
    {
      for (group=table->group ; group ; group=group->next)
      {
        if (group->field->type() == MYSQL_TYPE_TIMESTAMP)
        {
          my_error(ER_GROUPING_ON_TIMESTAMP_IN_DST, MYF(0));
          DBUG_RETURN(NESTED_LOOP_ERROR);
        }
      }
    }
    if (create_ondisk_from_heap(join->thd, table,
                                tmp_tbl->start_recinfo,
                                &tmp_tbl->recinfo,
				error, FALSE, NULL))
      DBUG_RETURN(NESTED_LOOP_ERROR);            // Not a table_is_full error
>>>>>>> upstream/cluster-7.6
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
<<<<<<< HEAD
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
=======
      (idx=test_if_item_cache_changed(join->group_fields)) >= 0)
  {
    Temp_table_param *const tmp_tbl= qep_tab->tmp_table_param;
    if (join->first_record || (end_of_records && !join->grouped))
    {
      int send_group_parts= join->send_group_parts;
      if (idx < send_group_parts)
      {
        table_map save_nullinfo= 0;
        if (!join->first_record)
        {
          // Dead code or we need a test case for this branch
          assert(false);
          /*
            If this is a subquery, we need to save and later restore
            the const table NULL info before clearing the tables
            because the following executions of the subquery do not
            reevaluate constant fields. @see save_const_null_info
            and restore_const_null_info
          */
          if (join->select_lex->master_unit()->item && join->const_tables)
            save_const_null_info(join, &save_nullinfo);

>>>>>>> upstream/cluster-7.6
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
<<<<<<< HEAD
  DBUG_ASSERT(!join->plan_is_const() && fsort);
  table = tab->table();
=======
  assert(!join->plan_is_const() && fsort);
  table=  tab->table();
>>>>>>> upstream/cluster-7.6

  table->sort_result.io_cache =
      (IO_CACHE *)my_malloc(key_memory_TABLE_sort_io_cache, sizeof(IO_CACHE),
                            MYF(MY_WME | MY_ZEROFILL));

  // If table has a range, move it to select
<<<<<<< HEAD
  if (tab->quick() && tab->ref().key >= 0) {
    if (tab->type() != JT_REF_OR_NULL && tab->type() != JT_FT) {
      DBUG_ASSERT(tab->type() == JT_REF || tab->type() == JT_EQ_REF);
=======
  if (tab->quick() && tab->ref().key >= 0)
  {
    if (tab->type() != JT_REF_OR_NULL && tab->type() != JT_FT)
    {
      assert(tab->type() == JT_REF || tab->type() == JT_EQ_REF);
>>>>>>> upstream/cluster-7.6
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

<<<<<<< HEAD
  DBUG_ASSERT(join()->tmp_tables > 0 && table()->s->tmp_table != NO_TMP_TABLE);
  THD_STAGE_INFO(thd, stage_removing_duplicates);
=======
  assert(join()->tmp_tables > 0 && table()->s->tmp_table != NO_TMP_TABLE);
  THD_STAGE_INFO(join()->thd, stage_removing_duplicates);
>>>>>>> upstream/cluster-7.6

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

  file->extra(HA_EXTRA_NO_CACHE);
  DBUG_RETURN(false);
err:
  file->extra(HA_EXTRA_NO_CACHE);
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

<<<<<<< HEAD
  std::unique_ptr<uchar[]> key_buffer(new uchar[key_length]);
  if ((error = file->ha_rnd_init(1))) goto err;
  for (;;) {
    uchar *key_pos = key_buffer.get();
    if (thd->killed) {
=======
  {
    Field **ptr;
    ulong total_length= 0;
    for (ptr= first_field, field_length=field_lengths ; *ptr ; ptr++)
    {
      uint length= (*ptr)->sort_length();
      (*field_length++)= length;
      total_length+= length;
    }
    DBUG_PRINT("info",("field_count: %u  key_length: %lu  total_length: %lu",
                       field_count, key_length, total_length));
    assert(total_length <= key_length);
    key_length= total_length;
    extra_length= ALIGN_SIZE(key_length)-key_length;
  }

  if (my_hash_init(&hash, &my_charset_bin, (uint) file->stats.records, 0, 
                   key_length, (my_hash_get_key) 0, 0, 0,
                   key_memory_hash_index_key_buffer))
  {
    my_free(key_buffer);
    DBUG_RETURN(true);
  }

  if ((error= file->ha_rnd_init(1)))
    goto err;
  key_pos=key_buffer;
  for (;;)
  {
    uchar *org_key_pos;
    if (thd->killed)
    {
>>>>>>> upstream/cluster-7.6
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

  file->extra(HA_EXTRA_NO_CACHE);
  (void)file->ha_rnd_end();
  DBUG_RETURN(false);

err:
  file->extra(HA_EXTRA_NO_CACHE);
  if (file->inited) (void)file->ha_rnd_end();
  if (error) file->print_error(error, MYF(0));
  DBUG_RETURN(true);
}

bool cp_buffer_from_ref(THD *thd, TABLE *table, TABLE_REF *ref) {
>>>>>>> pr/231
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
    if (alloc_group_fields(curr_join, curr_join->group_list.order)) return true;
    main_join->group_fields_cache = curr_join->group_fields;
  }
  return false;
}

/**
  Get a list of buffers for saving last group.

  Groups are saved in reverse order for easier check loop.
*/

static bool alloc_group_fields(JOIN *join, ORDER *group) {
  if (group) {
    for (; group; group = group->next) {
      Cached_item *tmp = new_Cached_item(join->thd, *group->item);
      if (!tmp || join->group_fields.push_front(tmp)) return true;
    }
  }
  join->streaming_aggregation = true; /* Mark for do_query_block */
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

/// Compute the position mapping from fields to ref_item_array, cf.
/// detailed explanation in change_to_use_tmp_fields_except_sums
static size_t compute_ria_idx(const mem_root_deque<Item *> &fields, size_t i,
                              size_t added_non_hidden_fields, size_t border) {
  const size_t num_select_elements = fields.size() - border;
  const size_t orig_num_select_elements =
      num_select_elements - added_non_hidden_fields;
  size_t idx;

<<<<<<< HEAD
  if (i < border) {
    idx = fields.size() - i - 1 - added_non_hidden_fields;
  } else {
    idx = i - border;
    if (idx >= orig_num_select_elements) idx += border;
=======
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
<<<<<<< HEAD
        if (!(tmp = static_cast<uchar *>(sql_alloc(field->pack_length() + 1))))
          goto err;
        if (copy) {
          DBUG_ASSERT(param->field_count > (uint)(copy - copy_start));
=======
	if (!(tmp= static_cast<uchar*>(sql_alloc(field->pack_length() + 1))))
	  goto err;
        if (copy)
        {
          assert (param->field_count > (uint) (copy - copy_start));
>>>>>>> upstream/cluster-7.6
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
>>>>>>> pr/231
  }
  return idx;
}

/**
  Make a copy of all simple SELECT'ed fields.

  This is used in window functions, to copy fields to and from the frame buffer.
  (It used to be used in materialization, but now that is entirely done by
  copy_funcs(), even for Item_field.)

  @param param     Represents the current temporary file being produced
  @param thd       The current thread
  @param reverse_copy   If true, copies fields *back* from the frame buffer
                        tmp table to the output table's buffer,
                        cf. #bring_back_frame_row.

  @returns false if OK, true on error.
*/

bool copy_fields(Temp_table_param *param, const THD *thd, bool reverse_copy) {
  DBUG_TRACE;

<<<<<<< HEAD
  DBUG_PRINT("enter", ("for param %p", param));
  for (Copy_field &ptr : param->copy_fields) ptr.invoke_do_copy(reverse_copy);
=======
<<<<<<< HEAD
  DBUG_ASSERT((ptr != NULL && end >= ptr) || (ptr == NULL && end == NULL));
  DBUG_PRINT("enter", ("for param %p", param));
  for (; ptr < end; ptr++) ptr->invoke_do_copy(ptr);
=======
  assert((ptr != NULL && end >= ptr) || (ptr == NULL && end == NULL));

  for (; ptr < end; ptr++)
    ptr->invoke_do_copy(ptr);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

  if (thd->is_error()) return true;
  return false;
}

/**
  For each rollup wrapper below the given item, replace it with a temporary
  field, e.g.

    1 + rollup_group_item(a) -> 1 + \<temporary\>.`rollup_group_item(a)`

  Which temporary field to use is found by looking at the other fields;
  the rollup_group_item should already exist earlier in the list
  (and having a temporary table field set up), simply by virtue of being a
  group item.
 */
static bool replace_embedded_rollup_references_with_tmp_fields(
    THD *thd, Item *item, mem_root_deque<Item *> *fields) {
  if (!item->has_rollup_expr()) {
    return false;
  }
  const auto replace_functor = [thd, item, fields](Item *sub_item, Item *,
                                                   unsigned) -> ReplaceResult {
    if (!is_rollup_group_wrapper(sub_item)) {
      return {ReplaceResult::KEEP_TRAVERSING, nullptr};
    }
    for (Item *other_item : *fields) {
      if (other_item->eq(sub_item, false)) {
        Field *field = other_item->get_tmp_table_field();
        Item *item_field = new (thd->mem_root) Item_field(field);
        if (item_field == nullptr) return {ReplaceResult::ERROR, nullptr};
        item_field->item_name = item->item_name;
        return {ReplaceResult::REPLACE, item_field};
      }
    }
    assert(false);
    return {ReplaceResult::ERROR, nullptr};
  };
  return WalkAndReplace(thd, item, std::move(replace_functor));
}

/**
  Change all funcs and sum_funcs to fields in tmp table, and create
  new list of all items.

  @param fields                      list of all fields; should really be const,
                                       but Item does not always respect
                                       constness
  @param thd                         THD pointer
  @param [out] ref_item_array        array of pointers to top elements of filed
  list
  @param [out] res_fields            new list of all items
  @param added_non_hidden_fields     number of visible fields added by subquery
                                     to derived transformation

  @returns false if success, true if error
*/

bool change_to_use_tmp_fields(mem_root_deque<Item *> *fields, THD *thd,
                              Ref_item_array ref_item_array,
                              mem_root_deque<Item *> *res_fields,
                              size_t added_non_hidden_fields) {
  DBUG_TRACE;

  res_fields->clear();

  const auto num_hidden_fields = CountHiddenFields(*fields);
  auto it = fields->begin();

  for (size_t i = 0; it != fields->end(); ++i, ++it) {
    Item *item = *it;
    Item_field *orig_field = item->real_item()->type() == Item::FIELD_ITEM
                                 ? down_cast<Item_field *>(item->real_item())
                                 : nullptr;
    Item *new_item;
    Field *field;
    if (item->has_aggregation() && item->type() != Item::SUM_FUNC_ITEM)
      new_item = item;
    else if (item->type() == Item::FIELD_ITEM)
      new_item = item->get_tmp_table_item(thd);
    else if (item->type() == Item::FUNC_ITEM &&
             ((Item_func *)item)->functype() == Item_func::SUSERVAR_FUNC) {
      field = item->get_tmp_table_field();
      if (field != nullptr) {
        /*
          Replace "@:=<expression>" with "@:=<tmp table column>". Otherwise, we
          would re-evaluate <expression>, and if expression were a subquery,
          this would access already-unlocked tables.
        */
        Item_func_set_user_var *suv =
            new Item_func_set_user_var(thd, (Item_func_set_user_var *)item);
        Item_field *new_field = new Item_field(field);
        if (!suv || !new_field) return true;  // Fatal error
        mem_root_deque<Item *> list(thd->mem_root);
        if (list.push_back(new_field)) return true;
        if (suv->set_arguments(&list, true)) return true;
        new_item = suv;
      } else
        new_item = item;
    } else if ((field = item->get_tmp_table_field())) {
      if (item->type() == Item::SUM_FUNC_ITEM && field->table->group) {
        new_item = down_cast<Item_sum *>(item)->result_item(field);
        assert(new_item != nullptr);
      } else {
        new_item = new (thd->mem_root) Item_field(field);
        if (new_item == nullptr) return true;
      }
      new_item->item_name = item->item_name;
      if (item->type() == Item::REF_ITEM) {
        Item_field *ifield = down_cast<Item_field *>(new_item);
        Item_ref *iref = down_cast<Item_ref *>(item);
        ifield->table_name = iref->table_name;
        ifield->set_orignal_db_name(iref->original_db_name());
        ifield->db_name = iref->db_name;
      }
<<<<<<< HEAD
      if (orig_field != nullptr && item != new_item) {
        down_cast<Item_field *>(new_item)->set_original_table_name(
            orig_field->original_table_name());
=======
<<<<<<< HEAD
#ifndef DBUG_OFF
      if (!item_field->item_name.is_set()) {
=======
#ifndef NDEBUG
      if (!item_field->item_name.is_set())
      {
>>>>>>> upstream/cluster-7.6
        char buff[256];
        String str(buff, sizeof(buff), &my_charset_bin);
        str.length(0);
        item->print(&str, QT_ORDINARY);
        item_field->item_name.copy(str.ptr(), str.length());
>>>>>>> pr/231
      }
    } else {
      new_item = item;
      replace_embedded_rollup_references_with_tmp_fields(thd, item, fields);
    }

    new_item->hidden = item->hidden;
    res_fields->push_back(new_item);
    const size_t idx =
        compute_ria_idx(*fields, i, added_non_hidden_fields, num_hidden_fields);
    ref_item_array[idx] = new_item;
  }

  return false;
}

/**
  For each rollup wrapper below the given item, replace its argument with a
  temporary field, e.g.

    1 + rollup_group_item(a) -> 1 + rollup_group_item(\<temporary\>.a).

  Which temporary field to use is found by looking at the Query_block's group
  items, and looking up their (previously set) result fields.
 */
bool replace_contents_of_rollup_wrappers_with_tmp_fields(THD *thd,
                                                         Query_block *select,
                                                         Item *item_arg) {
  return WalkAndReplace(
      thd, item_arg,
      [thd, select](Item *item, Item *, unsigned) -> ReplaceResult {
        if (!is_rollup_group_wrapper(item)) {
          return {ReplaceResult::KEEP_TRAVERSING, nullptr};
        }
        Item_rollup_group_item *rollup_item =
            down_cast<Item_rollup_group_item *>(item);

        Item *real_item = item;
        while (is_rollup_group_wrapper(real_item)) {
          real_item = unwrap_rollup_group(real_item)->real_item();
        }
        ORDER *order = select->find_in_group_list(real_item, nullptr);
        Item_rollup_group_item *new_item = new Item_rollup_group_item(
            rollup_item->min_rollup_level(),
            order->rollup_item->inner_item()->get_tmp_table_item(thd));
        if (new_item == nullptr ||
            select->join->rollup_group_items.push_back(new_item)) {
          return {ReplaceResult::ERROR, nullptr};
        }
        new_item->quick_fix_field();
        return {ReplaceResult::REPLACE, new_item};
      });
}

/**
  Change all sum_func refs to fields to point at fields in tmp table.
  Change all funcs to be fields in tmp table.

  This is used when we set up a temporary table, but aggregate functions
  (sum_funcs) cannot be evaluated yet, for instance because data is not
  sorted in the right order. (Otherwise, change_to_use_tmp_fields() would
  be used.)

  @param fields                      list of all fields; should really be const,
                                       but Item does not always respect
                                       constness
  @param select                      the query block we are doing this to
  @param thd                         THD pointer
  @param [out] ref_item_array        array of pointers to top elements of filed
  list
  @param [out] res_fields            new list of items of select item list
  @param added_non_hidden_fields     number of visible fields added by subquery
                                     to derived transformation

  @returns false if success, true if error
*/

bool change_to_use_tmp_fields_except_sums(mem_root_deque<Item *> *fields,
                                          THD *thd, Query_block *select,
                                          Ref_item_array ref_item_array,
                                          mem_root_deque<Item *> *res_fields,
                                          size_t added_non_hidden_fields) {
  DBUG_TRACE;
  res_fields->clear();

  const auto num_hidden_items = CountHiddenFields(*fields);
  auto it = fields->begin();

  for (size_t i = 0; it != fields->end(); ++i, ++it) {
    Item *item = *it;
    /*
      Below we create "new_item" using get_tmp_table_item
      based on all_fields[i] and assign them to res_all_fields[i].

      The new items are also put into ref_item_array, but in another order,
      cf the diagram below.

      Example of the population of ref_item_array and the fields argument
      containing hidden and selected fields. "border" is computed by counting
      the number of hidden fields at the beginning of fields:

       fields                       (selected fields)
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
         4   5   6   7   8   9   3   2   1   0   position in fields
                                                 similar to ref_all_fields pos
       fields.elements == 10        border == 4 (i.e. # of hidden fields)
       (visible) elements == 6

       i==0   ->   afe-0-1 == 9     i==4 -> 4-4 == 0
       i==1   ->   afe-1-1 == 8      :
       i==2   ->   afe-2-1 == 7
       i==3   ->   afe-3-1 == 6     i==9 -> 9-4 == 5

      This mapping is further compilated if a scalar subquery to join with
      derived table transformation has added (visible) fields to field_list
      *after* resolving and adding hidden fields,
      cf. decorrelate_derived_scalar_subquery. This is signalled by a value
      of added_non_hidden_fields > 0. This makes the mapping look like this,
      (Note: only one original select list item "orig" in a scalar subquery):

       fields            (selected_fields)
       |                 |
       V                 V (orig: 2, added by transform: 3, 4)
       +--+    +--+    +--+    +--+    +--+
       |0 | -> |1 | -> |2 | -> |3 | -> |4 |
       +--+    +--+    +--+    +--+    +--+

       +---#---+---#---+---+
       | 2 # 1 | 0 # 3 | 4 | resulting ref_item_array
       +---#---+---#---+---+

       all_fields.elements == 5      border == 2
       (visible) elements == 3       added_non_hidden_fields == 2
                                     orig_num_select_elements == 1

      If the added visible fields had not been there we would have seen this:

       +---#---+---+
       | 2 # 1 | 0 | ref_item_array
       +---#---+---+

       all_fields.elements == 3      border == 2
       (visible) elements == 1       added_non_hidden_fields == 0
                                     orig_num_select_elements == 1

      so the logic below effectively lets the original fields stay where they
      are, tucking the extra fields on at the end, since references
      (Item_ref::ref) will point to those positions in the effective slice
      array.
    */
    Item *new_item;

    if (is_rollup_group_wrapper(item)) {
      // If we cannot evaluate aggregates at this point, we also cannot
      // evaluate rollup NULL items, so we will need to move the wrapper out
      // into this layer.
      Item_rollup_group_item *rollup_item =
          down_cast<Item_rollup_group_item *>(item);

      rollup_item->inner_item()->set_result_field(item->get_result_field());
      new_item = rollup_item->inner_item()->get_tmp_table_item(thd);

      ORDER *order =
          select->find_in_group_list(rollup_item->inner_item(), nullptr);
      order->rollup_item->inner_item()->set_result_field(
          item->get_result_field());

      new_item =
          new Item_rollup_group_item(rollup_item->min_rollup_level(), new_item);
      if (new_item == nullptr ||
          select->join->rollup_group_items.push_back(
              down_cast<Item_rollup_group_item *>(new_item))) {
        return true;
      }
      new_item->quick_fix_field();

      // Remove the rollup wrapper on the inner level; it's harmless to keep
      // on the lower level, but also pointless.
      Item *unwrapped_item = unwrap_rollup_group(item);
      unwrapped_item->hidden = item->hidden;
      thd->change_item_tree(&*it, unwrapped_item);

    } else if ((select->is_implicitly_grouped() &&
                ((item->used_tables() & ~(RAND_TABLE_BIT | INNER_TABLE_BIT)) ==
                 0)) ||                    // (1)
               item->has_rollup_expr()) {  // (2)
      /*
        We go here when:
        (1) The Query_block is implicitly grouped and 'item' does not
            depend on any table. Then that field should be evaluated exactly
            once, whether there are zero or more rows in the temporary table
            (@see create_tmp_table()).
        (2) 'item' has a rollup expression. Then we delay processing
            until below; see comment further down.
      */
      new_item = item->copy_or_same(thd);
      if (new_item == nullptr) return true;
    } else {
      new_item = item->get_tmp_table_item(thd);
      if (new_item == nullptr) return true;
    }

    new_item->update_used_tables();

    assert_consistent_hidden_flags(*res_fields, new_item, item->hidden);
    new_item->hidden = item->hidden;
    res_fields->push_back(new_item);
    const size_t idx =
        compute_ria_idx(*fields, i, added_non_hidden_fields, num_hidden_items);
    ref_item_array[idx] = new_item;
  }

  for (Item *item : *fields) {
    if (!is_rollup_group_wrapper(item) && item->has_rollup_expr()) {
      // An item that isn't a rollup wrapper itself, but depends on one (or
      // multiple). We need to go into those items, find the rollup wrappers,
      // and replace them with rollup wrappers around the temporary fields,
      // as in the conditional above. Note that this needs to be done after
      // we've gone through all the items, so that we know the right result
      // fields for all the rollup wrappers (the function uses them to know
      // which temporary field to replace with).
      if (replace_contents_of_rollup_wrappers_with_tmp_fields(thd, select,
                                                              item)) {
        return true;
      }
    }
  }

  assert(!thd->is_error());
  return false;
}

/**
  Set all column values from all input tables to NULL.

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
<<<<<<< HEAD

bool JOIN::clear_fields(table_map *save_nullinfo) {
  for (uint tableno = 0; tableno < primary_tables; tableno++) {
    QEP_TAB *const tab = qep_tab + tableno;
    TABLE *const table = tab->table_ref->table;
    if (!table->has_null_row()) {
      *save_nullinfo |= tab->table_ref->map();
      if (table->const_table) table->save_null_flags();
      table->set_null_row();  // All fields are NULL
    }
=======
static void save_const_null_info(JOIN *join, table_map *save_nullinfo)
{
  assert(join->const_tables);

  for (uint tableno= 0; tableno < join->const_tables; tableno++)
  {
    QEP_TAB *const tab= join->qep_tab + tableno;
    TABLE *const table= tab->table();
    /*
      table->status and table->null_row must be in sync: either both set
      or none set. Otherwise, an additional table_map parameter is
      needed to save/restore_const_null_info() these separately
    */
    assert(table->has_null_row() ? (table->status & STATUS_NULL_ROW) :
           !(table->status & STATUS_NULL_ROW));

    if (!table->has_null_row())
      *save_nullinfo|= tab->table_ref->map();
>>>>>>> upstream/cluster-7.6
  }
  return false;
}

/**
  Restore all result fields for all tables specified in save_nullinfo.

  @param save_nullinfo Set of tables for which restore is necessary.

  @note Const tables must have their NULL value flags restored,
        @see JOIN::clear_fields().
*/
<<<<<<< HEAD
void JOIN::restore_fields(table_map save_nullinfo) {
<<<<<<< HEAD
  assert(save_nullinfo);
=======
  DBUG_ASSERT(save_nullinfo);
=======
static void restore_const_null_info(JOIN *join, table_map save_nullinfo)
{
  assert(join->const_tables && save_nullinfo);
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

  for (uint tableno = 0; tableno < primary_tables; tableno++) {
    QEP_TAB *const tab = qep_tab + tableno;
    if (save_nullinfo & tab->table_ref->map()) {
      TABLE *const table = tab->table_ref->table;
      if (table->const_table) table->restore_null_flags();
      table->reset_null_row();
    }
  }
}

/******************************************************************************
  Code for pfs_batch_update
******************************************************************************/

bool QEP_TAB::pfs_batch_update(const JOIN *join) const {
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

bool MaterializeIsDoingDeduplication(TABLE *table) {
  if (table->hash_field != nullptr) {
    // Doing deduplication via hash field.
    return true;
  }

  // We assume that if there's an unique index, it has to be used for
  // deduplication (create_tmp_table() never makes them for any other
  // reason).
  if (table->key_info != nullptr) {
    for (size_t i = 0; i < table->s->keys; ++i) {
      if ((table->key_info[i].flags & HA_NOSAME) != 0) {
        return true;
      }
    }
  }
  return false;
}

/**
  create_table_access_path is used to scan by using a number of different
  methods. Which method to use is set-up in this call so that you can
  create an iterator from the returned access path and fetch rows through
  said iterator afterwards.

  @param thd      Thread handle
  @param table    Table the data [originally] comes from
  @param range_scan AccessPath to scan the table with, or nullptr
  @param table_ref
                  Position for the table, must be non-nullptr for
                  WITH RECURSIVE
  @param position Place to get cost information from, or nullptr
  @param count_examined_rows
    See AccessPath::count_examined_rows.
 */
AccessPath *create_table_access_path(THD *thd, TABLE *table,
                                     AccessPath *range_scan,
                                     Table_ref *table_ref, POSITION *position,
                                     bool count_examined_rows) {
  AccessPath *path;
  if (range_scan != nullptr) {
    range_scan->count_examined_rows = count_examined_rows;
    path = range_scan;
  } else if (table_ref != nullptr && table_ref->is_recursive_reference()) {
    path = NewFollowTailAccessPath(thd, table, count_examined_rows);
  } else {
    path = NewTableScanAccessPath(thd, table, count_examined_rows);
  }
  if (position != nullptr) {
    SetCostOnTableAccessPath(*thd->cost_model(), position,
                             /*is_after_filter=*/false, path);
  }
  return path;
}

unique_ptr_destroy_only<RowIterator> init_table_iterator(
    THD *thd, TABLE *table, AccessPath *range_scan, Table_ref *table_ref,
    POSITION *position, bool ignore_not_found_rows, bool count_examined_rows) {
  unique_ptr_destroy_only<RowIterator> iterator;

  empty_record(table);

  if (table->unique_result.io_cache &&
      my_b_inited(table->unique_result.io_cache)) {
    DBUG_PRINT("info", ("using SortFileIndirectIterator"));
    iterator = NewIterator<SortFileIndirectIterator>(
        thd, thd->mem_root, Mem_root_array<TABLE *>{table},
        table->unique_result.io_cache, ignore_not_found_rows,
        /*has_null_flags=*/false,
        /*examined_rows=*/nullptr);
    table->unique_result.io_cache =
        nullptr;  // Now owned by SortFileIndirectIterator.
  } else if (table->unique_result.has_result_in_memory()) {
    /*
      The Unique class never puts its results into table->sort's
      Filesort_buffer.
    */
    assert(!table->unique_result.sorted_result_in_fsbuf);
    DBUG_PRINT("info", ("using SortBufferIndirectIterator (unique)"));
    iterator = NewIterator<SortBufferIndirectIterator>(
        thd, thd->mem_root, Mem_root_array<TABLE *>{table},
        &table->unique_result, ignore_not_found_rows, /*has_null_flags=*/false,
        /*examined_rows=*/nullptr);
  } else {
    AccessPath *path = create_table_access_path(
        thd, table, range_scan, table_ref, position, count_examined_rows);
    iterator = CreateIteratorFromAccessPath(thd, path,
                                            /*join=*/nullptr,
                                            /*eligible_for_batch_mode=*/false);
  }
  if (iterator->Init()) {
    return nullptr;
  }
  return iterator;
}
