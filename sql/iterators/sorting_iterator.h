#ifndef SQL_ITERATORS_SORTING_ITERATOR_H_
#define SQL_ITERATORS_SORTING_ITERATOR_H_

/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include "my_alloc.h"
#include "my_base.h"
#include "my_table_map.h"
#include "sql/iterators/basic_row_iterators.h"
#include "sql/iterators/row_iterator.h"
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
  //
  // num_rows_estimate is used only for whether we intend to use the priority
  // queue optimization or not; if we estimate fewer rows than we can fit into
  // RAM, we never use the priority queue.
  SortingIterator(THD *thd, Filesort *filesort,
                  unique_ptr_destroy_only<RowIterator> source,
                  ha_rows num_rows_estimate, table_map tables_to_get_rowid_for,
                  ha_rows *examined_rows);
  ~SortingIterator() override;

  // Calls Init() on the source iterator, then does the actual sort.
  // NOTE: If you call Init() again, SortingIterator will actually
  // do a _new sort_, not just rewind the iterator. This is because a
  // Index_lookup we depend on may have changed so the produced record
  // set could be different from what we had last time.
  //
  // Currently, this isn't a big problem performance-wise, since we never
  // really sort the right-hand side of a join (we only sort the leftmost
  // table or the final result, and we don't have merge joins). However,
  // re-inits could very well happen in the case of a dependent subquery
  // that needs ORDER BY with LIMIT, so for correctness, we really need
  // the re-sort. Longer-term we should test whether the Index_lookup is
  // unchanged, and if so, just re-init the result iterator.
  bool Init() override;

  int Read() override { return m_result_iterator->Read(); }

  void SetNullRowFlag(bool is_null_row) override;

  void UnlockRow() override { m_result_iterator->UnlockRow(); }

  /// Optional (when JOIN::destroy() runs, the iterator and its buffers
  /// will be cleaned up anyway); used to clean up the buffers a little
  /// bit earlier.
  ///
  /// When we get cached JOIN objects (prepare/optimize once) that can
  /// live for a long time between queries, calling this will become more
  /// important.
  void CleanupAfterQuery();

  const Filesort *filesort() const { return m_filesort; }

 private:
  int DoSort();
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

  // Holds the buffers for m_sort_result.
  Filesort_info m_fs_info;

  Sort_result m_sort_result;

  const ha_rows m_num_rows_estimate;
  const table_map m_tables_to_get_rowid_for;
  ha_rows *m_examined_rows;

  // Holds one out of all RowIterator implementations we need so that it is
  // possible to initialize a RowIterator without heap allocations.
  // (m_result_iterator typically points to this union, and is responsible for
  // running the right destructor.)
  //
  // TODO: If we need to add TimingIterator directly on this iterator,
  // switch to allocating it on the MEM_ROOT.
  union IteratorHolder {
    IteratorHolder() {}   // NOLINT(modernize-use-equals-default)
    ~IteratorHolder() {}  // NOLINT(modernize-use-equals-default)

    SortBufferIterator<true> sort_buffer_packed_addons;
    SortBufferIterator<false> sort_buffer;
    SortBufferIndirectIterator sort_buffer_indirect;
    SortFileIterator<true> sort_file_packed_addons;
    SortFileIterator<false> sort_file;
    SortFileIndirectIterator sort_file_indirect;
  } m_result_iterator_holder;
};

#endif  // SQL_ITERATORS_SORTING_ITERATOR_H_
