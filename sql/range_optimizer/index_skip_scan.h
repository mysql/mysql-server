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

#ifndef SQL_RANGE_OPTIMIZER_INDEX_SKIP_SCAN_H_
#define SQL_RANGE_OPTIMIZER_INDEX_SKIP_SCAN_H_

#include <sys/types.h>

#include "my_alloc.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_inttypes.h"
#include "sql/field.h"  // Field
#include "sql/key.h"
#include "sql/malloc_allocator.h"  // IWYU pragma: keep
#include "sql/range_optimizer/index_skip_scan_plan.h"
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

class IndexSkipScanIterator : public TableRowIterator {
 private:
  uint index;              /* Index this quick select uses */
  KEY *index_info;         /* Index for skip scan */
  MY_BITMAP column_bitmap; /* Map of key parts to be read */

  const uint eq_prefix_len; /* Total length of the equality prefix. */
  uint eq_prefix_key_parts; /* A number of keyparts in skip scan prefix */
  EQPrefix *eq_prefixes;
  uchar *eq_prefix; /* Storage for current equality prefix. */

  // Total length of first used_key_parts parts of the key.
  uint max_used_key_length;

  // Max. number of (first) key parts this quick select uses for retrieval.
  // eg. for "(key1p1=c1 AND key1p2=c2) OR key1p1=c2" used_key_parts == 2.
  uint used_key_parts;

  uchar *distinct_prefix; /* Storage for prefix A_1, ... B_m. */
  uint distinct_prefix_len;
  uint distinct_prefix_key_parts;

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
  IndexSkipScanIterator(THD *thd, TABLE *table, KEY *index_info, uint index,
                        uint eq_prefix_len, uint eq_prefix_key_parts,
                        EQPrefix *eq_prefixes, uint used_key_parts,
                        MEM_ROOT *temp_mem_root, bool has_aggregate_function,
                        uchar *min_range_key, uchar *max_range_key,
                        uchar *min_search_key, uchar *max_search_key,
                        uint range_cond_flag, uint range_key_len);
  ~IndexSkipScanIterator() override;
  bool Init() override;
  int Read() override;
};

#endif  // SQL_RANGE_OPTIMIZER_INDEX_SKIP_SCAN_H_
