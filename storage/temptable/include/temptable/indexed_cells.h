/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

/** @file storage/temptable/include/temptable/indexed_cells.h
TempTable Indexed Cells declaration. */

#ifndef TEMPTABLE_INDEXED_CELLS_H
#define TEMPTABLE_INDEXED_CELLS_H

#include <array>
#include <cstddef>
#include <limits>

#include "sql/key.h"
#include "sql/sql_const.h"
#include "storage/temptable/include/temptable/cell.h"
#include "storage/temptable/include/temptable/column.h"
#include "storage/temptable/include/temptable/row.h"
#include "storage/temptable/include/temptable/storage.h"

namespace temptable {

class Index;

/** Indexed cells represent one or more cells that are covered by an index. */
class Indexed_cells {
 public:
  /** Construct from a MySQL indexed cells (eg index_read() input buffer). */
  Indexed_cells(
      /** [in] Search cells in "index_read() input" format. These must remain
       * valid during the lifetime of the created `Indexed_cells` object. */
      const unsigned char *mysql_search_cells,
      /** [in] The length of `mysql_search_cells` in bytes. */
      uint16_t mysql_search_cells_length,
      /** [in] MySQL index, used for querying metadata. */
      const Index &index);

  /** Construct from a mysql row. The row must remain valid during the
   * lifetime of the created `Indexed_cells` object. */
  Indexed_cells(
      /** [in] MySQL row. */
      const unsigned char *mysql_row,
      /** [in] MySQL index whose cells this `Indexed_cells` represents. */
      const Index &index);

  /** Construct from a row in a table. The row must remain valid during the
   * lifetime of the created `Indexed_cells` object. */
  Indexed_cells(
      /** [in] Row from which to create the indexed cells. */
      const Row &row,
      /** [in] MySQL index, used for querying metadata. */
      const Index &index);

  /** Get the row of these indexed cells. There is no row if this Indexed_cells
   * object has been created from a MySQL search cells (Handler::index_read()
   * input), so this method must not be called in this case.
   * @return row, an element of Table::m_rows */
  Storage::Element *row() const;

  /** Export the row of these indexed cells in the mysql row format
   * (write_row()). As with the `row()` method, this one does not make sense
   * and must not be called if the current Indexed_cells object has been
   * created from MySQL search cells. */
  void export_row_to_mysql(
      /** [in] Metadata for the columns that constitute the exported row. */
      const Columns &columns,
      /** [out] Buffer to write the MySQL row into. */
      unsigned char *mysql_row,
      /** [in] Presumed length of the mysql row in bytes. */
      size_t mysql_row_length) const;

  /** Get the number of indexed cells.
   * @return number of indexed cells */
  size_t number_of_cells() const;

  /** Set the number of indexed cells. It only makes sense to reduce the number
   * in order to compare less cells for the purposes of prefix search. We treat
   * (10) == (10, 20). */
  void number_of_cells(size_t n);

  /** Get a given indexed cell.
   * @return cell */
  Cell cell(
      /** [in] Index of the cell within the indexed cells, must be in
       * [0, number_of_cells()). */
      size_t i,
      /** [in] Index to which the current objects belongs, used for
       * querying metadata. */
      const Index &index) const;

  /** Compare to another indexed cells object. Each cell is compared
   * individually until a differing cells are found. If the compared
   * objects contain different number of cells and all cells are equal
   * up to the smaller object, then the objects are considered equal.
   * E.g. (10, 15) == (10, 15, 23).
   * @retval <0 if this < rhs
   * @retval 0 if this == rhs
   * @retval >0 if this > rhs */
  int compare(
      /** [in] Indexed cells to compare with the current object. */
      const Indexed_cells &rhs,
      /** [in] Index, used for querying metadata. */
      const Index &index) const;

 private:
  /** Enum that designates where the actual user data is stored. */
  enum class Data_location : uint8_t {
    /** The data is in a MySQL buffer in index_read() input format (MySQL
     * search cells). */
    MYSQL_BUF_INDEX_READ,
    /** The data is in a MySQL buffer in write_row() format (MySQL row). */
    MYSQL_BUF_WRITE_ROW,
    /** The data is in `temptable::Row`. */
    ROW,
  };

  /** Generate a cell from a `temptable::Row` object with a possibly reduced
   * length, if a prefix index is used. */
  static Cell cell_from_row(
      /** [in] Indexed cell number in the index. E.g. if we have a row
       * (a, b, c, d) and an index on (b, c) and we want the cell `c`,
       * then this will be 1. */
      size_t i,
      /** [in] Index to which the current objects belongs, used for
       * querying metadata. */
      const Index &index,
      /** [in] Row that contains the data. */
      const Row &row);

  /** Derive the Nth cell if
   * `m_data_location == Data_location::MYSQL_BUF_INDEX_READ`.
   * @return Nth cell */
  Cell cell_from_mysql_buf_index_read(
      /** [in] Index of the cell within the indexed cells, must be in
       * [0, number_of_cells()). */
      size_t i,
      /** [in] Index, for querying metadata via the MySQL index. */
      const Index &index) const;

  /** Flag indicating whether we are interpreting MySQL buffer or we
   * have references to a `temptable::Row` object. */
  Data_location m_data_location;

  /** Number of cells that are indexed. */
  uint8_t m_number_of_cells;

  static_assert(std::numeric_limits<decltype(m_number_of_cells)>::max() >=
                    MAX_REF_PARTS,
                "m_number_of_cells is not large enough to store the maximum "
                "number of indexed cells");

  /** MySQL search cells' length, used only when
   * `m_data_location == MYSQL_BUF_INDEX_READ`. */
  uint16_t m_length;

  static_assert(
      std::numeric_limits<decltype(m_length)>::max() >= MAX_KEY_LENGTH,
      "m_length is not large enough to store the maximum length of an index");

  /** Save space by putting the members in a union. Exactly one of those is
   * used. */
  union {
    /** Pointer to one of:
     * - MySQL search cells buffer (index_read() input format)
     *   used when m_data_location == MYSQL_BUF_INDEX_READ or
     * - MySQL row in write_row() format
     *   used when m_data_location == MYSQL_BUF_WRITE_ROW. */
    const unsigned char *m_mysql_buf;

    /** Pointer to the row, used when m_data_location == ROW. */
    const Row *m_row;
  };
};

/** Indexed cells comparator (a < b). */
class Indexed_cells_less {
 public:
  explicit Indexed_cells_less(const Index &index);

  bool operator()(const Indexed_cells &lhs, const Indexed_cells &rhs) const;

 private:
  const Index &m_index;
};

/** Indexed cells hasher. */
class Indexed_cells_hash {
 public:
  explicit Indexed_cells_hash(const Index &index);

  size_t operator()(const Indexed_cells &indexed_cells) const;

 private:
  const Index &m_index;
};

/** Indexed cells comparator (a == b). */
class Indexed_cells_equal_to {
 public:
  explicit Indexed_cells_equal_to(const Index &index);

  bool operator()(const Indexed_cells &lhs, const Indexed_cells &rhs) const;

 private:
  const Index &m_index;
};

/* Implementation of inlined methods. */

inline Storage::Element *Indexed_cells::row() const {
  /*
  switch (m_data_location) {
    case Data_location::MYSQL_BUF_INDEX_READ:
      my_abort();
    case Data_location::MYSQL_BUF_WRITE_ROW:
      return ...;
    case Data_location::ROW:
      return ...;
  }
  my_abort();  <-- this is executed when m_data_location == Data_location::ROW
  and compiled with "Studio 12.5 Sun C++ 5.14 SunOS_sparc 2016/05/31" !!!
  So we use if-else instead of switch below. */

  if (m_data_location == Data_location::MYSQL_BUF_INDEX_READ) {
    my_abort();
  } else if (m_data_location == Data_location::MYSQL_BUF_WRITE_ROW) {
    return const_cast<unsigned char *>(m_mysql_buf);
  } else if (m_data_location == Data_location::ROW) {
    return const_cast<Row *>(m_row);
  }

  /* Not reached. */
  my_abort();
}

inline void Indexed_cells::export_row_to_mysql(const Columns &columns,
                                               unsigned char *mysql_row,
                                               size_t mysql_row_length) const {
  /*
  switch (m_data_location) {
    case Data_location::MYSQL_BUF_INDEX_READ:
      my_abort();
    case Data_location::MYSQL_BUF_WRITE_ROW:
      ...
      return;
    case Data_location::ROW:
      ...
      return;
  }
  my_abort();  <-- this is executed when m_data_location == Data_location::ROW
  and compiled with "Studio 12.5 Sun C++ 5.14 SunOS_sparc 2016/05/31" !!!
  So we use if-else instead of switch below. */

  if (m_data_location == Data_location::MYSQL_BUF_INDEX_READ) {
    my_abort();
  } else if (m_data_location == Data_location::MYSQL_BUF_WRITE_ROW) {
    memcpy(mysql_row, m_mysql_buf, mysql_row_length);
    return;
  } else if (m_data_location == Data_location::ROW) {
    m_row->copy_to_mysql_row(columns, mysql_row, mysql_row_length);
    return;
  }

  /* Not reached. */
  my_abort();
}

inline size_t Indexed_cells::number_of_cells() const {
  return m_number_of_cells;
}

inline void Indexed_cells::number_of_cells(size_t n) {
  assert(n <= m_number_of_cells);

  m_number_of_cells = static_cast<decltype(m_number_of_cells)>(n);
}

inline Indexed_cells_less::Indexed_cells_less(const Index &index)
    : m_index(index) {}

inline bool Indexed_cells_less::operator()(const Indexed_cells &lhs,
                                           const Indexed_cells &rhs) const {
  return lhs.compare(rhs, m_index) < 0;
}

inline Indexed_cells_hash::Indexed_cells_hash(const Index &index)
    : m_index(index) {}

inline Indexed_cells_equal_to::Indexed_cells_equal_to(const Index &index)
    : m_index(index) {}

inline bool Indexed_cells_equal_to::operator()(const Indexed_cells &lhs,
                                               const Indexed_cells &rhs) const {
  return lhs.compare(rhs, m_index) == 0;
}

} /* namespace temptable */

#endif /* TEMPTABLE_INDEXED_CELLS_H */
