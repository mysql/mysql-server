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

#ifndef SQL_RANGE_OPTIMIZER_ROWID_ORDERED_RETRIEVAL_PLAN_H_
#define SQL_RANGE_OPTIMIZER_ROWID_ORDERED_RETRIEVAL_PLAN_H_

#include <sys/types.h>

#include "my_base.h"
#include "sql/handler.h"
#include "sql/join_optimizer/overflow_bitset.h"
#include "sql/range_optimizer/range_opt_param.h"
#include "sql/range_optimizer/range_optimizer.h"

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
  OverflowBitset covered_fields;

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

// Planning related information when picking the best combination
// of rowid ordered scans for a ROR-Intersect plan.
class ROR_intersect_plan {
 public:
  ROR_intersect_plan(const RANGE_OPT_PARAM *param, size_t num_fields);
  ROR_intersect_plan(const ROR_intersect_plan &) = delete;
  ROR_intersect_plan &operator=(const ROR_intersect_plan &plan);

  bool add(OverflowBitset needed_fields, ROR_SCAN_INFO *ror_scan,
           bool is_cpk_scan, Opt_trace_object *trace_costs, bool ignore_cost);
  double get_scan_selectivity(const ROR_SCAN_INFO *scan) const;
  size_t num_scans() const { return m_ror_scans.size(); }

 public:
  /// Range optimizer parameter
  const RANGE_OPT_PARAM *m_param;
  /// Rowid ordered scans that are part of this plan.
  Mem_root_array<ROR_SCAN_INFO *> m_ror_scans;
  /// Whether this plan with the chosen rowid ordered scans is covering or not.
  bool m_is_covering{false};
  /// Output rows for this plan.
  double m_out_rows;
  /// Total cost for the plan - m_index_read_cost + disk_sweep_cost
  Cost_estimate m_total_cost;

 private:
  /// Bitmap of fields covered by the scans in the plan.
  OverflowBitset m_covered_fields;
  /// Number of rows to be read from indexes that are used for rowid ordered
  /// scans
  ha_rows m_index_records{0};
  /// Total cost for reading the indexes picked in the plan.
  Cost_estimate m_index_read_cost;
};

AccessPath *get_best_ror_intersect(THD *thd, const RANGE_OPT_PARAM *param,
                                   TABLE *table,
                                   bool index_merge_intersect_allowed,
                                   SEL_TREE *tree, double cost_est,
                                   bool force_index_merge_result,
                                   bool reuse_handler);

void trace_basic_info_rowid_intersection(THD *thd, const AccessPath *path,
                                         const RANGE_OPT_PARAM *param,
                                         Opt_trace_object *trace_object);

void trace_basic_info_rowid_union(THD *thd, const AccessPath *path,
                                  const RANGE_OPT_PARAM *param,
                                  Opt_trace_object *trace_object);

void add_keys_and_lengths_rowid_intersection(const AccessPath *path,
                                             String *key_names,
                                             String *used_lengths);

void add_keys_and_lengths_rowid_union(const AccessPath *path, String *key_names,
                                      String *used_lengths);

OverflowBitset get_needed_fields(const RANGE_OPT_PARAM *param);

ROR_SCAN_INFO *make_ror_scan(const RANGE_OPT_PARAM *param, int idx,
                             SEL_ROOT *sel_root, OverflowBitset needed_fields);

void find_intersect_order(Mem_root_array<ROR_SCAN_INFO *> *ror_scans,
                          OverflowBitset needed_fields, MEM_ROOT *mem_root);

AccessPath *MakeRowIdOrderedIndexScanAccessPath(ROR_SCAN_INFO *scan,
                                                TABLE *table,
                                                KEY_PART *used_key_part,
                                                bool reuse_handler,
                                                MEM_ROOT *mem_root);

#ifndef NDEBUG
void dbug_dump_rowid_intersection(int indent, bool verbose,
                                  const Mem_root_array<AccessPath *> &children);

void dbug_dump_rowid_union(int indent, bool verbose,
                           const Mem_root_array<AccessPath *> &children);
#endif

#endif  // SQL_RANGE_OPTIMIZER_ROWID_ORDERED_RETRIEVAL_PLAN_H_
