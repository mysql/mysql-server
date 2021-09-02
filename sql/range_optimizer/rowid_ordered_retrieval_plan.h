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
class SEL_ROOT;
class SEL_TREE;
struct MEM_ROOT;

// TODO(sgunders): Consider whether we can create INDEX_RANGE_SCAN AccessPaths
// directly, instead of first creating this structure and then creating
// AccessPaths from it.
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

/*
  Plan for QUICK_ROR_UNION_SELECT scan.
*/

class TRP_ROR_UNION : public TABLE_READ_PLAN {
 public:
  TRP_ROR_UNION(TABLE *table_arg, bool forced_by_hint_arg,
                Bounds_checked_array<AccessPath *> ror_scans_arg)
      : TABLE_READ_PLAN(table_arg, MAX_KEY, /*used_key_parts=*/0,
                        forced_by_hint_arg),
        ror_scans(ror_scans_arg) {}
  RowIterator *make_quick(THD *thd, double expected_rows,
                          MEM_ROOT *return_mem_root,
                          ha_rows *examined_rows) override;

  void trace_basic_info(THD *thd, const RANGE_OPT_PARAM *param, double cost,
                        double num_output_rows,
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
  // The subplans for merged scans. Can be INDEX_RANGE_SCAN or
  // TRP_ROR_INTERSECT.
  Bounds_checked_array<AccessPath *> ror_scans;
};

AccessPath *get_best_ror_intersect(
    THD *thd, const RANGE_OPT_PARAM *param, TABLE *table,
    bool index_merge_intersect_allowed, enum_order order_direction,
    SEL_TREE *tree, const MY_BITMAP *needed_fields, double cost_est,
    bool force_index_merge_result, bool reuse_handler);

void trace_basic_info_rowid_intersection(THD *thd, const AccessPath *path,
                                         const RANGE_OPT_PARAM *param,
                                         Opt_trace_object *trace_object);

void add_keys_and_lengths_rowid_intersection(const AccessPath *path,
                                             String *key_names,
                                             String *used_lengths);

#ifndef NDEBUG
void dbug_dump_rowid_intersection(int indent, bool verbose,
                                  const Mem_root_array<AccessPath *> &children);
#endif

#endif  // SQL_RANGE_OPTIMIZER_ROWID_ORDERED_RETRIEVAL_PLAN_H_
