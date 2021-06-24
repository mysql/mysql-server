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

#ifndef SQL_RANGE_OPTIMIZER_GROUP_MIN_MAX_PLAN_H_
#define SQL_RANGE_OPTIMIZER_GROUP_MIN_MAX_PLAN_H_

#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "sql/range_optimizer/table_read_plan.h"
#include "sql/sql_const.h"

class Cost_estimate;
class KEY;
class KEY_PART_INFO;
class Opt_trace_object;
class PARAM;
class QUICK_SELECT_I;
class SEL_ROOT;
class SEL_TREE;
struct MEM_ROOT;

/*
  Plan for a QUICK_GROUP_MIN_MAX_SELECT scan.
*/

class TRP_GROUP_MIN_MAX : public TABLE_READ_PLAN {
 private:
  bool have_min;  ///< true if there is a MIN function
  bool have_max;  ///< true if there is a MAX function
  /**
    true if there is an aggregate distinct function, e.g.
    "COUNT(DISTINCT x)"
  */
  bool have_agg_distinct;
  /**
    The key_part of the only field used by all MIN/MAX functions.
    Note that TRP_GROUP_MIN_MAX is not used if there are MIN/MAX
    functions on more than one field.
  */
  KEY_PART_INFO *min_max_arg_part;
  uint group_prefix_len;  ///< Length of all key parts in the group prefix
  uint used_key_parts;    ///< Number of index key parts used for access
  uint group_key_parts;   ///< Number of index key parts in the group prefix
  KEY *index_info;        ///< The index chosen for data access
  uint index;             ///< The id of the chosen index
  uchar key_infix[MAX_KEY_LENGTH];  ///< Constants from equality predicates
  uint key_infix_len;               ///< Length of key_infix
  SEL_TREE *range_tree;  ///< Represents all range predicates in the query
  SEL_ROOT *index_tree;  ///< The sub-tree corresponding to index_info
  uint param_idx;        ///< Index of used key in param->key
  bool is_index_scan;    ///< Use index_next() instead of random read
 public:
  /** Number of records selected by the ranges in index_tree. */
  ha_rows quick_prefix_records;

 public:
  void trace_basic_info(const PARAM *param,
                        Opt_trace_object *trace_object) const override;

  TRP_GROUP_MIN_MAX(bool have_min_arg, bool have_max_arg,
                    bool have_agg_distinct_arg,
                    KEY_PART_INFO *min_max_arg_part_arg,
                    uint group_prefix_len_arg, uint used_key_parts_arg,
                    uint group_key_parts_arg, KEY *index_info_arg,
                    uint index_arg, uint key_infix_len_arg, SEL_TREE *tree_arg,
                    SEL_ROOT *index_tree_arg, uint param_idx_arg,
                    ha_rows quick_prefix_records_arg)
      : have_min(have_min_arg),
        have_max(have_max_arg),
        have_agg_distinct(have_agg_distinct_arg),
        min_max_arg_part(min_max_arg_part_arg),
        group_prefix_len(group_prefix_len_arg),
        used_key_parts(used_key_parts_arg),
        group_key_parts(group_key_parts_arg),
        index_info(index_info_arg),
        index(index_arg),
        key_infix_len(key_infix_len_arg),
        range_tree(tree_arg),
        index_tree(index_tree_arg),
        param_idx(param_idx_arg),
        is_index_scan(false),
        quick_prefix_records(quick_prefix_records_arg) {}

  QUICK_SELECT_I *make_quick(PARAM *param, bool retrieve_full_rows,
                             MEM_ROOT *parent_alloc) override;
  void use_index_scan() { is_index_scan = true; }
};

TRP_GROUP_MIN_MAX *get_best_group_min_max(PARAM *param, SEL_TREE *tree,
                                          enum_order order_direction,
                                          const Cost_estimate *cost_est);

#endif  // SQL_RANGE_OPTIMIZER_GROUP_MIN_MAX_PLAN_H_
