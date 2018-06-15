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

/** @file storage/temptable/src/row.cc
TempTable Row implementation. */

#include <cstring>
#include <utility>

#include "my_dbug.h"
#include "sql/field.h"
#include "sql/table.h"
#include "storage/temptable/include/temptable/allocator.h"
#include "storage/temptable/include/temptable/cell.h"
#include "storage/temptable/include/temptable/cell_calculator.h"
#include "storage/temptable/include/temptable/column.h"
#include "storage/temptable/include/temptable/misc.h"
#include "storage/temptable/include/temptable/result.h"
#include "storage/temptable/include/temptable/row.h"

namespace temptable {

#ifndef DBUG_OFF
int Row::compare(const Row &lhs, const Row &rhs, const Columns &columns,
                 Field **mysql_fields) {
  for (size_t i = 0; i < columns.size(); ++i) {
    const Field *mysql_field = mysql_fields[i];
    const Cell &lhs_cell = lhs.cell(columns[i], i);
    const Cell &rhs_cell = rhs.cell(columns[i], i);
    Cell_calculator calculator(mysql_field);

    const int cmp_result = calculator.compare(lhs_cell, rhs_cell);
    if (cmp_result != 0) {
      return cmp_result;
    }
  }

  return 0;
}
#endif /* DBUG_OFF */

Result Row::copy_to_own_memory(const Columns &columns,
                               size_t mysql_row_length) const {
  DBUG_ASSERT(m_data_is_in_mysql_memory);

  const unsigned char *mysql_row = m_ptr;

  size_t buf_len = sizeof(size_t);

  for (const auto &column : columns) {
    buf_len += sizeof(Cell) + column.read_user_data_length(mysql_row);
  }

  try {
    m_ptr = m_allocator->allocate(buf_len);
  } catch (Result ex) {
    return ex;
  }

  *reinterpret_cast<size_t *>(m_ptr) = buf_len;

  m_data_is_in_mysql_memory = false;

  /* This is inside `m_ptr`. */
  Cell *cell = cells();

  /* User data begins after the cells array. */
  unsigned char *data_ptr =
      reinterpret_cast<unsigned char *>(cell + columns.size());

  for (const auto &column : columns) {
    const bool is_null = column.read_is_null(mysql_row);

    const uint32_t data_length = column.read_user_data_length(mysql_row);

    if (data_length > 0) {
      DBUG_ASSERT(buf_is_inside_another(data_ptr, data_length, m_ptr, buf_len));

      column.read_user_data(data_ptr, data_length, mysql_row, mysql_row_length);
    }

    new (cell) Cell{is_null, data_length, data_ptr};

    ++cell;

    data_ptr += data_length;
  }

  return (Result::OK);
}

void Row::copy_to_mysql_row(const Columns &columns, unsigned char *mysql_row,
                            size_t mysql_row_length) const {
  DBUG_ASSERT(!m_data_is_in_mysql_memory);

  for (size_t i = 0; i < columns.size(); ++i) {
    const Column &column = columns[i];
    const Cell &cell = cells()[i];

    /* No need to copy the BLOB memory as the row will remain valid
     * till next operation. */

    column.write_is_null(cell.is_null(), mysql_row, mysql_row_length);
    column.write_user_data_length(cell.data_length(), mysql_row,
                                  mysql_row_length);
    column.write_user_data(cell.is_null(), cell.data(), cell.data_length(),
                           mysql_row, mysql_row_length);
  }
}

} /* namespace temptable */
