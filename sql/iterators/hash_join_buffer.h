#ifndef SQL_ITERATORS_HASH_JOIN_BUFFER_H_
#define SQL_ITERATORS_HASH_JOIN_BUFFER_H_

/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

/// @file
///
/// This file contains the HashJoinRowBuffer class and related
/// functions/classes.
///
/// A HashJoinBuffer is a row buffer that can hold a certain amount of rows.
/// The rows are stored in a hash table, which allows for constant-time lookup.
/// The HashJoinBuffer maintains its own internal MEM_ROOT, where all of the
/// data is allocated.
///
/// The HashJoinBuffer contains an operand with rows from one or more tables,
/// keyed on the value we join on. Consider the following trivial example:
///
///   SELECT t1.data FROM t1 JOIN t2 ON (t1.key = t2.key);
///
/// Let us say that the table "t2" is stored in a HashJoinBuffer. In this case,
/// the hash table key will be the value found in "t2.key", since that is the
/// join condition that belongs to t2. If we have multiple equalities, they
/// will be concatenated together in order to form the hash table key. The hash
/// table key is a std::string_view.
///
/// In order to store a row, we use the function StoreFromTableBuffers. See the
/// comments attached to the function for more details.
///
/// The amount of memory a HashJoinBuffer instance can use is limited by the
/// system variable "join_buffer_size". However, note that we check whether we
/// have exceeded the memory limit _after_ we have inserted data into the row
/// buffer. As such, we will probably use a little bit more memory than
/// specified by join_buffer_size.
///
/// The primary use case for these classes is, as the name implies,
/// for implementing hash join.

#include <stddef.h>
#include <cassert>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "my_alloc.h"
#include "sql/immutable_string.h"
#include "sql/pack_rows.h"
#include "sql_string.h"

class HashJoinCondition;
class THD;

struct StoreLinkedInfo {
  bool m_dont_error{false};  // input
  bool m_full{false};        // output
  size_t m_bytes_needed{0};  // output
};

namespace hash_join_buffer {

/// The key type for the hash structure in HashJoinRowBuffer.
///
/// A key consists of the value from one or more columns, taken from the join
/// condition(s) in the query.  E.g., if the join condition is
/// (t1.col1 = t2.col1 AND t1.col2 = t2.col2), the key is (col1, col2), with the
/// two key parts concatenated together.
///
/// What the data actually contains depends on the comparison context for the
/// join condition. For instance, if the join condition is between a string
/// column and an integer column, the comparison will be done in a string
/// context, and thus the integers will be converted to strings before storing.
/// So the data we store in the key are in some cases converted, so that we can
/// hash and compare them byte-by-byte (i.e. decimals), while other types are
/// already comparable byte-by-byte (i.e. integers), and thus stored as-is.
///
/// Note that the key data can come from items as well as fields if the join
/// condition is an expression. E.g. if the join condition is
/// UPPER(t1.col1) = UPPER(t2.col1), the join key data will come from an Item
/// instead of a Field.
///
/// The Key class never takes ownership of the data. As such, the user must
/// ensure that the data has the proper lifetime. When storing rows in the row
/// buffer, the data must have the same lifetime as the row buffer itself.
/// When using the Key class for lookups in the row buffer, the same lifetime is
/// not needed; the key object is only needed when the lookup is done.
using Key = std::string_view;

// A row in the hash join buffer is the same as the Key class.
using BufferRow = Key;

// A convenience form of LoadIntoTableBuffers() that also verifies the end
// pointer for us.
void LoadBufferRowIntoTableBuffers(const pack_rows::TableCollection &tables,
                                   BufferRow row);

// A convenience form of the above that also decodes the LinkedImmutableString
// for us.
void LoadImmutableStringIntoTableBuffers(
    const pack_rows::TableCollection &tables, LinkedImmutableString row);

enum class StoreRowResult { ROW_STORED, BUFFER_FULL, FATAL_ERROR };

class HashJoinRowBuffer {
 public:
  // Construct the buffer. Note that Init() must be called before the buffer can
  // be used.
  HashJoinRowBuffer(pack_rows::TableCollection tables,
                    std::vector<HashJoinCondition> join_conditions,
                    size_t max_mem_available_bytes);

  ~HashJoinRowBuffer();

  // Initialize the HashJoinRowBuffer so it is ready to store rows. This
  // function can be called multiple times; subsequent calls will only clear the
  // buffer for existing rows.
  bool Init();

  /// Store the row that is currently lying in the tables record buffers.
  /// The hash map key is extracted from the join conditions that the row buffer
  /// holds.
  ///
  /// @param thd the thread handler
  /// @param reject_duplicate_keys If true, reject rows with duplicate keys.
  ///        If a row is rejected, the function will still return ROW_STORED.
  ///
  /// @retval ROW_STORED the row was stored.
  /// @retval BUFFER_FULL the row was stored, and the buffer is full.
  /// @retval FATAL_ERROR an unrecoverable error occurred (most likely,
  ///         malloc failed). It is the caller's responsibility to call
  ///         my_error().
  StoreRowResult StoreRow(THD *thd, bool reject_duplicate_keys);

  size_t size() const;

  bool empty() const { return size() == 0; }

  std::optional<LinkedImmutableString> find(Key key) const;

  std::optional<LinkedImmutableString> first_row() const;

  LinkedImmutableString LastRowStored() const {
    assert(Initialized());
    return m_last_row_stored;
  }

  bool Initialized() const { return m_hash_map != nullptr; }

  bool contains(const Key &key) const { return find(key).has_value(); }

 private:
  // The type of hash map in which the rows are stored.
  class HashMap;

  const std::vector<HashJoinCondition> m_join_conditions;

  // A row can consist of parts from different tables. This structure tells us
  // which tables that are involved.
  const pack_rows::TableCollection m_tables;

  // The MEM_ROOT on which all of the hash table keys and values are allocated.
  // The actual hash map is on the regular heap.
  MEM_ROOT m_mem_root;

  // A MEM_ROOT used only for storing the final row (possibly both key and
  // value). The code assumes fairly deeply that inserting a row never fails, so
  // when m_mem_root goes full (we set a capacity on it to ensure that the last
  // allocated block does not get too big), we allocate the very last row on
  // this MEM_ROOT and the signal fullness so that we can start spilling to
  // disk.
  MEM_ROOT m_overflow_mem_root;

  // The hash table where the rows are stored.
  std::unique_ptr<HashMap> m_hash_map;

  // A buffer we can use when we are constructing a join key from a join
  // condition. In order to avoid reallocating memory, the buffer never shrinks.
  String m_buffer;
  size_t m_row_size_upper_bound;

  // The maximum size of the buffer, given in bytes.
  const size_t m_max_mem_available;

  // The last row that was stored in the hash table, or nullptr if the hash
  // table is empty. We may have to put this row back into the tables' record
  // buffers if we have a child iterator that expects the record buffers to
  // contain the last row returned by the storage engine (the probe phase of
  // hash join may put any row in the hash table in the tables' record buffer).
  // See HashJoinIterator::BuildHashTable() for an example of this.
  LinkedImmutableString m_last_row_stored{nullptr};

  // Fetch the relevant fields from each table, and pack them into m_mem_root
  // as a LinkedImmutableString where the “next” pointer points to “next_ptr”.
  // If that does not work (capacity reached), pack into m_overflow_mem_root
  // instead and set “full” to true. If _that_ does not work (fatally out
  // of memory), returns nullptr. Otherwise, returns a pointer to the newly
  // packed string.
  LinkedImmutableString StoreLinkedImmutableStringFromTableBuffers(
      LinkedImmutableString next_ptr, StoreLinkedInfo *info);
};

}  // namespace hash_join_buffer

/// External interface to the corresponding member in HashJoinRowBuffer
LinkedImmutableString StoreLinkedImmutableStringFromTableBuffers(
    MEM_ROOT *mem_root, MEM_ROOT *overflow_mem_root,
    const pack_rows::TableCollection &tables, LinkedImmutableString next_ptr,
    size_t row_size_upper_bound, StoreLinkedInfo *info);

#endif  // SQL_ITERATORS_HASH_JOIN_BUFFER_H_
