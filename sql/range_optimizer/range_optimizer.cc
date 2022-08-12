/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

/*
  TODO:
  Fix that MAYBE_KEY are stored in the tree so that we can detect use
  of full hash keys for queries like:

  select s.id, kws.keyword_id from sites as s,kws where s.id=kws.site_id and
  kws.keyword_id in (204,205);

*/

/*
  This file contains:

  Range/index_merge/groupby-minmax optimizer module
    A module that accepts a table, condition, and returns
     - an AccessPath that can give a RowIterator, that can be used to retrieve
       rows that match the specified condition, or
     - a "no records will match the condition" statement.

    The module entry point is
      test_quick_select()


  KeyTupleFormat
  ~~~~~~~~~~~~~~
  The code in this file (and elsewhere) makes operations on key value tuples.
  Those tuples are stored in the following format:

  The tuple is a sequence of key part values. The length of key part value
  depends only on its type (and not depends on the what value is stored)

    KeyTuple: keypart1-data, keypart2-data, ...

  The value of each keypart is stored in the following format:

    keypart_data: [isnull_byte] keypart-value-bytes

  If a keypart may have a NULL value (key_part->field->is_nullable() can
  be used to check this), then the first byte is a NULL indicator with the
  following valid values:
    1  - keypart has NULL value.
    0  - keypart has non-NULL value.

  <questionable-statement> If isnull_byte==1 (NULL value), then the following
  keypart->length bytes must be 0.
  </questionable-statement>

  keypart-value-bytes holds the value. Its format depends on the field type.
  The length of keypart-value-bytes may or may not depend on the value being
  stored. The default is that length is static and equal to
  KEY_PART_INFO::length.

  Key parts with (key_part_flag & HA_BLOB_PART) have length depending of the
  value:

     keypart-value-bytes: value_length value_bytes

  The value_length part itself occupies HA_KEY_BLOB_LENGTH=2 bytes.

  See key_copy() and key_restore() for code to move data between index tuple
  and table record

  CAUTION: the above description is only sergefp's understanding of the
           subject and may omit some details.
*/

#include "sql/range_optimizer/range_optimizer.h"

#include <float.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <queue>
#include <set>

#include "field_types.h"  // enum_field_types
#include "m_ctype.h"
#include "m_string.h"
#include "my_alloc.h"
#include "my_bitmap.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_sqlcommand.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "scope_guard.h"
#include "sql/check_stack.h"
#include "sql/current_thd.h"
#include "sql/field_common_properties.h"
#include "sql/item.h"
#include "sql/item_func.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/key.h"  // is_key_used
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"
#include "sql/opt_costmodel.h"
#include "sql/opt_hints.h"  // hint_key_state
#include "sql/opt_trace.h"  // Opt_trace_array
#include "sql/opt_trace_context.h"
#include "sql/psi_memory_key.h"
#include "sql/range_optimizer/group_index_skip_scan_plan.h"
#include "sql/range_optimizer/index_merge_plan.h"
#include "sql/range_optimizer/index_range_scan_plan.h"
#include "sql/range_optimizer/index_skip_scan_plan.h"
#include "sql/range_optimizer/internal.h"
#include "sql/range_optimizer/path_helpers.h"
#include "sql/range_optimizer/range_analysis.h"
#include "sql/range_optimizer/range_opt_param.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/range_optimizer/rowid_ordered_retrieval_plan.h"
#include "sql/range_optimizer/tree.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_select.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thr_malloc.h"
#include "sql/uniques.h"  // Unique

using std::min;

static AccessPath *get_best_disjunct_quick(
    THD *thd, RANGE_OPT_PARAM *param, TABLE *table,
    bool index_merge_union_allowed, bool index_merge_sort_union_allowed,
    bool index_merge_intersect_allowed, bool skip_records_in_range,
    const MY_BITMAP *needed_fields, SEL_IMERGE *imerge, const double cost_est,
    Key_map *needed_reg);
#ifndef NDEBUG
static void print_quick(AccessPath *path, const Key_map *needed_reg);
#endif

namespace opt_range {
SEL_ARG *null_element = nullptr;
}
using namespace opt_range;

void range_optimizer_init() {
  null_element = new SEL_ARG;
  null_element->color =
      SEL_ARG::BLACK;  // Don't trip up the test in test_rb_tree.
}

void range_optimizer_free() { delete null_element; }

/*
  Add SEL_TREE to this index_merge without any checks,

  NOTES
    This function implements the following:
      (x_1||...||x_N) || t = (x_1||...||x_N||t), where x_i, t are SEL_TREEs

  RETURN
    true on OOM.
*/

bool SEL_IMERGE::or_sel_tree(SEL_TREE *tree) { return trees.push_back(tree); }

/*
  Perform OR operation on this SEL_IMERGE and supplied SEL_TREE new_tree,
  combining new_tree with one of the trees in this SEL_IMERGE if they both
  have SEL_ARGs for the same key.

  SYNOPSIS
    or_sel_tree_with_checks()
      param    RANGE_OPT_PARAM from test_quick_select
      remove_jump_scans See get_mm_tree()
      new_tree SEL_TREE with type KEY or KEY_SMALLER.

  NOTES
    This does the following:
    (t_1||...||t_k)||new_tree =
     either
       = (t_1||...||t_k||new_tree)
     or
       = (t_1||....||(t_j|| new_tree)||...||t_k),

     where t_i, y are SEL_TREEs.
    new_tree is combined with the first t_j it has a SEL_ARG on common
    key with. As a consequence of this, choice of keys to do index_merge
    read may depend on the order of conditions in WHERE part of the query.

  RETURN
    0  OK
    1  One of the trees was combined with new_tree to SEL_TREE::ALWAYS,
       and (*this) should be discarded.
   -1  An error occurred.
*/

int SEL_IMERGE::or_sel_tree_with_checks(RANGE_OPT_PARAM *param,
                                        bool remove_jump_scans,
                                        SEL_TREE *new_tree) {
  DBUG_TRACE;
  for (SEL_TREE *&tree : trees) {
    if (sel_trees_can_be_ored(tree, new_tree, param)) {
      tree = tree_or(param, remove_jump_scans, tree, new_tree);
      if (tree == nullptr) return 1;
      if (tree->type == SEL_TREE::ALWAYS) return 1;
      /* SEL_TREE::IMPOSSIBLE is impossible here */
      return 0;
    }
  }

  /* New tree cannot be combined with any of existing trees. */
  if (or_sel_tree(new_tree)) {
    return -1;
  } else {
    return 0;
  }
}

/*
  Perform OR operation on this index_merge and supplied index_merge list.

  RETURN
    0 - OK
    1 - One of conditions in result is always true and this SEL_IMERGE
        should be discarded.
   -1 - An error occurred
*/

int SEL_IMERGE::or_sel_imerge_with_checks(RANGE_OPT_PARAM *param,
                                          bool remove_jump_scans,
                                          SEL_IMERGE *imerge) {
  for (SEL_TREE *tree : imerge->trees) {
    int ret = or_sel_tree_with_checks(param, remove_jump_scans, tree);
    if (ret != 0) {
      return ret;
    }
  }
  return 0;
}

SEL_IMERGE::SEL_IMERGE(SEL_IMERGE *arg, RANGE_OPT_PARAM *param)
    : trees(param->temp_mem_root, arg->trees) {}

void trace_quick_description(const AccessPath *path, Opt_trace_context *trace) {
  Opt_trace_object range_trace(trace, "range_details");

  String range_info;
  range_info.set_charset(system_charset_info);
  add_info_string(path, &range_info);
  range_trace.add_utf8("used_index", range_info.ptr(), range_info.length());
}

QUICK_RANGE::QUICK_RANGE()
    : min_key(nullptr),
      max_key(nullptr),
      min_length(0),
      max_length(0),
      flag(NO_MIN_RANGE | NO_MAX_RANGE),
      rkey_func_flag(HA_READ_INVALID),
      min_keypart_map(0),
      max_keypart_map(0) {}

QUICK_RANGE::QUICK_RANGE(MEM_ROOT *mem_root, const uchar *min_key_arg,
                         uint min_length_arg, key_part_map min_keypart_map_arg,
                         const uchar *max_key_arg, uint max_length_arg,
                         key_part_map max_keypart_map_arg, uint flag_arg,
                         enum ha_rkey_function rkey_func_flag_arg)
    : min_key(nullptr),
      max_key(nullptr),
      min_length((uint16)min_length_arg),
      max_length((uint16)max_length_arg),
      flag((uint16)flag_arg),
      rkey_func_flag(rkey_func_flag_arg),
      min_keypart_map(min_keypart_map_arg),
      max_keypart_map(max_keypart_map_arg) {
  min_key = mem_root->ArrayAlloc<uchar>(min_length_arg + 1);
  max_key = mem_root->ArrayAlloc<uchar>(max_length_arg + 1);
  if (min_key != nullptr) {
    memcpy(min_key, min_key_arg, min_length_arg + 1);
  }
  if (max_key != nullptr) {
    memcpy(max_key, max_key_arg, max_length_arg + 1);
  }
}

/*
  Fill needed_fields with bitmap of fields used in the query.
  SYNOPSIS
    fill_used_fields_bitmap()
      param Parameter from test_quick_select function.

  NOTES
    Clustered PK members are not put into the bitmap as they are implicitly
    present in all keys (and it is impossible to avoid reading them).
  RETURN
    0  Ok
    1  Out of memory.
*/

static int fill_used_fields_bitmap(RANGE_OPT_PARAM *param,
                                   MY_BITMAP *needed_fields) {
  TABLE *table = param->table;
  my_bitmap_map *tmp;
  uint pk;
  if (!(tmp = (my_bitmap_map *)param->return_mem_root->Alloc(
            table->s->column_bitmap_size)) ||
      bitmap_init(needed_fields, tmp, table->s->fields))
    return 1;

  bitmap_copy(needed_fields, table->read_set);
  bitmap_union(needed_fields, table->write_set);

  pk = param->table->s->primary_key;
  if (pk != MAX_KEY && param->table->file->primary_key_is_clustered()) {
    /* The table uses clustered PK and it is not internally generated */
    KEY_PART_INFO *key_part = param->table->key_info[pk].key_part;
    KEY_PART_INFO *key_part_end =
        key_part + param->table->key_info[pk].user_defined_key_parts;
    for (; key_part != key_part_end; ++key_part)
      bitmap_clear_bit(needed_fields, key_part->fieldnr - 1);
  }
  return 0;
}

bool setup_range_optimizer_param(THD *thd, MEM_ROOT *return_mem_root,
                                 MEM_ROOT *temp_mem_root, Key_map keys_to_use,
                                 TABLE *table, Query_block *query_block,
                                 RANGE_OPT_PARAM *param) {
  param->table = table;
  param->query_block = query_block;
  param->keys = 0;
  param->return_mem_root = return_mem_root;
  param->temp_mem_root = temp_mem_root;
  param->using_real_indexes = true;
  param->use_index_statistics = false;

  temp_mem_root->set_max_capacity(thd->variables.range_optimizer_max_mem_size);
  temp_mem_root->set_error_for_capacity_exceeded(true);

  // These are being stored in AccessPaths, so they need to be on
  // return_mem_root.
  param->real_keynr = return_mem_root->ArrayAlloc<uint>(table->s->keys);
  param->key = return_mem_root->ArrayAlloc<KEY_PART *>(table->s->keys);
  param->key_parts = return_mem_root->ArrayAlloc<KEY_PART>(table->s->key_parts);
  if (param->real_keynr == nullptr || param->key == nullptr ||
      param->key_parts == nullptr) {
    return true;  // Can't use range
  }
  KEY_PART *key_parts = param->key_parts;

  Opt_trace_context *const trace = &thd->opt_trace;
  {
    Opt_trace_array trace_idx(trace, "potential_range_indexes",
                              Opt_trace_context::RANGE_OPTIMIZER);
    /*
      Make an array with description of all key parts of all table keys.
      This is used in get_mm_parts function.
    */
    KEY *key_info = table->key_info;
    for (uint idx = 0; idx < table->s->keys; idx++, key_info++) {
      Opt_trace_object trace_idx_details(trace);
      trace_idx_details.add_utf8("index", key_info->name);
      KEY_PART_INFO *key_part_info;

      if (!keys_to_use.is_set(idx)) {
        trace_idx_details.add("usable", false)
            .add_alnum("cause", "not_applicable");
        continue;
      }

      if (hint_key_state(thd, table->pos_in_table_list, idx, NO_RANGE_HINT_ENUM,
                         0)) {
        trace_idx_details.add("usable", false)
            .add_alnum("cause", "no_range_optimization hint");
        continue;
      }

      if (key_info->flags & HA_FULLTEXT) {
        trace_idx_details.add("usable", false).add_alnum("cause", "fulltext");
        continue;  // ToDo: ft-keys in non-ft ranges, if possible   SerG
      }

      trace_idx_details.add("usable", true);

      param->key[param->keys] = key_parts;
      key_part_info = key_info->key_part;
      Opt_trace_array trace_keypart(trace, "key_parts");
      for (uint part = 0; part < actual_key_parts(key_info);
           part++, key_parts++, key_part_info++) {
        key_parts->key = param->keys;
        key_parts->part = part;
        key_parts->length = key_part_info->length;
        key_parts->store_length = key_part_info->store_length;
        key_parts->field = key_part_info->field;
        key_parts->null_bit = key_part_info->null_bit;
        key_parts->image_type = (part < key_info->user_defined_key_parts &&
                                 key_info->flags & HA_SPATIAL)
                                    ? Field::itMBR
                                    : Field::itRAW;
        /* Only HA_PART_KEY_SEG is used */
        key_parts->flag = key_part_info->key_part_flag;
        trace_keypart.add_utf8(
            get_field_name_or_expression(thd, key_part_info->field));
      }
      param->real_keynr[param->keys++] = idx;
    }
  }
  param->key_parts_end = key_parts;
  return false;
}

/*
  Test if a key can be used in different ranges, and create the QUICK
  access method (range, index merge etc) that is estimated to be
  cheapest unless table/index scan is even cheaper (exception: @see
  parameter force_quick_range).

  SYNOPSIS
    test_quick_select()
      thd               Current thread
      return_mem_root   MEM_ROOT to allocate AccessPaths, RowIterators and
                        dependent information on (ie., permanent artifacts
                        that must live on after the range optimizer
                        has finished executing).
      temp_mem_root     MEM_ROOT to use for temporary data. Should usually
                        be empty on entry, as we we will set memory limits
                        on it. The primary reason why it's declared in the
                        caller is that DynamicRangeIterator can clear it
                        and reuse its memory between calls.
      keys_to_use       Keys to use for range retrieval
      prev_tables       Tables assumed to be already read when the scan is
                        performed (but not read at the moment of this call),
                        including const tables. Otherwise 0.
      read_tables       If invoked during execution: tables already read
                        for this join (so values can be assumed to be present).
                        Otherwise 0.
      limit             Query limit
      force_quick_range Prefer to use range (instead of full table scan) even
                        if it is more expensive.
      interesting_order The sort order the range access method must be able
                        to provide. Three-value logic: asc/desc/don't care
      table             The table to optimize over.
      skip_records_in_range
                        Same as QEP_TAB::m_skip_records_in_range.
      cond              The condition to optimize for, if any.
      needed_reg        this info is used in make_join_query_block() even if
                          there is no quick.
      ignore_table_scan Disregard table scan while looking for range.
      query_block       The block the given table is part of.
      path [out]        Calculated AccessPath, or nullptr.

  NOTES
    Updates the following:
      needed_reg - Bits for keys with may be used if all prev regs are read

    In the table struct the following information is updated:
      quick_keys           - Which keys can be used
      quick_rows           - How many rows the key matches
      quick_condition_rows - E(# rows that will satisfy the table condition)

  IMPLEMENTATION
    quick_condition_rows value is obtained as follows:

      It is a minimum of E(#output rows) for all considered table access
      methods (range and index_merge accesses over various indexes).

    The obtained value is not a true E(#rows that satisfy table condition)
    but rather a pessimistic estimate. To obtain a true E(#...) one would
    need to combine estimates of various access methods, taking into account
    correlations between sets of rows they will return.

    For example, if values of tbl.key1 and tbl.key2 are independent (a right
    assumption if we have no information about their correlation) then the
    correct estimate will be:

      E(#rows("tbl.key1 < c1 AND tbl.key2 < c2")) =
      = E(#rows(tbl.key1 < c1)) / total_rows(tbl) * E(#rows(tbl.key2 < c2)

    which is smaller than

       MIN(E(#rows(tbl.key1 < c1), E(#rows(tbl.key2 < c2)))

    which is currently produced.

  TODO
   * Change the value returned in quick_condition_rows from a pessimistic
     estimate to true E(#rows that satisfy table condition).
     (we can re-use some of E(#rows) calcuation code from
  index_merge/intersection for this)

   * Check if this function really needs to modify keys_to_use, and change the
     code to pass it by reference if it doesn't.

   * In addition to force_quick_range other means can be (an usually are) used
     to make this function prefer range over full table scan. Figure out if
     force_quick_range is really needed.

  RETURN
   -1 if impossible select (i.e. certainly no rows will be selected)
    0 if can't use quick_select
    1 if found usable ranges and quick select has been successfully created.

  @note After this call, caller may decide to really use the returned QUICK,
  by calling QEP_TAB::set_range_scan() and updating tab->type() if appropriate.

*/
int test_quick_select(THD *thd, MEM_ROOT *return_mem_root,
                      MEM_ROOT *temp_mem_root, Key_map keys_to_use,
                      table_map prev_tables, table_map read_tables,
                      ha_rows limit, bool force_quick_range,
                      const enum_order interesting_order, TABLE *table,
                      bool skip_records_in_range, Item *cond,
                      Key_map *needed_reg, bool ignore_table_scan,
                      Query_block *query_block, AccessPath **path) {
  DBUG_TRACE;

  *path = nullptr;
  needed_reg->clear_all();

  if (keys_to_use.is_clear_all()) return 0;

  DBUG_PRINT("enter", ("keys_to_use: %lu  prev_tables: %lu  ",
                       (ulong)keys_to_use.to_ulonglong(), (ulong)prev_tables));

  const Cost_model_server *const cost_model = thd->cost_model();
  ha_rows records = table->file->stats.records;
  if (!records) records++; /* purecov: inspected */
  double scan_time =
      cost_model->row_evaluate_cost(static_cast<double>(records)) + 1;
  Cost_estimate cost_est = table->file->table_scan_cost();
  cost_est.add_io(1.1);
  cost_est.add_cpu(scan_time);
  if (ignore_table_scan) {
    scan_time = DBL_MAX;
    cost_est.set_max_cost();
  }
  if (limit < records) {
    cost_est.reset();
    // Force to use index
    cost_est.add_io(
        table->cost_model()->page_read_cost(static_cast<double>(records)) + 1);
    cost_est.add_cpu(scan_time);
  } else if (cost_est.total_cost() <= 2.0 && !force_quick_range)
    return 0; /* No need for quick select */

  Opt_trace_context *const trace = &thd->opt_trace;
  Opt_trace_object trace_range(trace, "range_analysis");
  Opt_trace_object(trace, "table_scan")
      .add("rows", table->file->stats.records)
      .add("cost", cost_est);

  keys_to_use.intersect(table->keys_in_use_for_query);
  if (keys_to_use.is_clear_all()) return 0;

  /*
    Use the 3 multiplier as range optimizer allocates big RANGE_OPT_PARAM
    structure and may evaluate a subquery expression
    TODO During the optimization phase we should evaluate only inexpensive
         single-lookup subqueries.
  */
  uchar buff[STACK_BUFF_ALLOC];
  if (check_stack_overrun(thd, 3 * STACK_MIN_SIZE + sizeof(RANGE_OPT_PARAM),
                          buff)) {
    return 0;  // Fatal error flag is set
  }

  /* set up parameter that is passed to all functions */
  RANGE_OPT_PARAM param;
  if (setup_range_optimizer_param(thd, return_mem_root, temp_mem_root,
                                  keys_to_use, table, query_block, &param)) {
    return 0;
  }
  thd->push_internal_handler(&param.error_handler);
  auto cleanup = create_scope_guard([thd] { thd->pop_internal_handler(); });

  /*
    Set index_merge_allowed from OPTIMIZER_SWITCH_INDEX_MERGE.
    Notice also that OPTIMIZER_SWITCH_INDEX_MERGE disables all
    index merge sub strategies.
  */
  const bool index_merge_allowed =
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_INDEX_MERGE);
  const bool index_merge_union_allowed =
      index_merge_allowed &&
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_INDEX_MERGE_UNION);
  const bool index_merge_sort_union_allowed =
      index_merge_allowed &&
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_INDEX_MERGE_SORT_UNION);
  const bool index_merge_intersect_allowed =
      index_merge_allowed &&
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_INDEX_MERGE_INTERSECT);

  /* Calculate cost of full index read for the shortest covering index */
  if (!table->covering_keys.is_clear_all()) {
    int key_for_use = find_shortest_key(table, &table->covering_keys);
    // find_shortest_key() should return a valid key:
    assert(key_for_use != MAX_KEY);

    Cost_estimate key_read_time = param.table->file->index_scan_cost(
        key_for_use, 1, static_cast<double>(records));
    key_read_time.add_cpu(
        cost_model->row_evaluate_cost(static_cast<double>(records)));

    bool chosen = false;
    if (key_read_time < cost_est) {
      cost_est = key_read_time;
      chosen = true;
    }

    Opt_trace_object trace_cov(trace, "best_covering_index_scan",
                               Opt_trace_context::RANGE_OPTIMIZER);
    trace_cov.add_utf8("index", table->key_info[key_for_use].name)
        .add("cost", key_read_time)
        .add("chosen", chosen);
    if (!chosen) trace_cov.add_alnum("cause", "cost");
  }

  AccessPath *best_path = nullptr;
  double best_cost = cost_est.total_cost();

  SEL_TREE *tree = nullptr;
  if (cond) {
    {
      Opt_trace_array trace_setup_cond(trace, "setup_range_conditions");
      tree = get_mm_tree(thd, &param, prev_tables | INNER_TABLE_BIT,
                         read_tables | INNER_TABLE_BIT,
                         table->pos_in_table_list->map(),
                         /*remove_jump_scans=*/true, cond);
    }
    if (tree) {
      if (tree->type == SEL_TREE::IMPOSSIBLE) {
        trace_range.add("impossible_range", true);
        cost_est.reset();
        cost_est.add_io(static_cast<double>(HA_POS_ERROR));
        return -1;
      }
      /*
        If the tree can't be used for range scans, proceed anyway, as we
        can construct a group-min-max quick select
      */
      if (tree->type != SEL_TREE::KEY) {
        trace_range.add("range_scan_possible", false);
        if (tree->type == SEL_TREE::ALWAYS)
          trace_range.add_alnum("cause", "condition_always_true");

        tree = nullptr;
      }
    }
  }

  /*
    Try to construct a GroupIndexSkipScanIterator.
    Notice that it can be constructed no matter if there is a range tree.
  */
  AccessPath *group_path = get_best_group_min_max(
      thd, &param, tree, interesting_order, skip_records_in_range, best_cost);
  if (group_path) {
    DBUG_EXECUTE_IF("force_lis_for_group_by", group_path->cost = 0.0;);
    param.table->quick_condition_rows =
        min<double>(group_path->num_output_rows(), table->file->stats.records);
    Opt_trace_object grp_summary(trace, "best_group_range_summary",
                                 Opt_trace_context::RANGE_OPTIMIZER);
    if (unlikely(trace->is_started()))
      trace_basic_info(thd, group_path, &param, &grp_summary);
    if (group_path->cost < best_cost) {
      grp_summary.add("chosen", true);
      best_path = group_path;
      best_cost = best_path->cost;
    } else
      grp_summary.add("chosen", false).add_alnum("cause", "cost");
  }

  bool force_skip_scan = hint_table_state(thd, param.table->pos_in_table_list,
                                          SKIP_SCAN_HINT_ENUM, 0);

  if (thd->optimizer_switch_flag(OPTIMIZER_SKIP_SCAN) || force_skip_scan) {
    AccessPath *skip_scan_path =
        get_best_skip_scan(thd, &param, tree, interesting_order,
                           skip_records_in_range, force_skip_scan);
    if (skip_scan_path) {
      param.table->quick_condition_rows = min<double>(
          skip_scan_path->num_output_rows(), table->file->stats.records);
      Opt_trace_object summary(trace, "best_skip_scan_summary",
                               Opt_trace_context::RANGE_OPTIMIZER);
      if (unlikely(trace->is_started()))
        trace_basic_info(thd, skip_scan_path, &param, &summary);

      if (skip_scan_path->cost < best_cost || force_skip_scan) {
        summary.add("chosen", true);
        best_path = skip_scan_path;
        best_cost = best_path->cost;
      } else
        summary.add("chosen", false).add_alnum("cause", "cost");
    }
  }

  if (tree && (best_path == nullptr || !get_forced_by_hint(best_path))) {
    /*
      It is possible to use a range-based quick select (but it might be
      slower than 'all' table scan).
    */
    dbug_print_tree("final_tree", tree, &param);

    MY_BITMAP needed_fields;
    if (fill_used_fields_bitmap(&param, &needed_fields)) {
      return 0;
    }

    {
      /*
        Calculate cost of single index range scan and possible
        intersections of these
      */
      Opt_trace_object trace_range_alt(trace, "analyzing_range_alternatives",
                                       Opt_trace_context::RANGE_OPTIMIZER);
      AccessPath *range_path = get_key_scans_params(
          thd, &param, tree, false, true, interesting_order,
          skip_records_in_range, best_cost, needed_reg);

      /* Get best 'range' plan and prepare data for making other plans */
      if (range_path) {
        best_path = range_path;
        best_cost = best_path->cost;
      }

      /*
        Simultaneous key scans and row deletes on several handler
        objects are not allowed so don't use ROR-intersection for
        table deletes. Also, ROR-intersection cannot return rows in
        descending order
      */
      if ((thd->lex->sql_command != SQLCOM_DELETE) &&
          (index_merge_allowed ||
           hint_table_state(thd, param.table->pos_in_table_list,
                            INDEX_MERGE_HINT_ENUM, 0)) &&
          interesting_order != ORDER_DESC) {
        /*
          Get best non-covering ROR-intersection plan and prepare data for
          building covering ROR-intersection.
        */
        AccessPath *rori_path = get_best_ror_intersect(
            thd, &param, table, index_merge_intersect_allowed, tree,
            &needed_fields, best_cost,
            /*force_index_merge_result=*/true, /*reuse_handler=*/true);
        if (rori_path) {
          best_path = rori_path;
          best_cost = best_path->cost;
        }
      }
    }

    // Here we calculate cost of union index merge
    if (!tree->merges.is_empty()) {
      // Cannot return rows in descending order.
      if ((index_merge_allowed ||
           hint_table_state(thd, param.table->pos_in_table_list,
                            INDEX_MERGE_HINT_ENUM, 0)) &&
          interesting_order != ORDER_DESC && param.table->file->stats.records) {
        /* Try creating index_merge/ROR-union scan. */
        AccessPath *best_conj_path = nullptr, *new_conj_path = nullptr;
        Opt_trace_array trace_idx_merge(trace, "analyzing_index_merge_union",
                                        Opt_trace_context::RANGE_OPTIMIZER);

        // Buffer for index_merge cost estimates.
        for (SEL_IMERGE &imerge : tree->merges) {
          new_conj_path = get_best_disjunct_quick(
              thd, &param, table, index_merge_union_allowed,
              index_merge_sort_union_allowed, index_merge_intersect_allowed,
              skip_records_in_range, &needed_fields, &imerge, best_cost,
              needed_reg);
          if (new_conj_path)
            param.table->quick_condition_rows =
                min<double>(param.table->quick_condition_rows,
                            new_conj_path->num_output_rows());
          if (!best_conj_path ||
              (new_conj_path && new_conj_path->cost < best_conj_path->cost)) {
            best_conj_path = new_conj_path;
          }
        }
        if (best_conj_path) best_path = best_conj_path;
      }
    }
  }

  /*
    If we got a read plan, return it, but only if the storage engine supports
    using indexes for access.
  */
  if (best_path && (table->file->ha_table_flags() & HA_NO_INDEX_ACCESS) == 0) {
    records = best_path->num_output_rows();
    *path = best_path;
  }

  if (unlikely(trace->is_started() && best_path)) {
    Opt_trace_object trace_range_summary(trace, "chosen_range_access_summary");
    {
      Opt_trace_object trace_range_plan(trace, "range_access_plan");
      trace_basic_info(thd, best_path, &param, &trace_range_plan);
    }
    trace_range_summary.add("rows_for_plan", best_path->num_output_rows())
        .add("cost_for_plan", best_path->cost)
        .add("chosen", true);
  }

  DBUG_EXECUTE("info", print_quick(*path, needed_reg););

  if (records == 0) {
    return -1;
  } else {
    return *path != nullptr;
  }
}

/**
  Helper function for get_best_disjunct_quick(), dealing with the case of
  creating a ROR union. Returns nullptr if either an error occurred, or if the
  ROR union was found to be more expensive than read_cost (which is presumably
  the cost for the index merge plan).
 */
static AccessPath *get_ror_union_path(
    THD *thd, RANGE_OPT_PARAM *param, TABLE *table,
    bool index_merge_intersect_allowed, const MY_BITMAP *needed_fields,
    SEL_IMERGE *imerge, const double read_cost, bool force_index_merge,
    Bounds_checked_array<AccessPath *> roru_read_plans,
    AccessPath **range_scans, Opt_trace_object *trace_best_disjunct) {
  double roru_index_cost = 0.0;
  ha_rows roru_total_records = 0;

  /* Find 'best' ROR scan for each of trees in disjunction */
  double roru_intersect_part = 1.0;
  {
    Opt_trace_context *const trace = &thd->opt_trace;
    Opt_trace_array trace_analyze_ror(trace, "analyzing_roworder_scans");
    AccessPath **cur_child = range_scans;
    AccessPath **cur_roru_plan = &roru_read_plans[0];
    for (auto tree_it = imerge->trees.begin(); tree_it != imerge->trees.end();
         tree_it++, cur_child++, cur_roru_plan++) {
      Opt_trace_object path(trace);
      if (unlikely(trace->is_started()))
        trace_basic_info(thd, *cur_child, param, &path);

      const auto &child_param = (*cur_child)->index_range_scan();

      /*
        Assume the best ROR scan is the one that has cheapest
        full-row-retrieval scan cost.
        Also accumulate index_only scan costs as we'll need them to
        calculate overall index_intersection cost.
      */
      double scan_cost = 0.0;
      if (child_param.can_be_used_for_ror) {
        /* Ok, we have index_only cost, now get full rows scan cost */
        scan_cost = table->file
                        ->read_cost(child_param.index, 1,
                                    (*cur_child)->num_output_rows())
                        .total_cost();
        scan_cost += table->cost_model()->row_evaluate_cost(
            (*cur_child)->num_output_rows());
      } else
        scan_cost = read_cost;

      AccessPath *prev_plan = *cur_child;
      if (!(*cur_roru_plan = get_best_ror_intersect(
                thd, param, table, index_merge_intersect_allowed, *tree_it,
                needed_fields, scan_cost,
                /*force_index_merge_result=*/false, /*reuse_handler=*/false))) {
        if (child_param.can_be_used_for_ror)
          *cur_roru_plan = prev_plan;
        else
          return nullptr;
      }
      roru_index_cost += (*cur_roru_plan)->cost;
      roru_total_records += (*cur_roru_plan)->num_output_rows();
      roru_intersect_part *=
          (*cur_roru_plan)->num_output_rows() / table->file->stats.records;
    }
  }

  /*
    rows to retrieve=
      SUM(rows_in_scan_i) - table_rows * PROD(rows_in_scan_i / table_rows).
    This is valid because index_merge construction guarantees that conditions
    in disjunction do not share key parts.
  */
  roru_total_records -=
      static_cast<ha_rows>(roru_intersect_part * table->file->stats.records);
  /* ok, got a ROR read plan for each of the disjuncts
    Calculate cost:
    cost(index_union_scan(scan_1, ... scan_n)) =
      SUM_i(cost_of_index_only_scan(scan_i)) +
      queue_use_cost(rowid_len, n) +
      cost_of_row_retrieval
    See get_merge_buffers_cost function for queue_use_cost formula derivation.
  */
  double roru_total_cost;
  {
    JOIN *join = param->query_block->join;
    const bool is_interrupted = join && join->tables != 1;
    Cost_estimate sweep_cost;
    get_sweep_read_cost(table, roru_total_records, is_interrupted, &sweep_cost);
    roru_total_cost = sweep_cost.total_cost();
    roru_total_cost += roru_index_cost;
    roru_total_cost += table->cost_model()->key_compare_cost(
        rows2double(roru_total_records) * std::log2(roru_read_plans.size()));
  }

  trace_best_disjunct->add("index_roworder_union_cost", roru_total_cost)
      .add("members", roru_read_plans.size());
  if (roru_total_cost < read_cost || force_index_merge) {
    trace_best_disjunct->add("chosen", true);

    auto *children = new (param->return_mem_root)
        Mem_root_array<AccessPath *>(param->return_mem_root);
    children->reserve(roru_read_plans.size());
    for (AccessPath *child : roru_read_plans) {
      // NOTE: This overwrites parameters in paths that may be used
      // for something else, but since we've already decided that
      // we are to choose a ROR union, it doesn't matter. If we are
      // to keep multiple candidates around, we need to clone the
      // AccessPaths here.
      switch (child->type) {
        case AccessPath::INDEX_RANGE_SCAN:
          child->index_range_scan().need_rows_in_rowid_order = true;
          break;
        case AccessPath::ROWID_INTERSECTION:
          child->rowid_intersection().need_rows_in_rowid_order = true;
          child->rowid_intersection().retrieve_full_rows = false;
          break;
        default:
          assert(false);
      }
      children->push_back(child);
    }
    AccessPath *path = new (param->return_mem_root) AccessPath;
    path->type = AccessPath::ROWID_UNION;
    path->cost = roru_total_cost;
    path->set_num_output_rows(roru_total_records);
    path->rowid_union().table = table;
    path->rowid_union().children = children;
    path->rowid_union().forced_by_hint = force_index_merge;
    return path;
  }
  return nullptr;
}

/*
  Get best plan for a SEL_IMERGE disjunctive expression.
  SYNOPSIS
    get_best_disjunct_quick()
      param             Parameter from check_quick_select function
      index_merge_union_allowed
      index_merge_sort_union_allowed
      index_merge_intersect_allowed
      interesting_order The sort order the range access method must be able
                        to provide. Three-value logic: asc/desc/don't care
      skip_records_in_range  Same value as JOIN_TAB::skip_records_in_range().
      needed_fields     Bitmap of fields used in the query
      imerge            Expression to use
      imerge_cost_buff  Buffer for index_merge cost estimates
      cost_est          Don't create scans with cost > cost_est
      needed_reg [out]  Bits for keys with may be used if all prev regs are read

  NOTES
    index_merge cost is calculated as follows:
    index_merge_cost =
      cost(index_reads) +         (see #1)
      cost(rowid_to_row_scan) +   (see #2)
      cost(unique_use)            (see #3)

    1. cost(index_reads) =SUM_i(cost(index_read_i))
       For non-CPK scans,
         cost(index_read_i) = {cost of ordinary 'index only' scan}
       For CPK scan,
         cost(index_read_i) = {cost of non-'index only' scan}

    2. cost(rowid_to_row_scan)
      If table PK is clustered then
        cost(rowid_to_row_scan) =
          {cost of ordinary clustered PK scan with n_ranges=n_rows}

      Otherwise, we use the following model to calculate costs:
      We need to retrieve n_rows rows from file that occupies n_blocks blocks.
      We assume that offsets of rows we need are independent variates with
      uniform distribution in [0..max_file_offset] range.

      We'll denote block as "busy" if it contains row(s) we need to retrieve
      and "empty" if doesn't contain rows we need.

      Probability that a block is empty is (1 - 1/n_blocks)^n_rows (this
      applies to any block in file). Let x_i be a variate taking value 1 if
      block #i is empty and 0 otherwise.

      Then E(x_i) = (1 - 1/n_blocks)^n_rows;

      E(n_empty_blocks) = E(sum(x_i)) = sum(E(x_i)) =
        = n_blocks * ((1 - 1/n_blocks)^n_rows) =
       ~= n_blocks * exp(-n_rows/n_blocks).

      E(n_busy_blocks) = n_blocks*(1 - (1 - 1/n_blocks)^n_rows) =
       ~= n_blocks * (1 - exp(-n_rows/n_blocks)).

      Average size of "hole" between neighbor non-empty blocks is
           E(hole_size) = n_blocks/E(n_busy_blocks).

      The total cost of reading all needed blocks in one "sweep" is:

        E(n_busy_blocks) * disk_seek_cost(n_blocks/E(n_busy_blocks))

      This cost estimate is calculated in get_sweep_read_cost().

    3. Cost of Unique use is calculated in Unique::get_use_cost function.

  ROR-union cost is calculated in the same way index_merge, but instead of
  Unique a priority queue is used.

  RETURN
    Created read plan
    NULL - Out of memory or no read scan could be built.
*/

static AccessPath *get_best_disjunct_quick(
    THD *thd, RANGE_OPT_PARAM *param, TABLE *table,
    bool index_merge_union_allowed, bool index_merge_sort_union_allowed,
    bool index_merge_intersect_allowed, bool skip_records_in_range,
    const MY_BITMAP *needed_fields, SEL_IMERGE *imerge, const double cost_est,
    Key_map *needed_reg) {
  double imerge_cost = 0.0;
  ha_rows cpk_scan_records = 0;
  ha_rows non_cpk_scan_records = 0;
  bool all_scans_ror_able = true;
  const Cost_model_table *const cost_model = table->cost_model();
  double read_cost = cost_est;

  DBUG_TRACE;
  DBUG_PRINT("info", ("Full table scan cost: %g", cost_est));

  assert(table->file->stats.records);

  const bool force_index_merge =
      hint_table_state(thd, table->pos_in_table_list, INDEX_MERGE_HINT_ENUM, 0);

  Opt_trace_context *const trace = &thd->opt_trace;
  Opt_trace_object trace_best_disjunct(trace);
  uint n_child_scans = imerge->trees.size();
  AccessPath **range_scans =
      param->return_mem_root->ArrayAlloc<AccessPath *>(n_child_scans);
  if (range_scans == nullptr) {
    return nullptr;
  }
  // Note: to_merge.end() is called to close this object after this for-loop.
  Opt_trace_array to_merge(trace, "indexes_to_merge");
  /*
    Collect best 'range' scan for each of disjuncts, and, while doing so,
    analyze possibility of ROR scans. Also calculate some values needed by
    other parts of the code.
  */
  {
    AccessPath **cpk_scan = nullptr;
    bool all_scans_rors = true;
    bool imerge_too_expensive = false;
    AccessPath **cur_child = range_scans;
    for (auto tree_it = imerge->trees.begin(); tree_it != imerge->trees.end();
         ++tree_it, cur_child++) {
      DBUG_EXECUTE("info",
                   print_sel_tree(param, *tree_it, &(*tree_it)->keys_map,
                                  "tree in SEL_IMERGE"););
      Opt_trace_object trace_idx(trace);
      if (!(*cur_child = get_key_scans_params(
                thd, param, *tree_it, true, false, ORDER_NOT_RELEVANT,
                skip_records_in_range, read_cost, needed_reg))) {
        /*
          One of index scans in this index_merge is more expensive than entire
          table read for another available option. The entire index_merge (and
          any possible ROR-union) will be more expensive then, too. We continue
          here only to update SQL_SELECT members.
        */
        imerge_too_expensive = true;
      }

      if (imerge_too_expensive) {
        trace_idx.add("chosen", false).add_alnum("cause", "cost");
        continue;
      }

      const auto &child_param = (*cur_child)->index_range_scan();
      if (!child_param.can_be_used_for_imerge) {
        trace_idx.add("chosen", false)
            .add_alnum("cause", "index has DESC key part");
        continue;
      }

      imerge_cost += (*cur_child)->cost;
      all_scans_ror_able &= ((*tree_it)->n_ror_scans > 0);
      all_scans_rors &= child_param.can_be_used_for_ror;
      const bool pk_is_clustered = table->file->primary_key_is_clustered();
      if (pk_is_clustered && child_param.index == table->s->primary_key) {
        cpk_scan = cur_child;
        cpk_scan_records = (*cur_child)->num_output_rows();
      } else
        non_cpk_scan_records += (*cur_child)->num_output_rows();

      trace_idx
          .add_utf8("index_to_merge", table->key_info[child_param.index].name)
          .add("cumulated_cost", imerge_cost);
    }

    // Note: to_merge trace object is closed here
    to_merge.end();

    trace_best_disjunct.add("cost_of_reading_ranges", imerge_cost);
    if (imerge_too_expensive || (((imerge_cost > read_cost) ||
                                  ((non_cpk_scan_records + cpk_scan_records >=
                                    table->file->stats.records) &&
                                   read_cost != DBL_MAX)) &&
                                 !force_index_merge)) {
      /*
        Bail out if it is obvious that both index_merge and ROR-union will be
        more expensive
      */
      DBUG_PRINT("info", ("Sum of index_merge scans is more expensive than "
                          "full table scan, bailing out"));
      trace_best_disjunct.add("chosen", false).add_alnum("cause", "cost");
      return nullptr;
    }

    /*
      If all scans happen to be ROR, proceed to generate a ROR-union plan (it's
      guaranteed to be cheaper than non-ROR union), unless ROR-unions are
      disabled in @@optimizer_switch
    */
    if (all_scans_rors && (index_merge_union_allowed || force_index_merge)) {
      trace_best_disjunct.add("use_roworder_union", true)
          .add_alnum("cause", "always_cheaper_than_not_roworder_retrieval");
      return get_ror_union_path(
          thd, param, table, index_merge_intersect_allowed, needed_fields,
          imerge, read_cost, force_index_merge, {range_scans, n_child_scans},
          range_scans, &trace_best_disjunct);
    }

    if (cpk_scan) {
      /*
        Add one rowid/key comparison for each row retrieved on non-CPK
        scan. (it is done in IndexRangeScanIterator::row_in_ranges)
      */
      const double rid_comp_cost = cost_model->key_compare_cost(
          static_cast<double>(non_cpk_scan_records));
      imerge_cost += rid_comp_cost;
      trace_best_disjunct.add("cost_of_mapping_rowid_in_non_clustered_pk_scan",
                              rid_comp_cost);
    }
  }

  /* Calculate cost(rowid_to_row_scan) */
  {
    Cost_estimate sweep_cost;
    JOIN *join = param->query_block->join;
    const bool is_interrupted = join && join->tables != 1;
    get_sweep_read_cost(table, non_cpk_scan_records, is_interrupted,
                        &sweep_cost);
    imerge_cost += sweep_cost.total_cost();
    trace_best_disjunct.add("cost_sort_rowid_and_read_disk", sweep_cost);
  }
  AccessPath *imerge_path = nullptr;
  DBUG_PRINT("info",
             ("index_merge cost with rowid-to-row scan: %g", imerge_cost));
  if ((imerge_cost > read_cost || !index_merge_sort_union_allowed) &&
      !force_index_merge) {
    trace_best_disjunct.add("use_roworder_index_merge", true)
        .add_alnum("cause", "cost");
  } else {
    /* Add Unique operations cost */
    const double dup_removal_cost = Unique::get_use_cost(
        (uint)non_cpk_scan_records, table->file->ref_length,
        thd->variables.sortbuff_size, cost_model);

    trace_best_disjunct.add("cost_duplicate_removal", dup_removal_cost);
    imerge_cost += dup_removal_cost;

    trace_best_disjunct.add("total_cost", imerge_cost);
    DBUG_PRINT("info", ("index_merge cost: %g (wanted: less then %g)",
                        imerge_cost, read_cost));

    if (imerge_cost < read_cost || force_index_merge) {
      imerge_path = new (param->return_mem_root) AccessPath;
      imerge_path->type = AccessPath::INDEX_MERGE;
      imerge_path->index_merge().table = table;
      imerge_path->index_merge().forced_by_hint = force_index_merge;
      imerge_path->index_merge().allow_clustered_primary_key_scan = true;
      imerge_path->index_merge().children =
          new (param->return_mem_root) Mem_root_array<AccessPath *>(
              param->return_mem_root, range_scans, range_scans + n_child_scans);

      // TODO(sgunders): init_cost is high in practice, so should not be zero.
      imerge_path->cost = imerge_cost;
      imerge_path->set_num_output_rows(min<double>(
          non_cpk_scan_records + cpk_scan_records, table->file->stats.records));
      read_cost = imerge_cost;
    }
  }

  if (!all_scans_ror_able || thd->lex->sql_command == SQLCOM_DELETE ||
      (!index_merge_union_allowed && !force_index_merge))
    return imerge_path;

  /* Ok, it is possible to build a ROR-union, try it. */
  AccessPath **roru_read_plans =
      param->return_mem_root->ArrayAlloc<AccessPath *>(n_child_scans);
  if (roru_read_plans == nullptr) {
    return imerge_path;
  }

  AccessPath *roru = get_ror_union_path(
      thd, param, table, index_merge_intersect_allowed, needed_fields, imerge,
      read_cost, force_index_merge, {roru_read_plans, n_child_scans},
      range_scans, &trace_best_disjunct);
  return (roru != nullptr) ? roru : imerge_path;
}

bool comparable_in_index(Item *cond_func, const Field *field,
                         const Field::imagetype itype,
                         Item_func::Functype comp_type, const Item *value) {
  /*
    Usually an index cannot be used if the column collation differs
    from the operation collation. However, a case insensitive index
    may be used for some binary searches:

       WHERE latin1_swedish_ci_column = 'a' COLLATE lati1_bin;
       WHERE latin1_swedish_ci_colimn = BINARY 'a '
  */
  if ((field->result_type() == STRING_RESULT &&
       field->match_collation_to_optimize_range() &&
       value->result_type() == STRING_RESULT && itype == Field::itRAW &&
       field->charset() != cond_func->compare_collation() &&
       !((comp_type == Item_func::EQUAL_FUNC ||
          comp_type == Item_func::EQ_FUNC) &&
         cond_func->compare_collation()->state & MY_CS_BINSORT)))
    return false;

  /*
    Temporal values: Cannot use range access if:
       'indexed_varchar_column = temporal_value'
    because there are many ways to represent the same date as a
    string. A few examples: "01-01-2001", "1-1-2001", "2001-01-01",
    "2001#01#01". The same problem applies to time. Thus, we cannot
    create a useful range predicate for temporal values into VARCHAR
    column indexes.
  */
  if (field->result_type() == STRING_RESULT &&
      !is_temporal_type(field->type()) && value->is_temporal())
    return false;

  /*
    Temporal values: Cannot use range access if IndexedTimeComparedToDate:
       'indexed_time = temporal_value_with_date_part'
    because:
      - without index, a TIME column with value '48:00:00' is
        equal to a DATETIME column with value
        'CURDATE() + 2 days'
      - with range access into the TIME column, CURDATE() + 2
        days becomes "00:00:00" (Field_timef::store_internal()
        simply extracts the time part from the datetime) which
        is a lookup key which does not match "48:00:00". On the other
        hand, we can do ref access for IndexedDatetimeComparedToTime
        because Field_temporal_with_date::store_time() will convert
        48:00:00 to CURDATE() + 2 days which is the correct lookup
        key.
  */
  if (field_time_cmp_date(field, value)) return false;

  /*
    We can't always use indexes when comparing a string index to a
    number. cmp_type() is checked to allow comparison of dates and
    numbers.
  */
  if (field->result_type() == STRING_RESULT &&
      value->result_type() != STRING_RESULT &&
      field->cmp_type() != value->result_type())
    return false;

  /*
    We can't use indexes when comparing to a JSON value. For example,
    the string '{}' should compare equal to the JSON string "{}". If
    we use a string index to compare the two strings, we will be
    comparing '{}' and '"{}"', which don't compare equal.
    The only exception is Item_json, which is a basic const item and is
    used to contain value coerced to index's type.
  */
  if (value->result_type() == STRING_RESULT &&
      value->data_type() == MYSQL_TYPE_JSON && !value->basic_const_item())
    return false;

  return true;
}

#ifndef NDEBUG

/**
  Debugging function to print out a SEL_ROOT and everything it points to,
  recursively. Used only when tracking bugs in the range optimizer
  (for printf debugging); will not normally have any calls to it.
 */
[[maybe_unused]] static void debug_print_tree(SEL_ROOT *origin);

static void debug_print_tree(SEL_ROOT *origin) {
  if (!origin) return;

  std::set<SEL_ROOT *> seen;
  std::queue<SEL_ROOT *> to_print;

  to_print.push(origin);
  while (!to_print.empty()) {
    SEL_ROOT *key = to_print.front();
    to_print.pop();
    if (seen.count(key)) continue;

    printf("Printing %p:\n", key);
    for (SEL_ARG *arg = key->root->first(); arg; arg = arg->next) {
      printf("  %p (next_key_part=%p)  ", arg, arg->next_key_part);
      if (arg->next_key_part) to_print.push(arg->next_key_part);

      String tmp;
      tmp.length(0);
      KEY_PART_INFO fake_key_part;
      fake_key_part.field = arg->field;
      fake_key_part.length = 0;
      append_range(&tmp, &fake_key_part, arg->min_value, arg->max_value,
                   arg->min_flag | arg->max_flag);
      printf("%s\n", tmp.ptr());
    }
    printf("\n");
  }
}

#endif  // !defined(NDEBUG)

/**
  Find the next different key value by skipping all the rows with the same key
  value.

  Implements a specialized loose index access method for queries
  containing aggregate functions with distinct of the form:
    SELECT [SUM|COUNT|AVG](DISTINCT a,...) FROM t
  This method comes to replace the index scan + Unique class
  (distinct selection) for loose index scan that visits all the rows of a
  covering index instead of jumping in the beginning of each group.
  TODO: Placeholder function. To be replaced by a handler API call

  @param is_index_scan     hint to use index scan instead of random index read
                           to find the next different value.
  @param file              table handler
  @param key_part          group key to compare
  @param record            row data
  @param group_prefix      current key prefix data
  @param group_prefix_len  length of the current key prefix data
  @param group_key_parts   number of the current key prefix columns
  @return status
    @retval  0  success
    @retval !0  failure
*/

int index_next_different(bool is_index_scan, handler *file,
                         KEY_PART_INFO *key_part, uchar *record,
                         const uchar *group_prefix, uint group_prefix_len,
                         uint group_key_parts) {
  if (is_index_scan) {
    int result = 0;

    while (!key_cmp(key_part, group_prefix, group_prefix_len)) {
      result = file->ha_index_next(record);
      if (result) return (result);
    }
    return result;
  } else
    return file->ha_index_read_map(record, group_prefix,
                                   make_prev_keypart_map(group_key_parts),
                                   HA_READ_AFTER_KEY);
}

/**
  Print a key to a string

  @param[out] out          String the key is appended to
  @param[in]  key_part     Index components description
  @param[in]  key          Key tuple
*/
void print_key_value(String *out, const KEY_PART_INFO *key_part,
                     const uchar *key) {
  Field *field = key_part->field;
  if (field->is_array()) {
    field = down_cast<Field_typed_array *>(field)->get_conv_field();
  }

  if (field->is_flag_set(BLOB_FLAG)) {
    // Byte 0 of a nullable key is the null-byte. If set, key is NULL.
    if (field->is_nullable() && *key)
      out->append(STRING_WITH_LEN("NULL"));
    else
      (field->type() == MYSQL_TYPE_GEOMETRY)
          ? out->append(STRING_WITH_LEN("unprintable_geometry_value"))
          : out->append(STRING_WITH_LEN("unprintable_blob_value"));
    return;
  }

  uint store_length = key_part->store_length;

  if (field->is_nullable()) {
    /*
      Byte 0 of key is the null-byte. If set, key is NULL.
      Otherwise, print the key value starting immediately after the
      null-byte
    */
    if (*key) {
      out->append(STRING_WITH_LEN("NULL"));
      return;
    }
    key++;  // Skip null byte
    store_length--;
  }

  /*
    Binary data cannot be converted to UTF8 which is what the
    optimizer trace expects. If the column is binary, the hex
    representation is printed to the trace instead.
  */
  if (field->result_type() == STRING_RESULT &&
      field->charset() == &my_charset_bin) {
    out->append("0x");
    for (uint i = 0; i < store_length; i++) {
      out->append(_dig_vec_lower[*(key + i) >> 4]);
      out->append(_dig_vec_lower[*(key + i) & 0x0F]);
    }
    return;
  }

  StringBuffer<128> tmp(system_charset_info);
  bool add_quotes = field->result_type() == STRING_RESULT;

  TABLE *table = field->table;
  my_bitmap_map *old_sets[2];

  dbug_tmp_use_all_columns(table, old_sets, table->read_set, table->write_set);

  field->set_key_image(key, key_part->length);
  if (field->type() == MYSQL_TYPE_BIT) {
    (void)field->val_int_as_str(&tmp, true);  // may change tmp's charset
    add_quotes = false;
  } else {
    field->val_str(&tmp);  // may change tmp's charset
  }

  dbug_tmp_restore_column_maps(table->read_set, table->write_set, old_sets);

  if (add_quotes) {
    out->append('\'');
    // Worst case: Every character is escaped.
    const size_t buffer_size = tmp.length() * 2 + 1;
    char *quoted_string = current_thd->mem_root->ArrayAlloc<char>(buffer_size);
    const size_t quoted_length = escape_string_for_mysql(
        tmp.charset(), quoted_string, buffer_size, tmp.ptr(), tmp.length());
    if (quoted_length == static_cast<size_t>(-1)) {
      // Overflow. Our worst case estimate for the buffer size was too low.
      assert(false);
      return;
    }
    out->append(quoted_string, quoted_length, tmp.charset());
    out->append('\'');
  } else {
    out->append(tmp.ptr(), tmp.length(), tmp.charset());
  }
}

static bool range_is_equality(const uchar *min_key, const uchar *max_key,
                              unsigned store_length, bool is_nullable) {
  if (is_nullable && *min_key && *max_key) {
    // Both keys are NULL, so don't check the rest; they could be uninitialized.
    return true;
  }
  return memcmp(min_key, max_key, store_length) == 0;
}

/**
  Append range info for a key part to a string

  @param[in,out] out          String the range info is appended to
  @param[in]     key_part     Indexed column used in a range select
  @param[in]     min_key      Key tuple describing lower bound of range
  @param[in]     max_key      Key tuple describing upper bound of range
  @param[in]     flag         Key range flags defining what min_key
                              and max_key represent @see my_base.h
*/
void append_range(String *out, const KEY_PART_INFO *key_part,
                  const uchar *min_key, const uchar *max_key, const uint flag) {
  if (out->length() > 0) out->append(STRING_WITH_LEN(" AND "));

  if (flag & GEOM_FLAG) {
    /*
      The flags of GEOM ranges do not work the same way as for other
      range types, so printing "col < some_geom" doesn't make sense.
      Just print the column name, not operator.
    */
    out->append(key_part->field->field_name);
    out->append(STRING_WITH_LEN(" "));
    print_key_value(out, key_part, min_key);
    return;
  }

  // Range scans over multi-valued indexes use a sequence of MEMBER OF
  // predicates ORed together.
  if (key_part->field->is_array()) {
    print_key_value(out, key_part, min_key);
    out->append(STRING_WITH_LEN(" MEMBER OF ("));
    const std::string expression = ItemToString(
        down_cast<Item_func *>(key_part->field->gcol_info->expr_item)
            ->get_arg(0));  // Strip off CAST(... AS <type> ARRAY).
    out->append(expression.data(), expression.size());
    out->append(')');
    return;
  }

  if (!Overlaps(flag, NO_MIN_RANGE | NO_MAX_RANGE | NEAR_MIN | NEAR_MAX) &&
      range_is_equality(min_key, max_key, key_part->store_length,
                        key_part->field->is_nullable())) {
    out->append(get_field_name_or_expression(current_thd, key_part->field));
    out->append(STRING_WITH_LEN(" = "));
    print_key_value(out, key_part, min_key);
    return;
  }

  if (!(flag & NO_MIN_RANGE)) {
    print_key_value(out, key_part, min_key);
    if (flag & NEAR_MIN)
      out->append(STRING_WITH_LEN(" < "));
    else
      out->append(STRING_WITH_LEN(" <= "));
  }

  out->append(get_field_name_or_expression(current_thd, key_part->field));

  if (!(flag & NO_MAX_RANGE)) {
    if (flag & NEAR_MAX)
      out->append(STRING_WITH_LEN(" < "));
    else
      out->append(STRING_WITH_LEN(" <= "));
    print_key_value(out, key_part, max_key);
  }
}

/**
  Traverse an R-B tree of range conditions and append all ranges for
  this keypart and consecutive keyparts to range_trace (if non-NULL)
  or to range_string (if range_trace is NULL). See description of R-B
  trees/SEL_ARG for details on how ranges are linked.

  @param[in,out] range_trace   Optimizer trace array ranges are appended to
  @param[in,out] range_string  The string where range predicates are
                               appended when the last keypart has
                               been reached.
  @param         range_so_far  String containing ranges for keyparts prior
                               to this keypart.
  @param         keypart       The R-B tree containing intervals for this
  keypart.
  @param         key_parts     Index components description, used when adding
                               information to the optimizer trace
  @param         print_full    Whether or not ranges on unusable keyparts
                               should be printed. Useful for debugging.

  @note This function mimics the behavior of sel_arg_range_seq_next()
*/
void append_range_all_keyparts(Opt_trace_array *range_trace,
                               String *range_string, String *range_so_far,
                               SEL_ROOT *keypart,
                               const KEY_PART_INFO *key_parts,
                               const bool print_full) {
  assert(keypart);
  const SEL_ARG *const keypart_root = keypart->root;
  assert(keypart_root && keypart_root != null_element);

  const bool append_to_trace = (range_trace != nullptr);

  // Either add info to range_string or to range_trace
  assert(append_to_trace ? !range_string : (range_string != nullptr));

  // Navigate to first interval in red-black tree
  const KEY_PART_INFO *cur_key_part = key_parts + keypart_root->part;
  const SEL_ARG *keypart_range = keypart_root->first();

  const size_t save_range_so_far_length = range_so_far->length();

  while (keypart_range) {
    /*
      Skip the rest of condition printing to avoid OOM if appending to
      range_string and the string becomes too long. Printing very long
      range conditions normally doesn't make sense either.
    */
    if (!append_to_trace && range_string->length() > 500) {
      range_string->append(STRING_WITH_LEN("..."));
      break;
    }

    // Append the current range predicate to the range String
    switch (keypart->type) {
      case SEL_ROOT::Type::KEY_RANGE:
        append_range(range_so_far, cur_key_part, keypart_range->min_value,
                     keypart_range->max_value,
                     keypart_range->min_flag | keypart_range->max_flag);
        break;
      case SEL_ROOT::Type::MAYBE_KEY:
        range_so_far->append("MAYBE_KEY");
        break;
      case SEL_ROOT::Type::IMPOSSIBLE:
        range_so_far->append("IMPOSSIBLE");
        break;
      default:
        assert(false);
        break;
    }

    /*
      Print range predicates for consecutive keyparts if
      1) There are predicates for later keyparts, and
      2) We explicitly requested to print even the ranges that will
         not be usable by range access, or
      3) There are no "holes" in the used keyparts (keypartX can only
         be used if there is a range predicate on keypartX-1), and
      4) The current range is an equality range
    */
    if (keypart_range->next_key_part &&  // 1
        (print_full ||                   // 2
         (keypart_range->next_key_part->root->part ==
              keypart_range->part + 1 &&      // 3
          keypart_range->is_singlepoint())))  // 4
    {
      append_range_all_keyparts(range_trace, range_string, range_so_far,
                                keypart_range->next_key_part, key_parts,
                                print_full);
    } else {
      /*
        This is the last keypart with a usable range predicate. Print
        full range info to the optimizer trace or to the string
      */
      if (append_to_trace)
        range_trace->add_utf8(range_so_far->ptr(), range_so_far->length());
      else {
        if (range_string->length() == 0)
          range_string->append(STRING_WITH_LEN("("));
        else
          range_string->append(STRING_WITH_LEN(" OR ("));

        range_string->append(range_so_far->ptr(), range_so_far->length());
        range_string->append(STRING_WITH_LEN(")"));
      }
    }
    keypart_range = keypart_range->next;
    /*
      Now moving to next range for this keypart, so "reset"
      range_so_far to include only range description of earlier
      keyparts
    */
    range_so_far->length(save_range_so_far_length);
  }
}

void append_range_to_string(const QUICK_RANGE *range,
                            const KEY_PART_INFO *first_key_part, String *out) {
  const uchar *min_key = range->min_key;
  const uchar *max_key = range->max_key;
  for (int keypart_idx :
       BitsSetIn(range->min_keypart_map | range->max_keypart_map)) {
    uint16 flag = range->flag;
    if (!IsBitSet(keypart_idx, range->min_keypart_map)) {
      flag |= NO_MIN_RANGE;
    }
    if (!IsBitSet(keypart_idx, range->max_keypart_map)) {
      flag |= NO_MAX_RANGE;
    }
    if (Overlaps(range->min_keypart_map | range->max_keypart_map,
                 BitsBetween(keypart_idx + 1, MAX_REF_PARTS))) {
      // We're not the last keypart, so we need to show <= and >= instead of
      // < and >; e.g. a < (1,2) is printed as “a <= 1 AND a < 2”, not
      // “a < 1 AND a < 2”. This isn't strictly correct, though, as the right
      // thing to print would be “a < 1 OR (a <= 1 AND a < 2)”, but it's
      // how it's always been done traditionally.
      // TODO(sgunders): Consider changing this to using the tuple syntax
      // instead.
      flag &= ~(NEAR_MIN | NEAR_MAX);
    }

    // NOTE: append_range() automatically adds “ AND ” if needed.
    append_range(out, &first_key_part[keypart_idx], min_key, max_key, flag);
    min_key += first_key_part[keypart_idx].store_length;
    max_key += first_key_part[keypart_idx].store_length;
  }
}

void print_tree(String *out, const char *tree_name, SEL_TREE *tree,
                const RANGE_OPT_PARAM *param, const bool print_full) {
  if (!param->using_real_indexes) {
    if (out) {
      out->append(tree_name);
      out->append(" uses a partitioned index and cannot be printed");
    } else
      DBUG_PRINT("info", ("sel_tree: "
                          "%s uses a partitioned index and cannot be printed",
                          tree_name));
    return;
  }

  if (!tree) {
    if (out) {
      out->append(tree_name);
      out->append(" is NULL");
    } else
      DBUG_PRINT("info", ("sel_tree: %s is NULL", tree_name));
    return;
  }

  if (tree->type == SEL_TREE::IMPOSSIBLE) {
    if (out) {
      out->append(tree_name);
      out->append(" is IMPOSSIBLE");
    } else
      DBUG_PRINT("info", ("sel_tree: %s is IMPOSSIBLE", tree_name));
    return;
  }

  if (tree->type == SEL_TREE::ALWAYS) {
    if (out) {
      out->append(tree_name);
      out->append(" is ALWAYS");
    } else
      DBUG_PRINT("info", ("sel_tree: %s is ALWAYS", tree_name));
    return;
  }

  if (!tree->merges.is_empty()) {
    if (out) {
      out->append(tree_name);
      out->append(" contains the following merges");
    } else
      DBUG_PRINT("info", ("sel_tree: "
                          "%s contains the following merges",
                          tree_name));

    List_iterator<SEL_IMERGE> it(tree->merges);
    int i = 1;
    for (SEL_IMERGE *el = it++; el; el = it++, i++) {
      if (out) {
        out->append("\n--- alternative ");
        char istr[22];
        out->append(llstr(i, istr));
        out->append(" ---\n");
      } else
        DBUG_PRINT("info", ("sel_tree: --- alternative %d ---", i));
      for (SEL_TREE *current : el->trees)
        print_tree(out, "  merge_tree", current, param, print_full);
    }
  }

  for (uint i = 0; i < param->keys; i++) {
    if (tree->keys[i] == NULL) continue;

    uint real_key_nr = param->real_keynr[i];

    const KEY &cur_key = param->table->key_info[real_key_nr];
    const KEY_PART_INFO *key_part = cur_key.key_part;

    /*
      String holding the final range description from
      append_range_all_keyparts()
    */
    char buff1[512];
    buff1[0] = '\0';
    String range_result(buff1, sizeof(buff1), system_charset_info);
    range_result.length(0);

    /*
      Range description up to a certain keypart - used internally in
      append_range_all_keyparts()
    */
    char buff2[128];
    String range_so_far(buff2, sizeof(buff2), system_charset_info);
    range_so_far.length(0);

    append_range_all_keyparts(nullptr, &range_result, &range_so_far,
                              tree->keys[i], key_part, print_full);

    if (out) {
      char istr[22];

      out->append(tree_name);
      out->append(" keys[");
      out->append(llstr(i, istr));
      out->append("]: ");
      out->append(range_result.ptr());
      out->append("\n");
    } else
      DBUG_PRINT("info", ("sel_tree: %p, type=%d, %s->keys[%u(%u)]: %s",
                          tree->keys[i], static_cast<int>(tree->keys[i]->type),
                          tree_name, i, real_key_nr, range_result.ptr()));
  }
}

/*****************************************************************************
** Print a quick range for debugging
** TODO:
** This should be changed to use a String to store each row instead
** of locking the DEBUG stream !
*****************************************************************************/

#ifndef NDEBUG

static void print_quick(AccessPath *path, const Key_map *needed_reg) {
  char buf[MAX_KEY / 8 + 1];
  my_bitmap_map *old_sets[2];
  DBUG_TRACE;
  if (path == nullptr) return;
  DBUG_LOCK_FILE;

  TABLE *table = nullptr;
  switch (path->type) {
    case AccessPath::INDEX_RANGE_SCAN:
      table = path->index_range_scan().used_key_part[0].field->table;
      break;
    case AccessPath::INDEX_MERGE:
      table = path->index_merge().table;
      break;
    case AccessPath::ROWID_INTERSECTION:
      table = path->rowid_intersection().table;
      break;
    case AccessPath::ROWID_UNION:
      table = path->rowid_union().table;
      break;
    case AccessPath::INDEX_SKIP_SCAN:
      table = path->index_skip_scan().table;
      break;
    case AccessPath::GROUP_INDEX_SKIP_SCAN:
      table = path->group_index_skip_scan().table;
      break;
    default:
      assert(false);
  }
  dbug_tmp_use_all_columns(table, old_sets, table->read_set, table->write_set);
  dbug_dump(path, 0, true);
  dbug_tmp_restore_column_maps(table->read_set, table->write_set, old_sets);

  fprintf(DBUG_FILE, "other_keys: 0x%s:\n", needed_reg->print(buf));

  DBUG_UNLOCK_FILE;
}

#endif /* !NDEBUG */
