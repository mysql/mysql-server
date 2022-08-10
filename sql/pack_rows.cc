/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "sql/pack_rows.h"

#include <assert.h>
#include <sys/types.h>

#include "mysql_com.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/join_optimizer.h"
#include "sql/sql_executor.h"
#include "sql/sql_optimizer.h"
#include "sql_string.h"

namespace pack_rows {

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
TableCollection::TableCollection(const Prealloced_array<TABLE *, 4> &tables,
                                 bool store_rowids,
                                 table_map tables_to_get_rowid_for)
    : m_tables_bitmap(0),
      m_store_rowids(store_rowids),
      m_tables_to_get_rowid_for(tables_to_get_rowid_for) {
  if (!store_rowids) {
    assert(m_tables_to_get_rowid_for == table_map{0});
  }
  for (TABLE *table : tables) {
    AddTable(table);
    if (table->pos_in_table_list != nullptr) {
      m_tables_bitmap |= table->pos_in_table_list->map();
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
  for (const Column &column : table.columns) {
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
      assert(false);
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
    assert(buffer->alloced_length() >= ComputeRowSizeUpperBound(tables));
  }

  char *dptr = pointer_cast<char *>(
      StoreFromTableBuffersRaw(tables, pointer_cast<uchar *>(buffer->ptr())));
  assert(dptr <= buffer->ptr() + buffer->alloced_length());
  const size_t actual_length = dptr - buffer->ptr();
  buffer->length(actual_length);
  return false;
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

// Request the row ID for all tables where it should be kept.
void RequestRowId(const Prealloced_array<Table, 4> &tables,
                  table_map tables_to_get_rowid_for) {
  for (const Table &it : tables) {
    const TABLE *table = it.table;
    if ((tables_to_get_rowid_for & table->pos_in_table_list->map()) &&
        can_call_position(table)) {
      table->file->position(table->record[0]);
    }
  }
}

void PrepareForRequestRowId(const Prealloced_array<Table, 4> &tables,
                            table_map tables_to_get_rowid_for) {
  for (const Table &it : tables) {
    if (tables_to_get_rowid_for & it.table->pos_in_table_list->map()) {
      it.table->prepare_for_position();
    }
  }
}

}  // namespace pack_rows
