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

#ifndef SQL_RANGE_OPTIMIZER_GROUP_INDEX_SKIP_SCAN_PLAN_H_
#define SQL_RANGE_OPTIMIZER_GROUP_INDEX_SKIP_SCAN_PLAN_H_

#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/sql_const.h"

class KEY;
class KEY_PART_INFO;
class Opt_trace_object;
class RANGE_OPT_PARAM;
class SEL_ROOT;
class SEL_TREE;

/*
  Plan for a GroupIndexSkipScanIterator scan.
*/

struct GroupIndexSkipScanParameters {
  Mem_root_array<Item_sum *> min_functions;
  Mem_root_array<Item_sum *> max_functions;
  /**
    true if there is an aggregate distinct function, e.g.
    "COUNT(DISTINCT x)"
  */
  bool have_agg_distinct;
  /**
    The key_part of the only field used by all MIN/MAX functions.
    Note that GROUP_INDEX_SKIP_SCAN is not used if there are MIN/MAX
    functions on more than one field.
  */
  KEY_PART_INFO *min_max_arg_part;
  uint group_prefix_len;  ///< Length of all key parts in the group prefix
  uint group_key_parts;   ///< Number of index key parts in the group prefix
  KEY *index_info;        ///< The index chosen for data access
  uint key_infix_len;     ///< Longest key for equality predicates
  SEL_ROOT
  *index_tree_tracing_only;  ///< The sub-tree corresponding to index_info
  bool is_index_scan;        ///< Use index_next() instead of random read
  KEY_PART *used_key_part;
  uint real_key_parts;
  uint max_used_key_length;
  Quick_ranges prefix_ranges;
  Quick_ranges_array key_infix_ranges;
  Quick_ranges min_max_ranges;
};

AccessPath *get_best_group_min_max(THD *thd, RANGE_OPT_PARAM *param,
                                   SEL_TREE *tree, enum_order order_direction,
                                   bool skip_records_in_range, double cost_est);

void trace_basic_info_group_index_skip_scan(THD *thd, const AccessPath *path,
                                            const RANGE_OPT_PARAM *,
                                            Opt_trace_object *trace_object);

void dbug_dump_group_index_skip_scan(int indent, bool verbose,
                                     const AccessPath *path);

#endif  // SQL_RANGE_OPTIMIZER_GROUP_INDEX_SKIP_SCAN_PLAN_H_
