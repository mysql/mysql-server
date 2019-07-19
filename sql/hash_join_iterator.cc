/* Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/hash_join_iterator.h"

#include <sys/types.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "extra/lz4/my_xxhash.h"
#include "my_alloc.h"
#include "my_bit.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "scope_guard.h"
#include "sql/handler.h"
#include "sql/hash_join_buffer.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/row_iterator.h"
#include "sql/sql_class.h"
#include "sql/sql_executor.h"
#include "sql/sql_select.h"
#include "sql/table.h"

constexpr size_t HashJoinIterator::kMaxChunks;

// Make a hash join condition for each equality comparison. This may entail
// allocating type cast nodes; see the comments on HashJoinCondition for more
// details.
static std::vector<HashJoinCondition> ItemToHashJoinConditions(
    const std::vector<Item_func_eq *> &join_conditions, MEM_ROOT *mem_root) {
  std::vector<HashJoinCondition> result;
  for (Item_func_eq *item_func_eq : join_conditions) {
    result.emplace_back(item_func_eq, mem_root);
  }

  return result;
}

HashJoinIterator::HashJoinIterator(
    THD *thd, unique_ptr_destroy_only<RowIterator> build_input,
    const std::vector<QEP_TAB *> &build_input_tables,
    unique_ptr_destroy_only<RowIterator> probe_input,
    QEP_TAB *probe_input_table, size_t max_memory_available,
    const std::vector<Item_func_eq *> &join_conditions,
    bool allow_spill_to_disk)
    : RowIterator(thd),
      m_state(State::READING_ROW_FROM_PROBE_ITERATOR),
      m_build_input(move(build_input)),
      m_probe_input(move(probe_input)),
      m_probe_input_table({probe_input_table}),
      m_build_input_tables(build_input_tables),
      m_row_buffer(m_build_input_tables,
                   ItemToHashJoinConditions(join_conditions, thd->mem_root),
                   max_memory_available),
      m_join_conditions(PSI_NOT_INSTRUMENTED),
      m_chunk_files_on_disk(thd->mem_root, kMaxChunks),
      m_enable_batch_mode_for_probe_input(
          probe_input_table->pfs_batch_update(probe_input_table->join())),
      m_allow_spill_to_disk(allow_spill_to_disk) {
  DBUG_ASSERT(m_build_input != nullptr);
  DBUG_ASSERT(m_probe_input != nullptr);

  for (Item_func_eq *join_condition : join_conditions) {
    DBUG_ASSERT(join_condition->arg_count == 2);
    m_join_conditions.emplace_back(join_condition, thd->mem_root);
  }

  // Mark that this iterator will provide the row ID, so that iterators above
  // this one does not call position(). See QEP_TAB::rowid_status for more
  // details.
  for (const hash_join_buffer::Table &it : m_build_input_tables.tables()) {
    if (it.qep_tab->rowid_status == NEED_TO_CALL_POSITION_FOR_ROWID) {
      it.qep_tab->rowid_status = ROWID_PROVIDED_BY_ITERATOR_READ_CALL;
    }
  }

  for (const hash_join_buffer::Table &it : m_probe_input_table.tables()) {
    if (it.qep_tab->rowid_status == NEED_TO_CALL_POSITION_FOR_ROWID) {
      it.qep_tab->rowid_status = ROWID_PROVIDED_BY_ITERATOR_READ_CALL;
    }
  }
}

// Whether to turn on batch mode for the build input. This code is basically a
// copy of QEP_TAB::pfs_batch_update, except that we do not reject innermost
// tables.
static bool EnableBatchModeForBuildInput(
    const hash_join_buffer::TableCollection &build_input_tables) {
  // Use PFS batch mode unless
  //  1. the build input is a more complext subtree (typically
  //     NestedLoopIterator). If that is the case, we leave the responsibility
  //     of turning on batch mode to the iterator subtree.
  //  2. a table has eq_ref or const access type, or
  //  3. this tab contains a subquery that accesses one or more tables
  if (build_input_tables.tables().size() > 1) {  // case 1
    return false;
  }

  QEP_TAB *qep_tab = build_input_tables.tables()[0].qep_tab;
  return !(qep_tab->type() == JT_EQ_REF ||  // case 2
           qep_tab->type() == JT_CONST || qep_tab->type() == JT_SYSTEM ||
           (qep_tab->condition() != nullptr &&
            qep_tab->condition()->has_subquery()));  // case 3
}

bool HashJoinIterator::InitRowBuffer() {
  // After the row buffer is initialized, we want the row buffer iterators to
  // point to the end of the row buffer in order to have a clean state. But on
  // some platforms, especially windows, the iterator assignment operator will
  // try to access the data it points to. This may be problematic if the hash
  // join iterator is being re-inited; the iterators will point to data that has
  // already been freed when doing the iterator assignment. To avoid the
  // iterators to point to any data, call the destructors so that they have a
  // clean state.
  {
    // Due to a bug in LLVM, we have to introduce a non-nested alias in order to
    // call the destructor (https://bugs.llvm.org//show_bug.cgi?id=12350).
    using iterator = hash_join_buffer::HashJoinRowBuffer::hash_map_iterator;
    m_hash_map_iterator.iterator::~iterator();
    m_hash_map_end.iterator::~iterator();
  }

  if (m_row_buffer.Init(kHashTableSeed)) {
    DBUG_ASSERT(thd()->is_error());  // my_error should have been called.
    return true;
  }

  m_hash_map_iterator = m_row_buffer.end();
  m_hash_map_end = m_row_buffer.end();
  return false;
}

bool HashJoinIterator::Init() {
  // Prepare to read the build input into the hash map.
  if (m_build_input->Init()) {
    DBUG_ASSERT(thd()->is_error());  // my_error should have been called.
    return true;
  }

  // Set up the buffer that is used when
  // a) moving a row between the tables' record buffers, and,
  // b) when constructing a join key from join conditions.
  size_t upper_row_size = 0;
  if (!m_build_input_tables.has_blob_column()) {
    upper_row_size =
        hash_join_buffer::ComputeRowSizeUpperBound(m_build_input_tables);
  }

  if (!m_probe_input_table.has_blob_column()) {
    upper_row_size = std::max(
        upper_row_size,
        hash_join_buffer::ComputeRowSizeUpperBound(m_probe_input_table));
  }

  if (m_temporary_row_and_join_key_buffer.reserve(upper_row_size)) {
    my_error(ER_OUTOFMEMORY, MYF(0), upper_row_size);
    return true;  // oom
  }

  // Close any leftover files from previous iterations.
  m_chunk_files_on_disk.clear();

  m_build_chunk_current_row = 0;
  m_probe_chunk_current_row = 0;
  m_current_chunk = -1;

  if (EnableBatchModeForBuildInput(m_build_input_tables)) {
    m_build_input->StartPSIBatchMode();
  }

  // Build the hash table
  bool ret = BuildHashTable();
  m_build_input->EndPSIBatchModeIfStarted();
  if (ret) {
    DBUG_ASSERT(thd()->is_error());  // my_error should have been called.
    return true;
  }

  DBUG_ASSERT(m_state == State::END_OF_ROWS ||
              m_state == State::READING_ROW_FROM_PROBE_ITERATOR);
  return m_state == State::END_OF_ROWS ? false : m_probe_input->Init();
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
            thd, tables_bitmap, hash_join_condition, join_key_buffer)) {
      // The join condition returned SQL NULL.
      return true;
    }
  }
  return false;
}

// Write a single row to a HashJoinChunk. The row must lie in the record buffer
// (record[0]) for each involved table. The row is put into one of the chunks in
// the input vector "chunks"; which chunk to use is decided by the hash value of
// the join attribute.
static bool WriteRowToChunk(
    THD *thd, Mem_root_array<ChunkPair> *chunks, bool write_to_build_chunk,
    const hash_join_buffer::TableCollection &tables,
    const Prealloced_array<HashJoinCondition, 4> &join_conditions,
    const uint32 xxhash_seed, String *join_key_and_row_buffer) {
  if (ConstructJoinKey(thd, join_conditions, tables.tables_bitmap(),
                       join_key_and_row_buffer)) {
    // NULL values will never match in a inner join. The optimizer will often
    // set up a NULL filter for inner joins, but not in all cases. So we must
    // handle this gracefully instead of asserting.
    return false;
  }

  const uint64_t join_key_hash =
      MY_XXH64(join_key_and_row_buffer->ptr(),
               join_key_and_row_buffer->length(), xxhash_seed);

  DBUG_ASSERT((chunks->size() & (chunks->size() - 1)) == 0);
  // Since we know that the number of chunks will be a power of two, do a
  // bitwise AND instead of (join_key_hash % chunks->size()).
  const size_t chunk_index = join_key_hash & (chunks->size() - 1);
  ChunkPair &chunk_pair = (*chunks)[chunk_index];
  if (write_to_build_chunk) {
    return chunk_pair.build_chunk.WriteRowToChunk(join_key_and_row_buffer);
  } else {
    return chunk_pair.probe_chunk.WriteRowToChunk(join_key_and_row_buffer);
  }
}

// Request the row ID for all tables where it should be kept.
static void RequestRowId(
    const Prealloced_array<hash_join_buffer::Table, 4> &tables) {
  for (const hash_join_buffer::Table &it : tables) {
    TABLE *table = it.qep_tab->table();
    if (it.rowid_status == NEED_TO_CALL_POSITION_FOR_ROWID &&
        can_call_position(table)) {
      table->file->position(table->record[0]);
    }
  }
}

// Write all the remaining rows from the given iterator out to chunk files
// on disk. If the function returns true, an unrecoverable error occurred
// (IO error etc.).
static bool WriteRowsToChunks(
    THD *thd, RowIterator *iterator,
    const hash_join_buffer::TableCollection &tables,
    const Prealloced_array<HashJoinCondition, 4> &join_conditions,
    const uint32 xxhash_seed, Mem_root_array<ChunkPair> *chunks,
    bool write_to_build_chunk, String *join_key_buffer) {
  for (;;) {  // Termination condition within loop.
    int res = iterator->Read();
    if (res == 1) {
      DBUG_ASSERT(thd->is_error());  // my_error should have been called.
      return true;
    }

    if (res == -1) {
      return false;  // EOF; success.
    }

    DBUG_ASSERT(res == 0);

    RequestRowId(tables.tables());
    if (WriteRowToChunk(thd, chunks, write_to_build_chunk, tables,
                        join_conditions, xxhash_seed, join_key_buffer)) {
      DBUG_ASSERT(thd->is_error());  // my_error should have been called.
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
static bool InitializeChunkFiles(
    size_t estimated_rows_produced_by_join, size_t rows_in_hash_table,
    size_t max_chunk_files,
    const hash_join_buffer::TableCollection &probe_tables,
    const hash_join_buffer::TableCollection &build_tables,
    Mem_root_array<ChunkPair> *chunk_pairs) {
  constexpr double kReductionFactor = 0.9;
  const size_t reduced_rows_in_hash_table =
      std::max<size_t>(1, rows_in_hash_table * kReductionFactor);

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

  DBUG_ASSERT(chunk_pairs != nullptr && chunk_pairs->empty());
  chunk_pairs->resize(num_chunks_pow_2);
  for (ChunkPair &chunk_pair : *chunk_pairs) {
    if (chunk_pair.build_chunk.Init(build_tables) ||
        chunk_pair.probe_chunk.Init(probe_tables)) {
      my_error(ER_TEMP_FILE_WRITE_FAILURE, MYF(0));
      return true;
    }
  }

  return false;
}

bool HashJoinIterator::BuildHashTable() {
  if (InitRowBuffer()) {
    return true;
  }

  for (;;) {  // Termination condition within loop.
    int res = m_build_input->Read();
    if (res == 1) {
      DBUG_ASSERT(thd()->is_error());  // my_error should have been called.
      return true;
    }

    if (res == -1) {
      if (m_row_buffer.empty()) {
        m_state = State::END_OF_ROWS;
        return false;
      }

      m_state = State::READING_ROW_FROM_PROBE_ITERATOR;
      return false;
    }
    DBUG_ASSERT(res == 0);
    RequestRowId(m_build_input_tables.tables());

    switch (m_row_buffer.StoreRow(thd())) {
      case hash_join_buffer::StoreRowResult::ROW_STORED:
        break;
      case hash_join_buffer::StoreRowResult::BUFFER_FULL: {
        // The row buffer is full, so start spilling to disk (if allowed). Note
        // that the row buffer checks for OOM _after_ the row was inserted, so
        // we should always manage to insert at least one row.
        DBUG_ASSERT(!m_row_buffer.empty());

        // If we are not allowed to spill to disk, just go on to reading from
        // the probe iterator.
        if (!m_allow_spill_to_disk) {
          m_state = State::READING_ROW_FROM_PROBE_ITERATOR;
          return false;
        }

        // Ideally, we would use the estimated row count from the iterator. But
        // not all iterators has the row count available (i.e.
        // RemoveDuplicatesIterator), so get the row count directly from the
        // QEP_TAB.
        const QEP_TAB *last_table_in_join =
            m_build_input_tables.tables().back().qep_tab;
        if (InitializeChunkFiles(
                last_table_in_join->position()->prefix_rowcount,
                m_row_buffer.size(), kMaxChunks, m_probe_input_table,
                m_build_input_tables, &m_chunk_files_on_disk)) {
          DBUG_ASSERT(thd()->is_error());  // my_error should have been called.
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
        if (WriteRowsToChunks(thd(), m_build_input.get(), m_build_input_tables,
                              m_join_conditions, kChunkPartitioningHashSeed,
                              &m_chunk_files_on_disk,
                              true /* write_to_build_chunks */,
                              &m_temporary_row_and_join_key_buffer)) {
          DBUG_ASSERT(thd()->is_error());  // my_error should have been called.
          return true;
        }

        // Flush and position all chunk files from the build input at the
        // beginning.
        for (ChunkPair &chunk_pair : m_chunk_files_on_disk) {
          if (chunk_pair.build_chunk.Rewind()) {
            DBUG_ASSERT(
                thd()->is_error());  // my_error should have been called.
            return true;
          }
        }
        m_state = State::READING_ROW_FROM_PROBE_ITERATOR;
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

  for (; m_build_chunk_current_row < build_chunk.num_rows();
       ++m_build_chunk_current_row) {
    // Read the next row from the chunk file, and put it in the in-memory row
    // buffer. If the buffer goes full, do the probe phase against the rows we
    // managed to put in the buffer and continue reading where we left in the
    // next iteration.
    if (build_chunk.LoadRowFromChunk(&m_temporary_row_and_join_key_buffer)) {
      DBUG_ASSERT(thd()->is_error());  // my_error should have been called.
      return true;
    }

    hash_join_buffer::StoreRowResult store_row_result =
        m_row_buffer.StoreRow(thd());

    if (store_row_result == hash_join_buffer::StoreRowResult::BUFFER_FULL) {
      // The row buffer checks for OOM _after_ the row was inserted, so we
      // should always manage to insert at least one row.
      DBUG_ASSERT(!m_row_buffer.empty());

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

    DBUG_ASSERT(store_row_result ==
                hash_join_buffer::StoreRowResult::ROW_STORED);
  }

  // Prepare to do a lookup in the hash table for all rows from the probe
  // chunk.
  if (m_chunk_files_on_disk[m_current_chunk].probe_chunk.Rewind()) {
    DBUG_ASSERT(thd()->is_error());  // my_error should have been called.
    return true;
  }
  m_probe_chunk_current_row = 0;
  m_state = State::READING_ROW_FROM_PROBE_CHUNK_FILE;
  return false;
}

bool HashJoinIterator::ReadRowFromProbeIterator() {
  DBUG_ASSERT(m_current_chunk == -1);

  int result = m_probe_input->Read();
  if (result == 1) {
    DBUG_ASSERT(thd()->is_error());  // my_error should have been called.
    return true;
  } else if (result == -1) {
    // The probe iterator is out of rows. If we haven't degraded into an
    // on-disk hash join (i.e. we were not allowed due to a LIMIT in the
    // query), re-populate the hash table with the remaining rows from the
    // build input.
    if (!m_allow_spill_to_disk) {
      if (BuildHashTable()) {
        DBUG_ASSERT(thd()->is_error());  // my_error should have been called.
        return true;
      }

      // Start reading from the beginning of the probe iterator.
      DBUG_ASSERT(m_state == State::END_OF_ROWS ||
                  m_state == State::READING_ROW_FROM_PROBE_ITERATOR);
      return m_state == State::END_OF_ROWS ? false : m_probe_input->Init();
    } else {
      m_state = State::LOADING_NEXT_CHUNK_PAIR;
      return false;
    }
  }

  DBUG_ASSERT(result == 0);
  RequestRowId(m_probe_input_table.tables());

  // If we are spilling to disk, we need to match the row against rows from
  // the build input that are written out to chunk files. So we need to write
  // the probe row to chunk files as well.
  if (on_disk_hash_join()) {
    if (WriteRowToChunk(thd(), &m_chunk_files_on_disk,
                        false /* write_to_build_chunk */, m_probe_input_table,
                        m_join_conditions, kChunkPartitioningHashSeed,
                        &m_temporary_row_and_join_key_buffer)) {
      DBUG_ASSERT(thd()->is_error());  // my_error should have been called.
      return true;
    }
  }

  // A row from the probe iterator is ready.
  LookupProbeRowInHashTable();
  m_state = State::READING_FROM_HASH_TABLE;
  return false;
}

bool HashJoinIterator::ReadRowFromProbeChunkFile() {
  DBUG_ASSERT(on_disk_hash_join() && m_current_chunk != -1);

  // Read one row from the current HashJoinChunk, and put
  // that row into the record buffer of the probe input table.
  HashJoinChunk &current_probe_chunk =
      m_chunk_files_on_disk[m_current_chunk].probe_chunk;
  if (m_probe_chunk_current_row >= current_probe_chunk.num_rows()) {
    // No more rows in the current probe chunk, so load the next chunk of
    // build rows into the hash table.
    m_state = State::LOADING_NEXT_CHUNK_PAIR;
    return false;
  } else if (current_probe_chunk.LoadRowFromChunk(
                 &m_temporary_row_and_join_key_buffer)) {
    DBUG_ASSERT(thd()->is_error());  // my_error should have been called.
    return true;
  }

  m_probe_chunk_current_row++;

  // A row from the chunk file is ready.
  LookupProbeRowInHashTable();
  m_state = State::READING_FROM_HASH_TABLE;
  return false;
}

void HashJoinIterator::LookupProbeRowInHashTable() {
  if (m_join_conditions.empty()) {
    // Skip the call to equal_range in case we don't have any join conditions.
    // This can save up to 20% in case of multi-table joins.
    m_hash_map_iterator = m_row_buffer.begin();
    m_hash_map_end = m_row_buffer.end();
    return;
  }

  // Extract the join key from the probe input, and use that key as the lookup
  // key in the hash table.
  if (ConstructJoinKey(thd(), m_join_conditions,
                       m_probe_input_table.tables_bitmap(),
                       &m_temporary_row_and_join_key_buffer)) {
    // The join condition returned SQL NULL, and will never match in an inner
    // join.
    m_state = m_current_chunk == -1 ? State::READING_ROW_FROM_PROBE_ITERATOR
                                    : State::READING_ROW_FROM_PROBE_CHUNK_FILE;
    return;
  }

  hash_join_buffer::Key key(
      pointer_cast<const uchar *>(m_temporary_row_and_join_key_buffer.ptr()),
      m_temporary_row_and_join_key_buffer.length());

  auto range = m_row_buffer.equal_range(key);
  m_hash_map_iterator = range.first;
  m_hash_map_end = range.second;
}

int HashJoinIterator::ReadJoinedRow() {
  if (m_hash_map_iterator == m_hash_map_end) {
    // End of hash table entries. Read the next row from the probe input.
    m_state = m_current_chunk == -1 ? State::READING_ROW_FROM_PROBE_ITERATOR
                                    : State::READING_ROW_FROM_PROBE_CHUNK_FILE;
    return -1;
  }

  // A row is ready in the hash table, so put the data from the hash table row
  // into the record buffers of the build input tables.
  hash_join_buffer::LoadIntoTableBuffers(m_build_input_tables,
                                         m_hash_map_iterator->second);
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
        if (m_enable_batch_mode_for_probe_input) {
          m_probe_input->StartPSIBatchMode();
          m_enable_batch_mode_for_probe_input = false;
        }

        if (ReadRowFromProbeIterator()) {
          return 1;
        }
        break;
      case State::READING_ROW_FROM_PROBE_CHUNK_FILE:
        if (ReadRowFromProbeChunkFile()) {
          return 1;
        }
        break;
      case State::READING_FROM_HASH_TABLE: {
        const int res = ReadJoinedRow();
        if (res == -1) {
          DBUG_ASSERT(m_state == State::READING_ROW_FROM_PROBE_ITERATOR ||
                      m_state == State::READING_ROW_FROM_PROBE_CHUNK_FILE);
          continue;  // No more rows in the hash table. Get a new row from the
                     // probe input.
        }

        DBUG_ASSERT(res == 0);
        ++m_hash_map_iterator;
        return 0;  // A row is ready in the tables buffer
      }
      case State::END_OF_ROWS:
        return -1;
    }
  }

  // Unreachable.
  DBUG_ASSERT(false);
  return 1;
}

std::vector<std::string> HashJoinIterator::DebugString() const {
  std::string ret("Inner hash join");

  for (const HashJoinCondition &join_condition : m_join_conditions) {
    if (join_condition.join_condition() !=
        m_join_conditions[0].join_condition()) {
      ret.append(",");
    }
    ret.append(" " + ItemToString(join_condition.join_condition()));
  }

  return {ret};
}
