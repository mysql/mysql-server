/* Copyright (c) 2018, 2020, Oracle and/or its affiliates.

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

#include "sql/hash_join_buffer.h"

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
#include "my_dbug.h"
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

namespace hash_join_buffer {

Column::Column(Field *field) : field(field), field_type(field->real_type()) {}

// Take in a QEP_TAB and extract the columns that are needed to satisfy the SQL
// query (determined by the read set of the table).
Table::Table(TABLE *table) : table(table), columns(PSI_NOT_INSTRUMENTED) {
  for (uint i = 0; i < table->s->fields; ++i) {
    if (bitmap_is_set(table->read_set, i)) {
      columns.emplace_back(table->field[i]);
    }
  }
}

// Take a set of tables involed in a hash join and extract the columns that are
// needed to satisfy the SQL query. Note that we might very well include a table
// with no columns, like t2 in the following query:
//
//   SELECT t1.col1 FROM t1, t2;  # t2 will be included without any columns.
TableCollection::TableCollection(const JOIN *join, table_map tables,
                                 bool store_rowids,
                                 table_map tables_to_get_rowid_for)
    : m_tables_bitmap(tables),
      m_store_rowids(store_rowids),
      m_tables_to_get_rowid_for(tables_to_get_rowid_for) {
  if (!store_rowids) {
    assert(m_tables_to_get_rowid_for == table_map{0});
  }
  for (uint table_idx = 0; table_idx < join->tables; ++table_idx) {
    TABLE *table = join->qep_tab[table_idx].table();
    if (table == nullptr || table->pos_in_table_list == nullptr) {
      continue;
    }
    if (Overlaps(tables, table->pos_in_table_list->map())) {
      AddTable(table);
    }
  }
}

void TableCollection::AddTable(TABLE *tab) {
  // When constructing the iterator tree, we might end up adding a
  // WeedoutIterator _after_ a HashJoinIterator has been constructed.
  // When adding the WeedoutIterator, QEP_TAB::rowid_status will be changed
  // indicate that a row ID is needed. A side effect of this is that
  // rowid_status might say that no row ID is needed here, while it says
  // otherwise while hash join is executing. As such, we may write outside of
  // the allocated buffers since we did not take the size of the row ID into
  // account here. To overcome this, we always assume that the row ID should
  // be kept; reserving some extra bytes in a few buffers should not be an
  // issue.
  m_ref_and_null_bytes_size += tab->file->ref_length;

  if (tab->is_nullable()) {
    m_ref_and_null_bytes_size += sizeof(tab->null_row);
  }

  Table table(tab);
  for (const hash_join_buffer::Column &column : table.columns) {
    // Field_typed_array will mask away the BLOB_FLAG for all types. Hence,
    // we will treat all Field_typed_array as blob columns.
    if (column.field->is_flag_set(BLOB_FLAG) || column.field->is_array()) {
      m_has_blob_column = true;
    }

    // If a column is marked as nullable, we need to copy the NULL flags.
    if (!column.field->is_flag_set(NOT_NULL_FLAG)) {
      table.copy_null_flags = true;
    }

    // BIT fields stores some of its data in the NULL flags of the table. So
    // if we have a BIT field, we must copy the NULL flags.
    if (column.field->type() == MYSQL_TYPE_BIT &&
        down_cast<const Field_bit *>(column.field)->bit_len > 0) {
      table.copy_null_flags = true;
    }
  }

  if (table.copy_null_flags) {
    m_ref_and_null_bytes_size += tab->s->null_bytes;
  }

  m_tables.push_back(table);
}

// Calculate how many bytes the data in the column uses. We don't bother
// calculating the exact size for all types, since we consider reserving some
// extra bytes in buffers harmless. In particular, as long as the column is not
// of type BLOB, TEXT, JSON or GEOMETRY, we return an upper bound of the storage
// size. In the case of said types, we return the actual storage size; we do not
// want to return 4 gigabytes for a BLOB column if it only contains 10 bytes of
// data.
static size_t CalculateColumnStorageSize(const Column &column) {
  bool is_blob_column = false;
  switch (column.field_type) {
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_BOOL:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_NULL:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_TIMESTAMP2:
    case MYSQL_TYPE_DATETIME2:
    case MYSQL_TYPE_TIME2:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_STRING:
      // Field_typed_array inherits from Field_blob, so we have to treat it as a
      // BLOB column. And is_array() the only way to detect if the field is a
      // typed array.
      is_blob_column = column.field->is_array();
      break;
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_JSON:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB: {
      is_blob_column = true;
      break;
    }
    case MYSQL_TYPE_INVALID:      // Should not occur
    case MYSQL_TYPE_TYPED_ARRAY:  // Type only used for replication
    {
      DBUG_ASSERT(false);
      return 0;
    }
  }

  if (is_blob_column) {
    // If we have a BLOB type, look at the actual length instead of taking the
    // upper length, which could happen to be 4GB. Note that data_length()
    // does not include the size of the length variable for blob types, so we
    // have to add that ourselves.
    const Field_blob *field_blob = down_cast<const Field_blob *>(column.field);
    return field_blob->data_length() + field_blob->pack_length_no_ptr();
  }

  return column.field->max_data_length();
}

size_t ComputeRowSizeUpperBound(const TableCollection &tables) {
  size_t total_size = tables.ref_and_null_bytes_size();
  for (const Table &table : tables.tables()) {
    for (const Column &column : table.columns) {
      // Even though we only store non-null columns, we count up the size of all
      // columns unconditionally. This means that NULL columns may very well be
      // counted here, but the only effect is that we end up reserving a bit too
      // much space in the buffer for holding the row data. That is more welcome
      // than having to call Field::is_null() for every column in every row.
      total_size += CalculateColumnStorageSize(column);
    }
  }

  return total_size;
}

static bool ShouldCopyRowId(const TABLE *table) {
  // It is not safe to copy the row ID if we have a NULL-complemented row; the
  // value is undefined, or the buffer location can even be nullptr.
  return !table->const_table && !(table->is_nullable() && table->null_row);
}

static ALWAYS_INLINE uchar *StoreFromTableBuffersRaw(
    const TableCollection &tables, uchar *dptr) {
  for (const Table &tbl : tables.tables()) {
    const TABLE *table = tbl.table;

    // Store the NULL flags.
    if (tbl.copy_null_flags) {
      memcpy(dptr, table->null_flags, table->s->null_bytes);
      dptr += table->s->null_bytes;
    }

    if (tbl.table->is_nullable()) {
      const size_t null_row_size = sizeof(tbl.table->null_row);
      memcpy(dptr, pointer_cast<const uchar *>(&tbl.table->null_row),
             null_row_size);
      dptr += null_row_size;
    }

    if (tables.store_rowids() && ShouldCopyRowId(tbl.table)) {
      // Store the row ID, since it is needed by weedout.
      memcpy(dptr, table->file->ref, table->file->ref_length);
      dptr += table->file->ref_length;
    }

    for (const Column &column : tbl.columns) {
      DBUG_ASSERT(bitmap_is_set(column.field->table->read_set,
                                column.field->field_index()));
      if (!column.field->is_null()) {
        // Store the data in packed format. The packed format will also
        // include the length of the data if needed.
        dptr = column.field->pack(dptr);
      }
    }
  }
  return dptr;
}

bool StoreFromTableBuffers(const TableCollection &tables, String *buffer) {
  buffer->length(0);

  if (tables.has_blob_column()) {
    const size_t upper_data_length = ComputeRowSizeUpperBound(tables);
    if (buffer->alloced_length() < upper_data_length + buffer->length() &&
        buffer->reserve(upper_data_length)) {
      return true;
    }
  } else {
    // If the table doesn't have any blob columns, we expect that the caller
    // already has reserved enough space in the provided buffer.
    DBUG_ASSERT(buffer->alloced_length() >= ComputeRowSizeUpperBound(tables));
  }

  char *dptr = pointer_cast<char *>(
      StoreFromTableBuffersRaw(tables, pointer_cast<uchar *>(buffer->ptr())));
  DBUG_ASSERT(dptr <= buffer->ptr() + buffer->alloced_length());
  const size_t actual_length = dptr - buffer->ptr();
  buffer->length(actual_length);
  return false;
}

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

// Take the contents of this row and put it back in the tables' record buffers
// (record[0]). The row ID and NULL flags will also be restored, if needed.
// Returns a pointer to where we ended reading.
const uchar *LoadIntoTableBuffers(const TableCollection &tables,
                                  const uchar *ptr) {
  for (const Table &tbl : tables.tables()) {
    TABLE *table = tbl.table;

    // If the NULL row flag is set, it may override the NULL flags for the
    // columns. This may in turn cause columns not to be restored when they
    // should, so clear the NULL row flag when restoring the row.
    table->reset_null_row();

    if (tbl.copy_null_flags) {
      memcpy(table->null_flags, ptr, table->s->null_bytes);
      ptr += table->s->null_bytes;
    }

    if (tbl.table->is_nullable()) {
      const size_t null_row_size = sizeof(tbl.table->null_row);
      memcpy(pointer_cast<uchar *>(&tbl.table->null_row), ptr, null_row_size);
      ptr += null_row_size;
    }

    if (tables.store_rowids() && ShouldCopyRowId(tbl.table)) {
      memcpy(table->file->ref, ptr, table->file->ref_length);
      ptr += table->file->ref_length;
    }

    for (const Column &column : tbl.columns) {
      if (!column.field->is_null()) {
        ptr = column.field->unpack(ptr);
      }
    }
  }
  return ptr;
}

// A convenience form of the above that also verifies the end pointer for us.
void LoadIntoTableBuffers(const TableCollection &tables, BufferRow row) {
  const uchar *end MY_ATTRIBUTE((unused)) =
      LoadIntoTableBuffers(tables, row.data());
  DBUG_ASSERT(end == row.data() + row.size());
}

void LoadIntoTableBuffers(const TableCollection &tables,
                          LinkedImmutableString row) {
  LoadIntoTableBuffers(tables, pointer_cast<const uchar *>(row.Decode().data));
}

HashJoinRowBuffer::HashJoinRowBuffer(
    TableCollection tables, std::vector<HashJoinCondition> join_conditions,
    size_t max_mem_available)
    : m_join_conditions(move(join_conditions)),
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

StoreRowResult HashJoinRowBuffer::StoreRow(
    THD *thd, bool reject_duplicate_keys,
    bool store_rows_with_null_in_condition) {
  bool full = false;

  // Make the key from the join conditions.
  m_buffer.length(0);
  for (const HashJoinCondition &hash_join_condition : m_join_conditions) {
    bool null_in_join_condition =
        hash_join_condition.join_condition()->append_join_key_for_hash_join(
            thd, m_tables.tables_bitmap(), hash_join_condition, &m_buffer);

    if (thd->is_error()) {
      // An error was raised while evaluating the join condition.
      return StoreRowResult::FATAL_ERROR;
    }

    if (null_in_join_condition && !store_rows_with_null_in_condition) {
      // SQL NULL values will never match in an inner join or semijoin, so skip
      // the row.
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
    uint64_t byte = static_cast<uint8_t>(p[i]);
    res += (byte - 1) << (7 * i);
    if (likely(byte < 128)) {
      return {p + i + 1, res};
    }
  }
  return {nullptr, 0};
}
