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

#ifndef SQL_RANGE_OPTIMIZER_ROWID_ORDERED_RETRIEVAL_H_
#define SQL_RANGE_OPTIMIZER_ROWID_ORDERED_RETRIEVAL_H_

#include <assert.h>
#include <sys/types.h>
#include <vector>

#include "my_alloc.h"
#include "my_inttypes.h"
#include "priority_queue.h"
#include "sql/handler.h"
#include "sql/malloc_allocator.h"
#include "sql/range_optimizer/index_range_scan.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/sql_list.h"
#include "sql/table.h"

class String;
class THD;
struct MY_BITMAP;

/*
  Rowid-Ordered Retrieval (ROR) index intersection quick select.
  This quick select produces intersection of row sequences returned
  by several IndexRangeScanIterators it "merges".

  All merged IndexRangeScanIterators must return rowids in rowid order.
  RowIDIntersectionIterator will return rows in rowid order, too.

  All merged quick selects retrieve {rowid, covered_fields} tuples (not full
  table records).
  RowIDIntersectionIterator retrieves full records if it is not being used
  by RowIDUnionIterator and all merged quick selects together don't
  cover needed all fields.

  If one of the merged quick selects is a Clustered PK range scan, it is
  used only to filter rowid sequence produced by other merged quick selects.
*/

class RowIDIntersectionIterator : public RowIDCapableRowIterator {
 public:
  RowIDIntersectionIterator(
      THD *thd, MEM_ROOT *return_mem_root, TABLE *table_arg,
      bool retrieve_full_rows, bool need_rows_in_rowid_order,
      Mem_root_array<unique_ptr_destroy_only<RowIterator>> children,
      unique_ptr_destroy_only<RowIterator> cpk_child);
  ~RowIDIntersectionIterator() override;

  bool Init() override;
  int Read() override;
  uchar *last_rowid() const override {
    assert(need_rows_in_rowid_order);
    return m_last_rowid;
  }

 private:
  /*
    Range quick selects this intersection consists of, not including
    cpk_quick.
  */
  Mem_root_array<unique_ptr_destroy_only<RowIterator>> m_children;

  /*
    Merged quick select that uses Clustered PK, if there is one. This quick
    select is not used for row retrieval, it is used for row retrieval.
  */
  unique_ptr_destroy_only<RowIterator> m_cpk_child;

  /*
    If true, do retrieve full table rows.

    The way this works is somewhat convoluted; this is my (sgunders')
    understanding as of September 2021:

    For covering indexes (for some complicated value of “covering” if there are
    multiple indexes involved), we always use index-only scans; otherwise,
    the index range scan uses a normal scan (table->file->set_keyread(false)),
    which does first a lookup into the index, and then the secondary lookup to
    get the actual row.

    However, for intersection scans, we don't actually need all sub-scans to
    fetch the actual row; that's just a waste, especially since in most cases,
    we won't need the row. So in this case, the _intention_ is that we'd always
    turn on index-only scans, although it seems the code for this was never
    written. The idea is that the intersection iterator then is responsible for
    doing a kind of “fetch after the fact” once the intersection has yielded a
    row (unless we're covering). This is done by

      table->file->ha_rnd_pos(table->record[0], rowid);

    although index merge uses position() instead of ha_rnd_pos().
    Both seem to have the (undocumented?) side effect of actually fetching the
    row even on an index-only scan. This is the reason why we need the
    intersection iterator to reuse the handler reuse for MyISAM; otherwise, we'd
    never actually get the row, since it's stored privately in MI_INFO and not
    in the row ID.

    But if there's something above the intersection scan again (which can only
    be a union), it's the same game; when we find a row, it might be a duplicate
    of the same row ID from another sub-iterator of the union (whether a range
    scan or an intersection of range scans), and then it's not worth it to fetch
    the entire row. So that's why the intersection scan needs to be told “no,
    don't do ha_rnd_pos; your parent will be doing that if it's interested”. And
    that is what this variable is for.
   */
  bool retrieve_full_rows;

  /* in top-level quick select, true if merged scans where initialized */
  bool scans_inited;

  const bool need_rows_in_rowid_order;
  uchar *m_last_rowid;
  bool inited = false;

  bool init_ror_merged_scan();
};

/*
  Comparison function to be used RowIDUnionIterator::queue priority
  queue.
*/
struct Quick_ror_union_less {
  explicit Quick_ror_union_less(const handler *file) : m_file(file) {}
  bool operator()(RowIterator *a, RowIterator *b) {
    RowIDCapableRowIterator *real_a =
        down_cast<RowIDCapableRowIterator *>(a->real_iterator());
    RowIDCapableRowIterator *real_b =
        down_cast<RowIDCapableRowIterator *>(b->real_iterator());
    return m_file->cmp_ref(real_a->last_rowid(), real_b->last_rowid()) > 0;
  }
  const handler *m_file;
};

/*
  Rowid-Ordered Retrieval index union select.
  This quick select produces union of row sequences returned by several
  quick select it "merges".

  All merged quick selects must return rowids in rowid order.
  RowIDUnionIterator will return rows in rowid order, too.

  All merged quick selects are set not to retrieve full table records.
  ROR-union quick select always retrieves full records.

*/

class RowIDUnionIterator : public TableRowIterator {
 public:
  RowIDUnionIterator(
      THD *thd, MEM_ROOT *return_mem_root, TABLE *table,
      Mem_root_array<unique_ptr_destroy_only<RowIterator>> children);
  ~RowIDUnionIterator() override;

  bool Init() override;
  int Read() override;

 private:
  Mem_root_array<unique_ptr_destroy_only<RowIterator>> m_children;

  Priority_queue<RowIterator *,
                 std::vector<RowIterator *, Malloc_allocator<RowIterator *>>,
                 Quick_ror_union_less>
      queue; /* Priority queue for merge operation */

  MEM_ROOT *mem_root; /* Memory pool for this and merged quick selects data. */
  uchar *cur_rowid;   /* buffer used in Read() */
  uchar *prev_rowid;  /* rowid of last row returned by Read() */
  bool have_prev_rowid; /* true if prev_rowid has valid data */
  uint rowid_length;    /* table rowid length */

  bool scans_inited;
  bool inited = false;
};

#endif  // SQL_RANGE_OPTIMIZER_ROWID_ORDERED_RETRIEVAL_H_
