#ifndef SQL_SORTING_ITERATOR_H_
#define SQL_SORTING_ITERATOR_H_

/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <memory>
#include <string>
#include <vector>

#include "my_base.h"
#include "sql/basic_row_iterators.h"
#include "sql/row_iterator.h"
#include "sql/sql_sort.h"

class Filesort;
class QEP_TAB;
class THD;

/**
  An adapter that takes in another RowIterator and produces the same rows,
  just in sorted order. (The actual sort happens in Init().) Unfortunately, it
  is still bound to working off a TABLE object, which means that you can't
  use it to e.g. sort the output of a join without materializing into a
  temporary table first (ignoring that we currently have no Iterators for
  joins).

  The primary reason for this is that we currently have no way of communicating
  read sets through Iterators, and SortingIterator needs to add fields used in
  ORDER BY to the read set for the appropriate tables. This could be mitigated
  by e.g. sending in an unordered_set<Field *>, but we don't currently have
  such a mechanism.
 */
class SortingIterator final : public RowIterator {
 public:
  // Does not take ownership of "filesort", which must live for at least
  // as long as SortingIterator lives (since Init() may be called multiple
  // times). It _does_ take ownership of "source", and is responsible for
  // calling Init() on it, but does not hold the memory.
  // "examined_rows", if not nullptr, is incremented for each successful Read().
  SortingIterator(THD *thd, Filesort *filesort,
                  unique_ptr_destroy_only<RowIterator> source,
                  ha_rows *examined_rows);
  ~SortingIterator() override;

  // Calls Init() on the source iterator, then does the actual sort.
  // NOTE: If you call Init() again, SortingIterator will actually
  // do a _new sort_, not just rewind the iterator. This is because a
  // TABLE_REF we depend on may have changed so the produced record set could be
  // different from what we had last time.
  //
  // Currently, this isn't a big problem performance-wise, since we never
  // really sort the right-hand side of a join (we only sort the leftmost
  // table or the final result, and we don't have merge joins). However,
  // re-inits could very well happen in the case of a dependent subquery
  // that needs ORDER BY with LIMIT, so for correctness, we really need
  // the re-sort. Longer-term we should test whether the TABLE_REF is
  // unchanged, and if so, just re-init the result iterator.
  bool Init() override;

  int Read() override { return m_result_iterator->Read(); }

  void SetNullRowFlag(bool is_null_row) override {
    m_result_iterator->SetNullRowFlag(is_null_row);
  }

  void UnlockRow() override { m_result_iterator->UnlockRow(); }

  std::vector<Child> children() const override {
    return std::vector<Child>{{m_source_iterator.get(), ""}};
  }

  std::vector<std::string> DebugString() const override;

 private:
  int DoSort(QEP_TAB *qep_tab);
  void ReleaseBuffers();

  Filesort *m_filesort;

  // The iterator we are reading records from. We don't read from it
  // after Init() is done, but we may read from the TABLE it wraps,
  // so we don't destroy it until our own destructor.
  unique_ptr_destroy_only<RowIterator> m_source_iterator;

  // The actual iterator of sorted records, populated in Init();
  // Read() only proxies to this. Always points to one of the members
  // in m_result_iterator_holder; the type can be different depending on
  // e.g. whether the sort result fit into memory or not, whether we are
  // using packed addons, etc..
  unique_ptr_destroy_only<RowIterator> m_result_iterator;

  Sort_result m_sort_result;

  ha_rows *m_examined_rows;

  // Similarly to iterator_holder in READ_RECORD, allows us to keep one of
  // the different forms of result iterators without incurring a heap
  // allocation.
  union IteratorHolder {
    IteratorHolder() {}
    ~IteratorHolder() {}

    SortBufferIterator<true> sort_buffer_packed_addons;
    SortBufferIterator<false> sort_buffer;
    SortBufferIndirectIterator sort_buffer_indirect;
    SortFileIterator<true> sort_file_packed_addons;
    SortFileIterator<false> sort_file;
    SortFileIndirectIterator sort_file_indirect;
  } m_result_iterator_holder;
};

#endif  // SQL_SORTING_ITERATOR_H_
