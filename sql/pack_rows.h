#ifndef SQL_PACK_ROWS_H_
#define SQL_PACK_ROWS_H_

/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
/// join or BKA join, or the input columns in a streaming aggregation operation.
/// When the join or aggregate iterator is constructed, we extract the columns
/// that are needed to satisfy the SQL query.
struct Table {
  explicit Table(TABLE *tab);
  TABLE *table;
  Prealloced_array<Column, 8> columns;

  // Whether to copy the NULL flags or not.
  bool copy_null_flags{false};

  // Whether to store the actual contents of NULL-complemented rows.
  // This is needed by AggregateIterator in order to be able to
  // restore the exact contents of the record buffer for a table
  // accessed with EQRefIterator, so that the cache in EQRefIterator
  // is not disturbed.
  bool store_contents_of_null_rows{false};
};

/// A structure that contains a list of input tables for a hash join operation,
/// BKA join operation or a streaming aggregation operation, and some
/// pre-computed properties for the tables.
class TableCollection {
 public:
  TableCollection() = default;

  TableCollection(const Prealloced_array<TABLE *, 4> &tables, bool store_rowids,
                  table_map tables_to_get_rowid_for,
                  table_map tables_to_store_contents_of_null_rows_for);

  // A single table (typically one for which there is no map bit).
  explicit TableCollection(TABLE *table) {
    AddTable(table, /*store_contents_of_null_rows=*/false);
  }

  const Prealloced_array<Table, 4> &tables() const { return m_tables; }

  table_map tables_bitmap() const { return m_tables_bitmap; }

  size_t ref_and_null_bytes_size() const { return m_ref_and_null_bytes_size; }

  bool has_blob_column() const { return m_has_blob_column; }

  bool store_rowids() const { return m_store_rowids; }

  table_map tables_to_get_rowid_for() const {
    return m_tables_to_get_rowid_for;
  }

 private:
  void AddTable(TABLE *tab, bool store_contents_of_null_rows);

  Prealloced_array<Table, 4> m_tables{PSI_NOT_INSTRUMENTED};

  // We frequently use the bitmap to determine which side of the join an Item
  // belongs to, so precomputing the bitmap saves quite some time.
  table_map m_tables_bitmap = 0;

  // Sum of the NULL bytes and the row ID for all of the tables.
  size_t m_ref_and_null_bytes_size = 0;

  // Whether any of the tables has a BLOB/TEXT column. This is used to determine
  // whether we need to estimate the row size every time we store a row to the
  // row buffer or to a chunk file on disk. If this is set to false, we can
  // pre-allocate any necessary buffers we need during the operation, and thus
  // eliminate the need for recalculating the row size every time.
  bool m_has_blob_column = false;

  bool m_store_rowids = false;
  table_map m_tables_to_get_rowid_for = 0;
};

/// Possible values of the NULL-row flag stored by StoreFromTableBuffers(). It
/// tells whether or not a row is a NULL-complemented row in which all column
/// values (including non-nullable columns) are NULL. Additionally, in case it
/// is a NULL-complemented row, the flag contains information about whether the
/// buffer contains the actual non-NULL values that were available in the record
/// buffer at the time the row was stored, or if no column values are stored for
/// the NULL-complemented row. Usually, no values are stored for
/// NULL-complemented rows, but it may be necessary in order to avoid corrupting
/// the internal cache of EQRefIterator. See Table::store_contents_of_null_rows.
enum class NullRowFlag {
  /// The row is not a NULL-complemented one.
  kNotNull,
  /// The row is NULL-complemented. No column values are stored in the buffer.
  kNullWithoutData,
  /// The row is NULL-complemented. The actual non-NULL values that were in the
  /// record buffer at the time StoreFromTableBuffers() was called, will however
  /// be available in the buffer.
  kNullWithData
};

/// Count up how many bytes a single row from the given tables will occupy,
/// in "packed" format. Note that this is an upper bound, so the length after
/// calling Field::pack may very well be shorter than the size returned by this
/// function.
///
/// The value returned from this function will sum up
/// 1) The row-id if that is to be kept.
/// 2) Size of the NULL flags. This includes:
///     - Space for a NULL flag per nullable column.
///     - Space for a NULL flag per nullable table (tables on the inner side of
///     an outer join).
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
/// The output buffer will contain the following data for each table in
/// "tables":
///
/// 1) NULL-row flag if the table is nullable.
/// 2) NULL flags for each nullable column.
/// 3) The actual data from the columns.
/// 4) The row ID for each row. This is only stored if the optimizer requested
/// row IDs when creating the TableCollection.
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
/// @param tables All tables involved in the operation.
/// @param tables_to_get_rowid_for A bitmap of which tables to actually
///     get row IDs for. (A table needs to be in both sets to be processed.)
void RequestRowId(const Prealloced_array<pack_rows::Table, 4> &tables,
                  table_map tables_to_get_rowid_for);

void PrepareForRequestRowId(const Prealloced_array<pack_rows::Table, 4> &tables,
                            table_map tables_to_get_rowid_for);

inline bool ShouldCopyRowId(const TABLE *table) {
  // It is not safe to copy the row ID if we have a NULL-complemented row; the
  // value is undefined, or the buffer location can even be nullptr.
  return !table->const_table && !(table->is_nullable() && table->null_row);
}

ALWAYS_INLINE uchar *StoreFromTableBuffersRaw(const TableCollection &tables,
                                              uchar *dptr) {
  for (const Table &tbl : tables.tables()) {
    const TABLE *table = tbl.table;

    NullRowFlag null_row_flag = NullRowFlag::kNotNull;
    if (table->is_nullable()) {
      if (table->has_null_row()) {
        null_row_flag = tbl.store_contents_of_null_rows && table->has_row()
                            ? NullRowFlag::kNullWithData
                            : NullRowFlag::kNullWithoutData;
      }
      *dptr++ = static_cast<uchar>(null_row_flag);
      if (null_row_flag == NullRowFlag::kNullWithData) {
        assert(table->is_started());
        // If we want to store the actual values in the table buffer for the
        // NULL-complemented row, instead of the NULLs, we need to restore the
        // original null flags first. We reset the flags after we have stored
        // the column values.
        tbl.table->restore_null_flags();
        tbl.table->reset_null_row();
      }
    }

    // Store the NULL flags.
    if (tbl.copy_null_flags) {
      memcpy(dptr, table->null_flags, table->s->null_bytes);
      dptr += table->s->null_bytes;
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

    if (null_row_flag == NullRowFlag::kNullWithData) {
      // The null flags were changed in order to get the actual contents of the
      // null row stored. Restore the original null flags.
      tbl.table->set_null_row();
    }

    if (tables.store_rowids() && ShouldCopyRowId(table)) {
      // Store the row ID, since it is needed by weedout.
      memcpy(dptr, table->file->ref, table->file->ref_length);
      dptr += table->file->ref_length;
    }
  }
  return dptr;
}

}  // namespace pack_rows

#endif  // SQL_PACK_ROWS_H_
