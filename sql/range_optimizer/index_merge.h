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

#ifndef SQL_RANGE_OPTIMIZER_INDEX_MERGE_H_
#define SQL_RANGE_OPTIMIZER_INDEX_MERGE_H_

#include <assert.h>

#include "my_alloc.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/range_optimizer/range_scan.h"
#include "sql/sql_list.h"

class RowIterator;
class String;
class Unique;
struct MY_BITMAP;
struct TABLE;

/*
  QUICK_INDEX_MERGE_SELECT - index_merge access method quick select.

    QUICK_INDEX_MERGE_SELECT uses
     * QUICK_RANGE_SELECTs to get rows
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

    Phase 1 (implemented in QUICK_INDEX_MERGE_SELECT::prepare_unique):
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

    Phase 2 (implemented as sequence of QUICK_INDEX_MERGE_SELECT::get_next
    calls):

    fetch()
    {
      retrieve all rows from row pointers stored in Unique;
      free Unique;
      retrieve all rows for CPK scan;
    }
*/

class QUICK_INDEX_MERGE_SELECT : public QUICK_SELECT_I {
  Unique *unique;

 public:
  QUICK_INDEX_MERGE_SELECT(MEM_ROOT *mem_root, TABLE *table);
  ~QUICK_INDEX_MERGE_SELECT() override;

  int init() override;
  void need_sorted_output() override { assert(false); /* Can't do it */ }
  int reset(void) override;
  int get_next() override;
  bool reverse_sorted() const override { return false; }
  bool reverse_sort_possible() const override { return false; }
  bool unique_key_range() override { return false; }
  RangeScanType get_type() const override { return QS_TYPE_INDEX_MERGE; }
  bool is_loose_index_scan() const override { return false; }
  bool is_agg_loose_index_scan() const override { return false; }
  void add_keys_and_lengths(String *key_names, String *used_lengths) override;
  void add_info_string(String *str) override;
  bool is_keys_used(const MY_BITMAP *fields) override;
#ifndef NDEBUG
  void dbug_dump(int indent, bool verbose) override;
#endif

  bool push_quick_back(QUICK_RANGE_SELECT *quick_sel_range);

  /* range quick selects this index_merge read consists of */
  List<QUICK_RANGE_SELECT> quick_selects;

  /* quick select that uses clustered primary key (NULL if none) */
  QUICK_RANGE_SELECT *pk_quick_select;

  /* true if this select is currently doing a clustered PK scan */
  bool doing_pk_scan;

  int read_keys_and_merge();

  void get_fields_used(MY_BITMAP *used_fields) override {
    List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
    QUICK_RANGE_SELECT *quick;
    while ((quick = it++)) quick->get_fields_used(used_fields);

    if (pk_quick_select) pk_quick_select->get_fields_used(used_fields);
  }

  /* used to get rows collected in Unique */
  unique_ptr_destroy_only<RowIterator> read_record;

 private:
  MEM_ROOT *mem_root;
};

#endif  // SQL_RANGE_OPTIMIZER_INDEX_MERGE_H_
