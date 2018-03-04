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

/** @file storage/temptable/src/indexed_column.cc
TempTable Indexed Column implementation. */

#include <limits> /* std::numeric_limits */

#include "my_dbug.h"               /* DBUG_ASSERT() */
#include "sql/field.h"             /* Field */
#include "sql/key.h"               /* KEY */
#include "storage/temptable/include/temptable/indexed_column.h" /* temptable::Indexed_column */

namespace temptable {

Indexed_column::Indexed_column(const KEY_PART_INFO& mysql_key_part)
    : m_mysql_key_part(&mysql_key_part) {
  DBUG_ASSERT(mysql_key_part.field->field_index <=
              std::numeric_limits<decltype(m_mysql_field_index)>::max());

  m_mysql_field_index = static_cast<decltype(m_mysql_field_index)>(
      mysql_key_part.field->field_index);

  m_mysql_field = mysql_key_part.field;

  m_prefix_length = mysql_key_part.length;

  m_cs = Indexed_column::field_charset(*m_mysql_field);

  /* Mimic hp_hashnr() from storage/heap/hp_hash.c. */

  if (m_cs != nullptr) {
    /* Decide if we should use my_charpos. */
    bool use_char_length =
        m_cs->mbmaxlen > 1 && (mysql_key_part.key_part_flag & HA_PART_KEY_SEG);

    DBUG_EXECUTE_IF("temptable_use_char_length", use_char_length = true;);

    if (use_char_length) {
      m_char_length = mysql_key_part.length / m_cs->mbmaxlen;
      m_cell_hash_function = Cell_hash_function::CHARSET_AND_CHAR_LENGTH;
    } else {
      m_cell_hash_function = Cell_hash_function::CHARSET;
    }
  } else {
    m_cell_hash_function = Cell_hash_function::BINARY;
  }
}

const CHARSET_INFO* Indexed_column::field_charset(const Field& field) {
  /* Decide if we should use charset+collation for comparisons, or rely on pure
   * binary data. */
  switch (field.key_type()) {
    case HA_KEYTYPE_TEXT:
    case HA_KEYTYPE_VARTEXT1:
    case HA_KEYTYPE_VARTEXT2:
    case HA_KEYTYPE_VARBINARY1:
    case HA_KEYTYPE_VARBINARY2:
      if (field.flags & (ENUM_FLAG | SET_FLAG)) {
        return &my_charset_bin;
      } else {
        return field.charset_for_protocol();
      }
    default:
      return nullptr;
  }
}
}
