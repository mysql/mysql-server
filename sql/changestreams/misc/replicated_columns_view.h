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

#ifndef RPL_REPLICATED_COLUMNS_VIEW_H
#define RPL_REPLICATED_COLUMNS_VIEW_H

#include "sql/changestreams/misc/column_filters/column_filter_factory.h"
#include "sql/rpl_utility.h"            // table_def
#include "sql/table_column_iterator.h"  // Table_columns_view::iterator

namespace cs {
namespace util {
/**
  @class ReplicatedColumnsView

  Since it's not mandatory that all fields in a TABLE object are replicated,
  this class extends Table_columns_view container and adds logic to filter out
  not needed columns.

  One active use-case relates to hidden generated columns. These type of
  columns are used to support functional indexes and are not meant to be
  replicated nor included in the serialization/deserialization of binlog
  events.
  @see ColumnFilterInboundFunctionalIndexes
  @see ColumnFilterOutboundFunctionalIndexes

  Another case relates to GIPKs, where the source or replica might contain a
  generated primary key that does not exist on the other side of the stream.
  In cases of differences in GIPK, the columns should be filtered out
  @see ColumnFilterInboundGipk

  This class allows for the use of other filters that can be added at any point.
 */
class ReplicatedColumnsView : public Table_columns_view<> {
 public:
  /**
    Constructor for table iteration where a table and filters can be configured

    @param thd instance of `THD` class to be used to determine if filtering is
               to be enabled in some cases. It may be `nullptr`.
   */
  ReplicatedColumnsView(THD const *thd = nullptr);
  /**
    Constructor which takes the TABLE object whose field set will be iterated.

    @param table reference to the target TABLE object.
    @param thd instance of `THD` class to be used to determine if filtering is
               to be enabled. It may be `nullptr`.
   */
  ReplicatedColumnsView(TABLE const *table, THD const *thd = nullptr);
  /**
    Destructor for the class.
   */
  ~ReplicatedColumnsView() override = default;
  /**
    Setter to initialize the `THD` object instance to be used to determine if
    filtering is enabled.

    @param thd instance of `THD` class to be used to determine if filtering is
               to be enabled. It may be `nullptr`.

    @return this object reference (for chaining purposes).
   */
  ReplicatedColumnsView &set_thd(THD const *thd);

  /**
    Returns whether or not the field of table `table` at `column_index` is to be
    filtered from this container iteration, according to the list of filters

    @param table reference to the target TABLE object.
    @param column_index index for the column to be tested for filtering,

    @return true if the field is to be filtered out and false otherwise.
   */
  virtual bool execute_filtering(TABLE const *table, size_t column_index);

  /**
    Adds a new filter according to the given type

    @param filter_type the filter type to be added to this column view
  */
  void add_filter(cs::util::ColumnFilterFactory::ColumnFilterType filter_type);

  /**
     @brief adds a new filter if the filter's static member function
     is_filter_needed returns true

     @param thd the THD with context on the situation
     @param table the table being iterated
     @param tabledef the replicated table context if applicable
     @param filter_type the filter type to be added
   */
  void add_filter_if_needed(
      THD const &thd, TABLE *table, table_def const *tabledef,
      cs::util::ColumnFilterFactory::ColumnFilterType filter_type);

  // --> Deleted constructors and methods to remove default move/copy semantics
  ReplicatedColumnsView(const ReplicatedColumnsView &rhs) = delete;
  ReplicatedColumnsView(ReplicatedColumnsView &&rhs) = delete;
  ReplicatedColumnsView &operator=(const ReplicatedColumnsView &rhs) = delete;
  ReplicatedColumnsView &operator=(ReplicatedColumnsView &&rhs) = delete;
  // <--

 private:
  /**
    Instance of `THD` class to be used to determine if filtering is to be
    enabled.
   */
  THD const *m_thd;

  /** List of filters to be used against the list of fields */
  std::vector<std::unique_ptr<cs::util::ColumnFilter>> m_filters;
};
}  // namespace util
}  // namespace cs

#endif  // RPL_REPLICATED_COLUMNS_VIEW_H
