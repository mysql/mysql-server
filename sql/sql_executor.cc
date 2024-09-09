/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "field_types.h"
#include "mem_root_deque.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_byteorder.h"
#include "my_checksum.h"
#include "my_dbug.h"
#include "my_hash_combine.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/my_loglevel.h"
#include "mysql/strings/m_ctype.h"
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
#include "sql/iterators/basic_row_iterators.h"
#include "sql/iterators/row_iterator.h"
#include "sql/iterators/timing_iterator.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/cost_model.h"
#include "sql/join_optimizer/join_optimizer.h"
#include "sql/join_optimizer/materialize_path_parameters.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/join_optimizer/replace_item.h"
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
#include "sql/query_term.h"
#include "sql/record_buffer.h"  // Record_buffer
#include "sql/sort_param.h"
#include "sql/sql_array.h"  // Bounds_checked_array
#include "sql/sql_class.h"
#include "sql/sql_cmd.h"
#include "sql/sql_const.h"
#include "sql/sql_delete.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_resolver.h"
#include "sql/sql_select.h"
#include "sql/sql_sort.h"
#include "sql/sql_tmp_table.h"  // create_tmp_table
#include "sql/sql_update.h"
#include "sql/table.h"
#include "sql/temp_table_param.h"
#include "sql/visible_fields.h"
#include "sql/window.h"
#include "template_utils.h"
#include "thr_lock.h"

using std::any_of;
using std::make_pair;
using std::max;
using std::min;
using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;

static int read_system(TABLE *table);
static bool alloc_group_fields(JOIN *join, ORDER *group);

/// The minimum size of the record buffer allocated by set_record_buffer().
/// If all the rows (estimated) can be accomodated with a smaller
/// buffer than the minimum size, we allocate only the required size.
/// Else, set_record_buffer() adjusts the size to the minimum size for
/// smaller ranges. This value shouldn't be too high, as benchmarks
/// have shown that a too big buffer can hurt performance in some
/// high-concurrency scenarios.
static constexpr size_t MIN_RECORD_BUFFER_SIZE = 4 * 1024;  // 4KB

/// The maximum size of the record buffer allocated by set_record_buffer().
/// Having a bigger buffer than this does not seem to give noticeably better
/// performance, and having a too big buffer has been seen to hurt performance
/// in high-concurrency scenarios.
static constexpr size_t MAX_RECORD_BUFFER_SIZE = 128 * 1024;  // 128KB

/// How big a fraction of the estimated number of returned rows to make room
/// for in the record buffer allocated by set_record_buffer(). The actual
/// size of the buffer will be adjusted to a value between
/// MIN_RECORD_BUFFER_SIZE and MAX_RECORD_BUFFER_SIZE if it falls outside of
/// this range. If all rows can be accomodated with a much smaller buffer
/// size than MIN_RECORD_BUFFER_SIZE, we only allocate the required size.
///
/// The idea behind using a fraction of the estimated number of rows, and not
/// just allocate a buffer big enough to hold all returned rows if they fit
/// within the maximum size, is that using big record buffers for small ranges
/// have been seen to hurt performance in high-concurrency scenarios. So we want
/// to pull the buffer size towards the minimum buffer size if the range is not
/// that large, while still pulling the buffer size towards the maximum buffer
/// size for large ranges and table scans.
///
/// The actual number is the result of an attempt to find the balance between
/// the advantages of big buffers in scenarios with low concurrency and/or large
/// ranges, and the disadvantages of big buffers in scenarios with high
/// concurrency. Increasing it could improve the performance of some queries
/// when the concurrency is low and hurt the performance if the concurrency is
/// high, and reducing it could have the opposite effect.
static constexpr double RECORD_BUFFER_FRACTION = 0.1f;

string RefToString(const Index_lookup &ref, const KEY &key,
                   bool include_nulls) {
  string ret;

  if (ref.keypart_hash != nullptr) {
    assert(!include_nulls);
    ret = key.key_part[0].field->field_name;
    ret += " = hash(";
    for (unsigned key_part_idx = 0; key_part_idx < ref.key_parts;
         ++key_part_idx) {
      if (key_part_idx != 0) {
        ret += ", ";
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
    const Field *field = key.key_part[key_part_idx].field;
    if (field->is_field_for_functional_index()) {
      // Do not print out the column name if the column represents a functional
      // index. Instead, print out the indexed expression.
      ret += ItemToString(field->gcol_info->expr_item);
    } else {
      assert(!field->is_hidden_by_system());
      ret += field->field_name;
    }
    ret += " = ";
    ret += ItemToString(ref.items[key_part_idx]);

    // If we have ref_or_null access, find out if this keypart is the one that
    // is -or-NULL (there's always only a single one).
    if (include_nulls && key_buff == ref.null_ref_key) {
      ret += " or NULL";
    }
    key_buff += key.key_part[key_part_idx].store_length;
  }
  return ret;
}

#ifndef NDEBUG
[[maybe_unused]] static const char *cft_name(Copy_func_type type) {
  switch (type) {
    case CFT_ALL:
      return "CFT_ALL";
    case CFT_WF_FRAMING:
      return "CFT_WF_FRAMING";
    case CFT_WF_NON_FRAMING:
      return "CFT_WF_NON_FRAMING";
    case CFT_WF_NEEDS_PARTITION_CARDINALITY:
      return "CFT_WF_NEEDS_PARTITION_CARDINALITY";
    case CFT_WF_USES_ONLY_ONE_ROW:
      return "CFT_WF_USES_ONLY_ONE_ROW";
    case CFT_HAS_NO_WF:
      return "CFT_HAS_NO_WF";
    case CFT_HAS_WF:
      return "CFT_HAS_WF";
    case CFT_WF:
      return "CFT_WF";
    case CFT_FIELDS:
      return "CFT_FIELDS";
    default:
      assert(false);
  }
}
#endif

bool JOIN::create_intermediate_table(
    QEP_TAB *const tab, const mem_root_deque<Item *> &tmp_table_fields,
    ORDER_with_src &tmp_table_group, bool save_sum_fields, const char *alias) {
  DBUG_TRACE;
  THD_STAGE_INFO(thd, stage_creating_tmp_table);
  const bool windowing = m_windows.elements > 0;
  /*
    Pushing LIMIT to the temporary table creation is not applicable
    when there is ORDER BY or GROUP BY or aggregate/window functions, because
    in all these cases we need all result rows.
  */
  const ha_rows tmp_rows_limit =
      ((order.empty() || skip_sort_order) && tmp_table_group.empty() &&
       !windowing && !query_block->with_sum_func)
          ? m_select_limit
          : HA_POS_ERROR;

  tab->tmp_table_param =
      new (thd->mem_root) Temp_table_param(thd->mem_root, tmp_table_param);
  tab->tmp_table_param->skip_create_table = true;

  const bool distinct_arg =
      select_distinct &&
      // GROUP BY is absent or has been done in a previous step
      group_list.empty() &&
      // We can only do DISTINCT in last window's tmp table step
      (!windowing || (tab->tmp_table_param->m_window &&
                      tab->tmp_table_param->m_window->is_last()));

  TABLE *table = create_tmp_table(
      thd, tab->tmp_table_param, tmp_table_fields, tmp_table_group.order,
      distinct_arg, save_sum_fields, query_block->active_options(),
      tmp_rows_limit, (alias != nullptr ? alias : ""));
  if (!table) return true;
  tmp_table_param.using_outer_summary_function =
      tab->tmp_table_param->using_outer_summary_function;

  assert(tab->idx() > 0);
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

bool is_rollup_group_wrapper(const Item *item) {
  return item->type() == Item::FUNC_ITEM &&
         down_cast<const Item_func *>(item)->functype() ==
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
    assert(m_ordered_index_usage == ORDERED_INDEX_ORDER_BY);
    if (m_ordered_index_usage == ORDERED_INDEX_ORDER_BY) {
      order.clean();
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
                 Item_func::MULTI_EQ_FUNC) {
    Item_multi_eq *item_equal = down_cast<Item_multi_eq *>(cond);
    const bool contained_const = item_equal->const_arg() != nullptr;
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
               use && use->table_ref == item_field.m_table_ref; use++) {
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

void setup_tmptable_write_func(QEP_TAB *tab, Opt_trace_object *trace) {
  DBUG_TRACE;
  JOIN *join = tab->join();
  TABLE *table = tab->table();
  Temp_table_param *const tmp_tbl = tab->tmp_table_param;
  const uint phase = tab->ref_item_slice;
  const char *description = nullptr;
  assert(table);

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

  ha_rows expected_rows =
      static_cast<ha_rows>(std::ceil(expected_rows_to_fetch));
  ha_rows rows_in_buffer = expected_rows;

  /*
    How much space do we need to allocate for each record? Enough to
    hold all columns from the beginning and up to the last one in the
    read set. We don't need to allocate space for unread columns at
    the end of the record.
  */
  const size_t record_size = record_prefix_size(table);

  if (record_size > 0) {
    const ha_rows min_rows =
        std::ceil(double{MIN_RECORD_BUFFER_SIZE} / record_size);
    // If the expected rows to fetch can be accomodated with a
    // lesser buffer size than MIN_RECORD_BUFFER_SIZE, we allocate
    // only the required size.
    if (expected_rows < min_rows) {
      rows_in_buffer = expected_rows;
    } else {
      rows_in_buffer = std::ceil(rows_in_buffer * RECORD_BUFFER_FRACTION);
      // Adjust the number of rows, if necessary, to fit within the
      // minimum and maximum buffer size range.
      const ha_rows local_max_rows = (MAX_RECORD_BUFFER_SIZE / record_size);
      rows_in_buffer = std::clamp(rows_in_buffer, min_rows, local_max_rows);
    }
  }

  // After adjustments made above, we still need a minimum of 2 rows to
  // use a record buffer.
  if (rows_in_buffer <= 1) {
    return false;
  }

  const auto bufsize = Record_buffer::buffer_size(rows_in_buffer, record_size);
  const auto ptr = pointer_cast<uchar *>(current_thd->alloc(bufsize));
  if (ptr == nullptr) return true; /* purecov: inspected */

  table->m_record_buffer = Record_buffer{rows_in_buffer, record_size, ptr};
  table->file->ha_set_record_buffer(&table->m_record_buffer);
  return false;
}

bool ExtractConditions(Item *condition,
                       Mem_root_array<Item *> *condition_parts) {
  return WalkConjunction(condition, [condition_parts](Item *item) {
    return condition_parts->push_back(item);
  });
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
  if (items->size() == 1) {
    return items->head();
  }
  Item_cond_and *condition = new Item_cond_and(*items);
  condition->quick_fix_field();
  condition->update_used_tables();
  condition->apply_is_true();
  return condition;
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
  invalidator->set_cost(path->cost());

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
  for (size_t idx : BitsSetIn(tables)) {
    assert(idx < join->tables);
    map |= join->qep_tab[idx].table_ref->map();
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
      Item_multi_eq *item_eq = find_item_equal(
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
  const table_map used_tables = item->used_tables();
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
    const table_map used_tables = item->used_tables();
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
  const table_map build_table_map = ConvertQepTabMapToTableMap(
      current_table->join(), current_table->idx_map());
  const table_map probe_table_map =
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
  const plan_idx outer_join_end =
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

static AccessPath *GetAccessPathForDerivedTable(THD *thd, QEP_TAB *qep_tab,
                                                AccessPath *table_path) {
  return GetAccessPathForDerivedTable(
      thd, qep_tab->table_ref, qep_tab->table(), qep_tab->rematerialize,
      qep_tab->invalidators, /*need_rowid=*/false, table_path);
}

/**
   Recalculate the cost of 'path'.
   @param thd Current thread.
   @param path the access path for which we update the cost numbers.
   @param outer_query_block the query block to which 'path' belongs.
*/
static void RecalculateTablePathCost(THD *thd, AccessPath *path,
                                     const Query_block &outer_query_block) {
  switch (path->type) {
    case AccessPath::FILTER: {
      const AccessPath &child = *path->filter().child;
      path->set_num_output_rows(child.num_output_rows());
      path->set_init_cost(child.init_cost());

      const FilterCost filterCost =
          EstimateFilterCost(current_thd, path->num_output_rows(),
                             path->filter().condition, &outer_query_block);

      path->set_cost(child.cost() +
                     (path->filter().materialize_subqueries
                          ? filterCost.cost_if_materialized
                          : filterCost.cost_if_not_materialized));
    } break;

    case AccessPath::SORT:
      EstimateSortCost(thd, path);
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

    case AccessPath::WINDOW:
      EstimateWindowCost(path);
      break;

    default:
      assert(false);
  }
}

AccessPath *MoveCompositeIteratorsFromTablePath(
    THD *thd, AccessPath *path, const Query_block &outer_query_block) {
  assert(path->cost() >= 0.0);
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
      case AccessPath::DYNAMIC_INDEX_RANGE_SCAN:
        // We found our real bottom.
        path->materialize().table_path = sub_path;
        if (explain) {
          EstimateMaterializeCost(current_thd, path);
        }
        return true;
      case AccessPath::SAMPLE_SCAN: /* LCOV_EXCL_LINE */
        // SampleScan can be executed only in the secondary engine.
        assert(false); /* LCOV_EXCL_LINE */
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
        assert(bottom_of_table_path->materialize().param->m_operands.size() ==
               1);
        bottom_of_table_path->materialize().param->m_operands[0].subquery_path =
            path;
        break;
      case AccessPath::WINDOW:
        bottom_of_table_path->window().child = path;
        break;
      default:
        assert(false);
    }

    path = table_path;
  }

  if (explain) {
    // Update cost from the bottom and up, so that the cost of each path
    // includes the cost of its descendants.
    for (auto ancestor = ancestor_paths.end() - 1;
         ancestor >= ancestor_paths.begin(); ancestor--) {
      RecalculateTablePathCost(thd, *ancestor, outer_query_block);
    }
  }

  return path;
}

/**
   Find the bottom of 'table_path', i.e. the path that actually accesses the
   materialized table.
*/
static AccessPath *GetTablePathBottom(AccessPath *table_path) {
  AccessPath *bottom{nullptr};

  const auto find_bottom{[&](AccessPath *path, const JOIN *) {
    switch (path->type) {
      case AccessPath::TABLE_SCAN:
        assert(path->table_scan().table->pos_in_table_list == nullptr ||
               path->table_scan()
                   .table->pos_in_table_list->uses_materialization());
        bottom = path;
        return true;

      case AccessPath::REF:
        assert(path->ref().table->pos_in_table_list == nullptr ||
               path->ref().table->pos_in_table_list->uses_materialization());
        bottom = path;
        return true;

      default:
        return false;
    }
  }};

  WalkAccessPaths(table_path, /*join=*/nullptr,
                  WalkAccessPathPolicy::STOP_AT_MATERIALIZATION, find_bottom);

  assert(bottom != nullptr);
  return bottom;
}

AccessPath *GetAccessPathForDerivedTable(
    THD *thd, Table_ref *table_ref, TABLE *table, bool rematerialize,
    Mem_root_array<const AccessPath *> *invalidators, bool need_rowid,
    AccessPath *table_path) {
  // Make a new path if none is cached.
  const auto make_path{[&]() {
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
      tmp_table_param->items_to_copy = nullptr;
    } else if (query_expression->set_operation()->is_materialized()) {
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
      // our result table. This saves us from doing double materialization
      // (first into a UNION result table, then from there into our own).
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
          thd, path, *query_expression->outer_query_block());
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
          thd, path, *query_expression->outer_query_block());
    }

    path->set_cost_before_filter(path->cost());
    path->num_output_rows_before_filter = path->num_output_rows();
    return path;
  }};

  if (thd->lex->using_hypergraph_optimizer()) {
    // For the Hypergraph optimizer there may be several alternative table paths
    // cached.
    AccessPath *const table_path_bottom{GetTablePathBottom(table_path)};
    AccessPath *const cached_path{
        table_ref->GetCachedMaterializedPath(table_path_bottom)};

    if (cached_path == nullptr) {
      AccessPath *const new_path{make_path()};
      table_ref->AddMaterializedPathToCache(thd, new_path, table_path_bottom);
      return new_path;
    } else {
      if (cached_path->type == AccessPath::MATERIALIZE) {
        cached_path->materialize().table_path->set_num_output_rows(
            cached_path->num_output_rows());
      }
      return cached_path;
    }
  } else {
    return make_path();
  }
}

/**
  Get the RowIterator used for scanning the given table, with any required
  materialization operations done first.
 */
static AccessPath *GetTableAccessPath(THD *thd, QEP_TAB *qep_tab,
                                      QEP_TAB *qep_tabs) {
  AccessPath *table_path;
  if (qep_tab->materialize_table == QEP_TAB::MATERIALIZE_DERIVED) {
    table_path =
        GetAccessPathForDerivedTable(thd, qep_tab, qep_tab->access_path());
  } else if (qep_tab->materialize_table ==
             QEP_TAB::MATERIALIZE_TABLE_FUNCTION) {
    table_path = NewMaterializedTableFunctionAccessPath(
        thd, qep_tab->table(), qep_tab->table_ref->table_function,
        qep_tab->access_path());

    CopyBasicProperties(*qep_tab->access_path(), table_path);
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

    const int join_start = sjm->inner_table_index;
    const int join_end = join_start + sjm->table_count;

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

    const bool copy_items_in_materialize =
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
    table_path = MoveCompositeIteratorsFromTablePath(
        thd, table_path, *qep_tab->join()->query_block);

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
  const double num_rows_after_filtering =
      pos->rows_fetched * pos->filter_effect;
  if (is_after_filter) {
    path->set_num_output_rows(num_rows_after_filtering);
  } else {
    path->set_num_output_rows(pos->rows_fetched);
  }

  // Note that we don't try to adjust for the filtering here;
  // we estimate the same cost as the table itself.
  const double cost =
      pos->read_cost + cost_model.row_evaluate_cost(num_rows_after_filtering);
  if (pos->prefix_rowcount <= 0.0) {
    path->set_cost(cost);
  } else {
    // Scale the estimated cost to being for one loop only, to match the
    // measured costs.
    path->set_cost(cost * num_rows_after_filtering / pos->prefix_rowcount);
  }
  path->set_init_cost(kUnknownCost);
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
  const double inner_expected_rows_before_filter =
      pos_inner->filter_effect > 0.0
          ? (inner->num_output_rows() / pos_inner->filter_effect)
          : 0.0;
  const double joined_rows =
      outer->num_output_rows() * inner_expected_rows_before_filter;
  path->set_num_output_rows(joined_rows * pos_inner->filter_effect);
  path->set_cost(outer->cost() + pos_inner->read_cost +
                 cost_model.row_evaluate_cost(joined_rows));
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
  const double joined_rows =
      outer->num_output_rows() * inner->num_output_rows();
  path->set_num_output_rows(joined_rows * pos_outer->filter_effect);
  path->set_cost(inner->cost() + pos_outer->read_cost +
                 cost_model.row_evaluate_cost(joined_rows));
}

static bool ConditionIsAlwaysTrue(Item *item) {
  return item->const_item() && item->val_bool();
}

/// Find all the tables below "path" that have been pruned and replaced by a
/// ZERO_ROWS access path.
static table_map GetPrunedTables(const AccessPath *path) {
  table_map pruned_tables = 0;

  WalkAccessPaths(
      path, /*join=*/nullptr, WalkAccessPathPolicy::STOP_AT_MATERIALIZATION,
      [&pruned_tables](const AccessPath *subpath, const JOIN *) {
        if (subpath->type == AccessPath::ZERO_ROWS) {
          pruned_tables |=
              GetUsedTableMap(subpath, /*include_pruned_tables=*/true);
          return true;  // Stop recursing into this subtree.
        }
        return false;
      });

  return pruned_tables;
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
  const table_map left_table_map =
      ConvertQepTabMapToTableMap(qep_tab->join(), probe_tables);
  const table_map right_table_map =
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

        if (func_item->contains_only_equi_join_condition()) {
          Item_eq_base *join_condition = down_cast<Item_eq_base *>(func_item);
          if (IsHashEquijoinCondition(join_condition, left_table_map,
                                      right_table_map)) {
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
  // see would be NULL-complemented rows, so if the join condition is
  // NULL-rejecting on the pruned table, it will never match.
  // In this case, we can remove the entire build path (ie., propagate the
  // zero-row property to our own join).
  //
  // We also remove the join conditions, to avoid using time on extracting their
  // hash values. (Also, Item_eq_base::append_join_key_for_hash_join has an
  // assert that this case should never happen, so it would trigger.)
  if (const table_map pruned_tables =
          GetPrunedTables(probe_path) | GetPrunedTables(build_path);
      pruned_tables != 0 &&
      any_of(hash_join_conditions.begin(), hash_join_conditions.end(),
             [pruned_tables](const HashJoinCondition &condition) {
               return Overlaps(pruned_tables,
                               condition.join_condition()->not_null_tables());
             })) {
    if (build_path->type != AccessPath::ZERO_ROWS) {
      build_path = NewZeroRowsAccessPath(
          thd, build_path, "Join condition requires pruned table");
    }
    expr->equijoin_conditions.clear();
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
    const Substructure substructure =
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
      const qep_tab_map left_tables = TablesBetween(first_idx, i);
      const qep_tab_map right_tables = TablesBetween(i, substructure_end);

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

    const qep_tab_map right_tables = qep_tab->idx_map();
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
      const double expected_rows = table_path->num_output_rows();
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
AccessPath *JOIN::attach_access_path_for_update_or_delete(
    AccessPath *path) const {
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
        GetImmediateUpdateTable(this, std::has_single_bit(target_tables)));
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

static AccessPath *add_filter_access_path(THD *thd, AccessPath *path,
                                          Item *condition,
                                          const Query_block *query_block) {
  AccessPath *filter_path = NewFilterAccessPath(thd, path, condition);
  CopyBasicProperties(*path, filter_path);
  if (thd->lex->using_hypergraph_optimizer()) {
    // We cannot call EstimateFilterCost() in the pre-hypergraph optimizer,
    // as on repeated execution of a prepared query, the condition may contain
    // references to subqueries that are destroyed and not re-optimized yet.
    const FilterCost filter_cost = EstimateFilterCost(
        thd, filter_path->num_output_rows(), condition, query_block);
    filter_path->set_cost(filter_path->cost() +
                          filter_cost.cost_if_not_materialized);
    filter_path->set_init_cost(filter_path->init_cost() +
                               filter_cost.init_cost_if_not_materialized);
  }
  return filter_path;
}

AccessPath *JOIN::create_root_access_path_for_join() {
  if (select_count) {
    return NewUnqualifiedCountAccessPath(thd);
  }

  // OK, so we're good. Go through the tables and make the join access paths.
  AccessPath *path = nullptr;
  if (query_block->is_table_value_constructor) {
    best_rowcount = query_block->row_value_list->size();
    path = NewTableValueConstructorAccessPath(thd, this);
    path->set_num_output_rows(best_rowcount);
    path->set_cost(0.0);
    path->set_init_cost(0.0);
    // Table value constructors may get a synthetic WHERE clause from an
    // IN-to-EXISTS transformation. If so, add a filter for it.
    if (where_cond != nullptr) {
      path = add_filter_access_path(thd, path, where_cond, query_block);
    }
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
        path = NewAggregateAccessPath(thd, path, query_block->olap);
        EstimateAggregateCost(thd, path, query_block);
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
        const Switch_ref_item_slice slice_switch(this, qep_tab->ref_item_slice);
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
          thd, path, /*join=*/this, qep_tab->tmp_table_param, qep_tab->table(),
          table_path, qep_tab->ref_item_slice);
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
      path = NewAggregateAccessPath(thd, path, query_block->olap);
      EstimateAggregateCost(thd, path, query_block);
    }
  }

  return path;
}

AccessPath *JOIN::attach_access_paths_for_having_and_limit(
    AccessPath *path) const {
  // Attach HAVING and LIMIT if needed.
  // NOTE: We can have HAVING even without GROUP BY, although it's not very
  // useful.
  // We don't currently bother with materializing subqueries
  // in HAVING, as they should be rare.
  if (having_cond != nullptr) {
    path = add_filter_access_path(thd, path, having_cond, query_block);
  }

  Query_expression *const qe = query_expression();

  // For IN/EXISTS subqueries, it's ok to optimize by adding LIMIT 1 for top
  // level of a set operation in the presence of EXCEPT ALL, but not in nested
  // set operands, lest we lose rows significant to the result of the EXCEPT
  // ALL.
  const bool skip_limit =
      (qe->m_contains_except_all &&
       // check that qe inside subquery: no user given
       // limit/offset can interfere,
       // cf. ER_NOT_SUPPORTED_YET("LIMIT & IN/ALL/ANY/SOME
       // subquery"), so safe to assume the limit is the
       // optimization we want to suppress
       qe->item != nullptr && qe->query_term()->query_block() != query_block);

  // Note: For select_count, LIMIT 0 is handled in JOIN::optimize() for the
  // common case, but not for CALC_FOUND_ROWS. OFFSET also isn't handled there.
  if ((qe->select_limit_cnt != HA_POS_ERROR && !skip_limit) ||
      qe->offset_limit_cnt != 0) {
    path =
        NewLimitOffsetAccessPath(thd, path, qe->select_limit_cnt,
                                 qe->offset_limit_cnt, calc_found_rows, false,
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

  if (!check_unique_fields(sjtbl->tmp_table)) return 1;
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
  return 0;
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
    assert(!tab->join_cond()->cost().IsExpensive());
    if (tab->join_cond()->val_int() == 0) table->set_null_row();
    if (thd->is_error()) return 1;
  }

  /* Check appearance of new constant items in Item_multi_eq objects */
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
  return table->has_row() ? 0 : -1;
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

AccessPath *QEP_TAB::access_path() {
  assert(table());
  // Only some access methods support reversed access:
  assert(!m_reversed_access || type() == JT_REF || type() == JT_INDEX_SCAN);
  Index_lookup *used_ref = nullptr;
  AccessPath *path = nullptr;

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

    case JT_CONST:
      path = NewConstTableAccessPath(join()->thd, table(), &ref(),
                                     /*count_examined_rows=*/true);
      break;

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
  }

  if (position() != nullptr) {
    SetCostOnTableAccessPath(*join()->thd->cost_model(), position(),
                             /*is_after_filter=*/false, path);
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
        // At least one condition guard is relevant, so we need to use
        // the AlternativeIterator.
        AccessPath *table_scan_path = NewTableScanAccessPath(
            join()->thd, table(), /*count_examined_rows=*/true);
        table_scan_path->set_num_output_rows(table()->file->stats.records);
        table_scan_path->set_cost(
            table()->file->table_scan_cost().total_cost());
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

static bool cmp_field_value(Field *field, ptrdiff_t diff) {
  assert(field);
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
    const bool err = json_field->val_json(&right_wrapper);
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
  const ptrdiff_t diff = rec1 - rec0;

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

bool table_rec_cmp(TABLE *table) {
  DBUG_TRACE;
  const ptrdiff_t diff = table->record[1] - table->record[0];
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

ulonglong calc_field_hash(const Field *field, ulonglong *hash_val) {
  uint64_t seed1 = 0, seed2 = 4;
  uint64_t crc = *hash_val;

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
    my_hash_combine(crc, seed1);
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

  for (ORDER *ord = group; ord; ord = ord->next) {
    Field *field = ord->field_in_tmp_table;
    assert(field);
    calc_field_hash(field, &crc);
  }

  return crc;
}

/**
  Generate hash for unique_constraint for all visible fields of a table
  @param table the table for which we want a hash of its fields
  @return the hash value
*/
ulonglong calc_row_hash(TABLE *table) {
  ulonglong crc = 0;
  Field **fields = table->visible_field_ptr();

  for (uint i = 0; i < table->visible_field_count(); i++)
    calc_field_hash(fields[i], &crc);

  return crc;
}

/**
  Check whether a row is already present in the tmp table

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

bool check_unique_fields(TABLE *table) {
  ulonglong hash;

  if (!table->hash_field) return true;

  if (table->no_keyread) return true;

  if (table->group)
    hash = unique_hash_group(table->group);
  else
    hash = calc_row_hash(table);
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

bool construct_lookup(THD *thd, TABLE *table, Index_lookup *ref) {
  const enum enum_check_fields save_check_for_truncated_fields =
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
size_t compute_ria_idx(const mem_root_deque<Item *> &fields, size_t i,
                       size_t added_non_hidden_fields, size_t border) {
  const size_t num_select_elements = fields.size() - border;
  const size_t orig_num_select_elements =
      num_select_elements - added_non_hidden_fields;
  size_t idx;

  if (i < border) {
    idx = fields.size() - i - 1 - added_non_hidden_fields;
  } else {
    idx = i - border;
    if (idx >= orig_num_select_elements) idx += border;
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

  DBUG_PRINT("enter", ("for param %p", param));
  for (Copy_field &ptr : param->copy_fields) ptr.invoke_do_copy(reverse_copy);

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
  if (!item->has_grouping_set_dep()) {
    return false;
  }
  const auto replace_functor = [thd, item, fields](Item *sub_item, Item *,
                                                   unsigned) -> ReplaceResult {
    if (!is_rollup_group_wrapper(sub_item)) {
      return {ReplaceResult::KEEP_TRAVERSING, nullptr};
    }
    for (Item *other_item : *fields) {
      if (other_item->eq(sub_item)) {
        Field *field = other_item->get_tmp_table_field();
        Item *item_field = new (thd->mem_root) Item_field(field);
        if (item_field == nullptr) return {ReplaceResult::ERROR, nullptr};
        item_field->item_name = item->item_name;
        return {ReplaceResult::REPLACE, item_field};
      }
    }
    // A const item that is part of group by and not found in
    // select list will not be found in "fields" (It's not added
    // as a hidden item).
    if (unwrap_rollup_group(sub_item)->const_for_execution()) {
      return {ReplaceResult::REPLACE, sub_item};
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
  @param windowing                   true if creating a tmp table for windowing
                                     materialization
  @returns false if success, true if error
*/

bool change_to_use_tmp_fields(mem_root_deque<Item *> *fields, THD *thd,
                              Ref_item_array ref_item_array,
                              mem_root_deque<Item *> *res_fields,
                              size_t added_non_hidden_fields, bool windowing) {
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
             ((Item_func *)item)->functype() == Item_func::SUSERVAR_FUNC &&
             (!windowing || item->has_wf())) {
      field = item->get_tmp_table_field();
      if (field != nullptr) {
        /*
          Replace "@:=<expr>" with "@:=<tmp_table_column>" rather than
          "<tmp_table_column>".
          We do not perform the special handling for tmp tables used for
          windowing, though.
        */
        new_item = ReplaceSetVarItem(thd, item, new Item_field(field));
        if (new_item == nullptr) return true;
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
      if (orig_field != nullptr && item != new_item) {
        down_cast<Item_field *>(new_item)->set_original_table_name(
            orig_field->original_table_name());
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

static Item_rollup_group_item *find_rollup_item_in_group_list(
    Item *item, Query_block *query_block) {
  for (ORDER *group = query_block->group_list.first; group;
       group = group->next) {
    Item_rollup_group_item *rollup_item = group->rollup_item;
    // If we have duplicate fields in group by
    // (E.g. GROUP BY f1,f1,f2), rollup_item is set only for
    // the first field.
    if (rollup_item != nullptr) {
      if (item->eq(rollup_item)) {
        return rollup_item;
      }
    }
  }
  return nullptr;
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

        Item_rollup_group_item *group_rollup_item =
            find_rollup_item_in_group_list(rollup_item, select);
        assert(group_rollup_item != nullptr);
        Item_rollup_group_item *new_item = new Item_rollup_group_item(
            rollup_item->min_rollup_level(),
            group_rollup_item->inner_item()->get_tmp_table_item(thd));
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

      Item_rollup_group_item *group_rollup_item =
          find_rollup_item_in_group_list(rollup_item, select);
      assert(group_rollup_item != nullptr);
      group_rollup_item->inner_item()->set_result_field(
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
                 0)) ||                         // (1)
               item->has_grouping_set_dep()) {  // (2)
      /*
        We go here when:
        (1) The Query_block is implicitly grouped and 'item' does not
            depend on any table. Then that field should be evaluated exactly
            once, whether there are zero or more rows in the temporary table
            (@see create_tmp_table()).
        (2) 'item' has a group by modifier. Then we delay processing
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
    if (!is_rollup_group_wrapper(item) && item->has_grouping_set_dep()) {
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

bool JOIN::clear_fields(table_map *save_nullinfo) {
  assert(*save_nullinfo == 0);

  for (Table_ref *table_ref = tables_list; table_ref != nullptr;
       table_ref = table_ref->next_leaf) {
    TABLE *const table = table_ref->table;
    if (!table->has_null_row()) {
      *save_nullinfo |= table_ref->map();
      if (table->const_table) table->save_null_flags();
      table->set_null_row();  // All fields are NULL
    }
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
  assert(save_nullinfo);

  for (Table_ref *table_ref = tables_list; table_ref != nullptr;
       table_ref = table_ref->next_leaf) {
    if (save_nullinfo & table_ref->map()) {
      TABLE *const table = table_ref->table;
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
  For the given access path, set "count_examined_rows" to the value
  specified. For index merge scans, we set "count_examined_rows"
  for all the child paths too.
  @param path     Access path (A range scan)
  @param count_examined_rows See AccessPath::count_examined_rows.
*/
void set_count_examined_rows(AccessPath *path, bool count_examined_rows) {
  path->count_examined_rows = count_examined_rows;
  switch (path->type) {
    case AccessPath::INDEX_MERGE:
      for (AccessPath *child : *path->index_merge().children) {
        set_count_examined_rows(child, count_examined_rows);
      }
      break;
    case AccessPath::ROWID_INTERSECTION:
      for (AccessPath *child : *path->rowid_intersection().children) {
        set_count_examined_rows(child, count_examined_rows);
      }
      if (path->rowid_intersection().cpk_child != nullptr) {
        set_count_examined_rows(path->rowid_intersection().cpk_child,
                                count_examined_rows);
      }
      break;
    case AccessPath::ROWID_UNION:
      for (AccessPath *child : *path->rowid_union().children) {
        set_count_examined_rows(child, count_examined_rows);
      }
      break;
    default:
      return;
  }
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
    set_count_examined_rows(range_scan, count_examined_rows);
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
