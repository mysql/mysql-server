#ifndef SQL_PACK_ROWS_H_
#define SQL_PACK_ROWS_H_

/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

/**
  @file

  Generic routines for packing rows (possibly from multiple tables
  at the same time) into strings, and then back again. Used for (at least)
  hash join, BKA, and streaming aggregation.
 */

#include <cassert>
#include <cstddef>
#include <cstring>

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
  explicit Table(TABLE *table_arg);
  TABLE *table;
  Prealloced_array<Column, 8> columns;

  // Whether to copy the NULL flags or not.
  bool copy_null_flags{false};
};

/// A structure that contains a list of input tables for a hash join operation,
/// BKA join operation or a streaming aggregation operation, and some
/// pre-computed properties for the tables.
class TableCollection {
 public:
  TableCollection() = default;

  TableCollection(const Prealloced_array<TABLE *, 4> &tables, bool store_rowids,
                  table_map tables_to_get_rowid_for);

  const Prealloced_array<Table, 4> &tables() const { return m_tables; }

  table_map tables_bitmap() const { return m_tables_bitmap; }

  size_t ref_and_null_bytes_size() const { return m_ref_and_null_bytes_size; }

  bool has_blob_column() const { return m_has_blob_column; }

  bool store_rowids() const { return m_store_rowids; }

  table_map tables_to_get_rowid_for() const {
    return m_tables_to_get_rowid_for;
  }

  /// For each of the tables that we should get row IDs for, request that the
  /// row ID is filled in (the equivalent of calling handler::position()) if
  /// needed.
  ///
  /// Since this function is typically called once per row read, the check for
  /// the common case where no row IDs are required, is inlined to reduce the
  /// overhead.
  void RequestRowId() const {
    if (m_tables_to_get_rowid_for != 0) {
      RequestRowIdInner();
    }
  }

  /// For each of the tables that we should get row IDs for, inform the handler
  /// than row IDs will be needed.
  void PrepareForRequestRowId() const;

 private:
  void AddTable(TABLE *tab);
  void RequestRowIdInner() const;

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
///    \c read_set_internal.
/// We do not necessarily have valid data in the table buffers, so we do not try
/// to calculate size for blobs.
size_t ComputeRowSizeUpperBoundSansBlobs(const TableCollection &tables);
/// Similar to ComputeRowSizeUpperBoundSansBlobs, but will calculate blob size
/// as well.  To do this, we need to look at the data stored in the record
/// buffers.
/// \note{This means that the function cannot be called without making sure
/// there is valid data in the table buffers.}
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

inline bool ShouldCopyRowId(const TABLE *table) {
  // It is not safe to copy the row ID if we have a NULL-complemented row; the
  // value is undefined, or the buffer location can even be nullptr.
  return !table->const_table && !(table->is_nullable() && table->null_row);
}

ALWAYS_INLINE uchar *StoreFromTableBuffersRaw(const TableCollection &tables,
                                              uchar *dptr) {
  for (const Table &tbl : tables.tables()) {
    const TABLE *table = tbl.table;

    bool null_row_flag = false;
    if (table->is_nullable()) {
      null_row_flag = table->has_null_row();
      *dptr++ = uchar{null_row_flag};
    }

    // Store the NULL flags.
    if (tbl.copy_null_flags) {
      memcpy(dptr, table->null_flags, table->s->null_bytes);
      dptr += table->s->null_bytes;
    }

    for (const Column &column : tbl.columns) {
      assert(bitmap_is_set(&column.field->table->read_set_internal,
                           column.field->field_index()));
      if (!column.field->is_null()) {
        // Store the data in packed format. The packed format will also
        // include the length of the data if needed.
        dptr = column.field->pack(dptr);
      }
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
