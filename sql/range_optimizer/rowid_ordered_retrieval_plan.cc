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

#include "sql/range_optimizer/rowid_ordered_retrieval_plan.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <utility>

#include "m_string.h"
#include "my_alloc.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql/strings/m_ctype.h"
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
static void print_ror_scans(TABLE *table, const char *msg,
                            const Mem_root_array<ROR_SCAN_INFO *> &ror_scans) {
  DBUG_TRACE;

  StringBuffer<1024> tmp;
  for (ROR_SCAN_INFO *scan : ror_scans) {
    if (!tmp.is_empty()) tmp.append(',');
    tmp.append(table->key_info[scan->keynr].name);
  }
  if (tmp.is_empty()) tmp.append(STRING_WITH_LEN("(empty)"));
  DBUG_PRINT("info", ("ROR key scans (%s): %s", msg, tmp.ptr()));
  fprintf(DBUG_FILE, "ROR key scans (%s): %s", msg, tmp.ptr());
}
#endif

/*
  Get the needed fields used in the query.
  NOTES
  Clustered PK members are not put into the bitmap as they are implicitly
  present in all keys (and it is impossible to avoid reading them).
*/

OverflowBitset get_needed_fields(const RANGE_OPT_PARAM *param) {
  TABLE *table = param->table;

  MutableOverflowBitset fields(param->temp_mem_root, table->s->fields);

  for (size_t i = bitmap_get_first_set(table->read_set); i != MY_BIT_NONE;
       i = bitmap_get_next_set(table->read_set, i)) {
    fields.SetBit(i);
  }
  for (size_t i = bitmap_get_first_set(table->write_set); i != MY_BIT_NONE;
       i = bitmap_get_next_set(table->write_set, i)) {
    fields.SetBit(i);
  }

  uint pk = table->s->primary_key;
  if (pk != MAX_KEY && table->file->primary_key_is_clustered()) {
    /* The table uses clustered PK and it is not internally generated */
    KEY_PART_INFO *key_part = table->key_info[pk].key_part;
    KEY_PART_INFO *key_part_end =
        key_part + table->key_info[pk].user_defined_key_parts;
    for (; key_part != key_part_end; ++key_part)
      fields.ClearBit(key_part->fieldnr - 1);
  }
  return fields;
}

void trace_basic_info_rowid_intersection(THD *thd, const AccessPath *path,
                                         const RANGE_OPT_PARAM *param,
                                         Opt_trace_object *trace_object) {
  trace_object->add_alnum("type", "index_roworder_intersect")
      .add("rows", path->num_output_rows())
      .add("cost", path->cost())
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

ROR_SCAN_INFO *make_ror_scan(const RANGE_OPT_PARAM *param, int idx,
                             SEL_ROOT *sel_root, OverflowBitset needed_fields) {
  ROR_SCAN_INFO *ror_scan;
  uint keynr;
  DBUG_TRACE;

  if (!(ror_scan = new (param->return_mem_root) ROR_SCAN_INFO)) return nullptr;

  ror_scan->idx = idx;
  ror_scan->keynr = keynr = param->real_keynr[idx];
  ror_scan->sel_root = sel_root;
  ror_scan->records = param->table->quick_rows[keynr];

  KEY_PART_INFO *key_part = param->table->key_info[keynr].key_part;
  KEY_PART_INFO *key_part_end =
      key_part + param->table->key_info[keynr].user_defined_key_parts;
  MutableOverflowBitset covered_fields(param->temp_mem_root,
                                       needed_fields.capacity());
  for (; key_part != key_part_end; ++key_part) {
    if (IsBitSet(key_part->fieldnr - 1, needed_fields))
      covered_fields.SetBit(key_part->fieldnr - 1);
  }
  ror_scan->covered_fields = std::move(covered_fields);
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
  Sort indexes in an order that is likely to be a good index merge
  intersection order. After running this function ror_scans are
  ordered according to this strategy:

    1) Minimize the number of indexes that must be used in the
       intersection. I.e., the index covering most fields not already
       covered by other indexes earlier in the sort order is picked first.
    2) When multiple indexes cover equally many uncovered fields, the
       index with lowest E(Number of rows) is chosen.

  Note that all permutations of index ordering are not tested, so this
  function may not find the optimal order.

  @param[in,out] ror_scans      ror scans to be used in index merge intersection
  @param         needed_fields  Bitmask of fields needed by the query.
  @param         mem_root       memory root to be used.
*/
void find_intersect_order(Mem_root_array<ROR_SCAN_INFO *> *ror_scans,
                          OverflowBitset needed_fields, MEM_ROOT *mem_root) {
  for (uint index = 0; index < ror_scans->size(); index++) {
    std::stable_sort(
        ror_scans->begin() + index, ror_scans->end(),
        [needed_fields](ROR_SCAN_INFO *a, ROR_SCAN_INFO *b) {
          /*
           Compare two ROR_SCAN_INFO* by
           1. Number of fields in this index that are not already
           covered by other indexes earlier in the intersect
           ordering: descending
           2. E(Number of records): ascending
          */
          auto fields_in_a = BitsSetInBoth(a->covered_fields, needed_fields);
          uint num_fields_a =
              std::distance(fields_in_a.begin(), fields_in_a.end());
          auto fields_in_b = BitsSetInBoth(b->covered_fields, needed_fields);
          uint num_fields_b =
              std::distance(fields_in_b.begin(), fields_in_b.end());
          if (num_fields_a < num_fields_b) return false;
          if (num_fields_a > num_fields_b) return true;
          return a->records < b->records;
        });
    MutableOverflowBitset fields_to_be_covered = needed_fields.Clone(mem_root);
    for (uint i : BitsSetIn((*ror_scans)[index]->covered_fields))
      fields_to_be_covered.ClearBit(i);
    needed_fields = std::move(fields_to_be_covered);
    if (needed_fields.empty()) break;
  }
}

ROR_intersect_plan::ROR_intersect_plan(const RANGE_OPT_PARAM *param,
                                       size_t num_fields)
    : m_param(param),
      m_ror_scans(param->return_mem_root, 0),
      m_out_rows(m_param->table->file->stats.records),
      m_covered_fields(
          MutableOverflowBitset(param->temp_mem_root, num_fields)) {}

ROR_intersect_plan &ROR_intersect_plan::operator=(
    const ROR_intersect_plan &plan) {
  m_param = plan.m_param;
  m_ror_scans.clear();
  for (ROR_SCAN_INFO *scan : plan.m_ror_scans) m_ror_scans.push_back(scan);
  m_is_covering = plan.m_is_covering;
  m_covered_fields = plan.m_covered_fields;
  m_out_rows = plan.m_out_rows;
  m_total_cost = plan.m_total_cost;
  m_index_records = plan.m_index_records;
  m_index_read_cost = plan.m_index_read_cost;
  return *this;
}

/*
  Get selectivity of adding a ROR scan to the ROR-intersection.

  SYNOPSIS
    get_scan_selectivity()
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

double ROR_intersect_plan::get_scan_selectivity(
    const ROR_SCAN_INFO *scan) const {
  double selectivity_mult = 1.0;
  const TABLE *const table = m_param->table;
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
  bool prev_covered = IsBitSet(key_part->fieldnr - 1, m_covered_fields);
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
    cur_covered =
        IsBitSet(key_part[sel_root->root->part].fieldnr - 1, m_covered_fields);
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
      if (!m_param->use_index_statistics ||  // (1)
          is_null_range ||                   // (2)
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
      add()
      needed_fields  Bitmask of fields needed by the query.
      ror_scan       ROR scan info to add.
      is_cpk_scan    If true, add the scan as CPK scan (this can be inferred
                     from other parameters and is passed separately only to
                     avoid duplicating the inference code)
      trace_costs    Optimizer trace object cost details are added to
      ignore_cost    Ignore cost check due to use of INDEX_MERGE hint

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

bool ROR_intersect_plan::add(OverflowBitset needed_fields,
                             ROR_SCAN_INFO *ror_scan, bool is_cpk_scan,
                             Opt_trace_object *trace_costs, bool ignore_cost) {
  double selectivity_mult = 1.0;

  DBUG_TRACE;
  DBUG_PRINT("info", ("Current out_rows= %g", m_out_rows));
  DBUG_PRINT("info", ("Adding scan on %s",
                      m_param->table->key_info[ror_scan->keynr].name));
  DBUG_PRINT("info", ("is_cpk_scan: %d", is_cpk_scan));

  selectivity_mult = get_scan_selectivity(ror_scan);
  if (selectivity_mult == 1.0 && !ignore_cost) {
    /* Don't add this scan if it doesn't improve selectivity. */
    DBUG_PRINT("info", ("The scan doesn't improve selectivity."));
    return false;
  }

  m_out_rows *= selectivity_mult;

  if (is_cpk_scan) {
    /*
      CPK scan is used to filter out rows. We apply filtering for each
      record of every scan. For each record we assume that one key
      compare is done:
    */
    const Cost_model_table *const cost_model = m_param->table->cost_model();
    const double idx_cost =
        cost_model->key_compare_cost(rows2double(m_index_records));
    m_index_read_cost.add_cpu(idx_cost);
    if (trace_costs != nullptr) trace_costs->add("index_scan_cost", idx_cost);
  } else {
    m_index_records += m_param->table->quick_rows[ror_scan->keynr];
    m_index_read_cost += ror_scan->index_read_cost;
    if (trace_costs != nullptr)
      trace_costs->add("index_scan_cost", ror_scan->index_read_cost);
    m_covered_fields = OverflowBitset::Or(
        m_param->temp_mem_root, m_covered_fields, ror_scan->covered_fields);
    if (!m_is_covering && IsSubset(needed_fields, m_covered_fields)) {
      DBUG_PRINT("info", ("ROR-intersect is covering now"));
      m_is_covering = true;
    }
    m_ror_scans.push_back(ror_scan);
  }

  m_total_cost = m_index_read_cost;
  if (trace_costs != nullptr)
    trace_costs->add("cumulated_index_scan_cost", m_index_read_cost);

  if (!m_is_covering) {
    Cost_estimate sweep_cost;
    JOIN *join = m_param->query_block->join;
    const bool is_interrupted = join && join->tables != 1;

    get_sweep_read_cost(m_param->table, double2rows(m_out_rows), is_interrupted,
                        &sweep_cost);
    m_total_cost += sweep_cost;
    if (trace_costs != nullptr) trace_costs->add("disk_sweep_cost", sweep_cost);
  } else if (trace_costs != nullptr)
    trace_costs->add("disk_sweep_cost", 0);

  DBUG_PRINT("info", ("New out_rows: %g", m_out_rows));
  DBUG_PRINT("info", ("New cost: %g, %scovering", m_total_cost.total_cost(),
                      m_is_covering ? "" : "non-"));
  return true;
}

AccessPath *MakeRowIdOrderedIndexScanAccessPath(ROR_SCAN_INFO *scan,
                                                TABLE *table,
                                                KEY_PART *used_key_part,
                                                bool reuse_handler,
                                                MEM_ROOT *mem_root) {
  AccessPath *path = new (mem_root) AccessPath;
  path->type = AccessPath::INDEX_RANGE_SCAN;

  // TODO(sgunders): The initial cost is high (it needs to read all rows and
  // sort), so we should not have zero init_cost.
  path->set_cost_before_filter(scan->index_read_cost.total_cost());
  path->set_cost(path->cost_before_filter());
  path->set_init_cost(0.0);
  path->set_num_output_rows(scan->records);
  path->num_output_rows_before_filter = path->num_output_rows();
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

    See add function for ROR intersection costs.

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

AccessPath *get_best_ror_intersect(THD *thd, const RANGE_OPT_PARAM *param,
                                   TABLE *table,
                                   bool index_merge_intersect_allowed,
                                   SEL_TREE *tree, double cost_est,
                                   bool force_index_merge_result,
                                   bool reuse_handler) {
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
  ROR_SCAN_INFO *cpk_scan = nullptr;
  uint cpk_no;
  bool cpk_scan_used = false;

  cpk_no = ((table->file->primary_key_is_clustered()) ? table->s->primary_key
                                                      : MAX_KEY);
  Mem_root_array<ROR_SCAN_INFO *> ror_scans(param->temp_mem_root);
  OverflowBitset needed_fields = get_needed_fields(param);
  for (idx = 0; idx < param->keys; idx++) {
    ROR_SCAN_INFO *scan;
    if (!tree->ror_scans_map.is_set(idx)) continue;
    if (!(scan = make_ror_scan(param, idx, tree->keys[idx], needed_fields)))
      return nullptr;
    if (param->real_keynr[idx] == cpk_no) {
      cpk_scan = scan;
      tree->n_ror_scans--;
    } else {
      ror_scans.push_back(scan);
    }
  }

  DBUG_EXECUTE("info", print_ror_scans(table, "original", ror_scans););

  /*
    Get best ROR-intersection using an approximate algorithm.
  */
  find_intersect_order(&ror_scans, needed_fields, param->temp_mem_root);

  DBUG_EXECUTE("info", print_ror_scans(table, "ordered", ror_scans););
  /*
    Note: trace_isect_idx.end() is called to close this object after
    this while-loop.
  */
  Opt_trace_array trace_isect_idx(trace, "intersecting_indexes");
  ROR_intersect_plan cur_plan(param, needed_fields.capacity()),
      best_plan(param, needed_fields.capacity());

  for (uint index = 0; index < ror_scans.size() && !cur_plan.m_is_covering;
       index++) {
    ROR_SCAN_INFO *cur_scan = ror_scans[index];
    Opt_trace_object trace_idx(trace);
    trace_idx.add_utf8("index", table->key_info[cur_scan->keynr].name);

    if (!compound_hint_key_enabled(table, cur_scan->keynr,
                                   INDEX_MERGE_HINT_ENUM)) {
      trace_idx.add("usable", false).add_alnum("cause", "index_merge_hint");
      continue;
    }

    /* S= S + first(R);  R= R - first(R); */
    if (!cur_plan.add(needed_fields, cur_scan, false, &trace_idx,
                      force_index_merge && !use_cheapest_index_merge)) {
      trace_idx.add("cumulated_total_cost", cur_plan.m_total_cost)
          .add("usable", false)
          .add_alnum("cause", "does_not_reduce_cost_of_intersect");
      continue;
    }

    trace_idx.add("cumulated_total_cost", cur_plan.m_total_cost)
        .add("usable", true)
        .add("matching_rows_now", cur_plan.m_out_rows)
        .add("isect_covering_with_this_index", cur_plan.m_is_covering);

    if (cur_plan.m_total_cost < min_cost ||
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
         ((best_plan.num_scans() < 2 && force_index_merge_result &&
           (!cpk_scan || cur_plan.m_is_covering)) ||
          !use_cheapest_index_merge))) {
      /* Local minimum found, save it */
      best_plan = cur_plan;
      min_cost = cur_plan.m_total_cost;
      trace_idx.add("chosen", true);
    } else {
      trace_idx.add("chosen", false).add_alnum("cause", "does_not_reduce_cost");
    }
  }
  // Note: trace_isect_idx trace object is closed here
  trace_isect_idx.end();

  uint num_scans = best_plan.num_scans();
  if (num_scans == 0) {
    trace_ror.add("chosen", false)
        .add_alnum("cause", "does_not_increase_selectivity");
    DBUG_PRINT("info", ("None of scans increase selectivity"));
    return nullptr;
  }

  DBUG_EXECUTE("info", print_ror_scans(table, "best ROR-intersection",
                                       cur_plan.m_ror_scans););

  cur_plan = best_plan;
  /*
    Ok, found the best ROR-intersection of non-CPK key scans.
    Check if we should add a CPK scan. If the obtained ROR-intersection is
    covering, it doesn't make sense to add CPK scan.
  */
  {  // Scope for trace object
    Opt_trace_object trace_cpk(trace, "clustered_pk");
    if (cpk_scan && !cur_plan.m_is_covering &&
        compound_hint_key_enabled(table, cpk_no, INDEX_MERGE_HINT_ENUM)) {
      if (cur_plan.add(needed_fields, cpk_scan, true, &trace_cpk, true) &&
          ((cur_plan.m_total_cost < min_cost) ||
           (force_index_merge &&
            (!use_cheapest_index_merge ||
             (num_scans == 1 && force_index_merge_result))))) {
        trace_cpk.add("clustered_pk_scan_added_to_intersect", true)
            .add("cumulated_cost", cur_plan.m_total_cost);
        cpk_scan_used = true;
        best_plan = cur_plan;
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
      (cpk_scan_used || num_scans > 1)) {
    // Create AccessPaths from the ROR child scans.
    auto *children = new (param->return_mem_root)
        Mem_root_array<AccessPath *>(param->return_mem_root);
    children->resize(num_scans);
    for (unsigned i = 0; i < num_scans; ++i) {
      (*children)[i] = MakeRowIdOrderedIndexScanAccessPath(
          best_plan.m_ror_scans[i], table,
          param->key[best_plan.m_ror_scans[i]->idx],
          /*reuse_handler=*/reuse_handler && best_plan.m_is_covering && i == 0,
          param->return_mem_root);
    }
    AccessPath *cpk_child =
        cpk_scan_used ? MakeRowIdOrderedIndexScanAccessPath(
                            cpk_scan, table, param->key[cpk_scan->idx],
                            /*reuse_handler=*/false, param->return_mem_root)
                      : nullptr;

    AccessPath *path = new (param->return_mem_root) AccessPath;
    path->type = AccessPath::ROWID_INTERSECTION;
    path->set_cost(best_plan.m_total_cost.total_cost());
    /* Prevent divisons by zero */
    double best_rows = max(best_plan.m_out_rows, 1.0);
    table->quick_condition_rows =
        min<ha_rows>(table->quick_condition_rows, best_rows);
    path->set_num_output_rows(best_rows);

    path->rowid_intersection().table = table;
    path->rowid_intersection().children = children;
    path->rowid_intersection().cpk_child = cpk_child;
    path->rowid_intersection().forced_by_hint = force_index_merge;
    path->rowid_intersection().retrieve_full_rows =
        !best_plan.m_is_covering;  // Can be overridden later.
    path->rowid_intersection().need_rows_in_rowid_order =
        false;  // Can be overridden later.
    path->rowid_intersection().reuse_handler = reuse_handler;
    path->rowid_intersection().is_covering = best_plan.m_is_covering;

    trace_ror.add("rows", path->num_output_rows())
        .add("cost", path->cost())
        .add("covering", best_plan.m_is_covering)
        .add("chosen", true);

    DBUG_PRINT("info", ("Returning non-covering ROR-intersect plan:"
                        "cost %g, records %g",
                        path->cost(), path->num_output_rows()));
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
