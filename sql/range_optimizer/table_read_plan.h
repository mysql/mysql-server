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

#ifndef SQL_RANGE_OPTIMIZER_TABLE_READ_PLAN_H_
#define SQL_RANGE_OPTIMIZER_TABLE_READ_PLAN_H_

#include "sql/range_optimizer/range_optimizer.h"

class Opt_trace_object;
class RANGE_OPT_PARAM;
class SEL_ROOT;

/*
  Table rows retrieval plan. Range optimizer creates QUICK_SELECT_I-derived
  objects from table read plans.
*/
class TABLE_READ_PLAN {
 public:
  /*
    Plan read cost, with or without cost of full row retrieval, depending
    on plan creation parameters.
  */
  Cost_estimate cost_est;
  ha_rows records; /* estimate of #rows to be examined */

  // The table scanned.
  TABLE *const table;

  /*
    Index this quick select uses, or MAX_KEY for quick selects
    that use several indexes
   */
  const uint index;

  /*
    Max. number of (first) key parts this quick select uses for retrieval.
    eg. for "(key1p1=c1 AND key1p2=c2) OR key1p1=c2" used_key_parts == 2.
    Applicable if index!= MAX_KEY.

    For QUICK_GROUP_MIN_MAX_SELECT it includes MIN/MAX argument keyparts.
   */
  uint used_key_parts;

  const bool forced_by_hint;

  /*
    Create quick select for this plan.
    SYNOPSIS
     make_quick()
       retrieve_full_rows  If true, created quick select will do full record
                           retrieval.
       return_mem_root     Memory pool to use.

    NOTES
      retrieve_full_rows is ignored by some implementations.

    RETURN
      created quick select
      NULL on any error.
  */
  virtual QUICK_SELECT_I *make_quick(bool retrieve_full_rows,
                                     MEM_ROOT *return_mem_root) = 0;

  TABLE_READ_PLAN(TABLE *table_arg, int index_arg, uint used_key_parts_arg,
                  bool forced_by_hint_arg)
      : table(table_arg),
        index(index_arg),
        used_key_parts(used_key_parts_arg),
        forced_by_hint(forced_by_hint_arg) {}
  virtual ~TABLE_READ_PLAN() = default;

  /**
     Add basic info for this TABLE_READ_PLAN to the optimizer trace.

     @param thd          Thread handle
     @param param        Parameters for range analysis of this table
     @param trace_object The optimizer trace object the info is appended to
  */
  virtual void trace_basic_info(THD *thd, const RANGE_OPT_PARAM *param,
                                Opt_trace_object *trace_object) const = 0;
};

#endif  // SQL_RANGE_OPTIMIZER_TABLE_READ_PLAN_H_
