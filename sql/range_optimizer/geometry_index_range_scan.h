/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

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

#ifndef SQL_RANGE_OPTIMIZER_GEOMETRY_INDEX_RANGE_SCAN_H_
#define SQL_RANGE_OPTIMIZER_GEOMETRY_INDEX_RANGE_SCAN_H_

#include <sys/types.h>

#include "sql/range_optimizer/index_range_scan.h"

class THD;
struct MEM_ROOT;
struct TABLE;

class GeometryIndexRangeScanIterator : public IndexRangeScanIterator {
 public:
  GeometryIndexRangeScanIterator(THD *thd, TABLE *table, ha_rows *examined_rows,
                                 double expected_rows, uint index_arg,
                                 bool need_rows_in_rowid_order_arg,
                                 bool reuse_handler_arg,
                                 MEM_ROOT *return_mem_root, uint mrr_flags_arg,
                                 uint mrr_buf_size_arg,
                                 Bounds_checked_array<QUICK_RANGE *> ranges_arg)
      : IndexRangeScanIterator(thd, table, examined_rows, expected_rows,
                               index_arg, need_rows_in_rowid_order_arg,
                               reuse_handler_arg, return_mem_root,
                               mrr_flags_arg, mrr_buf_size_arg, ranges_arg),
        m_examined_rows(examined_rows) {}
  int Read() override;

 private:
  ha_rows *m_examined_rows;
};

#endif  // SQL_RANGE_OPTIMIZER_GEOMETRY_INDEX_RANGE_SCAN_H_
