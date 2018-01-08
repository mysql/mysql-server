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

/** @file storage/temptable/include/temptable/indexed_column.h
TempTable Indexed Column. */

#ifndef TEMPTABLE_INDEXED_COLUMN_H
#define TEMPTABLE_INDEXED_COLUMN_H

#include <cstddef> /* size_t */

#include "m_ctype.h" /* CHARSET_INFO, my_charpos() */
#include "sql/field.h" /* Field */
#include "sql/key.h" /* KEY */

namespace temptable {

enum class Cell_hash_function : uint8_t {
  BINARY,
  CHARSET,
  CHARSET_AND_CHAR_LENGTH,
};

class Indexed_column {
 public:
  explicit Indexed_column(const KEY_PART_INFO& mysql_key_part);

  Indexed_column() = default;

  Cell_hash_function cell_hash_function() const;

  size_t field_index() const;

  bool use_char_length() const;

  size_t char_length() const;

  uint32_t prefix_length() const;

  const KEY_PART_INFO& key_part() const;

  const Field& field() const;

  const CHARSET_INFO* charset() const;

  static const CHARSET_INFO* field_charset(const Field& field);

 private:
  Cell_hash_function m_cell_hash_function;
  uint8_t m_mysql_field_index;
  uint8_t m_use_char_length;
  uint32_t m_char_length;
  uint32_t m_prefix_length;

  const KEY_PART_INFO* m_mysql_key_part;
  const Field* m_mysql_field;
  const CHARSET_INFO* m_cs;
};

/* Implementation of inlined methods. */

inline Cell_hash_function Indexed_column::cell_hash_function() const {
  return m_cell_hash_function;
}

inline size_t Indexed_column::field_index() const {
  return m_mysql_field_index;
}

inline bool Indexed_column::use_char_length() const {
  return m_use_char_length;
}

inline size_t Indexed_column::char_length() const { return m_char_length; }

inline uint32_t Indexed_column::prefix_length() const {
  return m_prefix_length;
}

inline const KEY_PART_INFO& Indexed_column::key_part() const {
  return *m_mysql_key_part;
}

inline const Field& Indexed_column::field() const { return *m_mysql_field; }

inline const CHARSET_INFO* Indexed_column::charset() const { return m_cs; }

} /* namespace temptable */

#endif /* TEMPTABLE_INDEXED_COLUMN_H */
