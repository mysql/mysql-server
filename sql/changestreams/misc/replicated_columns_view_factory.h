/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef CS_REPLICATED_COLUMNS_VIEW_UTILS_H
#define CS_REPLICATED_COLUMNS_VIEW_UTILS_H

#include "sql/changestreams/misc/replicated_columns_view.h"  // ReplicatedColumnsView
#include "sql/changestreams/misc/replicated_columns_view_with_gipk_on_source.h"
#include "sql/rpl_utility.h"  // tabledef
#include "sql/sql_gipk.h"     // table_has_generated_invisible_primary_key

namespace cs {
namespace util {

/**
  @brief This class allows the creation of different types of column view
  instances and also adds different filters depending on context.
*/
class ReplicatedColumnsViewFactory {
 public:
  /**
    This function returns the appropriate class to iterate over the given
    table columns. It also adds inbound filtering

    It might return a standard column view/iteration class or one that
    accounts for GIPK or other differences

    @param thd The thread object to extract context information
    @param table The table where the column view will be created
    @param tabledef Context on the replicated table

    @return A unique pointer to a ReplicatedColumnsView object instance
   */
  static std::unique_ptr<ReplicatedColumnsView>
  get_columns_view_with_inbound_filters(THD *thd, TABLE *table,
                                        table_def const *tabledef) {
    std::unique_ptr<ReplicatedColumnsView> column_view;

    bool source_has_gipk =
        tabledef && tabledef->is_gipk_present_on_source_table();
    bool replica_has_gipk =
        table && table_has_generated_invisible_primary_key(table);

    if (!replica_has_gipk && source_has_gipk)
      column_view = std::unique_ptr<cs::util::ReplicatedColumnsView>{
          new cs::util::ReplicatedColumnsViewWithGipkOnSource{table, thd}};
    else
      column_view = std::unique_ptr<cs::util::ReplicatedColumnsView>{
          new cs::util::ReplicatedColumnsView{table, thd}};

    column_view->add_filter_if_needed(
        *thd, table, tabledef,
        cs::util::ColumnFilterFactory::ColumnFilterType::inbound_func_index);
    column_view->add_filter_if_needed(
        *thd, table, tabledef,
        cs::util::ColumnFilterFactory::ColumnFilterType::inbound_gipk);

    return column_view;
  }

  /**
    This function returns the appropriate class to iterate over the given
    table columns. It also adds outbound filtering

    @param thd The thread object to extract context information
    @param table The table where the column view will be created

    @return A unique pointer to a ReplicatedColumnsView object instance
  */
  static std::unique_ptr<ReplicatedColumnsView>
  get_columns_view_with_outbound_filters(THD *thd, TABLE *table) {
    auto column_view = std::unique_ptr<cs::util::ReplicatedColumnsView>{
        new cs::util::ReplicatedColumnsView{table, thd}};

    column_view->add_filter_if_needed(
        *thd, table, nullptr,
        cs::util::ColumnFilterFactory::ColumnFilterType::outbound_func_index);

    return column_view;
  }
};
}  // namespace util
}  // namespace cs
#endif  // CS_REPLICATED_COLUMNS_VIEW_UTILS_H
