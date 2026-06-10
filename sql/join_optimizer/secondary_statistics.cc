/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/secondary_statistics.h"

#include <string>

#include "sql/field.h"
#include "sql/handler.h"
#include "sql/join_optimizer/optimizer_trace.h"
#include "sql/sql_class.h"
#include "sql/system_variables.h"
#include "sql/table.h"

namespace secondary_statistics {

double NumDistinctValues(THD *thd, const Field &field) {
  if (!thd->variables.enable_secondary_engine_statistics) return -1.0;
  const TABLE_SHARE *ts = field.table->s;
  if (!ts->secondary_load) return -1.0;

  const handlerton *secondary_engine =
      EligibleSecondaryEngineHandlerton(thd, &ts->secondary_engine);
  if (secondary_engine != nullptr &&
      secondary_engine->get_column_statistics != nullptr) {
    const auto rows_in_table =
        static_cast<double>(field.table->file->stats.records);
    auto column_stats = secondary_engine->get_column_statistics(
        thd, field.table->s->db.str, field.table->s->table_name.str,
        field.field_name, rows_in_table);
    if (column_stats.has_value() && column_stats->num_distinct_values > 0) {
      const double distinct_values = column_stats->num_distinct_values;
      if (TraceStarted(thd)) {
        Trace(thd) << " - Getting secondary statistics (NDV) for "
                   << field.table->s->table_name.str << '.' << field.field_name
                   << ": " << std::to_string(distinct_values) << '\n';
      }
      return distinct_values;
    }
  }
  return -1.0;
}

}  // namespace secondary_statistics
