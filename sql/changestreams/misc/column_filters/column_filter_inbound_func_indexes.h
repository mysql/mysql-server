/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef CS_COLUMN_FILTER_INBOUND_FUNC_INDEX_H
#define CS_COLUMN_FILTER_INBOUND_FUNC_INDEX_H

#include "sql/changestreams/misc/column_filters/column_filter.h"
#include "sql/rpl_utility.h"  // table_def

namespace cs {
namespace util {
/**
  @class ColumnFilterInboundFunctionalIndexes

  One use case for filtering relates to hidden generated columns. These type
  of columns are used to support functional indexes and are not meant to be
  replicated nor included in the serialization/deserialization of binlog
  events.  Moreover, since hidden generated columns are always placed at the
  end of the field set, replication would break for cases where replicas have
  extra columns, if they were not excluded from replication:

       SOURCE TABLE `t`                REPLICA TABLE `t`
       +----+----+----+------+------+  +----+----+----+-----+------+------+
       | C1 | C2 | C3 | HGC1 | HGC2 |  | C1 | C2 | C3 | EC1 | HGC1 | HGC2 |
       +----+----+----+------+------+  +----+----+----+-----+------+------+

  In the above example, the extra column `EC1` in the replica will be paired
  with the hidden generated column `HGC1` of the source, if hidden generated
  columns were to be replicated. With filtering enabled for hidden generated
  columns, applier will observe the columns as follows:

       SOURCE TABLE `t`                REPLICA TABLE `t`
       +----+----+----+                +----+----+----+-----+
       | C1 | C2 | C3 |                | C1 | C2 | C3 | EC1 |
       +----+----+----+                +----+----+----+-----+

   Inbound states we are receiving something from the source and filtering
   that data.
 */
class ColumnFilterInboundFunctionalIndexes : public ColumnFilter {
  bool filter_column(TABLE const *table, size_t column_index) override;
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
#endif  // CS_COLUMN_FILTER_INBOUND_FUNC_INDEX_H