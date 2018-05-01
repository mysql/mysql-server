/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/column.h
TempTable Column declaration. */

#ifndef TEMPTABLE_COLUMN_H
#define TEMPTABLE_COLUMN_H

#include <cstddef> /* size_t */
#include <vector>  /* std::vector */

#include "sql/field.h"                                     /* Field */
#include "storage/temptable/include/temptable/allocator.h" /* temptable::Allocator */

namespace temptable {

/** A column class that describes the metadata of a column. */
class Column {
 public:
  /** Constructor. */
  Column(
      /** [in] MySQL table that contains the column. */
      const TABLE &mysql_table,
      /** [in] MySQL field (column/cell) that describes the columns. */
      const Field &mysql_field);

  /** Check if the cells in this column can be NULL.
   * @return true if cells are allowed to be NULL. */
  bool is_nullable() const;

  /** Check if a particular cell is NULL. The cell is the intersection of this
   * column with the provided row (in MySQL write_row() format).
   * @return true if the cell is NULL */
  bool is_null(
      /** [in] MySQL row that contains the cell to be checked. */
      const unsigned char *mysql_row) const;

  /** In MySQL write_row() format - the offset of the byte (from the row start)
   * that contains the bit that denotes whether a given cell is NULL or not.
   * @return offset of the NULL-byte */
  uint32_t null_byte_offset() const;

  /** In MySQL write_row() format - the bitmask that denotes the bit of this
   * column in the NULL-byte. Only meaningful if `is_nullable()` is true.
   * @return bitmask with exactly one bit set */
  uint8_t null_bitmask() const;

  /** Check if different cells that belong to this column can have different
   * size (eg VARCHAR).
   * @return true if all cells are the same size */
  bool is_fixed_size() const;

  /** In MySQL write_row() format - the length (in bytes) of the portion that
   * denotes the length of a particular cell. For example a VARCHAR(32) column
   * that contains 'abc' will have 1 byte that contains 3. Only meaningful if
   * `is_fixed_size()` is false.
   * @return length of the size portion, 1 from the above example */
  uint8_t length_size() const;

  /** In MysQL write_row() format - the offset, from the start of the row (in
   * bytes), of the actual user data.
   * @return offset in bytes */
  size_t user_data_offset() const;

  /** In MySQL write_row() format - the length of the actual user data of a cell
   * in a given row.
   * @return user data length of the cell that corresponds to this column in the
   * given row */
  uint32_t user_data_length(
      /** [in] MySQL row that contains the cell. */
      const unsigned char *mysql_row) const;

 private:
  /** True if can be NULL. */
  bool m_nullable;

  /** Bitmask to extract is is-NULL bit from the is-NULL byte. */
  uint8_t m_null_bitmask;

  /** The number of bytes that indicate the length of the user data in the
   * cell, for variable sized cells. If this is 0, then the cell is fixed
   * size. */
  uint8_t m_length_bytes_size;

  /** Either the length or the offset of the bytes that indicate the length.
   * For fixed size cells (when `m_length_bytes_size == 0`) this is the length
   * of the user data of a cell.
   * For variable size cells (when `m_length_bytes_size > 0`) this is offset of
   * the bytes that indicate the user data length of a cell. */
  uint32_t m_length;

  /** The offset of the is-NULL byte from the start of the mysql row. If
   * `m_null_bitmask` is set in this byte and `m_nullable` is true, then that
   * particular cell is NULL. */
  uint32_t m_null_byte_offset;

  /** The offset of the user data from the start of the mysql row in bytes. */
  uint32_t m_user_data_offset;
};

/** A type that designates all the columns of a table. */
typedef std::vector<Column, Allocator<Column>> Columns;

/* Implementation of inlined methods. */

inline bool Column::is_nullable() const { return m_nullable; }

inline bool Column::is_null(const unsigned char *mysql_row) const {
  return m_nullable && (m_null_bitmask & *(mysql_row + m_null_byte_offset));
}

inline uint32_t Column::null_byte_offset() const { return m_null_byte_offset; }

inline uint8_t Column::null_bitmask() const { return m_null_bitmask; }

inline bool Column::is_fixed_size() const { return m_length_bytes_size == 0; }

inline uint8_t Column::length_size() const {
  DBUG_ASSERT(m_length_bytes_size > 0);
  return m_length_bytes_size;
}

inline size_t Column::user_data_offset() const { return m_user_data_offset; }

inline uint32_t Column::user_data_length(const unsigned char *mysql_row) const {
  const unsigned char *p = mysql_row + m_length;
  switch (m_length_bytes_size) {
    case 0:
      return m_length;
    case 1:
      return *p;
    case 2:
      return *p | (*(p + 1) << 8);
  }

  abort();
  return 0;
}

} /* namespace temptable */

#endif /* TEMPTABLE_COLUMN_H */
