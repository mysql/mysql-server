#ifndef SQL_PACK_ROWS_H_
#define SQL_PACK_ROWS_H_

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

/**
  @file

  Generic routines for packing rows (possibly from multiple tables
  at the same time) into strings, and then back again. Used for (at least)
  hash join, BKA, and streaming aggregation.
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "field_types.h"
#include "my_bitmap.h"
#include "my_compiler.h"

#include "my_inttypes.h"
#include "my_table_map.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "prealloced_array.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/table.h"
#include "template_utils.h"

class JOIN;
class String;

// Names such as “Column” and “Table” are a tad too generic for the global
// namespace.
namespace pack_rows {

/// A class that represents a field, which also holds a cached value of the
/// field's data type.
struct Column {
  explicit Column(Field *field);
  Field *const field;

  // The field type is used frequently, and caching it gains around 30% in some
  // of our microbenchmarks.
  const enum_field_types field_type;
};

/// This struct is primarily used for holding the extracted columns in a hash
/// join. When the hash join iterator is constructed, we extract the columns
/// that are needed to satisfy the SQL query.
struct Table {
  explicit Table(TABLE *tab);
  TABLE *table;
  Prealloced_array<Column, 8> columns;

  // Whether to copy the NULL flags or not.
  bool copy_null_flags{false};
};

/// A structure that contains a list of tables for the hash join operation,
/// and some pre-computed properties for the tables.
class TableCollection {
 public:
  TableCollection() = default;

  TableCollection(const Prealloced_array<TABLE *, 4> &tables, bool store_rowids,
                  table_map tables_to_get_rowid_for);

  // A single table (typically one for which there is no map bit).
  explicit TableCollection(TABLE *table) { AddTable(table); }

  const Prealloced_array<Table, 4> &tables() const { return m_tables; }

  table_map tables_bitmap() const { return m_tables_bitmap; }

  size_t ref_and_null_bytes_size() const { return m_ref_and_null_bytes_size; }

  bool has_blob_column() const { return m_has_blob_column; }

  bool store_rowids() const { return m_store_rowids; }

  table_map tables_to_get_rowid_for() const {
    return m_tables_to_get_rowid_for;
  }

 private:
  void AddTable(TABLE *tab);

  Prealloced_array<Table, 4> m_tables{PSI_NOT_INSTRUMENTED};

  // We frequently use the bitmap to determine which side of the join an Item
  // belongs to, so precomputing the bitmap saves quite some time.
  table_map m_tables_bitmap = 0;

  // Sum of the NULL bytes and the row ID for all of the tables.
  size_t m_ref_and_null_bytes_size = 0;

  // Whether any of the tables has a BLOB/TEXT column. This is used to determine
  // whether we need to estimate the row size every time we store a row to the
  // row buffer or to a chunk file on disk. If this is set to false, we can
  // pre-allocate any necessary buffers we need during the hash join, and thus
  // eliminate the need for recalculating the row size every time.
  bool m_has_blob_column = false;

  bool m_store_rowids = false;
  table_map m_tables_to_get_rowid_for = 0;
};

/// Count up how many bytes a single row from the given tables will occupy,
/// in "packed" format. Note that this is an upper bound, so the length after
/// calling Field::pack may very well be shorter than the size returned by this
/// function.
///
/// The value returned from this function will sum up
/// 1) The row-id if that is to be kept.
/// 2) Size of the NULL flags.
/// 3) Size of the buffer returned by pack() on all columns marked in the
///    read_set.
///
/// Note that if any of the tables has a BLOB/TEXT column, this function looks
/// at the data stored in the record buffers. This means that the function can
/// not be called before reading any rows if tables.has_blob_column is true.
size_t ComputeRowSizeUpperBound(const TableCollection &tables);

/// Take the data marked for reading in "tables" and store it in the provided
/// buffer. What data to store is determined by the read set of each table.
/// Note that any existing data in "buffer" will be overwritten.
///
/// The output buffer will contain three things:
///
/// 1) NULL flags for each nullable column.
/// 2) The row ID for each row. This is only stored if QEP_TAB::rowid_status !=
///    NO_ROWID_NEEDED.
/// 3) The actual data from the columns.
///
/// @retval true if error, false otherwise
bool StoreFromTableBuffers(const TableCollection &tables, String *buffer);

/// Take the data in "ptr" and put it back to the tables' record buffers.
/// The tables must be _exactly_ the same as when the row was created.
/// That is, it must contain the same tables in the same order, and the read set
/// of each table must be identical when storing and restoring the row.
/// If that's not the case, you will end up with undefined and unpredictable
/// behavior.
///
/// Returns a pointer to where we ended reading.
const uchar *LoadIntoTableBuffers(const TableCollection &tables,
                                  const uchar *ptr);

/// For each of the given tables, request that the row ID is filled in
/// (the equivalent of calling file->position()) if needed.
///
/// @param tables All tables involved in the hash join.
/// @param tables_to_get_rowid_for A bitmap of which tables to actually
///     get row IDs for. (A table needs to be in both sets to be processed.)
void RequestRowId(const Prealloced_array<pack_rows::Table, 4> &tables,
                  table_map tables_to_get_rowid_for);

void PrepareForRequestRowId(const Prealloced_array<pack_rows::Table, 4> &tables,
                            table_map tables_to_get_rowid_for);

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
      assert(bitmap_is_set(column.field->table->read_set,
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

}  // namespace pack_rows

#endif  // SQL_HASH_JOIN_BUFFER_H_
