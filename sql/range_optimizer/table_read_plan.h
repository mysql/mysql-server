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

  /*
    If true, the scan returns rows in rowid order. This is used only for
    scans that can be both ROR and non-ROR.
  */
  bool is_ror;

  /*
    If true, this plan can be used for index merge scan.
  */
  bool is_imerge;

  /*
    Create quick select for this plan.
    SYNOPSIS
     make_quick()
       param               Parameter from test_quick_select
       retrieve_full_rows  If true, created quick select will do full record
                           retrieval.
       return_mem_root     Memory pool to use.

    NOTES
      retrieve_full_rows is ignored by some implementations.

    RETURN
      created quick select
      NULL on any error.
  */
  virtual QUICK_SELECT_I *make_quick(RANGE_OPT_PARAM *param,
                                     bool retrieve_full_rows,
                                     MEM_ROOT *return_mem_root) = 0;

  virtual ~TABLE_READ_PLAN() = default;

  /**
     Add basic info for this TABLE_READ_PLAN to the optimizer trace.

     @param thd          Thread handle
     @param param        Parameters for range analysis of this table
     @param trace_object The optimizer trace object the info is appended to
  */
  virtual void trace_basic_info(THD *thd, const RANGE_OPT_PARAM *param,
                                Opt_trace_object *trace_object) const = 0;
  virtual bool is_forced_by_hint() { return false; }
};

#endif  // SQL_RANGE_OPTIMIZER_TABLE_READ_PLAN_H_
