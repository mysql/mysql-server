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

#ifndef SQL_RANGE_OPTIMIZER_INDEX_MERGE_H_
#define SQL_RANGE_OPTIMIZER_INDEX_MERGE_H_

#include <assert.h>

#include "my_alloc.h"
#include "sql/range_optimizer/index_range_scan.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/sql_list.h"

class RowIterator;
class String;
class Unique;
struct MY_BITMAP;
struct TABLE;

/*
  IndexMergeIterator - index_merge access method quick select.

    IndexMergeIterator uses
     * IndexRangeScanIterators to get rows
     * Unique class to remove duplicate rows

  INDEX MERGE OPTIMIZER
    Current implementation doesn't detect all cases where index_merge could
    be used, in particular:
     * index_merge will never be used if range scan is possible (even if
       range scan is more expensive)

     * index_merge+'using index' is not supported (this the consequence of
       the above restriction)

     * If WHERE part contains complex nested AND and OR conditions, some ways
       to retrieve rows using index_merge will not be considered. The choice
       of read plan may depend on the order of conjuncts/disjuncts in WHERE
       part of the query, see comments near imerge_list_or_list and
       SEL_IMERGE::or_sel_tree_with_checks functions for details.

     * There is no "index_merge_ref" method (but index_merge on non-first
       table in join is possible with 'range checked for each record').

    See comments around SEL_IMERGE class and test_quick_select for more
    details.

  ROW RETRIEVAL ALGORITHM

    index_merge uses Unique class for duplicates removal.  index_merge takes
    advantage of Clustered Primary Key (CPK) if the table has one.
    The index_merge algorithm consists of two phases:

    Phase 1 (implemented in IndexMergeIterator::prepare_unique):
    prepare()
    {
      activate 'index only';
      while(retrieve next row for non-CPK scan)
      {
        if (there is a CPK scan and row will be retrieved by it)
          skip this row;
        else
          put its rowid into Unique;
      }
      deactivate 'index only';
    }

    Phase 2 (implemented as sequence of IndexMergeIterator::Read()
    calls):

    fetch()
    {
      retrieve all rows from row pointers stored in Unique;
      free Unique;
      retrieve all rows for CPK scan;
    }
*/

class IndexMergeIterator : public TableRowIterator {
 public:
  // NOTE: Both pk_quick_select (if non-nullptr) and all children must be
  // of the type IndexRangeScanIterator, possibly wrapped in a TimingIterator.
  IndexMergeIterator(
      THD *thd, MEM_ROOT *mem_root, TABLE *table,
      unique_ptr_destroy_only<RowIterator> pk_quick_select,
      Mem_root_array<unique_ptr_destroy_only<RowIterator>> children);
  ~IndexMergeIterator() override;

  bool Init() override;
  int Read() override;

 private:
  unique_ptr_destroy_only<Unique> unique;

  /* used to get rows collected in Unique */
  unique_ptr_destroy_only<RowIterator> read_record;

  /* quick select that uses clustered primary key (NULL if none) */
  unique_ptr_destroy_only<RowIterator> pk_quick_select;

  /* range quick selects this index_merge read consists of */
  Mem_root_array<unique_ptr_destroy_only<RowIterator>> m_children;

  /* true if this select is currently doing a clustered PK scan */
  bool doing_pk_scan;

  MEM_ROOT *mem_root;
};

#endif  // SQL_RANGE_OPTIMIZER_INDEX_MERGE_H_
