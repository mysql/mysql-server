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

#include <cstddef>
#include <vector>

#include "my_dbug.h"
#include "sql/field.h"
#include "storage/temptable/include/temptable/allocator.h"
#include "storage/temptable/include/temptable/misc.h"

namespace temptable {

/** A column class that describes the metadata of a column. */
class Column {
 public:
  /** Constructor. */
  Column(
      /** [in] A pointer to the row (user data). */
      const unsigned char *mysql_row,
      /** [in] MySQL table that contains the column. */
      const TABLE &mysql_table TEMPTABLE_UNUSED_NODBUG,
      /** [in] MySQL field (column/cell) that describes the columns. */
      const Field &mysql_field);

  /** Check if a particular cell is NULL. The cell is the intersection of this
   * column with the provided row (in MySQL write_row() format).
   * @return true if the cell is NULL */
  bool read_is_null(
      /** [in] MySQL row that contains the cell to be checked. */
      const unsigned char *mysql_row) const;

  /** Write the information that cell is NULL or not. */
  void write_is_null(
      /** [in] True if cell is NULL, false if it has value. */
      bool is_null,
      /** [out] MySQL row buffer to write data into. */
      unsigned char *mysql_row,
      /** [in] Length of the row buffer. */
      size_t mysql_row_length) const;

  /** In MySQL write_row() format - the length of the actual user data of a cell
   * in a given row.
   * @return user data length of the cell that corresponds to this column in the
   * given row */
  uint32_t read_user_data_length(
      /** [in] MySQL row buffer to read data from. */
      const unsigned char *mysql_row) const;

  /** Write the length of user data stored in a cell. */
  void write_user_data_length(
      /** [in] User data length. */
      uint32_t data_length,
      /** [out] MySQL row buffer to write data into. */
      unsigned char *mysql_row,
      /** [in] Length of the row buffer. */
      size_t mysql_row_length) const;

  /** Return pointer to user data in MySQL row.
   * @return Pointer to user data. */
  const unsigned char *get_user_data_ptr(const unsigned char *mysql_row) const;

  /** Reads user data stored in a cell.
   * Cannot be used for columns stored as BLOBs.
   * Performs a deep copy of the data. */
  void read_user_data(
      /** [out] Pointer to store user data. */
      unsigned char *data,
      /** [out] Length of the data to read. */
      uint32_t data_length,
      /** [in] MySQL row buffer to read data from. */
      const unsigned char *mysql_row,
      /** [in] Length of the row buffer. */
      size_t mysql_row_length) const;

  /** Write user data stored in a cell.
   * Cannot be used for columns stored as blobs. */
  void write_user_data(
      /** [in] True if cell is NULL, false if it has value. */
      bool is_null,
      /** [in] Pointer to used data. */
      const unsigned char *data,
      /** [in] Length of user data. */
      uint32_t data_length,
      /** [out] MySQL row buffer to write data into. */
      unsigned char *mysql_row,
      /** [in] Length of the row buffer. */
      size_t mysql_row_length) const;

 private:
  /** Check if the cells in this column can be NULL.
   * @return true if cells are allowed to be NULL. */
  bool is_nullable() const;

  /** Check this column stores blobs.
   * @return true if it is a blob column. */
  bool is_blob() const;

  /** Check if different cells that belong to this column can have different
   * size (eg VARCHAR).
   * @return true if all cells are the same size */
  bool is_fixed_size() const;

  /** Reads user data stored in a cell.
   * Cannot be used for columns stored as BLOBs.
   * Performs a deep copy of the data. */
  void read_std_user_data(
      /** [out] Pointer to store user data. */
      unsigned char *data,
      /** [out] Length of the data to read. */
      uint32_t data_length,
      /** [in] MySQL row buffer to read data from. */
      const unsigned char *mysql_row,
      /** [in] Length of the row buffer. */
      size_t mysql_row_length TEMPTABLE_UNUSED_NODBUG) const;

  /** Write user data stored in a cell.
   * Cannot be used for columns stored as blobs. */
  void write_std_user_data(
      /** [in] Pointer to used data. */
      const unsigned char *data,
      /** [in] Length of user data. */
      uint32_t data_length,
      /** [out] MySQL row buffer to write data into. */
      unsigned char *mysql_row,
      /** [in] Length of the row buffer. */
      size_t mysql_row_length TEMPTABLE_UNUSED_NODBUG) const;

  /** Reads user data stored in a cell for columns stored as BLOBs.
   * Performs a deep copy of the data. */
  void read_blob_user_data(
      /** [out] Pointer to store user data. */
      unsigned char *data,
      /** [out] Length of the data to read. */
      uint32_t data_length,
      /** [in] MySQL row buffer to read data from. */
      const unsigned char *mysql_row,
      /** [in] Length of the row buffer. */
      size_t mysql_row_length TEMPTABLE_UNUSED_NODBUG) const;

  /** Write user data stored in a cell for columns stored as BLOBs.
   * NOTE: Currently only pointer is stored, no data is copied and the
   * length is ignored. */
  void write_blob_user_data(
      /** [in] Pointer to user data. Can be NULL for cells with NULL value. */
      const unsigned char *data,
      /** [in] Length of user data. */
      uint32_t data_length,
      /** [out] MySQL row buffer to write data into. */
      unsigned char *mysql_row,
      /** [in] Length of the row buffer. */
      size_t mysql_row_length TEMPTABLE_UNUSED_NODBUG) const;

  /** Calculate pointer to user data in a MySQL row.
   * Cannot be used for columns stored as BLOBs.
   * @return Pointer to user data stored in a cell. */
  const unsigned char *calculate_user_data_ptr(
      /** [in] MySQL row buffer that stores the user data. */
      const unsigned char *mysql_row) const;

  /** Reads pointer to user data for a column stored as BLOB.
   * @return Pointer to user data stored in a BLOB field. */
  const unsigned char *read_blob_data_ptr(
      /** [in] MySQL row buffer that stores BLOB data pointer. */
      const unsigned char *mysql_row) const;

  /** True if can be NULL. */
  bool m_nullable;

  /** True if it is a blob. */
  bool m_is_blob;

  /** Bitmask to extract is is-NULL bit from the is-NULL byte. */
  uint8_t m_null_bitmask;

  /** The number of bytes that indicate the length of the user data in the
   * cell, for variable sized cells. If this is 0, then the cell is fixed
   * size. */
  uint8_t m_length_bytes_size;

  union {
    /** Length of the user data of a cell.
     * It is for fixed size cells (when `m_length_bytes_size == 0`). */
    uint32_t m_length;

    /** Offset of the bytes that indicate the user data length of a cell.
     * It is used for variable size cells (when `m_length_bytes_size > 0`). */
    uint32_t m_offset;
  };

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

inline bool Column::is_blob() const { return m_is_blob; }

inline bool Column::read_is_null(const unsigned char *mysql_row) const {
  return m_nullable && (m_null_bitmask & *(mysql_row + m_null_byte_offset));
}

inline void Column::write_is_null(bool is_null, unsigned char *mysql_row,
                                  size_t mysql_row_length
                                      TEMPTABLE_UNUSED_NODBUG) const {
  if (is_nullable()) {
    unsigned char *b = mysql_row + m_null_byte_offset;

    DBUG_ASSERT(buf_is_inside_another(b, 1, mysql_row, mysql_row_length));

    if (is_null) {
      *b |= m_null_bitmask;
    } else {
      *b &= ~m_null_bitmask;
    }
  } else {
    DBUG_ASSERT(!is_null);
  }
}

inline bool Column::is_fixed_size() const { return m_length_bytes_size == 0; }

inline uint32_t Column::read_user_data_length(
    const unsigned char *mysql_row) const {
  const unsigned char *p = mysql_row + m_offset;
  switch (m_length_bytes_size) {
    case 0:
      /* Fixed size cell. */
      return m_length;
    case 1:
      return *p;
    case 2:
      return *p | (*(p + 1) << 8);
    case 3:
      return *p | (*(p + 1) << 8) | (*(p + 2) << 16);
    case 4:
      return *p | (*(p + 1) << 8) | (*(p + 2) << 16) | (*(p + 3) << 24);
  }

  abort();
  return 0;
}

inline void Column::write_user_data_length(
    uint32_t data_length, unsigned char *mysql_row,
    size_t mysql_row_length TEMPTABLE_UNUSED_NODBUG) const {
  unsigned char *p = mysql_row + m_offset;

  DBUG_ASSERT((m_length_bytes_size == 0) ||
              (buf_is_inside_another(p, m_length_bytes_size, mysql_row,
                                     mysql_row_length)));

  switch (m_length_bytes_size) {
    case 0:
      /* Fixed size cell. */
      break;
    case 1:
      DBUG_ASSERT(data_length <= 0xFF);
      p[0] = data_length;
      break;
    case 2:
      DBUG_ASSERT(data_length <= 0xFFFF);
      p[0] = (data_length & 0x000000FF);
      p[1] = (data_length & 0x0000FF00) >> 8;
      break;
    case 3:
      DBUG_ASSERT(data_length <= 0xFFFFFF);
      p[0] = (data_length & 0x000000FF);
      p[1] = (data_length & 0x0000FF00) >> 8;
      p[2] = (data_length & 0x00FF0000) >> 16;
      break;
    case 4:
      /* DBUG_ASSERT(data_length <= 0xFFFFFFFF). */
      p[0] = (data_length & 0x000000FF);
      p[1] = (data_length & 0x0000FF00) >> 8;
      p[2] = (data_length & 0x00FF0000) >> 16;
      p[3] = (data_length & 0xFF000000) >> 24;
      break;
    default:
      DBUG_ABORT();
  }
}

inline void Column::read_user_data(unsigned char *data, uint32_t data_length,
                                   const unsigned char *mysql_row,
                                   size_t mysql_row_length) const {
  if (is_blob()) {
    read_blob_user_data(data, data_length, mysql_row, mysql_row_length);
  } else {
    read_std_user_data(data, data_length, mysql_row, mysql_row_length);
  }
}

inline void Column::write_user_data(bool is_null, const unsigned char *data,
                                    uint32_t data_length,
                                    unsigned char *mysql_row,
                                    size_t mysql_row_length) const {
  if (is_blob()) {
    if (is_null) {
      write_blob_user_data(nullptr, 0, mysql_row, mysql_row_length);
    } else {
      DBUG_ASSERT(data);
      write_blob_user_data(data, data_length, mysql_row, mysql_row_length);
    }
  } else {
    write_std_user_data(data, data_length, mysql_row, mysql_row_length);
  }
}

inline void Column::read_std_user_data(
    unsigned char *data, uint32_t data_length, const unsigned char *mysql_row,
    size_t mysql_row_length TEMPTABLE_UNUSED_NODBUG) const {
  DBUG_ASSERT(!is_blob());

  const unsigned char *p = mysql_row + m_user_data_offset;

  DBUG_ASSERT(
      buf_is_inside_another(p, data_length, mysql_row, mysql_row_length));

  memcpy(data, p, data_length);
}

inline void Column::write_std_user_data(
    const unsigned char *data, uint32_t data_length, unsigned char *mysql_row,
    size_t mysql_row_length TEMPTABLE_UNUSED_NODBUG) const {
  DBUG_ASSERT(!is_blob());

  if (data_length > 0) {
    unsigned char *p = mysql_row + m_user_data_offset;

    DBUG_ASSERT(
        buf_is_inside_another(p, data_length, mysql_row, mysql_row_length));

    memcpy(p, data, data_length);
  }
}

inline void Column::read_blob_user_data(
    unsigned char *data, uint32_t data_length, const unsigned char *mysql_row,
    size_t mysql_row_length TEMPTABLE_UNUSED_NODBUG) const {
  DBUG_ASSERT(is_blob());

  const unsigned char *p = mysql_row + m_user_data_offset;

  const unsigned char *ptr_to_data;

  DBUG_ASSERT(buf_is_inside_another(p, sizeof(ptr_to_data), mysql_row,
                                    mysql_row_length));

  /* read the address */
  memcpy(&ptr_to_data, p, sizeof(ptr_to_data));

  DBUG_ASSERT((ptr_to_data) || (data_length == 0));

  memcpy(data, ptr_to_data, data_length);
}

inline void Column::write_blob_user_data(
    const unsigned char *data, uint32_t data_length TEMPTABLE_UNUSED,
    unsigned char *mysql_row,
    size_t mysql_row_length TEMPTABLE_UNUSED_NODBUG) const {
  DBUG_ASSERT(is_blob());

  unsigned char *p = mysql_row + m_user_data_offset;

  /* Note1: data could be NULL.
   * Note2: shallow copy - pointer to original data is stored. */
  const unsigned char *ptr_to_data = data;

  DBUG_ASSERT(buf_is_inside_another(p, sizeof(ptr_to_data), mysql_row,
                                    mysql_row_length));

  memcpy(p, &ptr_to_data, sizeof(ptr_to_data));
}

inline const unsigned char *Column::get_user_data_ptr(
    const unsigned char *mysql_row) const {
  if (is_blob()) {
    return read_blob_data_ptr(mysql_row);
  } else {
    return calculate_user_data_ptr(mysql_row);
  }
}

inline const unsigned char *Column::calculate_user_data_ptr(
    const unsigned char *mysql_row) const {
  DBUG_ASSERT(!is_blob());

  return (mysql_row + m_user_data_offset);
}

inline const unsigned char *Column::read_blob_data_ptr(
    const unsigned char *mysql_row) const {
  DBUG_ASSERT(is_blob());

  const unsigned char *p = mysql_row + m_user_data_offset;

  const unsigned char *data_ptr;

  /* read the address */
  memcpy(&data_ptr, p, sizeof(data_ptr));

  return data_ptr;
}

} /* namespace temptable */

#endif /* TEMPTABLE_COLUMN_H */
