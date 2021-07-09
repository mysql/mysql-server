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

#ifndef SQL_RANGE_OPTIMIZER_SKIP_SCAN_H_
#define SQL_RANGE_OPTIMIZER_SKIP_SCAN_H_

#include <sys/types.h>

#include "my_alloc.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_inttypes.h"
#include "sql/field.h"  // Field
#include "sql/key.h"
#include "sql/malloc_allocator.h"  // IWYU pragma: keep
#include "sql/range_optimizer/range_optimizer.h"

class Cost_estimate;
class JOIN;
class SEL_ARG;
class SEL_ROOT;
class String;
struct TABLE;

/*
  Index scan for range queries that can use skip scans.

  This class provides a specialized index access method for the queries
  of the forms:

       SELECT A_1,...,A_k, B_1,...,B_m, C
         FROM T
        WHERE
         EQ(A_1,...,A_k)
         AND RNG(C);

  where all selected fields are parts of the same index.
  The class of queries that can be processed by this quick select is fully
  specified in the description of get_best_skip_scan() in opt_range.cc.

  Since one of the requirements is that all select fields are part of the same
  index, this class produces only index keys, and not complete records.
*/

class QUICK_SKIP_SCAN_SELECT : public QUICK_SELECT_I {
 private:
  KEY *index_info;            /* Index for skip scan */
  SEL_ROOT *index_range_tree; /* Range tree for skip scan */
  MY_BITMAP column_bitmap;    /* Map of key parts to be read */

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
    uint cur_eq_prefix;
  };
  EQPrefix *eq_prefixes;
  const uint eq_prefix_len; /* Total length of the equality prefix. */
  uint eq_prefix_key_parts; /* A number of keyparts in skip scan prefix */
  uchar *eq_prefix;         /* Storage for current equality prefix. */

  uchar *distinct_prefix; /* Storage for prefix A_1, ... B_m. */
  uint distinct_prefix_len;
  uint distinct_prefix_key_parts;

  KEY_PART_INFO *range_key_part; /* The keypart of range condition 'C'. */
  MEM_ROOT *mem_root;
  const uint range_key_len;
  /*
    Denotes whether the first key for the current equality prefix was
    retrieved.
  */
  bool seen_first_key;

  /* Storage for full lookup key for use with handler::read_range_first/next */
  uchar *const min_range_key;
  uchar *const max_range_key;
  uchar *const min_search_key;
  uchar *const max_search_key;
  const uint range_cond_flag;

  key_range start_key;
  key_range end_key;

  bool has_aggregate_function;

  bool next_eq_prefix();

 public:
  QUICK_SKIP_SCAN_SELECT(TABLE *table, KEY *index_info, uint index,
                         KEY_PART_INFO *range_part, SEL_ROOT *index_range_tree,
                         uint eq_prefix_len, uint eq_prefix_parts,
                         uint used_key_parts,
                         const Cost_estimate *read_cost_arg, ha_rows records,
                         MEM_ROOT *temp_mem_root, bool has_aggregate_function,
                         uchar *min_range_key, uchar *max_range_key,
                         uchar *min_search_key, uchar *max_search_key,
                         uint range_cond_flag, uint range_key_len);
  ~QUICK_SKIP_SCAN_SELECT() override;
  int init() override;
  void need_sorted_output() override {}
  int reset() override;
  int get_next() override;
  bool reverse_sorted() const override { return false; }
  bool reverse_sort_possible() const override { return false; }
  bool unique_key_range() override { return false; }
  RangeScanType get_type() const override { return QS_TYPE_SKIP_SCAN; }
  bool is_loose_index_scan() const override { return true; }
  bool is_agg_loose_index_scan() const override {
    return has_aggregate_function;
  }
  void add_keys_and_lengths(String *key_names, String *used_lengths) override;
#ifndef NDEBUG
  void dbug_dump(int indent, bool verbose) override;
#endif
  void get_fields_used(MY_BITMAP *used_fields) override {
    for (uint i = 0; i < used_key_parts; i++)
      bitmap_set_bit(used_fields, index_info->key_part[i].field->field_index());
  }
  void add_info_string(String *str) override;
};

#endif  // SQL_RANGE_OPTIMIZER_SKIP_SCAN_H_
