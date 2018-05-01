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

/** @file storage/temptable/include/temptable/cell.h
TempTable Cell declaration. */

#ifndef TEMPTABLE_CELL_H
#define TEMPTABLE_CELL_H

#include <cstdint>

namespace temptable {

/** A cell is the intersection of a row and a column. In the handler interface
row format (what is provided to the write_row() handler method) a cell may
occupy too much space - in the case of a VARCHAR(N) column it will occupy N
bytes, even if a shorter string is stored in this particular cell. So, our
cell is derived from the above, but does not occupy unnecessary space.

This class is just an interpreter - it does not store the actual data, which
is stored in the `Row` class, allocated at once for all the cells of a row. */
class Cell {
 public:
  /** Constructor. */
  Cell(
      /** [in] Designate whether this cell is NULL. */
      bool is_null,
      /** [in] Length of the user data in bytes (no metadata or any leading
       * bytes to designate the length. Just the user data. */
      uint32_t data_length,
      /** [in] Pointer to the user data. It is not copied inside this newly
       * created Cell object, so it must remain valid for the lifetime of this
       * object. */
      const unsigned char *data);

  /** Check if this cell is NULL.
   * @return true if NULL */
  bool is_null() const;

  /** Get the length of the user data.
   * @return length in bytes */
  uint32_t data_length() const;

  /** Get a pointer to the user data inside the row.
   * @return a pointer */
  const unsigned char *data() const;

 private:
  /** Designate whether the cell is NULL. */
  const bool m_is_null;

  /** Length of the user data pointed by `m_data` in bytes. */
  const uint32_t m_data_length;

  /** User data. */
  const unsigned char *const m_data;
};

/* Implementation of inlined methods. */

inline Cell::Cell(bool is_null, uint32_t data_length, const unsigned char *data)
    : m_is_null(is_null), m_data_length(data_length), m_data(data) {}

inline bool Cell::is_null() const { return m_is_null; }

inline uint32_t Cell::data_length() const { return m_data_length; }

inline const unsigned char *Cell::data() const { return m_data; }

} /* namespace temptable */

#endif /* TEMPTABLE_CELL_H */
