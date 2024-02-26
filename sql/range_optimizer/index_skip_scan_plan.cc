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

#include "sql/range_optimizer/index_skip_scan_plan.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <algorithm>

#include "m_ctype.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_table_map.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_sum.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/key.h"
#include "sql/key_spec.h"
#include "sql/opt_costmodel.h"
#include "sql/opt_hints.h"
#include "sql/opt_statistics.h"
#include "sql/opt_trace.h"
#include "sql/opt_trace_context.h"
#include "sql/range_optimizer/index_range_scan_plan.h"
#include "sql/range_optimizer/index_skip_scan.h"
#include "sql/range_optimizer/internal.h"
#include "sql/range_optimizer/path_helpers.h"
#include "sql/range_optimizer/range_opt_param.h"
#include "sql/range_optimizer/tree.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_select.h"
#include "sql/table.h"
#include "sql_string.h"

struct MEM_ROOT;

using std::max;
using std::min;

void trace_basic_info_index_skip_scan(THD *thd, const AccessPath *path,
                                      const RANGE_OPT_PARAM *,
                                      Opt_trace_object *trace_object) {
  const IndexSkipScanParameters *param = path->index_skip_scan().param;

  trace_object->add_alnum("type", "skip_scan")
      .add_utf8("index", param->index_info->name);

  const KEY_PART_INFO *key_part = param->index_info->key_part;
  Opt_trace_context *const trace = &thd->opt_trace;
  {
    Opt_trace_array trace_keyparts(trace, "key_parts_used_for_access");
    for (uint partno = 0; partno < path->index_skip_scan().num_used_key_parts;
         partno++) {
      const KEY_PART_INFO *cur_key_part = key_part + partno;
      trace_keyparts.add_utf8(cur_key_part->field->field_name);
    }
  }

  if (param->index_range_tree && param->eq_prefix_key_parts > 0) {
    Opt_trace_array trace_range(trace, "prefix ranges");
    String range_info;
    range_info.set_charset(system_charset_info);
    append_range_all_keyparts(&trace_range, nullptr, &range_info,
                              param->index_range_tree, key_part, false);
  }

  Opt_trace_array trace_range(trace, "range");
  {
    String range_info;
    range_info.set_charset(system_charset_info);
    const KEY_PART_INFO *cur_key_part =
        key_part + param->range_part_tracing_only->part;
    append_range(&range_info, cur_key_part,
                 param->range_part_tracing_only->min_value,
                 param->range_part_tracing_only->max_value,
                 param->range_part_tracing_only->min_flag |
                     param->range_part_tracing_only->max_flag);
    trace_range.add_utf8(range_info.ptr(), range_info.length());
  }
}

static void cost_skip_scan(TABLE *table, uint key, uint distinct_key_parts,
                           ha_rows quick_prefix_records,
                           Cost_estimate *cost_est, ha_rows *records,
                           Item *where_cond, Opt_trace_object *trace_idx);

/**
  Test if skip scan is applicable and if so, construct a new AccessPath.

  DESCRIPTION
    Test whether a query can be computed via a IndexSkipScanIterator.
    The overall query form should look like this:

       SELECT A_1,...,A_k, B_1,...,B_m, C
         FROM T
        WHERE
         EQ(A_1,...,A_k)
         AND RNG(C);

    Queries computable via a IndexSkipScanIterator must satisfy the
    following conditions:

    A) Table T has at least one compound index I of the form:
       I = <A_1,...,A_k, B_1,..., B_m, C ,[D_1,...,D_n]>
       Keyparts A and D may be empty, but B and C must be non-empty.
    B) Only one table referenced.
    C) Cannot have group by/select distinct
    D) Query must reference fields in the index only.
    E) The predicates on A_1...A_k must be equality predicates and they need
       to be constants. This includes the 'IN' operator.
    F) The query must be a conjunctive query.
       In other words, it is a AND of ORs:
       (COND1(kp1) OR COND2(kp1)) AND (COND1(kp2) OR ...) AND ...
       See get_sel_arg_for_keypart for details.
    G) There must be a range condition on C.
    H) Conditions on D columns are allowed. Conditions on D must be in
       conjunction with range condition on C.

  NOTES
    If the current query satisfies the conditions above, and if
    (mem_root! = NULL), then the function constructs and returns a new
    AccessPath object, that is later used to construct a new
    IndexSkipScanIterator.

  @param  param     Parameter from test_quick_select
  @param  tree      Range tree generated by get_mm_tree
  @param  order_direction The sort order the range access method must be able
                    to provide. Three-value logic: asc/desc/don't care
  @param  skip_records_in_range Same value as JOIN_TAB::skip_records_in_range()
  @param  force_skip_scan  TRUE if skip scan is forced by optimizer hint

  @retval NULL, if skip index scan not applicable,
          otherwise skip index scan table read plan.
*/

AccessPath *get_best_skip_scan(THD *thd, RANGE_OPT_PARAM *param, SEL_TREE *tree,
                               enum_order order_direction,
                               bool skip_records_in_range,
                               bool force_skip_scan) {
  JOIN *join = param->query_block->join;
  TABLE *table = param->table;
  const char *cause = nullptr;
  Opt_trace_context *const trace = &thd->opt_trace;
  Cost_estimate best_read_cost;
  ha_rows best_records = 0;
  bool has_aggregate_function = false;
  DBUG_TRACE;
  best_read_cost.set_max_cost();
  Opt_trace_object trace_group(trace, "skip_scan_range",
                               Opt_trace_context::RANGE_OPTIMIZER);

  if (!join)
    cause = "no_join";
  else if (join->primary_tables != 1) /* Query must reference one table. */
    cause = "not_single_table";
  else if (table->s->keys == 0) /* There are no indexes to use. */
    cause = "no_index";
  else if (order_direction == ORDER_DESC)
    cause = "cannot_do_reverse_ordering";
  else if (!join->group_list.empty())
    cause = "has_group_by";
  else if (!tree)
    cause = "disjuntive_predicate_present";
  else if (join->select_distinct)
    cause = "has_select_distinct";

  if (cause != nullptr) {
    trace_group.add("chosen", false).add_alnum("cause", cause);
    return nullptr;
  }

  KEY *index_info = nullptr; /* The index chosen for data access. */
  uint index = 0;
  KEY_PART_INFO *range_key_part = nullptr;
  SEL_ROOT *index_range_tree = nullptr;
  SEL_ARG *range_sel_arg = nullptr;
  uint eq_prefix_len = 0;
  uint eq_prefix_key_parts = 0;
  uint used_key_parts = 0;
  Cost_estimate min_diff_cost;
  Opt_trace_array trace_indices(trace, "potential_skip_scan_indexes");

  if (join->sum_funcs[0]) {
    Item_sum *min_max_item;
    Item_sum **func_ptr = join->sum_funcs;
    while ((min_max_item = *(func_ptr++))) {
      /*
        TODO: Investigate if this condition could be relaxed in
        some cases.
      */
      if (min_max_item->sum_func() == Item_sum::COUNT_DISTINCT_FUNC ||
          min_max_item->sum_func() == Item_sum::SUM_DISTINCT_FUNC ||
          min_max_item->sum_func() == Item_sum::AVG_DISTINCT_FUNC) {
        trace_group.add("chosen", false)
            .add_alnum("cause", "has_aggregate_distinct");
        return nullptr;
      }
    }
    has_aggregate_function = true;
  }

  for (uint cur_param_idx = 0; cur_param_idx < param->keys; ++cur_param_idx) {
    const uint cur_index = param->real_keynr[cur_param_idx];
    KEY *const cur_index_info = &table->key_info[cur_index];

    Opt_trace_object trace_idx(trace);
    trace_idx.add_utf8("index", cur_index_info->name);
    KEY_PART_INFO *cur_part;
    KEY_PART_INFO *end_part;
    Cost_estimate cur_read_cost;
    ha_rows cur_records;
    SEL_ARG *cur_range_sel_arg = nullptr;
    SEL_ROOT *cur_index_range_tree = nullptr;
    uint cur_eq_prefix_len = 0;
    uint cur_eq_prefix_key_parts = 0;
    KEY_PART_INFO *cur_range_key_part = nullptr;
    enum {
      EQUALITY_KEYPART = 0,
      SKIPPED_KEYPART = 1,
      RANGE_KEYPART = 2,
      TRAILING_KEYPART = 3
    };
    uint keypart_stage = EQUALITY_KEYPART;
    uint cur_used_key_parts = 0;
    uint part = 0;
    ha_rows quick_prefix_records;

    if (!compound_hint_key_enabled(param->table, cur_index,
                                   SKIP_SCAN_HINT_ENUM)) {
      cause = "skip_scan_hint";
      goto next_index;
    }

    cur_part = cur_index_info->key_part;
    end_part = cur_part + actual_key_parts(cur_index_info);

    if (!table->covering_keys.is_set(cur_index)) {
      cause = "query_references_nonkey_column";
      goto next_index;
    }

    /* Extract equality constants that form the prefix. */
    cur_index_range_tree = get_index_range_tree(cur_index, tree, param);
    if (cur_index_range_tree == nullptr) {
      cause = "disjuntive_predicate_present";
      goto next_index;
    }
    for (cur_part = cur_index_info->key_part; cur_part != end_part;
         cur_part++, part++) {
      SEL_ROOT *cur_range_root = nullptr;
      if (get_sel_root_for_keypart(cur_part - cur_index_info->key_part,
                                   cur_index_range_tree, &cur_range_root)) {
        cause = "keypart_in_disjunctive_query";
        goto next_index;
      }

      if (cur_range_root && cur_range_root->type != SEL_ROOT::Type::KEY_RANGE) {
        cause = "not_a_key_range";
        goto next_index;
      }
      // There is no range predicate on current key part.
      if (cur_range_root == nullptr) {
        if (keypart_stage == EQUALITY_KEYPART ||
            keypart_stage == RANGE_KEYPART) {
          keypart_stage++;
        }
        continue;
      }

      SEL_ARG *cur_range = cur_range_root->root;
      // There exists a range predicate on the current key part.
      if (keypart_stage == EQUALITY_KEYPART) {
        int field_length = cur_part->store_length;
        for (cur_range = cur_range->first(); cur_range;
             cur_range = cur_range->next) {
          // NEAR_MIN/NEAR_MAX means a strict inequality.
          if ((cur_range->min_flag & NO_MIN_RANGE) ||
              (cur_range->max_flag & NO_MAX_RANGE) ||
              (cur_range->min_flag & NEAR_MIN) ||
              (cur_range->max_flag & NEAR_MAX)) {
            cause = "prefix_not_const_equality";
            goto next_index;
          }

          if (!(cur_range->maybe_null() && cur_range->min_value[0] &&
                cur_range->max_value[0]) &&  // IS NOT NULL
              memcmp(cur_range->min_value, cur_range->max_value,
                     field_length) != 0)  // not equality
          {
            cause = "prefix_not_const_equality";
            goto next_index;
          }
        }
        cur_eq_prefix_len += field_length;
        cur_eq_prefix_key_parts++;
      } else if (keypart_stage == SKIPPED_KEYPART) {
        if (cur_range_root->elements > 1) {
          cause = "range_predicate_too_complex";
          goto next_index;
        }
        cur_range_key_part = cur_part;
        cur_range_sel_arg = cur_range;
        cur_used_key_parts = part + 1;
        keypart_stage++;
      }
    }

    if (keypart_stage < RANGE_KEYPART) {
      cause = "no_range_predicate";
      goto next_index;
    }

    assert(cur_used_key_parts >= 2);
    table->possible_quick_keys.set_bit(cur_index);

    assert(cur_index_range_tree);
    {
      Cost_estimate dummy_cost;
      uint mrr_flags = HA_MRR_SORTED;
      uint mrr_bufsize = 0;
      /*
        Calculate number of records returned by prefix equality ranges.
      */
      bool is_ror_scan, is_imerge_scan;
      quick_prefix_records = check_quick_select(
          thd, param, cur_param_idx, true, cur_index_range_tree, false,
          order_direction, skip_records_in_range, &mrr_flags, &mrr_bufsize,
          &dummy_cost, &is_ror_scan, &is_imerge_scan);
    }
    cost_skip_scan(table, cur_index, cur_used_key_parts - 1,
                   quick_prefix_records, &cur_read_cost, &cur_records,
                   join->where_cond, &trace_idx);

    trace_idx.add("rows", cur_records).add("cost", cur_read_cost);

    min_diff_cost = cur_read_cost;
    min_diff_cost.multiply(DBL_EPSILON);
    if (cur_read_cost < (best_read_cost - min_diff_cost)) {
      index_info = cur_index_info;
      index = cur_index;
      best_read_cost = cur_read_cost;
      best_records = cur_records;

      eq_prefix_len = cur_eq_prefix_len;
      eq_prefix_key_parts = cur_eq_prefix_key_parts;
      index_range_tree = cur_index_range_tree;

      range_sel_arg = cur_range_sel_arg;
      range_key_part = cur_range_key_part;
      used_key_parts = cur_used_key_parts;
    }
  next_index:
    if (cause) {
      trace_idx.add("usable", false).add_alnum("cause", cause);
      cause = nullptr;
    }
  }
  trace_indices.end();

  if (!index_info) /* No usable index found. */
    return nullptr;

  // Setup fields that hold the range condition on key part C.
  //
  // This is only the suffix of the whole key, that we use
  // to append to the prefix to get the full key later on.
  uint range_cond_flag = 0;
  if (!(range_sel_arg->min_flag & NO_MIN_RANGE) &&
      !(range_sel_arg->max_flag & NO_MAX_RANGE)) {
    // IS NULL condition
    if (range_sel_arg->maybe_null() && range_sel_arg->min_value[0] &&
        range_sel_arg->max_value[0])
      range_cond_flag |= NULL_RANGE;
    // equality condition
    else if (memcmp(range_sel_arg->min_value, range_sel_arg->max_value,
                    range_key_part->store_length) == 0)
      range_cond_flag |= EQ_RANGE;
  }

  uchar *min_value, *max_value;
  if (range_sel_arg->is_ascending) {
    range_cond_flag |= range_sel_arg->min_flag | range_sel_arg->max_flag;
    min_value = range_sel_arg->min_value;
    max_value = range_sel_arg->max_value;
  } else {
    range_cond_flag |= invert_min_flag(range_sel_arg->min_flag) |
                       invert_max_flag(range_sel_arg->max_flag);
    min_value = range_sel_arg->max_value;
    max_value = range_sel_arg->min_value;
  }

  unsigned range_key_len = range_key_part->store_length;

  // Allocate storage for min/max key if they exist.
  unsigned max_used_key_length = 0;
  for (uint i = 0; i < used_key_parts; ++i) {
    max_used_key_length += index_info->key_part[i].store_length;
  }

  MEM_ROOT *const mem_root = param->return_mem_root;
  uchar *min_range_key = nullptr;
  uchar *min_search_key = nullptr;
  if (!(range_cond_flag & NO_MIN_RANGE)) {
    if (!(min_range_key = mem_root->ArrayAlloc<uchar>(range_key_len)))
      return nullptr;
    if (!(min_search_key = mem_root->ArrayAlloc<uchar>(max_used_key_length)))
      return nullptr;

    memcpy(min_range_key, min_value, range_key_len);
  }
  uchar *max_range_key = nullptr;
  uchar *max_search_key = nullptr;
  if (!(range_cond_flag & NO_MAX_RANGE)) {
    if (!(max_range_key = mem_root->ArrayAlloc<uchar>(range_key_len)))
      return nullptr;
    if (!(max_search_key = mem_root->ArrayAlloc<uchar>(max_used_key_length)))
      return nullptr;

    memcpy(max_range_key, max_value, range_key_len);
  }

  /* The query passes all tests, so construct a new AccessPath. */
  EQPrefix *eq_prefixes = nullptr;
  if (eq_prefix_key_parts > 0) {
    eq_prefixes = mem_root->ArrayAlloc<EQPrefix>(eq_prefix_key_parts);
    if (eq_prefixes == nullptr) {
      return nullptr;
    }

    const SEL_ARG *cur_range = index_range_tree->root->first();
    const SEL_ARG *first_range = nullptr;
    const SEL_ROOT *cur_root = index_range_tree;
    for (uint i = 0; i < eq_prefix_key_parts;
         i++, cur_range = cur_range->next_key_part->root) {
      eq_prefixes[i].cur_eq_prefix = 0;
      unsigned num_elements = cur_root->elements;
      cur_root = cur_range->next_key_part;
      assert(num_elements > 0);
      eq_prefixes[i].eq_key_prefixes =
          Bounds_checked_array<uchar *>::Alloc(mem_root, num_elements);

      uint j = 0;
      first_range = cur_range->first();
      for (cur_range = first_range; cur_range;
           j++, cur_range = cur_range->next) {
        KEY_PART_INFO *keypart = index_info->key_part + i;
        size_t field_length = keypart->store_length;
        //  Store ranges in the reverse order if key part is descending.
        uint pos = cur_range->is_ascending ? j : num_elements - j - 1;

        if (!(eq_prefixes[i].eq_key_prefixes[pos] =
                  mem_root->ArrayAlloc<uchar>(field_length)))
          return nullptr;

        if (cur_range->maybe_null() && cur_range->min_value[0] &&
            cur_range->max_value[0]) {
          assert(field_length > 0);
          eq_prefixes[i].eq_key_prefixes[pos][0] = 0x1;
        } else {
          assert(memcmp(cur_range->min_value, cur_range->max_value,
                        field_length) == 0);
          memcpy(eq_prefixes[i].eq_key_prefixes[pos], cur_range->min_value,
                 field_length);
        }
      }
      cur_range = first_range;
      assert(j == num_elements);
    }
  }

  AccessPath *path = new (param->return_mem_root) AccessPath;
  path->type = AccessPath::INDEX_SKIP_SCAN;
  path->cost = best_read_cost.total_cost();
  path->set_num_output_rows(best_records);

  IndexSkipScanParameters *ext =
      new (param->return_mem_root) IndexSkipScanParameters;
  ext->index_info = index_info;
  ext->eq_prefix_len = eq_prefix_len;
  ext->eq_prefix_key_parts = eq_prefix_key_parts;
  ext->eq_prefixes = eq_prefixes;
  ext->range_key_part = range_key_part;
  ext->min_range_key = min_range_key;
  ext->max_range_key = max_range_key;
  ext->min_search_key = min_search_key;
  ext->max_search_key = max_search_key;
  ext->range_cond_flag = range_cond_flag;
  ext->range_key_len = range_key_len;
  ext->range_part_tracing_only = range_sel_arg->first();
  ext->index_range_tree = index_range_tree;
  ext->has_aggregate_function = has_aggregate_function;

  path->index_skip_scan().table = table;
  path->index_skip_scan().index = index;
  path->index_skip_scan().num_used_key_parts = used_key_parts;
  path->index_skip_scan().forced_by_hint = force_skip_scan;
  path->index_skip_scan().param = ext;
  return path;
}

/**
  Compute the cost of a IndexSkipScanIterator for a particular index.

  SYNOPSIS
    cost_skip_scan()
    table                [in] The table being accessed
    key                  [in] The index used to access the table
    distinct_key_parts   [in] Number of key_parts used to get distinct prefix
    quick_prefix_records [in] Number of records processed by prefix ranges
    cost_est             [out] The cost to retrieve rows via this quick select
    records              [out] The number of rows retrieved
    where_cond           [in] WHERE condition
    trace_idx            [in] optimizer_trace object

  DESCRIPTION
    This method computes the access cost of an INDEX_SKIP_SCAN access path
    and the number of rows returned.

  NOTES

    To estimate the size of the groups to read, index statistics
    from rec_per_key is used. Each equality range decreases
    number of the groups to read. The total number of processed
    records from all the groups will be quick_prefix_records if
    there are equality ranges else it will be the entire table.
    Number of distinct group is calculated by dividing the
    number of processed record by the number keys in a group.

    Number of processed records is calculated using following formula:

    records = number_of_distinct_groups * records_per_group * filtering_effect

    where filtering_effect is filtering effect of the range condition.

  RETURN
    None
*/

void cost_skip_scan(TABLE *table, uint key, uint distinct_key_parts,
                    ha_rows quick_prefix_records, Cost_estimate *cost_est,
                    ha_rows *records, Item *where_cond,
                    Opt_trace_object *trace_idx) {
  ha_rows table_records, skip_scan_records;
  uint num_groups;
  rec_per_key_t keys_per_group;
  const KEY *const index_info = &table->key_info[key];
  DBUG_TRACE;

  table_records = table->file->stats.records;
  if (quick_prefix_records == HA_POS_ERROR)
    skip_scan_records = table_records;
  else
    skip_scan_records = quick_prefix_records;

  /* Compute the number of keys in a group. */
  if (index_info->has_records_per_key(distinct_key_parts - 1)) {
    // Use index statistics
    keys_per_group = index_info->records_per_key(distinct_key_parts - 1);
    assert(keys_per_group >= 0);
  } else
    /* If there is no statistics try to guess */
    keys_per_group = guess_rec_per_key(table, index_info, distinct_key_parts);

  num_groups = (uint)(skip_scan_records / keys_per_group) + 1;
  num_groups = std::max(num_groups, 1U);

  /* Calculate filtering effect for the range condition */
  {
    rec_per_key_t keys_per_range;
    table_map used_tables = 0;
    char buf[MAX_FIELDS / 8];
    my_bitmap_map *bitbuf =
        static_cast<my_bitmap_map *>(static_cast<void *>(&buf));
    MY_BITMAP ignored_fields;
    bitmap_init(&ignored_fields, bitbuf, table->s->fields);
    bitmap_set_all(&ignored_fields);
    bitmap_clear_bit(
        &ignored_fields,
        index_info->key_part[distinct_key_parts].field->field_index());

    /* Compute the number of records per group for the range. */
    if (index_info->has_records_per_key(distinct_key_parts))
      keys_per_range = index_info->records_per_key(distinct_key_parts);
    else
      keys_per_range =
          guess_rec_per_key(table, index_info, distinct_key_parts + 1);
    /*
      Calculation of the filtering effect is based on
      Item_field::get_cond_filter_default_probability() where
      max distinct values is used as an argument. So number of
      keys in distinct group is divided by keys_per_range.
    */
    double max_distinct_values = max(
        1.0, static_cast<double>(uint(keys_per_group) / uint(keys_per_range)));
    float filtering_effect = where_cond->get_filtering_effect(
        current_thd, table->pos_in_table_list->map(), used_tables,
        &ignored_fields, max_distinct_values);
    *records = max(ha_rows(1), ha_rows(skip_scan_records * filtering_effect));
  }

  /* Estimate IO cost. */
  const Cost_model_table *const cost_model = table->cost_model();
  Cost_estimate cost_skip_scan =
      table->file->index_scan_cost(key, num_groups, *records);

  /* CPU cost*/
  const double tree_height =
      table_records == 0 ? 1.0 : ceil(log2(double(table_records)));
  const double tree_traversal_cost = cost_model->key_compare_cost(tree_height);
  /* Number of re-positions happens twice per group. */
  trace_idx->add("tree_travel_cost", tree_traversal_cost)
      .add("num_groups", num_groups);
  const double cpu_cost =
      tree_traversal_cost * num_groups * 2 +
      cost_model->row_evaluate_cost(static_cast<double>(*records)) +
      cost_model->key_compare_cost(static_cast<double>(*records));
  cost_skip_scan.add_cpu(cpu_cost);

  *cost_est = cost_skip_scan;

  DBUG_PRINT("info",
             ("table rows: %lu keys/group: %u result rows: %lu",
              (ulong)table_records, (uint)keys_per_group, (ulong)*records));
}

#ifndef NDEBUG
void dbug_dump_index_skip_scan(int indent, bool verbose,
                               const AccessPath *path) {
  const IndexSkipScanParameters *param = path->index_skip_scan().param;
  fprintf(DBUG_FILE,
          "%*squick_skip_scan_query_block: index %s (%d), length: %d\n", indent,
          "", param->index_info->name, path->index_skip_scan().index,
          get_max_used_key_length(path));
  if (param->eq_prefix_len > 0) {
    fprintf(DBUG_FILE, "%*susing eq_prefix with length %d:\n", indent, "",
            param->eq_prefix_len);
  }

  if (verbose) {
    char buff1[512];
    buff1[0] = '\0';
    String range_result(buff1, sizeof(buff1), system_charset_info);

    if (param->index_range_tree && param->eq_prefix_key_parts > 0) {
      range_result.length(0);
      char buff2[128];
      String range_so_far(buff2, sizeof(buff2), system_charset_info);
      range_so_far.length(0);
      append_range_all_keyparts(nullptr, &range_result, &range_so_far,
                                param->index_range_tree,
                                param->index_info->key_part, false);
      fprintf(DBUG_FILE, "Prefix ranges: %s\n", range_result.c_ptr());
    }

    {
      range_result.length(0);
      append_range(&range_result, param->range_key_part, param->min_range_key,
                   param->max_range_key, param->range_cond_flag);
      fprintf(DBUG_FILE, "Range: %s\n", range_result.c_ptr());
    }
  }
}
#endif
