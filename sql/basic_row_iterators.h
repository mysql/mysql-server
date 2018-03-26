#ifndef SQL_BASIC_ROW_ITERATORS_H_
#define SQL_BASIC_ROW_ITERATORS_H_

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

/**
  @file
  Row iterators that scan a single table without reference to other tables
  or iterators.
 */

#include <sys/types.h>
#include <memory>

#include "map_helpers.h"
#include "my_alloc.h"
#include "my_inttypes.h"
#include "sql/row_iterator.h"

class Filesort_info;
class QEP_TAB;
class QUICK_SELECT_I;
class Sort_result;
class THD;
class handler;
struct IO_CACHE;
struct TABLE;

/**
  Scan a table from beginning to end.

  This is the most basic access method of a table using rnd_init,
  ha_rnd_next and rnd_end. No indexes are used.
 */
class TableScanIterator final : public RowIterator {
 public:
  TableScanIterator(THD *thd, TABLE *table);
  ~TableScanIterator();

  // Accepts nullptr for qep_tab; qep_tab is used only for condition pushdown
  // and setting up record buffers.
  bool Init(QEP_TAB *qep_tab) override;
  int Read() override;

 private:
  uchar *const m_record;
};

/** Perform a full index scan along an index. */
template <bool Reverse>
class IndexScanIterator final : public RowIterator {
 public:
  // use_order must be set to true if you actually need to get the records
  // back in index order. It can be set to false if you wish to scan
  // using the index (e.g. for an index-only scan of the entire table),
  // but do not actually care about the order. In particular, partitioned
  // tables can use this to deliver more efficient scans.
  IndexScanIterator(THD *thd, TABLE *table, int idx, bool use_order);
  ~IndexScanIterator();

  // Accepts nullptr for qep_tab; qep_tab is used only for condition pushdown
  // and setting up record buffers.
  bool Init(QEP_TAB *qep_tab) override;
  int Read() override;

 private:
  uchar *const m_record;
  const int m_idx;
  const bool m_use_order;
  bool m_first = true;
};

/**
  Scan a given range of the table (a “quick”), using an index.

  rr_quick uses one of the QUICK_SELECT classes in opt_range.cc to
  perform an index scan. There are loads of functionality hidden
  in these quick classes. It handles all index scans of various kinds.

  TODO: Convert the QUICK_SELECT framework to RowIterator, so that
  we do not need this adapter.
 */
class IndexRangeScanIterator final : public RowIterator {
 public:
  // Does _not_ take ownership of "quick" (but maybe it should).
  IndexRangeScanIterator(THD *thd, TABLE *table, QUICK_SELECT_I *quick);

  // Accepts nullptr for qep_tab; qep_tab is used only for condition pushdown
  // and setting up record buffers.
  bool Init(QEP_TAB *qep_tab) override;
  int Read() override;

 private:
  // NOTE: No destructor; quick_range will call ha_index_or_rnd_end() for us.
  QUICK_SELECT_I *const m_quick;
};

// Readers relating to reading sorted data (from filesort).
//
// Filesort will produce references to the records sorted; these
// references can be stored in memory or in a temporary file.
//
// The temporary file is normally used when the references doesn't fit into
// a properly sized memory buffer. For most small queries the references
// are stored in the memory buffer.
//
// The temporary file is also used when performing an update where a key is
// modified.

/**
  Fetch the records from a memory buffer.

  This method is used when table->sort.addon_field is allocated.
  This is allocated for most SELECT queries not involving any BLOB's.
  In this case the records are fetched from a memory buffer.
 */
template <bool Packed_addon_fields>
class SortBufferIterator final : public RowIterator {
 public:
  SortBufferIterator(THD *thd, TABLE *table, Filesort_info *sort,
                     Sort_result *sort_result);
  ~SortBufferIterator();

  // Accepts nullptr for qep_tab (obviously).
  bool Init(QEP_TAB *) override;
  int Read() override;

 private:
  // NOTE: No m_record -- unpacks directly into each Field's field->ptr.
  Filesort_info *const m_sort;
  Sort_result *const m_sort_result;
  unsigned m_unpack_counter;
};

/**
  Fetch the record IDs from a memory buffer, but the records themselves from
  the table on disk.

  Used when the above (comment on SortBufferIterator) is not true, UPDATE,
  DELETE and so forth and SELECT's involving BLOB's. It is also used when the
  addon_field buffer is not allocated due to that its size was bigger than the
  session variable max_length_for_sort_data. Finally, it is used for the
  result of Unique, which returns row IDs in the same format as filesort.
  In this case the record data is fetched from the handler using the saved
  reference using the rnd_pos handler call.
 */
class SortBufferIndirectIterator final : public RowIterator {
 public:
  // Ownership here is suboptimal: Takes only partial ownership of
  // "sort_result", so it must be alive for as long as the RowIterator is.
  // However, it _does_ free the buffers within on destruction.
  SortBufferIndirectIterator(THD *thd, TABLE *table, Sort_result *sort_result,
                             bool ignore_not_found_rows);
  ~SortBufferIndirectIterator();
  bool Init(QEP_TAB *qep_tab) override;
  int Read() override;

 private:
  Sort_result *const m_sort_result;
  const uint m_ref_length;
  uchar *m_record = nullptr;
  uchar *m_cache_pos = nullptr, *m_cache_end = nullptr;
  bool m_ignore_not_found_rows;
};

/**
  Fetch the records from a tempoary file.

  There used to be a comment here saying “should obviously not really happen
  other than in strange configurations”, but especially with packed addons
  and InnoDB (where fetching rows needs a primary key lookup), it's not
  necessarily suboptimal compared to e.g. SortBufferIndirectIterator.
 */
template <bool Packed_addon_fields>
class SortFileIterator final : public RowIterator {
 public:
  // Takes ownership of tempfile.
  SortFileIterator(THD *thd, TABLE *table, IO_CACHE *tempfile,
                   Filesort_info *sort);
  ~SortFileIterator();

  // Accepts nullptr for qep_tab (obviously).
  bool Init(QEP_TAB *) override { return false; }
  int Read() override;

 private:
  uchar *const m_rec_buf;
  const uint m_ref_length;
  IO_CACHE *const m_io_cache;
  Filesort_info *const m_sort;
};

/**
  Fetch the record IDs from a temporary file, then the records themselves from
  the table on disk.

  Same as SortBufferIndirectIterator except that references are fetched
  from temporary file instead of from a memory buffer. So first the record IDs
  are read from file, then those record IDs are used to look up rows in the
  table.
 */
class SortFileIndirectIterator final : public RowIterator {
 public:
  // Takes ownership of tempfile.
  SortFileIndirectIterator(THD *thd, TABLE *table, IO_CACHE *tempfile,
                           bool request_cache, bool ignore_not_found_rows);
  ~SortFileIndirectIterator();

  // Accepts nullptr for qep_tab; qep_tab is used only for condition pushdown.
  bool Init(QEP_TAB *qep_tab) override;
  int Read() override;

 private:
  bool InitCache();
  int CachedRead();
  int UncachedRead();

  IO_CACHE *m_io_cache = nullptr;
  uchar *m_record = nullptr;
  uchar *m_ref_pos = nullptr; /* pointer to form->refpos */
  bool m_ignore_not_found_rows;

  // This is a special variant that can be used for
  // handlers that is not using the HA_FAST_KEY_READ table flag. Instead
  // of reading the references one by one from the temporary file it reads
  // a set of them, sorts them and reads all of them into a buffer which
  // is then used for a number of subsequent calls to Read().
  // It is only used for SELECT queries and a number of other conditions
  // on table size.
  bool m_using_cache;
  uint m_cache_records;
  uint m_ref_length, m_struct_length, m_reclength, m_rec_cache_size,
      m_error_offset;
  unique_ptr_my_free<uchar[]> m_cache;
  uchar *m_cache_pos = nullptr, *m_cache_end = nullptr,
        *m_read_positions = nullptr;
};

#endif  // SQL_BASIC_ROW_ITERATORS_H_
