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

/** @file storage/temptable/src/column.cc
TempTable Column implementation. */

#include "storage/temptable/include/temptable/column.h" /* temptable::Column */

#include "my_dbug.h"   /* DBUG_ASSERT() */
#include "sql/field.h" /* Field */
#include "sql/key.h"   /* KEY */
#include "sql/table.h"

namespace temptable {

Column::Column(const TABLE &mysql_table, const Field &mysql_field) {
  m_nullable = mysql_field.real_maybe_null();

  m_null_bitmask = mysql_field.null_bit;

  /* A pointer to the user data inside TABLE::record[0] or TABLE::record[1].
   * Derived from Field::ptr which always points inside record[0] or record[1].
   * The contents of record[0] or record[1] could be bogus at this time, we
   * don't look at it, we just measure the offset of the user data and use the
   * same offset to get the user data inside our own copy of a row in
   * `m_mysql_row` which is neither record[0] nor record[1]. */
  unsigned char *ptr;
  const_cast<Field &>(mysql_field).get_ptr(&ptr);

  unsigned char *mysql_row = mysql_table.record[0];
  const size_t mysql_row_length = mysql_table.s->rec_buff_length;

  size_t user_data_offset = static_cast<size_t>(ptr - mysql_row);

  if (ptr >= mysql_row && user_data_offset < mysql_row_length) {
    /* ptr is inside record[0]. */
  } else {
    /* ptr does not point inside record[0], try record[1]. */
    mysql_row = mysql_table.record[1];
    user_data_offset = static_cast<size_t>(ptr - mysql_row);
    if (ptr >= mysql_row && user_data_offset < mysql_row_length) {
      /* ptr is inside record[1]. */
    } else {
      /* ptr does not point inside neither record[0] nor record[1]. */
      abort();
    }
  }

  DBUG_ASSERT(user_data_offset <=
              std::numeric_limits<decltype(m_user_data_offset)>::max());
  m_user_data_offset =
      static_cast<decltype(m_user_data_offset)>(user_data_offset);

  switch (mysql_field.real_type()) {
    case MYSQL_TYPE_VARCHAR:
      m_length = mysql_field.offset(mysql_row);
      DBUG_ASSERT(ptr > mysql_row + m_length);
      m_length_bytes_size = ptr - (mysql_row + m_length);
      break;
    default:
      m_length = const_cast<Field &>(mysql_field).data_length();
      m_length_bytes_size = 0;
      break;
  }

  if (m_nullable) {
    m_null_byte_offset = mysql_field.null_offset(mysql_row);
    DBUG_ASSERT(m_null_byte_offset < mysql_row_length);
  } else {
    m_null_byte_offset = 0;
  }
}
}  // namespace temptable
