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

#include "sql/iterators/hash_join_iterator.h"

#include <assert.h>
#include <algorithm>
#include <atomic>
#include <utility>
#include <vector>

#include "extra/robin-hood-hashing/robin_hood.h"
#include "field_types.h"
#include "my_alloc.h"
#include "my_bit.h"
#include "my_xxhash.h"

#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysqld_error.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/iterators/hash_join_buffer.h"
#include "sql/iterators/row_iterator.h"
#include "sql/pfs_batch_mode.h"
#include "sql/sql_class.h"
#include "sql/sql_list.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "template_utils.h"

class JOIN;

using hash_join_buffer::LoadBufferRowIntoTableBuffers;
using hash_join_buffer::LoadImmutableStringIntoTableBuffers;

// An arbitrary hash value for the empty string, to avoid the hash function
// from doing arithmetic on nullptr, which is undefined behavior.
static constexpr size_t kZeroKeyLengthHash = 2669509769;

HashJoinIterator::HashJoinIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> build_input,
    const Prealloced_array<TABLE *, 4> &build_input_tables,
    double estimated_build_rows,
    unique_ptr_destroy_only<RowIterator> probe_input,
    const Prealloced_array<TABLE *, 4> &probe_input_tables, bool store_rowids,
    table_map tables_to_get_rowid_for, size_t max_memory_available,
    const std::vector<HashJoinCondition> &join_conditions,
    bool allow_spill_to_disk, JoinType join_type,
    const Mem_root_array<Item *> &extra_conditions, bool probe_input_batch_mode,
    uint64_t *hash_table_generation)
    : RowIterator(thd),
      m_state(State::READING_ROW_FROM_PROBE_ITERATOR),
      m_hash_table_generation(hash_table_generation),
      m_build_input(std::move(build_input)),
      m_probe_input(std::move(probe_input)),
      m_probe_input_tables(probe_input_tables, store_rowids,
                           tables_to_get_rowid_for,
                           /*tables_to_store_contents_of_null_rows_for=*/0),
      m_build_input_tables(build_input_tables, store_rowids,
                           tables_to_get_rowid_for,
                           /*tables_to_store_contents_of_null_rows_for=*/0),
      m_tables_to_get_rowid_for(tables_to_get_rowid_for),
      m_row_buffer(m_build_input_tables, join_conditions, max_memory_available),
      m_join_conditions(PSI_NOT_INSTRUMENTED, join_conditions.data(),
                        join_conditions.data() + join_conditions.size()),
      m_chunk_files_on_disk(thd->mem_root, kMaxChunks),
      m_estimated_build_rows(estimated_build_rows),
      m_probe_input_batch_mode(probe_input_batch_mode),
      m_allow_spill_to_disk(allow_spill_to_disk),
      m_join_type(join_type) {
  assert(m_build_input != nullptr);
  assert(m_probe_input != nullptr);

  // If there are multiple extra conditions, merge them into a single AND-ed
  // condition, so evaluation of the item is a bit easier.
  if (extra_conditions.size() == 1) {
    m_extra_condition = extra_conditions[0];
  } else if (extra_conditions.size() > 1) {
    List<Item> items;
    for (Item *cond : extra_conditions) {
      items.push_back(cond);
    }
    m_extra_condition = new Item_cond_and(items);
    m_extra_condition->quick_fix_field();
    m_extra_condition->update_used_tables();
    m_extra_condition->apply_is_true();
  }
}

bool HashJoinIterator::InitRowBuffer() {
  if (m_row_buffer.Init()) {
    assert(thd()->is_error());  // my_error should have been called.
    return true;
  }

  m_current_row = LinkedImmutableString{nullptr};
  return false;
}

// Mark that blobs should be copied for each table that contains at least one
// geometry column.
static void MarkCopyBlobsIfTableContainsGeometry(
    const pack_rows::TableCollection &table_collection) {
  for (const pack_rows::Table &table : table_collection.tables()) {
    for (const pack_rows::Column &col : table.columns) {
      if (col.field_type == MYSQL_TYPE_GEOMETRY) {
        table.table->copy_blobs = true;
        break;
      }
    }
  }
}

bool HashJoinIterator::InitProbeIterator() {
  assert(m_state == State::READING_ROW_FROM_PROBE_ITERATOR);

  if (m_probe_input->Init()) {
    return true;
  }

  if (m_probe_input_batch_mode) {
    m_probe_input->StartPSIBatchMode();
  }
  return false;
}

bool HashJoinIterator::Init() {
  // If we are entirely in-memory and the JOIN we are part of hasn't been
  // asked to clear its hash tables since last time, we can reuse the table
  // without having to rebuild it. This is useful if we are on the right side
  // of a nested loop join, ie., we might be scanned multiple times.
  //
  // Note that this only ever happens in the hypergraph optimizer; see comments
  // in CreateIteratorFromAccessPath().
  if (m_row_buffer.inited() &&
      (m_hash_join_type == HashJoinType::IN_MEMORY ||
       (m_hash_join_type == HashJoinType::SPILL_TO_DISK &&
        m_chunk_files_on_disk.empty())) &&
      m_hash_table_generation != nullptr &&
      *m_hash_table_generation == m_last_hash_table_generation) {
    m_probe_row_match_flag = false;
    m_probe_chunk_current_row = 0;
    m_current_chunk = -1;
    m_hash_join_type = HashJoinType::IN_MEMORY;

    if (m_join_type == JoinType::ANTI && m_join_conditions.empty() &&
        m_extra_condition == nullptr && !m_row_buffer.empty()) {
      // See below.
      m_state = State::END_OF_ROWS;
      return false;
    } else {
      m_state = State::READING_ROW_FROM_PROBE_ITERATOR;
      m_probe_input->EndPSIBatchModeIfStarted();
      return InitProbeIterator();
    }
  }

  // Prepare to read the build input into the hash map.
  PrepareForRequestRowId(m_build_input_tables.tables(),
                         m_tables_to_get_rowid_for);
  if (m_build_input->Init()) {
    assert(thd()->is_error() ||
           thd()->killed);  // my_error should have been called.
    return true;
  }

  // We always start out by doing everything in memory.
  m_hash_join_type = HashJoinType::IN_MEMORY;
  m_write_to_probe_row_saving = false;

  m_build_iterator_has_more_rows = true;
  m_probe_input->EndPSIBatchModeIfStarted();
  m_probe_row_match_flag = false;

  // Set up the buffer that is used when
  // a) moving a row between the tables' record buffers, and,
  // b) when constructing a join key from join conditions.
  size_t upper_row_size = 0;
  if (!m_build_input_tables.has_blob_column()) {
    upper_row_size = ComputeRowSizeUpperBound(m_build_input_tables);
  }

  if (!m_probe_input_tables.has_blob_column()) {
    upper_row_size = std::max(upper_row_size,
                              ComputeRowSizeUpperBound(m_probe_input_tables));
  }

  if (m_temporary_row_and_join_key_buffer.reserve(upper_row_size)) {
    my_error(ER_OUTOFMEMORY, MYF(0), upper_row_size);
    return true;  // oom
  }

  // If any of the tables contains a geometry column, we must ensure that
  // the geometry data is copied to the row buffer (see
  // Field_geom::store_internal) instead of only setting the pointer to the
  // data. This is needed if the hash join spills to disk; when we read a row
  // back from chunk file, row data is stored in a temporary buffer. If not told
  // otherwise, Field_geom::store_internal will only store the pointer to the
  // data, and not the data itself. The data this field points to will then
  // become invalid when the temporary buffer is used for something else.
  MarkCopyBlobsIfTableContainsGeometry(m_probe_input_tables);
  MarkCopyBlobsIfTableContainsGeometry(m_build_input_tables);

  // Close any leftover files from previous iterations.
  m_chunk_files_on_disk.clear();

  m_build_chunk_current_row = 0;
  m_probe_chunk_current_row = 0;
  m_current_chunk = -1;

  PrepareForRequestRowId(m_probe_input_tables.tables(),
                         m_tables_to_get_rowid_for);

  // Build the hash table
  if (BuildHashTable()) {
    assert(thd()->is_error() ||
           thd()->killed);  // my_error should have been called.
    return true;
  }
  if (m_hash_table_generation != nullptr) {
    m_last_hash_table_generation = *m_hash_table_generation;
  }

  if (m_state == State::END_OF_ROWS) {
    // BuildHashTable() decided that the join is done (the build input is
    // empty, and we are in an inner-/semijoin. Anti-/outer join must output
    // NULL-complemented rows from the probe input).
    return false;
  }

  if (m_join_type == JoinType::ANTI && m_join_conditions.empty() &&
      m_extra_condition == nullptr && !m_row_buffer.empty()) {
    // For degenerate antijoins, we know we will never output anything
    // if there's anything in the hash table, so we can end right away.
    // (We also don't need to read more than one row, but
    // CreateHashJoinAccessPath() has already added a LIMIT 1 for us
    // in this case.)
    m_state = State::END_OF_ROWS;
    return false;
  }

  return InitProbeIterator();
}

// Construct a join key from a list of join conditions, where the join key from
// each join condition is concatenated together in the output buffer
// "join_key_buffer". The function returns true if a SQL NULL value is found.
static bool ConstructJoinKey(
    THD *thd, const Prealloced_array<HashJoinCondition, 4> &join_conditions,
    table_map tables_bitmap, String *join_key_buffer) {
  join_key_buffer->length(0);
  for (const HashJoinCondition &hash_join_condition : join_conditions) {
    if (hash_join_condition.join_condition()->append_join_key_for_hash_join(
            thd, tables_bitmap, hash_join_condition, join_conditions.size() > 1,
            join_key_buffer)) {
      // The join condition returned SQL NULL.
      return true;
    }
    if (thd->is_error()) return true;
  }
  return false;
}

// Write a single row to a HashJoinChunk. The row must lie in the record buffer
// (record[0]) for each involved table. The row is put into one of the chunks in
// the input vector "chunks"; which chunk to use is decided by the hash value of
// the join attribute.
static bool WriteRowToChunk(
    THD *thd, Mem_root_array<ChunkPair> *chunks, bool write_to_build_chunk,
    const pack_rows::TableCollection &tables,
    const Prealloced_array<HashJoinCondition, 4> &join_conditions,
    const uint32 xxhash_seed, bool row_has_match,
    bool store_row_with_null_in_join_key, String *join_key_and_row_buffer) {
  assert(!thd->is_error());
  bool null_in_join_key = ConstructJoinKey(
      thd, join_conditions, tables.tables_bitmap(), join_key_and_row_buffer);
  if (thd->is_error()) return true;

  if (null_in_join_key && !store_row_with_null_in_join_key) {
    // NULL values will never match in a inner join or a semijoin. The optimizer
    // will often set up a NULL filter for inner joins, but not in all cases. So
    // we must handle this gracefully instead of asserting.
    return false;
  }

  const uint64_t join_key_hash =
      join_key_and_row_buffer->length() == 0
          ? kZeroKeyLengthHash
          : MY_XXH64(join_key_and_row_buffer->ptr(),
                     join_key_and_row_buffer->length(), xxhash_seed);

  assert((chunks->size() & (chunks->size() - 1)) == 0);
  // Since we know that the number of chunks will be a power of two, do a
  // bitwise AND instead of (join_key_hash % chunks->size()).
  const size_t chunk_index = join_key_hash & (chunks->size() - 1);
  ChunkPair &chunk_pair = (*chunks)[chunk_index];
  if (write_to_build_chunk) {
    return chunk_pair.build_chunk.WriteRowToChunk(join_key_and_row_buffer,
                                                  row_has_match);
  } else {
    return chunk_pair.probe_chunk.WriteRowToChunk(join_key_and_row_buffer,
                                                  row_has_match);
  }
}

// Write all the remaining rows from the given iterator out to chunk files
// on disk. If the function returns true, an unrecoverable error occurred
// (IO error etc.).
static bool WriteRowsToChunks(
    THD *thd, RowIterator *iterator, const pack_rows::TableCollection &tables,
    const Prealloced_array<HashJoinCondition, 4> &join_conditions,
    const uint32 xxhash_seed, Mem_root_array<ChunkPair> *chunks,
    bool write_to_build_chunk, bool write_rows_with_null_in_join_key,
    table_map tables_to_get_rowid_for, String *join_key_buffer) {
  for (;;) {  // Termination condition within loop.
    int res = iterator->Read();
    if (res == 1) {
      assert(thd->is_error() ||
             thd->killed);  // my_error should have been called.
      return true;
    }

    if (res == -1) {
      return false;  // EOF; success.
    }

    assert(res == 0);

    RequestRowId(tables.tables(), tables_to_get_rowid_for);
    if (WriteRowToChunk(thd, chunks, write_to_build_chunk, tables,
                        join_conditions, xxhash_seed, /*row_has_match=*/false,
                        write_rows_with_null_in_join_key, join_key_buffer)) {
      assert(thd->is_error());  // my_error should have been called.
      return true;
    }
  }
}

// Initialize all HashJoinChunks for both inputs. When estimating how many
// chunks we need, we first assume that the estimated row count from the planner
// is correct. Furthermore, we assume that the current row buffer is
// representative of the overall row density, so that if we divide the
// (estimated) number of remaining rows by the number of rows read so far and
// use that as our chunk count, we will get on-disk chunks that each will fit
// into RAM when we read them back later. As a safeguard, we subtract a small
// percentage (reduction factor), since we'd rather get one or two extra chunks
// instead of having to re-read the probe input multiple times. We limit the
// number of chunks per input, so we don't risk hitting the server's limit for
// number of open files.
static bool InitializeChunkFiles(size_t estimated_rows_produced_by_join,
                                 size_t rows_in_hash_table,
                                 size_t max_chunk_files,
                                 const pack_rows::TableCollection &probe_tables,
                                 const pack_rows::TableCollection &build_tables,
                                 bool include_match_flag_for_probe,
                                 Mem_root_array<ChunkPair> *chunk_pairs) {
  constexpr double kReductionFactor = 0.9;
  const double reduced_rows_in_hash_table =
      std::max<double>(1, rows_in_hash_table * kReductionFactor);

  // Avoid underflow, since the hash table may contain more rows than the
  // estimate from the planner.
  const size_t remaining_rows =
      std::max(rows_in_hash_table, estimated_rows_produced_by_join) -
      rows_in_hash_table;

  const size_t chunks_needed = std::max<size_t>(
      1, std::ceil(remaining_rows / reduced_rows_in_hash_table));
  const size_t num_chunks = std::min(max_chunk_files, chunks_needed);

  // Ensure that the number of chunks is always a power of two. This allows
  // us to do some optimizations when calculating which chunk a row should
  // be placed in.
  const size_t num_chunks_pow_2 = my_round_up_to_next_power(num_chunks);

  assert(chunk_pairs != nullptr && chunk_pairs->empty());
  chunk_pairs->resize(num_chunks_pow_2);
  for (ChunkPair &chunk_pair : *chunk_pairs) {
    if (chunk_pair.build_chunk.Init(build_tables, /*uses_match_flags=*/false) ||
        chunk_pair.probe_chunk.Init(probe_tables,
                                    include_match_flag_for_probe)) {
      my_error(ER_TEMP_FILE_WRITE_FAILURE, MYF(0));
      return true;
    }
  }

  return false;
}

bool HashJoinIterator::BuildHashTable() {
  if (!m_build_iterator_has_more_rows) {
    m_state = State::END_OF_ROWS;
    return false;
  }

  // Restore the last row that was inserted into the row buffer. This is
  // necessary if the build input is a nested loop with a filter on the inner
  // side, like this:
  //
  //        +---Hash join---+
  //        |               |
  //  Nested loop          t1
  //  |         |
  //  t3    Filter: (t3.i < t2.i)
  //               |
  //              t2
  //
  // If the hash join is not allowed to spill to disk, we may need to re-fill
  // the hash table multiple times. If the nested loop happens to be in the
  // state "reading inner rows" when a re-fill is triggered, the filter will
  // look at the data in t3's record buffer in order to evaluate the filter. The
  // row in t3's record buffer may be any of the rows that was stored in the
  // hash table, and not the last row returned from t3. To ensure that the
  // filter is looking at the correct data, restore the last row that was
  // inserted into the hash table.
  if (m_row_buffer.Initialized() && m_row_buffer.LastRowStored() != nullptr) {
    LoadImmutableStringIntoTableBuffers(m_build_input_tables,
                                        m_row_buffer.LastRowStored());
  }

  if (InitRowBuffer()) {
    return true;
  }

  const bool reject_duplicate_keys = RejectDuplicateKeys();

  // If Init() is called multiple times (e.g., if hash join is inside an
  // dependent subquery), we must clear the NULL row flag, as it may have been
  // set by the previous executing of this hash join.
  m_build_input->SetNullRowFlag(/*is_null_row=*/false);

  PFSBatchMode batch_mode(m_build_input.get());
  for (;;) {  // Termination condition within loop.
    int res = m_build_input->Read();
    if (res == 1) {
      assert(thd()->is_error() ||
             thd()->killed);  // my_error should have been called.
      return true;
    }

    if (res == -1) {
      m_build_iterator_has_more_rows = false;
      // If the build input was empty, the result of inner joins and semijoins
      // will also be empty. However, if the build input was empty, the output
      // of antijoins will be all the rows from the probe input.
      if (m_row_buffer.empty() && m_join_type != JoinType::ANTI &&
          m_join_type != JoinType::OUTER) {
        m_state = State::END_OF_ROWS;
        return false;
      }

      // As we managed to read to the end of the build iterator, this is the
      // last time we will read from the probe iterator. Thus, we can disable
      // probe row saving again (it was enabled if the hash table ran out of
      // memory _and_ we were not allowed to spill to disk).
      m_write_to_probe_row_saving = false;
      SetReadingProbeRowState();
      return false;
    }
    assert(res == 0);
    RequestRowId(m_build_input_tables.tables(), m_tables_to_get_rowid_for);

    const hash_join_buffer::StoreRowResult store_row_result =
        m_row_buffer.StoreRow(thd(), reject_duplicate_keys);
    switch (store_row_result) {
      case hash_join_buffer::StoreRowResult::ROW_STORED:
        break;
      case hash_join_buffer::StoreRowResult::BUFFER_FULL: {
        // The row buffer is full, so start spilling to disk (if allowed). Note
        // that the row buffer checks for OOM _after_ the row was inserted, so
        // we should always manage to insert at least one row.
        assert(!m_row_buffer.empty());

        // If we are not allowed to spill to disk, just go on to reading from
        // the probe iterator.
        if (!m_allow_spill_to_disk) {
          if (m_join_type != JoinType::INNER) {
            // Enable probe row saving, so that unmatched probe rows are written
            // to the probe row saving file. After the next refill of the hash
            // table, we will read rows from the probe row saving file, ensuring
            // that we only read unmatched probe rows.
            InitWritingToProbeRowSavingFile();
          }
          SetReadingProbeRowState();
          return false;
        }

        if (InitializeChunkFiles(
                m_estimated_build_rows, m_row_buffer.size(), kMaxChunks,
                m_probe_input_tables, m_build_input_tables,
                /*include_match_flag_for_probe=*/m_join_type == JoinType::OUTER,
                &m_chunk_files_on_disk)) {
          assert(thd()->is_error());  // my_error should have been called.
          return true;
        }

        // Write out the remaining rows from the build input out to chunk files.
        // The probe input will be written out to chunk files later; we will do
        // it _after_ we have checked the probe input for matches against the
        // rows that are already written to the hash table. An alternative
        // approach would be to write out the remaining rows from the build
        // _and_ the rows that already are in the hash table. In that case, we
        // could also write out the entire probe input to disk here as well. But
        // we don't want to waste the rows that we already have stored in
        // memory.
        //
        // We never write out rows with NULL in condition for the build/right
        // input, as these rows will never match in a join condition.
        if (WriteRowsToChunks(thd(), m_build_input.get(), m_build_input_tables,
                              m_join_conditions, kChunkPartitioningHashSeed,
                              &m_chunk_files_on_disk,
                              true /* write_to_build_chunks */,
                              false /* write_rows_with_null_in_join_key */,
                              m_tables_to_get_rowid_for,
                              &m_temporary_row_and_join_key_buffer)) {
          assert(thd()->is_error() ||
                 thd()->killed);  // my_error should have been called.
          return true;
        }

        // Flush and position all chunk files from the build input at the
        // beginning.
        for (ChunkPair &chunk_pair : m_chunk_files_on_disk) {
          if (chunk_pair.build_chunk.Rewind()) {
            assert(thd()->is_error() ||
                   thd()->killed);  // my_error should have been called.
            return true;
          }
        }
        SetReadingProbeRowState();
        return false;
      }
      case hash_join_buffer::StoreRowResult::FATAL_ERROR:
        // An unrecoverable error. Most likely, malloc failed, so report OOM.
        // Note that we cannot say for sure how much memory we tried to allocate
        // when failing, so just report 'join_buffer_size' as the amount of
        // memory we tried to allocate.
        my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
                 thd()->variables.join_buff_size);
        return true;
    }
  }
}

bool HashJoinIterator::ReadNextHashJoinChunk() {
  // See if we should proceed to the next pair of chunk files. In general,
  // it works like this; if we are at the end of the build chunk, move to the
  // next. If not, keep reading from the same chunk pair. We also move to the
  // next pair of chunk files if the probe chunk file is empty.
  bool move_to_next_chunk = false;
  if (m_current_chunk == -1) {
    // We are before the first chunk, so move to the next.
    move_to_next_chunk = true;
  } else if (m_build_chunk_current_row >=
             m_chunk_files_on_disk[m_current_chunk].build_chunk.num_rows()) {
    // We are done reading all the rows from the build chunk.
    move_to_next_chunk = true;
  } else if (m_chunk_files_on_disk[m_current_chunk].probe_chunk.num_rows() ==
             0) {
    // The probe chunk file is empty.
    move_to_next_chunk = true;
  }

  if (move_to_next_chunk) {
    m_current_chunk++;
    m_build_chunk_current_row = 0;

    // Since we are moving to a new set of chunk files, ensure that we read from
    // the chunk file and not from the probe row saving file.
    m_read_from_probe_row_saving = false;
  }

  if (m_current_chunk == static_cast<int>(m_chunk_files_on_disk.size())) {
    // We have moved past the last chunk, so we are done.
    m_state = State::END_OF_ROWS;
    return false;
  }

  if (InitRowBuffer()) {
    return true;
  }

  HashJoinChunk &build_chunk =
      m_chunk_files_on_disk[m_current_chunk].build_chunk;

  const bool reject_duplicate_keys = RejectDuplicateKeys();
  for (; m_build_chunk_current_row < build_chunk.num_rows();
       ++m_build_chunk_current_row) {
    // Read the next row from the chunk file, and put it in the in-memory row
    // buffer. If the buffer goes full, do the probe phase against the rows we
    // managed to put in the buffer and continue reading where we left in the
    // next iteration.
    if (build_chunk.LoadRowFromChunk(&m_temporary_row_and_join_key_buffer,
                                     /*matched=*/nullptr)) {
      assert(thd()->is_error());  // my_error should have been called.
      return true;
    }

    hash_join_buffer::StoreRowResult store_row_result =
        m_row_buffer.StoreRow(thd(), reject_duplicate_keys);

    if (store_row_result == hash_join_buffer::StoreRowResult::BUFFER_FULL) {
      // The row buffer checks for OOM _after_ the row was inserted, so we
      // should always manage to insert at least one row.
      assert(!m_row_buffer.empty());

      // Since the last row read was actually stored in the buffer, increment
      // the row counter manually before breaking out of the loop.
      ++m_build_chunk_current_row;
      break;
    } else if (store_row_result ==
               hash_join_buffer::StoreRowResult::FATAL_ERROR) {
      // An unrecoverable error. Most likely, malloc failed, so report OOM.
      // Note that we cannot say for sure how much memory we tried to allocate
      // when failing, so just report 'join_buffer_size' as the amount of
      // memory we tried to allocate.
      my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
               thd()->variables.join_buff_size);
      return true;
    }

    assert(store_row_result == hash_join_buffer::StoreRowResult::ROW_STORED);
  }

  // Prepare to do a lookup in the hash table for all rows from the probe
  // chunk.
  if (m_chunk_files_on_disk[m_current_chunk].probe_chunk.Rewind()) {
    assert(thd()->is_error());  // my_error should have been called.
    return true;
  }
  m_probe_chunk_current_row = 0;
  SetReadingProbeRowState();

  if (m_build_chunk_current_row < build_chunk.num_rows() &&
      m_join_type != JoinType::INNER) {
    // The build chunk did not fit into memory, causing us to refill the hash
    // table once the probe input is consumed. If we don't take any special
    // action, we can end up outputting the same probe row twice if the probe
    // phase finds a match in both iterations through the hash table.
    // By enabling probe row saving, unmatched probe rows are written to a probe
    // row saving file. After the next hash table refill, we load the probe rows
    // from the probe row saving file instead of from the build chunk, and thus
    // ensuring that we only see unmatched probe rows. Note that we have not
    // started reading probe rows yet, but we are about to do so.
    InitWritingToProbeRowSavingFile();
  } else {
    m_write_to_probe_row_saving = false;
  }

  return false;
}

bool HashJoinIterator::ReadRowFromProbeIterator() {
  assert(m_current_chunk == -1);

  int result = m_probe_input->Read();
  if (result == 1) {
    assert(thd()->is_error() ||
           thd()->killed);  // my_error should have been called.
    return true;
  }

  if (result == 0) {
    RequestRowId(m_probe_input_tables.tables(), m_tables_to_get_rowid_for);

    // A row from the probe iterator is ready.
    LookupProbeRowInHashTable();
    if (thd()->is_error()) return true;
    return false;
  }

  assert(result == -1);
  m_probe_input->EndPSIBatchModeIfStarted();

  // The probe iterator is out of rows. We may be in three different situations
  // here (ordered from most common to less common):
  // 1. The build input is also empty, and the join is done. The iterator state
  //    will go into "LOADING_NEXT_CHUNK_PAIR", and we will see that there are
  //    no chunk files when trying to load the next pair of chunk files.
  // 2. We have degraded into an on-disk hash join, and we will now start
  //    reading from chunk files on disk.
  // 3. The build input is not empty, and we have not degraded into an on-disk
  //    hash join (i.e. we were not allowed due to a LIMIT in the query),
  //    re-populate the hash table with the remaining rows from the build input.
  if (m_allow_spill_to_disk) {
    m_hash_join_type = HashJoinType::SPILL_TO_DISK;
    m_state = State::LOADING_NEXT_CHUNK_PAIR;
    return false;
  }

  m_hash_join_type = HashJoinType::IN_MEMORY_WITH_HASH_TABLE_REFILL;
  if (m_write_to_probe_row_saving) {
    // If probe row saving is enabled, it means that the probe row saving write
    // file contains all the rows from the probe input that should be
    // read/processed again. We must swap the probe row saving writing and probe
    // row saving reading file _before_ calling BuildHashTable, since
    // BuildHashTable may initialize (and thus clear) the probe row saving write
    // file, losing any rows written to said file.
    if (InitReadingFromProbeRowSavingFile()) {
      assert(thd()->is_error());  // my_error should have been called.
      return true;
    }
  }

  if (BuildHashTable()) {
    assert(thd()->is_error() ||
           thd()->killed);  // my_error should have been called.
    return true;
  }

  switch (m_state) {
    case State::END_OF_ROWS:
      // BuildHashTable() decided that the join is done (the build input is
      // empty, and we are in an inner-/semijoin. Anti-/outer join must output
      // NULL-complemented rows from the probe input).
      return false;
    case State::READING_ROW_FROM_PROBE_ITERATOR:
      // Start reading from the beginning of the probe iterator.
      return InitProbeIterator();
    case State::READING_ROW_FROM_PROBE_ROW_SAVING_FILE:
      // The probe row saving read file is already initialized for reading
      // further up in this function.
      return false;
    default:
      assert(false);
      return true;
  }
}

bool HashJoinIterator::ReadRowFromProbeChunkFile() {
  assert(on_disk_hash_join() && m_current_chunk != -1);

  // Read one row from the current HashJoinChunk, and put
  // that row into the record buffer of the probe input table.
  HashJoinChunk &current_probe_chunk =
      m_chunk_files_on_disk[m_current_chunk].probe_chunk;
  if (m_probe_chunk_current_row >= current_probe_chunk.num_rows()) {
    // No more rows in the current probe chunk, so load the next chunk of
    // build rows into the hash table.
    if (m_write_to_probe_row_saving) {
      // If probe row saving is enabled, the build chunk did not fit in memory.
      // This causes us to refill the hash table with the rows from the build
      // chunk that did not fit, and thus read the probe chunk multiple times.
      // This can be problematic for semijoin; we do not want to output a probe
      // row that has a match in both parts of the hash table. To mitigate
      // this, we write probe rows that does not have a match in the hash table
      // to a probe row saving file (m_probe_row_saving_write_file), and read
      // from said file instead of from the probe input the next time.
      if (InitReadingFromProbeRowSavingFile()) {
        assert(thd()->is_error());  // my_error should have been called.
        return true;
      }
    } else {
      m_read_from_probe_row_saving = false;
    }

    m_state = State::LOADING_NEXT_CHUNK_PAIR;
    return false;
  } else if (current_probe_chunk.LoadRowFromChunk(
                 &m_temporary_row_and_join_key_buffer,
                 &m_probe_row_match_flag)) {
    assert(thd()->is_error());  // my_error should have been called.
    return true;
  }

  m_probe_chunk_current_row++;

  // A row from the chunk file is ready.
  LookupProbeRowInHashTable();
  return false;
}

bool HashJoinIterator::ReadRowFromProbeRowSavingFile() {
  // Read one row from the probe row saving file, and put that row into the
  // record buffer of the probe input table.
  if (m_probe_row_saving_read_file_current_row >=
      m_probe_row_saving_read_file.num_rows()) {
    // We are done reading all the rows from the probe row saving file. If probe
    // row saving is still enabled, we have a new set of rows in the probe row
    // saving write file.
    if (m_write_to_probe_row_saving) {
      if (InitReadingFromProbeRowSavingFile()) {
        assert(thd()->is_error());  // my_error should have been called.
        return true;
      }
    } else {
      m_read_from_probe_row_saving = false;
    }

    // If we are executing an on-disk hash join, go and load the next pair of
    // chunk files. If we are doing everything in memory with multiple hash
    // table refills, go and refill the hash table.
    if (m_hash_join_type == HashJoinType::SPILL_TO_DISK) {
      m_state = State::LOADING_NEXT_CHUNK_PAIR;
      return false;
    }
    assert(m_hash_join_type == HashJoinType::IN_MEMORY_WITH_HASH_TABLE_REFILL);

    // No more rows in the probe row saving file.
    if (BuildHashTable()) {
      assert(thd()->is_error() ||
             thd()->killed);  // my_error should have been called.
      return true;
    }

    if (m_state == State::END_OF_ROWS) {
      // BuildHashTable() decided that the join is done (the build input is
      // empty).
      return false;
    }

    SetReadingProbeRowState();
    return false;
  } else if (m_probe_row_saving_read_file.LoadRowFromChunk(
                 &m_temporary_row_and_join_key_buffer,
                 &m_probe_row_match_flag)) {
    assert(thd()->is_error());  // my_error should have been called.
    return true;
  }

  m_probe_row_saving_read_file_current_row++;

  // A row from the chunk file is ready.
  LookupProbeRowInHashTable();
  return false;
}

void HashJoinIterator::LookupProbeRowInHashTable() {
  if (m_join_conditions.empty()) {
    // Skip the call to find() in case we don't have any join conditions.
    // TODO(sgunders): Is this relevant for performance anymore?
    if (m_row_buffer.empty()) {
      m_current_row = LinkedImmutableString{nullptr};
    } else {
      m_current_row = m_row_buffer.begin()->second;
    }
    m_state = State::READING_FIRST_ROW_FROM_HASH_TABLE;
    return;
  }

  // Extract the join key from the probe input, and use that key as the lookup
  // key in the hash table.
  bool null_in_join_key = ConstructJoinKey(
      thd(), m_join_conditions, m_probe_input_tables.tables_bitmap(),
      &m_temporary_row_and_join_key_buffer);

  if (null_in_join_key) {
    if (m_join_type == JoinType::ANTI || m_join_type == JoinType::OUTER) {
      // SQL NULL was found, and we will never find a matching row in the hash
      // table. Let us indicate that, so that a null-complemented row is
      // returned.
      m_current_row = LinkedImmutableString{nullptr};
      m_state = State::READING_FIRST_ROW_FROM_HASH_TABLE;
    } else {
      SetReadingProbeRowState();
    }
    return;
  }

  hash_join_buffer::Key key{m_temporary_row_and_join_key_buffer.ptr(),
                            m_temporary_row_and_join_key_buffer.length()};

  auto it = m_row_buffer.find(key);
  if (it == m_row_buffer.end()) {
    m_current_row = LinkedImmutableString{nullptr};
  } else {
    m_current_row = it->second;
  }

  m_state = State::READING_FIRST_ROW_FROM_HASH_TABLE;
}

int HashJoinIterator::ReadJoinedRow() {
  if (m_current_row == nullptr) {
    // Signal that we have reached the end of hash table entries. Let the caller
    // determine which state we end up in.
    return -1;
  }

  // A row is ready in the hash table, so put the data from the hash table row
  // into the record buffers of the build input tables.
  LoadImmutableStringIntoTableBuffers(m_build_input_tables, m_current_row);
  return 0;
}

bool HashJoinIterator::WriteProbeRowToDiskIfApplicable() {
  // If we are spilling to disk, we need to match the row against rows from
  // the build input that are written out to chunk files. So we need to write
  // the probe row to chunk files as well. Semijoin/antijoin has an exception to
  // this; if the probe input already got a match in the hash table, we do not
  // need to write it out to disk. Outer joins should always write the row out
  // to disk, since the probe/left input should return NULL-complemented rows
  // even if the join condition contains SQL NULL.
  const bool write_rows_with_null_in_join_key = m_join_type == JoinType::OUTER;
  if (m_state == State::READING_FIRST_ROW_FROM_HASH_TABLE) {
    const bool found_match = m_current_row != nullptr;

    if ((m_join_type == JoinType::INNER || m_join_type == JoinType::OUTER) ||
        !found_match) {
      if (on_disk_hash_join() && m_current_chunk == -1) {
        if (WriteRowToChunk(thd(), &m_chunk_files_on_disk,
                            false /* write_to_build_chunk */,
                            m_probe_input_tables, m_join_conditions,
                            kChunkPartitioningHashSeed, found_match,
                            write_rows_with_null_in_join_key,
                            &m_temporary_row_and_join_key_buffer)) {
          return true;
        }
      }

      if (m_write_to_probe_row_saving &&
          m_probe_row_saving_write_file.WriteRowToChunk(
              &m_temporary_row_and_join_key_buffer,
              found_match || m_probe_row_match_flag)) {
        return true;
      }
    }
  }

  return false;
}

bool HashJoinIterator::JoinedRowPassesExtraConditions() const {
  if (m_extra_condition != nullptr) {
    return m_extra_condition->val_int() != 0;
  }

  return true;
}

int HashJoinIterator::ReadNextJoinedRowFromHashTable() {
  int res;
  bool passes_extra_conditions = false;
  do {
    res = ReadJoinedRow();

    // ReadJoinedRow() can only return 0 (row is ready) or -1 (EOF).
    assert(res == 0 || res == -1);

    // Evaluate any extra conditions that are attached to this iterator before
    // we return a row.
    if (res == 0) {
      passes_extra_conditions = JoinedRowPassesExtraConditions();
      if (thd()->is_error() || thd()->killed) {
        // Evaluation of extra conditions raised an error, so abort the join.
        return 1;
      }

      if (!passes_extra_conditions) {
        // Advance to the next matching row in the hash table. Note that the
        // iterator stays in the state READING_FIRST_ROW_FROM_HASH_TABLE even
        // though we are not actually reading the first row anymore. This is
        // because WriteProbeRowToDiskIfApplicable() needs to know if this is
        // the first row that matches both the join condition and any extra
        // conditions; only unmatched rows will be written to disk.
        m_current_row = m_current_row.Decode().next;
      }
    }
  } while (res == 0 && !passes_extra_conditions);

  // The row passed all extra conditions (or we are out of rows in the hash
  // table), so we can now write the row to disk.
  // Inner and outer joins: Write out all rows from the probe input (given that
  //   we have degraded into on-disk hash join).
  // Semijoin and antijoin: Write out rows that do not have any matching row in
  //   the hash table.
  if (WriteProbeRowToDiskIfApplicable()) {
    return 1;
  }

  if (res == -1) {
    // If we did not find a matching row in the hash table, antijoin and outer
    // join should output the last row read from the probe input together with a
    // NULL-complemented row from the build input. However, in case of on-disk
    // antijoin, a row from the probe input can match a row from the build input
    // that has already been written out to disk. So for on-disk antijoin, we
    // cannot output any rows until we have started reading from chunk files.
    //
    // On-disk outer join is a bit more tricky; we can only output a
    // NULL-complemented row if the probe row did not match anything from the
    // build input while doing any of the probe phases. We can have multiple
    // probe phases if e.g. a build chunk file is too big to fit in memory; we
    // would have to read the build chunk in multiple smaller chunks while doing
    // a probe phase for each of these smaller chunks. To keep track of this,
    // each probe row is prefixed with a match flag in the chunk files.
    bool return_null_complemented_row = false;
    if ((on_disk_hash_join() && m_current_chunk == -1) ||
        m_write_to_probe_row_saving) {
      return_null_complemented_row = false;
    } else if (m_join_type == JoinType::ANTI) {
      return_null_complemented_row = true;
    } else if (m_join_type == JoinType::OUTER &&
               m_state == State::READING_FIRST_ROW_FROM_HASH_TABLE &&
               !m_probe_row_match_flag) {
      return_null_complemented_row = true;
    }

    SetReadingProbeRowState();

    if (return_null_complemented_row) {
      m_build_input->SetNullRowFlag(true);
      return 0;
    }
    return -1;
  }

  // We have a matching row ready.
  switch (m_join_type) {
    case JoinType::SEMI:
      // Semijoin should return the first matching row, and then go to the next
      // row from the probe input.
      SetReadingProbeRowState();
      break;
    case JoinType::ANTI:
      // Antijoin should immediately go to the next row from the probe input,
      // without returning the matching row.
      SetReadingProbeRowState();
      return -1;  // Read the next row.
    case JoinType::OUTER:
    case JoinType::INNER:
      // Inner join should return all matching rows from the hash table before
      // moving to the next row from the probe input.
      m_state = State::READING_FROM_HASH_TABLE;
      break;
    case JoinType::FULL_OUTER:
      assert(false);
  }

  m_current_row = m_current_row.Decode().next;
  return 0;
}

int HashJoinIterator::Read() {
  for (;;) {
    if (thd()->killed) {  // Aborted by user.
      thd()->send_kill_message();
      return 1;
    }

    switch (m_state) {
      case State::LOADING_NEXT_CHUNK_PAIR:
        if (ReadNextHashJoinChunk()) {
          return 1;
        }
        break;
      case State::READING_ROW_FROM_PROBE_ITERATOR:
        if (ReadRowFromProbeIterator()) {
          return 1;
        }
        break;
      case State::READING_ROW_FROM_PROBE_CHUNK_FILE:
        if (ReadRowFromProbeChunkFile()) {
          return 1;
        }
        break;
      case State::READING_ROW_FROM_PROBE_ROW_SAVING_FILE:
        if (ReadRowFromProbeRowSavingFile()) {
          return 1;
        }
        break;
      case State::READING_FIRST_ROW_FROM_HASH_TABLE:
      case State::READING_FROM_HASH_TABLE: {
        const int res = ReadNextJoinedRowFromHashTable();
        if (res == 0) {
          // A joined row is ready, so send it to the client.
          return 0;
        }

        if (res == -1) {
          // No more matching rows in the hash table, or antijoin found a
          // matching row. Read a new row from the probe input.
          continue;
        }

        // An error occurred, so abort the join.
        assert(res == 1);
        return res;
      }
      case State::END_OF_ROWS:
        return -1;
    }
  }

  // Unreachable.
  assert(false);
  return 1;
}

bool HashJoinIterator::InitWritingToProbeRowSavingFile() {
  m_write_to_probe_row_saving = true;
  return m_probe_row_saving_write_file.Init(m_probe_input_tables,
                                            m_join_type == JoinType::OUTER);
}

bool HashJoinIterator::InitReadingFromProbeRowSavingFile() {
  m_probe_row_saving_read_file = std::move(m_probe_row_saving_write_file);
  m_probe_row_saving_read_file_current_row = 0;
  m_read_from_probe_row_saving = true;
  return m_probe_row_saving_read_file.Rewind();
}

void HashJoinIterator::SetReadingProbeRowState() {
  switch (m_hash_join_type) {
    case HashJoinType::IN_MEMORY:
      m_state = State::READING_ROW_FROM_PROBE_ITERATOR;
      break;
    case HashJoinType::IN_MEMORY_WITH_HASH_TABLE_REFILL:
      if (m_join_type == JoinType::INNER) {
        // As inner joins does not need probe row match flags, probe row saving
        // will never be activated for inner joins.
        m_state = State::READING_ROW_FROM_PROBE_ITERATOR;
      } else {
        m_state = State::READING_ROW_FROM_PROBE_ROW_SAVING_FILE;
      }
      break;
    case HashJoinType::SPILL_TO_DISK:
      if (m_read_from_probe_row_saving) {
        // Probe row saving may be activated if a build chunk did not fit in
        // memory.
        m_state = State::READING_ROW_FROM_PROBE_ROW_SAVING_FILE;
        return;
      }
      m_state = State::READING_ROW_FROM_PROBE_CHUNK_FILE;
      break;
  }
}
