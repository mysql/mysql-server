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

#ifndef SQL_RANGE_OPTIMIZER_INDEX_RANGE_SCAN_H_
#define SQL_RANGE_OPTIMIZER_INDEX_RANGE_SCAN_H_

#include <sys/types.h>
#include <memory>

#include "my_bitmap.h"
#include "my_inttypes.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/range_optimizer/rowid_capable_row_iterator.h"

class KEY_PART_INFO;
class RANGE_OPT_PARAM;
class SEL_ARG;
class SEL_ROOT;
class String;
class THD;
struct KEY_MULTI_RANGE;
struct MEM_ROOT;
struct TABLE;

/*
  MRR range sequence, array<QUICK_RANGE> implementation: sequence traversal
  context.
*/
struct QUICK_RANGE_SEQ_CTX {
  Quick_ranges::const_iterator first;
  Quick_ranges::const_iterator cur;
  Quick_ranges::const_iterator last;
};

/*
  Quick select that does a range scan on a single key. The records are
  returned in key order if ::need_sorted_output() has been called.
*/
class IndexRangeScanIterator : public RowIDCapableRowIterator {
 protected:
  handler *file;

  uint index; /* Index this quick select uses */

  /* Members to deal with case when this quick select is a ROR-merged scan */
  bool in_ror_merged_scan;

  // TODO: pre-allocate space to avoid malloc/free for small number of columns.
  MY_BITMAP column_bitmap;

  friend uint quick_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range);
  friend range_seq_t quick_range_seq_init(void *init_param, uint n_ranges,
                                          uint flags);
  friend class IndexMergeIterator;
  friend class RowIDIntersectionIterator;

  Bounds_checked_array<QUICK_RANGE *> ranges; /* ordered array of range ptrs */
  bool free_file; /* TRUE <=> this->file is "owned" by this quick select */

  /* Range pointers to be used when not using MRR interface */
  QUICK_RANGE **cur_range; /* current element in ranges  */
  QUICK_RANGE *last_range;

  /* Members needed to use the MRR interface */
  QUICK_RANGE_SEQ_CTX qr_traversal_ctx;

  uint mrr_flags;    /* Flags to be used with MRR interface */
  uint mrr_buf_size; /* copy from thd->variables.read_rnd_buff_size */
  HANDLER_BUFFER *mrr_buf_desc; /* the handler buffer */

  /* Info about index we're scanning */
  KEY_PART_INFO *key_part_info;

  const bool need_rows_in_rowid_order;
  const bool reuse_handler;

 private:
  MEM_ROOT *mem_root;
  bool inited = false;
  const bool m_expected_rows;
  ha_rows *m_examined_rows;

  int cmp_next(QUICK_RANGE *range);
  int cmp_prev(QUICK_RANGE *range);
  bool row_in_ranges();
  bool shared_init();
  bool shared_reset();
  bool init_ror_merged_scan();

 public:
  IndexRangeScanIterator(THD *thd, TABLE *table, ha_rows *examined_rows,
                         double expected_rows, uint index_arg,
                         bool need_rows_in_rowid_order, bool reuse_handler,
                         MEM_ROOT *return_mem_root, uint mrr_flags,
                         uint mrr_buf_size,
                         Bounds_checked_array<QUICK_RANGE *> ranges);
  ~IndexRangeScanIterator() override;

  IndexRangeScanIterator(const IndexRangeScanIterator &) = delete;

  /* Default move ctor used by ReverseIndexRangeScanIterator */
  IndexRangeScanIterator(IndexRangeScanIterator &&) = default;

  bool Init() override;
  int Read() override;
  void UnlockRow() override {
    // Override TableRowIterator::UnlockRow(), since we may use
    // a different handler from m_table->file.
    file->unlock_row();
  }

  uint get_mrr_flags() const { return mrr_flags; }
  uchar *last_rowid() const override {
    assert(need_rows_in_rowid_order);
    return file->ref;
  }
};

bool InitIndexRangeScan(TABLE *table, handler *file, int index,
                        unsigned mrr_flags, bool in_ror_merged_scan,
                        MY_BITMAP *column_bitmap);

range_seq_t quick_range_seq_init(void *init_param, uint, uint);
uint quick_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range);

#endif  // SQL_RANGE_OPTIMIZER_INDEX_RANGE_SCAN_H_
