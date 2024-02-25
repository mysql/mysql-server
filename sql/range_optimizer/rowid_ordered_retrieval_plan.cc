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

#include "sql/range_optimizer/rowid_ordered_retrieval_plan.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <utility>

#include "m_ctype.h"
#include "m_string.h"
#include "my_alloc.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql/key.h"
#include "sql/key_spec.h"
#include "sql/mem_root_array.h"
#include "sql/opt_costmodel.h"
#include "sql/opt_hints.h"
#include "sql/opt_trace.h"
#include "sql/range_optimizer/index_range_scan.h"
#include "sql/range_optimizer/index_range_scan_plan.h"
#include "sql/range_optimizer/internal.h"
#include "sql/range_optimizer/path_helpers.h"
#include "sql/range_optimizer/range_opt_param.h"
#include "sql/range_optimizer/rowid_ordered_retrieval.h"
#include "sql/range_optimizer/tree.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_lex.h"
#include "sql/sql_optimizer.h"
#include "sql/table.h"
#include "sql_string.h"

class Opt_trace_context;

using std::max;
using std::min;

#ifndef NDEBUG
static void print_ror_scans_arr(TABLE *table, const char *msg,
                                ROR_SCAN_INFO **start, ROR_SCAN_INFO **end) {
  DBUG_TRACE;

  char buff[1024];
  String tmp(buff, sizeof(buff), &my_charset_bin);
  tmp.length(0);
  for (; start != end; start++) {
    if (tmp.length()) tmp.append(',');
    tmp.append(table->key_info[(*start)->keynr].name);
  }
  if (!tmp.length()) tmp.append(STRING_WITH_LEN("(empty)"));
  DBUG_PRINT("info", ("ROR key scans (%s): %s", msg, tmp.ptr()));
  fprintf(DBUG_FILE, "ROR key scans (%s): %s", msg, tmp.ptr());
}
#endif

void trace_basic_info_rowid_intersection(THD *thd, const AccessPath *path,
                                         const RANGE_OPT_PARAM *param,
                                         Opt_trace_object *trace_object) {
  trace_object->add_alnum("type", "index_roworder_intersect")
      .add("rows", path->num_output_rows())
      .add("cost", path->cost)
      .add("covering", path->rowid_intersection().is_covering)
      .add("clustered_pk_scan",
           path->rowid_intersection().cpk_child != nullptr);

  Opt_trace_context *const trace = &thd->opt_trace;
  Opt_trace_array ota(trace, "intersect_of");
  for (AccessPath *child : *path->rowid_intersection().children) {
    Opt_trace_object trace_isect_idx(trace);
    trace_basic_info(thd, child, param, &trace_isect_idx);
  }
}

void trace_basic_info_rowid_union(THD *thd, const AccessPath *path,
                                  const RANGE_OPT_PARAM *param,
                                  Opt_trace_object *trace_object) {
  Opt_trace_context *const trace = &thd->opt_trace;
  trace_object->add_alnum("type", "index_roworder_union");
  Opt_trace_array ota(trace, "union_of");
  for (AccessPath *child : *path->rowid_union().children) {
    Opt_trace_object path_info(trace);
    ::trace_basic_info(thd, child, param, &path_info);
  }
}

/*
  Create ROR_SCAN_INFO* structure with a single ROR scan on index idx using
  sel_arg set of intervals.

  SYNOPSIS
    make_ror_scan()
      param    Parameter from test_quick_select function
      idx      Index of key in param->keys
      sel_root Set of intervals for a given key
      needed_fields  Bitmask of fields needed by the query

  RETURN
    NULL - out of memory
    ROR scan structure containing a scan for {idx, sel_arg}
*/

static ROR_SCAN_INFO *make_ror_scan(const RANGE_OPT_PARAM *param, int idx,
                                    SEL_ROOT *sel_root,
                                    const MY_BITMAP *needed_fields) {
  ROR_SCAN_INFO *ror_scan;
  my_bitmap_map *bitmap_buf1;
  my_bitmap_map *bitmap_buf2;
  uint keynr;
  DBUG_TRACE;

  if (!(ror_scan = new (param->return_mem_root) ROR_SCAN_INFO)) return nullptr;

  ror_scan->idx = idx;
  ror_scan->keynr = keynr = param->real_keynr[idx];
  ror_scan->sel_root = sel_root;
  ror_scan->records = param->table->quick_rows[keynr];

  if (!(bitmap_buf1 = (my_bitmap_map *)param->return_mem_root->Alloc(
            param->table->s->column_bitmap_size)))
    return nullptr;
  if (!(bitmap_buf2 = (my_bitmap_map *)param->return_mem_root->Alloc(
            param->table->s->column_bitmap_size)))
    return nullptr;

  if (bitmap_init(&ror_scan->covered_fields, bitmap_buf1,
                  param->table->s->fields))
    return nullptr;
  if (bitmap_init(&ror_scan->covered_fields_remaining, bitmap_buf2,
                  param->table->s->fields))
    return nullptr;

  bitmap_clear_all(&ror_scan->covered_fields);

  KEY_PART_INFO *key_part = param->table->key_info[keynr].key_part;
  KEY_PART_INFO *key_part_end =
      key_part + param->table->key_info[keynr].user_defined_key_parts;
  for (; key_part != key_part_end; ++key_part) {
    if (bitmap_is_set(needed_fields, key_part->fieldnr - 1))
      bitmap_set_bit(&ror_scan->covered_fields, key_part->fieldnr - 1);
  }
  bitmap_copy(&ror_scan->covered_fields_remaining, &ror_scan->covered_fields);

  double rows = rows2double(param->table->quick_rows[ror_scan->keynr]);
  ror_scan->index_read_cost =
      param->table->file->index_scan_cost(ror_scan->keynr, 1, rows);

  Quick_ranges ranges(param->return_mem_root);
  unsigned num_exact_key_parts_unused;
  if (get_ranges_from_tree(param->return_mem_root, param->table,
                           param->key[idx], param->real_keynr[idx], sel_root,
                           MAX_REF_PARTS, &ror_scan->used_key_parts,
                           &num_exact_key_parts_unused, &ranges)) {
    return nullptr;
  }
  ror_scan->ranges = {&ranges[0], ranges.size()};

  return ror_scan;
}

/**
  Compare two ROR_SCAN_INFO* by
    1. Number of fields in this index that are not already covered
       by other indexes earlier in the intersect ordering: descending
    2. E(Number of records): ascending

  @param scan1   first ror scan to compare
  @param scan2   second ror scan to compare

  @return true if scan1 > scan2, false otherwise
*/
static bool is_better_intersect_match(const ROR_SCAN_INFO *scan1,
                                      const ROR_SCAN_INFO *scan2) {
  if (scan1 == scan2) return false;

  if (scan1->num_covered_fields_remaining > scan2->num_covered_fields_remaining)
    return false;

  if (scan1->num_covered_fields_remaining < scan2->num_covered_fields_remaining)
    return true;

  return (scan1->records > scan2->records);
}

/**
  Sort indexes in an order that is likely to be a good index merge
  intersection order. After running this function, [start, ..., end-1]
  is ordered according to this strategy:

    1) Minimize the number of indexes that must be used in the
       intersection. I.e., the index covering most fields not already
       covered by other indexes earlier in the sort order is picked first.
    2) When multiple indexes cover equally many uncovered fields, the
       index with lowest E(Number of rows) is chosen.

  Note that all permutations of index ordering are not tested, so this
  function may not find the optimal order.

  @param[in,out] start     Pointer to the start of indexes that may
                           be used in index merge intersection
  @param         end       Pointer past the last index that may be used.
  @param         param     Parameter from test_quick_select function.
  @param         needed_fields  Bitmask of fields needed by the query.
*/
static void find_intersect_order(ROR_SCAN_INFO **start, ROR_SCAN_INFO **end,
                                 const RANGE_OPT_PARAM *param,
                                 const MY_BITMAP *needed_fields) {
  // nothing to sort if there are only zero or one ROR scans
  if ((start == end) || (start + 1 == end)) return;

  /*
    Bitmap of fields we would like the ROR scans to cover. Will be
    modified by the loop below so that when we're looking for a ROR
    scan in position 'x' in the ordering, all fields covered by ROR
    scans 0,...,x-1 have been removed.
  */
  MY_BITMAP fields_to_cover;
  my_bitmap_map *map;
  if (!(map = (my_bitmap_map *)param->temp_mem_root->Alloc(
            param->table->s->column_bitmap_size)))
    return;
  bitmap_init(&fields_to_cover, map, needed_fields->n_bits);
  bitmap_copy(&fields_to_cover, needed_fields);

  // Sort ROR scans in [start,...,end-1]
  for (ROR_SCAN_INFO **place = start; place < (end - 1); place++) {
    /* Placeholder for the best ROR scan found for position 'place' so far */
    ROR_SCAN_INFO **best = place;
    ROR_SCAN_INFO **current = place + 1;

    {
      /*
        Calculate how many fields in 'fields_to_cover' not already
        covered by [start,...,place-1] the 'best' index covers. The
        result is used in is_better_intersect_match() and is valid
        when finding the best ROR scan for position 'place' only.
      */
      bitmap_intersect(&(*best)->covered_fields_remaining, &fields_to_cover);
      (*best)->num_covered_fields_remaining =
          bitmap_bits_set(&(*best)->covered_fields_remaining);
    }
    for (; current < end; current++) {
      {
        /*
          Calculate how many fields in 'fields_to_cover' not already
          covered by [start,...,place-1] the 'current' index covers.
          The result is used in is_better_intersect_match() and is
          valid when finding the best ROR scan for position 'place' only.
        */
        bitmap_intersect(&(*current)->covered_fields_remaining,
                         &fields_to_cover);
        (*current)->num_covered_fields_remaining =
            bitmap_bits_set(&(*current)->covered_fields_remaining);

        /*
          No need to compare with 'best' if 'current' does not
          contribute with uncovered fields.
        */
        if ((*current)->num_covered_fields_remaining == 0) continue;
      }

      if (is_better_intersect_match(*best, *current)) best = current;
    }

    /*
      'best' is now the ROR scan that will be sorted in position
      'place'. When searching for the best ROR scans later in the sort
      sequence we do not need coverage of the fields covered by 'best'
    */
    bitmap_subtract(&fields_to_cover, &(*best)->covered_fields);
    if (best != place) std::swap(*best, *place);

    if (bitmap_is_clear_all(&fields_to_cover))
      return;  // No more fields to cover
  }
}

/* Auxiliary structure for incremental ROR-intersection creation */
typedef struct {
  const RANGE_OPT_PARAM *param;
  MY_BITMAP covered_fields; /* union of fields covered by all scans */
  /*
    Fraction of table records that satisfies conditions of all scans.
    This is the number of full records that will be retrieved if a
    non-index_only index intersection will be employed.
  */
  double out_rows;
  /* true if covered_fields is a superset of needed_fields */
  bool is_covering;

  ha_rows index_records;         /* sum(#records to look in indexes) */
  Cost_estimate index_scan_cost; /* SUM(cost of 'index-only' scans) */
  Cost_estimate total_cost;
} ROR_INTERSECT_INFO;

/*
  Allocate a ROR_INTERSECT_INFO and initialize it to contain zero scans.

  SYNOPSIS
    ror_intersect_init()
      param         Parameter from test_quick_select

  RETURN
    allocated structure
    NULL on error
*/

static ROR_INTERSECT_INFO *ror_intersect_init(const RANGE_OPT_PARAM *param) {
  ROR_INTERSECT_INFO *info;
  my_bitmap_map *buf;
  if (!(info = new (param->return_mem_root) ROR_INTERSECT_INFO)) return nullptr;
  info->param = param;
  if (!(buf = (my_bitmap_map *)param->temp_mem_root->Alloc(
            param->table->s->column_bitmap_size)))
    return nullptr;
  if (bitmap_init(&info->covered_fields, buf, param->table->s->fields))
    return nullptr;
  info->is_covering = false;
  info->index_scan_cost.reset();
  info->total_cost.reset();
  info->index_records = 0;
  info->out_rows = (double)param->table->file->stats.records;
  bitmap_clear_all(&info->covered_fields);
  return info;
}

static void ror_intersect_cpy(ROR_INTERSECT_INFO *dst,
                              const ROR_INTERSECT_INFO *src) {
  dst->param = src->param;
  memcpy(dst->covered_fields.bitmap, src->covered_fields.bitmap,
         no_bytes_in_map(&src->covered_fields));
  dst->out_rows = src->out_rows;
  dst->is_covering = src->is_covering;
  dst->index_records = src->index_records;
  dst->index_scan_cost = src->index_scan_cost;
  dst->total_cost = src->total_cost;
}

/*
  Get selectivity of adding a ROR scan to the ROR-intersection.

  SYNOPSIS
    ror_scan_selectivity()
      info  ROR-interection, an intersection of ROR index scans
      scan  ROR scan that may or may not improve the selectivity
            of 'info'

  NOTES
    Suppose we have conditions on several keys
    cond=k_11=c_11 AND k_12=c_12 AND ...  // key_parts of first key in 'info'
         k_21=c_21 AND k_22=c_22 AND ...  // key_parts of second key in 'info'
          ...
         k_n1=c_n1 AND k_n3=c_n3 AND ...  (1) //key_parts of 'scan'

    where k_ij may be the same as any k_pq (i.e. keys may have common parts).

    Note that for ROR retrieval, only equality conditions are usable so there
    are no open ranges (e.g., k_ij > c_ij) in 'scan' or 'info'.
    FIXME: This isn't true in practice; e.g. i_main.costmodel_planchange ends
    up calling this function with an inequality condition, and thus the
  estimation is probably wrong (since the code assumes only one element in the
  tree).

    A full row is retrieved if entire condition holds.

    The recursive procedure for finding P(cond) is as follows:

    First step:
    Pick 1st part of 1st key and break conjunction (1) into two parts:
      cond= (k_11=c_11 AND R)

    Here R may still contain condition(s) equivalent to k_11=c_11.
    Nevertheless, the following holds:

      P(k_11=c_11 AND R) = P(k_11=c_11) * P(R | k_11=c_11).

    Mark k_11 as fixed field (and satisfied condition) F, save P(F),
    save R to be cond and proceed to recursion step.

    Recursion step:
    We have a set of fixed fields/satisfied conditions) F, probability P(F),
    and remaining conjunction R
    Pick next key part on current key and its condition "k_ij=c_ij".
    We will add "k_ij=c_ij" into F and update P(F).
    Lets denote k_ij as t,  R = t AND R1, where R1 may still contain t. Then

     P((t AND R1)|F) = P(t|F) * P(R1|t|F) = P(t|F) * P(R1|(t AND F)) (2)

    (where '|' mean conditional probability, not "or")

    Consider the first multiplier in (2). One of the following holds:
    a) F contains condition on field used in t (i.e. t AND F = F).
      Then P(t|F) = 1

    b) F doesn't contain condition on field used in t. Then F and t are
     considered independent.

     P(t|F) = P(t|(fields_before_t_in_key AND other_fields)) =
          = P(t|fields_before_t_in_key).

     P(t|fields_before_t_in_key) = #records(fields_before_t_in_key) /
                                   #records(fields_before_t_in_key, t)

    The second multiplier is calculated by applying this step recursively.

  IMPLEMENTATION
    This function calculates the result of application of the "recursion step"
    described above for all fixed key members of a single key, accumulating set
    of covered fields, selectivity, etc.

    The calculation is conducted as follows:
    Lets denote #records(keypart1, ... keypartK) as n_k. We need to calculate

     n_{k1}      n_{k2}
    --------- * ---------  * .... (3)
     n_{k1-1}    n_{k2-1}

    where k1,k2,... are key parts which fields were not yet marked as fixed
    ( this is result of application of option b) of the recursion step for
      parts of a single key).
    Since it is reasonable to expect that most of the fields are not marked
    as fixed, we calculate (3) as

                                  n_{i1}      n_{i2}
    (3) = n_{max_key_part}  / (   --------- * ---------  * ....  )
                                  n_{i1-1}    n_{i2-1}

    where i1,i2, .. are key parts that were already marked as fixed.

    In order to minimize number of expensive records_in_range calls we
    group and reduce adjacent fractions. Note that on the optimizer's
    request, index statistics may be used instead of records_in_range
    @see RANGE_OPT_PARAM::use_index_statistics.

  RETURN
    Selectivity of given ROR scan, a number between 0 and 1. 1 means that
    adding 'scan' to the intersection does not improve the selectivity.
*/

static double ror_scan_selectivity(const ROR_INTERSECT_INFO *info,
                                   const ROR_SCAN_INFO *scan) {
  double selectivity_mult = 1.0;
  const TABLE *const table = info->param->table;
  const KEY_PART_INFO *const key_part = table->key_info[scan->keynr].key_part;
  /**
    key values tuple, used to store both min_range.key and
    max_range.key. This function is only called for equality ranges;
    open ranges (e.g. "min_value < X < max_value") cannot be used for
    rowid ordered retrieval, so in this function we know that
    min_range.key == max_range.key
  */
  uchar key_val[MAX_KEY_LENGTH + MAX_FIELD_WIDTH];
  uchar *key_ptr = key_val;
  SEL_ARG *tuple_arg = nullptr;
  key_part_map keypart_map = 0;
  bool cur_covered;
  bool prev_covered =
      bitmap_is_set(&info->covered_fields, key_part->fieldnr - 1);
  key_range min_range;
  key_range max_range;
  min_range.key = key_val;
  min_range.flag = HA_READ_KEY_EXACT;
  max_range.key = key_val;
  max_range.flag = HA_READ_AFTER_KEY;
  ha_rows prev_records = table->file->stats.records;
  DBUG_TRACE;

  for (SEL_ROOT *sel_root = scan->sel_root; sel_root;
       sel_root = sel_root->root->next_key_part) {
    DBUG_PRINT("info", ("sel_root step"));
    cur_covered = bitmap_is_set(&info->covered_fields,
                                key_part[sel_root->root->part].fieldnr - 1);
    if (cur_covered != prev_covered) {
      /* create (part1val, ..., part{n-1}val) tuple. */
      bool is_null_range = false;
      ha_rows records;
      if (!tuple_arg) {
        tuple_arg = scan->sel_root->root;
        /* Here we use the length of the first key part */
        tuple_arg->store_min_value(key_part[0].store_length, &key_ptr, 0);
        is_null_range |= tuple_arg->is_null_interval();
        keypart_map = 1;
      }
      while (tuple_arg->next_key_part != sel_root) {
        tuple_arg = tuple_arg->next_key_part->root;
        tuple_arg->store_min_value(key_part[tuple_arg->part].store_length,
                                   &key_ptr, 0);
        is_null_range |= tuple_arg->is_null_interval();
        keypart_map = (keypart_map << 1) | 1;
      }
      min_range.length = max_range.length = (size_t)(key_ptr - key_val);
      min_range.keypart_map = max_range.keypart_map = keypart_map;

      /*
        Get the number of rows in this range. This is done by calling
        records_in_range() unless all these are true:
          1) The user has requested that index statistics should be used
             for equality ranges to avoid the incurred overhead of
             index dives in records_in_range()
          2) The range is not on the form "x IS NULL". The reason is
             that the number of rows with this value are likely to be
             very different than the values in the index statistics
          3) Index statistics is available.
        @see key_val
      */
      if (!info->param->use_index_statistics ||  // (1)
          is_null_range ||                       // (2)
          !table->key_info[scan->keynr].has_records_per_key(
              tuple_arg->part))  // (3)
      {
        DBUG_EXECUTE_IF("crash_records_in_range", DBUG_SUICIDE(););
        assert(min_range.length > 0);
        assert(
            !table->pos_in_table_list->is_derived_unfinished_materialization());
        records =
            table->file->records_in_range(scan->keynr, &min_range, &max_range);
      } else {
        // Use index statistics
        records = static_cast<ha_rows>(
            table->key_info[scan->keynr].records_per_key(tuple_arg->part));
      }

      if (cur_covered) {
        /* uncovered -> covered */
        double tmp = rows2double(records) / rows2double(prev_records);
        DBUG_PRINT("info", ("Selectivity multiplier: %g", tmp));
        selectivity_mult *= tmp;
        prev_records = HA_POS_ERROR;
      } else {
        /* covered -> uncovered */
        prev_records = records;
      }
    }
    prev_covered = cur_covered;
  }
  if (!prev_covered) {
    double tmp =
        rows2double(table->quick_rows[scan->keynr]) / rows2double(prev_records);
    DBUG_PRINT("info", ("Selectivity multiplier: %g", tmp));
    selectivity_mult *= tmp;
  }
  // Todo: This assert fires in PB sysqa RQG tests.
  // assert(selectivity_mult <= 1.0);
  DBUG_PRINT("info", ("Returning multiplier: %g", selectivity_mult));
  return selectivity_mult;
}

/*
  Check if adding a ROR scan to a ROR-intersection reduces its cost of
  ROR-intersection and if yes, update parameters of ROR-intersection,
  including its cost.

  SYNOPSIS
    ror_intersect_add()
      info         ROR-intersection structure to add the scan to.
      needed_fields  Bitmask of fields needed by the query.
      ror_scan     ROR scan info to add.
      is_cpk_scan  If true, add the scan as CPK scan (this can be inferred
                   from other parameters and is passed separately only to
                   avoid duplicating the inference code)
      trace_costs  Optimizer trace object cost details are added to
      ignore_cost  Ignore cost check due to use of INDEX_MERGE hint

  NOTES
    Adding a ROR scan to ROR-intersect "makes sense" iff the cost of ROR-
    intersection decreases. The cost of ROR-intersection is calculated as
    follows:

    cost= SUM_i(key_scan_cost_i) + cost_of_full_rows_retrieval

    When we add a scan the first increases and the second decreases.

    cost_of_full_rows_retrieval=
      (union of indexes used covers all needed fields) ?
        cost_of_sweep_read(E(rows_to_retrieve), rows_in_table) :
        0

    E(rows_to_retrieve) = #rows_in_table * ror_scan_selectivity(null, scan1) *
                           ror_scan_selectivity({scan1}, scan2) * ... *
                           ror_scan_selectivity({scan1,...}, scanN).
  RETURN
    true   ROR scan added to ROR-intersection, cost updated.
    false  It doesn't make sense to add this ROR scan to this ROR-intersection.
*/

static bool ror_intersect_add(ROR_INTERSECT_INFO *info,
                              const MY_BITMAP *needed_fields,
                              ROR_SCAN_INFO *ror_scan, bool is_cpk_scan,
                              Opt_trace_object *trace_costs, bool ignore_cost) {
  double selectivity_mult = 1.0;

  DBUG_TRACE;
  DBUG_PRINT("info", ("Current out_rows= %g", info->out_rows));
  DBUG_PRINT("info", ("Adding scan on %s",
                      info->param->table->key_info[ror_scan->keynr].name));
  DBUG_PRINT("info", ("is_cpk_scan: %d", is_cpk_scan));

  selectivity_mult = ror_scan_selectivity(info, ror_scan);
  if (selectivity_mult == 1.0 && !ignore_cost) {
    /* Don't add this scan if it doesn't improve selectivity. */
    DBUG_PRINT("info", ("The scan doesn't improve selectivity."));
    return false;
  }

  info->out_rows *= selectivity_mult;

  if (is_cpk_scan) {
    /*
      CPK scan is used to filter out rows. We apply filtering for each
      record of every scan. For each record we assume that one key
      compare is done:
    */
    const Cost_model_table *const cost_model = info->param->table->cost_model();
    const double idx_cost =
        cost_model->key_compare_cost(rows2double(info->index_records));
    info->index_scan_cost.add_cpu(idx_cost);
    trace_costs->add("index_scan_cost", idx_cost);
  } else {
    info->index_records += info->param->table->quick_rows[ror_scan->keynr];
    info->index_scan_cost += ror_scan->index_read_cost;
    trace_costs->add("index_scan_cost", ror_scan->index_read_cost);
    bitmap_union(&info->covered_fields, &ror_scan->covered_fields);
    if (!info->is_covering &&
        bitmap_is_subset(needed_fields, &info->covered_fields)) {
      DBUG_PRINT("info", ("ROR-intersect is covering now"));
      info->is_covering = true;
    }
  }

  info->total_cost = info->index_scan_cost;
  trace_costs->add("cumulated_index_scan_cost", info->index_scan_cost);

  if (!info->is_covering) {
    Cost_estimate sweep_cost;
    JOIN *join = info->param->query_block->join;
    const bool is_interrupted = join && join->tables != 1;

    get_sweep_read_cost(info->param->table, double2rows(info->out_rows),
                        is_interrupted, &sweep_cost);
    info->total_cost += sweep_cost;
    trace_costs->add("disk_sweep_cost", sweep_cost);
  } else
    trace_costs->add("disk_sweep_cost", 0);

  DBUG_PRINT("info", ("New out_rows: %g", info->out_rows));
  DBUG_PRINT("info", ("New cost: %g, %scovering", info->total_cost.total_cost(),
                      info->is_covering ? "" : "non-"));
  return true;
}

static AccessPath *MakeAccessPath(ROR_SCAN_INFO *scan, TABLE *table,
                                  KEY_PART *used_key_part, bool reuse_handler,
                                  MEM_ROOT *mem_root) {
  AccessPath *path = new (mem_root) AccessPath;
  path->type = AccessPath::INDEX_RANGE_SCAN;

  // TODO(sgunders): The initial cost is high (it needs to read all rows and
  // sort), so we should not have zero init_cost.
  path->cost = scan->index_read_cost.total_cost();
  path->set_num_output_rows(scan->records);

  path->index_range_scan().used_key_part = used_key_part;
  path->index_range_scan().ranges = &scan->ranges[0];
  path->index_range_scan().num_ranges = scan->ranges.size();
  path->index_range_scan().mrr_flags = HA_MRR_SORTED;
  path->index_range_scan().mrr_buf_size = 0;
  path->index_range_scan().index = scan->keynr;
  path->index_range_scan().num_used_key_parts = scan->used_key_parts;
  path->index_range_scan().can_be_used_for_ror = true;
  path->index_range_scan().need_rows_in_rowid_order = true;
  path->index_range_scan().can_be_used_for_imerge = false;  // Irrelevant.
  path->index_range_scan().reuse_handler = reuse_handler;
  path->index_range_scan().geometry =
      Overlaps(table->key_info[scan->keynr].flags, HA_SPATIAL);
  path->index_range_scan().reverse = false;
  return path;
}

/*
  Get best ROR-intersection plan using non-covering ROR-intersection search
  algorithm. The returned plan may be covering.

  SYNOPSIS
    get_best_ror_intersect()
      param            Parameter from test_quick_select function.
      tree             Transformed restriction condition to be used to look
                       for ROR scans.
      cost_est         Do not return read plans with cost > cost_est.
      are_all_covering [out] set to true if union of all scans covers all
                       fields needed by the query (and it is possible to build
                       a covering ROR-intersection)
      force_index_merge_result true if the function must return cheapest
                               intersection object when INDEX_MERGE hint is
                               used without specified indexes, false otherwise.

  NOTES
    get_key_scans_params must be called before this function can be called.

    When this function is called by ROR-union construction algorithm it
    assumes it is building an uncovered ROR-intersection (and thus # of full
    records to be retrieved is wrong here). This is a hack.

  IMPLEMENTATION
    The approximate best non-covering plan search algorithm is as follows:

    find_min_ror_intersection_scan()
    {
      R= select all ROR scans;
      order R by (E(#records_matched) * key_record_length).

      S= first(R); -- set of scans that will be used for ROR-intersection
      R= R-first(S);
      min_cost= cost(S);
      min_scan= make_scan(S);
      while (R is not empty)
      {
        firstR= R - first(R);
        if (!selectivity(S + firstR < selectivity(S)))
          continue;

        S= S + first(R);
        if (cost(S) < min_cost)
        {
          min_cost= cost(S);
          min_scan= make_scan(S);
        }
      }
      return min_scan;
    }

    See ror_intersect_add function for ROR intersection costs.

    Special handling for Clustered PK scans
    Clustered PK contains all table fields, so using it as a regular scan in
    index intersection doesn't make sense: a range scan on CPK will be less
    expensive in this case.
    Clustered PK scan has special handling in ROR-intersection: it is not used
    to retrieve rows, instead its condition is used to filter row references
    we get from scans on other keys.

  RETURN
    ROR-intersection table read plan
    NULL if out of memory or no suitable plan found.
*/

AccessPath *get_best_ror_intersect(
    THD *thd, const RANGE_OPT_PARAM *param, TABLE *table,
    bool index_merge_intersect_allowed, SEL_TREE *tree,
    const MY_BITMAP *needed_fields, double cost_est,
    bool force_index_merge_result, bool reuse_handler) {
  uint idx;
  Cost_estimate min_cost;
  Opt_trace_context *const trace = &thd->opt_trace;
  DBUG_TRACE;

  bool use_cheapest_index_merge = false;
  bool force_index_merge =
      idx_merge_hint_state(thd, table, &use_cheapest_index_merge);

  Opt_trace_object trace_ror(trace, "analyzing_roworder_intersect");

  min_cost.set_max_cost();

  if (tree->n_ror_scans < 2 ||
      ((!table->file->stats.records || !index_merge_intersect_allowed) &&
       !force_index_merge)) {
    trace_ror.add("usable", false);
    if (tree->n_ror_scans < 2)
      trace_ror.add_alnum("cause", "too_few_roworder_scans");
    else
      trace_ror.add("need_tracing", true);
    return nullptr;
  }

  /*
    Step1: Collect ROR-able SEL_ARGs and create ROR_SCAN_INFO for each of
    them. Also find and save clustered PK scan if there is one.
  */
  ROR_SCAN_INFO **cur_ror_scan;
  ROR_SCAN_INFO *cpk_scan = nullptr;
  uint cpk_no;
  bool cpk_scan_used = false;

  if (!(tree->ror_scans =
            param->temp_mem_root->ArrayAlloc<ROR_SCAN_INFO *>(param->keys)))
    return nullptr;
  cpk_no = ((table->file->primary_key_is_clustered()) ? table->s->primary_key
                                                      : MAX_KEY);

  for (idx = 0, cur_ror_scan = tree->ror_scans; idx < param->keys; idx++) {
    ROR_SCAN_INFO *scan;
    if (!tree->ror_scans_map.is_set(idx)) continue;
    if (!(scan = make_ror_scan(param, idx, tree->keys[idx], needed_fields)))
      return nullptr;
    if (param->real_keynr[idx] == cpk_no) {
      cpk_scan = scan;
      tree->n_ror_scans--;
    } else
      *(cur_ror_scan++) = scan;
  }

  tree->ror_scans_end = cur_ror_scan;
  DBUG_EXECUTE("info", print_ror_scans_arr(table, "original", tree->ror_scans,
                                           tree->ror_scans_end););
  /*
    Ok, [ror_scans, ror_scans_end) is array of ptrs to initialized
    ROR_SCAN_INFO's.
    Step 2: Get best ROR-intersection using an approximate algorithm.
  */
  find_intersect_order(tree->ror_scans, tree->ror_scans_end, param,
                       needed_fields);

  DBUG_EXECUTE("info", print_ror_scans_arr(table, "ordered", tree->ror_scans,
                                           tree->ror_scans_end););

  ROR_SCAN_INFO **intersect_scans; /* ROR scans used in index intersection */
  ROR_SCAN_INFO **intersect_scans_end;
  if (!(intersect_scans = param->return_mem_root->ArrayAlloc<ROR_SCAN_INFO *>(
            tree->n_ror_scans)))
    return nullptr;
  intersect_scans_end = intersect_scans;

  /* Create and incrementally update ROR intersection. */
  ROR_INTERSECT_INFO *intersect, *intersect_best = nullptr;
  if (!(intersect = ror_intersect_init(param)) ||
      !(intersect_best = ror_intersect_init(param)))
    return nullptr;

  /* [intersect_scans,intersect_scans_best) will hold the best intersection */
  ROR_SCAN_INFO **intersect_scans_best;
  cur_ror_scan = tree->ror_scans;
  intersect_scans_best = intersect_scans;
  /*
    Note: trace_isect_idx.end() is called to close this object after
    this while-loop.
  */
  Opt_trace_array trace_isect_idx(trace, "intersecting_indexes");
  while (cur_ror_scan != tree->ror_scans_end && !intersect->is_covering) {
    Opt_trace_object trace_idx(trace);
    trace_idx.add_utf8("index", table->key_info[(*cur_ror_scan)->keynr].name);

    if (!compound_hint_key_enabled(table, (*cur_ror_scan)->keynr,
                                   INDEX_MERGE_HINT_ENUM)) {
      trace_idx.add("usable", false).add_alnum("cause", "index_merge_hint");
      cur_ror_scan++;
      continue;
    }

    /* S= S + first(R);  R= R - first(R); */
    if (!ror_intersect_add(intersect, needed_fields, *cur_ror_scan, false,
                           &trace_idx,
                           force_index_merge && !use_cheapest_index_merge)) {
      trace_idx.add("cumulated_total_cost", intersect->total_cost)
          .add("usable", false)
          .add_alnum("cause", "does_not_reduce_cost_of_intersect");
      cur_ror_scan++;
      continue;
    }

    trace_idx.add("cumulated_total_cost", intersect->total_cost)
        .add("usable", true)
        .add("matching_rows_now", intersect->out_rows)
        .add("isect_covering_with_this_index", intersect->is_covering);

    *(intersect_scans_end++) = *(cur_ror_scan++);

    if (intersect->total_cost < min_cost ||
        (force_index_merge &&
         /*
           If INDEX_MERGE hint is used without only specified index,
           index merge is forced and the cheapest combination of indexes
           will be chosen. Since ranges are sorted by index scan cost,
           index merge is forced for first two ranges and next ranges are
           added only if they reduce total cost and there is no clustered
           primary key scan or intersection is covering. If there is
           a range by clustered primary key and intersection is not covering,
           combination of first index and primary key is considered as
           a cheapest intersection.
         */
         ((intersect_scans_best - intersect_scans < 2 &&
           force_index_merge_result && (!cpk_scan || intersect->is_covering)) ||
          !use_cheapest_index_merge))) {
      /* Local minimum found, save it */
      ror_intersect_cpy(intersect_best, intersect);
      intersect_scans_best = intersect_scans_end;
      min_cost = intersect->total_cost;
      trace_idx.add("chosen", true);
    } else {
      trace_idx.add("chosen", false).add_alnum("cause", "does_not_reduce_cost");
    }
  }
  // Note: trace_isect_idx trace object is closed here
  trace_isect_idx.end();

  if (intersect_scans_best == intersect_scans) {
    trace_ror.add("chosen", false)
        .add_alnum("cause", "does_not_increase_selectivity");
    DBUG_PRINT("info", ("None of scans increase selectivity"));
    return nullptr;
  }

  DBUG_EXECUTE("info",
               print_ror_scans_arr(table, "best ROR-intersection",
                                   intersect_scans, intersect_scans_best););

  uint best_num = intersect_scans_best - intersect_scans;
  ror_intersect_cpy(intersect, intersect_best);

  /*
    Ok, found the best ROR-intersection of non-CPK key scans.
    Check if we should add a CPK scan. If the obtained ROR-intersection is
    covering, it doesn't make sense to add CPK scan.
  */
  {  // Scope for trace object
    Opt_trace_object trace_cpk(trace, "clustered_pk");
    if (cpk_scan && !intersect->is_covering &&
        compound_hint_key_enabled(table, cpk_no, INDEX_MERGE_HINT_ENUM)) {
      if (ror_intersect_add(intersect, needed_fields, cpk_scan, true,
                            &trace_cpk, true) &&
          ((intersect->total_cost < min_cost) ||
           (force_index_merge &&
            (!use_cheapest_index_merge ||
             (best_num == 1 && force_index_merge_result))))) {
        trace_cpk.add("clustered_pk_scan_added_to_intersect", true)
            .add("cumulated_cost", intersect->total_cost);
        cpk_scan_used = true;
        intersect_best = intersect;  // just set pointer here
      } else
        trace_cpk.add("clustered_pk_added_to_intersect", false)
            .add_alnum("cause", "cost");
    } else {
      trace_cpk.add("clustered_pk_added_to_intersect", false)
          .add_alnum("cause", cpk_scan ? "roworder_is_covering"
                                       : "no_clustered_pk_index");
    }
  }
  /* Ok, return ROR-intersect plan if we have found one */
  if ((min_cost.total_cost() < cost_est || force_index_merge) &&
      (cpk_scan_used || best_num > 1)) {
    // Create AccessPaths from the ROR child scans.
    auto *children = new (param->return_mem_root)
        Mem_root_array<AccessPath *>(param->return_mem_root);
    children->resize(best_num);
    for (unsigned i = 0; i < best_num; ++i) {
      (*children)[i] = MakeAccessPath(intersect_scans[i], table,
                                      param->key[intersect_scans[i]->idx],
                                      /*reuse_handler=*/reuse_handler &&
                                          intersect_best->is_covering && i == 0,
                                      param->return_mem_root);
    }
    AccessPath *cpk_child =
        cpk_scan_used
            ? MakeAccessPath(cpk_scan, table, param->key[cpk_scan->idx],
                             /*reuse_handler=*/false, param->return_mem_root)
            : nullptr;

    AccessPath *path = new (param->return_mem_root) AccessPath;
    path->type = AccessPath::ROWID_INTERSECTION;
    path->cost = intersect_best->total_cost.total_cost();
    /* Prevent divisions by zero */
    double best_rows = max(intersect_best->out_rows, 1.0);
    table->quick_condition_rows =
        min<ha_rows>(table->quick_condition_rows, best_rows);
    path->set_num_output_rows(best_rows);

    path->rowid_intersection().table = table;
    path->rowid_intersection().children = children;
    path->rowid_intersection().cpk_child = cpk_child;
    path->rowid_intersection().forced_by_hint = force_index_merge;
    path->rowid_intersection().retrieve_full_rows =
        !intersect_best->is_covering;  // Can be overridden later.
    path->rowid_intersection().need_rows_in_rowid_order =
        false;  // Can be overridden later.
    path->rowid_intersection().reuse_handler = reuse_handler;
    path->rowid_intersection().is_covering = intersect_best->is_covering;

    trace_ror.add("rows", path->num_output_rows())
        .add("cost", path->cost)
        .add("covering", intersect_best->is_covering)
        .add("chosen", true);

    DBUG_PRINT("info", ("Returning non-covering ROR-intersect plan:"
                        "cost %g, records %g",
                        path->cost, path->num_output_rows()));
    return path;
  } else {
    trace_ror.add("chosen", false)
        .add_alnum("cause", (cost_est > min_cost.total_cost())
                                ? "too_few_indexes_to_merge"
                                : "cost");
    return nullptr;
  }
}

static int find_max_used_key_length(const AccessPath *scan) {
  int max_used_key_length = 0;
  for (const QUICK_RANGE *range :
       Bounds_checked_array{scan->index_range_scan().ranges,
                            scan->index_range_scan().num_ranges}) {
    max_used_key_length = std::max<int>(max_used_key_length, range->min_length);
    max_used_key_length = std::max<int>(max_used_key_length, range->max_length);
  }
  return max_used_key_length;
}

void add_keys_and_lengths_rowid_intersection(const AccessPath *path,
                                             String *key_names,
                                             String *used_lengths) {
  TABLE *table = path->rowid_intersection().table;

  char buf[64];
  size_t length;
  bool first = true;
  for (AccessPath *current : *path->rowid_intersection().children) {
    KEY *key_info = table->key_info + current->index_range_scan().index;
    if (first)
      first = false;
    else {
      key_names->append(',');
      used_lengths->append(',');
    }
    key_names->append(key_info->name);

    length =
        longlong10_to_str(find_max_used_key_length(current), buf, 10) - buf;
    used_lengths->append(buf, length);
  }

  AccessPath *cpk_child = path->rowid_intersection().cpk_child;
  if (cpk_child) {
    KEY *key_info = table->key_info + cpk_child->index_range_scan().index;
    key_names->append(',');
    key_names->append(key_info->name);
    length =
        longlong10_to_str(find_max_used_key_length(cpk_child), buf, 10) - buf;
    used_lengths->append(',');
    used_lengths->append(buf, length);
  }
}

void add_keys_and_lengths_rowid_union(const AccessPath *path, String *key_names,
                                      String *used_lengths) {
  bool first = true;
  for (AccessPath *current : *path->rowid_union().children) {
    if (first) {
      first = false;
    } else {
      used_lengths->append(',');
      key_names->append(',');
    }
    ::add_keys_and_lengths(current, key_names, used_lengths);
  }
}

#ifndef NDEBUG
void dbug_dump_rowid_intersection(
    int indent, bool verbose, const Mem_root_array<AccessPath *> &children) {
  fprintf(DBUG_FILE, "%*squick ROR-intersect select\n", indent, ""),
      fprintf(DBUG_FILE, "%*smerged scans {\n", indent, "");
  for (AccessPath *range_scan : children) {
    dbug_dump(range_scan, indent + 2, verbose);
  }
  fprintf(DBUG_FILE, "%*s}\n", indent, "");
}

void dbug_dump_rowid_union(int indent, bool verbose,
                           const Mem_root_array<AccessPath *> &children) {
  fprintf(DBUG_FILE, "%*squick ROR-union select\n", indent, "");
  fprintf(DBUG_FILE, "%*smerged scans {\n", indent, "");
  for (AccessPath *child : children) {
    ::dbug_dump(child, indent + 2, verbose);
  }
  fprintf(DBUG_FILE, "%*s}\n", indent, "");
}
#endif
