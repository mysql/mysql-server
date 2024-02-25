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

#include "sql/range_optimizer/index_merge_plan.h"

#include "sql/join_optimizer/access_path.h"
#include "sql/opt_trace.h"
#include "sql/range_optimizer/path_helpers.h"
#include "sql/sql_class.h"

class Opt_trace_context;
struct MEM_ROOT;

void trace_basic_info_index_merge(THD *thd, const AccessPath *path,
                                  const RANGE_OPT_PARAM *param,
                                  Opt_trace_object *trace_object) {
  Opt_trace_context *const trace = &thd->opt_trace;
  trace_object->add_alnum("type", "index_merge");
  Opt_trace_array ota(trace, "index_merge_of");
  for (AccessPath *range_scan : *path->index_merge().children) {
    Opt_trace_object path_info(trace);
    trace_basic_info(thd, range_scan, param, &path_info);
  }
}

void add_keys_and_lengths_index_merge(const AccessPath *path, String *key_names,
                                      String *used_lengths) {
  bool first = true;
  TABLE *table = path->index_merge().table;

  // For EXPLAIN compatibility with older versions, PRIMARY is always
  // printed last.
  for (bool print_primary : {false, true}) {
    for (AccessPath *child : *path->index_merge().children) {
      const bool is_primary = table->file->primary_key_is_clustered() &&
                              used_index(child) == table->s->primary_key;
      if (is_primary != print_primary) continue;
      if (first) {
        first = false;
      } else {
        key_names->append(',');
        used_lengths->append(',');
      }

      ::add_keys_and_lengths(child, key_names, used_lengths);
    }
  }
}

#ifndef NDEBUG
void dbug_dump_index_merge(int indent, bool verbose,
                           const Mem_root_array<AccessPath *> &children) {
  fprintf(DBUG_FILE, "%*squick index_merge select\n", indent, "");
  fprintf(DBUG_FILE, "%*smerged scans {\n", indent, "");
  for (AccessPath *range_scan : children) {
    dbug_dump(range_scan, indent + 2, verbose);
  }
  fprintf(DBUG_FILE, "%*s}\n", indent, "");
}
#endif
