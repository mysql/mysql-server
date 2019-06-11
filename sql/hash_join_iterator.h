#ifndef SQL_HASH_JOIN_ITERATOR_H_
#define SQL_HASH_JOIN_ITERATOR_H_

/* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <stdio.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "my_alloc.h"
#include "my_inttypes.h"
#include "sql/hash_join_buffer.h"
#include "sql/hash_join_chunk.h"
#include "sql/item_cmpfunc.h"
#include "sql/mem_root_array.h"
#include "sql/row_iterator.h"
#include "sql/table.h"
#include "sql_string.h"

class THD;
class QEP_TAB;

struct ChunkPair {
  HashJoinChunk probe_chunk;
  HashJoinChunk build_chunk;
};

/// @file
///
/// An iterator for joining two inputs by using hashing to match rows from
/// the inputs.
///
/// The iterator starts out by doing everything in-memory. If everything fits
/// into memory, the joining algorithm works like this:
///
/// 1) Designate one input as the "build" input and one input as the "probe"
/// input. Ideally, the smallest input measured in total size (not number of
/// rows) should be designated as the build input.
///
/// 2) Read all the rows from the build input into an in-memory hash table.
/// The hash key used in the hash table is calculated from the join attributes,
/// e.g., if we have the following query where "orders" is designated as the
/// build input:
///
///   SELECT * FROM lineitem
///     INNER JOIN orders ON orders.o_orderkey = lineitem.l_orderkey;
///
/// the hash value will be calculated from the values in the column
/// orders.o_orderkey. Note that the optimizer recognizes implicit join
/// conditions, so this also works for SQL statements like:
///
///   SELECT * FROM orders, lineitem
///     WHERE orders.o_orderkey = lineitem.l_orderkey;
///
/// 3) Then, we read the rows from the probe input, one by one. For each row,
/// a hash key is calculated for the other side of the join (the probe input)
/// using the join attribute (lineitem.l_orderkey in the above example) and the
/// same hash function as in step 2. This hash key is used to do a lookup in the
/// hash table, and for each match, an output row is produced. Note that the row
/// from the probe input is already located in the table record buffers, and the
/// matching row stored in the hash table is restored back to the record buffers
/// where it originally came from. For details around how rows are stored and
/// restored, see comments on hash_join_buffer::StoreFromTableBuffers.
///
/// The size of the in-memory hash table is controlled by the system variable
/// join_buffer_size. If we run out of memory during step 2, we degrade into a
/// hybrid hash join. The data already in memory is processed using regular hash
/// join, and the remainder is processed using on-disk hash join. It works like
/// this:
///
/// 1) The rest of the rows in the build input that did not fit into the hash
/// table are partitioned out into a given amount of files, represented by
/// HashJoinChunks. We create an equal number of chunk files for both the probe
/// and build input. We determine which file to put a row in by calculating a
/// hash from the join attribute like in step 2 above, but using a different
/// hash function.
///
/// 2) Then, we read the rows from the probe input, one by one. We look for a
/// match in the hash table as described above, but the row is also written out
/// to the chunk file on disk, since it might match a row from the build input
/// that we've written to disk.
///
/// 3) When the entire probe input is read, we run the "classic" hash join on
/// each of the corresponding chunk file probe/build pairs. Since the rows are
/// partitioned using the same hash function for probe and build inputs, we know
/// that matching rows must be located in the same pair of chunk files.
///
/// If we are able to execute the hash join in memory (classic hash join),
/// the output will be sorted the same as the left (probe) input. If we start
/// spilling to disk, we lose any reasonable ordering properties.
///
/// Note that we still might end up in a case where a single chunk file from
/// disk won't fit into memory. This is resolved by reading as much as possible
/// into the hash table, and then reading the entire probe chunk file for each
/// time the hash table is reloaded. This might happen if we have a very skewed
/// data set, for instance.
///
/// When we start spilling to disk, we allocate a maximum of "kMaxChunks"
/// chunk files on disk for each of the two inputs. The reason for having an
/// upper limit is to avoid running out of file descriptors.
///
/// There is also a flag we can set to avoid hash join spilling to disk
/// regardless of the input size. If the flag is set, the join algorithm works
/// like this:
///
/// 1) Read as many rows as possible from the build input into an in-memory hash
/// table.
/// 2) When the hash table is full (we have reached the limit set by the system
/// variable join_buffer_size), start reading from the beginning of the probe
/// input, probing for matches in the hash table. Output a row for each match
/// found.
/// 3) When the probe input is empty, see if there are any remaining rows in the
/// build input. If so, clear the in-memory hash table and go to step 1,
/// continuing from the build input where we stopped the last time. If not, the
/// join is done.
///
/// Doing everything in memory can be beneficial in a few cases. Currently, it
/// is used when we have a LIMIT without sorting or grouping in the query. The
/// gain is that we start producing output rows a lot earlier than if we were to
/// spill both inputs out to disk. It could also be beneficial if the build
/// input _almost_ fits in memory; it would likely be better to read the probe
/// input twice instead of writing both inputs out to disk. However, we do not
/// currently do any such cost based optimization.
class HashJoinIterator final : public RowIterator {
 public:
  /// Construct a HashJoinIterator.
  ///
  /// @param thd
  ///   the thread handle
  /// @param build_input
  ///   the iterator for the build input
  /// @param build_input_tables
  ///   a list of all the tables in the build input. The tables are needed for
  ///   two things:
  ///   1) Accessing the columns when creating the join key during creation of
  ///   the hash table,
  ///   2) and accessing the column data when creating the row to be stored in
  ///   the hash table and/or the chunk file on disk.
  /// @param probe_input
  ///   the iterator for the probe input
  /// @param probe_input_table
  ///   the probe input table. Needed for the same reasons as
  ///   build_input_tables. We currently assume that this always is a single
  ///   table, but this is not a limitation per se; the iterator is ready to
  ///   handle multiple tables as the probe input.
  /// @param max_memory_available
  ///   the amount of memory available, in bytes, for this hash join iterator.
  ///   This can be user-controlled by setting the system variable
  ///   join_buffer_size.
  /// @param join_conditions
  ///   a list of all the join conditions between the two inputs
  /// @param allow_spill_to_disk
  ///   whether the hash join can spill to disk. This is set to false in some
  ///   cases where we have a LIMIT in the query
  HashJoinIterator(THD *thd, unique_ptr_destroy_only<RowIterator> build_input,
                   const std::vector<QEP_TAB *> &build_input_tables,
                   unique_ptr_destroy_only<RowIterator> probe_input,
                   QEP_TAB *probe_input_table, size_t max_memory_available,
                   const std::vector<Item_func_eq *> &join_conditions,
                   bool allow_spill_to_disk);

  bool Init() override;

  int Read() override;

  void SetNullRowFlag(bool is_null_row) override {
    m_build_input->SetNullRowFlag(is_null_row);
    m_probe_input->SetNullRowFlag(is_null_row);
  }

  void EndPSIBatchModeIfStarted() override {
    m_build_input->EndPSIBatchModeIfStarted();
    m_probe_input->EndPSIBatchModeIfStarted();
  }

  void UnlockRow() override {
    // Since both inputs may have been materialized to disk, we cannot unlock
    // them.
  }

  std::vector<std::string> DebugString() const override;

  std::vector<Child> children() const override {
    return std::vector<Child>{{m_probe_input.get(), ""},
                              {m_build_input.get(), "Hash"}};
  }

 private:
  /// Read all rows from the build input and store the rows into the in-memory
  /// hash table. If the hash table goes full, the rest of the rows are written
  /// out to chunk files on disk. See the class comment for more details.
  ///
  /// @retval true in case of error
  bool BuildHashTable();

  /// Read all rows from the next chunk file into the in-memory hash table.
  /// See the class comment for details.
  ///
  /// @retval true in case of error
  bool ReadNextHashJoinChunk();

  /// Read a single row from the probe iterator input into the tables' record
  /// buffers. If we have started spilling to disk, the row is written out to a
  /// chunk file on disk as well.
  ///
  /// The end condition is that either:
  /// a) a row is ready in the tables' record buffers, and the state will be set
  ///    to READING_FROM_HASH_TABLE.
  /// b) There are no more rows to process from the probe input, so the iterator
  ///    state will be LOADING_NEXT_CHUNK_PAIR.
  ///
  /// @retval true in case of error
  bool ReadRowFromProbeIterator();

  /// Read a single row from the current probe chunk file into the tables'
  /// record buffers. The end conditions are the same as for
  /// ReadRowFromProbeIterator().
  ///
  /// @retval true in case of error
  bool ReadRowFromProbeChunkFile();

  // Do a lookup in the hash table for matching rows from the build input.
  // The lookup is done by computing the join key from the probe input, and
  // using that join key for doing a lookup in the hash table. If the join key
  // contains one or more SQL NULLs, the row cannot match anything and will be
  // skipped, and the iterator state will be READING_ROW_FROM_PROBE_INPUT. If
  // not, the iterator state will be READING_FROM_HASH_TABLE.
  //
  // After this function is called, ReadJoinedRow() will return false until
  // there are no more matching rows for the computed join key.
  void LookupProbeRowInHashTable();

  /// Take the next matching row from the hash table, and put the row into the
  /// build tables' record buffers. The function expects that
  /// LookupProbeRowInHashTable() has been called up-front. The user must
  /// call ReadJoinedRow() as long as it returns false, as there may be
  /// multiple matching rows from the hash table.
  ///
  /// @retval 0 if a match was found and the row is put in the build tables'
  ///         record buffers
  /// @retval -1 if there are no more matching rows in the hash table
  int ReadJoinedRow();

  // Have we degraded into on-disk hash join?
  bool on_disk_hash_join() const { return !m_chunk_files_on_disk.empty(); }

  /// Clear the row buffer and reset all iterators pointing to it. This may be
  /// called multiple times to re-init the row buffer.
  ///
  /// @retval true in case of error. my_error has been called
  bool InitRowBuffer();

  enum class State {
    // We are reading a row from the probe input, where the row comes from
    // the iterator.
    READING_ROW_FROM_PROBE_ITERATOR,
    // We are reading a row from the probe input, where the row comes from a
    // chunk file.
    READING_ROW_FROM_PROBE_CHUNK_FILE,
    // The iterator is moving to the next pair of chunk files, where the chunk
    // file from the build input will be loaded into the hash table.
    LOADING_NEXT_CHUNK_PAIR,
    // We are reading the rows returned from the hash table lookup.
    READING_FROM_HASH_TABLE,
    // No more rows, both inputs are empty.
    END_OF_ROWS
  };

  State m_state;

  const unique_ptr_destroy_only<RowIterator> m_build_input;
  const unique_ptr_destroy_only<RowIterator> m_probe_input;

  // An iterator for reading rows from the hash table.
  // hash_join_buffer::HashJoinRowBuffer::Iterator m_hash_map_iterator;
  hash_join_buffer::HashJoinRowBuffer::hash_map_iterator m_hash_map_iterator;
  hash_join_buffer::HashJoinRowBuffer::hash_map_iterator m_hash_map_end;

  // These structures holds the tables and columns that are needed for the hash
  // join. Rows/columns that are not needed are filtered out in the constructor.
  // We need to know which tables that belong to each iterator, so that we can
  // compute the join key when needed.
  hash_join_buffer::TableCollection m_probe_input_table;
  hash_join_buffer::TableCollection m_build_input_tables;

  // An in-memory hash table that holds rows from the build input (directly from
  // the build input iterator, or from a chunk file). See the class comment for
  // details on how and when this is used.
  hash_join_buffer::HashJoinRowBuffer m_row_buffer;

  // A list of the join conditions (all of them are equi-join conditions).
  Prealloced_array<HashJoinCondition, 4> m_join_conditions;

  // Array to hold the list of chunk files on disk in case we degrade into
  // on-disk hash join.
  Mem_root_array<ChunkPair> m_chunk_files_on_disk;

  // Which HashJoinChunk, if any, we are currently reading from, in both
  // LOADING_NEXT_CHUNK_PAIR and READING_ROW_FROM_PROBE_CHUNK_FILE.
  // It is incremented during the state LOADING_NEXT_CHUNK_PAIR.
  int m_current_chunk{-1};

  // The seeds that are used by xxHash64 when calculating the hash from a join
  // key. We need one seed for the hashing done in the in-memory hash table,
  // and one seed when calculating the hash that is used for determining which
  // chunk file a row should be placed in (in case of on-disk hash join). If we
  // were to use the same seed for both operations, we would get a really bad
  // hash table when loading a chunk file to the hash table. The numbers are
  // chosen randomly and have no special meaning.
  static constexpr uint32_t kHashTableSeed{156211};
  static constexpr uint32_t kChunkPartitioningHashSeed{899339};

  // Which row we currently are reading from each of the hash join chunk file.
  ha_rows m_build_chunk_current_row = 0;
  ha_rows m_probe_chunk_current_row = 0;

  // The maximum number of HashJoinChunks that is allocated for each of the
  // inputs in case we spill to disk. We might very well end up with an amount
  // less than this number, but we keep an upper limit so we don't risk running
  // out of file descriptors. We always use a power of two number of files,
  // which allows us to do some optimizations when calculating which chunk a row
  // should be placed in.
  static constexpr size_t kMaxChunks = 128;

  // A buffer that is used during two phases:
  // 1) when constructing a join key from join conditions.
  // 2) when moving a row between tables' record buffers and the hash table.
  //
  // There are two functions that needs this buffer; ConstructJoinKey() and
  // StoreFromTableBuffers(). After calling one of these functions, the user
  // must take responsiblity of the data if it is needed for a longer lifetime.
  //
  // If there are no BLOB/TEXT column in the join, we calculate an upper bound
  // of the row size that is used to preallocate this buffer. In the case of
  // BLOB/TEXT columns, we cannot calculate a reasonable upper bound, and the
  // row size is calculated per row. The allocated memory is kept for the
  // duration of the iterator, so that we (most likely) avoid reallocations.
  String m_temporary_row_and_join_key_buffer;

  // Determines whether to enable performance schema batch mode when reading
  // from the probe input. If set to true, we enable batch mode just before we
  // read the first row from the probe input.
  bool m_enable_batch_mode_for_probe_input{false};

  // Whether we are allowed to spill to disk.
  bool m_allow_spill_to_disk{true};

  // Whether the build iterator has more rows. This is used to stop the hash
  // join iterator asking for more rows when we know for sure that the entire
  // build input is consumed. The variable is only used if m_allow_spill_to_disk
  // is false, as we have to see if there are more rows in the build input after
  // the probe input is consumed.
  bool m_build_iterator_has_more_rows{true};
};

/// For each of the given tables, request that the row ID is filled in
/// (the equivalent of calling file->position()) if needed.
///
/// @param tables The tables to request row IDs for.
void RequestRowId(const Prealloced_array<hash_join_buffer::Table, 4> &tables);

#endif  // SQL_HASH_JOIN_ITERATOR_H_
