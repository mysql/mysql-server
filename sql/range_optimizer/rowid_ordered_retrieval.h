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
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/range_optimizer/range_scan.h"
#include "sql/sql_list.h"
#include "sql/table.h"

class String;
class THD;
struct MY_BITMAP;

/*
  Rowid-Ordered Retrieval (ROR) index intersection quick select.
  This quick select produces intersection of row sequences returned
  by several QUICK_RANGE_SELECTs it "merges".

  All merged QUICK_RANGE_SELECTs must return rowids in rowid order.
  QUICK_ROR_INTERSECT_SELECT will return rows in rowid order, too.

  All merged quick selects retrieve {rowid, covered_fields} tuples (not full
  table records).
  QUICK_ROR_INTERSECT_SELECT retrieves full records if it is not being used
  by QUICK_ROR_INTERSECT_SELECT and all merged quick selects together don't
  cover needed all fields.

  If one of the merged quick selects is a Clustered PK range scan, it is
  used only to filter rowid sequence produced by other merged quick selects.
*/

class QUICK_ROR_INTERSECT_SELECT : public QUICK_SELECT_I {
 public:
  QUICK_ROR_INTERSECT_SELECT(TABLE *table, bool retrieve_full_rows,
                             MEM_ROOT *return_mem_root);
  ~QUICK_ROR_INTERSECT_SELECT() override;

  int init() override;
  void need_sorted_output() override { assert(false); /* Can't do it */ }
  int reset(void) override;
  int get_next() override;
  bool reverse_sorted() const override { return false; }
  bool reverse_sort_possible() const override { return false; }
  bool unique_key_range() override { return false; }
  RangeScanType get_type() const override { return QS_TYPE_ROR_INTERSECT; }
  bool is_loose_index_scan() const override { return false; }
  bool is_agg_loose_index_scan() const override { return false; }
  void add_keys_and_lengths(String *key_names, String *used_lengths) override;
  void add_info_string(String *str) override;
  bool is_keys_used(const MY_BITMAP *fields) override;
#ifndef NDEBUG
  void dbug_dump(int indent, bool verbose) override;
#endif
  int init_ror_merged_scan(bool reuse_handler) override;
  bool push_quick_back(QUICK_RANGE_SELECT *quick_sel_range);

  /*
    Range quick selects this intersection consists of, not including
    cpk_quick.
  */
  List<QUICK_RANGE_SELECT> quick_selects;

  void get_fields_used(MY_BITMAP *used_fields) override {
    List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
    QUICK_RANGE_SELECT *quick;
    while ((quick = it++)) quick->get_fields_used(used_fields);
  }

  MEM_ROOT *mem_root; /* Memory pool for this and merged quick selects data. */
  /*
    Merged quick select that uses Clustered PK, if there is one. This quick
    select is not used for row retrieval, it is used for row retrieval.
  */
  QUICK_RANGE_SELECT *cpk_quick;

  bool need_to_fetch_row; /* if true, do retrieve full table records. */
  /* in top-level quick select, true if merged scans where initialized */
  bool scans_inited;
};

/*
  Comparison function to be used QUICK_ROR_UNION_SELECT::queue priority
  queue.
*/
struct Quick_ror_union_less {
  explicit Quick_ror_union_less(const QUICK_SELECT_I *me) : m_me(me) {}
  bool operator()(QUICK_SELECT_I *a, QUICK_SELECT_I *b) {
    return m_me->m_table->file->cmp_ref(a->last_rowid, b->last_rowid) > 0;
  }
  const QUICK_SELECT_I *m_me;
};

/*
  Rowid-Ordered Retrieval index union select.
  This quick select produces union of row sequences returned by several
  quick select it "merges".

  All merged quick selects must return rowids in rowid order.
  QUICK_ROR_UNION_SELECT will return rows in rowid order, too.

  All merged quick selects are set not to retrieve full table records.
  ROR-union quick select always retrieves full records.

*/

class QUICK_ROR_UNION_SELECT : public QUICK_SELECT_I {
 public:
  QUICK_ROR_UNION_SELECT(MEM_ROOT *return_mem_root, TABLE *table);
  ~QUICK_ROR_UNION_SELECT() override;

  int init() override;
  void need_sorted_output() override { assert(false); /* Can't do it */ }
  int reset(void) override;
  int get_next() override;
  bool reverse_sorted() const override { return false; }
  bool reverse_sort_possible() const override { return false; }
  bool unique_key_range() override { return false; }
  RangeScanType get_type() const override { return QS_TYPE_ROR_UNION; }
  bool is_loose_index_scan() const override { return false; }
  bool is_agg_loose_index_scan() const override { return false; }
  void add_keys_and_lengths(String *key_names, String *used_lengths) override;
  void add_info_string(String *str) override;
  bool is_keys_used(const MY_BITMAP *fields) override;
#ifndef NDEBUG
  void dbug_dump(int indent, bool verbose) override;
#endif

  bool push_quick_back(QUICK_SELECT_I *quick_sel_range);

  List<QUICK_SELECT_I> quick_selects; /* Merged quick selects */

  void get_fields_used(MY_BITMAP *used_fields) override {
    List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
    QUICK_SELECT_I *quick;
    while ((quick = it++)) quick->get_fields_used(used_fields);
  }

  Priority_queue<
      QUICK_SELECT_I *,
      std::vector<QUICK_SELECT_I *, Malloc_allocator<QUICK_SELECT_I *>>,
      Quick_ror_union_less>
      queue; /* Priority queue for merge operation */

  MEM_ROOT *mem_root; /* Memory pool for this and merged quick selects data. */
  uchar *cur_rowid;   /* buffer used in get_next() */
  uchar *prev_rowid;  /* rowid of last row returned by get_next() */
  bool have_prev_rowid; /* true if prev_rowid has valid data */
  uint rowid_length;    /* table rowid length */

 private:
  bool scans_inited;
};

#endif  // SQL_RANGE_OPTIMIZER_ROWID_ORDERED_RETRIEVAL_H_
