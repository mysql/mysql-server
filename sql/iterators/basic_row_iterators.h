#ifndef SQL_ITERATORS_BASIC_ROW_ITERATORS_H_
#define SQL_ITERATORS_BASIC_ROW_ITERATORS_H_

/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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

#include <assert.h>
#include <sys/types.h>

#include "mem_root_deque.h"
#include "my_base.h"
#include "my_inttypes.h"
#include "sql/iterators/row_iterator.h"
#include "sql/mem_root_array.h"

class Filesort_info;
class Item;
class Item_values_column;
class JOIN;
class QUICK_RANGE;
class Sort_result;
class THD;
struct IO_CACHE;
struct TABLE;

/**
  Scan a table from beginning to end.

  This is the most basic access method of a table using rnd_init,
  ha_rnd_next and rnd_end. No indexes are used.
*/
class TableScanIterator final : public TableRowIterator {
 public:
  /**
    @param thd     session context
    @param table   table to be scanned. Notice that table may be a temporary
                   table that represents a set operation (UNION, INTERSECT or
                   EXCEPT). For the latter two, the counter field must be
                   interpreted by TableScanIterator::Read in order to give the
                   correct result set, but this is invisible to the consumer.
    @param expected_rows is used for scaling the record buffer.
                   If zero or less, no record buffer will be set up.
    @param examined_rows if not nullptr, is incremented for each successful
                   Read().
  */
  TableScanIterator(THD *thd, TABLE *table, double expected_rows,
                    ha_rows *examined_rows);
  ~TableScanIterator() override;

  bool Init() override;
  int Read() override;

 private:
  uchar *const m_record;
  const double m_expected_rows;
  ha_rows *const m_examined_rows;
  /// Used to keep track of how many more duplicates of the last read row that
  /// remains to be written to the next stage: used for EXCEPT and INTERSECT
  /// computation: we only ever materialize one row even if there are
  /// duplicates of it, but with a counter, cf TABLE::m_set_counter. When we
  /// start scanning we must produce as many duplicates as ALL semantics
  /// mandate, so we initialize m_examined_rows based on TABLE::m_set_counter
  /// and decrement for each row we emit, so as to produce the correct number
  /// of duplicates for the next stage.
  ulonglong m_remaining_dups{0};
  /// Used for EXCEPT and INTERSECT only: we cannot enforce limit during
  /// materialization as for UNION and single table, so we have to do it during
  /// the scan.
  const ha_rows m_limit_rows;
  /// Used for EXCEPT and INTERSECT only: rows scanned so far, see also
  /// m_limit_rows.
  ha_rows m_stored_rows{0};
};

/** Perform a full index scan along an index. */
template <bool Reverse>
class IndexScanIterator final : public TableRowIterator {
 public:
  // use_order must be set to true if you actually need to get the records
  // back in index order. It can be set to false if you wish to scan
  // using the index (e.g. for an index-only scan of the entire table),
  // but do not actually care about the order. In particular, partitioned
  // tables can use this to deliver more efficient scans.
  //
  // “expected_rows” is used for scaling the record buffer.
  // If zero or less, no record buffer will be set up.
  //
  // The pushed condition can be nullptr.
  //
  // "examined_rows", if not nullptr, is incremented for each successful Read().
  IndexScanIterator(THD *thd, TABLE *table, int idx, bool use_order,
                    double expected_rows, ha_rows *examined_rows);
  ~IndexScanIterator() override;

  bool Init() override;
  int Read() override;

 private:
  uchar *const m_record;
  const int m_idx;
  const bool m_use_order;
  const double m_expected_rows;
  ha_rows *const m_examined_rows;
  bool m_first = true;
};

/**
  Perform a distance index scan along an index.
  For now it is just like the IndexScanIterator, waiting for innodb
  implementation of distance index scan functions
*/
class IndexDistanceScanIterator final : public TableRowIterator {
 public:
  // “expected_rows” is used for scaling the record buffer.
  // If zero or less, no record buffer will be set up.
  //
  // The pushed condition can be nullptr.
  //
  // "examined_rows", if not nullptr, is incremented for each successful Read().
  IndexDistanceScanIterator(THD *thd, TABLE *table, int idx,
                            QUICK_RANGE *query_mbr, double expected_rows,
                            ha_rows *examined_rows);
  ~IndexDistanceScanIterator() override;

  bool Init() override;
  int Read() override;

 private:
  uchar *const m_record;
  const int m_idx;
  QUICK_RANGE *m_query_mbr;
  const double m_expected_rows;
  ha_rows *const m_examined_rows;
  bool m_first = true;
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
  // "examined_rows", if not nullptr, is incremented for each successful Read().
  // The table is used solely for NULL row flags.
  SortBufferIterator(THD *thd, Mem_root_array<TABLE *> tables,
                     Filesort_info *sort, Sort_result *sort_result,
                     ha_rows *examined_rows);
  ~SortBufferIterator() override;

  bool Init() override;
  int Read() override;
  void UnlockRow() override {}
  void SetNullRowFlag(bool) override {
    // Handled by SortingIterator.
    assert(false);
  }

 private:
  // NOTE: No m_record -- unpacks directly into each Field's field->ptr.
  Filesort_info *const m_sort;
  Sort_result *const m_sort_result;
  unsigned m_unpack_counter;
  ha_rows *const m_examined_rows;
  Mem_root_array<TABLE *> m_tables;
};

/**
  Fetch the record IDs from a memory buffer, but the records themselves from
  the table on disk.

  Used when the above (comment on SortBufferIterator) is not true, UPDATE,
  DELETE and so forth and SELECT's involving large BLOBs. It is also used for
  the result of Unique, which returns row IDs in the same format as filesort.
  In this case the record data is fetched from the handler using the saved
  reference using the rnd_pos handler call.
 */
class SortBufferIndirectIterator final : public RowIterator {
 public:
  // Ownership here is suboptimal: Takes only partial ownership of
  // "sort_result", so it must be alive for as long as the RowIterator is.
  // However, it _does_ free the buffers within on destruction.
  //
  // The pushed condition can be nullptr.
  //
  // "examined_rows", if not nullptr, is incremented for each successful Read().
  SortBufferIndirectIterator(THD *thd, Mem_root_array<TABLE *> tables,
                             Sort_result *sort_result,
                             bool ignore_not_found_rows, bool has_null_flags,
                             ha_rows *examined_rows);
  ~SortBufferIndirectIterator() override;
  bool Init() override;
  int Read() override;
  void SetNullRowFlag(bool) override {
    // Handled by SortingIterator.
    assert(false);
  }
  void UnlockRow() override {}

 private:
  Sort_result *const m_sort_result;
  Mem_root_array<TABLE *> m_tables;
  uint m_sum_ref_length;
  ha_rows *const m_examined_rows;
  uchar *m_cache_pos = nullptr, *m_cache_end = nullptr;
  bool m_ignore_not_found_rows;
  bool m_has_null_flags;
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
  // The table is used solely for NULL row flags.
  SortFileIterator(THD *thd, Mem_root_array<TABLE *> tables, IO_CACHE *tempfile,
                   Filesort_info *sort, ha_rows *examined_rows);
  ~SortFileIterator() override;

  bool Init() override { return false; }
  int Read() override;
  void UnlockRow() override {}
  void SetNullRowFlag(bool) override {
    // Handled by SortingIterator.
    assert(false);
  }

 private:
  uchar *const m_rec_buf;
  const uint m_buf_length;
  Mem_root_array<TABLE *> m_tables;
  IO_CACHE *const m_io_cache;
  Filesort_info *const m_sort;
  ha_rows *const m_examined_rows;
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
  //
  // The pushed condition can be nullptr.
  //
  // "examined_rows", if not nullptr, is incremented for each successful Read().
  SortFileIndirectIterator(THD *thd, Mem_root_array<TABLE *> tables,
                           IO_CACHE *tempfile, bool ignore_not_found_rows,
                           bool has_null_flags, ha_rows *examined_rows);
  ~SortFileIndirectIterator() override;

  bool Init() override;
  int Read() override;
  void SetNullRowFlag(bool) override {
    // Handled by SortingIterator.
    assert(false);
  }
  void UnlockRow() override {}

 private:
  IO_CACHE *m_io_cache = nullptr;
  ha_rows *const m_examined_rows;
  Mem_root_array<TABLE *> m_tables;
  uchar *m_ref_pos = nullptr;
  bool m_ignore_not_found_rows;
  bool m_has_null_flags;

  uint m_sum_ref_length;
};

// Used when the plan is const, ie. is known to contain a single row
// (and all values have been read in advance, so we don't need to read
// a single table).
class FakeSingleRowIterator final : public RowIterator {
 public:
  // "examined_rows", if not nullptr, is incremented for each successful Read().
  FakeSingleRowIterator(THD *thd, ha_rows *examined_rows)
      : RowIterator(thd), m_examined_rows(examined_rows) {}

  bool Init() override {
    m_has_row = true;
    return false;
  }

  int Read() override {
    if (m_has_row) {
      m_has_row = false;
      if (m_examined_rows != nullptr) {
        ++*m_examined_rows;
      }
      return 0;
    } else {
      return -1;
    }
  }

  void SetNullRowFlag(bool is_null_row [[maybe_unused]]) override {
    assert(!is_null_row);
  }

  void UnlockRow() override {}

 private:
  bool m_has_row;
  ha_rows *const m_examined_rows;
};

/**
  An iterator for unqualified COUNT(*) (ie., no WHERE, no join conditions,
  etc.), taking a special fast path in the handler. It returns a single row,
  much like FakeSingleRowIterator; however, unlike said iterator, it actually
  does the counting in Read() instead of expecting all fields to already be
  filled out.
 */
class UnqualifiedCountIterator final : public RowIterator {
 public:
  UnqualifiedCountIterator(THD *thd, JOIN *join)
      : RowIterator(thd), m_join(join) {}

  bool Init() override {
    m_has_row = true;
    return false;
  }

  int Read() override;

  void SetNullRowFlag(bool) override { assert(false); }

  void UnlockRow() override {}

 private:
  bool m_has_row;
  JOIN *const m_join;
};

/**
  A simple iterator that takes no input and produces zero output rows.
  Used when the optimizer has figured out ahead of time that a given table
  can produce no output (e.g. SELECT ... WHERE 2+2 = 5).

  The iterator can optionally have an array of the tables that are pruned away
  from the join tree by this iterator. It is only required when the iterator is
  on the inner side of an outer join, in which case it needs it in order to
  NULL-complement the rows in SetNullRowFlag().
 */
class ZeroRowsIterator final : public RowIterator {
 public:
  ZeroRowsIterator(THD *thd, Mem_root_array<TABLE *> pruned_tables);

  bool Init() override { return false; }

  int Read() override { return -1; }

  void SetNullRowFlag(bool is_null_row) override;

  void UnlockRow() override {}

 private:
  const Mem_root_array<TABLE *> m_pruned_tables;
};

/**
  Like ZeroRowsIterator, but produces a single output row, since there are
  aggregation functions present and no GROUP BY. E.g.,

    SELECT SUM(f1) FROM t1 WHERE 2+2 = 5;

  should produce a single row, containing only the value NULL.
 */
class ZeroRowsAggregatedIterator final : public RowIterator {
 public:
  // "examined_rows", if not nullptr, is incremented for each successful Read().
  ZeroRowsAggregatedIterator(THD *thd, JOIN *join, ha_rows *examined_rows)
      : RowIterator(thd), m_join(join), m_examined_rows(examined_rows) {}

  bool Init() override {
    m_has_row = true;
    return false;
  }

  int Read() override;

  void SetNullRowFlag(bool) override { assert(false); }

  void UnlockRow() override {}

 private:
  bool m_has_row;
  JOIN *const m_join;
  ha_rows *const m_examined_rows;
};

/**
  FollowTailIterator is a special version of TableScanIterator that is used
  as part of WITH RECURSIVE queries. It is designed to read from a temporary
  table at the same time as MaterializeIterator writes to the same table,
  picking up new records in the order they come in -- it follows the tail,
  much like the UNIX tool “tail -f”.

  Furthermore, when materializing a recursive query expression consisting of
  multiple query blocks, MaterializeIterator needs to run each block several
  times until convergence. (For a single query block, one iteration suffices,
  since the iterator sees new records as they come in.) Each such run, the
  recursive references should see only rows that were added since the last
  iteration, even though Init() is called anew. FollowTailIterator is thus
  different from TableScanIterator in that subsequent calls to Init() do not
  move the cursor back to the start.

  In addition, FollowTailIterator implements the WITH RECURSIVE iteration limit.
  This is not specified in terms of Init() calls, since one run can encompass
  many iterations. Instead, it keeps track of the number of records in the table
  at the start of iteration, and when it has read all of those records, the next
  iteration is deemed to have begun. If the iteration counter is above the
  user-set limit, it raises an error to stop runaway queries with infinite
  recursion.
 */
class FollowTailIterator final : public TableRowIterator {
 public:
  // "examined_rows", if not nullptr, is incremented for each successful Read().
  FollowTailIterator(THD *thd, TABLE *table, double expected_rows,
                     ha_rows *examined_rows);
  ~FollowTailIterator() override;

  bool Init() override;
  int Read() override;

  /**
    Signal where we can expect to find the number of generated rows for this
    materialization (this points into the MaterializeIterator's data).

    This must be called when we start materializing the CTE,
    before Init() runs.
   */
  void set_stored_rows_pointer(ha_rows *stored_rows) {
    m_stored_rows = stored_rows;
  }

  /**
    Signal to the iterator that the underlying table was closed and replaced
    with an InnoDB table with the same data, due to a spill-to-disk
    (e.g. the table used to be MEMORY and now is InnoDB). This is
    required so that Read() can continue scanning from the right place.
    Called by MaterializeIterator::MaterializeRecursive().
   */
  bool RepositionCursorAfterSpillToDisk();

 private:
  bool m_inited = false;
  uchar *const m_record;
  const double m_expected_rows;
  ha_rows *const m_examined_rows;
  ha_rows m_read_rows;
  ha_rows m_end_of_current_iteration;
  unsigned m_recursive_iteration_count;

  // Points into MaterializeIterator's data; set by BeginMaterialization() only.
  ha_rows *m_stored_rows = nullptr;
};

/**
  TableValueConstructor is the iterator for the table value constructor case of
  a query_primary (i.e. queries of the form VALUES row_list; e.g. VALUES ROW(1,
  10), ROW(2, 20)).

  The iterator is passed the field list of its parent JOIN object, which may
  contain Item_values_column objects that are created during
  Query_block::prepare_values(). This is required so that Read() can replace the
  currently selected row by simply changing the references of Item_values_column
  objects to the next row.

  The iterator must output multiple rows without being materialized, and does
  not scan any tables. The indirection of Item_values_column is required, as the
  executor outputs what is contained in join->fields (either directly, or
  indirectly through ConvertItemsToCopy), and is thus responsible for ensuring
  that join->fields contains the correct next row.
 */
class TableValueConstructorIterator final : public RowIterator {
 public:
  TableValueConstructorIterator(
      THD *thd, ha_rows *examined_rows,
      const mem_root_deque<mem_root_deque<Item *> *> &row_value_list,
      Mem_root_array<Item_values_column *> *output_refs);

  bool Init() override;
  int Read() override;

  void SetNullRowFlag(bool) override { assert(false); }

  void UnlockRow() override {}

 private:
  ha_rows *const m_examined_rows{nullptr};

  /// Contains the row values that are part of a VALUES clause. Read() will
  /// modify contained Item objects during execution by calls to is_null() and
  /// the required val function to extract its value.
  const mem_root_deque<mem_root_deque<Item *> *> &m_row_value_list;
  mem_root_deque<mem_root_deque<Item *> *>::const_iterator m_row_it;

  /// References to the row we currently want to output. When multiple rows must
  /// be output, this contains Item_values_column objects. In this case, each
  /// call to Read() will replace its current reference with the next row.
  /// It is nullptr if there is only one row.
  Mem_root_array<Item_values_column *> *m_output_refs;
};

/**
  Auxiliary class to squeeze two 32 bits integers into a 64 bits one, cf.
  logic of INTERSECT ALL in
  MaterializeIterator<Profiler>::MaterializeOperand.
  For INTERSECT ALL we need two counters: the number of duplicates in the left
  operand, and the number of matches seen (so far) from the right operand.
  Instead of adding another field to the temporary table, we subdivide the
  64 bits counter we already have. This imposes an implementation restriction
  on INTERSECT ALL: the resulting table must have <= uint32::max duplicates of
  any row.
 */
class HalfCounter {
  union {
    /// [0]: # of duplicates on left side of INTERSECT ALL
    /// [1]: # of duplicates on right side of INTERSECT ALL. Always <= [0].
    uint32_t m_value[2];
    uint64_t m_value64;
  } data;

 public:
  HalfCounter(uint64_t packed) { data.m_value64 = packed; }
  uint64_t value() const { return data.m_value64; }
  uint32_t &operator[](size_t idx) {
    assert(idx == 0 || idx == 1);
    return data.m_value[idx];
  }
};

#endif  // SQL_ITERATORS_BASIC_ROW_ITERATORS_H_
