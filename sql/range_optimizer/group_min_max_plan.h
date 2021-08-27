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
class RANGE_OPT_PARAM;
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
  List<Item_sum> min_functions;
  List<Item_sum> max_functions;
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
  uint group_key_parts;   ///< Number of index key parts in the group prefix
  KEY *index_info;        ///< The index chosen for data access
  uint key_infix_len;     ///< Longest key for equality predicates
  SEL_ROOT
  *index_tree_tracing_only;  ///< The sub-tree corresponding to index_info
  bool is_index_scan;        ///< Use index_next() instead of random read
  JOIN *join;
  KEY_PART *used_key_part;
  uint keyno;
  const uint real_key_parts;
  const uint max_used_key_length;
  Quick_ranges_array key_infix_ranges;
  Quick_ranges min_max_ranges;
  Quick_ranges prefix_ranges;

 public:
  /** Number of records selected by the ranges in index_tree. */
  ha_rows quick_prefix_records;

 public:
  void trace_basic_info(THD *thd, const RANGE_OPT_PARAM *param,
                        Opt_trace_object *trace_object) const override;

  TRP_GROUP_MIN_MAX(
      bool have_min_arg, bool have_max_arg, bool have_agg_distinct_arg,
      KEY_PART_INFO *min_max_arg_part_arg, uint group_prefix_len_arg,
      uint used_key_parts_arg, uint group_key_parts_arg, KEY *index_info_arg,
      uint index_arg, uint key_infix_len_arg, SEL_ROOT *index_tree_arg,
      ha_rows quick_prefix_records_arg, TABLE *table_arg, JOIN *join_arg,
      KEY_PART *used_key_part_arg, uint keyno_arg, uint real_key_parts_arg,
      uint max_used_key_length_arg, Quick_ranges_array key_infix_ranges_arg,
      Quick_ranges min_max_ranges_arg, Quick_ranges prefix_ranges_arg);

  RowIterator *make_quick(THD *thd, double expected_rows,
                          bool retrieve_full_rows, MEM_ROOT *mem_root,
                          ha_rows *examined_rows) override;
  void use_index_scan() { is_index_scan = true; }
  bool get_is_index_scan() const { return is_index_scan; }

  RangeScanType get_type() const override { return QS_TYPE_GROUP_MIN_MAX; }
  bool is_agg_loose_index_scan() const override { return have_agg_distinct; }
  void need_sorted_output() override { /* always do it */
  }

  void get_fields_used(MY_BITMAP *used_fields) const override {
    for (uint i = 0; i < used_key_parts; ++i) {
      bitmap_set_bit(used_fields, index_info->key_part[i].field->field_index());
    }
  }

  void add_info_string(String *str) const override;
  void add_keys_and_lengths(String *key_names,
                            String *used_lengths) const override;

  unsigned get_max_used_key_length() const final { return max_used_key_length; }

#ifndef NDEBUG
  void dbug_dump(int indent, bool verbose) override;
#endif
};

TRP_GROUP_MIN_MAX *get_best_group_min_max(THD *thd, RANGE_OPT_PARAM *param,
                                          SEL_TREE *tree,
                                          enum_order order_direction,
                                          bool skip_records_in_range,
                                          const Cost_estimate *cost_est);

#endif  // SQL_RANGE_OPTIMIZER_GROUP_MIN_MAX_PLAN_H_
