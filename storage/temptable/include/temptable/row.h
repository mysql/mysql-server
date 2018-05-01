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

/** @file storage/temptable/include/temptable/row.h
TempTable Row declarations. */

// clang-format off
/** @page PAGE_TEMPTABLE_ROW_FORMAT Row format

The handler interface uses two different formats. Assume the following table:

~~~~~~~~~~~~~{.sql}
CREATE TABLE tt (
  c1 INT(11) DEFAULT NULL,
  c2 INT(11) NOT NULL,
  c3 VARCHAR(8) DEFAULT NULL,
  c4 VARCHAR(8) NOT NULL,
  c5 CHAR(8) DEFAULT NULL,
  c6 CHAR(8) NOT NULL,
  c7 VARCHAR(300) DEFAULT NULL,
  KEY `i1` (`c1`,`c2`,`c3`,`c4`,`c5`,`c6`,`c7`)
);
~~~~~~~~~~~~~

@section WRITE_ROW_FORMAT write_row() format

This format is used in the input of the `write_row()` method. The same format
is used by the storage engine in the output buffers of `rnd_next()` and
`index_read()`. Consider the following INSERT statement:

~~~~~~~~~~~~~{.sql}
INSERT INTO t VALUES (123, 123, 'abcd', 'abcd', 'abcd', 'abcd', 'abcd');
~~~~~~~~~~~~~

the row buffer that is passed to `write_row()` is 352 bytes:

hex | raw | description
--- | --- | -----------
f0  | .   | NULLs bitmask 11110000 denoting that out of 4 columns that could
            possibly be NULL (c1, c3, c5 and c7) none is actually NULL.
7b  | {   | c1=123 stored in 4 bytes (little endian) (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
00  | .   | from above (4/4)
7b  | {   | c2=123 stored in 4 bytes (little endian) (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
00  | .   | from above (4/4)
04  | .   | the length of the following data (for VARCHAR cells): 4
61  | a   | c3='abcd' - the actual data (1/4)
62  | b   | from above (2/4)
63  | c   | from above (3/4)
64  | d   | from above (4/4)
a5  | .   | 4 wasted bytes for c3 (1/4)
a5  | .   | from above (2/4)
a5  | .   | from above (3/4)
a5  | .   | from above (4/4)
04  | .   | the length of the following data (for VARCHAR cells): 4
61  | a   | c4='abcd' - the actual data (1/4)
62  | b   | from above (2/4)
63  | c   | from above (3/4)
64  | d   | from above (4/4)
a5  | .   | 4 wasted bytes for c4 (1/4)
a5  | .   | from above (2/4)
a5  | .   | from above (3/4)
a5  | .   | from above (4/4)
61  | a   | c5='abcd    ' (padded to 8 bytes with spaces at the end) (1/8)
62  | b   | from above (2/8)
63  | c   | from above (3/8)
64  | d   | from above (4/8)
20  |     | from above (5/8)
20  |     | from above (6/8)
20  |     | from above (7/8)
20  |     | from above (8/8)
61  | a   | c6='abcd    ' (padded to 8 bytes with spaces at the end) (1/8)
62  | b   | from above (2/8)
63  | c   | from above (3/8)
64  | d   | from above (4/8)
20  |     | from above (5/8)
20  |     | from above (6/8)
20  |     | from above (7/8)
20  |     | from above (8/8)
04  | .   | the length (occupying 2 bytes) of the following data
            (for VARCHAR cells): 4 (1/2)
00  | .   | from above (2/2)
61  | a   | c7='abcd' - the actual data (1/4)
62  | b   | from above (2/4)
63  | c   | from above (3/4)
64  | d   | from above (4/4)
a5  | .   | a5 repeats 296 times, wasted bytes for c7 (1/296)
a5  | .   | from above (2/296)
..  | ..  | ..
a5  | .   | from above (296/296)

@section INDEX_READ_FORMAT index_read() format

Consider the following SELECT statement:

~~~~~~~~~~~~~{.sql}
SELECT * FROM t WHERE
c1=123 AND
c2=123 AND
c3='abcd' AND
c4='abcd' AND
c5='abcd' AND
c6='abcd' AND
c7='abcd';
~~~~~~~~~~~~~

the indexed cells buffer that is passed to `index_read()` is 350 bytes:

hex | raw | description
--- | --- | -----------
00  | .   | c1 NULL byte, denoting that c1 is not NULL
            (would have been 01 is c1 was NULL)
7b  | {   | c1=123 stored in 4 bytes (little endian) (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
00  | .   | from above (4/4)
7b  | {   | c2=123 stored in 4 bytes (little endian) (because c2 cannot be NULL
            there is no leading byte to indicate NULL or not NULL) (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
00  | .   | from above (4/4)
00  | .   | c3 NULL byte, denoting that c3 is not NULL
            (would have been 01 if c3 was NULL)
04  | .   | c3 length (4), always 2 bytes (1/2)
00  | .   | from above (2/2)
61  | a   | c3='abcd' - the actual data (1/4)
62  | b   | from above (2/4)
63  | c   | from above (3/4)
64  | d   | from above (4/4)
00  | .   | 4 wasted bytes for c3 (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
00  | .   | from above (4/4)
04  | .   | c4 length (4), always 2 bytes (no NULL byte for c4) (1/2)
00  | .   | from above (2/2)
61  | a   | c4='abcd' - the actual data (1/4)
62  | b   | from above (2/4)
63  | c   | from above (3/4)
64  | d   | from above (4/4)
00  | .   | 4 wasted bytes for c4 (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
00  | .   | from above (4/4)
00  | .   | c5 NULL byte, denoting that c5 is not NULL
            (would have been 01 if c5 was NULL)
61  | a   | c5='abcd    ' (1/8)
62  | b   | from above (2/8)
63  | c   | from above (3/8)
64  | d   | from above (4/8)
20  |     | from above (5/8)
20  |     | from above (6/8)
20  |     | from above (7/8)
20  |     | from above (8/8)
61  | a   | c6='abcd    ' (c6 cannot be NULL) (1/8)
62  | b   | from above (2/8)
63  | c   | from above (3/8)
64  | d   | from above (4/8)
20  |     | from above (5/8)
20  |     | from above (6/8)
20  |     | from above (7/8)
20  |     | from above (8/8)
00  | .   | c7 NULL byte
04  | .   | c7 length (4), always 2 bytes (1/2)
00  | .   | from above (2/2)
61  | a   | c7='abcd' - the actual data (1/4)
62  | b   | from above (2/4)
63  | c   | from above (3/4)
64  | d   | from above (4/4)
00  | .   | 296 wasted bytes for c7 (1/296)
00  | .   | from above (2/296)
..  | ..  | ..
00  | .   | from above (296/296)

@section TEMPTABLE_FORMAT TempTable format

We introduce a new format, lets call it TempTable format, that fulfills the
following:

1. Does not waste space for VARCHAR cells
2. It must be possible to convert from write_row() format to this new format
3. It must be possible to convert the new format to write_row() format
4. When a row is stored internally in this new format, it must be possible
   to compare its relevant cells to cells in the index_read() format without
   any heap memory allocation (malloc()/new) and without copying of user
   data (memcpy()).

For this we introduce a Cell class, which has the following properties:
1. NULL byte (bool, 1 byte)
2. user data length (uint32_t, 4 bytes)
3. pointer to the user data (void*, 8 bytes on 64-bit machines)
4. can compare itself to another cell
5. can hash itself

A Cell object does not store actual user data, only a pointer to it. This way
we can create cells that point inside the buffer provided to `write_row()` or
point inside our own buffer, where the user data is copied for storage.

A row in the TempTable format consists of a set of Cells stored in one buffer,
together with the actual user data. The size of a row is the size of all
user data + 16 bytes overhead for each cell (for the Cell object).

In the above example both the row (`write_row()` format) and the indexed cells
(`index_read()` format) would be represented like in the table below, in 148
bytes. Think of a POD
~~~~~~~~~~~~~{.cpp}
struct Cell {
  bool is_null;
  uint32_t len;
  void* data;
};
~~~~~~~~~~~~~

hex | raw | description
--- | --- | -----------
00  | .   | c1 NULL byte (00 means not NULL)
00  | .   | 3 bytes padding (1/3)
00  | .   | from above (2/3)
00  | .   | from above (3/3)
00  | .   | c1 length in 4 bytes in whatever is the machine's
            native byte order (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
04  | .   | from above (4/4)
f1  | .   | address in memory where c1 user data is stored (1/8)
f1  | .   | from above (2/8)
f1  | .   | from above (3/8)
f1  | .   | from above (4/8)
f1  | .   | from above (5/8)
f1  | .   | from above (6/8)
f1  | .   | from above (7/8)
f1  | .   | from above (8/8)
00  | .   | c2 NULL byte (00 means not NULL)
00  | .   | 3 bytes padding (1/3)
00  | .   | from above (2/3)
00  | .   | from above (3/3)
00  | .   | c2 length in 4 bytes in whatever is the machine's
            native byte order (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
04  | .   | from above (4/4)
f2  | .   | address in memory where c2 user data is stored (1/8)
f2  | .   | from above (2/8)
f2  | .   | from above (3/8)
f2  | .   | from above (4/8)
f2  | .   | from above (5/8)
f2  | .   | from above (6/8)
f2  | .   | from above (7/8)
f2  | .   | from above (8/8)
00  | .   | c3 NULL byte (00 means not NULL)
00  | .   | 3 bytes padding (1/3)
00  | .   | from above (2/3)
00  | .   | from above (3/3)
00  | .   | c3 length in 4 bytes in whatever is the machine's
            native byte order (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
04  | .   | from above (4/4)
f3  | .   | address in memory where c3 user data is stored (1/8)
f3  | .   | from above (2/8)
f3  | .   | from above (3/8)
f3  | .   | from above (4/8)
f3  | .   | from above (5/8)
f3  | .   | from above (6/8)
f3  | .   | from above (7/8)
f3  | .   | from above (8/8)
00  | .   | c4 NULL byte (00 means not NULL)
00  | .   | 3 bytes padding (1/3)
00  | .   | from above (2/3)
00  | .   | from above (3/3)
00  | .   | c4 length in 4 bytes in whatever is the machine's
            native byte order (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
04  | .   | from above (4/4)
f4  | .   | address in memory where c4 user data is stored (1/8)
f4  | .   | from above (2/8)
f4  | .   | from above (3/8)
f4  | .   | from above (4/8)
f4  | .   | from above (5/8)
f4  | .   | from above (6/8)
f4  | .   | from above (7/8)
f4  | .   | from above (8/8)
00  | .   | c5 NULL byte (00 means not NULL)
00  | .   | 3 bytes padding (1/3)
00  | .   | from above (2/3)
00  | .   | from above (3/3)
00  | .   | c5 length in 4 bytes in whatever is the machine's
            native byte order (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
08  | .   | from above (4/4)
f5  | .   | address in memory where c5 user data is stored (1/8)
f5  | .   | from above (2/8)
f5  | .   | from above (3/8)
f5  | .   | from above (4/8)
f5  | .   | from above (5/8)
f5  | .   | from above (6/8)
f5  | .   | from above (7/8)
f5  | .   | from above (8/8)
00  | .   | c6 NULL byte (00 means not NULL)
00  | .   | 3 bytes padding (1/3)
00  | .   | from above (2/3)
00  | .   | from above (3/3)
00  | .   | c6 length in 4 bytes in whatever is the machine's
            native byte order (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
08  | .   | from above (4/4)
f6  | .   | address in memory where c6 user data is stored (1/8)
f6  | .   | from above (2/8)
f6  | .   | from above (3/8)
f6  | .   | from above (4/8)
f6  | .   | from above (5/8)
f6  | .   | from above (6/8)
f6  | .   | from above (7/8)
f6  | .   | from above (8/8)
00  | .   | c7 NULL byte (00 means not NULL)
00  | .   | 3 bytes padding (1/3)
00  | .   | from above (2/3)
00  | .   | from above (3/3)
00  | .   | c7 length in 4 bytes in whatever is the machine's
            native byte order (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
04  | .   | from above (4/4)
f7  | .   | address in memory where c7 user data is stored (1/8)
f7  | .   | from above (2/8)
f7  | .   | from above (3/8)
f7  | .   | from above (4/8)
f7  | .   | from above (5/8)
f7  | .   | from above (6/8)
f7  | .   | from above (7/8)
f7  | .   | from above (8/8)
7b  | {   | c1=123, the address of this is f1f1f1f1 (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
00  | .   | from above (4/4)
7b  | {   | c2=123, the address of this is f2f2f2f2 (1/4)
00  | .   | from above (2/4)
00  | .   | from above (3/4)
00  | .   | from above (4/4)
61  | a   | c3='abcd', the address of this is f3f3f3f3 (1/4)
62  | b   | from above (2/4)
63  | c   | from above (3/4)
64  | d   | from above (4/4)
61  | a   | c4='abcd', the address of this is f4f4f4f4 (1/4)
62  | b   | from above (2/4)
63  | c   | from above (3/4)
64  | d   | from above (4/4)
61  | a   | c5='abcd    ', the address of this is f5f5f5f5 (1/8)
62  | b   | from above (2/8)
63  | c   | from above (3/8)
64  | d   | from above (4/8)
20  |     | from above (5/8)
20  |     | from above (6/8)
20  |     | from above (7/8)
20  |     | from above (8/8)
61  | a   | c6='abcd    ', the address of this is f6f6f6f6 (1/8)
62  | b   | from above (2/8)
63  | c   | from above (3/8)
64  | d   | from above (4/8)
20  |     | from above (5/8)
20  |     | from above (6/8)
20  |     | from above (7/8)
20  |     | from above (8/8)
61  | a   | c7='abcd', the address of this is f7f7f7f7 (1/4)
62  | b   | from above (2/4)
63  | c   | from above (3/4)
64  | d   | from above (4/4)
*/
// clang-format on

#ifndef TEMPTABLE_ROW_H
#define TEMPTABLE_ROW_H

#include "my_dbug.h"
#include "sql/field.h"
#include "storage/temptable/include/temptable/allocator.h"
#include "storage/temptable/include/temptable/cell.h"
#include "storage/temptable/include/temptable/column.h"
#include "storage/temptable/include/temptable/result.h"

namespace temptable {

/** A row representation. A row consists of multiple cells.
A row is created from a handler row (in write_row() format) and initially it
refers the data in the provided handler row - without copying any user data.
Nevertheless such a lightweight row can be used in the same way as a row
that has copied the user data and owns it. */
class Row {
 public:
  explicit Row(const unsigned char *mysql_row, Allocator<uint8_t> *allocator);

  /** Copy constructing is disabled because it is too expensive. */
  Row(const Row &) = delete;

  /** Copy assignment is disabled because it is too expensive. */
  Row &operator=(const Row &) = delete;

  /** Move constructor. `other` is undefined after this call. */
  Row(Row &&other);

  /** Move assignment. `rhs` is undefined after this call. */
  Row &operator=(Row &&rhs);

  /** Destructor. */
  ~Row();

  /** Get a given cell. The cell contains pointers inside the row, so its
   * lifetime should not be longer than the row.
   * @return a cell from the row. */
  Cell cell(
      /** [in] Column that corresponds to this cell. */
      const Column &column,
      /** [in] The index of the cell to fetch (must be < number_of_cells()). */
      size_t i) const;

  /** Copy the user data to an own buffer (convert from write_row() format).
   * @return Result:OK or other Result::* error code */
  Result copy_to_own_memory(
      /** [in] Metadata for the columns that constitute this row. */
      const Columns &columns,
      /** [in] Length of the mysql row in bytes (m_ptr). */
      size_t mysql_row_length) const;

  /** Copy the row in a MySQL buffer (convert to write_row() format). */
  void copy_to_mysql_row(
      /** [in] Metadata for the columns that constitute this row. */
      const Columns &columns,
      /** [out] Destination buffer to copy the row to. */
      unsigned char *mysql_row,
      /** [in] Presumed length of the mysql row in bytes. */
      size_t mysql_row_length) const;

#ifndef DBUG_OFF
  /** Compare to another row. Used by Table::update() and Table::remove() to
   * double check that the row which is passed as "old row" indeed equals to the
   * row pointed to by the specified position.
   * @retval <0 if lhs < rhs
   * @retval  0 if lhs == rhs
   * @retval >0 if lhs > rhs */
  static int compare(
      /** [in] First row to compare. */
      const Row &lhs,
      /** [in] Second row to compare. */
      const Row &rhs,
      /** [in] Columns that constitute `this` and in `rhs`. */
      const Columns &columns,
      /** [in] List of MySQL column definitions, used for querying metadata. */
      Field **mysql_fields);
#endif /* DBUG_OFF */

 private:
  /** Get a pointer to the cells array. Only defined if
   * `m_data_is_in_mysql_memory` is false.
   * @return cells array */
  Cell *cells() const;

  /** Get a given cell. The cell contains pointers inside the row, so its
   * lifetime should not be longer than the row.
   * @return a cell from the row. */
  Cell cell_in_row(
      /** [in] The index of the cell to fetch (must be < number_of_cells()). */
      size_t i) const;

  /** Get a given cell. The cell contains pointers inside the row, so the
   * returned cell's lifetime should not be longer than the row.
   * @return a cell from the row. */
  Cell cell_in_mysql_memory(
      /** [in] Column that corresponds to this cell. */
      const Column &column) const;

  /** Derives the length of the buffer pointed to by `m_ptr` in bytes (when
   * `m_data_is_in_mysql_memory` is false).
   * @return buffer length */
  size_t buf_length() const;

  /** Allocator to use when copying from MySQL row to our own memory. */
  Allocator<uint8_t> *m_allocator;

  /** Indicate whether this object is lightweight, with just pointers to the
   * MySQL row buffer or not. */
  mutable bool m_data_is_in_mysql_memory;

  /** A pointer to either the mysql row, or our buffer. If
   * - `m_data_is_in_mysql_memory` is true, then this points to a buffer in
   *    mysql write_row() format, not owned by the current Row object;
   * - `m_data_is_in_mysql_memory` is false, then this points a our own buffer
   *    that holds the cells and the user data. Its structure is:
   *    [0, A = sizeof(size_t)): buffer length
   *    [A, B = A + number_of_cells * sizeof(Cell)): cells array
   *    [B, B + sum(user data length for each cell)): user data of the cells */
  mutable unsigned char *m_ptr;
};

/* Implementation of inlined methods. */

inline Row::Row(const unsigned char *mysql_row, Allocator<uint8_t> *allocator)
    : m_allocator(allocator),
      m_data_is_in_mysql_memory(true),
      m_ptr(const_cast<unsigned char *>(mysql_row)) {}

inline Row::Row(Row &&other)
    : m_allocator(other.m_allocator),
      m_data_is_in_mysql_memory(other.m_data_is_in_mysql_memory),
      m_ptr(other.m_ptr) {
  other.m_allocator = nullptr;
  other.m_data_is_in_mysql_memory = false;
  other.m_ptr = nullptr;
}

inline Row &Row::operator=(Row &&rhs) {
  /* Clean up the destination (this). */
  if (!m_data_is_in_mysql_memory && m_ptr != nullptr) {
    m_allocator->deallocate(m_ptr, buf_length());
  }

  m_allocator = rhs.m_allocator;
  rhs.m_allocator = nullptr;

  m_data_is_in_mysql_memory = rhs.m_data_is_in_mysql_memory;
  rhs.m_data_is_in_mysql_memory = false;

  DBUG_ASSERT(rhs.m_ptr != nullptr);
  m_ptr = rhs.m_ptr;
  rhs.m_ptr = nullptr;

  return *this;
}

inline Row::~Row() {
  if (!m_data_is_in_mysql_memory && m_ptr != nullptr) {
    m_allocator->deallocate(m_ptr, buf_length());
  }
}

inline Cell Row::cell(const Column &column, size_t i) const {
  if (m_data_is_in_mysql_memory) {
    return cell_in_mysql_memory(column);
  } else {
    return cell_in_row(i);
  }
}

inline Cell *Row::cells() const {
  DBUG_ASSERT(!m_data_is_in_mysql_memory);
  DBUG_ASSERT(m_ptr != nullptr);
  return reinterpret_cast<Cell *>(m_ptr + sizeof(size_t));
}

inline Cell Row::cell_in_row(size_t i) const { return cells()[i]; }

inline Cell Row::cell_in_mysql_memory(const Column &column) const {
  DBUG_ASSERT(m_data_is_in_mysql_memory);

  const bool is_null = column.is_null(m_ptr);

  const uint32_t data_length = column.user_data_length(m_ptr);

  const unsigned char *data = m_ptr + column.user_data_offset();

  return Cell{is_null, data_length, data};
}

inline size_t Row::buf_length() const {
  DBUG_ASSERT(!m_data_is_in_mysql_memory);
  DBUG_ASSERT(m_ptr != nullptr);
  return *reinterpret_cast<size_t *>(m_ptr);
}

} /* namespace temptable */

#endif /* TEMPTABLE_ROW_H */
