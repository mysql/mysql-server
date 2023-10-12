#ifndef SQL_ITERATORS_COMPOSITE_ITERATORS_H_
#define SQL_ITERATORS_COMPOSITE_ITERATORS_H_

/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
#include <stdint.h>
#include <sys/types.h>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "my_alloc.h"
#include "my_base.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "mysqld_error.h"
#include "sql/immutable_string.h"
#include "sql/iterators/hash_join_buffer.h"
#include "sql/iterators/hash_join_chunk.h"
#include "sql/iterators/hash_join_iterator.h"
#include "sql/iterators/row_iterator.h"
#include "sql/join_type.h"
#include "sql/mem_root_array.h"
#include "sql/pack_rows.h"
#include "sql/sql_array.h"
#include "sql_string.h"

#include "extra/robin-hood-hashing/robin_hood.h"
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

using Operands = Mem_root_array<materialize_iterator::Operand>;

/**
  Contains spill state for set operations' use of in-memory hash map.

  If we encounter a situation in which the hash map for set operations
  overflows allowed memory, we initiate a spill to disk procedure. This class
  encapsulates state using during this procedure. Spill to disk starts
  with a call to \c handle_hash_map_full.

  We built a mechanism with an in-memory hash map which can spill
  gracefully to disk if the volume of rows gets large and still perform
  well. In the presence of wrong table cardinality information, we may not be
  able to complete the spill to disk procedure (if we still run out of memory
  when hashing chunks, see below). If so, we fall back on de-duplicating
  using the non-unique key of the output (materialized) result table.

  The spill code is partially based on code developed for hash join: e.g. we
  reuse packing/unpacking functions like
  \verbatim
    StoreFromTableBuffersRaw            (pack_rows.h)
    LoadImmutableStringIntoTableBuffers (hash_join_buffer.h)
  \endverbatim
  and furthermore, the Robin Hood hashing library (robin_hood.h), and the chunk
  file abstraction.
  \verbatim
  Definitions:
        A' - set of rows from operand 1 of set operation that fits in
             the in-memory hash map, deduplicated, with counters
        A  - set of rows from operand 1 before deduplication
        B  - non-deduplicated set of rows from operand 1 that didn't
             fit
        C = A + B
           - total set of rows in operand one; not known a priori, but we use
             the statistics for an estimate.

        M - (aka. m_num_chunks) total number of chunk files the tertiary
            hash distributes the rows to. Multiple of 2, as used for hash join.

        N - (aka. HashJoinIterator::kMaxChunks) the max number of HF and IF
             files that may be open at one time. May be smaller than M.

        S = ceiling(M/N)  (aka. m_no_of_chunk_file_sets)
           - number of sets of open files we need

        s - the set of chunk files opened (aka. m_chunk_files), sets are
            enumerated from 0..S-1, cf. m_current_chunk_file_set.

        n - number of operands in set operation

        REMAININGINPUT (aka. m_remaining_input) - tmp file needed if S > 1.
        MATERIALIZEDTABLE (aka. m_materialized_table) - output for
            EXCEPT/INTERSECT algorithm

        primary hash
          - MySQL record hash, aka. calc_row_hash(m_materialized_table)
        secondary hash
          - the hash function used by Robin Hood for the in-memory hash map
            based on primary hash
        tertiary hash
          - hash function for distributing rows to chunk files, cf.
            MY_XXH64 based on primary hash

   ============
   !In-memory !                  Two kinds of tmp chunk files, HF and IF
   !hash map  !                  HF: already Hashed and de-duplicated rows File
   !  A' rows !                  IF: Input File (not yet de-duplicated rows)
   !==========!
     |                            !---------!        !----------------!
     |                            ! B       !        ! REMAININGINPUT !
     |                            !---------!        !----------------!
     |                                   |
     ↓ tertiary  hash → 0:M-1            ↓
     +--------+------------\             +--------+------------\
     ↓        ↓            ↓             ↓        ↓            ↓
  !----!    !----!     !------!       !----!    !----!     !------!
  !HF_0!    !HF_1! ..  !HF_M-1!       !IF_0!    !IF_1! ..  !IF_M-1!
  !----!    !----!     !------!       !----!    !----!     !------!
                    ↑                                   ↑
                    N                                   N

   !-------------------!          !----------!    !----------!
   ! MATERIALIZEDTABLE !          ! operand-2! .. ! operand-n!
   !-------------------!          !----------!    !----------!

  If M > N, we cannot have open all chunk files at the same time, so in each
  chunk file we have this structure:

                           +-------+
                           |       | rows from set 0
                           +-------+
                               :
                           +-------+
                           |       | rows from set S-1
                           +-------+

  If we need more M than N, M will be a multiple of N as well as a multiple of
  2, since N is also chosen a multiple of two (currently 128). So, the physical
  tmp file contains several logical chunk files. For the HF chunks, we in
  addition have several generations of these: each round of processing appends
  a new generation (more updated) version of the chunks. For a 2 operand set
  operation, we have three generations:

  1. the initial row sets from the in-memory hash map (A' spread over M chunks)
  2. updated sets with the rest of the left operand (C deduplicated and spread
     over M chunks)
  3. updated sets after we have processed the right operand

  We keep track of the read and write positions on the tmp files, cf. methods
  HashJoinChunk::SetAppend and HashJoinChunk::ContinueRead. This enables
  reading back rows from the generation last written, and the writing of a new
  generation at the tail of the chunk file. More set operands than two adds
  further generations, one for each extra operand.

  * Algorithm


  1. The in-memory hash map can hit its memory limit when we read the
     left set operand (block) after having read A rows, resulting in A' rows in
     in-memory hash map. If we do not hit the limit, we are done, no spill to
     disk is required.

     Note: Spill can never happen when we read operand 2..n since operand 1 of
     INTERSECT and EXCEPT determines the maximum rows in the result set and
     hence the maximal size of the in-memory hash map.

     So, we will have established the spill-over storage *before* reading of
     operands 2..n starts.

  2. Before looking at operand 2..n, we need to finish processing the remaining
     rows in the left operand, cf. the details below:

  3. When we hit limit, we:

     Determine number N of chunk files based on the estimated number of rows in
     operand 1 (the left operand). As mentioned, if number of chunks needed (M)
     > maxOpenFiles, we still allow this but will keep open only a subset s at
     any one time, presuming worst case of no deduplication, i.e. A'==A.  In
     this case, M == N * S, but M can be as low as 2 (M << N). This is
     performed in the method `compute_chunk_file_sets' and
     `initialize_first_HF_chunk_files'.

   3.a)
        For all file sets s in 1..S:

           - rehash with tertiary hash and write A' to files HF-{0..N-1} all
             rows in in-mem hash map. Save the computed primary hash value in
             the hash column, so we do not need to compute it over again when
             we read HF-k into hash map again. This is done in method
             `spread_hash_map_to_HF_chunk_files'. HF chunk file sets are now
             in generation one.

           - When s contains hash for offending row, write the offending row
             |A|+1 that did't fit the in-memory hash map to IF-k in s.
             (aka. m_offending_row)

        Note these rows (A') have been de-duplicated down to A' and
        counters set accordingly.


     3.b)
        For all file sets s in 1..S:

        3.b.1) read the rest of the left input (or re-read them via
               REMAININGINPUT if s>1), hash and write to destination file IF-k
               the rows which, based on its tertiary hash value, have index k
               in the current set.  If s is the first file set AND S>1 and row
               didn't go to a file in s, also save input row to file
               REMAININGINPUT since we need it for another file set (since we
               cannot replay the source). See method
               `save_rest_of_operand_to_IF_chunk_files' and
               `reset_for_spill_handling'.

     At this point we have the rest of the input rows B (that that have not
     been matched against HFs) in IF-{0..N-1}.  HF rows already are unique and
     have set operation counters already set based on first part of input rows
     that did fit in memory (so we have no need to "remember" that part of
     input except as initialized counters): only the remaining input rows (from
     operand 1) are of concern to us now.

     From here on, the logic is driven from the read_next_row. The set counter
     logic is still handled by process_row_hash. Most of the machinery
     for reading, writing and switching chunk files are driven by a state
     machine from read_next_row, (almost) invisible to
     process_row_hash, except for a simplified handling when we
     re-enter HF rows into the hash map ready to process operand 2..n, cf. call
     to `load_HF_row_into_hash_map': these rows have already been
     de-duplicated and the hash table will not grow in size compared to
     operand one (intersect and except can't increase result set size), so we
     can use a shorter logic path.

     3.c)
        For each s in 1..S do
        For each pair of {HF-k, IF-k} in s do
           3.c.1) Read HF-k into hash map: optimization: use saved hash value
                  Cf. ReadingState::SS_READING_LEFT_HF

           3.c.2) Read rows from IF-k, continuing hash processing of
                  operand one. Cf. ReadingState::SS_READING_LEFT_IF.

                  If hash map overflows here, we recover by changing to
                  de-duplicating via the tmp table (we re-initialize it with a
                  non-unique index on the hash field in the row in
                  handle_hash_map_full).  This overflow means we cannot fit
                  even 1/M-th of set of unique rows in input set of operand 1
                  in memory). If row estimates are reasonably good, it should
                  not happen.  For details on secondary overflow recovery, see
                  handle_hash_map_full and comments in materialize_hash_map,
                  and logic in read_next_row_secondary_overflow.

           3.c.3) We are done with pair {HF-k, IF-k}, append hash map to HF-k
                  and empty in-memory hash map, cf. `append_hash_map_to_HF'.

      We are done with operand 1, and we have min(M,N) HF files with unique rows
      (incl counters) on disk in one or more sets, in generation two.

     4.a) For each operand 2..n do
        4.a.0) Empty all IFs and REMAININGINPUT.
        For each s in S do

           4.a.1) Read input operand (from block or REMAININGINPUT if s>1),
                  hash to IF-k, and write. If s==1 AND S>1 also save input row
                  to file REMAININGINPUT since we need them for the next file
                  set s, cf. save_operand_to_IF_chunk_files.
           4.a.2) Similar to same as 3.c, except with right side counter logic
                  cf. states ReadingState::SS_READING_RIGHT_{HF,IF}.

     5) We now have min(N,M) HF files with unique rows sets (incl set logic
        counters) on disk (generation three), all operands have been
        processed. For each HF-k read it and write to MATERIALIZEDTABLE.
  \endverbatim
*/
class SpillState {
 public:
  SpillState(THD *thd, MEM_ROOT *mem_root)
      : m_thd(thd),
        m_chunk_files(mem_root),
        m_row_counts(mem_root, HashJoinIterator::kMaxChunks) {}

  /**
    Inquire spill handling state

    @returns true if we are in spill to disk processing mode
  */
  bool spill() { return m_spill_read_state != ReadingState::SS_NONE; }

#ifndef NDEBUG
  bool simulated_secondary_overflow(bool *spill);

 private:
  size_t m_simulated_set_idx{std::numeric_limits<size_t>::max()};
  size_t m_simulated_chunk_idx{std::numeric_limits<size_t>::max()};
  size_t m_simulated_row_no{std::numeric_limits<size_t>::max()};

 public:
#endif

  void set_secondary_overflow() { m_secondary_overflow = true; }

  using hash_map_type = robin_hood::unordered_flat_map<
      ImmutableStringWithLength, LinkedImmutableString,
      hash_join_buffer::KeyHasher, hash_join_buffer::KeyEquals>;

  static void reset_hash_map(hash_map_type *hash_map) {
    hash_map->~hash_map_type();
    auto *map = new (hash_map)
        hash_map_type(/*bucket_count=*/10, hash_join_buffer::KeyHasher());
    if (map == nullptr) {
      my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), sizeof(hash_map_type));
    }
  }

  /// Getter, cf. comment for \c m_secondary_overflow
  bool secondary_overflow() const { return m_secondary_overflow; }
  void secondary_overflow_handling_done() {
    m_spill_read_state = ReadingState::SS_NONE;
    m_secondary_overflow = false;
    // free up resources from chunk files and hashmap
    reset_hash_map(m_hash_map);
    m_hash_map_mem_root->Clear();
    m_chunk_files.clear();
    m_row_counts.clear();
  }

  enum class ReadingState : uint8_t {
    SS_NONE,
    SS_READING_LEFT_HF,
    SS_READING_LEFT_IF,
    SS_COPY_OPERAND_N_TO_IF,  // done de-duplicating one operand, ready for next
    SS_READING_RIGHT_HF,
    SS_READING_RIGHT_IF,
    SS_FLUSH_REST_OF_LEFT_IFS  // only used after secondary overflow
  };

  ReadingState read_state() { return m_spill_read_state; }

  /**
    Initialize the spill to disk processing state with some variables.

    @param left_operand the left-most operand in a N-ary set operation
    @param hash_map     the in-memory hash map that overflowed, causing the
                        spill to disk
    @param rows_in_hash_table
                        the number of rows in the hash map
    @param read_rows_before_dedup
                        the number of rows read from the left operand
                        before de-duplicating into the hash map
    @param hash_map_mem_root
                        the mem_root used for allocating space for the hash
                        map's keys and values
    @param t            the materialized table that receive the result set of
                        the set operation
  */
  bool init(const Operand &left_operand, hash_map_type *hash_map,
            size_t rows_in_hash_table, size_t read_rows_before_dedup,
            MEM_ROOT *hash_map_mem_root, TABLE *t);

  /**
    Given current state of spill processing, return the next row up for
    inserting into or matching against the hash map.
    @param current_operand  the operand (query block) we are currently reading
                            from
    @retval
       0   OK
    @retval
      -1   End of records
    @retval
       1   Error
  */
  int read_next_row(const Operand *current_operand);

  /**
    Given current state of secondary overflow processing, return the next row
    up for inserting into or matching against the index in the result table (we
    no longer use hashing, having fallen back on de-duplicating via index in
    resulting output table.

    First, return the row which caused the overflow as row #1.  Next, we read
    the rest of the IF rows of the current chunk we were processing when the
    secondary overflow occured.  Finally, we read all remaining left side IF
    chunks, if any, which haven't been matched with their corresponding HF
    chunk, i.e. we do not need to read IF files that have already been matched
    up with their corresponding HF chunk files prior to the secondary overflow,
    if any.

    Processing of right operand(s) will proceed as for non-hashed
    de-duplication (similarly to what is done for UNION), and is not handled
    here. Cf. secondary_overflow_handling_done which completes secondary
    overflow handling and reverts to normal non hashed de-duplication for
    operands 2..n.

    @retval
       0   OK
    @retval
      -1   End of records
    @retval
       1   Error
  */
  int read_next_row_secondary_overflow();

  /**
    Used to write a complete (or incomplete in the case of secondary overflow)
    HF chunk to the materialized tmp table. Will handle spill to disk if
    needed.
    @param thd               Session state
    @param set               The set for which to write a chunk
    @param chunk_idx         The chunk for which to write rows
    @param operands          The operands of the set operation
    @param [out] stored_rows Incremented with the number of row written from
                             the specified chunk to the materialized tmp table
    @returns true if error, else false
  */
  bool write_HF(THD *thd, size_t set, size_t chunk_idx,
                const Operands &operands, ha_rows *stored_rows);
  /**
    Write the contents of the final generation of HD chunks to the materialized
    table which will hold the result set of the set operation.
    TODO: avoid materializing more rows than required if LIMIT is present
    TODO: stream rows as soon as final generation of a HF chunk file is ready?

    @param thd                Current session state
    @param operands           The operands of the set operation
    @param [out] stored_rows  Will be incremenented with the number of produced
                              rows
    @returns true on error, else false.
   */
  bool write_completed_HFs(THD *thd, const Operands &operands,
                           ha_rows *stored_rows);  // 5.

  /**
    Write the contents of the HD chunks that were completed when a secondary
    memory overflow has occurred. In the general case it is a mix of 1.
    and 2. generation HF chunks.

    @param thd                Current session state
    @param operands           The operands of the set operation
    @param [out] stored_rows  Will be updated with the written rows
    @returns true on error
   */
  bool write_partially_completed_HFs(THD *thd, const Operands &operands,
                                     ha_rows *stored_rows);

 private:
  void switch_to_HF() {
    assert(m_spill_read_state == ReadingState::SS_READING_LEFT_IF ||
           m_spill_read_state == ReadingState::SS_READING_RIGHT_IF);
    if (m_spill_read_state == ReadingState::SS_READING_LEFT_IF)
      m_spill_read_state = ReadingState::SS_READING_LEFT_HF;
    else
      m_spill_read_state = ReadingState::SS_READING_RIGHT_HF;
  }

  void switch_to_IF() {
    assert(m_spill_read_state == ReadingState::SS_READING_LEFT_HF ||
           m_spill_read_state == ReadingState::SS_READING_RIGHT_HF);
    if (m_spill_read_state == ReadingState::SS_READING_LEFT_HF)
      m_spill_read_state = ReadingState::SS_READING_LEFT_IF;
    else
      m_spill_read_state = ReadingState::SS_READING_RIGHT_IF;
  }

 public:
  // Save away the contents of the row that made the hash table run out of
  // memory - for later processing
  bool save_offending_row();
  THD *thd() { return m_thd; }

 private:
  /**
    Compute sizing of and set aside space for the on-disk chunks and their
    associated in-memory structures, based on the row estimate taken from
    Operand::m_estimated_output_rows. Also save away the offending row (the one
    that we read, but we couldn't put into the hash map) so that we can write
    it to an IF chunk later.
    @returns true on error
   */
  bool compute_chunk_file_sets(const Operand *current_operand);  // 3.
  bool initialize_first_HF_chunk_files();                        // 3.

  /**
    The initial hash map that overflowed will be spread over the determined
    number of chunk files, cf. initialize_next_HF_chunk_files
    @returns true on error
  */
  bool spread_hash_map_to_HF_chunk_files();  // 3.a)
  bool save_operand_to_IF_chunk_files(
      const Operand *current_operand);  // 4.a.1)
  bool save_rest_of_operand_to_IF_chunk_files(
      const Operand *current_operand) {  // 3.b
    // "rest of": what didn't fit of left operand in initial hash map
    return save_operand_to_IF_chunk_files(current_operand);
  }
  bool reset_for_spill_handling();  // 3.b/3.c
  /**
    We are done processing a {HF, IF} chunk pair. The results are
    in the in-memory hash map, which we now append to the current
    HF chunk file, i.e. m_chunk_files[offset].build_chunk; clear the
    in-memory hash map, and make the HF chunk file ready for reading
    of what we now append.
    @returns true on error
  */
  bool append_hash_map_to_HF();

  THD *m_thd;

  /// If not SS_NONE, we have detected an overflow in the in-memory hash map
  /// while reading the left(-most) operand of an INTERSECT or EXCEPT operation
  /// and are ready for reading next row from an operand (left or right).
  ReadingState m_spill_read_state{ReadingState::SS_NONE};

  /// If true, we have seen memory overflow also during spill handling. This is
  /// because a HF chunk won't fit in memory, i.e. the computation we made to
  /// ensure it would fit, was not sufficient to make it so. This can be because
  /// table cardinality statistics is not up to date, or data density is very
  /// skewed. In this case we fall back on using tmp table unique key for
  /// de-duplicating.
  bool m_secondary_overflow{false};

  /// The materialized table we are eventualy writing the result of the set
  /// operation to
  TABLE *m_materialized_table{nullptr};

  /// Cached value for {m_materialized_table}.
  pack_rows::TableCollection m_table_collection;

  /// The in-memory hash map that overflowed. We will use it also during
  /// spill phase, so we need a pointer to it.
  hash_map_type *m_hash_map{nullptr};

  static constexpr uint32_t m_magic_prime = 4391;
  /// Modify for each operator in a N-ary set operation to avoid initial
  /// chunks filling up right away due to row order in previous operation
  size_t m_hash_seed{0};

  /// At the time of overflow: how many rows from left operand are in hash map
  /// after deduplication
  size_t m_rows_in_hash_map{0};

  /// At the time of overflow: how many rows have we read from left operand
  size_t m_read_rows_before_dedup{0};

  /// The mem_root of m_hash_map. We need it for reusing its space.
  MEM_ROOT *m_hash_map_mem_root{nullptr};

  /// The number of chunks needed after rounding up to nearest power of two.
  /// It may be larger thank HashJoinIterator::kMaxChunks in which case
  /// m_no_of_chunk_file_sets > 1.
  size_t m_num_chunks{0};

  /// The number of chunk file sets needed to process all m_num_chunks
  /// chunks.
  size_t m_no_of_chunk_file_sets{0};

  /// The current chunk under processing. 0-based.
  size_t m_current_chunk_file_set{0};

 public:
  size_t current_chunk_file_set() const { return m_current_chunk_file_set; }

 private:
  /// Keeps the row that was just read from the left operand when we discovered
  /// that we were out of space in the in-memory hash map. Save it for
  /// writing it to IF-k.
  struct {
    String m_buffer;
    size_t m_chunk_offset{0};
    size_t m_set{0};
    bool m_unsaved{true};
  } m_offending_row;

  static size_t chunk_index_to_set(size_t chunk_index) {
    return chunk_index / HashJoinIterator::kMaxChunks;
  }

  inline size_t hash_to_chunk_index(uint64_t hash) const {
    // put all entropy into two bytes
    uint16 word1 = 0xffff & (hash);
    uint16 word2 = 0xffff & (hash >> 16);
    uint16 word3 = 0xffff & (hash >> 32);
    uint16 word4 = 0xffff & (hash >> 48);
    uint16 folded_hash = word1 + word2 + word3 + word4;
    assert(m_num_chunks <= 65535);
    /// hash modulo m_num_chunks optimized calculation
    const size_t result = folded_hash & (m_num_chunks - 1);
    return result;
  }

  static size_t chunk_offset(size_t chunk_index) {
    return chunk_index & (HashJoinIterator::kMaxChunks - 1);
  }

  /// Temporary space for (de)serializing a row. Cf also
  /// m_offending_row.m_buffer for a similar dedicated space.
  String m_row_buffer;

  /// Array to hold the list of chunk files on disk in case we degrade into
  /// on-disk set EXCEPT/INTERSECT. Maximally kMaxChunks can be open and used at
  /// one time.
  Mem_root_array<ChunkPair> m_chunk_files;

  /// The index of the chunk pair being read, incremented before use
  size_t m_current_chunk_idx{0};

 public:
  size_t current_chunk_idx() const { return m_current_chunk_idx; }

 private:
  /// The current row no (1-based) in a chunk being read, incremented before
  /// use.
  size_t m_current_row_in_chunk{0};

  /// Used if m_no_of_chunk_file_sets > 1 so we can replay input rows from
  /// operands over sets 1..S-1, i.e. not used for rows from set 0.
  /// Not used if we only have one chunk file set.
  HashJoinChunk m_remaining_input;

  /// For a given chunk file pair {HF, IF}, the count of rows in each chunk
  /// respectively.
  struct CountPair {
    size_t HF_count;  // left set operation operand
    size_t IF_count;  // right set operation operand
  };

  /// For a chunk file pair, an array of counts indexed by
  /// m_current_chunk_file_set
  using SetCounts = Mem_root_array<CountPair>;

  /// A matrix of counters keeping track of how many rows have been stashed
  /// away in the chunk files for each set in each chunk file of the current
  /// generation. Used to allow us to read back the correct set of rows from
  /// each chunk given the current m_current_chunk_file_set.
  /// It is indexed thus:
  ///     m_row_counts[ chunk index ][ set index ]
  Mem_root_array<SetCounts> m_row_counts;
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
