/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

AccessPath *get_best_ror_intersect(
    THD *thd, const RANGE_OPT_PARAM *param, TABLE *table,
    bool index_merge_intersect_allowed, SEL_TREE *tree,
    const MY_BITMAP *needed_fields, double cost_est,
    bool force_index_merge_result, bool reuse_handler);

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

#ifndef NDEBUG
void dbug_dump_rowid_intersection(int indent, bool verbose,
                                  const Mem_root_array<AccessPath *> &children);

void dbug_dump_rowid_union(int indent, bool verbose,
                           const Mem_root_array<AccessPath *> &children);
#endif

#endif  // SQL_RANGE_OPTIMIZER_ROWID_ORDERED_RETRIEVAL_PLAN_H_
