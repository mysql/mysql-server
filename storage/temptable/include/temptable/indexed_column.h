/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/indexed_column.h
TempTable Indexed Column. */

#ifndef TEMPTABLE_INDEXED_COLUMN_H
#define TEMPTABLE_INDEXED_COLUMN_H

#include <cstddef>

#include "mysql/strings/m_ctype.h"
#include "sql/field.h"
#include "sql/key.h"
#include "storage/temptable/include/temptable/cell_calculator.h"

namespace temptable {

class Indexed_column {
 public:
  /** Default constructor used for std::array initialization in Index. */
  Indexed_column() = default;

  explicit Indexed_column(const KEY_PART_INFO &mysql_key_part);

  size_t field_index() const;

  uint32_t prefix_length() const;

  const Cell_calculator &cell_calculator() const;

 private:
  /** The position of the indexed column. It can range from 1 to MAX_FIELDS.
  Use an appropriate datatype that can hold these values. */
  uint16_t m_mysql_field_index;
  uint32_t m_prefix_length;
  Cell_calculator m_cell_calculator;
};

/* Implementation of inlined methods. */

inline Indexed_column::Indexed_column(const KEY_PART_INFO &mysql_key_part)
    : m_mysql_field_index(static_cast<decltype(m_mysql_field_index)>(
          mysql_key_part.field->field_index())),
      m_prefix_length(mysql_key_part.length),
      m_cell_calculator(mysql_key_part) {
  assert(mysql_key_part.field->field_index() <=
         std::numeric_limits<decltype(m_mysql_field_index)>::max());
}

inline size_t Indexed_column::field_index() const {
  return m_mysql_field_index;
}

inline uint32_t Indexed_column::prefix_length() const {
  return m_prefix_length;
}

inline const Cell_calculator &Indexed_column::cell_calculator() const {
  return m_cell_calculator;
}

} /* namespace temptable */

#endif /* TEMPTABLE_INDEXED_COLUMN_H */
