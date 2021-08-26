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

#ifndef SQL_RANGE_OPTIMIZER_ROWID_ORDERED_RETRIEVAL_PLAN_H_
#define SQL_RANGE_OPTIMIZER_ROWID_ORDERED_RETRIEVAL_PLAN_H_

#include <sys/types.h>

#include "my_base.h"
#include "my_bitmap.h"
#include "sql/handler.h"
#include "sql/range_optimizer/table_read_plan.h"

class Opt_trace_object;
class RANGE_OPT_PARAM;
class QUICK_SELECT_I;
class SEL_ROOT;
class SEL_TREE;
struct MEM_ROOT;

struct ROR_SCAN_INFO {
  uint idx;         ///< # of used key in param->keys
  uint keynr;       ///< # of used key in table
  ha_rows records;  ///< estimate of # records this scan will return

  /** Set of intervals over key fields that will be used for row retrieval. */
  SEL_ROOT *sel_root;

  /** Fields used in the query and covered by this ROR scan. */
  MY_BITMAP covered_fields;
  /**
    Fields used in the query that are a) covered by this ROR scan and
    b) not already covered by ROR scans ordered earlier in the merge
    sequence.
  */
  MY_BITMAP covered_fields_remaining;
  /** Number of fields in covered_fields_remaining (caching of
   * bitmap_bits_set()) */
  uint num_covered_fields_remaining;

  /**
    Cost of reading all index records with values in sel_arg intervals set
    (assuming there is no need to access full table records)
  */
  Cost_estimate index_read_cost;

  /**
    The ranges to scan for this index. Must be allocated on the return_mem_root.
   */
  Bounds_checked_array<QUICK_RANGE *> ranges;
  uint used_key_parts;
};

/* Plan for QUICK_ROR_INTERSECT_SELECT scan. */

class TRP_ROR_INTERSECT : public TABLE_READ_PLAN {
 public:
  // intersect_scans (both the pointers and the structs themselves)
  // must be allocated on return_mem_root, as it will be used
  // by make_quick(), which can be called after the range optimizer
  // has returned.
  TRP_ROR_INTERSECT(TABLE *table_arg, bool forced_by_hint_arg,
                    KEY_PART *const *key_arg, const uint *real_keynr_arg,
                    Bounds_checked_array<ROR_SCAN_INFO *> intersect_scans_arg,
                    Cost_estimate index_scan_cost_arg, bool is_covering_arg,
                    ROR_SCAN_INFO *cpk_scan_arg)
      : TABLE_READ_PLAN(table_arg, MAX_KEY, /*used_key_parts=*/0,
                        forced_by_hint_arg),
        intersect_scans(intersect_scans_arg),
        cpk_scan(cpk_scan_arg),
        is_covering(is_covering_arg),
        index_scan_cost(index_scan_cost_arg),
        key(key_arg),
        real_keynr(real_keynr_arg) {}

  QUICK_SELECT_I *make_quick(THD *thd, double expected_rows,
                             bool retrieve_full_rows, MEM_ROOT *return_mem_root,
                             ha_rows *examined_rows) override;
  void trace_basic_info(THD *thd, const RANGE_OPT_PARAM *param,
                        Opt_trace_object *trace_object) const override;

  Cost_estimate get_index_scan_cost() const { return index_scan_cost; }
  bool get_is_covering() const { return is_covering; }

  RangeScanType get_type() const override { return QS_TYPE_ROR_INTERSECT; }
  bool is_keys_used(const MY_BITMAP *fields) override;
  void need_sorted_output() override { assert(false); /* Can't do it */ }
  void get_fields_used(MY_BITMAP *used_fields) const override;
  void add_info_string(String *str) const override;
  void add_keys_and_lengths(String *key_names,
                            String *used_lengths) const override;
  unsigned get_max_used_key_length() const final;
#ifndef NDEBUG
  void dbug_dump(int indent, bool verbose) override;
#endif

  // If true, the first child scan should reuse table->file instead of
  // creating its own. This is true if the intersection is the topmost
  // range scan, but _not_ if it's below a union. (The reasons for this
  // are unknown.) It can also be negated by logic involving
  // retrieve_full_rows and is_covering, again for unknown reasons.
  //
  // This is not only for performance; multi-table delete has a hidden
  // dependency on this behavior when running against certain types of
  // tables (e.g. MyISAM), as it assumes table->file is correctly positioned
  // when deleting (and not all table types can transfer the position of one
  // handler to another by using position()).
  bool reuse_handler = false;

 private:
  /* ROR range scans used in this intersection */
  Bounds_checked_array<ROR_SCAN_INFO *> intersect_scans;
  ROR_SCAN_INFO *cpk_scan; /* Clustered PK scan, if there is one */
  const bool is_covering;  /* true if no row retrieval phase is necessary */
  const Cost_estimate index_scan_cost; /* SUM(cost(index_scan)) */

  KEY_PART *const *key;
  const uint *real_keynr;
};

/*
  Plan for QUICK_ROR_UNION_SELECT scan.
  QUICK_ROR_UNION_SELECT always retrieves full rows, so retrieve_full_rows
  is ignored by make_quick.
*/

class TRP_ROR_UNION : public TABLE_READ_PLAN {
 public:
  TRP_ROR_UNION(TABLE *table_arg, bool forced_by_hint_arg,
                Bounds_checked_array<TABLE_READ_PLAN *> ror_scans_arg,
                const Cost_estimate &cost_est_arg, ha_rows records_arg)
      : TABLE_READ_PLAN(table_arg, MAX_KEY, /*used_key_parts=*/0,
                        forced_by_hint_arg),
        ror_scans(ror_scans_arg) {
    cost_est = cost_est_arg;
    records = records_arg;

    for (TABLE_READ_PLAN *child : ror_scans_arg) {
      child->need_rows_in_rowid_order = true;
    }
  }
  QUICK_SELECT_I *make_quick(THD *thd, double expected_rows,
                             bool retrieve_full_rows, MEM_ROOT *return_mem_root,
                             ha_rows *examined_rows) override;

  void trace_basic_info(THD *thd, const RANGE_OPT_PARAM *param,
                        Opt_trace_object *trace_object) const override;

  RangeScanType get_type() const override { return QS_TYPE_ROR_UNION; }
  bool is_keys_used(const MY_BITMAP *fields) override;
  void need_sorted_output() override { assert(false); /* Can't do it */ }
  void get_fields_used(MY_BITMAP *used_fields) const override;
  void add_info_string(String *str) const override;
  void add_keys_and_lengths(String *key_names,
                            String *used_lengths) const override;
  unsigned get_max_used_key_length() const final;
#ifndef NDEBUG
  void dbug_dump(int indent, bool verbose) override;
#endif

 private:
  // The subplans for merged scans. Can be TRP_RANGE or TRP_ROR_INTERSECT.
  Bounds_checked_array<TABLE_READ_PLAN *> ror_scans;
};

TRP_ROR_INTERSECT *get_best_ror_intersect(
    THD *thd, const RANGE_OPT_PARAM *param, TABLE *table,
    bool index_merge_intersect_allowed, enum_order order_direction,
    SEL_TREE *tree, const MY_BITMAP *needed_fields,
    const Cost_estimate *cost_est, bool force_index_merge_result);

#endif  // SQL_RANGE_OPTIMIZER_ROWID_ORDERED_RETRIEVAL_PLAN_H_
