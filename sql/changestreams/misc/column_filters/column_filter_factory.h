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

#ifndef CS_COLUMN_FILTER_FACTORY_H
#define CS_COLUMN_FILTER_FACTORY_H

#include "sql/changestreams/misc/column_filters/column_filter_inbound_func_indexes.h"
#include "sql/changestreams/misc/column_filters/column_filter_inbound_gipk.h"
#include "sql/changestreams/misc/column_filters/column_filter_outbound_func_indexes.h"

namespace cs {
namespace util {
/**
  @class ColumnFilterFactory

  This class allows the developer to create a filter instance given a type
 */
class ColumnFilterFactory {
 public:
  /**
    @brief The different types of filters for column iteration
  */
  enum class ColumnFilterType {
    outbound_func_index,  // filter functional indexes when outputting an event
    inbound_func_index,   // filter functional indexes when receiving an event
    inbound_gipk          // Filter GIPK when only present on the replica
  };

  /**
   @brief Create a filter object

   @param filter_type the filter to be created
   @return a unique pointer to a sub class of ColumnFilter
  */
  static std::unique_ptr<ColumnFilter> create_filter(
      ColumnFilterType filter_type) {
    switch (filter_type) {
      case ColumnFilterType::outbound_func_index:
        return std::make_unique<
            cs::util::ColumnFilterOutboundFunctionalIndexes>();
      case ColumnFilterType::inbound_func_index:
        return std::make_unique<ColumnFilterInboundFunctionalIndexes>();
      case ColumnFilterType::inbound_gipk:
        return std::make_unique<ColumnFilterInboundGipk>();
      default:
        /* This shall never happen. */
        assert(0); /* purecov: inspected */
        break;
    }
    return nullptr;
  }

  /**
    @brief Returns if a filter is needed given the parameters

    @param thd The thread being used in this context
    @param table The table being filtered
    @param tabledef The replicated table context if applicable
    @param filter_type The filter type being tested
    @return true if the filter should be used
    @return false if the filter is not needed
   */
  static bool is_filter_needed(THD const &thd, TABLE *table,
                               table_def const *tabledef,
                               ColumnFilterType filter_type) {
    switch (filter_type) {
      case ColumnFilterType::outbound_func_index:
        return ColumnFilterOutboundFunctionalIndexes::is_filter_needed(
            thd, table, tabledef);
      case ColumnFilterType::inbound_func_index:
        return ColumnFilterInboundFunctionalIndexes::is_filter_needed(
            thd, table, tabledef);
      case ColumnFilterType::inbound_gipk:
        return ColumnFilterInboundGipk::is_filter_needed(thd, table, tabledef);
      default:
        /* This shall never happen. */
        assert(0); /* purecov: inspected */
        break;
    }
    return false;
  }
};
}  // namespace util
}  // namespace cs
#endif  // CS_COLUMN_FILTER_FACTORY_H