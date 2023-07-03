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

#include "sql/changestreams/misc/column_filters/column_filter_outbound_func_indexes.h"

bool cs::util::ColumnFilterOutboundFunctionalIndexes::filter_column(
    TABLE const *table, size_t column_index) {
  // If the set of filtered columns is changed, we need to replicate the change
  // in other blocks that reproduce the behavior - Rapid binlog parser, for
  // instance.
  return bitmap_is_set(&table->fields_for_functional_indexes, column_index);
}

bool cs::util::ColumnFilterOutboundFunctionalIndexes::is_filter_needed(
    THD const &, TABLE *, table_def const *) {
  return true;
}
