#ifndef SQL_RECORDS_H
#define SQL_RECORDS_H
/* Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <sys/types.h>
#include <memory>

#include "my_alloc.h"
#include "sql/basic_row_iterators.h"
#include "sql/ref_row_iterators.h"
#include "sql/row_iterator.h"
#include "sql/sorting_iterator.h"

class QEP_TAB;
class THD;
struct TABLE;

struct READ_RECORD {
  RowIterator *operator->() { return iterator.get(); }

  unique_ptr_destroy_only<RowIterator> iterator;

  // Holds one out of all RowIterator implementations (except the ones used
  // for filesort, which are in sort_holder), so that it is possible to
  // initialize a RowIterator without heap allocations. (The iterator
  // member typically points to this union, and is responsible for
  // running the right destructor.)
  union IteratorHolder {
    IteratorHolder() {}
    ~IteratorHolder() {}

    TableScanIterator table_scan;
    IndexScanIterator<true> index_scan_reverse;
    IndexScanIterator<false> index_scan;
    IndexRangeScanIterator index_range_scan;
    RefIterator<false> ref;
    RefIterator<true> ref_reverse;
    RefOrNullIterator ref_or_null;
    EQRefIterator eq_ref;
    ConstIterator const_table;
    FullTextSearchIterator fts;
    DynamicRangeIterator dynamic_range_scan;
    PushedJoinRefIterator pushed_join_ref;

    // Used for unique, for now.
    SortBufferIndirectIterator sort_buffer_indirect;
    SortFileIndirectIterator sort_file_indirect;
  } iterator_holder;

  // Same, when we have sorting. If we sort, SortingIterator will be
  // responsible for destroying the inner object, but the memory will still be
  // held in iterator_holder, so we can't put this in the union.
  char sort_holder[sizeof(SortingIterator)];

  // Same technique as sort_holder, when we have an AlternativeIterator.
  char alternative_holder[sizeof(AlternativeIterator)];
};

void setup_read_record(READ_RECORD *info, THD *thd, TABLE *table,
                       QEP_TAB *qep_tab, bool disable_rr_cache,
                       bool ignore_not_found_rows, ha_rows *examined_rows);

/** Calls setup_read_record(), then calls Init() on the resulting iterator. */
bool init_read_record(READ_RECORD *info, THD *thd, TABLE *table,
                      QEP_TAB *qep_tab, bool disable_rr_cache,
                      bool ignore_not_found_rows);

void setup_read_record_idx(READ_RECORD *info, THD *thd, TABLE *table, uint idx,
                           bool reverse, QEP_TAB *qep_tab);

#endif /* SQL_RECORDS_H */
