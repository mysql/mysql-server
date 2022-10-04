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

#ifndef CS_COLUMN_FILTER_GIPK_H
#define CS_COLUMN_FILTER_GIPK_H

#include "sql/changestreams/misc/column_filters/column_filter.h"
#include "sql/rpl_utility.h"  // table_def

namespace cs {
namespace util {
/**
  @class ColumnFilterInboundGipk

  Class used when we want a column view over a table in a context where the
  replicated table contains a GIPK on the replica, but not on the source.

     SOURCE TABLE `t`
       +----+----+----+
       | C1 | C2 | C3 |
       +----+----+----+

     REPLICA TABLE `t`
       +------+----+----+----+
       | GIPK | C1 | C2 | C3 |
       +------+----+----+----+

  This class filters the first column on iteration.
 */
class ColumnFilterInboundGipk : public ColumnFilter {
  virtual bool filter_column(TABLE const *, size_t column_index) override;

  /**
    @brief Is this filter needed given context passed in the parameters

    @param thd the thread objected associated to filter
    @param table the table where the columns are being iterated
    @param tabledef the source table definition if applicable
    @return true if the filter should be used
    @return false if the filter is not needed
  */
 public:
  static bool is_filter_needed(THD const &thd, TABLE *table,
                               table_def const *tabledef);
};
}  // namespace util
}  // namespace cs
#endif  // CS_COLUMN_FILTER_GIPK_H