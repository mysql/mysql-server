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
#include "sql/range_optimizer/range_opt_param.h"
#include "sql/range_optimizer/range_scan.h"
#include "sql/range_optimizer/table_read_plan.h"
#include "sql/sql_const.h"

class Opt_trace_object;
class PARAM;
class QUICK_SELECT_I;
class SEL_ROOT;
class SEL_TREE;
struct MEM_ROOT;

QUICK_RANGE_SELECT *get_quick_select(THD *thd, TABLE *table, KEY_PART *key,
                                     uint keyno, uchar *min_key, uchar *max_key,
                                     SEL_ROOT *key_tree, uint mrr_flags,
                                     uint mrr_buf_size, MEM_ROOT *parent_alloc,
                                     uint num_key_parts = MAX_REF_PARTS);

/*
  Plan for a QUICK_RANGE_SELECT scan.
  TRP_RANGE::make_quick ignores retrieve_full_rows parameter because
  QUICK_RANGE_SELECT doesn't distinguish between 'index only' scans and full
  record retrieval scans.
*/

class TRP_RANGE : public TABLE_READ_PLAN {
 public:
  /**
    Root of red-black tree for intervals over key fields to be used in
    "range" method retrieval. See SEL_ARG graph description.
  */
  SEL_ROOT *key;
  uint key_idx; /* key number in PARAM::key and PARAM::real_keynr */
  uint mrr_flags;
  uint mrr_buf_size;

  TRP_RANGE(SEL_ROOT *key_arg, uint idx_arg, uint mrr_flags_arg)
      : key(key_arg), key_idx(idx_arg), mrr_flags(mrr_flags_arg) {}

  QUICK_SELECT_I *make_quick(PARAM *param, bool,
                             MEM_ROOT *parent_alloc) override {
    DBUG_TRACE;
    QUICK_RANGE_SELECT *quick;
    if ((quick = get_quick_select(param->thd, param->table, param->key[key_idx],
                                  param->real_keynr[key_idx], param->min_key,
                                  param->max_key, key, mrr_flags, mrr_buf_size,
                                  parent_alloc))) {
      quick->records = records;
      quick->cost_est = cost_est;
    }
    return quick;
  }

  void trace_basic_info(const PARAM *param,
                        Opt_trace_object *trace_object) const override;
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

TRP_RANGE *get_key_scans_params(PARAM *param, SEL_TREE *tree,
                                bool index_read_must_be_used,
                                bool update_tbl_stats,
                                enum_order interesting_order,
                                bool skip_records_in_range,
                                const Cost_estimate *cost_est,
                                Key_map *needed_reg);

/*
  Calculate estimate of number records that will be retrieved by a range
  scan on given index using given SEL_ARG intervals tree.

  SYNOPSIS
    check_quick_select()
      param             Parameter from test_quick_select
      idx               Number of index to use in PARAM::key SEL_TREE::key
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

  NOTES
    param->is_ror_scan is set to reflect if the key scan is a ROR (see
    is_key_scan_ror function for more info)
    param->table->quick_*, param->range_count (and maybe others) are
    updated with data of given key scan, see quick_range_seq_next for details.

  RETURN
    Estimate # of records to be retrieved.
    HA_POS_ERROR if estimate calculation failed due to table handler problems.
*/
ha_rows check_quick_select(PARAM *param, uint idx, bool index_only,
                           SEL_ROOT *tree, bool update_tbl_stats,
                           enum_order order_direction,
                           bool skip_records_in_range, uint *mrr_flags,
                           uint *bufsize, Cost_estimate *cost);

#endif  // SQL_RANGE_OPTIMIZER_RANGE_SCAN_PLAN_H_
