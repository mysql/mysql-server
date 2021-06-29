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

#include "sql/range_optimizer/index_merge_plan.h"

#include "sql/handler.h"
#include "sql/opt_trace.h"
#include "sql/range_optimizer/index_merge.h"
#include "sql/range_optimizer/range_opt_param.h"
#include "sql/range_optimizer/range_scan.h"
#include "sql/range_optimizer/range_scan_plan.h"
#include "sql/sql_class.h"

class Opt_trace_context;
class QUICK_SELECT_I;
struct MEM_ROOT;

void TRP_INDEX_MERGE::trace_basic_info(THD *thd, const RANGE_OPT_PARAM *param,
                                       Opt_trace_object *trace_object) const {
  Opt_trace_context *const trace = &thd->opt_trace;
  trace_object->add_alnum("type", "index_merge");
  Opt_trace_array ota(trace, "index_merge_of");
  for (TRP_RANGE **current = range_scans; current != range_scans_end;
       current++) {
    Opt_trace_object trp_info(trace);
    (*current)->trace_basic_info(thd, param, &trp_info);
  }
}

QUICK_SELECT_I *TRP_INDEX_MERGE::make_quick(bool, MEM_ROOT *return_mem_root) {
  QUICK_INDEX_MERGE_SELECT *quick_imerge;
  QUICK_RANGE_SELECT *quick;
  /* index_merge always retrieves full rows, ignore retrieve_full_rows */
  if (!(quick_imerge = new (return_mem_root)
            QUICK_INDEX_MERGE_SELECT(return_mem_root, table)))
    return nullptr;
  assert(quick_imerge->index == index);

  quick_imerge->records = records;
  quick_imerge->cost_est = cost_est;

  for (TRP_RANGE **range_scan = range_scans; range_scan != range_scans_end;
       range_scan++) {
    if (!(quick = down_cast<QUICK_RANGE_SELECT *>(
              (*range_scan)->make_quick(false, return_mem_root))) ||
        quick_imerge->push_quick_back(quick)) {
      destroy(quick);
      destroy(quick_imerge);
      return nullptr;
    }
  }
  quick_imerge->forced_by_hint = forced_by_hint;
  return quick_imerge;
}
