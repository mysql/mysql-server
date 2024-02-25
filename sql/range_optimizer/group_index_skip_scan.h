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

#ifndef SQL_RANGE_OPTIMIZER_GROUP_INDEX_SKIP_SCAN_H_
#define SQL_RANGE_OPTIMIZER_GROUP_INDEX_SKIP_SCAN_H_

#include <sys/types.h>

#include "m_string.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_inttypes.h"
#include "sql/field.h"
#include "sql/key.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/sql_const.h"
#include "sql_string.h"

class Cost_estimate;
class Item_sum;
class JOIN;
class IndexRangeScanIterator;
class SEL_ARG;
struct TABLE;
template <class T>
class List;
template <class T>
class List_iterator;

/*
  Index scan for GROUP-BY queries with MIN/MAX aggregate functions.

  This class provides a specialized index access method for GROUP-BY queries
  of the forms:

       SELECT A_1,...,A_k, [B_1,...,B_m], [MIN(C)], [MAX(C)]
         FROM T
        WHERE [RNG(A_1,...,A_p ; where p <= k)]
         [AND EQ(B_1,...,B_m)]
         [AND PC(C)]
         [AND PA(A_i1,...,A_iq)]
       GROUP BY A_1,...,A_k;

    or

       SELECT DISTINCT A_i1,...,A_ik
         FROM T
        WHERE [RNG(A_1,...,A_p ; where p <= k)]
         [AND PA(A_i1,...,A_iq)];

  where all selected fields are parts of the same index.
  The class of queries that can be processed by this quick select is fully
  specified in the description of get_best_group_min_max().

  The Read() method directly produces result tuples, thus obviating the
  need to use AggregateIterator, because all grouping is already done inside
  Read().

  Since one of the requirements is that all select fields are part of the same
  index, this class produces only index keys, and not complete records.
*/

class GroupIndexSkipScanIterator : public TableRowIterator {
 private:
  uint index;                  /* Index this quick select uses */
  KEY *index_info;             /* The index chosen for data access */
  uchar *group_prefix;         /* Key prefix consisting of the GROUP fields. */
  const uint group_prefix_len; /* Length of the group prefix. */
  uint group_key_parts;        /* A number of keyparts in the group prefix */
  uchar *last_prefix;          /* Prefix of the last group for detecting EOF. */
  bool have_agg_distinct;      /*   aggregate_function(DISTINCT ...).  */
  bool seen_first_key;         /* Denotes whether the first key was retrieved.*/
  KEY_PART_INFO *min_max_arg_part; /* The keypart of the only argument field */
                                   /* of all MIN/MAX functions.              */
  uint min_max_arg_len;     /* The length of the MIN/MAX argument field */
  bool min_max_keypart_asc; /* TRUE if min_max key part is ascending. */
  uint key_infix_len;
  // Total length of first used_key_parts parts of the key.
  uint max_used_key_length;
  // The current infix range position (in key_infix_ranges) used for row
  // retrieval.
  uint cur_infix_range_position[MAX_REF_PARTS];
  // Indicates if all infix ranges have been used to retrieve rows (all ranges
  // in key_infix_ranges)
  bool seen_all_infix_ranges;

  const Quick_ranges *prefix_ranges;
  unsigned cur_prefix_range_idx;
  QUICK_RANGE *last_prefix_range;

  const Quick_ranges
      *min_max_ranges; /* Array of range ptrs for the MIN/MAX field. */
  const Quick_ranges_array
      *key_infix_ranges; /* Array of key infix range arrays.   */
  uint real_prefix_len;  /* Length of key prefix extended with key_infix. */
  uint real_key_parts;   /* A number of keyparts in the above value.      */
  const Mem_root_array<Item_sum *> *min_functions;
  const Mem_root_array<Item_sum *> *max_functions;
  /*
    Use index scan to get the next different key instead of jumping into it
    through index read
  */
  bool is_index_scan;
  bool m_seen_eof;
  MEM_ROOT *mem_root;
  int next_prefix();
  int get_next_prefix(uint prefix_length, uint group_key_parts,
                      uchar *cur_prefix);
  bool append_next_infix();
  void reset_group();
  int next_min_in_range();
  int next_max_in_range();
  int next_min();
  int next_max();
  void update_min_result(bool *reset);
  void update_max_result(bool *reset);

 public:
  GroupIndexSkipScanIterator(THD *thd, TABLE *table_arg,
                             const Mem_root_array<Item_sum *> *min_functions,
                             const Mem_root_array<Item_sum *> *max_functions,
                             bool have_agg_distinct,
                             KEY_PART_INFO *min_max_arg_part,
                             uint group_prefix_len, uint group_key_parts,
                             uint real_key_parts, uint max_used_key_length_arg,
                             KEY *index_info, uint use_index,
                             uint key_infix_len, MEM_ROOT *return_mem_root,
                             bool is_index_scan,
                             const Quick_ranges *prefix_ranges,
                             const Quick_ranges_array *key_infix_ranges,
                             const Quick_ranges *min_max_ranges);
  ~GroupIndexSkipScanIterator() override;
  bool Init() override;
  int Read() override;
  bool is_agg_distinct() const { return have_agg_distinct; }
};

#endif  // SQL_RANGE_OPTIMIZER_GROUP_INDEX_SKIP_SCAN_H_
