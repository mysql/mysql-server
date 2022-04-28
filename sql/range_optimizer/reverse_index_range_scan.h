/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

#ifndef SQL_RANGE_OPTIMIZER_REVERSE_INDEX_RANGE_SCAN_H_
#define SQL_RANGE_OPTIMIZER_REVERSE_INDEX_RANGE_SCAN_H_

#include <sys/types.h>

#include "sql/range_optimizer/index_range_scan.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/sql_list.h"

/**
  An iterator much like IndexRangeScanIterator, but it scans in the reverse
  order. This makes it at times more complicated, but since it doesn't support
  being a part of a ROR scan, it is also less complicated in many ways.

  One could argue that this and IndexRangeScanIterator should be factored into
  a common base class with separate _ASC and _DESC classes, but they don't
  actually duplicate that much code.
 */
class ReverseIndexRangeScanIterator : public TableRowIterator {
 public:
  ReverseIndexRangeScanIterator(THD *thd, TABLE *table, ha_rows *examined_rows,
                                double expected_rows, int index,
                                MEM_ROOT *return_mem_root, uint mrr_flags,
                                Bounds_checked_array<QUICK_RANGE *> ranges,
                                bool using_extended_key_parts);
  ~ReverseIndexRangeScanIterator() override;
  int Read() override;
  bool Init() override;

 private:
  static range_seq_t quick_range_rev_seq_init(void *init_param, uint, uint);

  const uint m_index; /* Index this quick select uses */
  ha_rows m_expected_rows;
  ha_rows *m_examined_rows;

  MEM_ROOT *mem_root;
  bool inited = false;

  // TODO: pre-allocate space to avoid malloc/free for small number of columns.
  MY_BITMAP column_bitmap;

  uint m_mrr_flags; /* Flags to be used with MRR interface */

  Bounds_checked_array<QUICK_RANGE *> ranges; /* ordered array of range ptrs */
  /* Members needed to use the MRR interface */
  QUICK_RANGE_SEQ_CTX qr_traversal_ctx;

  QUICK_RANGE *last_range;  // The range we are currently scanning, or nullptr.
  int current_range_idx;

  /* Info about index we're scanning */
  KEY_PART_INFO *key_part_info;

  // Whether this reverse scan uses extended keyparts (in case of Innodb,
  // secondary index is extended to include primary key).
  bool m_using_extended_key_parts{false};

  bool range_reads_after_key(QUICK_RANGE *range);
  int cmp_prev(QUICK_RANGE *range);
};

#endif  // SQL_RANGE_OPTIMIZER_REVERSE_INDEX_RANGE_SCAN_H_
