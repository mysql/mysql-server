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

#include "sql/iterators/hash_join_buffer.h"

#include <assert.h>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <new>
#include <unordered_map>

#include "field_types.h"
#include "m_ctype.h"
#include "my_alloc.h"
#include "my_bit.h"
#include "my_bitmap.h"
#include "my_compiler.h"

#include "my_inttypes.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item_cmpfunc.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/psi_memory_key.h"
#include "sql/sql_class.h"
#include "sql/sql_executor.h"
#include "sql/sql_join_buffer.h"
#include "sql/sql_optimizer.h"
#include "sql/table.h"
#include "tables_contained_in.h"
#include "template_utils.h"

using pack_rows::TableCollection;

namespace hash_join_buffer {

LinkedImmutableString
HashJoinRowBuffer::StoreLinkedImmutableStringFromTableBuffers(
    LinkedImmutableString next_ptr, bool *full) {
  size_t row_size_upper_bound = m_row_size_upper_bound;
  if (m_tables.has_blob_column()) {
    // The row size upper bound can have changed.
    row_size_upper_bound = ComputeRowSizeUpperBound(m_tables);
  }

  const size_t required_value_bytes =
      LinkedImmutableString::RequiredBytesForEncode(row_size_upper_bound);

  std::pair<char *, char *> block = m_mem_root.Peek();
  if (static_cast<size_t>(block.second - block.first) < required_value_bytes) {
    // No room in this block; ask for a new one and try again.
    m_mem_root.ForceNewBlock(required_value_bytes);
    block = m_mem_root.Peek();
  }
  bool committed = false;
  char *start_of_value, *dptr;
  LinkedImmutableString ret{nullptr};
  if (static_cast<size_t>(block.second - block.first) >= required_value_bytes) {
    dptr = start_of_value = block.first;
  } else {
    dptr = start_of_value =
        pointer_cast<char *>(m_overflow_mem_root.Alloc(required_value_bytes));
    if (dptr == nullptr) {
      return LinkedImmutableString{nullptr};
    }
    committed = true;
    *full = true;
  }

  ret = LinkedImmutableString::EncodeHeader(next_ptr, &dptr);
  dptr = pointer_cast<char *>(
      StoreFromTableBuffersRaw(m_tables, pointer_cast<uchar *>(dptr)));

  if (!committed) {
    const size_t actual_length = dptr - pointer_cast<char *>(start_of_value);
    m_mem_root.RawCommit(actual_length);
  }
  return ret;
}

// A convenience form of LoadIntoTableBuffers() that also verifies the end
// pointer for us.
void LoadBufferRowIntoTableBuffers(const TableCollection &tables,
                                   BufferRow row) {
  const uchar *data = pointer_cast<const uchar *>(row.data());
  const uchar *end [[maybe_unused]] = LoadIntoTableBuffers(tables, data);
  assert(end == data + row.size());
}

void LoadImmutableStringIntoTableBuffers(const TableCollection &tables,
                                         LinkedImmutableString row) {
  LoadIntoTableBuffers(tables, pointer_cast<const uchar *>(row.Decode().data));
}

HashJoinRowBuffer::HashJoinRowBuffer(
    TableCollection tables, std::vector<HashJoinCondition> join_conditions,
    size_t max_mem_available)
    : m_join_conditions(std::move(join_conditions)),
      m_tables(std::move(tables)),
      m_mem_root(key_memory_hash_join, 16384 /* 16 kB */),
      m_overflow_mem_root(key_memory_hash_join, 256),
      m_hash_map(nullptr),
      m_max_mem_available(
          std::max<size_t>(max_mem_available, 16384 /* 16 kB */)) {
  // Limit is being applied only after the first row.
  m_mem_root.set_max_capacity(0);
}

bool HashJoinRowBuffer::Init() {
  if (m_hash_map.get() != nullptr) {
    // Reset the unique_ptr, so that the hash map destructors are called before
    // clearing the MEM_ROOT.
    m_hash_map.reset(nullptr);
    m_mem_root.Clear();
    // Limit is being applied only after the first row.
    m_mem_root.set_max_capacity(0);
    m_overflow_mem_root.ClearForReuse();

    // Now that the destructors are finished and the MEM_ROOT is cleared,
    // we can allocate a new hash map.
  }

  // NOTE: Will be ignored and re-calculated if there are any blobs in the
  // table.
  m_row_size_upper_bound = ComputeRowSizeUpperBound(m_tables);

  m_hash_map.reset(new hash_map_type(
      /*bucket_count=*/10, KeyHasher()));
  if (m_hash_map == nullptr) {
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), sizeof(hash_map_type));
    return true;
  }

  m_last_row_stored = LinkedImmutableString{nullptr};
  return false;
}

StoreRowResult HashJoinRowBuffer::StoreRow(THD *thd,
                                           bool reject_duplicate_keys) {
  bool full = false;

  // Make the key from the join conditions.
  m_buffer.length(0);
  for (const HashJoinCondition &hash_join_condition : m_join_conditions) {
    bool null_in_join_condition =
        hash_join_condition.join_condition()->append_join_key_for_hash_join(
            thd, m_tables.tables_bitmap(), hash_join_condition,
            m_join_conditions.size() > 1, &m_buffer);

    if (thd->is_error()) {
      // An error was raised while evaluating the join condition.
      return StoreRowResult::FATAL_ERROR;
    }

    if (null_in_join_condition) {
      // One of the components of the join key had a NULL value, and
      // that component was part of an equality predicate (=), *not* a
      // NULL-safe equality predicate, so it can never match a row in
      // the other table. There's no need to store the row in the hash
      // table. Skip it.
      return StoreRowResult::ROW_STORED;
    }
  }

  // Store the key in the MEM_ROOT. Note that we will only commit the memory
  // usage for it if the key was a new one (see the call to emplace() below)..
  const size_t required_key_bytes =
      ImmutableStringWithLength::RequiredBytesForEncode(m_buffer.length());
  ImmutableStringWithLength key;

  std::pair<char *, char *> block = m_mem_root.Peek();
  if (static_cast<size_t>(block.second - block.first) < required_key_bytes) {
    // No room in this block; ask for a new one and try again.
    m_mem_root.ForceNewBlock(required_key_bytes);
    block = m_mem_root.Peek();
  }
  size_t bytes_to_commit = 0;
  if (static_cast<size_t>(block.second - block.first) >= required_key_bytes) {
    char *ptr = block.first;
    key = ImmutableStringWithLength::Encode(m_buffer.ptr(), m_buffer.length(),
                                            &ptr);
    assert(ptr < block.second);
    bytes_to_commit = ptr - block.first;
  } else {
    char *ptr =
        pointer_cast<char *>(m_overflow_mem_root.Alloc(required_key_bytes));
    if (ptr == nullptr) {
      return StoreRowResult::FATAL_ERROR;
    }
    key = ImmutableStringWithLength::Encode(m_buffer.ptr(), m_buffer.length(),
                                            &ptr);
    // Keep bytes_to_commit == 0; the value is already committed.
  }

  std::pair<hash_map_type::iterator, bool> key_it_and_inserted;
  try {
    key_it_and_inserted =
        m_hash_map->emplace(key, LinkedImmutableString{nullptr});
  } catch (const std::overflow_error &) {
    // This can only happen if the hash function is extremely bad
    // (should never happen in practice).
    return StoreRowResult::FATAL_ERROR;
  }
  LinkedImmutableString next_ptr{nullptr};
  if (key_it_and_inserted.second) {
    // We inserted an element, so the hash table may have grown.
    // Update the capacity available for the MEM_ROOT; our total may
    // have gone slightly over already, and if so, we will signal
    // that and immediately start spilling to disk.
    size_t bytes_used = m_hash_map->calcNumBytesTotal(m_hash_map->mask() + 1);
    if (bytes_used >= m_max_mem_available) {
      // 0 means no limit, so set the minimum possible limit.
      m_mem_root.set_max_capacity(1);
      full = true;
    } else {
      m_mem_root.set_max_capacity(m_max_mem_available - bytes_used);
    }

    // We need to keep this key.
    m_mem_root.RawCommit(bytes_to_commit);
  } else {
    if (reject_duplicate_keys) {
      return StoreRowResult::ROW_STORED;
    }
    // We already have another element with the same key, so our insert
    // failed, Put the new value in the hash bucket, but keep track of
    // what the old one was; it will be our “next” pointer.
    next_ptr = key_it_and_inserted.first->second;
  }

  // Save the contents of all columns marked for reading.
  m_last_row_stored = key_it_and_inserted.first->second =
      StoreLinkedImmutableStringFromTableBuffers(next_ptr, &full);
  if (m_last_row_stored == nullptr) {
    return StoreRowResult::FATAL_ERROR;
  } else if (full) {
    return StoreRowResult::BUFFER_FULL;
  } else {
    return StoreRowResult::ROW_STORED;
  }
}

}  // namespace hash_join_buffer

// From protobuf.
std::pair<const char *, uint64_t> VarintParseSlow64(const char *p,
                                                    uint32_t res32) {
  uint64_t res = res32;
  for (std::uint32_t i = 2; i < 10; i++) {
    uint64_t x = static_cast<uint8_t>(p[i]);
    res += (x - 1) << (7 * i);
    if (likely(x < 128)) {
      return {p + i + 1, res};
    }
  }
  return {nullptr, 0};
}
