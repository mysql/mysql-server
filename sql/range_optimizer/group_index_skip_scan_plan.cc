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

#include "sql/range_optimizer/group_index_skip_scan_plan.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <algorithm>
#include <limits>

#include "mem_root_deque.h"
#include "my_bitmap.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/udf_registration_types.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/item_sum.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/key.h"
#include "sql/key_spec.h"
#include "sql/opt_costmodel.h"
#include "sql/opt_statistics.h"  // guess_rec_per_key
#include "sql/opt_trace.h"
#include "sql/opt_trace_context.h"
#include "sql/parser_yystype.h"
#include "sql/range_optimizer/group_index_skip_scan.h"
#include "sql/range_optimizer/index_range_scan_plan.h"
#include "sql/range_optimizer/internal.h"
#include "sql/range_optimizer/path_helpers.h"
#include "sql/range_optimizer/range_opt_param.h"
#include "sql/range_optimizer/tree.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_select.h"
#include "sql/table.h"
#include "sql/visible_fields.h"
#include "sql_string.h"
#include "template_utils.h"

struct MEM_ROOT;

using std::max;
using std::min;

static bool add_range(MEM_ROOT *return_mem_root, SEL_ARG *sel_range,
                      uint key_length, Quick_ranges *range_array);

void trace_basic_info_group_index_skip_scan(THD *thd, const AccessPath *path,
                                            const RANGE_OPT_PARAM *,
                                            Opt_trace_object *trace_object) {
  const GroupIndexSkipScanParameters *param =
      +path->group_index_skip_scan().param;

  trace_object->add_alnum("type", "index_group")
      .add_utf8("index", param->index_info->name);
  if (param->min_max_arg_part)
    trace_object->add_utf8("group_attribute",
                           param->min_max_arg_part->field->field_name);
  else
    trace_object->add_null("group_attribute");
  trace_object->add("min_aggregate", !param->min_functions.empty())
      .add("max_aggregate", !param->max_functions.empty())
      .add("distinct_aggregate", param->have_agg_distinct)
      .add("rows", path->num_output_rows())
      .add("cost", path->cost());

  const KEY_PART_INFO *key_part = param->index_info->key_part;
  Opt_trace_context *const trace = &thd->opt_trace;
  {
    Opt_trace_array trace_keyparts(trace, "key_parts_used_for_access");
    for (uint partno = 0;
         partno < path->group_index_skip_scan().num_used_key_parts; partno++) {
      const KEY_PART_INFO *cur_key_part = key_part + partno;
      trace_keyparts.add_utf8(cur_key_part->field->field_name);
    }
  }
  Opt_trace_array trace_range(trace, "ranges");

  // can have group quick without ranges
  if (param->index_tree) {
    String range_info;
    range_info.set_charset(system_charset_info);
    append_range_all_keyparts(&trace_range, nullptr, &range_info,
                              param->index_tree, key_part, false);
  }
}

static inline uint get_field_keypart(KEY *index, const Field *field);

static bool check_key_infix(SEL_ROOT *index_range_tree,
                            KEY_PART_INFO *first_non_group_part,
                            KEY_PART_INFO *min_max_arg_part,
                            KEY_PART_INFO *last_part, uint *key_infix_len,
                            KEY_PART_INFO **first_non_infix_part,
                            uint *infix_factor, KEY *index_info);

static bool check_group_min_max_predicates(Item *cond,
                                           Item_field *min_max_arg_item,
                                           Field::imagetype image_type);

static bool min_max_inspect_cond_for_fields(Item *cond,
                                            Item_field *min_max_arg_item,
                                            bool *min_max_arg_present,
                                            bool *non_min_max_arg_present);

static void cost_group_skip_scan(TABLE *table, uint key, uint used_key_parts,
                                 uint group_key_parts, SEL_TREE *range_tree,
                                 ha_rows quick_prefix_records, bool have_min,
                                 bool have_max, uint infix_factor,
                                 Cost_estimate *cost_est, ha_rows *records,
                                 bool single_group);

void collect_group_skip_scans(
    THD *thd, RANGE_OPT_PARAM *param, SEL_TREE *tree,
    enum_order order_direction, bool skip_records_in_range,
    Mem_root_array<GroupIndexSkipScanInfo *> *possible_group_skip_scans,
    bool &have_min, bool &have_max);

GroupIndexSkipScanInfo *select_best_group_skip_scan(
    Mem_root_array<GroupIndexSkipScanInfo *> *possible_group_skip_scans);

AccessPath *make_group_skip_scan_path(
    THD *thd, RANGE_OPT_PARAM *param, SEL_TREE *tree,
    GroupIndexSkipScanInfo *group_skip_scan_info, double cost_est,
    bool have_min, bool have_max);

/**
  Test if this access method is applicable to a GROUP query with MIN/MAX
  functions, and if so, construct a new AccessPath.

  DESCRIPTION
    Test whether a query can be computed via a GroupIndexSkipScanIterator.
    Queries computable via a GroupIndexSkipScanIterator must satisfy the
    following conditions:
    A) Table T has at least one compound index I of the form:
       I = <A_1, ...,A_k, [B_1,..., B_m], C, [D_1,...,D_n]>
    B) Query conditions:
    B0. Q is over a single table T.
    B1. The attributes referenced by Q are a subset of the attributes of I.
    B2. All attributes QA in Q can be divided into 3 overlapping groups:
        - SA = {S_1, ..., S_l, [C]} - from the SELECT clause, where C is
          referenced by any number of MIN and/or MAX functions if present.
        - WA = {W_1, ..., W_p} - from the WHERE clause
        - GA = <G_1, ..., G_k> - from the GROUP BY clause (if any)
             = SA              - if Q is a DISTINCT query (based on the
                                 equivalence of DISTINCT and GROUP queries.
        - NGA = QA - (GA union C) = {NG_1, ..., NG_m} - the ones not in
          GROUP BY and not referenced by MIN/MAX functions.
        with the following properties specified below.
    B3. If Q has a GROUP BY WITH ROLLUP clause the access method is not
        applicable.

    SA1. There is at most one attribute in SA referenced by any number of
         MIN and/or MAX functions which, which if present, is denoted as C.
    SA2. The position of the C attribute in the index is after the last A_k.
    SA3. The attribute C can be referenced in the WHERE clause only in
         predicates of the forms:
         - (C {< | <= | > | >= | =} const)
         - (const {< | <= | > | >= | =} C)
         - (C between const_i and const_j)
         - C IS NULL
         - C IS NOT NULL
         - C != const
    SA4. If Q has a GROUP BY clause, there are no other aggregate functions
         except MIN and MAX. For queries with DISTINCT, aggregate functions
         are allowed.
    SA5. The select list in DISTINCT queries should not contain expressions.
    SA6. Clustered index can not be used by GROUP_MIN_MAX quick select
         for AGG_FUNC(DISTINCT ...) optimization because cursor position is
         never stored after a unique key lookup in the clustered index and
         further index_next/prev calls can not be used. So loose index scan
         optimization can not be used in this case.
    SA7. If Q has both AGG_FUNC(DISTINCT ...) and MIN/MAX() functions then this
         access method is not used.
         For above queries MIN/MAX() aggregation has to be done at
         nested_loops_join (end_send_group). But with current design MIN/MAX()
         is always set as part of loose index scan. Because of this mismatch
         MIN() and MAX() values will be set incorrectly. For such queries to
         work we need a new interface for loose index scan. This new interface
         should only fetch records with min and max values and let
         end_send_group to do aggregation. Until then do not use
         loose_index_scan.
    GA1. If Q has a GROUP BY clause, then GA is a prefix of I. That is, if
         G_i = A_j => i = j.
    GA2. If Q has a DISTINCT clause, then there is a permutation of SA that
         forms a prefix of I. This permutation is used as the GROUP clause
         when the DISTINCT query is converted to a GROUP query.
    GA3. The attributes in GA may participate in arbitrary predicates, divided
         into two groups:
         - RNG(G_1,...,G_q ; where q <= k) is a range condition over the
           attributes of a prefix of GA
         - PA(G_i1,...G_iq) is an arbitrary predicate over an arbitrary subset
           of GA. Since P is applied to only GROUP attributes it filters some
           groups, and thus can be applied after the grouping.
    GA4. There are no expressions among G_i, just direct column references.
    NGA1.If in the index I there is a gap between the last GROUP attribute G_k,
         and the MIN/MAX attribute C, then NGA must consist of exactly the
         index attributes that constitute the gap. As a result there is a
         permutation of NGA, BA=<B_1,...,B_m>, that coincides with the gap
         in the index.
    NGA2.If BA <> {}, then the WHERE clause must contain a conjunction EQ of
         equality conditions for all NG_i of the form (NG_i = const) or
         (const = NG_i), such that each NG_i is referenced in exactly one
         conjunct. Informally, the predicates provide constants to fill the
         gap in the index.
    WA1. There are no other attributes in the WHERE clause except the ones
         referenced in predicates RNG, PA, PC, EQ defined above. Therefore
         WA is subset of (GA union NGA union C) for GA,NGA,C that pass the
         above tests. By transitivity then it also follows that each WA_i
         participates in the index I (if this was already tested for GA, NGA
         and C).
    WA2. If there is a predicate on C, then it must be in conjunction
         to all predicates on all earlier keyparts in I.

    C) Overall query form:
       SELECT EXPR([A_1,...,A_k], [B_1,...,B_m], [MIN(C)], [MAX(C)])
         FROM T
        WHERE [RNG(A_1,...,A_p ; where p <= k)]
         [AND EQ(B_1,...,B_m)]
         [AND PC(C)]
         [AND PA(A_i1,...,A_iq)]
       GROUP BY A_1,...,A_k
       [HAVING PH(A_1, ..., B_1,..., C)]
    where EXPR(...) is an arbitrary expression over some or all SELECT fields,
    or:
       SELECT DISTINCT A_i1,...,A_ik
         FROM T
        WHERE [RNG(A_1,...,A_p ; where p <= k)]
         [AND PA(A_i1,...,A_iq)];

  NOTES
    If the current query satisfies the conditions above, and if
    (mem_root! = NULL), then the function constructs and returns a new
    AccessPath.  object, that is later used to construct a new
    GroupIndexSkipScanIterator. If (mem_root == nullptr), then the function
    only tests whether the current query satisfies the conditions above,
    and, if so, sets is_applicable = true.

    Queries with DISTINCT for which index access can be used are transformed
    into equivalent group-by queries of the form:

    SELECT A_1,...,A_k FROM T
     WHERE [RNG(A_1,...,A_p ; where p <= k)]
      [AND PA(A_i1,...,A_iq)]
    GROUP BY A_1,...,A_k;

    The group-by list is a permutation of the select attributes, according
    to their order in the index.

  TODO
  - What happens if the query groups by the MIN/MAX field, and there is no
    other field as in: "select min(a) from t1 group by a" ?
  - We assume that the general correctness of the GROUP-BY query was checked
    before this point. Is this correct, or do we have to check it completely?
  - Lift the limitation in condition (B3), that is, make this access method
    applicable to ROLLUP queries.

 @param  thd       Thread handle
 @param  param     Parameter from test_quick_select
 @param  tree      Range tree generated by get_mm_tree
 @param  order_direction The sort order the range access method must be able
                   to provide. Three-value logic: asc/desc/don't care
 @param  skip_records_in_range  Same value as JOIN_TAB::skip_records_in_range().
 @param  cost_est  Best cost so far (=table/index scan time)

 @return group_skip_scan_path access path for best group skip scan
   @retval NULL  Group index skip scan not applicable or mem_root == NULL
   @retval !NULL Group index skip scan table read plan
*/
AccessPath *get_best_group_skip_scan(THD *thd, RANGE_OPT_PARAM *param,
                                     SEL_TREE *tree, enum_order order_direction,
                                     bool skip_records_in_range,
                                     double cost_est) {
  bool have_min = false; /* true if there is a MIN function. */
  bool have_max = false; /* true if there is a MAX function. */

  // collect all candidate group skip_scans
  Mem_root_array<GroupIndexSkipScanInfo *> possible_group_skip_scans(
      param->return_mem_root);
  collect_group_skip_scans(thd, param, tree, order_direction,
                           skip_records_in_range, &possible_group_skip_scans,
                           have_min, have_max);
  if (possible_group_skip_scans.size() == 0) return nullptr;

  // select group skip scan with lowest 'cost' value
  GroupIndexSkipScanInfo *best_group_skip_scan =
      select_best_group_skip_scan(&possible_group_skip_scans);
  // build access path for best group skip scan
  AccessPath *group_skip_scan_path = make_group_skip_scan_path(
      thd, param, tree, best_group_skip_scan, cost_est, have_min, have_max);
  return group_skip_scan_path;
}

/**
  Analyze indexes to see if a group index skip scan is possible and save
  GroupIndexSkipScanInfo data for every possible group index skip scan. Return
  a list of GroupIndexSkipScanInfo* objects which can be used to build a
  GROUP_INDEX_SKIP_SCAN AccessPath.

  @param  thd       Thread info
  @param  param     Range optimizer parameter
  @param  tree      Range tree generated by get_mm_tree
  @param  order_direction The sort order the range access method must be able
                    to provide. Three-value logic: asc/desc/don't care
  @param  skip_records_in_range Same value as JOIN_TAB::skip_records_in_range()
  @param  possible_group_skip_scans List to populate with candidate group skip
                    scans
  @param  have_min  Set to true if MIN() function is present
  @param  have_max  Set to true if MAX() function is present
*/
void collect_group_skip_scans(
    THD *thd, RANGE_OPT_PARAM *param, SEL_TREE *tree,
    enum_order order_direction, bool skip_records_in_range,
    Mem_root_array<GroupIndexSkipScanInfo *> *possible_group_skip_scans,
    bool &have_min, bool &have_max) {
  JOIN *join = param->query_block->join;
  TABLE *table = param->table;

  if (Overlaps(table->file->ha_table_flags(), HA_NO_INDEX_ACCESS)) return;

  Item_field *min_max_arg_item =
      nullptr; /* The argument of all MIN/MAX functions */
  uint key_part_nr;
  ORDER *tmp_group;
  Item_field *item_field;
  bool is_agg_distinct;
  Cost_estimate best_read_cost;

  Opt_trace_context *const trace = &thd->opt_trace;

  DBUG_TRACE;

  Opt_trace_object trace_group(trace, "group_index_skip_scan",
                               Opt_trace_context::RANGE_OPTIMIZER);
  const char *cause = nullptr;
  best_read_cost.set_max_cost();

  /* Perform few 'cheap' tests whether this access method is applicable. */
  if (!join)
    cause = "no_join";
  else if (param->query_block->leaf_table_count !=
           1) /* Query must reference one table. */
    cause = "not_single_table";
  else if (join->query_block->olap == ROLLUP_TYPE) /* Check (B3) for ROLLUP */
    cause = "rollup";
  else if (table->s->keys == 0) /* There are no indexes to use. */
    cause = "no_index";
  else if (order_direction == ORDER_DESC)
    cause = "cannot_do_reverse_ordering";
  if (cause != nullptr) {
    trace_group.add("chosen", false).add_alnum("cause", cause);
    return;
  }

  /* Check (SA1,SA4) and store the only MIN/MAX argument - the C attribute.*/
  mem_root_deque<Item_field *> agg_distinct_flds(param->temp_mem_root);
  is_agg_distinct = is_indexed_agg_distinct(join, &agg_distinct_flds);

  if (join->group_list.empty() && /* Neither GROUP BY nor a DISTINCT query. */
      (!join->select_distinct) && !is_agg_distinct) {
    trace_group.add("chosen", false)
        .add_alnum("cause", "not_group_by_or_distinct");
    return;
  }
  /* Analyze the query in more detail. */

  if (join->sum_funcs[0]) {
    Item_sum *min_max_item;
    Item_sum **func_ptr = join->sum_funcs;
    while ((min_max_item = *(func_ptr++))) {
      if (min_max_item->sum_func() == Item_sum::MIN_FUNC)
        have_min = true;
      else if (min_max_item->sum_func() == Item_sum::MAX_FUNC)
        have_max = true;
      else if (is_agg_distinct &&
               (min_max_item->sum_func() == Item_sum::COUNT_DISTINCT_FUNC ||
                min_max_item->sum_func() == Item_sum::SUM_DISTINCT_FUNC ||
                min_max_item->sum_func() == Item_sum::AVG_DISTINCT_FUNC))
        continue;
      else {
        trace_group.add("chosen", false)
            .add_alnum("cause", "not_applicable_aggregate_function");
        return;
      }

      /* The argument of MIN/MAX. */
      Item *expr = min_max_item->get_arg(0)->real_item();
      if (expr->type() == Item::FIELD_ITEM) /* Is it an attribute? */
      {
        if (!min_max_arg_item)
          min_max_arg_item = (Item_field *)expr;
        else if (!min_max_arg_item->eq(expr, true))
          return;
      } else
        return;
    }
  }

  /**
    Test (Part of WA2): Skip loose index scan on disjunctive WHERE clause which
    results in null tree or merge tree.
  */
  if (tree && !tree->merges.is_empty()) {
    /**
      The tree structure contains multiple disjoint trees. This happens when
      the WHERE clause can't be represented in a single range tree due to the
      disjunctive nature of it but there exists indexes to perform index
      merge scan.
    */
    trace_group.add("chosen", false)
        .add_alnum("cause", "disjuntive_predicate_present");
    return;
  } else if (!tree && join->where_cond && min_max_arg_item) {
    /**
      Skip loose index scan if min_max attribute is present along with
      at least one other attribute in the WHERE cluse when the tree is null.
      There is no range tree if WHERE condition can't be represented in a
      single range tree and index merge is not possible.
    */
    bool min_max_arg_present = false;
    bool non_min_max_arg_present = false;
    if (min_max_inspect_cond_for_fields(join->where_cond, min_max_arg_item,
                                        &min_max_arg_present,
                                        &non_min_max_arg_present)) {
      trace_group.add("chosen", false)
          .add_alnum("cause", "minmax_keypart_in_disjunctive_query");
      return;
    }
  }

  /* Check (SA7). */
  if (is_agg_distinct && (have_max || have_min)) {
    trace_group.add("chosen", false)
        .add_alnum("cause", "have_both_agg_distinct_and_min_max");
    return;
  }

  /* Check (SA5). */
  if (join->select_distinct) {
    trace_group.add("distinct_query", true);
    for (Item *item : VisibleFields(*join->fields)) {
      if (item->real_item()->type() != Item::FIELD_ITEM) return;
    }
  }

  /* Check (GA4) - that there are no expressions among the group attributes. */
  for (tmp_group = join->group_list.order; tmp_group;
       tmp_group = tmp_group->next) {
    if ((*tmp_group->item)->real_item()->type() != Item::FIELD_ITEM) {
      trace_group.add("chosen", false)
          .add_alnum("cause", "group_field_is_expression");
      return;
    }
  }

  /*
    Check that table has at least one compound index such that the conditions
    (GA1,GA2) are all true. If there is more than one such index, select the
    first one. Here we set the variables: group_prefix_len and index_info.
  */
  const uint pk = param->table->s->primary_key;
  SEL_ROOT *cur_index_tree = nullptr;
  ha_rows cur_quick_prefix_records = 0;
  Opt_trace_array trace_indexes(trace, "potential_group_skip_scan_indexes");
  // We go through allowed indexes
  for (uint cur_param_idx = 0; cur_param_idx < param->keys; ++cur_param_idx) {
    const uint cur_index = param->real_keynr[cur_param_idx];
    KEY *const cur_index_info = &table->key_info[cur_index];
    Opt_trace_object trace_idx(trace);
    trace_idx.add_utf8("index", cur_index_info->name);
    KEY_PART_INFO *cur_part;
    KEY_PART_INFO *end_part; /* Last part for loops. */
    /* Last index part. */
    KEY_PART_INFO *last_part;
    KEY_PART_INFO *first_non_group_part;
    KEY_PART_INFO *first_non_infix_part;
    uint key_infix_parts;
    uint cur_group_key_parts = 0;
    uint cur_group_prefix_len = 0;
    Cost_estimate cur_read_cost;
    // Possible number of combination of infix ranges
    uint cur_infix_factor = 1;
    ha_rows cur_records;
    Key_map used_key_parts_map;
    uint max_key_part = 0;
    uint cur_key_infix_len = 0;
    uint cur_used_key_parts;
    KEY_PART_INFO *cur_min_max_arg_part = nullptr;
    // Set to true if the query has equality predicate on the grouping
    // attributes.
    bool is_eq_range_pred = false;

    /* Check (B1) - if current index is covering. */
    if (!table->covering_keys.is_set(cur_index)) {
      cause = "not_covering";
      goto next_index;
    }

    /*
      If the current storage manager is such that it appends the primary key to
      each index, then the above condition is insufficient to check if the
      index is covering. In such cases it may happen that some fields are
      covered by the PK index, but not by the current index. Since we can't
      use the concatenation of both indexes for index lookup, such an index
      does not qualify as covering in our case. If this is the case, below
      we check that all query fields are indeed covered by 'cur_index'.
    */
    if (pk < MAX_KEY && cur_index != pk &&
        (table->file->ha_table_flags() & HA_PRIMARY_KEY_IN_READ_INDEX)) {
      /* For each table field */
      for (uint i = 0; i < table->s->fields; i++) {
        Field *cur_field = table->field[i];
        /*
          If the field is used in the current query ensure that it's
          part of 'cur_index'
        */
        if (bitmap_is_set(table->read_set, cur_field->field_index()) &&
            !cur_field->is_part_of_actual_key(thd, cur_index, cur_index_info)) {
          cause = "not_covering";
          goto next_index;  // Field was not part of key
        }
      }
    }
    trace_idx.add("covering", true);

    /*
      Check (GA1) for GROUP BY queries. While at it, check if the query produces
      only one group.
    */
    if (!join->group_list.empty()) {
      cur_part = cur_index_info->key_part;
      end_part = cur_part + actual_key_parts(cur_index_info);
      SEL_ROOT *cur_tree = nullptr;
      if (tree) cur_tree = get_index_range_tree(cur_index, tree, param);
      /* Iterate in parallel over the GROUP list and the index parts. */
      for (tmp_group = join->group_list.order;
           tmp_group && (cur_part != end_part);
           tmp_group = tmp_group->next, cur_part++) {
        /*
          TODO:
          tmp_group::item is an array of Item, is it OK to consider only the
          first Item? If so, then why? What is the array for?
        */
        /* Above we already checked that all group items are fields. */
        assert((*tmp_group->item)->real_item()->type() == Item::FIELD_ITEM);
        Item_field *group_field = (Item_field *)(*tmp_group->item)->real_item();
        if (group_field->field->eq(cur_part->field)) {
          cur_group_prefix_len += cur_part->store_length;
          ++cur_group_key_parts;
          max_key_part = cur_part - cur_index_info->key_part + 1;
          used_key_parts_map.set_bit(max_key_part);
        } else {
          cause = "group_attribute_not_prefix_in_index";
          goto next_index;
        }
        // Special case (determine if the query produces only one group):
        // If all the grouping attributes have an equality predicate or
        // have NULL range (IS NULL), query produces only one group.
        // Determining this will help in cost calculation for using this
        // index.
        if (cur_tree &&
            // Check if the range tree is for the key part that is being looked
            // into.
            (cur_tree->root->part == cur_part - cur_index_info->key_part) &&
            // There should not be any disjuntive predicates on the key part.
            cur_tree->root->first() != nullptr &&
            cur_tree->root->first()->next == nullptr) {
          SEL_ARG *range = cur_tree->root->first();
          const uint is_open_range =
              (NO_MIN_RANGE | NO_MAX_RANGE | NEAR_MIN | NEAR_MAX | GEOM_FLAG);
          is_eq_range_pred = !(range->min_flag & is_open_range) &&
                             !(range->max_flag & is_open_range) &&
                             ((range->maybe_null() && range->min_value[0] &&
                               range->max_value[0]) ||
                              memcmp(range->min_value, range->max_value,
                                     cur_part->store_length) == 0);
        } else
          is_eq_range_pred = false;
        cur_tree =
            is_eq_range_pred ? cur_tree->root->first()->next_key_part : nullptr;
      }
    }

    /*
      Check (GA2) if this is a DISTINCT query.
      If GA2, then Store a new ORDER object in group_fields_array at the
      position of the key part of item_field->field. Thus we get the ORDER
      objects for each field ordered as the corresponding key parts.
      Later group_fields_array of ORDER objects is used to convert the query
      to a GROUP query.
    */
    if ((join->group_list.empty() && join->select_distinct) ||
        is_agg_distinct) {
      auto agg_distinct_flds_it = agg_distinct_flds.begin();
      auto select_items_it = VisibleFields(*join->fields).begin();
      while (is_agg_distinct
                 ? (agg_distinct_flds_it != agg_distinct_flds.end())
                 : (select_items_it != VisibleFields(*join->fields).end())) {
        Item *item =
            (is_agg_distinct ? static_cast<Item *>(*agg_distinct_flds_it++)
                             : *select_items_it++);

        /* (SA5) already checked above. */
        item_field = (Item_field *)item->real_item();
        assert(item->real_item()->type() == Item::FIELD_ITEM);

        /* not doing loose index scan for derived tables */
        if (!item_field->field) {
          cause = "derived_table";
          goto next_index;
        }

        /* Find the order of the key part in the index. */
        key_part_nr = get_field_keypart(cur_index_info, item_field->field);
        /*
          Check if this attribute was already present in the select list.
          If it was present, then its corresponding key part was already used.
        */
        if (used_key_parts_map.is_set(key_part_nr)) continue;
        if (key_part_nr < 1 ||
            (!is_agg_distinct &&
             key_part_nr > CountVisibleFields(*join->fields))) {
          cause = "select_attribute_not_prefix_in_index";
          goto next_index;
        }
        cur_part = cur_index_info->key_part + key_part_nr - 1;
        cur_group_prefix_len += cur_part->store_length;
        used_key_parts_map.set_bit(key_part_nr);
        ++cur_group_key_parts;
        max_key_part = max(max_key_part, key_part_nr);
      }
      /*
        Check that used key parts forms a prefix of the index.
        To check this we compare bits in all_parts and cur_parts.
        all_parts have all bits set from 0 to (max_key_part-1).
        cur_parts have bits set for only used keyparts.
      */
      ulonglong all_parts, cur_parts;
      all_parts = (1ULL << max_key_part) - 1;
      cur_parts = used_key_parts_map.to_ulonglong() >> 1;
      if (all_parts != cur_parts) goto next_index;
    }

    /* Check (SA2). */
    if (min_max_arg_item) {
      key_part_nr = get_field_keypart(cur_index_info, min_max_arg_item->field);
      if (key_part_nr <= cur_group_key_parts) {
        cause = "aggregate_column_not_suffix_in_idx";
        goto next_index;
      }
      cur_min_max_arg_part = cur_index_info->key_part + key_part_nr - 1;
    }

    /* Check (SA6) if clustered key is used. */
    if (is_agg_distinct && cur_index == table->s->primary_key &&
        table->file->primary_key_is_clustered()) {
      cause = "primary_key_is_clustered";
      goto next_index;
    }

    /*
      Check (NGA1, NGA2) and extract a sequence of constants to be used as part
      of all search keys.
    */

    /*
      If there is MIN/MAX, each keypart between the last group part and the
      MIN/MAX part must participate in equalities with constants, and all
      keyparts after the MIN/MAX part must not be referenced in the query.

      If there is no MIN/MAX, the keyparts after the last group part can be
      referenced only in equalities with constants, and the referenced keyparts
      must form a sequence without any gaps that starts immediately after the
      last group keypart.
    */
    last_part = cur_index_info->key_part + actual_key_parts(cur_index_info);
    first_non_group_part =
        (cur_group_key_parts < actual_key_parts(cur_index_info))
            ? cur_index_info->key_part + cur_group_key_parts
            : nullptr;
    first_non_infix_part = cur_min_max_arg_part
                               ? (cur_min_max_arg_part < last_part)
                                     ? cur_min_max_arg_part
                                     : nullptr
                               : nullptr;
    if (first_non_group_part &&
        (!cur_min_max_arg_part ||
         (cur_min_max_arg_part - first_non_group_part > 0))) {
      if (tree) {
        SEL_ROOT *index_range_tree =
            get_index_range_tree(cur_index, tree, param);

        if (!check_key_infix(index_range_tree, first_non_group_part,
                             cur_min_max_arg_part, last_part,
                             &cur_key_infix_len, &first_non_infix_part,
                             &cur_infix_factor, cur_index_info)) {
          cause = "non_equality_gap_attribute";
          goto next_index;
        }
      } else if (cur_min_max_arg_part &&
                 (cur_min_max_arg_part - first_non_group_part > 0)) {
        /*
          There is a gap but no range tree, thus no predicates at all for the
          non-group keyparts.
        */
        cause = "no_nongroup_keypart_predicate";
        goto next_index;
      } else if (first_non_group_part && join->where_cond) {
        /*
          If there is no MIN/MAX function in the query, but some index
          key part is referenced in the WHERE clause, then this index
          cannot be used because the WHERE condition over the keypart's
          field cannot be 'pushed' to the index (because there is no
          range 'tree'), and the WHERE clause must be evaluated before
          GROUP BY/DISTINCT.
        */
        /*
          Store the first and last keyparts that need to be analyzed
          into one array that can be passed as parameter.
        */
        KEY_PART_INFO *key_part_range[2];
        key_part_range[0] = first_non_group_part;
        key_part_range[1] = last_part;

        /* Check if cur_part is referenced in the WHERE clause. */
        if (join->where_cond->walk(&Item::find_item_in_field_list_processor,
                                   enum_walk::SUBQUERY_POSTFIX,
                                   (uchar *)key_part_range)) {
          cause = "keypart_reference_from_where_clause";
          goto next_index;
        }
      }
    }

    /*
      Test (WA1) partially - that no other keypart after the last infix part is
      referenced in the query.
    */
    if (first_non_infix_part) {
      cur_part = first_non_infix_part +
                 (cur_min_max_arg_part && (cur_min_max_arg_part < last_part));
      for (; cur_part != last_part; cur_part++) {
        if (bitmap_is_set(table->read_set, cur_part->field->field_index())) {
          cause = "keypart_after_infix_in_query";
          goto next_index;
        }
      }
    }

    /**
      Test Part of WA2:If there are conditions on a column C participating in
      MIN/MAX, those conditions must be conjunctions to all earlier
      keyparts. Otherwise, Loose Index Scan cannot be used.
    */
    if (tree && min_max_arg_item) {
      SEL_ROOT *index_range_tree = get_index_range_tree(cur_index, tree, param);
      SEL_ROOT *cur_range = nullptr;
      if (get_sel_root_for_keypart(
              cur_min_max_arg_part - cur_index_info->key_part, index_range_tree,
              &cur_range) ||
          (cur_range && cur_range->type != SEL_ROOT::Type::KEY_RANGE)) {
        cause = "minmax_keypart_in_disjunctive_query";
        goto next_index;
      }
    }

    /* If we got to this point, cur_index_info passes the test. */
    key_infix_parts = cur_key_infix_len
                          ? (uint)(first_non_infix_part - first_non_group_part)
                          : 0;
    cur_used_key_parts = cur_group_key_parts + key_infix_parts;

    /* Compute the cost of using this index. */
    if (tree) {
      /* Find the SEL_ARG sub-tree that corresponds to the chosen index. */
      cur_index_tree = get_index_range_tree(cur_index, tree, param);
      /* Check if this range tree can be used for prefix retrieval. */
      Cost_estimate dummy_cost;
      uint mrr_flags = HA_MRR_SORTED;
      uint mrr_bufsize = 0;
      bool is_ror_scan, is_imerge_scan;
      cur_quick_prefix_records = check_quick_select(
          thd, param, cur_param_idx, false /*don't care*/, cur_index_tree, true,
          order_direction, skip_records_in_range, &mrr_flags, &mrr_bufsize,
          &dummy_cost, &is_ror_scan, &is_imerge_scan);
      if (unlikely(cur_index_tree && trace->is_started())) {
        trace_idx.add("index_dives_for_eq_ranges",
                      !param->use_index_statistics);
        Opt_trace_array trace_range(trace, "ranges");

        const KEY_PART_INFO *key_part = cur_index_info->key_part;

        String range_info;
        range_info.set_charset(system_charset_info);
        append_range_all_keyparts(&trace_range, nullptr, &range_info,
                                  cur_index_tree, key_part, false);
      }
    }
    cost_group_skip_scan(table, cur_index, cur_used_key_parts,
                         cur_group_key_parts, tree, cur_quick_prefix_records,
                         have_min, have_max, cur_infix_factor, &cur_read_cost,
                         &cur_records, is_eq_range_pred);
    trace_idx.add("rows", cur_records).add("cost", cur_read_cost);
    {
      GroupIndexSkipScanInfo *group_skip_scan_info =
          new (param->return_mem_root) GroupIndexSkipScanInfo;
      GroupIndexSkipScanParameters *group_skip_scan_param =
          new (param->return_mem_root) GroupIndexSkipScanParameters;

      group_skip_scan_param->have_agg_distinct = is_agg_distinct;
      group_skip_scan_param->min_max_arg_part = cur_min_max_arg_part;
      group_skip_scan_param->group_prefix_len = cur_group_prefix_len;
      group_skip_scan_param->group_key_parts = cur_group_key_parts;
      group_skip_scan_param->index_info = cur_index_info;
      group_skip_scan_param->key_infix_len = cur_key_infix_len;
      group_skip_scan_param->index_tree = cur_index_tree;
      group_skip_scan_param->used_key_part = param->key[cur_param_idx];
      group_skip_scan_info->param = group_skip_scan_param;
      group_skip_scan_info->quick_prefix_records = cur_quick_prefix_records;
      group_skip_scan_info->cost = cur_read_cost;
      group_skip_scan_info->records = cur_records;
      group_skip_scan_info->param_idx = cur_param_idx;
      group_skip_scan_info->num_used_key_parts = cur_used_key_parts;
      group_skip_scan_info->min_max_arg_item = min_max_arg_item;

      possible_group_skip_scans->push_back(group_skip_scan_info);
    }

  next_index:
    if (cause) {
      trace_idx.add("usable", false).add_alnum("cause", cause);
      cause = nullptr;
    }
  }
  trace_indexes.end();
  trace_group.end();
  return;
}

/**
 Iterates over list of group skip scan candidates and returns pointer
 to candidate with lowest cost.
 @param possible_group_skip_scans List of candidate group index skip scans
 @retval best_scan Pointer to element in possible_group_skip_scans which has
 lowest cost.
*/
GroupIndexSkipScanInfo *select_best_group_skip_scan(
    Mem_root_array<GroupIndexSkipScanInfo *> *possible_group_skip_scans) {
  GroupIndexSkipScanInfo *best_scan = nullptr;
  for (GroupIndexSkipScanInfo *gskip_scan : *possible_group_skip_scans) {
    /*
      If current scan cost is lower than best scan cost, use current scan.
      Do not compare doubles directly because they may have different
      representations (64 vs. 80 bits).
    */
    Cost_estimate min_diff_cost = gskip_scan->cost;
    min_diff_cost.multiply(DBL_EPSILON);
    if ((best_scan == nullptr) ||
        (gskip_scan->cost < (best_scan->cost - min_diff_cost))) {
      best_scan = gskip_scan;
    }
  }
  return best_scan;
}

/**
  Test if group index skip scan is applicable and if so, construct a new
  AccessPath for every candidate group index skip scan.

  @param  thd       Thread info
  @param  param     Range optimizer parameter
  @param  tree      Range tree generated by get_mm_tree
  @param  order_direction The sort order the range access method must be able
                    to provide. Three-value logic: asc/desc/don't care
  @param  skip_records_in_range Same value as JOIN_TAB::skip_records_in_range()
  @param  cost_est  Best cost so far (=table/index scan time)

  @retval Mem_root_array of candidate GROUP_INDEX_SKIP_SCAN AccessPaths.
*/
Mem_root_array<AccessPath *> get_all_group_skip_scans(
    THD *thd, RANGE_OPT_PARAM *param, SEL_TREE *tree,
    enum_order order_direction, bool skip_records_in_range, double cost_est) {
  bool have_min = false; /* true if there is a MIN function. */
  bool have_max = false; /* true if there is a MAX function. */
  Mem_root_array<AccessPath *> group_skip_scan_paths(param->temp_mem_root);
  Mem_root_array<GroupIndexSkipScanInfo *> possible_group_skip_scans(
      param->temp_mem_root);

  collect_group_skip_scans(thd, param, tree, order_direction,
                           skip_records_in_range, &possible_group_skip_scans,
                           have_min, have_max);
  double rows = kUnknownRowCount;
  // Retrieve highest rowcount estimate and use for all group skip scans.
  for (GroupIndexSkipScanInfo *gskip_scan : possible_group_skip_scans) {
    rows = max<double>(rows, gskip_scan->records);
  }
  for (GroupIndexSkipScanInfo *gskip_scan : possible_group_skip_scans) {
    AccessPath *cur_path = make_group_skip_scan_path(
        thd, param, tree, gskip_scan, cost_est, have_min, have_max);
    if (cur_path != nullptr) {
      // Adjust num_output_rows for hypergraph to match aggregate path rowcounts
      cur_path->set_num_output_rows(rows > 1 ? rows - 1 : rows);
      cur_path->num_output_rows_before_filter = cur_path->num_output_rows();
      group_skip_scan_paths.push_back(cur_path);
    }
  }
  return group_skip_scan_paths;
}

/**
 Build a GROUP_INDEX_SKIP_SCAN AccessPath based on scan info

  @param  thd       Thread info
  @param  param     Range optimizer parameter
  @param  tree      Range tree generated by get_mm_tree
  @param  group_skip_scan_info Info for a candidate group skip scan
  @param  cost_est  Best cost so far (=table/index scan time)
  @param  have_min  Set to true if MIN() function is present
  @param  have_max  Set to true if MAX() function is present
*/
AccessPath *make_group_skip_scan_path(
    THD *thd, RANGE_OPT_PARAM *param, SEL_TREE *tree,
    GroupIndexSkipScanInfo *group_skip_scan_info, double cost_est,
    bool have_min, bool have_max) {
  if (group_skip_scan_info == nullptr) return nullptr;

  Opt_trace_context *const trace = &thd->opt_trace;
  DBUG_TRACE;
  Opt_trace_object trace_group_skip(trace, "make_group_skip_scan_path",
                                    Opt_trace_context::RANGE_OPTIMIZER);

  JOIN *join = param->query_block->join;
  TABLE *table = param->table;

  // TODO(sgunders): Figure out why (this was kept across a refactoring,
  // and there is an assert about it further below.).
  if (tree != nullptr && group_skip_scan_info->quick_prefix_records == 0) {
    return nullptr;
  }

  GroupIndexSkipScanParameters *group_skip_scan_param =
      group_skip_scan_info->param;
  /* Check (SA3) for the where clause. */
  if (join->where_cond && group_skip_scan_info->min_max_arg_item &&
      !check_group_min_max_predicates(
          join->where_cond, group_skip_scan_info->min_max_arg_item,
          (group_skip_scan_param->index_info->flags & HA_SPATIAL)
              ? Field::itMBR
              : Field::itRAW)) {
    trace_group_skip.add("usable", false)
        .add_alnum("cause", "unsupported_predicate_on_agg_attribute");
    return nullptr;
  }

  // Populate key_infix_ranges from index_tree.
  MEM_ROOT *const return_mem_root = param->return_mem_root;
  SEL_TREE *const range_tree = tree;
  SEL_ROOT *const index_tree = group_skip_scan_param->index_tree;
  const uint keyno = param->real_keynr[group_skip_scan_info->param_idx];
  Quick_ranges_array key_infix_ranges(return_mem_root);
  uint num_infix_keyparts = group_skip_scan_info->num_used_key_parts -
                            group_skip_scan_param->group_key_parts;
  for (uint i = 0; i < num_infix_keyparts; i++) {
    key_infix_ranges.push_back(new (return_mem_root)
                                   Quick_ranges(return_mem_root));
  }
  if (group_skip_scan_param->key_infix_len > 0) {
    if (range_tree) {
      KEY_PART_INFO *infix_keypart =
          group_skip_scan_param->index_info->key_part +
          group_skip_scan_param->group_key_parts;

      // Find the start of key infix ranges in the range tree
      SEL_ARG *infix_key = index_tree->root;
      while (infix_key) {
        if (infix_key->field->eq(infix_keypart->field)) break;
        infix_key = infix_key->next_key_part->root;
      }

      // Get ranges on infix keyparts
      for (uint i = 0; i < num_infix_keyparts; i++) {
        /*
          Infix ranges are always contiguous. Get the next set of
          infix ranges from the first one. infix_key tracks
          current keypart while cur_range tracks current range
          within a keypart.
        */
        bool is_ascending = infix_key->is_ascending;
        KEY_PART_INFO *key_infix_part =
            group_skip_scan_param->index_info->key_part +
            group_skip_scan_param->group_key_parts + i;
        for (SEL_ARG *cur_range = is_ascending ? infix_key->first()
                                               : infix_key->last();
             cur_range != nullptr;
             cur_range = is_ascending ? cur_range->next : cur_range->prev) {
          assert(cur_range->field->eq(infix_keypart[i].field));
          if (add_range(return_mem_root, cur_range,
                        key_infix_part->store_length, key_infix_ranges[i])) {
            return nullptr;
          }
        }
        // Get the next infix key part.
        if (infix_key->next_key_part)
          infix_key = infix_key->next_key_part->root;
      }
    }
  }

  Quick_ranges min_max_ranges(return_mem_root);
  KEY_PART_INFO *min_max_arg_part = group_skip_scan_param->min_max_arg_part;
  if (range_tree && min_max_arg_part) {
    /*
      Extract the SEL_ARG subtree that contains only ranges for the MIN/MAX
      attribute, and create an array of QUICK_RANGES to be used by the
      new quick select.
    */
    const SEL_ROOT *min_max_range_root = index_tree;
    while (min_max_range_root) /* Find the tree for the MIN/MAX key part. */
    {
      if (min_max_range_root->root->field->eq(min_max_arg_part->field)) break;
      min_max_range_root = min_max_range_root->root->next_key_part;
    }
    if (min_max_range_root) {
      /* Create an array of QUICK_RANGEs for the MIN/MAX argument. */
      for (SEL_ARG *min_max_range = min_max_range_root->root->first();
           min_max_range; min_max_range = min_max_range->next) {
        if (add_range(return_mem_root, min_max_range,
                      min_max_arg_part->store_length, &min_max_ranges)) {
          return nullptr;
        }
      }
    }
  }

  /*
    Determine the total number and length of the keys that will be used for
    index lookup.

    The total length of the keys used for index lookup depends on whether
    there are any predicates referencing the min/max argument, and/or if
    the min/max argument field can be NULL.
    This function does an optimistic analysis whether the search key might
    be extended by a constant for the min/max keypart. It is 'optimistic'
    because during actual execution it may happen that a particular range
    is skipped, and then a shorter key will be used. However this is data
    dependent and can't be easily estimated here.
   */
  uint real_prefix_len = group_skip_scan_param->group_prefix_len +
                         group_skip_scan_param->key_infix_len;
  uint max_used_key_length = real_prefix_len;
  unsigned real_key_parts = group_skip_scan_info->num_used_key_parts;
  if (min_max_ranges.size() > 0) {
    // Check if the right-most range has a lower boundary,
    // or the left-most range has an upper boundary.
    if ((have_min && !(min_max_ranges.back()->flag & NO_MIN_RANGE)) ||
        (have_max && !(min_max_ranges[0]->flag & NO_MAX_RANGE))) {
      max_used_key_length += min_max_arg_part->store_length;
      group_skip_scan_info->num_used_key_parts++;
    }
  } else if (have_min && min_max_arg_part &&
             min_max_arg_part->field->is_nullable()) {
    /*
      If a MIN argument value is NULL, we can quickly determine
      that we're in the beginning of the next group, because NULLs
      are always < any other value. This allows us to quickly
      determine the end of the current group and jump to the next
      group (see next_min()) and thus effectively increases the
      usable key length(see next_min()).
    */
    max_used_key_length += min_max_arg_part->store_length;
    group_skip_scan_info->num_used_key_parts++;
  }

  Quick_ranges prefix_ranges(return_mem_root);
  KEY_PART *const used_key_part = param->key[group_skip_scan_info->param_idx];
  if (range_tree) {
    assert(group_skip_scan_info->quick_prefix_records > 0);
    if (group_skip_scan_info->quick_prefix_records != HA_POS_ERROR) {
      /* Prepare for a IndexRangeScanIterator to be used for group prefix
       * retrieval.
       */
      unsigned used_key_parts_unused, num_exact_key_parts_unused;
      if (get_ranges_from_tree(
              return_mem_root, table, used_key_part, keyno, index_tree,
              group_skip_scan_param->group_key_parts, &used_key_parts_unused,
              &num_exact_key_parts_unused, &prefix_ranges)) {
        return nullptr;
      }
      // Opens the ranges if there are more conditions in
      // quick_prefix_query_block than the ones used for jumping through
      // the prefixes.
      //
      // quick_prefix_query_block is made over the conditions on the whole key.
      // It defines a number of ranges of length x.
      // However when jumping through the prefixes we use only the the first
      // few most significant keyparts in the range key. However if there
      // are more keyparts to follow the ones we are using we must make the
      // condition on the key inclusive (because x < "ab" means
      // x[0] < 'a' OR (x[0] == 'a' AND x[1] < 'b').
      // To achieve the above we must turn off the NEAR_MIN/NEAR_MAX
      uint prefix_max_length = 0;
      for (const QUICK_RANGE *range : prefix_ranges) {
        prefix_max_length =
            std::max(prefix_max_length, uint(range->min_length));
        prefix_max_length =
            std::max(prefix_max_length, uint(range->max_length));
      }
      if (group_skip_scan_param->group_prefix_len < prefix_max_length) {
        for (QUICK_RANGE *range : prefix_ranges) {
          range->flag &= ~(NEAR_MIN | NEAR_MAX);
        }
      }
    }
  }

  /* The query passes all tests, so construct a new AccessPath. */
  AccessPath *path = new (param->return_mem_root) AccessPath;
  path->type = AccessPath::GROUP_INDEX_SKIP_SCAN;
  path->set_cost(group_skip_scan_info->cost.total_cost());
  path->set_num_output_rows(group_skip_scan_info->records);
  path->has_group_skip_scan = true;

  // Extract the list of MIN and MAX functions; join->sum_funcs will
  // change after temporary table setup, so it needs to be done before the
  // iterator is created.
  group_skip_scan_param->min_functions.init(param->return_mem_root);
  group_skip_scan_param->max_functions.init(param->return_mem_root);
  if (group_skip_scan_param->min_max_arg_part != nullptr) {
    Item_sum *min_max_item;
    Item_sum **func_ptr = join->sum_funcs;
    while ((min_max_item = *(func_ptr++))) {
      if (min_max_item->sum_func() == Item_sum::MIN_FUNC)
        group_skip_scan_param->min_functions.push_back(min_max_item);
      else if (min_max_item->sum_func() == Item_sum::MAX_FUNC)
        group_skip_scan_param->max_functions.push_back(min_max_item);
    }
  }

  group_skip_scan_param->real_key_parts = real_key_parts;
  group_skip_scan_param->max_used_key_length = max_used_key_length;
  group_skip_scan_param->prefix_ranges = std::move(prefix_ranges);
  group_skip_scan_param->key_infix_ranges = std::move(key_infix_ranges);
  group_skip_scan_param->min_max_ranges = std::move(min_max_ranges);
  if (cost_est < group_skip_scan_info->cost.total_cost() &&
      group_skip_scan_param->have_agg_distinct) {
    trace_group_skip.add("index_scan", true);
    path->set_cost(0.0);
    group_skip_scan_param->is_index_scan = true;
  } else {
    group_skip_scan_param->is_index_scan = false;
  }

  path->group_index_skip_scan().table = table;
  path->group_index_skip_scan().index =
      param->real_keynr[group_skip_scan_info->param_idx];
  path->group_index_skip_scan().num_used_key_parts =
      group_skip_scan_info->num_used_key_parts;
  path->group_index_skip_scan().param = group_skip_scan_param;

  trace_group_skip.end();

  return path;
}

/*
  Check that the MIN/MAX attribute participates only in range predicates
  with constants.

  SYNOPSIS
    check_group_min_max_predicates()
    cond              tree (or subtree) describing all or part of the WHERE
                      clause being analyzed
    min_max_arg_item  the field referenced by the MIN/MAX function(s)
    min_max_arg_part  the keypart of the MIN/MAX argument if any

  DESCRIPTION
    The function walks recursively over the cond tree representing a WHERE
    clause, and checks condition (SA3) - if a field is referenced by a MIN/MAX
    aggregate function, it is referenced only by one of the following
    predicates: {=, !=, <, <=, >, >=, between, is null, is not null}.

  RETURN
    true  if cond passes the test
    false o/w
*/

static bool check_group_min_max_predicates(Item *cond,
                                           Item_field *min_max_arg_item,
                                           Field::imagetype image_type) {
  DBUG_TRACE;
  assert(cond && min_max_arg_item);

  cond = cond->real_item();
  Item::Type cond_type = cond->type();
  if (cond_type == Item::COND_ITEM) /* 'AND' or 'OR' */
  {
    DBUG_PRINT("info", ("Analyzing: %s", ((Item_func *)cond)->func_name()));
    List_iterator_fast<Item> li(*((Item_cond *)cond)->argument_list());
    Item *and_or_arg;
    while ((and_or_arg = li++)) {
      if (!check_group_min_max_predicates(and_or_arg, min_max_arg_item,
                                          image_type))
        return false;
    }
    return true;
  }

  /*
    TODO:
    This is a very crude fix to handle sub-selects in the WHERE clause
    (Item_subselect objects). With the test below we rule out from the
    optimization all queries with subselects in the WHERE clause. What has to
    be done, is that here we should analyze whether the subselect references
    the MIN/MAX argument field, and disallow the optimization only if this is
    so.
    Need to handle subselect in min_max_inspect_cond_for_fields() once this
    is fixed.
  */
  if (cond_type == Item::SUBQUERY_ITEM) return false;

  /*
    Condition of the form 'field' is equivalent to 'field <> 0' and thus
    satisfies the SA3 condition.
  */
  if (cond_type == Item::FIELD_ITEM) {
    DBUG_PRINT("info", ("Analyzing: %s", cond->full_name()));
    return true;
  }

  /*
    At this point, we have weeded out most conditions other than
    function items. However, there are cases like the following:

      select 1 in (select max(c) from t1 where max(1) group by a)

    Here the condition "where max(1)" is an Item_sum_max, not an
    Item_func. In this particular case, the where clause should
    be equivalent to "where max(1) <> 0". A where clause
    phrased that way does not satisfy the SA3 condition of
    get_best_group_skip_scan(). The "where max(1) = true" clause
    causes this method to reject the access method
    (i.e., to return false).

    It's been suggested that it may be possible to use the access method
    for a sub-family of cases when we're aggregating constants or
    outer references. For the moment, we bail out and we reject
    the access method for the query.

    It's hard to prove that there are no other cases where the
    condition is not an Item_func. So, for the moment, don't apply
    the optimization if the condition is not a function item.
  */
  if (cond_type == Item::SUM_FUNC_ITEM) {
    return false;
  }

  /*
   If this is a debug server, then we want to know about
   additional oddball cases which might benefit from this
   optimization.
  */
  assert(cond_type == Item::FUNC_ITEM);
  if (cond_type != Item::FUNC_ITEM) {
    return false;
  }

  /* Test if cond references only group-by or non-group fields. */
  Item_func *pred = (Item_func *)cond;
  Item *cur_arg;
  DBUG_PRINT("info", ("Analyzing: %s", pred->func_name()));
  for (uint arg_idx = 0; arg_idx < pred->argument_count(); arg_idx++) {
    Item **arguments = pred->arguments();
    cur_arg = arguments[arg_idx]->real_item();
    DBUG_PRINT("info", ("cur_arg: %s", cur_arg->full_name()));
    if (cur_arg->type() == Item::FIELD_ITEM) {
      if (min_max_arg_item->eq(cur_arg, true)) {
        /*
          If pred references the MIN/MAX argument, check whether pred is a range
          condition that compares the MIN/MAX argument with a constant.
        */
        Item_func::Functype pred_type = pred->functype();
        if (pred_type != Item_func::EQUAL_FUNC &&
            pred_type != Item_func::LT_FUNC &&
            pred_type != Item_func::LE_FUNC &&
            pred_type != Item_func::GT_FUNC &&
            pred_type != Item_func::GE_FUNC &&
            pred_type != Item_func::BETWEEN &&
            pred_type != Item_func::ISNULL_FUNC &&
            pred_type != Item_func::ISNOTNULL_FUNC &&
            pred_type != Item_func::EQ_FUNC && pred_type != Item_func::NE_FUNC)
          return false;

        /* Check that pred compares min_max_arg_item with a constant. */
        Item *args[3];
        memset(args, 0, 3 * sizeof(Item *));
        bool inv;
        /* Test if this is a comparison of a field and a constant. */
        if (!is_simple_predicate(pred, args, &inv)) return false;

        /* Check for compatible string comparisons - similar to get_mm_leaf. */
        if (args[0] && args[1] && !args[2] &&  // this is a binary function
            min_max_arg_item->result_type() == STRING_RESULT &&
            /*
              Don't use an index when comparing strings of different collations.
            */
            ((args[1]->result_type() == STRING_RESULT &&
              image_type == Field::itRAW &&
              min_max_arg_item->field->charset() !=
                  pred->compare_collation()) ||
             /*
               We can't always use indexes when comparing a string index to a
               number.
             */
             (args[1]->result_type() != STRING_RESULT &&
              min_max_arg_item->field->cmp_type() != args[1]->result_type())))
          return false;
      }
    } else if (cur_arg->type() == Item::FUNC_ITEM) {
      if (!check_group_min_max_predicates(cur_arg, min_max_arg_item,
                                          image_type))
        return false;
    } else if (cur_arg->const_item()) {
      /*
        For predicates of the form "const OP expr" we also have to check 'expr'
        to make a decision.
      */
      continue;
    } else
      return false;
  }

  return true;
}

/**
  Utility function used by min_max_inspect_cond_for_fields() for comparing
  FIELD item with given MIN/MAX item and setting appropriate out parameter.

@param         item_field         Item field for comparison.
@param         min_max_arg_item   The field referenced by the MIN/MAX
                                  function(s).
@param [out]   min_max_arg_present    This out parameter is set to true if
                                      MIN/MAX argument is present in cond.
@param [out]   non_min_max_arg_present This out parameter is set to true if
                                       any field item other than MIN/MAX
                                       argument is present in cond.
*/
static inline void util_min_max_inspect_item(Item *item_field,
                                             Item_field *min_max_arg_item,
                                             bool *min_max_arg_present,
                                             bool *non_min_max_arg_present) {
  if (item_field->type() == Item::FIELD_ITEM) {
    if (min_max_arg_item->eq(item_field, true))
      *min_max_arg_present = true;
    else
      *non_min_max_arg_present = true;
  }
}

/**
  This function detects the presents of MIN/MAX field along with at least
  one non MIN/MAX field participation in the given condition. Subqueries
  inspection is skipped as of now.

  @param         cond   tree (or subtree) describing all or part of the WHERE
                        clause being analyzed.
  @param         min_max_arg_item   The field referenced by the MIN/MAX
                                    function(s).
  @param [out]   min_max_arg_present    This out parameter is set to true if
                                        MIN/MAX argument is present in cond.
  @param [out]   non_min_max_arg_present This out parameter is set to true if
                                         any field item other than MIN/MAX
                                         argument is present in cond.

  @return  TRUE if both MIN/MAX field and non MIN/MAX field is present in cond.
           FALSE o/w.

  @todo: When the hack present in check_group_min_max_predicate() is removed,
         subqueries needs to be inspected.
*/

static bool min_max_inspect_cond_for_fields(Item *cond,
                                            Item_field *min_max_arg_item,
                                            bool *min_max_arg_present,
                                            bool *non_min_max_arg_present) {
  DBUG_TRACE;
  assert(cond && min_max_arg_item);

  cond = cond->real_item();
  Item::Type cond_type = cond->type();

  switch (cond_type) {
    case Item::COND_ITEM: {
      DBUG_PRINT("info", ("Analyzing: %s", ((Item_func *)cond)->func_name()));
      List_iterator_fast<Item> li(*((Item_cond *)cond)->argument_list());
      Item *and_or_arg;
      while ((and_or_arg = li++)) {
        min_max_inspect_cond_for_fields(and_or_arg, min_max_arg_item,
                                        min_max_arg_present,
                                        non_min_max_arg_present);
        if (*min_max_arg_present && *non_min_max_arg_present) return true;
      }

      return false;
    }
    case Item::FUNC_ITEM: {
      /* Test if cond references both group-by and non-group fields. */
      Item_func *pred = (Item_func *)cond;
      Item *cur_arg;
      DBUG_PRINT("info", ("Analyzing: %s", pred->func_name()));
      for (uint arg_idx = 0; arg_idx < pred->argument_count(); arg_idx++) {
        Item **arguments = pred->arguments();
        cur_arg = arguments[arg_idx]->real_item();
        DBUG_PRINT("info", ("cur_arg: %s", cur_arg->full_name()));

        if (cur_arg->type() == Item::FUNC_ITEM) {
          min_max_inspect_cond_for_fields(cur_arg, min_max_arg_item,
                                          min_max_arg_present,
                                          non_min_max_arg_present);
        } else {
          util_min_max_inspect_item(cur_arg, min_max_arg_item,
                                    min_max_arg_present,
                                    non_min_max_arg_present);
        }

        if (*min_max_arg_present && *non_min_max_arg_present) return true;
      }

      if (pred->functype() == Item_func::MULT_EQUAL_FUNC) {
        /*
          Analyze participating fields in a multiequal condition.
        */
        for (Item_field &item_field :
             down_cast<Item_equal *>(cond)->get_fields()) {
          util_min_max_inspect_item(&item_field, min_max_arg_item,
                                    min_max_arg_present,
                                    non_min_max_arg_present);

          if (*min_max_arg_present && *non_min_max_arg_present) return true;
        }
      }

      break;
    }
    case Item::FIELD_ITEM: {
      util_min_max_inspect_item(cond, min_max_arg_item, min_max_arg_present,
                                non_min_max_arg_present);
      DBUG_PRINT("info", ("Analyzing: %s", cond->full_name()));
      return false;
    }
    default:
      break;
  }

  return false;
}

/*
  Check for conjunction of equality predicates on the non-group key parts.

  SYNOPSIS
    check_key_infix()
    index_range_tree       [in]  Range tree for the chosen index
    first_non_group_part   [in]  First index part after group attribute parts
    min_max_arg_part       [in]  The keypart of the MIN/MAX argument if any
    last_part              [in]  Last keypart of the index
    key_infix_len          [out] Length of the infix
    first_non_infix_part   [out] The first keypart after the infix (if any)
    infix_factor           [out] The number of combinations of infixes
                                 that can be possible.
    index_info             [in]  Pointer to KEY object

  DESCRIPTION
    Test conditions (NGA1, NGA2) from get_best_group_skip_scan(). Namely,
    for each keypart field NGF_i not in GROUP-BY, check that there is at least
    one equality range predicate for each key part among conds with the form
    (NGF_i = const_ci) or (const_ci = NGF_i).
    Thus all the NGF_i attributes must fill the 'gap' between the last group-by
    attribute and the MIN/MAX attribute in the index (if present). If these
    conditions hold, update key_infix_len with the total length of the key
    parts in key_infix. Increase the infix_factor by the number of ranges
    present on NGF_i.

  RETURN
    true  if the index passes the test
    false o/w
*/

static bool check_key_infix(SEL_ROOT *index_range_tree,
                            KEY_PART_INFO *first_non_group_part,
                            KEY_PART_INFO *min_max_arg_part,
                            KEY_PART_INFO *last_part, uint *key_infix_len,
                            KEY_PART_INFO **first_non_infix_part,
                            uint *infix_factor, KEY *index_info) {
  SEL_ROOT *cur_range;
  KEY_PART_INFO *cur_part;
  /* End part for the first loop below. */
  KEY_PART_INFO *end_part = min_max_arg_part ? min_max_arg_part : last_part;

  *key_infix_len = 0;
  *infix_factor = 1;

  for (cur_part = first_non_group_part; cur_part != end_part; cur_part++) {
    cur_range = nullptr;
    /*
      get_sel_root_for_keypart gets the range tree for the key part and
      also checks for a unique conjunction of this tree with all the
      predicates on the earlier keyparts in the index.
    */
    if (get_sel_root_for_keypart(cur_part - index_info->key_part,
                                 index_range_tree, &cur_range))
      return false;

    if (!cur_range || cur_range->type != SEL_ROOT::Type::KEY_RANGE) {
      if (min_max_arg_part)
        return false; /* The current keypart has no range predicates at all. */
      else {
        *first_non_infix_part = cur_part;
        return true;
      }
    }

    /*
      Check if all ranges are equality or NULL ranges for key the current
      key part.
    */
    SEL_ARG *tmp_range = cur_range->root->first();
    while (tmp_range) {
      if ((tmp_range->min_flag & NO_MIN_RANGE) ||
          (tmp_range->max_flag & NO_MAX_RANGE) ||
          (tmp_range->min_flag & NEAR_MIN) || (tmp_range->max_flag & NEAR_MAX))
        return false;
      if (!((tmp_range->maybe_null() && tmp_range->min_value[0] &&
             tmp_range->max_value[0]) ||
            memcmp(tmp_range->min_value, tmp_range->max_value,
                   cur_part->store_length) == 0))
        return false;
      tmp_range = tmp_range->next;
    }
    *key_infix_len += cur_part->store_length;
    *infix_factor *= cur_range->elements;
  }

  if (!min_max_arg_part && (cur_part == last_part))
    *first_non_infix_part = last_part;

  return true;
}

/*
  Find the key part referenced by a field.

  SYNOPSIS
    get_field_keypart()
    index  descriptor of an index
    field  field that possibly references some key part in index

  NOTES
    The return value can be used to get a KEY_PART_INFO pointer by
    part= index->key_part + get_field_keypart(...) - 1;

  RETURN
    Positive number which is the consecutive number of the key part, or
    0 if field does not reference any index field.
*/

static inline uint get_field_keypart(KEY *index, const Field *field) {
  KEY_PART_INFO *part, *end;

  for (part = index->key_part, end = part + actual_key_parts(index); part < end;
       part++) {
    if (field->eq(part->field)) return part - index->key_part + 1;
  }
  return 0;
}

/*
  Compute the cost of a quick_group_skip_scan_query_block for a particular
  index.

  SYNOPSIS
    cost_group_skip_scan()
    table                [in] The table being accessed
    key                  [in] The index used to access the table
    used_key_parts       [in] Number of key parts used to access the index
    group_key_parts      [in] Number of index key parts in the group prefix
    range_tree           [in] Tree of ranges for all indexes
    quick_prefix_records [in] Number of records retrieved by the internally
                              used quick range select if any
    have_min             [in] True if there is a MIN function
    have_max             [in] True if there is a MAX function
    infix_factor         [in] The number of combinations of infix ranges that
                              can be possible (increases the number of groups).
    cost_est            [out] The cost to retrieve rows via this quick select
    records             [out] The number of rows retrieved
    single_group         [in] True if this query produces only one group because
                              there are equality predicates on grouping
                              attributes.

  DESCRIPTION
    This method computes the access cost of an GROUP_INDEX_SKIP_SCAN access path
    and the number of rows returned.

  NOTES
    The cost computation distinguishes several cases:
    1) No equality predicates over non-group attributes (thus no key_infix).
       If groups are bigger than blocks on the average, then we assume that it
       is very unlikely that block ends are aligned with group ends, thus even
       if we look for both MIN and MAX keys, all pairs of neighbor MIN/MAX
       keys, except for the first MIN and the last MAX keys, will be in the
       same block.  If groups are smaller than blocks, then we are going to
       read all blocks.
    2) There are equality predicates over non-group attributes.
       In this case the group prefix is extended by additional constants, and
       as a result the min/max values are inside sub-groups of the original
       groups. The number of blocks that will be read depends on whether the
       ends of these sub-groups will be contained in the same or in different
       blocks. We compute the probability for the two ends of a subgroup to be
       in two different blocks as the ratio of:
       - the number of positions of the left-end of a subgroup inside a group,
         such that the right end of the subgroup is past the end of the buffer
         containing the left-end, and
       - the total number of possible positions for the left-end of the
         subgroup, which is the number of keys in the containing group.
       We assume it is very unlikely that two ends of subsequent subgroups are
       in the same block.
    3) The are range predicates over the group attributes.
       Then some groups may be filtered by the range predicates. We use the
       selectivity of the range predicates to decide how many groups will be
       filtered.

  TODO
     - Take into account the optional range predicates over the MIN/MAX
       argument.
     - Check if we have a PK index and we use all cols - then each key is a
       group, and it will be better to use an index scan.
     - quick_prefix_records used in calculating group prefix selectivity
       is not the correct estimate always. When group prefix has equality
       predicates, range optimizer uses infix ranges (if present) to get
       possible number of rows that would be read from storage engine
       (quick_prefix_records). But we just need the number of rows that
       would be read when only group prefix predicates are used.
       This needs fixing.
     - When both min and max are present, LIS will make two reads per group
       instead of one. Similarly when min and max functions are not present,
       rows retrieved are different. Cost model should reflect what happens
       in GroupIndexSkipScanIterator::Read()

  RETURN
    None
*/

static void cost_group_skip_scan(TABLE *table, uint key, uint used_key_parts,
                                 uint group_key_parts, SEL_TREE *range_tree,
                                 ha_rows quick_prefix_records, bool have_min,
                                 bool have_max, uint infix_factor,
                                 Cost_estimate *cost_est, ha_rows *records,
                                 bool single_group) {
  ha_rows table_records;
  uint num_groups;
  uint num_blocks;
  uint keys_per_block;
  rec_per_key_t keys_per_group;
  double p_overlap; /* Probability that a sub-group overlaps two blocks. */
  double quick_prefix_selectivity;
  double io_blocks;  // Number of blocks to read from table
  DBUG_TRACE;
  assert(cost_est->is_zero());

  const KEY *const index_info = &table->key_info[key];
  table_records = table->file->stats.records;
  keys_per_block = (table->file->stats.block_size / 2 /
                        (index_info->key_length + table->file->ref_length) +
                    1);
  num_blocks = (uint)(table_records / keys_per_block) + 1;

  /* Compute the number of keys in a group. */
  if (index_info->has_records_per_key(group_key_parts - 1))
    // Use index statistics
    keys_per_group = index_info->records_per_key(group_key_parts - 1);
  else
    /* If there is no statistics try to guess */
    keys_per_group = guess_rec_per_key(table, index_info, group_key_parts);

  if (single_group)
    // Pre-determined in the calling function that the query will have
    // only one group.
    num_groups = 1;
  else {
    num_groups = (uint)(table_records / keys_per_group) + 1;

    /* Apply the selectivity of the quick select for group prefixes. */
    if (range_tree && (quick_prefix_records != HA_POS_ERROR)) {
      quick_prefix_selectivity =
          (double)quick_prefix_records / (double)table_records;
      num_groups = (uint)rint(num_groups * quick_prefix_selectivity);
      num_groups = std::max(num_groups, 1U);
    }
  }

  if (used_key_parts > group_key_parts) {
    // Average number of keys in sub-groups formed by a key infixes
    rec_per_key_t keys_in_subgroup;
    if (index_info->has_records_per_key(used_key_parts - 1))
      // Use index statistics
      keys_in_subgroup = index_info->records_per_key(used_key_parts - 1);
    else {
      // If no index statistics then we use a guessed records per key value.
      keys_in_subgroup = guess_rec_per_key(table, index_info, used_key_parts);
      keys_in_subgroup = std::min(keys_in_subgroup, keys_per_group);
    }

    /*
      Compute the probability that two ends of subgroups are inside
      different blocks. Keys in subgroup need to be increases by the number
      of infix ranges possible.
    */
    keys_in_subgroup = keys_in_subgroup * infix_factor;
    if (keys_in_subgroup >= keys_per_block) /* If a subgroup is bigger than */
      p_overlap = 1.0; /* a block, it will overlap at least two blocks. */
    else {
      double blocks_per_group = (double)num_blocks / (double)num_groups;
      p_overlap = (blocks_per_group * (keys_in_subgroup - 1)) / keys_per_group;
      p_overlap = min(p_overlap, 1.0);
    }
    io_blocks = min<double>(num_groups * (1 + p_overlap), num_blocks);
  } else
    io_blocks = (keys_per_group > keys_per_block)
                    ? (have_min && have_max) ? (double)(num_groups + 1)
                                             : (double)num_groups
                    : (double)num_blocks;

  /*
    Estimate IO cost.
  */
  const Cost_model_table *const cost_model = table->cost_model();
  cost_est->add_io(cost_model->page_read_cost_index(key, io_blocks));

  /* Infix factor increases the number of groups (rows) examined. */
  num_groups *= infix_factor;
  /*
    CPU cost must be comparable to that of an index scan as computed
    in test_quick_select(). When the groups are small,
    e.g. for a unique index, using index scan will be cheaper since it
    reads the next record without having to re-position to it on every
    group. To make the CPU cost reflect this, we estimate the CPU cost
    as the sum of:
    1. Cost for evaluating the condition (similarly as for index scan).
    2. Cost for navigating the index structure (assuming a b-tree).
       Note: We only add the cost for one comparison per block. For a
             b-tree the number of comparisons will be larger.
       TODO: This cost should be provided by the storage engine.
  */
  if (keys_per_block <= 1) {
    // Only one key per block? A *very* high tree.
    cost_est->add_cpu(std::numeric_limits<double>::max());
  } else {
    const double tree_height =
        table_records == 0
            ? 1.0
            : ceil(log(double(table_records)) / log(double(keys_per_block)));
    const double tree_traversal_cost =
        cost_model->key_compare_cost(tree_height);
    const double cpu_cost =
        num_groups * (tree_traversal_cost + cost_model->row_evaluate_cost(1.0));
    cost_est->add_cpu(cpu_cost);
  }
  *records = num_groups;

  DBUG_PRINT("info", ("table rows: %lu  keys/block: %u  keys/group: %.1f  "
                      "result rows: %lu  blocks: %u",
                      (ulong)table_records, keys_per_block, keys_per_group,
                      (ulong)*records, num_blocks));
}

static bool add_range(MEM_ROOT *return_mem_root, SEL_ARG *sel_range,
                      uint key_length, Quick_ranges *range_array) {
  uint range_flag = sel_range->min_flag | sel_range->max_flag;

  /* Skip (-inf,+inf) ranges, e.g. (x < 5 or x > 4). */
  if ((range_flag & NO_MIN_RANGE) && (range_flag & NO_MAX_RANGE)) return false;

  if (!(sel_range->min_flag & NO_MIN_RANGE) &&
      !(sel_range->max_flag & NO_MAX_RANGE)) {
    if (sel_range->maybe_null() && sel_range->min_value[0] &&
        sel_range->max_value[0])
      range_flag |= NULL_RANGE; /* IS NULL condition */
    /*
      Do not perform comparison if one of the arguments is NULL.
    */
    else if (!sel_range->min_value[0] && !sel_range->max_value[0] &&
             memcmp(sel_range->min_value, sel_range->max_value, key_length) ==
                 0)
      range_flag |= EQ_RANGE; /* equality condition */
  }
  QUICK_RANGE *range = new (return_mem_root) QUICK_RANGE(
      return_mem_root, sel_range->min_value, key_length,
      make_keypart_map(sel_range->part), sel_range->max_value, key_length,
      make_keypart_map(sel_range->part), range_flag, HA_READ_INVALID);
  if (!range) return true;
  if (range_array->push_back(range)) return true;
  return false;
}

#ifndef NDEBUG
void dbug_dump_group_index_skip_scan(int indent, bool, const AccessPath *path) {
  const GroupIndexSkipScanParameters *param =
      path->group_index_skip_scan().param;

  fprintf(DBUG_FILE,
          "%*squick_group_skip_scan_query_block: index %s (%d), length: %d\n",
          indent, "", param->index_info->name,
          path->group_index_skip_scan().index, param->max_used_key_length);
  if (param->key_infix_len > 0) {
    fprintf(DBUG_FILE, "%*susing key_infix with length %d:\n", indent, "",
            param->key_infix_len);
  }
  if (param->min_max_ranges.size() > 0) {
    fprintf(DBUG_FILE, "%*susing %d quick_ranges for MIN/MAX:\n", indent, "",
            static_cast<int>(param->min_max_ranges.size()));
  }
}
#endif
