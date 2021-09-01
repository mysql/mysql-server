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
#include "sql/range_optimizer/trp_helpers.h"
#include "sql/sql_class.h"

class Opt_trace_context;
struct MEM_ROOT;

void TRP_INDEX_MERGE::trace_basic_info(THD *thd, const RANGE_OPT_PARAM *param,
                                       double, double,
                                       Opt_trace_object *trace_object) const {
  Opt_trace_context *const trace = &thd->opt_trace;
  trace_object->add_alnum("type", "index_merge");
  Opt_trace_array ota(trace, "index_merge_of");
  for (AccessPath *range_scan : range_scans) {
    Opt_trace_object trp_info(trace);
    ::trace_basic_info(thd, range_scan, param, &trp_info);
  }
}

RowIterator *TRP_INDEX_MERGE::make_quick(THD *thd, double expected_rows,
                                         MEM_ROOT *return_mem_root,
                                         ha_rows *examined_rows) {
  assert(!need_rows_in_rowid_order);

  QUICK_INDEX_MERGE_SELECT *quick_imerge;
  QUICK_RANGE_SELECT *quick;
  if (!(quick_imerge = new (return_mem_root)
            QUICK_INDEX_MERGE_SELECT(return_mem_root, thd, table)))
    return nullptr;

  // TODO: This needs to move into CreateIteratorFromAccessPath() instead.
  for (AccessPath *range_scan : range_scans) {
    if (!(quick = down_cast<QUICK_RANGE_SELECT *>(
              range_scan->trp_wrapper().trp->make_quick(
                  thd, expected_rows, return_mem_root, examined_rows))) ||
        quick_imerge->push_quick_back(quick)) {
      destroy(quick);
      destroy(quick_imerge);
      return nullptr;
    }
  }
  return quick_imerge;
}

bool TRP_INDEX_MERGE::is_keys_used(const MY_BITMAP *fields) {
  for (AccessPath *range_scan : range_scans) {
    if (is_key_used(table, used_index(range_scan), fields)) return true;
  }
  return false;
}

void TRP_INDEX_MERGE::get_fields_used(MY_BITMAP *used_fields) const {
  for (AccessPath *range_scan : range_scans) {
    ::get_fields_used(range_scan, used_fields);
  }
}

void TRP_INDEX_MERGE::add_info_string(String *str) const {
  bool first = true;
  str->append(STRING_WITH_LEN("sort_union("));

  // For EXPLAIN compatibility with older versions, PRIMARY is always printed
  // last.
  for (bool print_primary : {false, true}) {
    for (AccessPath *range_scan : range_scans) {
      const bool is_primary = table->file->primary_key_is_clustered() &&
                              used_index(range_scan) == table->s->primary_key;
      if (is_primary != print_primary) continue;
      if (!first)
        str->append(',');
      else
        first = false;
      ::add_info_string(range_scan, str);
    }
  }
  str->append(')');
}

void TRP_INDEX_MERGE::add_keys_and_lengths(String *key_names,
                                           String *used_lengths) const {
  bool first = true;

  // For EXPLAIN compatibility with older versions, PRIMARY is always printed
  // last.
  for (bool print_primary : {false, true}) {
    for (AccessPath *range_scan : range_scans) {
      const bool is_primary = table->file->primary_key_is_clustered() &&
                              used_index(range_scan) == table->s->primary_key;
      if (is_primary != print_primary) continue;
      if (first) {
        first = false;
      } else {
        key_names->append(',');
        used_lengths->append(',');
      }

      ::add_keys_and_lengths(range_scan, key_names, used_lengths);
    }
  }
}

#ifndef NDEBUG
void TRP_INDEX_MERGE::dbug_dump(int indent, bool verbose) {
  fprintf(DBUG_FILE, "%*squick index_merge select\n", indent, "");
  fprintf(DBUG_FILE, "%*smerged scans {\n", indent, "");
  for (AccessPath *range_scan : range_scans) {
    ::dbug_dump(range_scan, indent + 2, verbose);
  }
  fprintf(DBUG_FILE, "%*s}\n", indent, "");
}
#endif
