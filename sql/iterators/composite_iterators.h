#ifndef SQL_ITERATORS_COMPOSITE_ITERATORS_H_
#define SQL_ITERATORS_COMPOSITE_ITERATORS_H_

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
  @file composite_iterators.h

  A composite row iterator is one that takes in one or more existing iterators
  and processes their rows in some interesting way. They are usually not bound
  to a single table or similar, but are the inner (non-leaf) nodes of the
  iterator execution tree. They consistently own their source iterator, although
  not its memory (since we never allocate row iterators on the heap--usually on
  a MEM_ROOT>). This means that in the end, you'll end up with a single root
  iterator which then owns everything else recursively.

  SortingIterator and the two window iterators are also composite iterators,
  but are defined in their own files.
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "my_alloc.h"
#include "my_base.h"
#include "my_inttypes.h"
#include "my_table_map.h"
#include "sql/iterators/row_iterator.h"
#include "sql/join_type.h"
#include "sql/mem_root_array.h"
#include "sql/pack_rows.h"
#include "sql/sql_array.h"
#include "sql_string.h"

class Cached_item;
class FollowTailIterator;
class Item;
class JOIN;
class KEY;
struct MaterializePathParameters;
class SJ_TMP_TABLE;
class Table_ref;
class THD;
class Table_function;
class Temp_table_param;
struct TABLE;

/**
  An iterator that takes in a stream of rows and passes through only those that
  meet some criteria (i.e., a condition evaluates to true). This is typically
  used for WHERE/HAVING.
 */
class FilterIterator final : public RowIterator {
 public:
  FilterIterator(THD *thd, unique_ptr_destroy_only<RowIterator> source,
                 Item *condition)
      : RowIterator(thd), m_source(std::move(source)), m_condition(condition) {}

  bool Init() override { return m_source->Init(); }

  int Read() override;

  void SetNullRowFlag(bool is_null_row) override {
    m_source->SetNullRowFlag(is_null_row);
  }

  void StartPSIBatchMode() override { m_source->StartPSIBatchMode(); }
  void EndPSIBatchModeIfStarted() override {
    m_source->EndPSIBatchModeIfStarted();
  }
  void UnlockRow() override { m_source->UnlockRow(); }

 private:
  unique_ptr_destroy_only<RowIterator> m_source;
  Item *m_condition;
};

/**
  Handles LIMIT and/or OFFSET; Init() eats the first "offset" rows, and Read()
  stops as soon as it's seen "limit" rows (including any skipped by offset).
 */
class LimitOffsetIterator final : public RowIterator {
 public:
  /**
    @param thd Thread context
    @param source Row source
    @param limit Maximum number of rows to read, including the ones skipped by
      offset. Can be HA_POS_ERROR for no limit.
    @param offset Number of initial rows to skip. Can be 0 for no offset.
    @param count_all_rows If true, the query will run to completion to get
      more accurate numbers for skipped_rows, so you will not get any
      performance benefits of early end.
    @param reject_multiple_rows True if a derived table transformed from a
      scalar subquery needs a run-time cardinality check
    @param skipped_rows If not nullptr, is incremented for each row skipped by
      offset or limit.
   */
  LimitOffsetIterator(THD *thd, unique_ptr_destroy_only<RowIterator> source,
                      ha_rows limit, ha_rows offset, bool count_all_rows,
                      bool reject_multiple_rows, ha_rows *skipped_rows)
      : RowIterator(thd),
        m_source(std::move(source)),
        m_limit(limit),
        m_offset(offset),
        m_count_all_rows(count_all_rows),
        m_reject_multiple_rows(reject_multiple_rows),
        m_skipped_rows(skipped_rows) {
    if (count_all_rows) {
      assert(m_skipped_rows != nullptr);
    }
  }

  bool Init() override;

  int Read() override;

  void SetNullRowFlag(bool is_null_row) override {
    m_source->SetNullRowFlag(is_null_row);
  }

  void StartPSIBatchMode() override { m_source->StartPSIBatchMode(); }
  void EndPSIBatchModeIfStarted() override {
    m_source->EndPSIBatchModeIfStarted();
  }
  void UnlockRow() override { m_source->UnlockRow(); }

 private:
  unique_ptr_destroy_only<RowIterator> m_source;

  // Note: The number of seen rows starts off at m_limit if we have OFFSET,
  // which means we don't need separate LIMIT and OFFSET tests on the
  // fast path of Read().
  ha_rows m_seen_rows;

  /**
     Whether we have OFFSET rows that we still need to skip.
   */
  bool m_needs_offset;

  const ha_rows m_limit, m_offset;
  const bool m_count_all_rows;
  const bool m_reject_multiple_rows;
  ha_rows *m_skipped_rows;
};

/**
  Handles aggregation (typically used for GROUP BY) for the case where the rows
  are already properly grouped coming in, ie., all rows that are supposed to be
  part of the same group are adjacent in the input stream. (This could be
  because they were sorted earlier, because we are scanning an index that
  already gives us the rows in a group-compatible order, or because there is no
  grouping.)

  AggregateIterator needs to be able to save and restore rows; it doesn't know
  when a group ends until it's seen the first row that is part of the _next_
  group. When that happens, it needs to tuck away that next row, and then
  restore the previous row so that the output row gets the correct grouped
  values. A simple example, doing SELECT a, SUM(b) FROM t1 GROUP BY a:

    t1.a  t1.b                                       SUM(b)
     1     1     <-- first row, save it                1
     1     2                                           3
     1     3                                           6
     2     1     <-- group changed, save row
    [1     1]    <-- restore first row, output         6
                     reset aggregate              -->  0
    [2     1]    <-- restore new row, process it       1
     2    10                                          11
                 <-- EOF, output                      11

  To save and restore rows like this, it uses the infrastructure from
  pack_rows.h to pack and unpack all relevant rows into record[0] of every input
  table. (Currently, there can only be one input table, but this may very well
  change in the future.) It would be nice to have a more abstract concept of
  sending a row around and taking copies of it if needed, as opposed to it
  implicitly staying in the table's buffer. (This would also solve some
  issues in EQRefIterator and when synthesizing NULL rows for outer joins.)
  However, that's a large refactoring.
 */
class AggregateIterator final : public RowIterator {
 public:
  AggregateIterator(THD *thd, unique_ptr_destroy_only<RowIterator> source,
                    JOIN *join, pack_rows::TableCollection tables, bool rollup);

  bool Init() override;
  int Read() override;
  void SetNullRowFlag(bool is_null_row) override {
    m_source->SetNullRowFlag(is_null_row);
  }

  void StartPSIBatchMode() override { m_source->StartPSIBatchMode(); }
  void EndPSIBatchModeIfStarted() override {
    m_source->EndPSIBatchModeIfStarted();
  }
  void UnlockRow() override {
    // Most likely, HAVING failed. Ideally, we'd like to backtrack and
    // unlock all rows that went into this aggregate, but we can't do that,
    // and we also can't unlock the _current_ row, since that belongs to a
    // different group. Thus, do nothing.
  }

 private:
  enum {
    READING_FIRST_ROW,
    LAST_ROW_STARTED_NEW_GROUP,
    OUTPUTTING_ROLLUP_ROWS,
    DONE_OUTPUTTING_ROWS
  } m_state;

  unique_ptr_destroy_only<RowIterator> m_source;

  /**
    The join we are part of. It would be nicer not to rely on this,
    but we need a large number of members from there, like which
    aggregate functions we have, the THD, temporary table parameters
    and so on.
   */
  JOIN *m_join = nullptr;

  /// Whether we have seen the last input row.
  bool m_seen_eof;

  /**
    Used to save NULL information in the specific case where we have
    zero input rows.
   */
  table_map m_save_nullinfo;

  /// Whether this is a rollup query.
  const bool m_rollup;

  /**
    For rollup: The index of the first group item that did _not_ change when we
    last switched groups. E.g., if we have group fields A,B,C,D and then switch
    to group A,B,E,D, this value will become 1 (which means that we need
    to output rollup rows for 2 -- A,B,E,NULL -- and then 1 -- A,B,NULL,NULL).
    m_current_rollup_position will count down from the end until it becomes
    less than this value.

    If we do not have rollup, this value is perennially zero.
   */
  int m_last_unchanged_group_item_idx;

  /**
    If we are in state OUTPUTTING_ROLLUP_ROWS, where we are in the iteration.
    This value will start at the index of the last group expression and then
    count backwards down to and including m_last_unchanged_group_item_idx.
    It is used to communicate to the rollup group items whether to turn
    themselves into NULLs, and the sum items which of their sums to output.
   */
  int m_current_rollup_position;

  /**
    The list of tables we are reading from; they are the ones for which we need
    to save and restore rows.
   */
  pack_rows::TableCollection m_tables;

  /// Packed version of the first row in the group we are currently processing.
  String m_first_row_this_group;

  /**
    If applicable, packed version of the first row in the _next_ group. This is
    used only in the LAST_ROW_STARTED_NEW_GROUP state; we just saw a row that
    didn't belong to the current group, so we saved it here and went to output
    a group. On the next Read() call, we need to process this deferred row
    first of all.

    Even when not in use, this string contains a buffer that is large enough to
    pack a full row into, sans blobs. (If blobs are present,
    StoreFromTableBuffers() will automatically allocate more space if needed.)
   */
  String m_first_row_next_group;

  /**
    The slice we're setting when returning rows. See the comment in the
    constructor.
   */
  int m_output_slice = -1;

  void SetRollupLevel(int level);
};

/**
  A simple nested loop join, taking in two iterators (left/outer and
  right/inner) and joining them together. This may, of course, scan the inner
  iterator many times. It is currently the only form of join we have.

  The iterator works as a state machine, where the state records whether we need
  to read a new outer row or not, and whether we've seen any rows from the inner
  iterator at all (if not, an outer join need to synthesize a new NULL row).

  The iterator takes care of activating performance schema batch mode on the
  right iterator if needed; this is typically only used if it is the innermost
  table in the entire join (where the gains from turning on batch mode is the
  largest, and the accuracy loss from turning it off are the least critical).
 */
class NestedLoopIterator final : public RowIterator {
 public:
  NestedLoopIterator(THD *thd,
                     unique_ptr_destroy_only<RowIterator> source_outer,
                     unique_ptr_destroy_only<RowIterator> source_inner,
                     JoinType join_type, bool pfs_batch_mode)
      : RowIterator(thd),
        m_source_outer(std::move(source_outer)),
        m_source_inner(std::move(source_inner)),
        m_join_type(join_type),
        m_pfs_batch_mode(pfs_batch_mode) {
    assert(m_source_outer != nullptr);
    assert(m_source_inner != nullptr);

    // Batch mode makes no sense for anti- or semijoins, since they should only
    // be reading one row.
    if (join_type == JoinType::ANTI || join_type == JoinType::SEMI) {
      assert(!pfs_batch_mode);
    }
  }

  bool Init() override;

  int Read() override;

  void SetNullRowFlag(bool is_null_row) override {
    // TODO: write something here about why we can't do this lazily.
    m_source_outer->SetNullRowFlag(is_null_row);
    m_source_inner->SetNullRowFlag(is_null_row);
  }

  void EndPSIBatchModeIfStarted() override {
    m_source_outer->EndPSIBatchModeIfStarted();
    m_source_inner->EndPSIBatchModeIfStarted();
  }

  void UnlockRow() override {
    // Since we don't know which condition that caused the row to be rejected,
    // we can't know whether we could also unlock the outer row
    // (it may still be used as parts of other joined rows).
    if (m_state == READING_FIRST_INNER_ROW || m_state == READING_INNER_ROWS) {
      m_source_inner->UnlockRow();
    }
  }

 private:
  enum {
    NEEDS_OUTER_ROW,
    READING_FIRST_INNER_ROW,
    READING_INNER_ROWS,
    END_OF_ROWS
  } m_state;

  unique_ptr_destroy_only<RowIterator> const m_source_outer;
  unique_ptr_destroy_only<RowIterator> const m_source_inner;
  const JoinType m_join_type;

  /** Whether to use batch mode when scanning the inner iterator. */
  const bool m_pfs_batch_mode;
};

/**
  An iterator that helps invalidating caches. Every time a row passes through it
  or it changes state in any other way, it increments its “generation” counter.
  This allows MaterializeIterator to see whether any of its dependencies has
  changed, and then force a rematerialization -- this is typically used for
  LATERAL tables, where we're joining in a derived table that depends on
  something earlier in the join.
 */
class CacheInvalidatorIterator final : public RowIterator {
 public:
  CacheInvalidatorIterator(THD *thd,
                           unique_ptr_destroy_only<RowIterator> source_iterator,
                           const std::string &name)
      : RowIterator(thd),
        m_source_iterator(std::move(source_iterator)),
        m_name(name) {}

  bool Init() override {
    ++m_generation;
    return m_source_iterator->Init();
  }

  int Read() override {
    ++m_generation;
    return m_source_iterator->Read();
  }

  void SetNullRowFlag(bool is_null_row) override {
    ++m_generation;
    m_source_iterator->SetNullRowFlag(is_null_row);
  }

  void UnlockRow() override { m_source_iterator->UnlockRow(); }

  int64_t generation() const { return m_generation; }
  std::string name() const { return m_name; }

 private:
  unique_ptr_destroy_only<RowIterator> m_source_iterator;
  int64_t m_generation = 0;
  std::string m_name;
};

namespace materialize_iterator {
/**
   An operand (query block) to be materialized by MaterializeIterator.
   (@see MaterializeIterator for details.)
*/
struct Operand {
  /// The iterator to read the actual rows from.
  unique_ptr_destroy_only<RowIterator> subquery_iterator;

  /// Used only for optimizer trace.
  int select_number;

  /// The JOIN that this query block represents. Used for performance
  /// schema batch mode: When materializing a query block that consists of
  /// a single table, MaterializeIterator needs to set up schema batch mode,
  /// since there is no nested loop iterator to do it. (This is similar to
  /// what ExecuteIteratorQuery() needs to do at the top level.)
  JOIN *join;

  /// If true, de-duplication checking via hash key is disabled
  /// when materializing this query block (ie., we simply avoid calling
  /// check_unique_fields() for each row). Used when materializing
  /// UNION DISTINCT and UNION ALL parts into the same table.
  /// We'd like to just use a unique constraint via unique index instead,
  /// but there might be other indexes on the destination table
  /// that we'd like to keep, and the implementation doesn't allow
  /// disabling only one index.
  ///
  /// If you use this on a query block, doing_hash_deduplication()
  /// must be true.
  bool disable_deduplication_by_hash_field = false;

  /// If set to false, the Field objects in the output row are
  /// presumed already to be filled out. This is the case iff
  /// there's a windowing iterator earlier in the chain.
  bool copy_items;

  /// The number of operands (i.e. blocks) involved in the set operation:
  /// used for INTERSECT to determine if a value is present in all operands
  ulonglong m_total_operands{0};
  /// The current operand (i.e. block) number, starting at zero. We use this
  /// for INTERSECT and EXCEPT materialization operand.
  ulonglong m_operand_idx{0};
  /// Used for EXCEPT computation: the index of the first operand involved in
  /// a N-ary except operation which has DISTINCT. This is significant for
  /// calculating whether to set the counter to zero or just decrement it
  /// when we see a right side operand.
  uint m_first_distinct{0};

  /// If copy_items is true, used for copying the Field objects
  /// into the temporary table row. Otherwise unused.
  Temp_table_param *temp_table_param;

  // Whether this query block is a recursive reference back to the
  // output of the materialization.
  bool is_recursive_reference = false;

  // If is_recursive_reference is true, contains the FollowTailIterator
  // in the query block (there can be at most one recursive reference
  // in a join list, as per the SQL standard, so there should be exactly one).
  // Used for informing the iterators about various shared state in the
  // materialization (including coordinating rematerializations).
  FollowTailIterator *recursive_reader = nullptr;

  /// The estimated number of rows produced by this block
  double m_estimated_output_rows{0.0};
};

/**
  Create an iterator that materializes a set of row into a temporary table
  and sets up a (pre-existing) iterator to access that.
  @see MaterializeIterator.

  @param thd Thread handler.
  @param operands List of operands (query blocks) to materialize.
  @param path_params MaterializePath settings.
  @param table_iterator Iterator used for accessing the temporary table
    after materialization.
  @param join
    When materializing within the same JOIN (e.g., into a temporary table
    before sorting), as opposed to a derived table or a CTE, we may need
    to change the slice on the join before returning rows from the result
    table. If so, join and ref_slice would need to be set, and
    query_blocks_to_materialize should contain only one member, with the same
    join.
  @return the iterator.
*/
RowIterator *CreateIterator(
    THD *thd, Mem_root_array<materialize_iterator::Operand> operands,
    const MaterializePathParameters *path_params,
    unique_ptr_destroy_only<RowIterator> table_iterator, JOIN *join);

}  // namespace materialize_iterator

namespace temptable_aggregate_iterator {
/**
   Create an iterator that aggregates the output rows from another iterator
   into a temporary table and then sets up a (pre-existing) iterator to
   access the temporary table.
   @see TemptableAggregateIterator.

   @param thd Thread handler.
   @param subquery_iterator input to aggregation.
   @param temp_table_param temporary table settings.
   @param table_iterator Iterator used for scanning the temporary table
    after materialization.
   @param table the temporary table.
   @param join the JOIN in which we aggregate.
   @param ref_slice the slice to set when accessing temporary table;
    used if anything upstream  wants to evaluate values based on its contents.
   @return the iterator.
*/
RowIterator *CreateIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> subquery_iterator,
    Temp_table_param *temp_table_param, TABLE *table,
    unique_ptr_destroy_only<RowIterator> table_iterator, JOIN *join,
    int ref_slice);

}  // namespace temptable_aggregate_iterator

/**
  StreamingIterator is a minimal version of MaterializeIterator that does not
  actually materialize; instead, every Read() just forwards the call to the
  subquery iterator and does the required copying from one set of fields to
  another.

  It is used for when the optimizer would normally set up a materialization,
  but you don't actually need one, ie. you don't want to read the rows multiple
  times after writing them, and you don't want to access them by index (only
  a single table scan). It also takes care of setting the NULL row flag
  on the temporary table.
 */
class StreamingIterator final : public TableRowIterator {
 public:
  /**
    @param thd Thread handle.
    @param subquery_iterator The iterator to read rows from.
    @param temp_table_param Parameters for the temp table.
    @param table The table we are streaming through. Will never actually
      be written to, but its fields will be used.
    @param provide_rowid If true, generate a row ID for each row we stream.
      This is used if the parent needs row IDs for deduplication, in particular
      weedout.
    @param join See MaterializeIterator.
    @param ref_slice See MaterializeIterator.
   */
  StreamingIterator(THD *thd,
                    unique_ptr_destroy_only<RowIterator> subquery_iterator,
                    Temp_table_param *temp_table_param, TABLE *table,
                    bool provide_rowid, JOIN *join, int ref_slice);

  bool Init() override;

  int Read() override;

  void StartPSIBatchMode() override {
    m_subquery_iterator->StartPSIBatchMode();
  }
  void EndPSIBatchModeIfStarted() override {
    m_subquery_iterator->EndPSIBatchModeIfStarted();
  }
  void UnlockRow() override { m_subquery_iterator->UnlockRow(); }

 private:
  unique_ptr_destroy_only<RowIterator> m_subquery_iterator;
  Temp_table_param *m_temp_table_param;
  ha_rows m_row_number;
  JOIN *const m_join;
  const int m_output_slice;
  int m_input_slice;

  // Whether the iterator should generate and provide a row ID. Only true if the
  // iterator is part of weedout, where the iterator will create a fake row ID
  // to uniquely identify the rows it produces.
  const bool m_provide_rowid;
};

/**
  An iterator that wraps a Table_function (e.g. JSON_TABLE) and allows you to
  iterate over the materialized temporary table. The table is materialized anew
  for every Init().

  TODO: Just wrapping it is probably not the optimal thing to do;
  Table_function is highly oriented around materialization, but never caches.
  Thus, perhaps we should rewrite Table_function to return a RowIterator
  instead of going through a temporary table.
 */
class MaterializedTableFunctionIterator final : public TableRowIterator {
 public:
  MaterializedTableFunctionIterator(
      THD *thd, Table_function *table_function, TABLE *table,
      unique_ptr_destroy_only<RowIterator> table_iterator);

  bool Init() override;
  int Read() override { return m_table_iterator->Read(); }
  void SetNullRowFlag(bool is_null_row) override {
    m_table_iterator->SetNullRowFlag(is_null_row);
  }

  void StartPSIBatchMode() override { m_table_iterator->StartPSIBatchMode(); }
  void EndPSIBatchModeIfStarted() override {
    m_table_iterator->EndPSIBatchModeIfStarted();
  }

  // The temporary table is private to us, so there's no need to worry about
  // locks to other transactions.
  void UnlockRow() override {}

 private:
  unique_ptr_destroy_only<RowIterator> m_table_iterator;

  Table_function *m_table_function;
};

/**
  Like semijoin materialization, weedout works on the basic idea that a semijoin
  is just like an inner join as we long as we can get rid of the duplicates
  somehow. (This is advantageous, because inner joins can be reordered, whereas
  semijoins generally can't.) However, unlike semijoin materialization, weedout
  removes duplicates after the join, not before it. Consider something like

    SELECT * FROM t1 WHERE a IN ( SELECT b FROM t2 );

  Semijoin materialization solves this by materializing t2, with deduplication,
  and then joining. Weedout joins t1 to t2 and then leaves only one output row
  per t1 row. The disadvantage is that this potentially needs to discard more
  rows; the (potential) advantage is that we deduplicate on t1 instead of t2.

  Weedout, unlike materialization, works in a streaming fashion; rows are output
  (or discarded) as they come in, with a temporary table used for recording the
  row IDs we've seen before. (We need to deduplicate on t1's row IDs, not its
  contents.) See SJ_TMP_TABLE for details about the table format.
 */
class WeedoutIterator final : public RowIterator {
 public:
  WeedoutIterator(THD *thd, unique_ptr_destroy_only<RowIterator> source,
                  SJ_TMP_TABLE *sj, table_map tables_to_get_rowid_for);

  bool Init() override;
  int Read() override;

  void SetNullRowFlag(bool is_null_row) override {
    m_source->SetNullRowFlag(is_null_row);
  }

  void EndPSIBatchModeIfStarted() override {
    m_source->EndPSIBatchModeIfStarted();
  }
  void UnlockRow() override { m_source->UnlockRow(); }

 private:
  unique_ptr_destroy_only<RowIterator> m_source;
  SJ_TMP_TABLE *m_sj;
  const table_map m_tables_to_get_rowid_for;
};

/**
  An iterator that removes consecutive rows that are the same according to
  a set of items (typically the join key), so-called “loose scan”
  (not to be confused with “loose index scan”, which is made by the
  range optimizer). This is similar in spirit to WeedoutIterator above
  (removing duplicates allows us to treat the semijoin as a normal join),
  but is much cheaper if the data is already ordered/grouped correctly,
  as the removal can happen before the join, and it does not need a
  temporary table.
 */
class RemoveDuplicatesIterator final : public RowIterator {
 public:
  RemoveDuplicatesIterator(THD *thd,
                           unique_ptr_destroy_only<RowIterator> source,
                           JOIN *join, Item **group_items,
                           int group_items_size);

  bool Init() override;
  int Read() override;

  void SetNullRowFlag(bool is_null_row) override {
    m_source->SetNullRowFlag(is_null_row);
  }

  void StartPSIBatchMode() override { m_source->StartPSIBatchMode(); }
  void EndPSIBatchModeIfStarted() override {
    m_source->EndPSIBatchModeIfStarted();
  }
  void UnlockRow() override { m_source->UnlockRow(); }

 private:
  unique_ptr_destroy_only<RowIterator> m_source;
  Bounds_checked_array<Cached_item *> m_caches;
  bool m_first_row;
};

/**
  Much like RemoveDuplicatesIterator, but works on the basis of a given index
  (or more accurately, its keypart), not an arbitrary list of grouped fields.
  This is only used in the non-hypergraph optimizer; the hypergraph optimizer
  can deal with groupings that come from e.g. sorts.
 */
class RemoveDuplicatesOnIndexIterator final : public RowIterator {
 public:
  RemoveDuplicatesOnIndexIterator(THD *thd,
                                  unique_ptr_destroy_only<RowIterator> source,
                                  const TABLE *table, KEY *key, size_t key_len);

  bool Init() override;
  int Read() override;

  void SetNullRowFlag(bool is_null_row) override {
    m_source->SetNullRowFlag(is_null_row);
  }

  void StartPSIBatchMode() override { m_source->StartPSIBatchMode(); }
  void EndPSIBatchModeIfStarted() override {
    m_source->EndPSIBatchModeIfStarted();
  }
  void UnlockRow() override { m_source->UnlockRow(); }

 private:
  unique_ptr_destroy_only<RowIterator> m_source;
  const TABLE *m_table;
  KEY *m_key;
  uchar *m_key_buf;  // Owned by the THD's MEM_ROOT.
  const size_t m_key_len;
  bool m_first_row;
};

/**
  An iterator that is semantically equivalent to a semijoin NestedLoopIterator
  immediately followed by a RemoveDuplicatesOnIndexIterator. It is used to
  implement the “loose scan” strategy in queries with multiple tables on the
  inside of a semijoin, like

    ... FROM t1 WHERE ... IN ( SELECT ... FROM t2 JOIN t3 ... )

  In this case, the query tree without this iterator would ostensibly look like

    -> Nested loop join
       -> Table scan on t1
       -> Remove duplicates on t2_idx
          -> Nested loop semijoin
             -> Index scan on t2 using t2_idx
             -> Filter (e.g. t3.a = t2.a)
                -> Table scan on t3

  (t3 will be marked as “first match” on t2 when implementing loose scan,
  thus the semijoin.)

  First note that we can't put the duplicate removal directly on t2 in this
  case, as the first t2 row doesn't necessarily match anything in t3, so it
  needs to be above. However, this is wasteful, because once we find a matching
  t2/t3 pair, we should stop scanning t3 until we have a new t2.

  NestedLoopSemiJoinWithDuplicateRemovalIterator solves the problem by doing
  exactly this; it gets a row from the outer side, gets exactly one row from the
  inner side, and then skips over rows from the outer side (_without_ scanning
  the inner side) until its keypart changes.
 */
class NestedLoopSemiJoinWithDuplicateRemovalIterator final
    : public RowIterator {
 public:
  NestedLoopSemiJoinWithDuplicateRemovalIterator(
      THD *thd, unique_ptr_destroy_only<RowIterator> source_outer,
      unique_ptr_destroy_only<RowIterator> source_inner, const TABLE *table,
      KEY *key, size_t key_len);

  bool Init() override;

  int Read() override;

  void SetNullRowFlag(bool is_null_row) override {
    m_source_outer->SetNullRowFlag(is_null_row);
    m_source_inner->SetNullRowFlag(is_null_row);
  }

  void EndPSIBatchModeIfStarted() override {
    m_source_outer->EndPSIBatchModeIfStarted();
    m_source_inner->EndPSIBatchModeIfStarted();
  }

  void UnlockRow() override {
    m_source_outer->UnlockRow();
    m_source_inner->UnlockRow();
  }

 private:
  unique_ptr_destroy_only<RowIterator> const m_source_outer;
  unique_ptr_destroy_only<RowIterator> const m_source_inner;

  const TABLE *m_table_outer;
  KEY *m_key;
  uchar *m_key_buf;  // Owned by the THD's MEM_ROOT.
  const size_t m_key_len;
  bool m_deduplicate_against_previous_row;
};

/**
  MaterializeInformationSchemaTableIterator makes sure a given I_S temporary
  table is materialized (filled out) before we try to scan it.
 */
class MaterializeInformationSchemaTableIterator final : public RowIterator {
 public:
  MaterializeInformationSchemaTableIterator(
      THD *thd, unique_ptr_destroy_only<RowIterator> table_iterator,
      Table_ref *table_list, Item *condition);

  bool Init() override;
  int Read() override { return m_table_iterator->Read(); }

  void SetNullRowFlag(bool is_null_row) override {
    m_table_iterator->SetNullRowFlag(is_null_row);
  }

  void StartPSIBatchMode() override { m_table_iterator->StartPSIBatchMode(); }
  void EndPSIBatchModeIfStarted() override {
    m_table_iterator->EndPSIBatchModeIfStarted();
  }

  // The temporary table is private to us, so there's no need to worry about
  // locks to other transactions.
  void UnlockRow() override {}

 private:
  /// The iterator that reads from the materialized table.
  unique_ptr_destroy_only<RowIterator> m_table_iterator;
  Table_ref *m_table_list;
  Item *m_condition;
};

/**
  Takes in two or more iterators and output rows from them sequentially
  (first all rows from the first one, the all from the second one, etc.).
  Used for implementing UNION ALL, typically together with StreamingIterator.
 */
class AppendIterator final : public RowIterator {
 public:
  AppendIterator(
      THD *thd,
      std::vector<unique_ptr_destroy_only<RowIterator>> &&sub_iterators);

  bool Init() override;
  int Read() override;

  void StartPSIBatchMode() override;
  void EndPSIBatchModeIfStarted() override;

  void SetNullRowFlag(bool is_null_row) override;
  void UnlockRow() override;

 private:
  std::vector<unique_ptr_destroy_only<RowIterator>> m_sub_iterators;
  size_t m_current_iterator_index = 0;
  bool m_pfs_batch_mode_enabled = false;
};

#endif  // SQL_ITERATORS_COMPOSITE_ITERATORS_H_
