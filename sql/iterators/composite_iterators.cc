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

#include "sql/iterators/composite_iterators.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <functional>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <ankerl/unordered_dense.h>

#include "field_types.h"
#include "mem_root_deque.h"
#include "my_config.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_xxhash.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "prealloced_array.h"
#include "scope_guard.h"
#include "sql/debug_sync.h"
#include "sql/error_handler.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/immutable_string.h"
#include "sql/item.h"
#include "sql/item_func.h"
#include "sql/item_sum.h"
#include "sql/iterators/basic_row_iterators.h"
#include "sql/iterators/hash_join_buffer.h"
#include "sql/iterators/hash_join_chunk.h"
#include "sql/iterators/hash_join_iterator.h"
#include "sql/iterators/timing_iterator.h"
#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/materialize_path_parameters.h"
#include "sql/key.h"
#include "sql/opt_trace.h"
#include "sql/opt_trace_context.h"
#include "sql/pfs_batch_mode.h"
#include "sql/psi_memory_key.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"
#include "sql/sql_show.h"
#include "sql/sql_tmp_table.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/table_function.h"  // Table_function
#include "sql/temp_table_param.h"
#include "sql/window.h"
#include "template_utils.h"

using pack_rows::TableCollection;
using std::any_of;

int FilterIterator::Read() {
  for (;;) {
    int err = m_source->Read();
    if (err != 0) return err;

    bool matched = m_condition->val_int();

    if (thd()->killed) {
      thd()->send_kill_message();
      return 1;
    }

    /* check for errors evaluating the condition */
    if (thd()->is_error()) return 1;

    if (!matched) {
      m_source->UnlockRow();
      continue;
    }

    // Successful row.
    return 0;
  }
}

bool LimitOffsetIterator::Init() {
  if (m_source->Init()) {
    return true;
  }
  if (m_offset > 0) {
    m_seen_rows = m_limit;
    m_needs_offset = true;
  } else {
    m_seen_rows = 0;
    m_needs_offset = false;
  }
  return false;
}

int LimitOffsetIterator::Read() {
  if (m_seen_rows >= m_limit) {
    // We either have hit our LIMIT, or we need to skip OFFSET rows.
    // Check which one.
    if (m_needs_offset) {
      // We skip OFFSET rows here and not in Init(), since performance schema
      // batch mode may not be set up by the executor before the first Read().
      // This makes sure that
      //
      //   a) we get the performance benefits of batch mode even when reading
      //      OFFSET rows, and
      //   b) we don't inadvertedly enable batch mode (e.g. through the
      //      NestedLoopIterator) during Init(), since the executor may not
      //      be ready to _disable_ it if it gets an error before first Read().
      for (ha_rows row_idx = 0; row_idx < m_offset; ++row_idx) {
        int err = m_source->Read();
        if (err != 0) {
          // Note that we'll go back into this loop if Init() is called again,
          // and return the same error/EOF status.
          return err;
        }
        if (m_skipped_rows != nullptr) {
          ++*m_skipped_rows;
        }
        m_source->UnlockRow();
      }
      m_seen_rows = m_offset;
      m_needs_offset = false;

      // Fall through to LIMIT testing.
    }

    if (m_seen_rows >= m_limit) {
      // We really hit LIMIT (or hit LIMIT immediately after OFFSET finished),
      // so EOF.
      if (m_count_all_rows) {
        // Count rows until the end or error (ignore the error if any).
        while (m_source->Read() == 0) {
          ++*m_skipped_rows;
        }
      }
      return -1;
    }
  }

  const int result = m_source->Read();
  if (m_reject_multiple_rows) {
    if (result != 0) {
      ++m_seen_rows;
      return result;
    }
    // We read a row. Check for scalar subquery cardinality violation
    if (m_seen_rows - m_offset > 0) {
      my_error(ER_SUBQUERY_NO_1_ROW, MYF(0));
      return 1;
    }
  }

  ++m_seen_rows;
  return result;
}

AggregateIterator::AggregateIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> source, JOIN *join,
    TableCollection tables, bool rollup)
    : RowIterator(thd),
      m_source(std::move(source)),
      m_join(join),
      m_rollup(rollup),
      m_tables(std::move(tables)) {
  if (!tables.has_blob_column()) {
    // If blob, we reserve lazily in StoreFromTableBuffers since we can't know
    // upper bound here.
    const size_t upper_data_length =
        ComputeRowSizeUpperBoundSansBlobs(m_tables);
    m_first_row_this_group.reserve(upper_data_length);
    m_first_row_next_group.reserve(upper_data_length);
  }
}

bool AggregateIterator::Init() {
  assert(!m_join->tmp_table_param.precomputed_group_by);

  // Disable any leftover rollup items used in children.
  m_current_rollup_position = -1;
  SetRollupLevel(INT_MAX);

  // If the iterator has been executed before, restore the state of
  // the table buffers. This is needed for correctness if there is an
  // EQRefIterator below this iterator, as the restoring of the
  // previous group in Read() may have disturbed the cache in
  // EQRefIterator.
  if (!m_first_row_next_group.is_empty()) {
    LoadIntoTableBuffers(
        m_tables, pointer_cast<const uchar *>(m_first_row_next_group.ptr()));
    m_first_row_next_group.length(0);
  }

  if (m_source->Init()) {
    return true;
  }

  // If we have a HAVING after us, it needs to be evaluated within the context
  // of the slice we're in (unless we're in the hypergraph optimizer, which
  // doesn't use slices). However, we might have a sort before us, and
  // SortingIterator doesn't set the slice except on Init(); it just keeps
  // whatever was already set. When there is a temporary table after the HAVING,
  // the slice coming from there might be wrongly set on Read(), and thus,
  // we need to properly restore it before returning any rows.
  //
  // This is a hack. It would be good to get rid of the slice system altogether.
  if (!(m_join->implicit_grouping || m_join->group_optimized_away) &&
      !thd()->lex->using_hypergraph_optimizer()) {
    m_output_slice = m_join->get_ref_item_slice();
  }

  m_seen_eof = false;
  m_save_nullinfo = 0;

  // Not really used, just to be sure.
  m_last_unchanged_group_item_idx = 0;

  m_state = READING_FIRST_ROW;

  return false;
}

int AggregateIterator::Read() {
  switch (m_state) {
    case READING_FIRST_ROW: {
      // Start the first group, if possible. (If we're not at the first row,
      // we already saw the first row in the new group at the previous Read().)
      int err = m_source->Read();
      if (err == -1) {
        m_seen_eof = true;
        m_state = DONE_OUTPUTTING_ROWS;
        if (m_rollup && m_join->send_group_parts > 0) {
          // No rows in result set, but we must output one grouping row: we
          // just want the final totals row, not subtotals rows according
          // to SQL standard.
          SetRollupLevel(0);
          if (m_output_slice != -1) {
            m_join->set_ref_item_slice(m_output_slice);
          }
          return 0;
        }
        if (m_join->grouped || m_join->group_optimized_away) {
          SetRollupLevel(m_join->send_group_parts);
          return -1;
        } else {
          // If there's no GROUP BY, we need to output a row even if there are
          // no input rows.

          // Calculate aggregate functions for no rows
          for (Item *item : *m_join->get_current_fields()) {
            if (!item->hidden ||
                (item->type() == Item::SUM_FUNC_ITEM &&
                 down_cast<Item_sum *>(item)->aggr_query_block ==
                     m_join->query_block)) {
              item->no_rows_in_result();
            }
          }

          /*
            Mark tables as containing only NULL values for ha_write_row().
            Calculate a set of tables for which NULL values need to
            be restored after sending data.
          */
          if (m_join->clear_fields(&m_save_nullinfo)) {
            return 1;
          }
          for (Item_sum **item = m_join->sum_funcs; *item != nullptr; ++item) {
            (*item)->clear();
          }
          if (m_output_slice != -1) {
            m_join->set_ref_item_slice(m_output_slice);
          }
          return 0;
        }
      }
      if (err != 0) return err;

      // Set the initial value of the group fields.
      (void)update_item_cache_if_changed(m_join->group_fields);

      StoreFromTableBuffers(m_tables, &m_first_row_next_group);

      m_last_unchanged_group_item_idx = 0;
    }
      [[fallthrough]];

    case LAST_ROW_STARTED_NEW_GROUP:
      SetRollupLevel(m_join->send_group_parts);

      // We don't need m_first_row_this_group for the old group anymore,
      // but we'd like to reuse its buffer, so swap instead of std::move.
      // (Testing for state == READING_FIRST_ROW and avoiding the swap
      // doesn't seem to give any speed gains.)
      swap(m_first_row_this_group, m_first_row_next_group);
      LoadIntoTableBuffers(
          m_tables, pointer_cast<const uchar *>(m_first_row_this_group.ptr()));

      for (Item_sum **item = m_join->sum_funcs; *item != nullptr; ++item) {
        if (m_rollup) {
          if (down_cast<Item_rollup_sum_switcher *>(*item)
                  ->reset_and_add_for_rollup(m_last_unchanged_group_item_idx))
            return true;
        } else {
          if ((*item)->reset_and_add()) return true;
        }
      }

      // Keep reading rows as long as they are part of the existing group.
      for (;;) {
        int err = m_source->Read();
        if (err == 1) return 1;  // Error.

        if (err == -1) {
          m_seen_eof = true;

          // We need to be able to restore the table buffers in Init()
          // if the iterator is reexecuted (can happen if it's inside
          // a correlated subquery).
          StoreFromTableBuffers(m_tables, &m_first_row_next_group);

          // End of input rows; return the last group. (One would think this
          // LoadIntoTableBuffers() call is unneeded, since the last row read
          // would be from the last group, but there may be filters in-between
          // us and whatever put data into the row buffers, and those filters
          // may have caused other rows to be loaded before discarding them.)
          LoadIntoTableBuffers(m_tables, pointer_cast<const uchar *>(
                                             m_first_row_this_group.ptr()));

          if (m_rollup && m_join->send_group_parts > 0) {
            // Also output the final groups, including the total row
            // (with NULLs in all fields).
            SetRollupLevel(m_join->send_group_parts);
            m_last_unchanged_group_item_idx = 0;
            m_state = OUTPUTTING_ROLLUP_ROWS;
          } else {
            SetRollupLevel(m_join->send_group_parts);
            m_state = DONE_OUTPUTTING_ROWS;
          }
          if (m_output_slice != -1) {
            m_join->set_ref_item_slice(m_output_slice);
          }
          return 0;
        }

        int first_changed_idx =
            update_item_cache_if_changed(m_join->group_fields);
        if (first_changed_idx >= 0) {
          // The group changed. Store the new row (we can't really use it yet;
          // next Read() will deal with it), then load back the group values
          // so that we can output a row for the current group.
          // NOTE: This does not save and restore FTS information,
          // so evaluating MATCH() on these rows may give the wrong result.
          // (Storing the row ID and repositioning it with ha_rnd_pos()
          // would, but we can't do the latter without disturbing
          // ongoing scans. See bug #32565923.) For the old join optimizer,
          // we generally solve this by inserting temporary tables or sorts
          // (both of which restore the information correctly); for the
          // hypergraph join optimizer, we add a special streaming step
          // for MATCH columns.
          StoreFromTableBuffers(m_tables, &m_first_row_next_group);
          LoadIntoTableBuffers(m_tables, pointer_cast<const uchar *>(
                                             m_first_row_this_group.ptr()));

          // If we have rollup, we may need to output more than one row.
          // Mark so that the next calls to Read() will return those rows.
          //
          // NOTE: first_changed_idx is the first group value that _changed_,
          // while what we store is the last item that did _not_ change.
          if (m_rollup) {
            m_last_unchanged_group_item_idx = first_changed_idx + 1;
            if (static_cast<unsigned>(first_changed_idx) <
                m_join->send_group_parts - 1) {
              SetRollupLevel(m_join->send_group_parts);
              m_state = OUTPUTTING_ROLLUP_ROWS;
            } else {
              SetRollupLevel(m_join->send_group_parts);
              m_state = LAST_ROW_STARTED_NEW_GROUP;
            }
          } else {
            m_last_unchanged_group_item_idx = 0;
            m_state = LAST_ROW_STARTED_NEW_GROUP;
          }
          if (m_output_slice != -1) {
            m_join->set_ref_item_slice(m_output_slice);
          }
          return 0;
        }

        // Give the new values to all the new aggregate functions.
        for (Item_sum **item = m_join->sum_funcs; *item != nullptr; ++item) {
          if (m_rollup) {
            if (down_cast<Item_rollup_sum_switcher *>(*item)
                    ->aggregator_add_all()) {
              return 1;
            }
          } else {
            if ((*item)->aggregator_add()) {
              return 1;
            }
          }
        }

        // We're still in the same group, so just loop back.
      }

    case OUTPUTTING_ROLLUP_ROWS:
      SetRollupLevel(m_current_rollup_position - 1);

      if (m_current_rollup_position <= m_last_unchanged_group_item_idx) {
        // Done outputting rollup rows; on next Read() call, deal with the new
        // group instead.
        if (m_seen_eof) {
          m_state = DONE_OUTPUTTING_ROWS;
        } else {
          m_state = LAST_ROW_STARTED_NEW_GROUP;
        }
      }

      if (m_output_slice != -1) {
        m_join->set_ref_item_slice(m_output_slice);
      }
      return 0;

    case DONE_OUTPUTTING_ROWS:
      if (m_save_nullinfo != 0) {
        m_join->restore_fields(m_save_nullinfo);
        m_save_nullinfo = 0;
      }
      SetRollupLevel(INT_MAX);  // Higher-level iterators up above should not
                                // activate any rollup.
      return -1;
  }

  assert(false);
  return 1;
}

void AggregateIterator::SetRollupLevel(int level) {
  if (m_rollup && m_current_rollup_position != level) {
    m_current_rollup_position = level;
    for (Item_rollup_group_item *item : m_join->rollup_group_items) {
      item->set_current_rollup_level(level);
    }
    for (Item_rollup_sum_switcher *item : m_join->rollup_sums) {
      item->set_current_rollup_level(level);
    }
  }
}

bool NestedLoopIterator::Init() {
  if (m_source_outer->Init()) {
    return true;
  }
  m_state = NEEDS_OUTER_ROW;
  if (m_pfs_batch_mode) {
    m_source_inner->EndPSIBatchModeIfStarted();
  }
  return false;
}

int NestedLoopIterator::Read() {
  if (m_state == END_OF_ROWS) {
    return -1;
  }

  for (;;) {  // Termination condition within loop.
    if (m_state == NEEDS_OUTER_ROW) {
      int err = m_source_outer->Read();
      if (err == 1) {
        return 1;  // Error.
      }
      if (err == -1) {
        m_state = END_OF_ROWS;
        return -1;
      }
      if (m_pfs_batch_mode) {
        m_source_inner->StartPSIBatchMode();
      }

      // Init() could read the NULL row flags (e.g., when building a hash
      // map), so unset them before instead of after.
      m_source_inner->SetNullRowFlag(false);

      if (m_source_inner->Init()) {
        return 1;
      }
      m_state = READING_FIRST_INNER_ROW;
    }
    assert(m_state == READING_INNER_ROWS || m_state == READING_FIRST_INNER_ROW);

    int err = m_source_inner->Read();
    if (err != 0 && m_pfs_batch_mode) {
      m_source_inner->EndPSIBatchModeIfStarted();
    }
    if (err == 1) {
      return 1;  // Error.
    }
    if (thd()->killed) {  // Aborted by user.
      thd()->send_kill_message();
      return 1;
    }
    if (err == -1) {
      // Out of inner rows for this outer row. If we are an outer join
      // and never found any inner rows, return a null-complemented row.
      // If not, skip that and go straight to reading a new outer row.
      if ((m_join_type == JoinType::OUTER &&
           m_state == READING_FIRST_INNER_ROW) ||
          m_join_type == JoinType::ANTI) {
        m_source_inner->SetNullRowFlag(true);
        m_state = NEEDS_OUTER_ROW;
        return 0;
      } else {
        m_state = NEEDS_OUTER_ROW;
        continue;
      }
    }

    // An inner row has been found.

    if (m_join_type == JoinType::ANTI) {
      // Anti-joins should stop scanning the inner side as soon as we see
      // a row, without returning that row.
      m_state = NEEDS_OUTER_ROW;
      continue;
    }

    // We have a new row. Semijoins should stop after the first row;
    // regular joins (inner and outer) should go on to scan the rest.
    if (m_join_type == JoinType::SEMI) {
      m_state = NEEDS_OUTER_ROW;
    } else {
      m_state = READING_INNER_ROWS;
    }
    return 0;
  }
}

namespace {

/**
   This is a no-op class with a public interface identical to that of the
   IteratorProfilerImpl class. This allows iterators with internal time
   keeping (such as MaterializeIterator) to use the same code whether
   time keeping is enabled or not. And all the mutators are inlinable no-ops,
   so that there should be no runtime overhead.
*/
class DummyIteratorProfiler final : public IteratorProfiler {
 public:
  struct TimeStamp {};

  static TimeStamp Now() { return TimeStamp(); }

  double GetFirstRowMs() const override {
    assert(false);
    return 0.0;
  }

  double GetLastRowMs() const override {
    assert(false);
    return 0.0;
  }

  uint64_t GetNumInitCalls() const override {
    assert(false);
    return 0;
  }

  uint64_t GetNumRows() const override {
    assert(false);
    return 0;
  }

  /*
     The methods below are non-virtual with the same name and signature as
     in IteratorProfilerImpl. The compiler should thus be able to suppress
     calls to these for iterators without profiling.
  */
  void StopInit([[maybe_unused]] TimeStamp start_time) {}

  void IncrementNumRows([[maybe_unused]] uint64_t materialized_rows) {}

  void StopRead([[maybe_unused]] TimeStamp start_time,
                [[maybe_unused]] bool read_ok) {}
};

/// Calculates a hash for an ImmutableStringWithLength so that it can be used as
/// a key in a hash map.
class ImmutableStringHasher {
 public:
  // This is a marker telling ankerl::unordered_dense that the hash function has
  // good quality.
  using is_avalanching = void;

  uint64_t operator()(ImmutableStringWithLength string) const {
    return ankerl::unordered_dense::hash<std::string_view>()(string.Decode());
  }
};

using materialize_iterator::Operand;
using Operands = Mem_root_array<Operand>;
using hash_map_type = ankerl::unordered_dense::segmented_map<
    ImmutableStringWithLength, LinkedImmutableString, ImmutableStringHasher>;

void reset_hash_map(hash_map_type *hash_map) {
  std::destroy_at(hash_map);
  std::construct_at(hash_map);
}

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
  and furthermore, the ankerl::unordered_dense library, and the chunk
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
          - the hash function used by ankerl::unordered_dense for the in-memory
            hash map based on primary hash
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

  /// Getter, cf. comment for \c m_secondary_overflow
  bool secondary_overflow() const { return m_secondary_overflow; }
  void secondary_overflow_handling_done() {
    m_spill_read_state = ReadingState::SS_NONE;
    m_secondary_overflow = false;
    // free up resources from chunk files and hashmap
    reset_hash_map(m_hash_map);
    // Use Clear over ClearForReuse: we want all space recycled, ClearForReuse
    // doesn't reclaim used space in the kept block (last allocated)
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
  Handles materialization; the first call to Init() will scan the given iterator
  to the end, store the results in a temporary table (optionally with
  deduplication), and then Read() will allow you to read that table repeatedly
  without the cost of executing the given subquery many times (unless you ask
  for rematerialization).

  When materializing, MaterializeIterator takes care of evaluating any items
  that need so, and storing the results in the fields of the outgoing table --
  which items is governed by the temporary table parameters.

  Conceptually (although not performance-wise!), the MaterializeIterator is a
  no-op if you don't ask for deduplication[1], and in some cases (e.g. when
  scanning a table only once), we elide it. However, it's not necessarily
  straightforward to do so by just not inserting the iterator, as the optimizer
  will have set up everything (e.g., read sets, or what table upstream items
  will read from) assuming the materialization will happen, so the realistic
  option is setting up everything as if materialization would happen but not
  actually write to the table; see StreamingIterator for details.

  [1] if we have a UNION DISTINCT or INTERSECT or EXCEPT it is not a no-op
    - for UNION DISTINCT MaterializeIterator de-duplicates rows via a key
      on the materialized table in two ways: a) a unique key if possible or
      a non-unique key on a hash of the row, if not. For details, see
      \c create_tmp_table.
    - INTERSECT and EXCEPE use two ways: a) using
      in-memory hashing (with posible spill to disk), in which case the
  materialized table is keyless, or if this approach overflows, b) using a
  non-unique key on the materialized table, the keys being the hash of the rows.

  MaterializeIterator conceptually materializes iterators, not JOINs or
  Query_expressions. However, there are many details that leak out
  (e.g., setting performance schema batch mode, slices, reusing CTEs,
  etc.), so we need to send them in anyway.

  'Profiler' should be 'IteratorProfilerImpl' for 'EXPLAIN ANALYZE' and
  'DummyIteratorProfiler' otherwise. It is implemented as a a template
  parameter rather than a pointer to a base class in order to minimize
  the impact this probe has on normal query execution.
 */
template <typename Profiler>
class MaterializeIterator final : public TableRowIterator {
 public:
  /**
    @param thd Thread handler.
    @param operands List of operands (aka query blocks) to materialize.
    @param path_params MaterializePath settings.
    @param table_iterator Iterator used for scanning the temporary table
      after materialization.
    @param join
      When materializing within the same JOIN (e.g., into a temporary table
      before sorting), as opposed to a derived table or a CTE, we may need
      to change the slice on the join before returning rows from the result
      table. If so, join and ref_slice would need to be set, and
      query_blocks_to_materialize should contain only one member, with the same
      join.
   */
  MaterializeIterator(THD *thd, Operands operands,
                      const MaterializePathParameters *path_params,
                      unique_ptr_destroy_only<RowIterator> table_iterator,
                      JOIN *join);

  bool Init() override;
  int Read() override;

  void SetNullRowFlag(bool is_null_row) override {
    m_table_iterator->SetNullRowFlag(is_null_row);
  }

  void StartPSIBatchMode() override { m_table_iterator->StartPSIBatchMode(); }
  void EndPSIBatchModeIfStarted() override;

  // The temporary table is private to us, so there's no need to worry about
  // locks to other transactions.
  void UnlockRow() override {}

  const IteratorProfiler *GetProfiler() const override {
    assert(thd()->lex->is_explain_analyze);
    return &m_profiler;
  }

  const Profiler *GetTableIterProfiler() const {
    return &m_table_iter_profiler;
  }

 private:
  Operands m_operands;
  unique_ptr_destroy_only<RowIterator> m_table_iterator;

  /// If we are materializing a CTE, points to it (otherwise nullptr).
  /// Used so that we see if some other iterator already materialized the table,
  /// avoiding duplicate work.
  Common_table_expr *m_cte;

  /// The query expression we are materializing. For derived tables,
  /// we materialize the entire query expression; for materialization within
  /// a query expression (e.g. for sorting or for windowing functions),
  /// we materialize only parts of it. Used to clear correlated CTEs within
  /// the unit when we rematerialize, since they depend on values from
  /// outside the query expression, and those values may have changed
  /// since last materialization.
  Query_expression *m_query_expression;

  /// See constructor.
  JOIN *const m_join;

  /// The slice to set when accessing temporary table; used if anything upstream
  /// (e.g. WHERE, HAVING) wants to evaluate values based on its contents.
  /// See constructor.
  const int m_ref_slice;

  /// If true, we need to materialize anew for each Init() (because the contents
  /// of the table will depend on some outer non-constant value).
  const bool m_rematerialize;

  /// See constructor.
  const bool m_reject_multiple_rows;

  /// See constructor.
  const ha_rows m_limit_rows;

  struct Invalidator {
    const CacheInvalidatorIterator *iterator;
    int64_t generation_at_last_materialize;
  };
  Mem_root_array<Invalidator> m_invalidators;

  /**
     Profiling data for this iterator. Used for 'EXPLAIN ANALYZE'.
     Note that MaterializeIterator merely (re)materializes a set of rows.
     It delegates the task of iterating over those rows to m_table_iterator.
     m_profiler thus records:
     - The total number of rows materialized (for the initial
       materialization and any subsequent rematerialization).
     - The total time spent on all materializations.

     It does not measure the time spent accessing the materialized rows.
     That is handled by m_table_iter_profiler. The example below illustrates
     what 'EXPLAIN ANALYZE' output will be like. (Cost-data has been removed
     for the sake of simplicity.) The second line represents the
     MaterializeIterator that materializes x1, and the first line represents
     m_table_iterator, which is a TableScanIterator in this example.

     -> Table scan on x1 (actual time=t1..t2 rows=r1 loops=l1)
         -> Materialize CTE x1 if needed (actual time=t3..t4 rows=r2 loops=l2)

     t3 is the average time (across l2 materializations) spent materializing x1.
     Since MaterializeIterator does no iteration, we always set t3=t4.
     'actual time' is cumulative, so that the values for an iterator should
     include the time spent in all its descendants. Therefore we know that
     t1*l1>=t3*l2 . (Note that t1 may be smaller than t3. We may re-scan x1
     repeatedly without rematerializing it. Restarting a scan is quick, bringing
     the average time for fetching the first row (t1) down.)
  */
  Profiler m_profiler;

  /**
     Profiling data for m_table_iterator. 'this' is a descendant of
     m_table_iterator in 'EXPLAIN ANALYZE' output, and 'elapsed time'
     should be cumulative. Therefore, m_table_iter_profiler will measure
     the sum of the time spent materializing the result rows and iterating
     over those rows.
  */
  Profiler m_table_iter_profiler;

  /// Use a hash map to implement row matching for set operations if true
  bool m_use_hash_map{true};

  // Iff m_use_hash_map, the MEM_ROOT on which all of the hash map keys and
  // values are allocated. The actual hash map is on the regular heap.
  unique_ptr_destroy_only<MEM_ROOT> m_mem_root;
  size_t m_row_size_upper_bound{0};

  // The hash map where the rows are stored.
  std::unique_ptr<hash_map_type> m_hash_map;

  // The number of rows stored in m_hash_map (<= rows read in left operand)
  // (before spill happens).
  size_t m_rows_in_hash_map{0};

  // The number of rows read from left operand (before spill happens)
  size_t m_read_rows_before_dedup{0};

  /// Used to keep track of the current hash table entry focus after insertion
  /// or lookup.
  hash_map_type::iterator m_hash_map_iterator;

  /// Holds encoded row (if any) stored in the hash table
  LinkedImmutableString m_next_ptr{nullptr};

  /// last found row in hash map
  LinkedImmutableString m_last_row{nullptr};

  /// Needed for interfacing hash join function by hash set ops. We only ever
  /// have one table (the resulting tmp table of the set operation).
  TableCollection m_table_collection;

  /// Spill to disk state for set operation: when in-memory hash map
  /// overflows, this keeps track of state.
  SpillState m_spill_state;

  /// Whether we are deduplicating using a hash field on the temporary
  /// table. (This condition mirrors check_unique_fields().)
  /// If so, we compute a hash value for every row, look up all rows with
  /// the same hash and manually compare them to the row we are trying to
  /// insert.
  ///
  /// Note that this is _not_ the common way of deduplicating as we go.
  /// The common method is to have a regular index on the table
  /// over the right columns, and in that case, ha_write_row() will fail
  /// with an ignorable error, so that the row is ignored even though
  /// check_unique_fields() is not called. However, B-tree indexes
  /// have limitations, in particular on length, that sometimes require us
  /// to do this instead. See create_tmp_table() for details.
  bool doing_hash_deduplication() const { return table()->hash_field; }

  bool MaterializeRecursive();
  bool MaterializeOperand(const Operand &operand, ha_rows *stored_rows);
  int read_next_row(const Operand &operand);
  bool check_unique_fields_hash_map(TABLE *t, bool write, bool *found,
                                    bool *spill);
  void backup_or_restore_blob_pointers(bool backup);
  void update_row_in_hash_map();
  bool store_row_in_hash_map(bool *single_row_too_large);
  bool handle_hash_map_full(const Operand &operand, ha_rows *stored_rows,
                            bool single_row_too_large);
  bool process_row(const Operand &operand, Operands &operands, TABLE *t,
                   uchar *set_counter_0, uchar *set_counter_1, bool *read_next);
  bool process_row_hash(const Operand &operand, TABLE *t, ha_rows *stored_rows);
  bool materialize_hash_map(TABLE *t, ha_rows *stored_rows,
                            bool single_row_too_large);
  bool load_HF_row_into_hash_map();
  bool init_hash_map_for_new_exec();
  friend class SpillState;
};

}  // namespace

template <typename Profiler>
MaterializeIterator<Profiler>::MaterializeIterator(
    THD *thd, Operands operands, const MaterializePathParameters *path_params,
    unique_ptr_destroy_only<RowIterator> table_iterator, JOIN *join)
    : TableRowIterator(thd, path_params->table),
      m_operands(std::move(operands)),
      m_table_iterator(std::move(table_iterator)),
      m_cte(path_params->cte),
      m_query_expression(path_params->unit),
      m_join(join),
      m_ref_slice(path_params->ref_slice),
      m_rematerialize(path_params->rematerialize),
      m_reject_multiple_rows(path_params->reject_multiple_rows),
      m_limit_rows(path_params->limit_rows),
      m_invalidators(thd->mem_root),
      m_use_hash_map(
          !table()->is_union_or_table() &&
          thd->optimizer_switch_flag(OPTIMIZER_SWITCH_HASH_SET_OPERATIONS)),
      m_spill_state(thd, thd->mem_root) {
  assert(m_limit_rows == HA_POS_ERROR /* EXCEPT, INTERCEPT */ ||
         path_params->table->is_union_or_table());
  if (m_ref_slice != -1) {
    assert(m_join != nullptr);
  }
  if (m_join != nullptr) {
    assert(m_operands.size() == 1);
    assert(m_operands[0].join == m_join);
  }
  if (path_params->invalidators != nullptr) {
    for (const AccessPath *invalidator_path : *path_params->invalidators) {
      // We create iterators left-to-right, so we should have created the
      // invalidators before this.
      assert(invalidator_path->iterator != nullptr);
      /*
        Add a cache invalidator that must be checked on every Init().
        If its generation has increased since last materialize, we need to
        rematerialize even if m_rematerialize is false.
      */
      m_invalidators.push_back(
          Invalidator{down_cast<CacheInvalidatorIterator *>(
                          invalidator_path->iterator->real_iterator()),
                      /*generation_at_last_materialize=*/-1});

      // If we're invalidated, the join also needs to invalidate all of its
      // own materialization operations, but it will automatically do so by
      // virtue of the Query_block being marked as uncachable
      // (create_iterators() always sets rematerialize=true for such cases).
    }
  }
}

template <typename Profiler>
bool MaterializeIterator<Profiler>::Init() {
  const typename Profiler::TimeStamp start_time = Profiler::Now();

  if (!table()->materialized && table()->pos_in_table_list != nullptr &&
      table()->pos_in_table_list->is_view_or_derived()) {
    // Create the table if it's the very first time.
    //
    // TODO(sgunders): create_materialized_table() calls
    // instantiate_tmp_table(), and then has some logic to deal with more
    // complicated cases like multiple reference to the same CTE.
    // Consider unifying this with the instantiate_tmp_table() case below
    // (which is used for e.g. materialization for sorting).
    if (table()->pos_in_table_list->create_materialized_table(thd())) {
      return true;
    }
  }

  // If this is a CTE, it could be referred to multiple times in the same query.
  // If so, check if we have already been materialized through any of our alias
  // tables.
  const bool use_shared_cte_materialization =
      !table()->materialized && m_cte != nullptr && !m_rematerialize &&
      any_of(m_cte->tmp_tables.begin(), m_cte->tmp_tables.end(),
             [](const Table_ref *table_ref) {
               return table_ref->table != nullptr &&
                      table_ref->table->materialized;
             });

  if (use_shared_cte_materialization) {
    // If using an already materialized shared CTE table, update the
    // invalidators with the latest generation.
    for (Invalidator &invalidator : m_invalidators) {
      invalidator.generation_at_last_materialize =
          invalidator.iterator->generation();
    }
    table()->materialized = true;
  }

  if (table()->materialized) {
    bool rematerialize = m_rematerialize;

    if (!rematerialize && !use_shared_cte_materialization) {
      // See if any lateral tables that we depend on have changed since
      // last time (which would force a rematerialization).
      //
      // TODO: It would be better, although probably much harder, to check
      // the actual column values instead of just whether we've seen any
      // new rows.
      for (const Invalidator &invalidator : m_invalidators) {
        if (invalidator.iterator->generation() !=
            invalidator.generation_at_last_materialize) {
          rematerialize = true;
          break;
        }
      }
    }

    if (!rematerialize) {
      // Just a rescan of the same table.
      const bool err = m_table_iterator->Init();
      m_table_iter_profiler.StopInit(start_time);
      return err;
    }
  }
  table()->set_not_started();

  if (!table()->is_created()) {
    if (instantiate_tmp_table(thd(), table())) {
      return true;
    }
    empty_record(table());
  } else {
    table()->file->ha_index_or_rnd_end();  // @todo likely unneeded => remove
    table()->file->ha_delete_all_rows();
    if (m_use_hash_map && init_hash_map_for_new_exec()) return true;
  }

  if (m_query_expression != nullptr)
    if (m_query_expression->clear_correlated_query_blocks()) return true;

  if (m_cte != nullptr) {
    // This is needed in a special case. Consider:
    // SELECT FROM ot WHERE EXISTS(WITH RECURSIVE cte (...)
    //                             SELECT * FROM cte)
    // and assume that the CTE is outer-correlated. When EXISTS is
    // evaluated, Query_expression::ClearForExecution() calls
    // clear_correlated_query_blocks(), which scans the WITH clause and clears
    // the CTE, including its references to itself in its recursive definition.
    // But, if the query expression owning WITH is merged up, e.g. like this:
    // FROM ot SEMIJOIN cte ON TRUE,
    // then there is no Query_expression anymore, so its WITH clause is
    // not reached. But this "lateral CTE" still needs comprehensive resetting.
    // That's done here.
    if (m_cte->clear_all_references()) return true;
  }

  // If we are removing duplicates by way of a hash field
  // (see doing_hash_deduplication() for an explanation), we need to
  // initialize scanning of the index over that hash field. (This is entirely
  // separate from any index usage when reading back the materialized table;
  // m_table_iterator will do that for us.)
  auto end_unique_index = create_scope_guard([&] {
    if (table()->file->inited == handler::INDEX) table()->file->ha_index_end();
  });

  if (doing_hash_deduplication()) {
    if (m_use_hash_map) {
      if (!table()->uses_hash_map()) {
        // The value of the user variable 'hash_set_operations' has changed to
        // true, so we re-open the resulting tmp table without index: we will
        // deduplicate set operation with hashing.
        assert(table()->s->keys == 1);
        close_tmp_table(table());
        table()->s->keys = 0;  // don't need key for hash based dedup
        table()->set_use_hash_map(true);
        if (instantiate_tmp_table(thd(), table())) return true;
      }
    } else {
      if (table()->uses_hash_map()) {
        // The value of the user variable 'hash_set_operations' has changed to
        // false, so we re-open the resulting tmp table with index so we can
        // perform set operation de-duplication using index.
        assert(table()->s->keys == 0);
        close_tmp_table(table());
        table()->s->keys = 1;  // activate key index de-duplication
        table()->set_use_hash_map(false);
        if (instantiate_tmp_table(thd(), table())) return true;
      }
      if (table()->file->ha_index_init(0, /*sorted=*/false)) {
        return true;
      }
    }
  }

  ha_rows stored_rows = 0;

  if (m_query_expression != nullptr && m_query_expression->is_recursive()) {
    if (MaterializeRecursive()) return true;
  } else {
    for (const Operand &operand : m_operands) {
      if (MaterializeOperand(operand, &stored_rows)) {
        return true;
      }
      if (table()->is_union_or_table()) {
        // For INTERSECT and EXCEPT, this is done in TableScanIterator
        if (m_reject_multiple_rows && stored_rows > 1) {
          my_error(ER_SUBQUERY_NO_1_ROW, MYF(0));
          return true;
        } else if (stored_rows >= m_limit_rows) {
          break;
        }
      } else {
        // INTERSECT, EXCEPT: no rows in left operand: no need to process more
        if (m_use_hash_map) {
          if (m_hash_map == nullptr) break;
        } else {
          if (stored_rows == 0) break;
        }
      }
    }
  }

  end_unique_index.reset();
  table()->materialized = true;

  if (!m_rematerialize) {
    DEBUG_SYNC(thd(), "after_materialize_derived");
  }

  for (Invalidator &invalidator : m_invalidators) {
    invalidator.generation_at_last_materialize =
        invalidator.iterator->generation();
  }

  m_profiler.StopInit(start_time);
  const bool err = m_table_iterator->Init();
  m_table_iter_profiler.StopInit(start_time);
  /*
    MaterializeIterator reads all rows during Init(), so we do not measure
    the time spent on individual read operations.
  */
  m_profiler.IncrementNumRows(stored_rows);
  return err;
}

/**
  Recursive materialization happens much like regular materialization,
  but some steps are repeated multiple times. Our general strategy is:

    1. Materialize all non-recursive query blocks, once.

    2. Materialize all recursive query blocks in turn.

    3. Repeat #2 until no query block writes any more rows (ie., we have
       converged) -- for UNION DISTINCT queries, rows removed by deduplication
       do not count. Each materialization sees only rows that were newly added
       since the previous iteration; see FollowTailIterator for more details
       on the implementation.

  Note that the result table is written to while other iterators are still
  reading from it; again, see FollowTailIterator. This means that each run
  of #2 can potentially run many actual CTE iterations -- possibly the entire
  query to completion if we have only one query block.

  This is not how the SQL standard specifies recursive CTE execution
  (it assumes building up the new result set from scratch for each iteration,
  using the previous iteration's results), but it is equivalent, and more
  efficient for the class of queries we support, since we don't need to
  re-create the same rows over and over again.
 */
template <typename Profiler>
bool MaterializeIterator<Profiler>::MaterializeRecursive() {
  /*
    For RECURSIVE, beginners will forget that:
    - the CTE's column types are defined by the non-recursive member
    - which implies that recursive member's selected expressions are cast to
    the non-recursive member's type.
    That will cause silent truncation and possibly an infinite recursion due
    to a condition like: 'LENGTH(growing_col) < const', or,
    'growing_col < const',
    which is always satisfied due to truncation.

    This situation is similar to
    create table t select "x" as a;
    insert into t select concat("x",a) from t;
    which sends ER_DATA_TOO_LONG in strict mode.

    So we should inform the user.

    If we only raised warnings: it will not interrupt an infinite recursion,
    a MAX_RECURSION hint (if we featured one) may interrupt; but then the
    warnings won't be seen, as the interruption will raise an error. So
    warnings are useless.
    Instead, we send a truncation error: it is visible, indicates the
    source of the problem, and is consistent with the INSERT case above.

    Usually, truncation in SELECT triggers an error only in
    strict mode; but if we don't send an error we get a runaway query;
    and as WITH RECURSIVE is a new feature we don't have to carry the
    permissiveness of the past, so we send an error even if in non-strict
    mode.

    For a non-recursive UNION, truncation shouldn't happen as all UNION
    members participated in type calculation.
  */
  Strict_error_handler strict_handler(
      Strict_error_handler::ENABLE_SET_SELECT_STRICT_ERROR_HANDLER);
  enum_check_fields save_check_for_truncated_fields{};
  bool set_error_handler = thd()->is_strict_mode();
  if (set_error_handler) {
    save_check_for_truncated_fields = thd()->check_for_truncated_fields;
    thd()->check_for_truncated_fields = CHECK_FIELD_WARN;
    thd()->push_internal_handler(&strict_handler);
  }
  auto cleanup_handler = create_scope_guard(
      [this, set_error_handler, save_check_for_truncated_fields] {
        if (set_error_handler) {
          thd()->pop_internal_handler();
          thd()->check_for_truncated_fields = save_check_for_truncated_fields;
        }
      });

  ha_rows stored_rows = 0;

  // Give each recursive iterator access to the stored number of rows
  // (see FollowTailIterator::Read() for details).
  for (const Operand &operand : m_operands) {
    if (operand.is_recursive_reference) {
      operand.recursive_reader->set_stored_rows_pointer(&stored_rows);
    }
  }

#ifndef NDEBUG
  // Trash the pointers on exit, to ease debugging of dangling ones to the
  // stack.
  auto pointer_cleanup = create_scope_guard([this] {
    for (const Operand &operand : m_operands) {
      if (operand.is_recursive_reference) {
        operand.recursive_reader->set_stored_rows_pointer(nullptr);
      }
    }
  });
#endif

  // First, materialize all non-recursive operands.
  for (const Operand &operand : m_operands) {
    if (!operand.is_recursive_reference) {
      if (MaterializeOperand(operand, &stored_rows)) return true;
    }
  }

  // Then, materialize all recursive query blocks until we converge.
  Opt_trace_context &trace = thd()->opt_trace;
  bool disabled_trace = false;
  ha_rows last_stored_rows;
  do {
    last_stored_rows = stored_rows;
    for (const Operand &operand : m_operands) {
      if (operand.is_recursive_reference) {
        if (MaterializeOperand(operand, &stored_rows)) return true;
      }
    }

    /*
      If recursive query blocks have been executed at least once, and repeated
      executions should not be traced, disable tracing, unless it already is
      disabled.
    */
    if (!disabled_trace &&
        !trace.feature_enabled(Opt_trace_context::REPEATED_SUBSELECT)) {
      trace.disable_I_S_for_this_and_children();
      disabled_trace = true;
    }
  } while (stored_rows > last_stored_rows);

  m_profiler.IncrementNumRows(stored_rows);

  if (disabled_trace) {
    trace.restore_I_S();
  }
  return false;
}

/**
  Walk through de-duplicated rows from in-memory hash table and/or spilled
  overflow HF chunks [1] and write said rows to table t, updating stored_rows
  counter.

  [1] Depending on spill state. We have four cases:

  a) No spill to disk: write rows from in-memory hash table
  b) Spill to disk: write completed HF chunks, all chunks exist in the same
     generation >= 2 (the number is the same as the number of set operands).
  c) We saw secondary overflow during spill processing and must recover: write
     completed HF chunks (mix of 1. and 2.generation) and write the in-memory
     hash table
  d) too large single row, move to index based de-duplication
  @param t            output table
  @param stored_rows  counter for # of rows stored in output table
  @param single_row_too_large
                      move straight to index based de-duplication
  @returns true on error
*/
template <typename Profiler>
bool MaterializeIterator<Profiler>::materialize_hash_map(
    TABLE *t, ha_rows *stored_rows, bool single_row_too_large) {
  // b)
  if (m_spill_state.spill()) {
    if (m_spill_state.secondary_overflow()) {
      // c) write finished HF chunks (a strict subset with secondary overflow)
      //
      // Write all de-duplicated rows directly to output file. These rows have
      // set counters initialized. Steps are:
      //
      // 1. We can write all 1. generation HF files which we didn't get to when
      // we saw the secondary overflow.  These are the ones "after" [1] the
      // current logical chunk, whose {set_idx, chunk_idx} > {current_set_idx,
      // current_chunk_idx}, but since we haven't read them we dispose of them
      // first so read positions will be correct for 2. generation chunks.
      //
      // 2. Write 2. generation HF chunks which already have all rows from left
      // operand (of their hash bucket), so we can write all those as
      // well. These are the ones "before" [1] the current logical chunk whose
      // processing blew up, i.e. those which have {set_idx, chunk_idx} <
      // {current_set_idx, current_chunk_idx}
      //
      // 3. The rows in the current in-memory hash table, these are ready for
      // writing, as they already have counters.
      //
      // [1] see explanatory diagram in write_partially_completed_HFs
      //
      // As the next phase in the recovery process for secondary overflow, we
      // switch to reading in not de-duplicated rows and match those against
      // the index in the output table (as done for UNION).
      //
      // The rows that still need reading and de-duplicating are:
      //
      // 4. offending row (i.e. the one which caused the memory overflow - we
      // saved a copy),
      // 5. any remaining rows in the current (left operand) IF
      // chunk file, and
      // 6. all rows in the remaining (left) IF chunk files.
      //
      // We handle 1,2 and 3 here, the rest are handled by
      // read_next_row_secondary_overflow when that gets called as part of
      // normal (no longer hashing) processing of left operand rows.  Right
      // operand(s) are then processed as normal.
      //
      if (m_spill_state.write_partially_completed_HFs(thd(), m_operands,
                                                      stored_rows))
        return true;
      // Next, write the de-duplicated rows in in-memory hash table, case c)
    } else {
      // b) Read finished HF chunks and write them, nothing remains in in-memory
      // hash table, so we can return when this is done
      return m_spill_state.write_completed_HFs(thd(), m_operands, stored_rows);
    }
  }

  if (m_hash_map == nullptr) return false;  // left operand is empty

  // a), c), d)
  for (const auto &[hash_key, first_row] : *m_hash_map) {
    if (*stored_rows >= m_limit_rows) break;

    for (LinkedImmutableString row_with_same_hash = first_row;
         row_with_same_hash != nullptr;
         row_with_same_hash = row_with_same_hash.Decode().next) {
      hash_join_buffer::LoadImmutableStringIntoTableBuffers(m_table_collection,
                                                            row_with_same_hash);
      int error = t->file->ha_write_row(t->record[0]);
      if (error == 0) {
        ++*stored_rows;
      } else {
        // create_ondisk_from_heap will generate error if needed.
        assert(!t->file->is_ignorable_error(error));
        if (create_ondisk_from_heap(thd(), t, error,
                                    /*insert_last_record=*/true,
                                    /*ignore_last_dup=*/true, nullptr))
          return true; /* purecov: inspected */
        if (m_spill_state.secondary_overflow() || single_row_too_large) {
          // c), d)
          assert(t->s->keys == 1);
          if (t->file->ha_index_init(0, false) != 0) return true;
        } else {
          // else: a) we use hashing, so skip ha_index_init
          assert(t->s->keys == 0);
        }
        ++*stored_rows;

        // Inform each reader that the table has changed under their feet,
        // so they'll need to reposition themselves.
        for (const Operand &operand : m_operands) {
          if (operand.is_recursive_reference) {
            operand.recursive_reader->RepositionCursorAfterSpillToDisk();
          }
        }
      }
    }
  }

  return false;
}

/**
  We just read a row from a HF chunk file. Now, insert it into the hash map
  to prepare for the set operation with another operand, in IF chunk files.
  @returns true on error
*/
template <typename Profiler>
bool MaterializeIterator<Profiler>::load_HF_row_into_hash_map() {
  bool found = false;
  bool spill = false;
  if (check_unique_fields_hash_map(table(), /*write*/ true, &found, &spill))
    return true;

  // Since the HF chunk files only contain unique rows, we assert that the row
  // is not already in the hash map
  assert(!found);

  if (spill) {
    // It fit before, should fit now
    assert(false);
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
             thd()->variables.set_operations_buffer_size);
    return true;
  }

  bool dummy;
  spill = store_row_in_hash_map(&dummy);
  if (spill) {
    // It fit before, should fit now
    assert(false);
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
             thd()->variables.set_operations_buffer_size);
    return true;
  }

  return false;
}

// Reset the mem_root used for the in-memory hash table for set operations.  On
// the initial call, free all space and reallocate one single block big enough
// to hold the allocated space from the first usage of the mem_root. This
// avoids issues with fragmentation being different in different rounds
// re-reading chunk files, and allows us to avoid de/re-allocating the space on
// subsequent rounds; ClearForReuse will just trash the single large block, but
// not free it.
static bool reset_mem_root(MEM_ROOT *mem_root,
                           bool initial [[maybe_unused]] = false) {
#if !defined(HAVE_VALGRIND) && !defined(HAVE_ASAN)
  if (initial) {
    // reallocate the total space used as one block, should give more
    // efficient allocation and less fragmentation when we re-read chunk files
    const size_t heap_size = mem_root->allocated_size();
    mem_root->Clear();
    mem_root->set_max_capacity(0);
    if (mem_root->ForceNewBlock(heap_size)) return true;  // malloc failed
  } else {
    // If we allocate with ASAN, my_alloc.cc will allocate from OS every single
    // requested block, and also not reuse any block since ClearForReuse just
    // calls Clean. So we cannot assert.
    assert(mem_root->IsSingleBlock());
    // should not deallocate the (single) large block:
    mem_root->ClearForReuse();
    mem_root->set_max_capacity(0);
  }
#else
  // No point in the above; ClearForReuse just calls Clear, no reuse of blocks
  // and the assert would fail. See my_alloc.cc use of MEM_ROOT_SINGLE_CHUNKS
  mem_root->Clear();
  mem_root->set_max_capacity(0);
#endif
  return false;
}

template <typename Profiler>
bool MaterializeIterator<Profiler>::init_hash_map_for_new_exec() {
  if (m_hash_map == nullptr) return false;  // not used yet
  reset_hash_map(m_hash_map.get());
  if (reset_mem_root(m_mem_root.get())) return true;
  m_rows_in_hash_map = 0;
  return false;
}

/**
  Save (or restore) blob pointers in \c Field::m_blob_backup. We need to have
  two full copies of a record for comparison, so we save record[0] to record[1]
  before reading from the hash table with LoadImmutableStringIntoTableBuffers.
  This will only work correctly for blobs if we also save the blob pointers
  lest they be clobbered when reading from the hash table, which reestablishes
  a full record in record[0] and resets Field blob pointers based on
  record[0]'s blob pointers. By saving them here we make sure that record[1]'s
  blob pointers do not point to overwritten or deallocated space.

  @param backup   If true, do backup, else restore blob pointers
*/
template <typename Profiler>
void MaterializeIterator<Profiler>::backup_or_restore_blob_pointers(
    bool backup) {
  assert(m_table_collection.tables().size() == 1);
  const pack_rows::Table &tbl = m_table_collection.tables()[0];

  for (const pack_rows::Column &column : tbl.columns) {
    if (column.field->is_flag_set(BLOB_FLAG) || column.field->is_array()) {
      Field_blob *bf = down_cast<Field_blob *>(column.field);
      if (backup)
        bf->backup_blob_field();
      else
        bf->restore_blob_backup();
    }
  }
}

/**
  Check presence of row in hash map, and make hash map iterator ready
  for writing value. If we find the row or prepare a write, we set

    - m_hash_map_iterator
    - m_last_row
    - m_next_ptr

  for use by companion method \c store_row_in_hash_map.

  @param[in]  t       the source table
  @param[in]  write   if true, prepare a new write of row in hash map if
                      not already there (left block only)
  @param[out] found   set to true if the row was found in hash map
  @param[out] spill   set to true of we ran out of space
  @returns true on error
*/
template <typename Profiler>
bool MaterializeIterator<Profiler>::check_unique_fields_hash_map(TABLE *t,
                                                                 bool write,
                                                                 bool *found,
                                                                 bool *spill) {
  const size_t max_mem_available = thd()->variables.set_operations_buffer_size;

  *spill = false;

#ifndef NDEBUG
  if (m_spill_state.spill()) {
    if (write &&  // Only inject error for left operand: can only happen there
        m_spill_state.read_state() ==
            SpillState::ReadingState::SS_READING_LEFT_IF &&
        m_spill_state.simulated_secondary_overflow(spill))
      return true;
    if (*spill) return false;
  }
#endif
  if (m_mem_root == nullptr) {
    assert(write);
    m_mem_root = make_unique_destroy_only<MEM_ROOT>(
        thd()->mem_root, key_memory_hash_op, /* blocksize 16K */ 16384);
    if (m_mem_root == nullptr) return true;
    m_hash_map.reset(new hash_map_type());

    if (m_hash_map == nullptr) {
      my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), sizeof(hash_map_type));
      return true;
    }

    Prealloced_array<TABLE *, 4> ta(key_memory_hash_op, 1);
    ta[0] = t;
    TableCollection tc(ta, false, 0, 0);
    m_table_collection = tc;
    if (!m_table_collection.has_blob_column()) {
      m_row_size_upper_bound =
          ComputeRowSizeUpperBoundSansBlobs(m_table_collection);
    }
  }

  ulonglong primary_hash = 0;
  if (m_spill_state.read_state() != SpillState::ReadingState::SS_NONE) {
    // We have read this row from a chunk file, so we know its hash already
    primary_hash = static_cast<ulonglong>(t->hash_field->val_int());
    assert(primary_hash == calc_row_hash(t));
  } else {
    // Create the key, a hash of the record
    primary_hash = calc_row_hash(t);
    // Save hash field in record: avoids rehash of HF-k chunk files when read
    // but takes unnecessary space in memory hash map.
    t->hash_field->store(static_cast<longlong>(primary_hash), true);
  }
  // A secondary hash based on primary hash.
  ImmutableStringWithLength secondary_hash_key;
  const size_t required_key_bytes =
      ImmutableStringWithLength::RequiredBytesForEncode(sizeof(primary_hash));
  std::pair<char *, char *> block = m_mem_root->Peek();
  if (static_cast<size_t>(block.second - block.first) < required_key_bytes) {
    // No room in this block; ask for a new one and try again.
    m_mem_root->ForceNewBlock(required_key_bytes);
    block = m_mem_root->Peek();
  }
  size_t bytes_to_commit = 0;
  if (static_cast<size_t>(block.second - block.first) >= required_key_bytes) {
    char *ptr = block.first;
    secondary_hash_key = ImmutableStringWithLength::Encode(
        pointer_cast<const char *>(&primary_hash), sizeof(primary_hash), &ptr);
    assert(ptr < block.second);
    bytes_to_commit = ptr - block.first;
  } else if (write) {
    assert(m_spill_state.read_state() == SpillState::ReadingState::SS_NONE);
    // spill to disk
    *spill = true;
    return false;
  }

  *found = false;
  bool inserted = false;
  if (write) {
    std::pair<hash_map_type::iterator, bool> key_it_and_inserted;
    try {
      key_it_and_inserted = m_hash_map->emplace(secondary_hash_key,
                                                LinkedImmutableString{nullptr});
    } catch (const std::overflow_error &) {
      // This can only happen if the hash function is extremely bad
      // (should never happen in practice).
      my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
               thd()->variables.set_operations_buffer_size);
      return true;
    }
    m_hash_map_iterator = key_it_and_inserted.first;
    inserted = key_it_and_inserted.second;
    *found = false;
  } else {
    m_hash_map_iterator = m_hash_map->find(secondary_hash_key);
    if (m_hash_map_iterator == m_hash_map->end()) {
      return false;
    }
  }
  m_next_ptr = LinkedImmutableString(nullptr);

  if (inserted) {
    // We inserted an element, so the hash map may have grown.
    // Update the capacity available for the MEM_ROOT; our total may
    // have gone slightly over already, and if so, we will signal
    // that and immediately start spilling to disk.
    const size_t bytes_used =
        m_hash_map->bucket_count() * sizeof(hash_map_type::bucket_type) +
        m_hash_map->values().capacity() *
            sizeof(hash_map_type::value_container_type::value_type);
    if (bytes_used >= max_mem_available) {
      // spill to disk
      *spill = true;
      return false;
    }
    m_mem_root->set_max_capacity(max_mem_available - bytes_used);

    // We need to keep this key.
    m_mem_root->RawCommit(bytes_to_commit);
  } else {
    // Check if rows are equal.
    // We have the record we just read in record[0], save that into record[1]
    // because LoadImmutableStringIntoTableBuffers will instantiate into
    // record[0], then compare record[1] against record[0].
    store_record(t, record[1]);
    if (m_table_collection.has_blob_column()) {
      // LoadImmutableStringIntoTableBuffers will destroy/clobber any blob we
      // have in record[0] lest we change the pointer, so save blob pointers in
      // record[0].
      backup_or_restore_blob_pointers(/*backup=*/true);
    }
    m_last_row = m_hash_map_iterator->second;
    while (m_last_row != nullptr) {
      hash_join_buffer::LoadImmutableStringIntoTableBuffers(m_table_collection,
                                                            m_last_row);
      if (!table_rec_cmp(t)) {
        *found = true;
        break;
      }
      m_last_row = m_last_row.Decode().next;
    }

    if (m_table_collection.has_blob_column()) {
      backup_or_restore_blob_pointers(/*backup=*/false);
    }
    if (!*found) {
      // We didn't find the record just read in the hash table, so our caller
      // will want to "write" it to the hash table, which means we need it back
      // in record[0] since that is where it is expected to be found.
      restore_record(t, record[1]);
    } else {
      // We found it, and our caller will want to update counter in the record
      // we just retrieved from the hash table and then update the hash table's
      // record , so we must leave the hash table's version of the record in
      // place in record[0]
    }

    // We already have another element with the same key, so our insert
    // failed, put the new value in the hash bucket, but keep track of
    // what the old one was; it will be our “next” pointer.
    m_next_ptr = m_hash_map_iterator->second;
  }

  return false;
}

/// Handle the situation that the in-memory hash map is full.
/// @param operand       the left operand
/// @param stored_rows   pointer to the number of stored rows on the output tmp
///                      table
/// @param single_row_too_large
///                      if true, we found a (blob) row so large compared to
///                      set_operations_buffer_size, that we do not attempt
///                      spill handling, move directly to index based
///                      de-duplication
/// @return true on error, false on success
template <typename Profiler>
bool MaterializeIterator<Profiler>::handle_hash_map_full(
    const Operand &operand, ha_rows *stored_rows, bool single_row_too_large) {
  if (thd()->is_error()) return true;
  if (m_spill_state.spill() || single_row_too_large) {
    assert((m_spill_state.spill() && !single_row_too_large) ||
           (!m_spill_state.spill() && single_row_too_large));
    Opt_trace_context *trace = &thd()->opt_trace;
    Opt_trace_object trace_wrapper(trace);
    Opt_trace_object trace_exec(
        trace, m_spill_state.spill()
                   ? "spill handling overflow, reverting to index"
                   : "spill handling not attempted due to large row, reverting "
                     "to index");
    Opt_trace_array trace_steps(trace, "steps");
    m_use_hash_map = false;

    if (m_spill_state.spill()) {
      m_spill_state.set_secondary_overflow();
      // Save current row for later use, see save_operand_to_IF_chunk_files
      if (m_spill_state.save_offending_row()) return true;
    }

    TABLE *const t = table();
    close_tmp_table(t);
    t->s->keys = 1;  // activate hash key index
    t->set_use_hash_map(false);
    if (instantiate_tmp_table(thd(), t)) return true;
    if (t->file->ha_index_init(0, false) != 0) return true;

    if (materialize_hash_map(t, stored_rows, single_row_too_large)) return true;
    return false;
  }
  if (m_spill_state.init(operand, m_hash_map.get(), m_rows_in_hash_map,
                         m_read_rows_before_dedup, m_mem_root.get(), table()))
    return true;

  return false;
}

/**
  Store the current row image into the hash map. Presumes the hash map iterator
  has looked up the (secondary hash), and possibly inserted its key and is
  positioned on it.  Links any existing entry behind it, i.e. we insert at
  front of the hash bucket, cf.  StoreLinkedImmutableStringFromTableBuffers.
  Update \c m_rows_in_hash_map.
  @param[out] single_row_too_large
                  set if we discover that we have a single
                  blob which is so large that it consumes (most) of the entire
                  allocated space (cf. set_operations_buffer_size).
  @returns true on error, which will also set \c single_row_too_large if
                  relevant
 */
template <typename Profiler>
bool MaterializeIterator<Profiler>::store_row_in_hash_map(
    bool *single_row_too_large) {
  // Save the contents of all columns and make the hash map iterator's value
  // field ("->second") point to it.

  const bool is_right_operand = m_spill_state.read_state() ==
                                SpillState::ReadingState::SS_READING_RIGHT_HF;
  StoreLinkedInfo info{!is_right_operand, false, 0};
  LinkedImmutableString last_row_stored =
      StoreLinkedImmutableStringFromTableBuffers(m_mem_root.get(), nullptr,
                                                 m_table_collection, m_next_ptr,
                                                 m_row_size_upper_bound, &info);

  const bool too_large_row =
      m_spill_state.read_state() == SpillState::ReadingState::SS_NONE &&
      info.m_bytes_needed * 2 > thd()->variables.set_operations_buffer_size;

  if (last_row_stored == nullptr) {
    if (too_large_row) {
      // just store it in session's mem_root so we can immediately fall back
      // on index based tmp table de-duplication; do not attempt spill
      // handling.
      last_row_stored = StoreLinkedImmutableStringFromTableBuffers(
          m_mem_root.get(), thd()->mem_root, m_table_collection, m_next_ptr,
          m_row_size_upper_bound, &info);
      if (last_row_stored == nullptr) {
        my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
                 ComputeRowSizeUpperBound(m_table_collection));
        return true;
      }
      m_hash_map_iterator->second = last_row_stored;
      m_rows_in_hash_map++;
      *single_row_too_large = true;
    }  // else too many rows, initiate spill handling
    return true;
  }
  m_hash_map_iterator->second = last_row_stored;
  m_rows_in_hash_map++;
  return false;
}

template <typename Profiler>
void MaterializeIterator<Profiler>::update_row_in_hash_map() {
  LinkedImmutableString::Decoded data = m_last_row.Decode();
  // We are just writing back the row, its size hasn't changed so this should be
  // safe, only the set counter (uint_64_t) will have changed.
  const uchar *ptr = pointer_cast<const uchar *>(data.data);
  StoreFromTableBuffersRaw(m_table_collection, const_cast<uchar *>(ptr));
}

template <typename Profiler>
int MaterializeIterator<Profiler>::read_next_row(const Operand &operand) {
  if (m_spill_state.spill()) {
    return m_spill_state.read_next_row(&operand);
  }
  int result = operand.subquery_iterator->Read();
  if (result == 0) {
    m_read_rows_before_dedup++;
    // Materialize items for this row.
    if (operand.copy_items) {
      if (copy_funcs(operand.temp_table_param, thd())) return 1;
    }
  }
  return result;
}

template <typename Profiler>
bool MaterializeIterator<Profiler>::process_row_hash(const Operand &operand,
                                                     TABLE *t,
                                                     ha_rows *stored_rows) {
  auto read_counter = [t]() -> ulonglong {
    ulonglong cnt = static_cast<ulonglong>(t->set_counter()->val_int());
    return cnt;
  };

  if (m_spill_state.read_state() ==
      SpillState::ReadingState::SS_READING_RIGHT_HF) {
    // Need special handling here since otherwise for operand > 0, we wouldn't
    // be re-populating the hash map, just checking against it, cf comment
    // below: "never write"
    return load_HF_row_into_hash_map();
  }

  const bool left_operand = operand.m_operand_idx == 0;
  bool found = false;  // true if we just found a record in the hash map
  bool spill_to_disk = false;

  if (check_unique_fields_hash_map(t, /*write*/ left_operand, &found,
                                   &spill_to_disk))
    return true;

  if (spill_to_disk) {
    assert(left_operand);
    return handle_hash_map_full(operand, stored_rows, false);
  }

  switch (t->set_op_type()) {
    case TABLE::SOT_UNION_ALL:
      assert(false);
      break;
    case TABLE::SOT_UNION_DISTINCT:
      assert(false);
      break;
    case TABLE::SOT_EXCEPT_ALL:
    case TABLE::SOT_EXCEPT_DISTINCT:
      if (left_operand) {
        //
        // After we finish reading the left side, each row's counter contains
        // the number of duplicates seen of that row.
        //
        if (!found) {
          // counter := 1
          t->set_counter()->store(1, true);
          // next, go on to write the row
        } else {
          // counter := counter + 1
          t->set_counter()->store(read_counter() + 1, true);
          update_row_in_hash_map();
          return false;
        }
      } else {  // right operand(s)
        //
        // After this right side has been processed, the counter contains the
        // number of duplicates not yet matched (and thus removed) by this
        // right side or any previous right side(s).
        //
        if (!found) {
          // row doesn't have a counter-part in left side, so we can ignore it
          return false;
        }
        ulonglong cnt = read_counter();
        if (cnt > 0) {
          if (operand.m_operand_idx < operand.m_first_distinct)
            // counter := counter - 1
            t->set_counter()->store(static_cast<longlong>(cnt - 1), true);
          else
            t->set_counter()->store(0, true);
          update_row_in_hash_map();
        }
        return false;  // right hand side of EXCEPT, never write
      }

      break;
    case TABLE::SOT_INTERSECT_ALL:
      if (left_operand) {
        //
        // In left pass we establish initial count of each row in subcounter
        // 0. In right block we increment subcounter 1 (up to initial count),
        // on final read we use min(subcounter 0, subcounter 1) as the
        // intersection result. NOTE: this only works correctly if we only ever
        // have two blocks for INTERSECT ALL: so they should not have been
        // merged.
        //
        if (!found) {
          HalfCounter c(0);
          // left side counter := 1
          c[0] = 1;
          t->set_counter()->store(static_cast<longlong>(c.value()), true);
          // go on to write the row
        } else {
          HalfCounter c(read_counter());
          if (static_cast<uint64_t>(c[0]) + 1 >
              std::numeric_limits<uint32_t>::max()) {
            my_error(ER_INTERSECT_ALL_MAX_DUPLICATES_EXCEEDED, MYF(0));
            return true;
          }
          // left side counter := left side counter + 1
          c[0]++;
          t->set_counter()->store(static_cast<longlong>(c.value()), true);
          update_row_in_hash_map();
          return false;
        }
      } else {  // right operand
        assert(operand.m_operand_idx <= 1);
        //
        // At the end of the (single) right side pass, we have two counters for
        // each row: in one, the number of duplicates seen on the left side,
        // and in the other, the number of times this row was matched on the
        // right side (we do not increment it past the number seen on the left
        // side, since we can maximally get that number of duplicates for the
        // operation).
        //
        if (!found) {
          // row doesn't have a counter-part in left side, so we can ignore it
          return false;
        }
        // we found a left side candidate
        HalfCounter c(read_counter());
        const uint32_t left_side = c[0];
        if (c[1] + 1 <= left_side) {
          // right side counter = right side counter + 1
          c[1]++;
          t->set_counter()->store(static_cast<longlong>(c.value()), true);
          update_row_in_hash_map();
        }  // else: already matched all occurences from left side table
        return false;  // right hand side of INTERSECT, never write
      }

      break;
    case TABLE::SOT_INTERSECT_DISTINCT:
      if (left_operand) {
        //
        // After we finish reading the left side, each row's counter contains N
        // - 1, i.e. the number of operands intersected.
        //
        if (!found) {
          // counter := no_of_operands - 1
          t->set_counter()->store(
              static_cast<longlong>(operand.m_total_operands - 1), true);
          // next, go on to write the row
        } else {
          // We have already written this row and initialized its counter.
          return false;
        }
      } else {  // right operand(s)
        //
        // After this right side operand, each row's counter either wasn't seen
        // by this block (and is thus still left undecremented), or we see it,
        // in which the counter is decremented once to indicated that it was
        // matched by this right side and thus is still a candidate for the
        // final inclusion, pending outcome of any further right side
        // operands. The number of the present set operand (materialized block
        // number) is used for this purpose.
        //
        if (!found) {
          // row doesn't have a counter-part in left side, so we can ignore it
          return false;
        }
        // we found a left side candidate, now check its counter to see if it
        // has already been matched with this right side row or not. If so,
        // decrement to indicate is has been matched by this operand. If the
        // row was missing in a previous right side operand, we will also skip
        // it here, since its counter is too high, and we will leave it behind.
        ulonglong cnt = read_counter();
        if (cnt == operand.m_total_operands - operand.m_operand_idx) {
          // counter -:= 1
          cnt = cnt - 1;
          t->set_counter()->store(static_cast<longlong>(cnt), true);
          update_row_in_hash_map();
        }
        return false;  // right hand side of INTERSECT, never write
      }
      break;
    case TABLE::SOT_NONE:
      assert(false);
  }
  bool single_row_too_large{false};
  return store_row_in_hash_map(&single_row_too_large) &&
         handle_hash_map_full(operand, stored_rows, single_row_too_large);
}

template <typename Profiler>
bool MaterializeIterator<Profiler>::process_row(const Operand &operand,
                                                Operands &operands, TABLE *t,
                                                uchar *set_counter_0,
                                                uchar *set_counter_1,
                                                bool *read_next) {
  /**
    Read the value of TABLE::m_set_counter from record[1]. The value can be
    found there after a call to check_unique_fields if the row was
    found. Note that m_set_counter a priori points to record[0], which is
    used when writing and updating the counter.
   */
  auto read_counter = [t, set_counter_0, set_counter_1]() -> ulonglong {
    assert(t->record[1] - t->record[0] == set_counter_1 - set_counter_0);
    t->set_counter()->set_field_ptr(set_counter_1);
    ulonglong cnt = static_cast<ulonglong>(t->set_counter()->val_int());
    t->set_counter()->set_field_ptr(set_counter_0);
    return cnt;
  };

  auto spill_to_disk_and_retry_update_row =
      [this, t, &operands](int seen_tmp_table_error) -> int {
    bool dummy = false;
    if (create_ondisk_from_heap(thd(), t, seen_tmp_table_error,
                                /*insert_last_record=*/false,
                                /*ignore_last_dup=*/true, &dummy))
      return 1; /* purecov: inspected */
    // Table's engine changed; index is not initialized anymore.
    if (t->file->ha_index_init(0, /*sorted*/ false) != 0) return 1;

    // Inform each reader that the table has changed under their feet,
    // so they'll need to reposition themselves.
    for (const Operand &op : operands) {
      if (op.is_recursive_reference) {
        op.recursive_reader->RepositionCursorAfterSpillToDisk();
      }
    }
    // re-try update: 1. reposition to same row
    bool found [[maybe_unused]] = !check_unique_fields(t);
    assert(found);
    return t->file->ha_update_row(t->record[1], t->record[0]);
  };

  const bool left_operand = operand.m_operand_idx == 0;
  int error = 0;

  switch (t->set_op_type()) {
    case TABLE::SOT_UNION_ALL:
      assert(false);
      break;
    case TABLE::SOT_UNION_DISTINCT:
      assert(false);
      break;
    case TABLE::SOT_EXCEPT_ALL:
    case TABLE::SOT_EXCEPT_DISTINCT:
      if (left_operand) {
        //
        // After we finish reading the left side, each row's counter contains
        // the number of duplicates seen of that row.
        if (check_unique_fields(t)) {
          // counter := 1
          t->set_counter()->store(1, true);
          // next, go on to write the row
        } else {
          // counter := counter + 1
          t->set_counter()->store(read_counter() + 1, true);
          error = t->file->ha_update_row(t->record[1], t->record[0]);
          return !t->file->is_ignorable_error(error) &&
                 spill_to_disk_and_retry_update_row(error);
        }
      } else {  // right operand(s)
        //
        // After this right side has been processed, the counter contains the
        // number of duplicates not yet matched (and thus removed) by this right
        // side or any previous right side(s).
        if (check_unique_fields(t)) {
          // row doesn't have a counter-part in left side, so we can ignore it
          return false;
        }
        ulonglong cnt = read_counter();
        if (cnt > 0) {
          if (operand.m_operand_idx < operand.m_first_distinct)
            // counter := counter - 1
            t->set_counter()->store(static_cast<longlong>(cnt - 1), true);
          else
            t->set_counter()->store(0, true);
          error = t->file->ha_update_row(t->record[1], t->record[0]);
          if (!t->file->is_ignorable_error(error) &&
              spill_to_disk_and_retry_update_row(error))
            return true;
        }
        // right hand side of EXCEPT, never write
        return false;
      }
      break;
    case TABLE::SOT_INTERSECT_ALL:
      if (left_operand) {
        //
        // In left pass we establish initial count of each row in subcounter
        // 0. In right block we increment subcounter 1 (up to initial count), on
        // final read we use min(subcounter 0, subcounter 1) as the intersection
        // result. NOTE: this only works correctly if we only ever have two
        // operands for INTERSECT ALL: so they should not have been merged.
        if (check_unique_fields(t)) {
          HalfCounter c(0);
          // left side counter := 1
          c[0] = 1;
          t->set_counter()->store(static_cast<longlong>(c.value()), true);
          // go on to write the row
        } else {
          HalfCounter c(read_counter());
          if (static_cast<uint64_t>(c[0]) + 1 >
              std::numeric_limits<uint32_t>::max()) {
            my_error(ER_INTERSECT_ALL_MAX_DUPLICATES_EXCEEDED, MYF(0));
            return true;
          }
          // left side counter := left side counter + 1
          c[0]++;
          t->set_counter()->store(static_cast<longlong>(c.value()), true);
          error = t->file->ha_update_row(t->record[1], t->record[0]);
          if (!t->file->is_ignorable_error(error) &&
              spill_to_disk_and_retry_update_row(error))
            return true;
        }
      } else {  // right operand
        assert(operand.m_operand_idx <= 1);
        //
        // At the end of the (single) right side pass, we have two counters for
        // each row: in one, the number of duplicates seen on the left side, and
        // in the other, the number of times this row was matched on the right
        // side (we do not increment it past the number seen on the left side,
        // since we can maximally get that number of duplicates for the
        // operation).
        if (check_unique_fields(t))
          // row doesn't have a counter-part in left side, so we can ignore it
          return false;
        // we found a left side candidate
        HalfCounter c(read_counter());
        const uint32_t left_side = c[0];
        if (c[1] + 1 <= left_side) {
          // right side counter = right side counter + 1
          c[1]++;
          t->set_counter()->store(static_cast<longlong>(c.value()), true);
          error = t->file->ha_update_row(t->record[1], t->record[0]);
          if (!t->file->is_ignorable_error(error) &&
              spill_to_disk_and_retry_update_row(error))
            return true;
        }  // else: already matched all occurences from left side table

        // right hand side of INTERSECT, never write
        return false;
      }
      break;
    case TABLE::SOT_INTERSECT_DISTINCT:
      if (left_operand) {
        //
        // After we finish reading the left side, each row's counter contains N
        // - 1, i.e. the number of operands intersected.
        if (check_unique_fields(t)) {
          // counter := no_of_operands - 1
          t->set_counter()->store(
              static_cast<longlong>(operand.m_total_operands - 1), true);
          // next, go on to write the row
        } else {
          // We have already written this row and initialized its counter.
          return false;
        }
      } else {  // right operand(s)
        //
        // After this right side, each row's counter either wasn't seen by this
        // block (and is thus still left undecremented), or we see it, in which
        // the counter is decremented once to indicate that it was matched by
        // this right side and thus is still a candidate for the final
        // inclusion, pending outcome of any further right side operands. The
        // number of the present set operand (materialized block number) is used
        // for this purpose.
        //
        if (check_unique_fields(t)) {
          // row doesn't have a counter-part in left side, so we can ignore it
          return false;
        }
        // we found a left side candidate, now check its counter to see if
        // it has already been matched with this right side row or not. If
        // so, decrement to indicate is has been matched by this operand. If
        // the row was missing in a previous right side operand, we will
        // also skip it here, since its counter is too high, and we will
        // leave it behind.
        ulonglong cnt = read_counter();
        if (cnt == operand.m_total_operands - operand.m_operand_idx) {
          // counter -:= 1
          cnt = cnt - 1;
          t->set_counter()->store(static_cast<longlong>(cnt), true);
          error = t->file->ha_update_row(t->record[1], t->record[0]);
          if (!t->file->is_ignorable_error(error) &&
              spill_to_disk_and_retry_update_row(error))
            return true;
        }
        // right hand side of INTERSECT, never write
        return false;
      }
      break;
    case TABLE::SOT_NONE:
      assert(false);
  }

  *read_next = false;  // proceed to write;
  return false;
}

template <typename Profiler>
bool MaterializeIterator<Profiler>::MaterializeOperand(const Operand &operand,
                                                       ha_rows *stored_rows) {
  TABLE *const t = table();
  Opt_trace_context *const trace = &thd()->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  const char *legend = nullptr;
  switch (t->set_op_type()) {
    case TABLE::SOT_EXCEPT_ALL:
    case TABLE::SOT_EXCEPT_DISTINCT:
      legend = "materialize for except";
      break;
    case TABLE::SOT_INTERSECT_ALL:
    case TABLE::SOT_INTERSECT_DISTINCT:
      legend = "materialize for intersect";
      break;
    case TABLE::SOT_UNION_ALL:
    case TABLE::SOT_UNION_DISTINCT:
      if (m_operands.size() == 1)
        legend = "materialize";
      else
        legend = "materialize for union";
      break;
    case TABLE::SOT_NONE:
      legend = "materialize";
      break;
  }
  Opt_trace_object trace_exec(trace, legend);
  trace_exec.add_select_number(operand.select_number);
  Opt_trace_array trace_steps(trace, "steps");
  Opt_trace_object trace_wrapper2(trace);

  const bool is_union_or_table = t->is_union_or_table();

  if (is_union_or_table) {
    if (t->hash_field != nullptr) {
      if (operand.disable_deduplication_by_hash_field)
        legend = "no de-duplication";
      else
        legend = "de-duplicate with index on hash field";
    } else {
      if (t->s->keys > 0)
        legend = "de-duplicate with index";
      else
        legend = "no de-duplication";
    }
  } else {
    if (m_use_hash_map)
      legend = "de-duplicate with hash table";
    else
      legend = "de-duplicate with index";
  }
  Opt_trace_object trace_exec2(trace, legend);
  Opt_trace_array trace_steps2(trace, "steps");

  // The next two declarations are for read_counter: used to implement
  // INTERSECT and EXCEPT.
  uchar *const set_counter_0 =
      t->set_counter() != nullptr ? t->set_counter()->field_ptr() : nullptr;
  uchar *const set_counter_1 = t->set_counter() != nullptr
                                   ? set_counter_0 + t->s->rec_buff_length
                                   : nullptr;

  JOIN *join = operand.join;
  if (join != nullptr) {
    join->set_executed();  // The dynamic range optimizer expects this.

    // TODO(sgunders): Consider doing this in some iterator instead.
    if (join->m_windows.elements > 0 && !join->m_windowing_steps) {
      // Initialize state of window functions as window access path
      // will be shortcut.
      for (Window &w : join->m_windows) {
        w.reset_all_wf_state();
      }
    }
  }

  if (operand.subquery_iterator->Init()) {
    return true;
  }

  PFSBatchMode pfs_batch_mode(operand.subquery_iterator.get());

  while (true) {
    // For EXCEPT and INTERSECT we test LIMIT in ScanTableIterator
    assert(is_union_or_table || m_limit_rows == HA_POS_ERROR);

    if (*stored_rows >= m_limit_rows) break;

    int error = read_next_row(operand);
    if (error > 0 || thd()->is_error()) {
      return true;
    }
    if (error < 0) {
      // When using hash map, we haven't written any rows to the materialized
      // table yet, so do that now that we have seen all rows from all set
      // operands (alias blocks).
      if (m_use_hash_map &&
          operand.m_operand_idx + 1 == operand.m_total_operands &&
          materialize_hash_map(t, stored_rows, /*single_row_too_large=*/false))
        return true;
      break;
    }
    if (thd()->killed) {
      thd()->send_kill_message();
      return true;
    }

    if (is_union_or_table) {
      if (operand.disable_deduplication_by_hash_field) {
        assert(doing_hash_deduplication());
      } else if (!check_unique_fields(t)) {
        continue;
      }
    } else {
      if (m_use_hash_map) {
        if (process_row_hash(operand, t, stored_rows)) return true;
        continue;
      }
      bool read_next = true;
      if (process_row(operand, m_operands, t, set_counter_0, set_counter_1,
                      &read_next))
        return true;
      if (read_next) continue;
      // else proceed to write
    }

    error = t->file->ha_write_row(t->record[0]);
    if (error == 0) {
      ++*stored_rows;
      continue;
    }
    // create_ondisk_from_heap will generate error if needed.
    if (!t->file->is_ignorable_error(error)) {
      bool is_duplicate = false;
      if (create_ondisk_from_heap(thd(), t, error,
                                  /*insert_last_record=*/true,
                                  /*ignore_last_dup=*/true, &is_duplicate))
        return true; /* purecov: inspected */
      // Table's engine changed; index is not initialized anymore.
      if (t->hash_field) t->file->ha_index_init(0, false);
      if (!is_duplicate && (is_union_or_table || operand.m_operand_idx == 0))
        ++*stored_rows;

      // Inform each reader that the table has changed under their feet,
      // so they'll need to reposition themselves.
      for (const Operand &query_b : m_operands) {
        if (query_b.is_recursive_reference) {
          query_b.recursive_reader->RepositionCursorAfterSpillToDisk();
        }
      }
    } else {
      // An ignorable error means duplicate key, ie. we deduplicated
      // away the row. This is seemingly separate from
      // check_unique_fields(), which only checks hash indexes.
    }
  }

  return false;
}

template <typename Profiler>
int MaterializeIterator<Profiler>::Read() {
  const typename Profiler::TimeStamp start_time = Profiler::Now();
  /*
    Enable the items which one should use if one wants to evaluate
    anything (e.g. functions in WHERE, HAVING) involving columns of this
    table.
  */
  if (m_ref_slice != -1) {
    assert(m_join != nullptr);
    if (!m_join->ref_items[m_ref_slice].is_null()) {
      m_join->set_ref_item_slice(m_ref_slice);
    }
  }

  const int err = m_table_iterator->Read();
  m_table_iter_profiler.StopRead(start_time, err == 0);
  return err;
}

template <typename Profiler>
void MaterializeIterator<Profiler>::EndPSIBatchModeIfStarted() {
  for (const Operand &operand : m_operands) {
    operand.subquery_iterator->EndPSIBatchModeIfStarted();
  }
  m_table_iterator->EndPSIBatchModeIfStarted();
}

bool SpillState::save_offending_row() {
  // Save offending row, we may not be able to write it in first set of
  // chunk files, so make a copy. This space goes out of the normal mem_root
  // since it's only one row.
  if (!m_table_collection.has_blob_column()) {
    const size_t max_row_size =
        ComputeRowSizeUpperBoundSansBlobs(m_table_collection);
    if (m_offending_row.m_buffer.reserve(max_row_size)) {
      return true;
    }
  }
  return StoreFromTableBuffers(m_table_collection, &m_offending_row.m_buffer);
}

bool SpillState::compute_chunk_file_sets(const Operand *current_operand) {
  /// This could be 1 too high, if we managed to insert key but not value, but
  /// never mind.
  const size_t rows_in_hash_map = m_hash_map->size();

  double total_estimated_rows =
      // Check sanity of estimate and override if (way) too low. We
      // make a shot in the dark and guess 800% of hash table size. If that
      // proves too low, we will revert to tmp table index based de-duplication.
      (current_operand->m_estimated_output_rows <= rows_in_hash_map
           ? rows_in_hash_map * 8
           : current_operand->m_estimated_output_rows);

  // Slightly underestimate number of rows in hash map to avoid
  // hash map overflow when reading chunk files.
  constexpr double kReductionFactor = 0.9;
  const double reduced_rows_in_hash_map =
      std::max<double>(1, rows_in_hash_map * kReductionFactor);

  // Since we have de-duplicated rows, rows_in_hash_map may represent a
  // larger number of rows read than actually in the table, but we ignore that
  // when making the estimate: the remaining rows of the left operand may not
  // contain duplicates.
  const size_t num_chunks =
      std::ceil(total_estimated_rows / reduced_rows_in_hash_map);

  // Ensure that the number of chunks is always a power of two. This allows
  // us to do some optimizations when calculating which chunk a row should
  // be placed in.
  m_num_chunks = std::bit_ceil(num_chunks);
  m_no_of_chunk_file_sets = (m_num_chunks + HashJoinIterator::kMaxChunks - 1) /
                            HashJoinIterator::kMaxChunks;
  m_current_chunk_file_set = 0;

  // Save offending row, we may not be able to write it in first set of
  // chunk files, so make a copy. This space goes out of the normal mem_root
  // since it's only one row.
  if (save_offending_row()) return true;

  // Hash offending row's content with tertiary hash value to determine its
  // chunk index.
  const ulonglong primary_hash =
      static_cast<ulonglong>(m_materialized_table->hash_field->val_int());
  const uint64_t chunk_hash =
      MY_XXH64(&primary_hash, sizeof(primary_hash), m_hash_seed);

  const size_t chunk_index = hash_to_chunk_index(chunk_hash);
  m_offending_row.m_chunk_offset = chunk_offset(chunk_index);
  m_offending_row.m_set = chunk_index_to_set(chunk_index);
  // Set up the row buffer used when deserializing chunk rows.
  if (!m_table_collection.has_blob_column()) {
    const size_t upper_row_size =
        ComputeRowSizeUpperBoundSansBlobs(m_table_collection);
    if (m_row_buffer.reserve(upper_row_size)) {
      return true;
    }
  }

  m_chunk_files.resize(std::min(m_num_chunks, HashJoinIterator::kMaxChunks));

  /// Set aside space for current generation of chunk row counters. This is
  /// a two dimensional matrix. Each element is allocated in
  /// initialize_first_HF_chunk_files
  m_row_counts.resize(m_chunk_files.size());

  Opt_trace_context *const trace = &m_thd->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_exec(trace, "spill to disk initiated");
  trace_exec.add("chunk files",
                 m_chunk_files.size() * 2 +                   // *2: HF + IF
                     (m_no_of_chunk_file_sets > 1 ? 1 : 0));  // REMAININGINPUT
  trace_exec.add("chunk sets", m_no_of_chunk_file_sets);
  return false;
}

bool SpillState::initialize_first_HF_chunk_files() {
  // Initialize HF and IF chunk files. We assign HF as the "build" chunk and IF
  // as the "probe" chunk (the terms "build" and "probe" were chosen for the
  // hash join usage of the chunk abstraction).
  for (size_t i = 0; i < m_chunk_files.size(); i++) {
    ChunkPair &chunk_pair = m_chunk_files[i];
    if (chunk_pair.build_chunk.Init(m_table_collection) ||
        chunk_pair.probe_chunk.Init(m_table_collection)) {
      my_error(ER_TEMP_FILE_WRITE_FAILURE, MYF(0));
      return true;
    }
    // Initialize counters matrix
    Mem_root_array<CountPair> ma(m_thd->mem_root, 1);
    ma.resize(m_no_of_chunk_file_sets);
    m_row_counts[i] = std::move(ma);
  }

  /// Initialize REMAININGINPUT tmp file for replay of input rows for chunk file
  /// sets 2..S
  return m_no_of_chunk_file_sets > 1 &&
         m_remaining_input.Init(m_table_collection);
}

/// Spread the contents of the hash map over the HF files. All rows for
/// chunk file set 0 precede all rows for chunk file set 1 etc, so we can
/// later retrieve all rows belonging to each file set by scanning only a
/// section of each chunk file.
bool SpillState::spread_hash_map_to_HF_chunk_files() {
  for (size_t set = 0; set < m_no_of_chunk_file_sets; set++) {
    for (const auto &[hash_key, first_row] : *m_hash_map) {
      for (LinkedImmutableString row_with_same_hash = first_row;
           row_with_same_hash != nullptr;
           row_with_same_hash = row_with_same_hash.Decode().next) {
        hash_join_buffer::LoadImmutableStringIntoTableBuffers(
            m_table_collection, row_with_same_hash);
        if (StoreFromTableBuffers(m_table_collection, &m_row_buffer)) {
          return true;
        }

        // Hash row's content with tertiary hash to determine its chunk index,
        // and write it if its set index equals the current set. This way,
        // all rows belonging to a set are stored consecutively, and sets in
        // index order.
        const ulonglong primary_hash =
            static_cast<ulonglong>(m_materialized_table->hash_field->val_int());
        const uint64_t chunk_hash =
            MY_XXH64(&primary_hash, sizeof(primary_hash), m_hash_seed);
        const size_t chunk_index = hash_to_chunk_index(chunk_hash);
        const size_t set_index = chunk_index_to_set(chunk_index);
        const size_t offset = chunk_offset(chunk_index);

        if (set_index == set) {
          // If this row goes into current chunk file set, write it.
          if (m_chunk_files[offset].build_chunk.WriteRowToChunk(
                  &m_row_buffer, /*matched*/ false, set_index /*, &offset*/)) {
            my_error(ER_TEMP_FILE_WRITE_FAILURE, MYF(0));
            return true;
          }
        }
      }
    }

    size_t idx = 0;
    for (auto &chunk : m_chunk_files) {
      /// TODO: this matrix of counts is allocated on normal execution
      /// \c MEM_ROOT.  If this space usage is seen as excessive, we could get
      /// rid of it by instead putting sentinel rows in a chunk at the start of
      /// each new chunk file set. That way, we can know when we have read all
      /// rows belonging to a chunk file set (instead of relying on this
      /// counter matrix).
      m_row_counts[idx][set].HF_count = chunk.build_chunk.NumRows();
      /// Reset number at end of each set, so we can determine number of rows
      /// for each set in a chunk file, cf. m_row_counts above.
      chunk.build_chunk.SetNumRows(0);
      idx++;
    }
  }

  // Reset correct total number of rows (sum for all sets) in each chunk
  // and rewind the HF files for reading from the start of the file
  for (size_t chunk_idx = 0; chunk_idx < m_chunk_files.size(); chunk_idx++) {
    size_t chunk_total = 0;
    for (size_t set_idx = 0; set_idx < m_no_of_chunk_file_sets; set_idx++)
      chunk_total += m_row_counts[chunk_idx][set_idx].HF_count;
    m_chunk_files[chunk_idx].build_chunk.SetNumRows(chunk_total);
    if (m_chunk_files[chunk_idx].build_chunk.Rewind()) return true;
  }

  return false;
}

bool SpillState::append_hash_map_to_HF() {
  m_chunk_files[m_current_chunk_idx].build_chunk.SetAppend();

  size_t rows_visited = 0;
  for (const auto &[hash_key, first_row] : *m_hash_map) {
    for (LinkedImmutableString row_with_same_hash = first_row;
         row_with_same_hash != nullptr;
         row_with_same_hash = row_with_same_hash.Decode().next) {
      hash_join_buffer::LoadImmutableStringIntoTableBuffers(m_table_collection,
                                                            row_with_same_hash);
      if (StoreFromTableBuffers(m_table_collection, &m_row_buffer)) {
        return true;
      }
      rows_visited++;
      // We shouldn't need to do this over again: we know chunk no and set
      // already. Compute it for assertion.
#ifndef NDEBUG
      const ulonglong primary_hash =
          static_cast<ulonglong>(m_materialized_table->hash_field->val_int());
      const uint64_t chunk_hash =
          MY_XXH64(&primary_hash, sizeof(primary_hash), m_hash_seed);
      const size_t chunk_index = hash_to_chunk_index(chunk_hash);
      const size_t set_index = chunk_index_to_set(chunk_index);
      const size_t offset = chunk_offset(chunk_index);
      // If this row goes into current chunk file set, write it.
      assert(offset == m_current_chunk_idx);
      assert(set_index == m_current_chunk_file_set);
#endif
      if (m_chunk_files[m_current_chunk_idx].build_chunk.WriteRowToChunk(
              &m_row_buffer, /*matched*/ false, m_current_chunk_file_set)) {
        my_error(ER_TEMP_FILE_WRITE_FAILURE, MYF(0));
        return true;
      }
    }
  }
  m_row_counts[m_current_chunk_idx][m_current_chunk_file_set].HF_count =
      rows_visited;

  reset_hash_map(m_hash_map);
  if (reset_mem_root(m_hash_map_mem_root)) return true;

  m_chunk_files[m_current_chunk_idx].build_chunk.ContinueRead();

  return false;
}

bool SpillState::save_operand_to_IF_chunk_files(
    const Operand *current_operand) {
  for (size_t set = 0; set < m_no_of_chunk_file_sets; set++) {
    size_t rows_left_in_remaining_input = 0;
    if (set > 0) {
      rows_left_in_remaining_input = m_remaining_input.NumRows();
      /// Prepare to read from REMAININGINPUT
      m_remaining_input.Rewind();
    }
    while (true) {
      bool done = false;  // done reading rows for this chunk file set

      // Save the input row that caused the spill, we saved that away
      if (m_offending_row.m_unsaved) {
        assert(set == 0);
        m_offending_row.m_unsaved = false;
        LoadIntoTableBuffers(
            m_table_collection,
            pointer_cast<const uchar *>(m_offending_row.m_buffer.ptr()));
        if (m_offending_row.m_set == 0) {
          if (m_chunk_files[m_offending_row.m_chunk_offset]
                  .probe_chunk.WriteRowToChunk(&m_offending_row.m_buffer,
                                               /*matched*/ false, set)) {
            return true;
          }
          m_row_counts[m_offending_row.m_chunk_offset][set].IF_count++;
        } else if (m_no_of_chunk_file_sets > 1) {
          /// If we have more than one chunk file set, we need the input rows
          /// (that we couldn't write to set 0) again for writing to the next
          /// sets, so save in REMAININGINPUT
          if (m_remaining_input.WriteRowToChunk(&m_offending_row.m_buffer,
                                                /*matched*/ false)) {
            return true;
          }
        }
      }

      if (set == 0) {
        int error = current_operand->subquery_iterator->Read();
        if (error > 0 || m_thd->is_error()) return true;
        if (error < 0) {
          done = true;  // done reading the rest of left operand
        } else {
          // Materialize items for this row.
          if (current_operand->copy_items) {
            if (copy_funcs(current_operand->temp_table_param, m_thd))
              return true;
          }
          if (StoreFromTableBuffers(m_table_collection, &m_row_buffer))
            return true;
        }

      } else {
        if (rows_left_in_remaining_input-- > 0) {
          bool dummy = false;
          if (m_remaining_input.LoadRowFromChunk(&m_row_buffer,
                                                 /*matched*/ &dummy))
            return true;
        } else {
          done = true;
        }
      }
      if (done) break;  // move on to next chunk file set, if any

      const ulonglong primary_hash = calc_row_hash(m_materialized_table);
      m_materialized_table->hash_field->store(
          static_cast<longlong>(primary_hash), true);
      const uint64_t chunk_hash =
          MY_XXH64(&primary_hash, sizeof(primary_hash), m_hash_seed);
      const size_t chunk_index = hash_to_chunk_index(chunk_hash);
      const size_t set_index = chunk_index_to_set(chunk_index);
      const size_t offset = chunk_offset(chunk_index);
      // If this row goes into current chunk file set, write it.
      if (set_index == set) {
        if (m_chunk_files[offset].probe_chunk.WriteRowToChunk(
                &m_row_buffer, /*matched*/ false, set_index)) {
          return true;
        }
        // Update the set counter of IF-k. Note that
        m_row_counts[offset][set].IF_count++;
      }
      /// If we have more than one chunk file set, we need the input rows (that
      /// we couldn't write to set 0) again for writing to the next sets, so
      /// save in REMAININGINPUT
      if (m_no_of_chunk_file_sets > 1 && set == 0 && set_index != 0) {
        if (m_remaining_input.WriteRowToChunk(&m_row_buffer, /*matched*/ false))
          return true;
      }
    }
  }

  /// Rewind all IF chunk files and possibly REMAININGINPUT.
  for (auto &chunk : m_chunk_files) {
    if (chunk.probe_chunk.Rewind()) return true;
  }

  return m_no_of_chunk_file_sets > 1 && m_remaining_input.Rewind();
}

bool SpillState::reset_for_spill_handling() {
  // We have HF and IF on chunk files, get ready for reading rest of left
  // operand rows
  reset_hash_map(m_hash_map);
  if (reset_mem_root(m_hash_map_mem_root, /*initial*/ true)) return true;
  m_current_chunk_idx = 0;
  m_current_chunk_file_set = 0;
  m_current_row_in_chunk = 0;

  return false;
}

bool SpillState::write_HF(THD *thd, size_t set, size_t chunk_idx,
                          const Operands &operands, ha_rows *stored_rows) {
  for (size_t rowno = 0; rowno < m_row_counts[chunk_idx][set].HF_count;
       rowno++) {
    size_t set_idx = 0;
    if (m_chunk_files[chunk_idx].build_chunk.LoadRowFromChunk(
            &m_row_buffer, nullptr, &set_idx))
      return true;
#ifndef NDEBUG
    const ulonglong primary_hash =
        static_cast<ulonglong>(m_materialized_table->hash_field->val_int());
    const uint64_t chunk_hash =
        MY_XXH64(&primary_hash, sizeof(primary_hash), m_hash_seed);
    const size_t chunk_index = hash_to_chunk_index(chunk_hash);
    const size_t set_index = chunk_index_to_set(chunk_index);
    assert(chunk_offset(chunk_index) == chunk_idx);
    assert(set_idx == set_index);
    assert(set == set_idx);
#endif
    int error = m_materialized_table->file->ha_write_row(
        m_materialized_table->record[0]);
    if (error == 0) {
      ++*stored_rows;
    } else {
      // create_ondisk_from_heap will generate error if needed.
      assert(!m_materialized_table->file->is_ignorable_error(error));
      if (create_ondisk_from_heap(thd, m_materialized_table, error,
                                  /*insert_last_record=*/true,
                                  /*ignore_last_dup=*/true, nullptr))
        return true; /* purecov: inspected */
      if (secondary_overflow()) {
        assert(m_materialized_table->s->keys == 1);
        if (m_materialized_table->file->ha_index_init(0, false) != 0)
          return true;
      } else {
        // else: we use hashing, so skip ha_index_init
        assert(m_materialized_table->s->keys == 0);
      }
      ++*stored_rows;

      // Inform each reader that the table has changed under their feet,
      // so they'll need to reposition themselves.
      for (const Operand &query_b : operands) {
        if (query_b.is_recursive_reference) {
          query_b.recursive_reader->RepositionCursorAfterSpillToDisk();
        }
      }
    }
  }
  return false;
}

bool SpillState::write_partially_completed_HFs(THD *thd,
                                               const Operands &operands,
                                               ha_rows *stored_rows) {
  // State when secondary overflow occurred when making 2. generation HF chunks
  // is described by m_current_chunk_idx, m_current_chunk_file_set and
  // m_current_row_in_chunk, corresponding to OF ("overflow") below
  //
  // 1.generation
  //     0        m_current_chunk_idx  m_chunk_files.size() - 1
  //     |                        |           |
  //     V                        V           V
  // |------+------+------+    +------+    +------|
  // |   r  |   r  |   r  | .. |   r  | .. |   r  |  0
  // |------+------+------+    +------+    +------|
  //                   :
  // |------+------+------+    +------+    +------|
  // |   r  |   r  |   r  | .. |   r  | .. |   i  |  m_current_chunk_file_set
  // |------+------+------+    +------+    +------|
  //                   :
  // |------+------+------+    +------+    +------|
  // |   i  |   i  |   i  | .. |   i  | .. |   i  |  m_no_of_chunk_file_sets - 1
  // |------+------+------+    +------+    +------|
  //
  // 2.generation
  // |------+------+------+    +------+    +------|
  // |   C  |   C  |   C  | .. |   C  | .. |   C  |  0
  // |------+------+------+    +------+    +------|
  //                   :
  // |------+------+------+    +------+    +------|
  // |   C  |   C  |   C  | .. |  OF  | .. |      |  m_current_chunk_file_set
  // |------+------+------+    +------+    +------|
  //                   :
  // |------+------+------+    +------+    +------|
  // |      |      |      | .. |      | .. |      |  m_no_of_chunk_file_sets - 1
  // |------+------+------+    +------+    +------|
  //
  // We successfully read all chunks labelled r and matched them with their
  // respective IF chunks, so they exist in 2. generation labeled C ("complete")
  //
  // Now, read and write to output table the already de-duplicated rows.
  // First the remaining chunk files from the 1. generation labeled i
  // ("incomplete").  Reading 1. generation chunks first ensures that the
  // reading positions are correct for reading the 2. generation chunks in the
  // next step.
  // Next, read and write the completed chunk files from 2. generation.
  // The 1. generation of the chunk file OF, given by {m_current_chunk_idx,
  // m_current_chunk_file_set} has already been read into the in-memory hash
  // table. Writing the latter as well as the offending row and the
  // reading/de-duplicating rest of the left operand IF files will be handled
  // elsewhere (in materialize_hash_map and read_next_row_secondary_overflow).

  // Write Is
  for (size_t set = m_current_chunk_file_set; set < m_no_of_chunk_file_sets;
       set++) {
    const size_t min_C =
        set == m_current_chunk_file_set ? m_current_chunk_idx + 1 : 0;
    for (size_t offset = min_C; offset < m_chunk_files.size(); offset++) {
      if (write_HF(thd, set, offset, operands, stored_rows)) return true;
    }
  }
  // Write Cs
  for (size_t set = 0; set <= m_current_chunk_file_set; set++) {
    const size_t max_C = set == m_current_chunk_file_set ? m_current_chunk_idx
                                                         : m_chunk_files.size();
    for (size_t offset = 0; offset < max_C; offset++) {
      if (write_HF(thd, set, offset, operands, stored_rows)) return true;
    }
  }

  return false;
}

bool SpillState::write_completed_HFs(THD *thd, const Operands &operands,
                                     ha_rows *stored_rows) {
  // Write >= 2. generation finished chunk files
  for (size_t set = 0; set < m_no_of_chunk_file_sets; set++) {
    for (size_t offset = 0; offset < m_chunk_files.size(); offset++) {
      if (write_HF(thd, set, offset, operands, stored_rows)) return true;
    }
  }
  return false;
}

#ifndef NDEBUG
bool SpillState::simulated_secondary_overflow(bool *spill) {
  const char *const common_msg =
      "in debug_set_operations_secondary_overflow_at too high: should be "
      "lower than or equal to:";
  // Have we indicated a value?
  if (strlen(m_thd->variables.debug_set_operations_secondary_overflow_at) > 0 &&
      m_simulated_set_idx == std::numeric_limits<size_t>::max()) {
    // Parse out variables with
    // syntax: <set-idx:integer 0-based> <chunk-idx:integer 0-based>
    // <row_no:integer 1-based>
    int tokens =
        sscanf(m_thd->variables.debug_set_operations_secondary_overflow_at,
               "%zu %zu %zu", &m_simulated_set_idx, &m_simulated_chunk_idx,
               &m_simulated_row_no);
    if (tokens != 3 || m_simulated_row_no < 1 ||
        m_simulated_chunk_idx >= m_chunk_files.size()) {
      my_error(ER_SIMULATED_INJECTION_ERROR, MYF(0), "Chunk number", common_msg,
               m_chunk_files.size() - 1);
      return true;
    }

    if (static_cast<size_t>(m_simulated_set_idx) >= m_no_of_chunk_file_sets) {
      my_error(ER_SIMULATED_INJECTION_ERROR, MYF(0), "Set number", common_msg,
               m_no_of_chunk_file_sets - 1);
      return true;
    }
  }

  if (m_simulated_set_idx <
      std::numeric_limits<size_t>::max()) {  // initialized
    if (m_current_chunk_file_set == m_simulated_set_idx &&
        m_current_chunk_idx == m_simulated_chunk_idx) {
      if (m_simulated_row_no >
          m_row_counts[m_current_chunk_idx][m_current_chunk_file_set]
              .IF_count) {
        my_error(ER_SIMULATED_INJECTION_ERROR, MYF(0), "Row number", common_msg,
                 m_row_counts[m_current_chunk_idx][m_current_chunk_file_set]
                     .IF_count);
        return true;
      }

      if (m_current_row_in_chunk == static_cast<size_t>(m_simulated_row_no)) {
        *spill = true;
      }
    }
  }

  return false;
}
#endif

bool SpillState::init(const Operand &left_operand, hash_map_type *hash_map,
                      size_t rows_in_hash_table, size_t read_rows_before_dedup,
                      MEM_ROOT *hash_map_mem_root, TABLE *t) {
  m_hash_map = hash_map;
  m_rows_in_hash_map = rows_in_hash_table;
  m_read_rows_before_dedup = read_rows_before_dedup;
  m_hash_map_mem_root = hash_map_mem_root;
  m_materialized_table = t;

  // prepare m_materialized_table as a TableCollection; this is needed by
  // some APIs we use
  {
    Prealloced_array<TABLE *, 4> ta(key_memory_hash_op, 1);
    ta[0] = t;
    pack_rows::TableCollection tc(ta, false, 0, 0);
    m_table_collection = tc;
  }
  // Use different hash seed for chunking of EXCEPT and INTERSECT
  // to avoid effects of initial set of rows all from one set op
  // ending up in same chunk in the next set op if that spills over too.
  // TODO: maybe assign a different seed to all set ops in a query.
  m_hash_seed = m_materialized_table->is_except()
                    ? HashJoinIterator::kChunkPartitioningHashSeed
                    : HashJoinIterator::kChunkPartitioningHashSeed *
                          m_magic_prime; /* arbitrary prime */

  if (compute_chunk_file_sets(&left_operand) ||                 // 3.
      initialize_first_HF_chunk_files() ||                      // 3.a
      spread_hash_map_to_HF_chunk_files() ||                    // 3.a
      save_rest_of_operand_to_IF_chunk_files(&left_operand) ||  // 3.b
      reset_for_spill_handling())                               // 3.b/3.c
    return true;
  m_spill_read_state = ReadingState::SS_READING_LEFT_HF;
  return false;
}

int SpillState::read_next_row(const Operand *current_operand) {
  if (m_secondary_overflow) return read_next_row_secondary_overflow();

  ReadingState prev_state =
      ReadingState::SS_NONE;  // init: silly but silences clang-tidy
  do {
    prev_state = m_spill_read_state;
    switch (m_spill_read_state) {
      case ReadingState::SS_READING_LEFT_HF:
      case ReadingState::SS_READING_RIGHT_HF: {
        while (true) {
          if (++m_current_row_in_chunk <=
              m_row_counts[m_current_chunk_idx][m_current_chunk_file_set]
                  .HF_count) {
            size_t set_idx = 0;
            if (m_chunk_files[m_current_chunk_idx].build_chunk.LoadRowFromChunk(
                    &m_row_buffer, nullptr, &set_idx))
              return 1;
#ifndef NDEBUG
            const ulonglong primary_hash = static_cast<ulonglong>(
                m_materialized_table->hash_field->val_int());
            const uint64_t chunk_hash =
                MY_XXH64(&primary_hash, sizeof(primary_hash), m_hash_seed);
            const size_t chunk_index = hash_to_chunk_index(chunk_hash);
            const size_t set_index = chunk_index_to_set(chunk_index);
            assert(chunk_offset(chunk_index) == m_current_chunk_idx);
            assert(set_idx == set_index);
            assert(set_idx == m_current_chunk_file_set);
#endif
            // We loaded a row from the current HF chunk, return and
            // put it in hash map
            break;
          }
          // We have read the entire current HF chunk into the hash map, now
          // read current IF chunk's rows and process them against the hash
          // map
          m_current_row_in_chunk = 0;
          switch_to_IF();
          break;
        }
      } break;
      case ReadingState::SS_READING_LEFT_IF:
      case ReadingState::SS_READING_RIGHT_IF: {
        while (true) {
          if (++m_current_row_in_chunk <=
              m_row_counts[m_current_chunk_idx][m_current_chunk_file_set]
                  .IF_count) {
            size_t set_idx = 0;
            if (m_chunk_files[m_current_chunk_idx].probe_chunk.LoadRowFromChunk(
                    &m_row_buffer, nullptr, &set_idx))
              return 1;
#ifndef NDEBUG
            const ulonglong primary_hash = static_cast<ulonglong>(
                m_materialized_table->hash_field->val_int());
            const uint64_t chunk_hash =
                MY_XXH64(&primary_hash, sizeof(primary_hash), m_hash_seed);
            const size_t chunk_index = hash_to_chunk_index(chunk_hash);
            assert(chunk_index_to_set(chunk_index) == set_idx);
            assert(chunk_offset(chunk_index) == m_current_chunk_idx);
#endif
            if (set_idx == m_current_chunk_file_set) {
              // OK, we found a row from the current set, process row
              break;
            }
          } else {
            append_hash_map_to_HF();

            m_current_row_in_chunk = 0;
            if (++m_current_chunk_idx < m_chunk_files.size()) {
              // Move on to next {HF, IF} chunk pair
              switch_to_HF();
              break;
            }
            // We have exhaused the chunk pairs in the current set, move to
            // the first {HF, IF} chunk pair in the next set.
            m_current_chunk_idx = 0;
            if (++m_current_chunk_file_set < m_no_of_chunk_file_sets) {
              switch_to_HF();
              break;
            }
            // We have exhausted all {HF, IF} chunk pairs for the current
            // operand (left or right), get ready for the next right
            // operand(s)
            m_current_chunk_file_set = 0;
            m_spill_read_state = ReadingState::SS_COPY_OPERAND_N_TO_IF;
            for (auto &chunk : m_chunk_files) {
              // We have read and de-duplicated all operand rows, and the
              // rows are located in the current generation of HF chunk sets
              // and these are already correctly positioned for reading,
              // residing immediately after the previous generation chunk
              // sets.  We now want to fill the probe chunks with the next
              // (right) operand's rows and perform set operation against
              // the current generation's HF chunk sets.
              if (chunk.probe_chunk.Init(m_table_collection)) return 1;
            }
            if (m_remaining_input.Init(m_table_collection)) return 1;
            // Done reading current operand
            return -1;
          }
        }
      } break;

      case ReadingState::SS_COPY_OPERAND_N_TO_IF: {
        // We are reading a new operand, so reset IF counters
        for (size_t set = 0; set < m_no_of_chunk_file_sets; set++)
          for (size_t i = 0; i < m_row_counts.size(); i++)
            m_row_counts[i][set].IF_count = 0;

        if (save_operand_to_IF_chunk_files(current_operand) ||
            reset_for_spill_handling())
          return 1;
        m_spill_read_state = ReadingState::SS_READING_RIGHT_HF;
      } break;

      case ReadingState::SS_NONE:
      case ReadingState::SS_FLUSH_REST_OF_LEFT_IFS:
        assert(false);
    }
  } while (m_spill_read_state != prev_state);

  return 0;
}

int SpillState::read_next_row_secondary_overflow() {
  do {
    switch (m_spill_read_state) {
      case ReadingState::SS_READING_LEFT_IF:
        LoadIntoTableBuffers(
            m_table_collection,
            pointer_cast<const uchar *>(m_offending_row.m_buffer.ptr()));
        m_spill_read_state = ReadingState::SS_FLUSH_REST_OF_LEFT_IFS;
        break;
      case ReadingState::SS_FLUSH_REST_OF_LEFT_IFS:
        if (++m_current_row_in_chunk <=
            m_row_counts[m_current_chunk_idx][m_current_chunk_file_set]
                .IF_count) {
          size_t set_idx = 0;
          if (m_chunk_files[m_current_chunk_idx].probe_chunk.LoadRowFromChunk(
                  &m_row_buffer, nullptr, &set_idx))
            return 1;
#ifndef NDEBUG
          const ulonglong primary_hash = static_cast<ulonglong>(
              m_materialized_table->hash_field->val_int());
          const uint64_t chunk_hash =
              MY_XXH64(&primary_hash, sizeof(primary_hash), m_hash_seed);
          const size_t chunk_index = hash_to_chunk_index(chunk_hash);
          assert(chunk_index_to_set(chunk_index) == set_idx);
          assert(chunk_offset(chunk_index) == m_current_chunk_idx);
          assert(m_current_chunk_file_set == set_idx);
#endif
        } else {
          m_current_row_in_chunk = 0;
          if (++m_current_chunk_idx < m_chunk_files.size()) continue;
          m_current_chunk_idx = 0;
          if (++m_current_chunk_file_set < m_no_of_chunk_file_sets) continue;
          // No more IF chunks left to process, we have recovered from secondary
          // overflow, all de-duplicated rows are on output table, now it is
          // safe to move on to right operand(s).
          secondary_overflow_handling_done();
          return -1;
        }
        break;
      default:
        assert(false);
    }
    break;  // got a row
  } while (true);

  return 0;
}

RowIterator *materialize_iterator::CreateIterator(
    THD *thd, Mem_root_array<Operand> operands,
    const MaterializePathParameters *path_params,
    unique_ptr_destroy_only<RowIterator> table_iterator, JOIN *join) {
  if (thd->lex->is_explain_analyze) {
    RowIterator *const table_iter_ptr = table_iterator.get();

    auto iter = new (thd->mem_root) MaterializeIterator<IteratorProfilerImpl>(
        thd, std::move(operands), path_params, std::move(table_iterator), join);

    /*
      Provide timing data for the iterator that iterates over the temporary
      table. This should include the time spent both materializing the table
      and iterating over it.
    */
    table_iter_ptr->SetOverrideProfiler(iter->GetTableIterProfiler());
    return iter;
  } else {
    return new (thd->mem_root) MaterializeIterator<DummyIteratorProfiler>(
        thd, std::move(operands), path_params, std::move(table_iterator), join);
  }
}

StreamingIterator::StreamingIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> subquery_iterator,
    Temp_table_param *temp_table_param, TABLE *table, bool provide_rowid,
    JOIN *join, int ref_slice)
    : TableRowIterator(thd, table),
      m_subquery_iterator(std::move(subquery_iterator)),
      m_temp_table_param(temp_table_param),
      m_join(join),
      m_output_slice(ref_slice),
      m_provide_rowid(provide_rowid) {
  assert(m_subquery_iterator != nullptr);

  // If we have weedout in this query, it will expect to have row IDs that
  // uniquely identify each row, so calling position() will fail (since we
  // do not actually write these rows to anywhere). Use the row number as a
  // fake ID; since the real handler on this temporary table is never called,
  // it is safe to replace it with something of the same length.
  if (m_provide_rowid) {
    if (table->file->ref_length < sizeof(m_row_number)) {
      table->file->ref_length = sizeof(m_row_number);
      table->file->ref = nullptr;
    }
    if (table->file->ref == nullptr) {
      table->file->ref =
          pointer_cast<uchar *>(thd->mem_calloc(table->file->ref_length));
    }
  }
}

bool StreamingIterator::Init() {
  if (m_join->query_expression()->clear_correlated_query_blocks()) return true;

  if (m_provide_rowid) {
    memset(table()->file->ref, 0, table()->file->ref_length);
  }

  if (m_join != nullptr) {
    if (m_join->m_windows.elements > 0 && !m_join->m_windowing_steps) {
      // Initialize state of window functions as window access path
      // will be shortcut.
      for (Window &w : m_join->m_windows) {
        w.reset_all_wf_state();
      }
    }
  }

  m_input_slice = m_join->get_ref_item_slice();

  m_row_number = 0;
  return m_subquery_iterator->Init();
}

int StreamingIterator::Read() {
  /*
    Enable the items which one should use if one wants to evaluate
    anything (e.g. functions in WHERE, HAVING) involving columns of this
    table. Make sure to switch to the right output slice before we
    exit the function.
  */
  m_join->set_ref_item_slice(m_input_slice);
  auto switch_to_output_slice = create_scope_guard([&] {
    if (m_output_slice != -1 && !m_join->ref_items[m_output_slice].is_null()) {
      m_join->set_ref_item_slice(m_output_slice);
    }
  });

  int error = m_subquery_iterator->Read();
  if (error != 0) return error;

  // Materialize items for this row.
  if (copy_funcs(m_temp_table_param, thd())) return 1;

  if (m_provide_rowid) {
    memcpy(table()->file->ref, &m_row_number, sizeof(m_row_number));
    ++m_row_number;
  }

  return 0;
}

/**
  Aggregates unsorted data into a temporary table, using update operations
  to keep running aggregates. After that, works as a MaterializeIterator
  in that it allows the temporary table to be scanned.

  'Profiler' should be 'IteratorProfilerImpl' for 'EXPLAIN ANALYZE' and
  'DummyIteratorProfiler' otherwise. It is implemented as a a template parameter
  to minimize the impact this probe has on normal query execution.
 */
template <typename Profiler>
class TemptableAggregateIterator final : public TableRowIterator {
 public:
  TemptableAggregateIterator(
      THD *thd, unique_ptr_destroy_only<RowIterator> subquery_iterator,
      Temp_table_param *temp_table_param, TABLE *table,
      unique_ptr_destroy_only<RowIterator> table_iterator, JOIN *join,
      int ref_slice);

  bool Init() override;
  int Read() override;
  void SetNullRowFlag(bool is_null_row) override {
    m_table_iterator->SetNullRowFlag(is_null_row);
  }
  void EndPSIBatchModeIfStarted() override {
    m_table_iterator->EndPSIBatchModeIfStarted();
    m_subquery_iterator->EndPSIBatchModeIfStarted();
  }
  void UnlockRow() override {}

  const IteratorProfiler *GetProfiler() const override {
    assert(thd()->lex->is_explain_analyze);
    return &m_profiler;
  }

  const Profiler *GetTableIterProfiler() const {
    return &m_table_iter_profiler;
  }

 private:
  /// The iterator we are reading rows from.
  unique_ptr_destroy_only<RowIterator> m_subquery_iterator;

  /// The iterator used to scan the resulting temporary table.
  unique_ptr_destroy_only<RowIterator> m_table_iterator;

  Temp_table_param *m_temp_table_param;
  JOIN *const m_join;
  const int m_ref_slice;

  /**
      Profiling data for this iterator. Used for 'EXPLAIN ANALYZE'.
      @see MaterializeIterator#m_profiler for a description of how
      this is used.
  */
  Profiler m_profiler;

  /**
      Profiling data for m_table_iterator,
      @see MaterializeIterator#m_table_iter_profiler.
  */
  Profiler m_table_iter_profiler;

  // See MaterializeIterator::doing_hash_deduplication().
  bool using_hash_key() const { return table()->hash_field; }

  bool move_table_to_disk(int error, bool was_insert);
};

/**
  Move the in-memory temporary table to disk.

  @param[in] error_code The error code because of which the table
                        is being moved to disk.
  @param[in] was_insert True, if the table is moved to disk during
                        an insert operation.

  @returns true if error.
*/
template <typename Profiler>
bool TemptableAggregateIterator<Profiler>::move_table_to_disk(int error_code,
                                                              bool was_insert) {
  if (create_ondisk_from_heap(thd(), table(), error_code, was_insert,
                              /*ignore_last_dup=*/false,
                              /*is_duplicate=*/nullptr)) {
    return true;
  }
  int error = table()->file->ha_index_init(0, false);
  if (error != 0) {
    PrintError(error);
    return true;
  }
  return false;
}

template <typename Profiler>
TemptableAggregateIterator<Profiler>::TemptableAggregateIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> subquery_iterator,
    Temp_table_param *temp_table_param, TABLE *table,
    unique_ptr_destroy_only<RowIterator> table_iterator, JOIN *join,
    int ref_slice)
    : TableRowIterator(thd, table),
      m_subquery_iterator(std::move(subquery_iterator)),
      m_table_iterator(std::move(table_iterator)),
      m_temp_table_param(temp_table_param),
      m_join(join),
      m_ref_slice(ref_slice) {}

template <typename Profiler>
bool TemptableAggregateIterator<Profiler>::Init() {
  // NOTE: We never scan these tables more than once, so we don't need to
  // check whether we have already materialized.

  Opt_trace_context *const trace = &thd()->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_exec(trace, "temp_table_aggregate");
  trace_exec.add_select_number(m_join->query_block->select_number);
  Opt_trace_array trace_steps(trace, "steps");
  const typename Profiler::TimeStamp start_time = Profiler::Now();

  if (m_subquery_iterator->Init()) {
    return true;
  }

  if (!table()->is_created()) {
    if (instantiate_tmp_table(thd(), table())) {
      return true;
    }
    empty_record(table());
  } else {
    if (table()->file->inited) {
      // If we're being called several times (in particular, as part of a
      // LATERAL join), the table iterator may have started a scan, so end it
      // before we start our own.
      table()->file->ha_index_or_rnd_end();
    }
    table()->file->ha_delete_all_rows();
  }

  // Initialize the index used for finding the groups.
  if (table()->file->ha_index_init(0, false)) {
    return true;
  }
  auto end_unique_index =
      create_scope_guard([&] { table()->file->ha_index_end(); });

  PFSBatchMode pfs_batch_mode(m_subquery_iterator.get());
  for (;;) {
    int read_error = m_subquery_iterator->Read();
    if (read_error > 0 || thd()->is_error())  // Fatal error
      return true;
    else if (read_error < 0)
      break;
    else if (thd()->killed)  // Aborted by user
    {
      thd()->send_kill_message();
      return true;
    }

    // Materialize items for this row.
    if (copy_funcs(m_temp_table_param, thd(), CFT_FIELDS))
      return true; /* purecov: inspected */

    // See if we have seen this row already; if so, we want to update it,
    // not insert a new one.
    bool group_found;
    if (using_hash_key()) {
      /*
        We need to call copy_funcs here in order to get correct value for
        hash_field. However, this call isn't needed so early when
        hash_field isn't used as it would cause unnecessary additional
        evaluation of functions to be copied when 2nd and further records
        in group are found.
      */
      if (copy_funcs(m_temp_table_param, thd()))
        return true; /* purecov: inspected */
      group_found = !check_unique_fields(table());
    } else {
      for (ORDER *group = table()->group; group; group = group->next) {
        Item *item = *group->item;
        item->save_org_in_field(group->field_in_tmp_table);
        if (thd()->is_error()) return true;
        /* Store in the used key if the field was 0 */
        if (item->is_nullable())
          group->buff[-1] = (char)group->field_in_tmp_table->is_null();
      }
      const uchar *key = m_temp_table_param->group_buff;
      group_found = !table()->file->ha_index_read_map(
          table()->record[1], key, HA_WHOLE_KEY, HA_READ_KEY_EXACT);
    }
    if (group_found) {
      // Update the existing record. (If it's unchanged, that's a
      // nonfatal error.)
      restore_record(table(), record[1]);
      update_tmptable_sum_func(m_join->sum_funcs, table());
      if (thd()->is_error()) {
        return true;
      }
      DBUG_EXECUTE_IF("simulate_temp_storage_engine_full",
                      DBUG_SET("+d,temptable_allocator_record_file_full"););
      int error =
          table()->file->ha_update_row(table()->record[1], table()->record[0]);

      DBUG_EXECUTE_IF("simulate_temp_storage_engine_full",
                      DBUG_SET("-d,temptable_allocator_record_file_full"););
      /*
        The agrregation can result in a row update with the same values,
        ignore the error. In case the temporary table has exhausted the memory
        (error HA_ERR_RECORD_FILE_FULL checked in create_ondisk_from_heap()),
        move the table to disk and retry the update operation.
      */
      if (error != 0 && error != HA_ERR_RECORD_IS_THE_SAME) {
        if (move_table_to_disk(error, /*insert_operation=*/false)) {
          end_unique_index.release();
          return true;
        }
        /*
          The key of the temporary table can be a hash of the group-by columns
          or the group-by columns themselves. Find the row to be updated in the
          newly created table.
        */
        const uchar *key;
        if (using_hash_key()) {
          key = table()->hash_field->field_ptr();
        } else {
          key = m_temp_table_param->group_buff;
        }
        // Read the record to be updated.
        if (table()->file->ha_index_read_map(table()->record[1], key,
                                             HA_WHOLE_KEY, HA_READ_KEY_EXACT)) {
          return true;
        }
        /*
          As the table has moved to disk, the references to the blobs
          in record[0], if any, would be stale. Copy the record and
          re-evaluate the functions.
        */
        restore_record(table(), record[1]);
        update_tmptable_sum_func(m_join->sum_funcs, table());
        if (thd()->is_error()) {
          return true;
        }
        // Retry the update on the newly created on-disk table.
        error = table()->file->ha_update_row(table()->record[1],
                                             table()->record[0]);
        if (error != 0 && error != HA_ERR_RECORD_IS_THE_SAME) {
          PrintError(error);
          return true;
        }
      }
      continue;
    }

    // OK, we need to insert a new row; we need to materialize any items
    // that we are doing GROUP BY on.

    /*
      Why do we advance the slice here and not before copy_fields()?
      Because of the evaluation of *group->item above: if we do it with
      this tmp table's slice, *group->item points to the field
      materializing the expression, which hasn't been calculated yet. We
      could force the missing calculation by doing copy_funcs() before
      evaluating *group->item; but then, for a group made of N rows, we
      might be doing N evaluations of another function when only one would
      suffice (like the '*' in "SELECT a, a*a ... GROUP BY a": only the
      first/last row of the group, needs to evaluate a*a).
    */
    Switch_ref_item_slice slice_switch(m_join, m_ref_slice);

    /*
      Copy null bits from group key to table
      We can't copy all data as the key may have different format
      as the row data (for example as with VARCHAR keys)
    */
    if (!using_hash_key()) {
      ORDER *group;
      KEY_PART_INFO *key_part;
      for (group = table()->group, key_part = table()->key_info[0].key_part;
           group; group = group->next, key_part++) {
        // Field null indicator is located one byte ahead of field value.
        // @todo - check if this NULL byte is really necessary for
        // grouping
        if (key_part->null_bit)
          memcpy(table()->record[0] + key_part->offset - 1, group->buff - 1, 1);
      }
      /* See comment on copy_funcs above. */
      if (copy_funcs(m_temp_table_param, thd())) return true;
    }
    assert(!thd()->is_error());
    init_tmptable_sum_functions(m_join->sum_funcs);
    if (thd()->is_error()) {
      return true;
    }
    int error = table()->file->ha_write_row(table()->record[0]);
    if (error != 0) {
      /*
         If the error is HA_ERR_FOUND_DUPP_KEY and the grouping involves a
         TIMESTAMP field, throw a meaningful error to user with the actual
         reason and the workaround. I.e, "Grouping on temporal is
         non-deterministic for timezones having DST. Please consider switching
         to UTC for this query". This is a temporary measure until we implement
         WL#13148 (Do all internal handling TIMESTAMP in UTC timezone), which
         will make such problem impossible.
       */
      if (error == HA_ERR_FOUND_DUPP_KEY) {
        for (ORDER *group = table()->group; group; group = group->next) {
          if (group->field_in_tmp_table->type() == MYSQL_TYPE_TIMESTAMP) {
            my_error(ER_GROUPING_ON_TIMESTAMP_IN_DST, MYF(0));
            return true;
          }
        }
      }

      if (move_table_to_disk(error, /*insert_operation=*/true)) {
        end_unique_index.release();
        return true;
      }
    } else {
      // Count the number of rows materialized.
      m_profiler.IncrementNumRows(1);
    }
  }

  table()->file->ha_index_end();
  end_unique_index.release();

  table()->materialized = true;

  m_profiler.StopInit(start_time);
  const bool err = m_table_iterator->Init();
  m_table_iter_profiler.StopInit(start_time);
  return err;
}

template <typename Profiler>
int TemptableAggregateIterator<Profiler>::Read() {
  const typename Profiler::TimeStamp start_time = Profiler::Now();

  /*
    Enable the items which one should use if one wants to evaluate
    anything (e.g. functions in WHERE, HAVING) involving columns of this
    table.
  */
  if (m_join != nullptr && m_ref_slice != -1) {
    if (!m_join->ref_items[m_ref_slice].is_null()) {
      m_join->set_ref_item_slice(m_ref_slice);
    }
  }
  int err = m_table_iterator->Read();
  m_table_iter_profiler.StopRead(start_time, err == 0);
  return err;
}

RowIterator *temptable_aggregate_iterator::CreateIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> subquery_iterator,
    Temp_table_param *temp_table_param, TABLE *table,
    unique_ptr_destroy_only<RowIterator> table_iterator, JOIN *join,
    int ref_slice) {
  if (thd->lex->is_explain_analyze) {
    RowIterator *const table_iter_ptr = table_iterator.get();

    auto iter =
        new (thd->mem_root) TemptableAggregateIterator<IteratorProfilerImpl>(
            thd, std::move(subquery_iterator), temp_table_param, table,
            std::move(table_iterator), join, ref_slice);

    /*
      Provide timing data for the iterator that iterates over the temporary
      table. This should include the time spent both materializing the table
      and iterating over it.
    */
    table_iter_ptr->SetOverrideProfiler(iter->GetTableIterProfiler());
    return iter;
  } else {
    return new (thd->mem_root)
        TemptableAggregateIterator<DummyIteratorProfiler>(
            thd, std::move(subquery_iterator), temp_table_param, table,
            std::move(table_iterator), join, ref_slice);
  }
}

MaterializedTableFunctionIterator::MaterializedTableFunctionIterator(
    THD *thd, Table_function *table_function, TABLE *table,
    unique_ptr_destroy_only<RowIterator> table_iterator)
    : TableRowIterator(thd, table),
      m_table_iterator(std::move(table_iterator)),
      m_table_function(table_function) {}

bool MaterializedTableFunctionIterator::Init() {
  if (!table()->materialized) {
    // Create the table if it's the very first time.
    if (table()->pos_in_table_list->create_materialized_table(thd())) {
      return true;
    }
  }
  if (m_table_function->fill_result_table()) {
    return true;
  }
  return m_table_iterator->Init();
}

WeedoutIterator::WeedoutIterator(THD *thd,
                                 unique_ptr_destroy_only<RowIterator> source,
                                 SJ_TMP_TABLE *sj,
                                 table_map tables_to_get_rowid_for)
    : RowIterator(thd),
      m_source(std::move(source)),
      m_sj(sj),
      m_tables_to_get_rowid_for(tables_to_get_rowid_for) {
  // Confluent weedouts should have been rewritten to LIMIT 1 earlier.
  assert(!m_sj->is_confluent);
  assert(m_sj->tmp_table != nullptr);
}

bool WeedoutIterator::Init() {
  if (m_sj->tmp_table->file->ha_delete_all_rows()) {
    return true;
  }
  if (m_sj->tmp_table->hash_field != nullptr &&
      !m_sj->tmp_table->file->inited) {
    m_sj->tmp_table->file->ha_index_init(0, false);
  }
  for (SJ_TMP_TABLE_TAB *tab = m_sj->tabs; tab != m_sj->tabs_end; ++tab) {
    TABLE *table = tab->qep_tab->table();
    if (m_tables_to_get_rowid_for & table->pos_in_table_list->map()) {
      table->prepare_for_position();
    }
  }
  return m_source->Init();
}

int WeedoutIterator::Read() {
  for (;;) {
    int ret = m_source->Read();
    if (ret != 0) {
      // Error, or EOF.
      return ret;
    }

    for (SJ_TMP_TABLE_TAB *tab = m_sj->tabs; tab != m_sj->tabs_end; ++tab) {
      TABLE *table = tab->qep_tab->table();
      if ((m_tables_to_get_rowid_for & table->pos_in_table_list->map()) &&
          can_call_position(table)) {
        table->file->position(table->record[0]);
      }
    }

    ret = do_sj_dups_weedout(thd(), m_sj);
    if (ret == -1) {
      // Error.
      return 1;
    }

    if (ret == 0) {
      // Not a duplicate, so return the row.
      return 0;
    }

    // Duplicate, so read the next row instead.
  }
}

RemoveDuplicatesIterator::RemoveDuplicatesIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> source, JOIN *join,
    Item **group_items, int group_items_size)
    : RowIterator(thd), m_source(std::move(source)) {
  m_caches = Bounds_checked_array<Cached_item *>::Alloc(thd->mem_root,
                                                        group_items_size);
  for (int i = 0; i < group_items_size; ++i) {
    m_caches[i] = new_Cached_item(thd, group_items[i]);
    join->semijoin_deduplication_fields.push_back(m_caches[i]);
  }
}

bool RemoveDuplicatesIterator::Init() {
  m_first_row = true;
  return m_source->Init();
}

int RemoveDuplicatesIterator::Read() {
  for (;;) {
    int err = m_source->Read();
    if (err != 0) {
      return err;
    }

    if (thd()->killed) {  // Aborted by user.
      thd()->send_kill_message();
      return 1;
    }

    bool any_changed = false;
    for (Cached_item *cache : m_caches) {
      any_changed |= cache->cmp();
    }

    if (m_first_row || any_changed) {
      m_first_row = false;
      return 0;
    }

    // Same as previous row, so keep scanning.
    continue;
  }
}

RemoveDuplicatesOnIndexIterator::RemoveDuplicatesOnIndexIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> source, const TABLE *table,
    KEY *key, size_t key_len)
    : RowIterator(thd),
      m_source(std::move(source)),
      m_table(table),
      m_key(key),
      m_key_buf(new(thd->mem_root) uchar[key_len]),
      m_key_len(key_len) {}

bool RemoveDuplicatesOnIndexIterator::Init() {
  m_first_row = true;
  return m_source->Init();
}

int RemoveDuplicatesOnIndexIterator::Read() {
  for (;;) {
    int err = m_source->Read();
    if (err != 0) {
      return err;
    }

    if (thd()->killed) {  // Aborted by user.
      thd()->send_kill_message();
      return 1;
    }

    if (!m_first_row && key_cmp(m_key->key_part, m_key_buf, m_key_len) == 0) {
      // Same as previous row, so keep scanning.
      continue;
    }

    m_first_row = false;
    key_copy(m_key_buf, m_table->record[0], m_key, m_key_len);
    return 0;
  }
}

NestedLoopSemiJoinWithDuplicateRemovalIterator::
    NestedLoopSemiJoinWithDuplicateRemovalIterator(
        THD *thd, unique_ptr_destroy_only<RowIterator> source_outer,
        unique_ptr_destroy_only<RowIterator> source_inner, const TABLE *table,
        KEY *key, size_t key_len)
    : RowIterator(thd),
      m_source_outer(std::move(source_outer)),
      m_source_inner(std::move(source_inner)),
      m_table_outer(table),
      m_key(key),
      m_key_buf(new(thd->mem_root) uchar[key_len]),
      m_key_len(key_len) {
  assert(m_source_outer != nullptr);
  assert(m_source_inner != nullptr);
}

bool NestedLoopSemiJoinWithDuplicateRemovalIterator::Init() {
  if (m_source_outer->Init()) {
    return true;
  }
  m_deduplicate_against_previous_row = false;
  return false;
}

int NestedLoopSemiJoinWithDuplicateRemovalIterator::Read() {
  m_source_inner->SetNullRowFlag(false);

  for (;;) {  // Termination condition within loop.
    // Find an outer row that is different (key-wise) from the previous
    // one we returned.
    for (;;) {
      int err = m_source_outer->Read();
      if (err != 0) {
        return err;
      }
      if (thd()->killed) {  // Aborted by user.
        thd()->send_kill_message();
        return 1;
      }

      if (m_deduplicate_against_previous_row &&
          key_cmp(m_key->key_part, m_key_buf, m_key_len) == 0) {
        // Same as previous row, so keep scanning.
        continue;
      }

      break;
    }

    if (thd()->killed) {  // Aborted by user.
      thd()->send_kill_message();
      return 1;
    }

    // Now find a single (matching) inner row.
    if (m_source_inner->Init()) {
      return 1;
    }

    int err = m_source_inner->Read();
    if (err == 1) {
      return 1;  // Error.
    }
    if (err == -1) {
      // No inner row found for this outer row, so search for a new outer
      // row, potentially with the same key.
      m_deduplicate_against_previous_row = false;
      continue;
    }

    // We found an inner row for this outer row, so we do not want more
    // with the same key.
    m_deduplicate_against_previous_row = true;
    key_copy(m_key_buf, m_table_outer->record[0], m_key, m_key_len);

    return 0;
  }
}

MaterializeInformationSchemaTableIterator::
    MaterializeInformationSchemaTableIterator(
        THD *thd, unique_ptr_destroy_only<RowIterator> table_iterator,
        Table_ref *table_list, Item *condition)
    : RowIterator(thd),
      m_table_iterator(std::move(table_iterator)),
      m_table_list(table_list),
      m_condition(condition) {}

bool MaterializeInformationSchemaTableIterator::Init() {
  if (!m_table_list->schema_table_filled) {
    m_table_list->table->file->ha_extra(HA_EXTRA_RESET_STATE);
    m_table_list->table->file->ha_delete_all_rows();
    free_io_cache(m_table_list->table);
    m_table_list->table->set_not_started();

    if (do_fill_information_schema_table(thd(), m_table_list, m_condition)) {
      return true;
    }

    m_table_list->schema_table_filled = true;
  }

  return m_table_iterator->Init();
}

AppendIterator::AppendIterator(
    THD *thd, std::vector<unique_ptr_destroy_only<RowIterator>> &&sub_iterators)
    : RowIterator(thd), m_sub_iterators(std::move(sub_iterators)) {
  assert(!m_sub_iterators.empty());
}

bool AppendIterator::Init() {
  m_current_iterator_index = 0;
  m_pfs_batch_mode_enabled = false;
  return m_sub_iterators[0]->Init();
}

int AppendIterator::Read() {
  if (m_current_iterator_index >= m_sub_iterators.size()) {
    // Already exhausted all iterators.
    return -1;
  }
  int err = m_sub_iterators[m_current_iterator_index]->Read();
  if (err != -1) {
    // A row, or error.
    return err;
  }

  // EOF. Go to the next iterator.
  m_sub_iterators[m_current_iterator_index]->EndPSIBatchModeIfStarted();
  if (++m_current_iterator_index >= m_sub_iterators.size()) {
    return -1;
  }
  if (m_sub_iterators[m_current_iterator_index]->Init()) {
    return 1;
  }
  if (m_pfs_batch_mode_enabled) {
    m_sub_iterators[m_current_iterator_index]->StartPSIBatchMode();
  }
  return Read();  // Try again, with the new iterator as current.
}

void AppendIterator::SetNullRowFlag(bool is_null_row) {
  assert(m_current_iterator_index < m_sub_iterators.size());
  m_sub_iterators[m_current_iterator_index]->SetNullRowFlag(is_null_row);
}

void AppendIterator::StartPSIBatchMode() {
  m_pfs_batch_mode_enabled = true;
  m_sub_iterators[m_current_iterator_index]->StartPSIBatchMode();
}

void AppendIterator::EndPSIBatchModeIfStarted() {
  for (const unique_ptr_destroy_only<RowIterator> &sub_iterator :
       m_sub_iterators) {
    sub_iterator->EndPSIBatchModeIfStarted();
  }
  m_pfs_batch_mode_enabled = false;
}

void AppendIterator::UnlockRow() {
  assert(m_current_iterator_index < m_sub_iterators.size());
  m_sub_iterators[m_current_iterator_index]->UnlockRow();
}
