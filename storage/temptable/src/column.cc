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

/** @file storage/temptable/src/column.cc
TempTable Column implementation. */

#include "storage/temptable/include/temptable/column.h"

#include <assert.h>

#include "sql/field.h"
#include "sql/key.h"
#include "sql/table.h"

#include "storage/temptable/include/temptable/misc.h"

namespace temptable {

Column::Column(const unsigned char *mysql_row,
               const TABLE &mysql_table TEMPTABLE_UNUSED_NODBUG,
               const Field &mysql_field) {
  /* NOTE: The contents of mysql_row could be bogus at this time,
   * we don't look at the data, we just use it to calculate offsets
   * later used to get the user data inside our own copy of a row in
   * `m_mysql_row` which is neither record[0] nor record[1]. */

#if !defined(NDEBUG)
  const unsigned char *field_ptr = mysql_field.field_ptr();
  const size_t mysql_row_length = mysql_table.s->rec_buff_length;

  assert(field_ptr >= mysql_row);
  assert(field_ptr < mysql_row + mysql_row_length);
#endif /* NDEBUG */

  size_t data_offset;

  m_is_blob = (mysql_field.is_flag_set(BLOB_FLAG) || mysql_field.is_array());

  if (m_is_blob) {
    auto &blob_field = static_cast<const Field_blob &>(mysql_field);

    assert(blob_field.pack_length_no_ptr() <=
           std::numeric_limits<decltype(m_length_bytes_size)>::max());

    m_length_bytes_size = blob_field.pack_length_no_ptr();
    m_offset = mysql_field.offset(const_cast<unsigned char *>(mysql_row));

    const unsigned char *data_ptr =
        mysql_field.field_ptr() + m_length_bytes_size;

    data_offset = static_cast<size_t>(data_ptr - mysql_row);
  } else if (mysql_field.type() == MYSQL_TYPE_VARCHAR) {
    auto &varstring_field = static_cast<const Field_varstring &>(mysql_field);

    assert(varstring_field.get_length_bytes() <=
           std::numeric_limits<decltype(m_length_bytes_size)>::max());

    m_length_bytes_size = varstring_field.get_length_bytes();
    m_offset = mysql_field.offset(const_cast<unsigned char *>(mysql_row));
    data_offset = static_cast<size_t>(
        mysql_field.field_ptr() + mysql_field.get_length_bytes() - mysql_row);
  } else {
    m_length_bytes_size = 0;
    m_length = mysql_field.data_length();
    data_offset = static_cast<size_t>(mysql_field.field_ptr() - mysql_row);
  }
  assert(data_offset <=
         std::numeric_limits<decltype(m_user_data_offset)>::max());
  m_user_data_offset = static_cast<decltype(m_user_data_offset)>(data_offset);

  m_nullable = mysql_field.is_nullable();
  m_null_bitmask = mysql_field.null_bit;

  if (m_nullable) {
    m_null_byte_offset = mysql_field.null_offset(mysql_row);
    assert(m_null_byte_offset < mysql_row_length);
  } else {
    m_null_byte_offset = 0;
  }
}
}  // namespace temptable
