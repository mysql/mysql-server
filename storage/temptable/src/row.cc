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

/** @file storage/temptable/src/row.cc
TempTable Row implementation. */

#include <cstring> /* memcpy() */
#include <utility> /* std::move() */

#include "my_dbug.h"          /* DBUG_ASSERT */
#include "sql/field.h"        /* Field */
#include "sql/table.h"        /* TABLE */
#include "storage/temptable/include/temptable/allocator.h" /* temptable::Allocator */
#include "storage/temptable/include/temptable/cell.h" /* temptable::Cell */
#include "storage/temptable/include/temptable/column.h" /* temptable::Column, temptable::Columns */
#include "storage/temptable/include/temptable/misc.h" /* temptable::buf_is_inside_another() */
#include "storage/temptable/include/temptable/result.h" /* temptable::Result */
#include "storage/temptable/include/temptable/row.h" /* temptable::Row */

namespace temptable {

#ifndef DBUG_OFF
int Row::compare(const Columns& columns, Field** mysql_fields,
                 const Row& rhs) const {
  const Row& lhs = *this;

  for (size_t i = 0; i < columns.size(); ++i) {
    const Field& mysql_field = *mysql_fields[i];
    const Cell& lhs_cell = lhs.cell(columns[i], i);
    const Cell& rhs_cell = rhs.cell(columns[i], i);

    const int cmp_result = lhs_cell.compare(mysql_field, rhs_cell);

    if (cmp_result != 0) {
      return cmp_result;
    }
  }

  return 0;
}
#endif /* DBUG_OFF */

Result Row::copy_to_own_memory(const Columns& columns,
                               size_t mysql_row_length
#ifdef DBUG_OFF
                                   MY_ATTRIBUTE((unused))
#endif /* DBUG_OFF */
                                   ) const {
  DBUG_ASSERT(m_data_is_in_mysql_memory);

  const unsigned char* mysql_row = m_ptr;

  size_t buf_len = sizeof(size_t);

  for (const auto& column : columns) {
    buf_len += sizeof(Cell) + column.user_data_length(mysql_row);
  }

  try {
    m_ptr = m_allocator->allocate(buf_len);
  } catch (Result ex) {
    return ex;
  }

  *reinterpret_cast<size_t*>(m_ptr) = buf_len;

  m_data_is_in_mysql_memory = false;

  /* This is inside `m_ptr`. */
  Cell* cell = cells();

  /* User data begins after the cells array. */
  unsigned char* data_ptr =
      reinterpret_cast<unsigned char*>(cell + columns.size());

  for (const auto& column : columns) {
    const bool is_null = column.is_null(mysql_row);

    const uint32_t data_length = column.user_data_length(mysql_row);

    if (data_length > 0) {
      const unsigned char* data_in_mysql_buf =
          mysql_row + column.user_data_offset();

      DBUG_ASSERT(buf_is_inside_another(data_in_mysql_buf, data_length,
                                        mysql_row, mysql_row_length));
      DBUG_ASSERT(buf_is_inside_another(data_ptr, data_length, m_ptr, buf_len));

      memcpy(data_ptr, data_in_mysql_buf, data_length);
    }

    new (cell) Cell{is_null, data_length, data_ptr};

    ++cell;

    data_ptr += data_length;
  }

  return Result::OK;
}

void Row::copy_to_mysql_row(const Columns& columns, unsigned char* mysql_row,
                            size_t mysql_row_length
#ifdef DBUG_OFF
                                MY_ATTRIBUTE((unused))
#endif /* DBUG_OFF */
                                ) const {
  DBUG_ASSERT(!m_data_is_in_mysql_memory);

  for (size_t i = 0; i < columns.size(); ++i) {
    const Column& column = columns[i];
    const Cell& cell = cells()[i];

    if (column.is_nullable()) {
      unsigned char* b = mysql_row + column.null_byte_offset();

      DBUG_ASSERT(buf_is_inside_another(b, 1, mysql_row, mysql_row_length));

      if (cell.is_null()) {
        *b |= column.null_bitmask();
      } else {
        *b &= ~column.null_bitmask();
      }
    } else {
      DBUG_ASSERT(!cell.is_null());
    }

    const uint32_t data_length = cell.data_length();

    if (!column.is_fixed_size()) {
      const uint8_t length_size = column.length_size();

      /* We must write the length of the user data in a few bytes (length_size)
       * just before the user data itself. This is where l points. */
      unsigned char* l = mysql_row + column.user_data_offset() - length_size;

      DBUG_ASSERT(
          buf_is_inside_another(l, length_size, mysql_row, mysql_row_length));

      switch (length_size) {
        case 0:
          break;
        case 1:
          DBUG_ASSERT(data_length <= 0xFF);
          l[0] = data_length;
          break;
        case 2:
          DBUG_ASSERT(data_length <= 0xFFFF);
          l[0] = data_length & 0x000000FF;
          l[1] = (data_length & 0x0000FF00) >> 8;
          break;
        default:
          DBUG_ABORT();
      }
    }

    unsigned char* u = mysql_row + column.user_data_offset();

    DBUG_ASSERT(
        buf_is_inside_another(u, data_length, mysql_row, mysql_row_length));

    memcpy(u, cell.data(), data_length);
  }
}

} /* namespace temptable */
