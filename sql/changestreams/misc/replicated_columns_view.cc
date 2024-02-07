/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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

#include "sql/changestreams/misc/replicated_columns_view.h"

#include "sql/rpl_record.h"  // Bit_reader
#include "sql/sql_class.h"   // THD

cs::util::ReplicatedColumnsView::ReplicatedColumnsView(THD const *thd)
    : cs::util::ReplicatedColumnsView{nullptr, thd} {}

cs::util::ReplicatedColumnsView::ReplicatedColumnsView(TABLE const *target,
                                                       THD const *thd)
    : Table_columns_view{} {
  filter_fn_type filter{nullptr};

  filter = [this](TABLE const *table, size_t column_index) -> bool {
    return this->execute_filtering(table, column_index);
  };

  this->set_thd(thd)       //
      .set_filter(filter)  //
      .set_table(target);
}

cs::util::ReplicatedColumnsView &cs::util::ReplicatedColumnsView::set_thd(
    THD const *thd) {
  this->m_thd = thd;
  this->init_fields_bitmaps();
  return (*this);
}

void cs::util::ReplicatedColumnsView::add_filter(
    cs::util::ColumnFilterFactory::ColumnFilterType filter_type) {
  m_filters.push_back(
      cs::util::ColumnFilterFactory::create_filter(filter_type));
  this->init_fields_bitmaps();
}

void cs::util::ReplicatedColumnsView::add_filter_if_needed(
    THD const &thd, TABLE *table, table_def const *tabledef,
    cs::util::ColumnFilterFactory::ColumnFilterType filter_type) {
  if (cs::util::ColumnFilterFactory::is_filter_needed(thd, table, tabledef,
                                                      filter_type)) {
    m_filters.push_back(
        cs::util::ColumnFilterFactory::create_filter(filter_type));
    this->init_fields_bitmaps();
  }
}

bool cs::util::ReplicatedColumnsView::execute_filtering(TABLE const *table,
                                                        size_t column_index) {
  for (const auto &filter : m_filters) {
    if (filter->filter_column(table, column_index)) return true;
  }
  return false;
}
