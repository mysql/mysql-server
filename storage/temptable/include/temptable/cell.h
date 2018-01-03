/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reserved.

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

#include <algorithm> /* std::min */
#include <cstdint>   /* uint32_t */

#include "m_ctype.h"               /* CHARSET_INFO, my_charpos() */
#include "my_murmur3.h"            /* murmur3_32() */
#include "sql/field.h"             /* Field */
#include "storage/temptable/include/temptable/indexed_column.h" /* temptable::Indexed_column */

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
      const unsigned char* data);

  /** Check if this cell is NULL.
   * @return true if NULL */
  bool is_null() const;

  /** Get the length of the user data.
   * @return length in bytes */
  uint32_t data_length() const;

  /** Get a pointer to the user data inside the row.
   * @return a pointer */
  const unsigned char* data() const;

  /** Hash this cell.
   * @return a hash number */
  size_t hash(
      /** [in] Describes the column that holds this cell. Used for dealing with
       * charsets. */
      const Indexed_column& indexed_column) const;

  /** Compare to another cell.
   * @retval <0 if this < rhs
   * @retval  0 if this == rhs
   * @retval >0 if this > rhs */
  int compare(
      /** [in] Defines how to compare this cell with other cells (in a
       * given index). */
      const Indexed_column& indexed_column,
      /** [in] The other cell to compare to. */
      const Cell& rhs) const;

  /** Compare to another cell.
   * @retval <0 if this < rhs
   * @retval  0 if this == rhs
   * @retval >0 if this > rhs */
  int compare(
      /** [in] Auxiliary structure that describes the MySQL column that holds
       * this cell. */
      const Field& mysql_field,
      /** [in] The other cell to compare to. */
      const Cell& rhs) const;

 private:
  /** Compare to another cell.
   * @retval <0 if this < rhs
   * @retval  0 if this == rhs
   * @retval >0 if this > rhs */
  int compare(
      /** [in] Auxiliary structure that describes the MySQL column that holds
       * this cell. */
      const Field& mysql_field,
      /** [in] Charset which should be used while comparing this cell with
       * other cells, or nullptr if we should use mysql_field->key_cmp. */
      const CHARSET_INFO* cs,
      /** [in] The other cell to compare to. */
      const Cell& rhs) const;

  /** Designate whether the cell is NULL. */
  const bool m_is_null;

  /** Length of the user data pointed by `m_data` in bytes. */
  const uint32_t m_data_length;

  /** User data. */
  const unsigned char* const m_data;
};

/* Implementation of inlined methods. */

inline Cell::Cell(bool is_null, uint32_t data_length, const unsigned char* data)
    : m_is_null(is_null), m_data_length(data_length), m_data(data) {}

inline bool Cell::is_null() const { return m_is_null; }

inline uint32_t Cell::data_length() const { return m_data_length; }

inline const unsigned char* Cell::data() const { return m_data; }

inline size_t Cell::hash(const Indexed_column& indexed_column) const {
  if (m_is_null) {
    return 1;
  }

  if (m_data_length == 0) {
    return 0;
  }

  size_t length = 0;

  /*
  switch (indexed_column.cell_hash_function()) {
    case Cell_hash_function::CHARSET:
      length = ...
      break;
    case Cell_hash_function::CHARSET_AND_CHAR_LENGTH:
      length = ...
      break;
    case Cell_hash_function::BINARY:
      return ...
  }
  code <-- this is executed when
  indexed_column.cell_hash_function() == Cell_hash_function::BINARY
  and compiled with "Studio 12.5 Sun C++ 5.14 SunOS_sparc 2016/05/31" !!!
  So we use if-else instead of switch below. */

  const auto& hf = indexed_column.cell_hash_function();

  if (hf == Cell_hash_function::CHARSET) {
    length = m_data_length;
  } else if (hf == Cell_hash_function::CHARSET_AND_CHAR_LENGTH) {
    length = std::min(
        static_cast<size_t>(m_data_length),
        my_charpos(indexed_column.charset(), m_data, m_data + m_data_length,
                   indexed_column.char_length()));
  } else if (hf == Cell_hash_function::BINARY) {
    return murmur3_32(m_data, m_data_length, 0);
  } else {
    abort();
  }

  const CHARSET_INFO* cs = indexed_column.charset();

  if (cs->pad_attribute == NO_PAD) {
    /* Strip trailing spaces. */
    length =
        cs->cset->lengthsp(cs, reinterpret_cast<const char*>(m_data), length);
  }

  unsigned long h1 = 1;
  unsigned long h2 = 4;
  cs->coll->hash_sort(cs, m_data, length, &h1, &h2);
  return h1;
}

inline int Cell::compare(const Indexed_column& indexed_column,
                         const Cell& rhs) const {
  return compare(indexed_column.field(), indexed_column.charset(), rhs);
}

inline int Cell::compare(const Field& mysql_field, const Cell& rhs) const {
  return compare(mysql_field, Indexed_column::field_charset(mysql_field), rhs);
}

inline int Cell::compare(const Field& field, const CHARSET_INFO* cs,
                         const Cell& rhs) const {
  const Cell& lhs = *this;

  if (lhs.m_is_null) {
    if (rhs.m_is_null) {
      /* Both are NULL. */
      return 0;
    } else {
      /* NULL < whatever (not NULL). */
      return -1;
    }
  } else {
    if (rhs.m_is_null) {
      /* whatever (not NULL) > NULL. */
      return 1;
    }
  }

  /* Both cells are not NULL. */

  /* If both cells' data is identical, then no need to use the expensive
   * comparisons below because we know that they will report equality. */
  if (lhs.m_data_length == rhs.m_data_length &&
      (lhs.m_data_length == 0 ||
       memcmp(lhs.m_data, rhs.m_data, lhs.m_data_length) == 0)) {
    return 0;
  }

  if (cs != nullptr) {
    size_t lhs_length = lhs.m_data_length;
    size_t rhs_length = rhs.m_data_length;

    if (cs->pad_attribute == NO_PAD) {
      /* Strip trailing spaces. */
      lhs_length = cs->cset->lengthsp(
          cs, reinterpret_cast<const char*>(lhs.m_data), lhs_length);
      rhs_length = cs->cset->lengthsp(
          cs, reinterpret_cast<const char*>(rhs.m_data), rhs_length);
    }

    return cs->coll->strnncollsp(cs, lhs.m_data, lhs_length, rhs.m_data,
                                 rhs_length);
  } else {
    return const_cast<Field*>(&field)->key_cmp(lhs.m_data, rhs.m_data);
  }
}

} /* namespace temptable */

#endif /* TEMPTABLE_CELL_H */
