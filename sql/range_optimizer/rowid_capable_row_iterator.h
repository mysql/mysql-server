/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef SQL_RANGE_OPTIMIZER_ROWID_CAPABLE_ROW_ITERATOR_H_
#define SQL_RANGE_OPTIMIZER_ROWID_CAPABLE_ROW_ITERATOR_H_

#include "my_inttypes.h"
#include "sql/iterators/row_iterator.h"

/**
  An interface for RowIterators that have a given row ID, ie.,
  they can be children in ROR (rowid-ordered) scans. The only
  examples of this are QUICK_RANGE_SCAN_SELECT and RowIDIntersectionIterator
  (which itself can also be a parent).
 */
class RowIDCapableRowIterator : public TableRowIterator {
 public:
  RowIDCapableRowIterator(THD *thd, TABLE *table)
      : TableRowIterator(thd, table) {}

  /*
    Row ID of last row retrieved by this quick select. This is used only when
    doing ROR-index_merge selects. Updated on successful Read().
  */
  virtual uchar *last_rowid() const = 0;
};

#endif  // SQL_RANGE_OPTIMIZER_ROWID_CAPABLE_ROW_ITERATOR_H_
