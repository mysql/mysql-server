/* Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "sql/changestreams/misc/column_filters/column_filter_inbound_gipk.h"
#include "sql/sql_gipk.h"  // table_has_generated_invisible_primary_key

bool cs::util::ColumnFilterInboundGipk::filter_column(TABLE const *,
                                                      size_t column_index) {
  return column_index == 0;
}

bool cs::util::ColumnFilterInboundGipk::is_filter_needed(
    THD const &, TABLE *table, table_def const *tabledef) {
  bool source_has_gipk = tabledef->is_gipk_present_on_source_table();
  bool replica_has_gipk = table_has_generated_invisible_primary_key(table);

  return replica_has_gipk && !source_has_gipk;
}
