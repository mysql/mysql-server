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

#ifndef CS_COLUMN_FILTER_H
#define CS_COLUMN_FILTER_H

#include "sql/sql_class.h"

namespace cs {
namespace util {
/**
  @class ColumnFilter

  When iterating the table fields for sometimes you want to skip some columns.
  Such a case is the filtering of GIPK or functional indexes when replicating
  a table.

  Classes that inherit this class shall also implement the method

    static bool is_filter_needed(THD const *thd, TABLE *table,
                                 table_def const *tabledef);

  It allows for the conditional addition of the filter only when it makes sense
 */
class ColumnFilter {
 public:
  virtual ~ColumnFilter() = default;
  virtual bool filter_column(TABLE const *table, size_t column_index) = 0;
};
}  // namespace util
}  // namespace cs
#endif  // CS_COLUMN_FILTER_H