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

#ifndef SQL_RANGE_OPTIMIZER_INDEX_SKIP_SCAN_PLAN_H_
#define SQL_RANGE_OPTIMIZER_INDEX_SKIP_SCAN_PLAN_H_

#include <sys/types.h>

#include "my_base.h"
#include "sql/range_optimizer/range_optimizer.h"

class KEY;
class KEY_PART_INFO;
class Opt_trace_object;
class RANGE_OPT_PARAM;
class SEL_ARG;
class SEL_ROOT;
class SEL_TREE;
struct MEM_ROOT;

/*
  This is an array of array of equality constants with length
  eq_prefix_key_parts.

  For example, an equality predicate like "a IN (1, 2) AND b IN (2, 3, 4)",
  eq_prefixes will contain:

  [
    { eq_key_prefixes = array[1, 2], cur_eq_prefix = ... },
    { eq_key_prefixes = array[2, 3, 4], cur_eq_prefix = ... }
  ]
 */
struct EQPrefix {
  Bounds_checked_array<uchar *> eq_key_prefixes;

  /*
     During skip scan, we will have to iterate through all possible
     equality prefixes. This is the product of all the elements in
     eq_prefix_elements. In the above example, there are 2 x 3 = 6 possible
     equality prefixes.

     To track which prefix we are on, we use cur_eq_prefix. For example,
     if both EQPrefixes have the value 1 here, it indicates that the current
     equality prefix is (2, 3).
   */
  unsigned cur_eq_prefix;
};

/**
  Logically a part of AccessPath::index_skip_scan(), but is too large,
  so split out into its own struct.
 */
struct IndexSkipScanParameters {
  KEY *index_info;           ///< The index chosen for data access
  uint eq_prefix_len;        ///< Length of the equality prefix
  uint eq_prefix_key_parts;  ///< Number of key parts in the equality prefix
  EQPrefix *eq_prefixes;     ///< Array of equality constants (IN list)
  KEY_PART_INFO *range_key_part;  ///< The key part matching the range condition
  uint used_key_parts;            ///< Number of index keys used for skip scan
  double read_cost;               ///< Total cost of read
  uint index;                     ///< Position of chosen index

  uchar *min_range_key;
  uchar *max_range_key;
  uchar *min_search_key;
  uchar *max_search_key;
  uint range_cond_flag;
  uint range_key_len;
  uint num_output_rows;

  // The sub-tree corresponding to the range condition
  // (on key part C - for more details see description of get_best_skip_scan()).
  //
  // Does not necessarily live as long as the AccessPath, so used for tracing
  // only.
  const SEL_ARG *range_part_tracing_only;

  SEL_ROOT *index_range_tree;   ///< The sub-tree corresponding to index_info
  bool has_aggregate_function;  ///< TRUE if there are aggregate functions.
};

Mem_root_array<AccessPath *> get_all_skip_scans(THD *thd,
                                                RANGE_OPT_PARAM *param,
                                                SEL_TREE *tree,
                                                enum_order order_direction,
                                                bool skip_records_in_range,
                                                bool force_skip_scan);
AccessPath *get_best_skip_scan(THD *thd, RANGE_OPT_PARAM *param, SEL_TREE *tree,
                               enum_order order_direction,
                               bool skip_records_in_range,
                               bool force_skip_scan);

void trace_basic_info_index_skip_scan(THD *thd, const AccessPath *path,
                                      const RANGE_OPT_PARAM *param,
                                      Opt_trace_object *trace_object);

#ifndef NDEBUG
void dbug_dump_index_skip_scan(int indent, bool verbose,
                               const AccessPath *path);
#endif

#endif  // SQL_RANGE_OPTIMIZER_INDEX_SKIP_SCAN_PLAN_H_
