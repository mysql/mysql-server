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

#ifndef SQL_RANGE_OPTIMIZER_RANGE_SCAN_PLAN_H_
#define SQL_RANGE_OPTIMIZER_RANGE_SCAN_PLAN_H_

#include <sys/types.h>

#include "my_dbug.h"
#include "sql/handler.h"
#include "sql/range_optimizer/geometry.h"
#include "sql/range_optimizer/range_opt_param.h"
#include "sql/range_optimizer/range_scan.h"
#include "sql/range_optimizer/range_scan_desc.h"
#include "sql/range_optimizer/table_read_plan.h"
#include "sql/sql_const.h"

class Opt_trace_object;
class RANGE_OPT_PARAM;
class SEL_ROOT;
class SEL_TREE;
struct MEM_ROOT;

bool get_ranges_from_tree(MEM_ROOT *return_mem_root, TABLE *table,
                          KEY_PART *key, uint keyno, SEL_ROOT *key_tree,
                          uint num_key_parts, unsigned *used_key_parts,
                          Quick_ranges *ranges);

/*
  Plan for a QUICK_RANGE_SELECT scan.
  TRP_RANGE::make_quick ignores retrieve_full_rows parameter because
  QUICK_RANGE_SELECT doesn't distinguish between 'index only' scans and full
  record retrieval scans.
*/

class TRP_RANGE : public TABLE_READ_PLAN {
 public:
  // NOTE: Both used_key_part_arg and ranges must be allocated on the
  // return_mem_root, as they need to outlive the range optimizer.
  TRP_RANGE(SEL_ROOT *key_arg, uint mrr_flags_arg, uint mrr_buf_size_arg,
            TABLE *table_arg, KEY_PART *used_key_part_arg, uint keyno_arg,
            bool is_ror_arg, bool is_imerge_arg,
            Bounds_checked_array<QUICK_RANGE *> ranges_arg,
            uint used_key_parts_arg)
      : TABLE_READ_PLAN(table_arg, keyno_arg, used_key_parts_arg,
                        /*forced_by_hint_arg=*/false),
        key(key_arg),
        mrr_flags(mrr_flags_arg),
        mrr_buf_size(mrr_buf_size_arg),
        used_key_part(used_key_part_arg),
        is_ror(is_ror_arg),
        is_imerge(is_imerge_arg),
        ranges(ranges_arg) {}

  RowIterator *make_quick(THD *thd, double expected_rows, bool,
                          MEM_ROOT *return_mem_root,
                          ha_rows *examined_rows) override {
    DBUG_TRACE;

    QUICK_RANGE_SELECT *quick;
    if (table->key_info[index].flags & HA_SPATIAL) {
      quick = new (return_mem_root) QUICK_RANGE_SELECT_GEOM(
          thd, table, examined_rows, expected_rows, index,
          need_rows_in_rowid_order,
          /*reuse_handler=*/false, return_mem_root, mrr_flags, mrr_buf_size,
          used_key_part, ranges);
    } else {
      quick = new (return_mem_root)
          QUICK_RANGE_SELECT(thd, table, examined_rows, expected_rows, index,
                             need_rows_in_rowid_order,
                             /*reuse_handler=*/false, return_mem_root,
                             mrr_flags, mrr_buf_size, used_key_part, ranges);
      if (reverse) {
        // TODO: Unify the two classes, or at least make some way
        // of costructing a QUICK_SELECT_DESC without creating
        // the forward class first.
        QUICK_RANGE_SELECT *reverse_quick = new (return_mem_root)
            QUICK_SELECT_DESC(std::move(*quick), used_key_parts);
        destroy(quick);
        return reverse_quick;
      } else {
        return quick;
      }
    }
    return quick;
  }

  void trace_basic_info(THD *thd, const RANGE_OPT_PARAM *param, double cost,
                        double num_output_rows,
                        Opt_trace_object *trace_object) const override;

  bool can_be_used_for_ror() const { return is_ror; }
  bool can_be_used_for_imerge() const { return is_imerge; }
  uint get_mrr_flags() const { return mrr_flags; }

  bool unique_key_range() const override;

  RangeScanType get_type() const override { return QS_TYPE_RANGE; }
  bool reverse_sorted() const override { return reverse; }

  void need_sorted_output() override { mrr_flags |= HA_MRR_SORTED; }

  bool make_reverse(uint used_key_parts_arg) override {
    reverse = true;
    used_key_parts = used_key_parts_arg;
    return false;
  }

  void get_fields_used(MY_BITMAP *used_fields) const override {
    for (uint i = 0; i < used_key_parts; ++i) {
      bitmap_set_bit(used_fields, used_key_part[i].field->field_index());
    }
  }

  void add_info_string(String *str) const override;
  void add_keys_and_lengths(String *key_names,
                            String *used_lengths) const override;
  unsigned get_max_used_key_length() const final;

#ifndef NDEBUG
  void dbug_dump(int indent, bool verbose) override;
#endif

 private:
  /**
    Root of red-black tree for intervals over key fields to be used in
    "range" method retrieval. See SEL_ARG graph description.

    Used only for tracing.
   */
  SEL_ROOT *key;
  uint mrr_flags;
  uint mrr_buf_size;

  // The key part(s) we are scanning on. Note that this may be an array.
  KEY_PART *used_key_part;

  /*
    If true, the scan returns rows in rowid order.
   */
  const bool is_ror;

  /*
    If true, this plan can be used for index merge scan.
   */
  const bool is_imerge;

  // The actual ranges we are scanning over (originally derived from “key”).
  Bounds_checked_array<QUICK_RANGE *> ranges;

  bool reverse = false;
};

/*
  Get best "range" table read plan for given SEL_TREE, also update some info

  SYNOPSIS
    get_key_scans_params()
      param                    Parameters from test_quick_select
      tree                     Make range select for this SEL_TREE
      index_read_must_be_used  true <=> assume 'index only' option will be set
                               (except for clustered PK indexes)
      update_tbl_stats         true <=> update table->quick_* with information
                               about range scans we've evaluated.
      interesting_order        The sort order the range access method must be
                               able to provide. Three-value logic:
                               asc/desc/don't care
      skip_records_in_range    Same value as JOIN_TAB::skip_records_in_range().
      cost_est                 Maximum cost. i.e. don't create read plans with
                               cost > cost_est.
      needed_reg               ptr to needed_reg argument
                               of test_quick_select().

  DESCRIPTION
    Find the best "range" table read plan for given SEL_TREE.
    The side effects are
     - tree->ror_scans is updated to indicate which scans are ROR scans.
     - if update_tbl_stats=true then table->quick_* is updated with info
       about every possible range scan.

  RETURN
    Best range read plan
    NULL if no plan found or error occurred
*/

AccessPath *get_key_scans_params(THD *thd, RANGE_OPT_PARAM *param,
                                 SEL_TREE *tree, bool index_read_must_be_used,
                                 bool update_tbl_stats,
                                 enum_order interesting_order,
                                 bool skip_records_in_range, double cost_est,
                                 Key_map *needed_reg);

/*
  Calculate estimate of number records that will be retrieved by a range
  scan on given index using given SEL_ARG intervals tree.

  SYNOPSIS
    check_quick_select()
      param             Parameter from test_quick_select
      idx               Number of index to use in RANGE_OPT_PARAM::key
                        SEL_TREE::key
      index_only        true  - assume only index tuples will be accessed
                        false - assume full table rows will be read
      tree              Transformed selection condition, tree->key[idx] holds
                        the intervals for the given index.
      update_tbl_stats  true <=> update table->quick_* with information
                        about range scan we've evaluated.
      order_direction   The sort order the range access method must be able
                        to provide. Three-value logic: asc/desc/don't care
      skip_records_in_range Same value as JOIN_TAB::skip_records_in_range().
      mrr_flags   INOUT MRR access flags
      cost        OUT   Scan cost
      is_ror_scan OUT   Set to reflect if the key scan is a ROR
                        (see is_key_scan_ror function for more info)
      is_imerge_scan OUT  Set to reflect if the key scan can be used for
                        index-merge-scan

  NOTES
    param->table->quick_*, param->range_count (and maybe others) are
    updated with data of given key scan, see quick_range_seq_next for details.

  RETURN
    Estimate # of records to be retrieved.
    HA_POS_ERROR if estimate calculation failed due to table handler problems.
*/
ha_rows check_quick_select(THD *thd, RANGE_OPT_PARAM *param, uint idx,
                           bool index_only, SEL_ROOT *tree,
                           bool update_tbl_stats, enum_order order_direction,
                           bool skip_records_in_range, uint *mrr_flags,
                           uint *bufsize, Cost_estimate *cost,
                           bool *is_ror_scan, bool *is_imerge_scan);

#ifndef NDEBUG
void dbug_dump_range(int indent, bool verbose, TABLE *table, int index,
                     KEY_PART *used_key_part,
                     Bounds_checked_array<QUICK_RANGE *> ranges);
#endif

#endif  // SQL_RANGE_OPTIMIZER_RANGE_SCAN_PLAN_H_
