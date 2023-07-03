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

#ifndef RPL_REPLICATED_COLUMNS_VIEW_GIPK_ON_SOURCE__H
#define RPL_REPLICATED_COLUMNS_VIEW_GIPK_ON_SOURCE__H

#include "sql/changestreams/misc/replicated_columns_view.h"  // ReplicatedColumnsView

namespace cs {
namespace util {
/**
  @class ReplicatedColumnsViewWithGipkOnSource

  Class used when we want a column view over a table in a context where the
  replicated table contains a GIPK on the source, but not on the replica.

     SOURCE TABLE `t`
       +------+----+----+----+
       | GIPK | C1 | C2 | C3 |
       +------+----+----+----+

     REPLICA TABLE `t`
       +----+----+----+
       | C1 | C2 | C3 |
       +----+----+----+

  This class differs from a standard column view:
   - begin() will cause that returned filtered position, that pertain to the
     source, are always incremented by 1.
     This way, C1 on the replica matches C1 on the source data
 */
class ReplicatedColumnsViewWithGipkOnSource : public ReplicatedColumnsView {
 public:
  /**
    Constructor which takes the TABLE object whose field set will be iterated.

    @param table reference to the target TABLE object.
    @param thd instance of `THD` class to be used to determine if filtering is
               to be enabled. It may be `nullptr`.
   */
  ReplicatedColumnsViewWithGipkOnSource(TABLE const *table,
                                        THD const *thd = nullptr);
  /**
    Destructor for the class.
   */
  virtual ~ReplicatedColumnsViewWithGipkOnSource() override = default;
  /**
    This method overrides Table_columns_view::begin
    Its start value insure that filtered positions are incremented by 1
    when compared to the base iterator.

    @return a column view iterator over the table
   */
  Table_columns_view<>::iterator begin() override;

  // --> Deleted constructors and methods to remove default move/copy semantics
  ReplicatedColumnsViewWithGipkOnSource(
      const ReplicatedColumnsViewWithGipkOnSource &rhs) = delete;
  ReplicatedColumnsViewWithGipkOnSource(
      ReplicatedColumnsViewWithGipkOnSource &&rhs) = delete;
  ReplicatedColumnsViewWithGipkOnSource &operator=(
      const ReplicatedColumnsViewWithGipkOnSource &rhs) = delete;
  ReplicatedColumnsViewWithGipkOnSource &operator=(
      ReplicatedColumnsViewWithGipkOnSource &&rhs) = delete;
  // <--
};
}  // namespace util
}  // namespace cs
#endif  // RPL_REPLICATED_COLUMNS_VIEW_GIPK_ON_SOURCE__H
