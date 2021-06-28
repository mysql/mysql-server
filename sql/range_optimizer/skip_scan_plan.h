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

#ifndef SQL_RANGE_OPTIMIZER_SKIP_SCAN_PLAN_H_
#define SQL_RANGE_OPTIMIZER_SKIP_SCAN_PLAN_H_

#include <sys/types.h>

#include "my_base.h"
#include "sql/range_optimizer/table_read_plan.h"

class KEY;
class KEY_PART_INFO;
class Opt_trace_object;
class RANGE_OPT_PARAM;
class QUICK_SELECT_I;
class SEL_ARG;
class SEL_ROOT;
class SEL_TREE;
struct MEM_ROOT;

/*
  Plan for a QUICK_SKIP_SCAN_SELECT scan.
*/

class TRP_SKIP_SCAN : public TABLE_READ_PLAN {
 private:
  KEY *index_info;                ///< The index chosen for data access
  uint index;                     ///< The id of the chosen index
  uint eq_prefix_len;             ///< Length of the equality prefix
  uint eq_prefix_parts;           ///< Number of parts in the equality prefix
  KEY_PART_INFO *range_key_part;  ///< The key part corresponding to the range
                                  ///< condition

  /**
    The sub-tree corresponding to the range condition
    (on key part C - for more details see description of get_best_skip_scan()).
  */
  SEL_ARG *range_cond;
  SEL_ROOT *index_range_tree;  ///< The sub-tree corresponding to index_info
  uint used_key_parts;         ///< Number of index key parts used for access
  bool forced_by_hint;  ///< TRUE if skip scan is forced by optimizer hint
  bool has_aggregate_function;  ///< TRUE if there are aggregate functions.

 public:
  void trace_basic_info(THD *thd, const RANGE_OPT_PARAM *param,
                        Opt_trace_object *trace_object) const override;

  TRP_SKIP_SCAN(KEY *index_info, uint index, SEL_ROOT *index_range_tree,
                uint eq_prefix_len, uint eq_prefix_parts,
                KEY_PART_INFO *range_key_part, SEL_ARG *range_cond,
                uint used_key_parts, bool forced_by_hint, ha_rows read_records,
                bool has_aggregate_function)
      : index_info(index_info),
        index(index),
        eq_prefix_len(eq_prefix_len),
        eq_prefix_parts(eq_prefix_parts),
        range_key_part(range_key_part),
        range_cond(range_cond),
        index_range_tree(index_range_tree),
        used_key_parts(used_key_parts),
        forced_by_hint(forced_by_hint),
        has_aggregate_function(has_aggregate_function) {
    records = read_records;
  }

  ~TRP_SKIP_SCAN() override = default;

  QUICK_SELECT_I *make_quick(RANGE_OPT_PARAM *param, bool retrieve_full_rows,
                             MEM_ROOT *return_mem_root) override;
  bool is_forced_by_hint() override { return forced_by_hint; }
};

TRP_SKIP_SCAN *get_best_skip_scan(THD *thd, RANGE_OPT_PARAM *param,
                                  SEL_TREE *tree, enum_order order_direction,
                                  bool skip_records_in_range,
                                  bool force_skip_scan);

#endif  // SQL_RANGE_OPTIMIZER_SKIP_SCAN_PLAN_H_
