/* Copyright (c) 2000, 2021, Oracle and/or its affiliates.

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

#ifndef SQL_RANGE_OPTIMIZER_RANGE_SCAN_DESC_H_
#define SQL_RANGE_OPTIMIZER_RANGE_SCAN_DESC_H_

#include <sys/types.h>

#include "sql/range_optimizer/range_optimizer.h"
#include "sql/range_optimizer/range_scan.h"
#include "sql/sql_list.h"

class QUICK_SELECT_DESC : public QUICK_RANGE_SELECT {
 public:
  QUICK_SELECT_DESC(QUICK_RANGE_SELECT &&q, uint used_key_parts);
  int get_next() override;
  bool reverse_sorted() const override { return true; }
  bool reverse_sort_possible() const override { return true; }
  RangeScanType get_type() const override { return QS_TYPE_RANGE_DESC; }
  bool is_loose_index_scan() const override { return false; }
  bool is_agg_loose_index_scan() const override { return false; }
  QUICK_SELECT_I *make_reverse(uint) override {
    return this;  // is already reverse sorted
  }

 private:
  bool range_reads_after_key(QUICK_RANGE *range);
  int reset(void) override {
    rev_it.rewind();
    return QUICK_RANGE_SELECT::reset();
  }
  List<QUICK_RANGE> rev_ranges;
  List_iterator<QUICK_RANGE> rev_it;
  uint m_used_key_parts;
};

#endif  // SQL_RANGE_OPTIMIZER_RANGE_SCAN_DESC_H_
