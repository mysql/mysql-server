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

/** @file storage/temptable/src/indexed_cells.cc
TempTable Indexed Cells implementation. */

#include <array>
#include <cstddef>
#include <limits>

#include "my_dbug.h"
#include "my_hash_combine.h"
#include "sql/field.h"
#include "sql/key.h"
#include "storage/temptable/include/temptable/cell.h"
#include "storage/temptable/include/temptable/index.h"
#include "storage/temptable/include/temptable/indexed_cells.h"
#include "storage/temptable/include/temptable/row.h"
#include "storage/temptable/include/temptable/storage.h"
#include "storage/temptable/include/temptable/table.h"

namespace temptable {

Indexed_cells::Indexed_cells(const unsigned char *mysql_search_cells,
                             uint16_t mysql_search_cells_length,
                             const Index &index)
    : m_data_location(Data_location::MYSQL_BUF_INDEX_READ),
      m_number_of_cells(0 /* set below */),
      m_length(mysql_search_cells_length),
      m_mysql_buf(mysql_search_cells) {
  const KEY &mysql_index = index.mysql_index();

  /* It is possible that a shorter buffer is provided than what would comprise
   * the entire index (prefix search). For example: if an index has 3 columns
   * with lengths 5, 6 and 7, the provided buffer could only have length 11 (5 +
   * 6) instead of the full length 18. So we set `number_of_cells` based on the
   * size of the buffer provided, up to and including the last cell that is
   * fully contained in the buffer. */

  uint16_t taken_length = 0;

  for (size_t i = 0; i < mysql_index.user_defined_key_parts; ++i) {
    const uint16_t length_with_current_cell =
        taken_length + mysql_index.key_part[i].store_length;

    if (length_with_current_cell > mysql_search_cells_length) {
      break;
    }

    ++m_number_of_cells;
    taken_length = length_with_current_cell;
  }
}

Indexed_cells::Indexed_cells(const unsigned char *mysql_row, const Index &index)
    : m_data_location(Data_location::MYSQL_BUF_WRITE_ROW),
      m_number_of_cells(static_cast<decltype(m_number_of_cells)>(
          index.number_of_indexed_columns())),
      m_mysql_buf(mysql_row) {
  DBUG_ASSERT(index.number_of_indexed_columns() <=
              std::numeric_limits<decltype(m_number_of_cells)>::max());
}

Indexed_cells::Indexed_cells(const Row &row, const Index &index)
    : m_data_location(Data_location::ROW),
      m_number_of_cells(static_cast<decltype(m_number_of_cells)>(
          index.number_of_indexed_columns())),
      m_row(&row) {
  DBUG_ASSERT(index.number_of_indexed_columns() <=
              std::numeric_limits<decltype(m_number_of_cells)>::max());
}

Cell Indexed_cells::cell(size_t i, const Index &index) const {
  DBUG_ASSERT(i < m_number_of_cells);

  /** Generate a cell from a `temptable::Row` object with a possibly reduced
   * length, if a prefix index is used. */
  auto indexed_cell_from_row =
      [&index](
          /** [in] Indexed cell number in the index. E.g. if we have a row
           * (a, b, c, d) and an index on (b, c) and we want the cell `c`,
           * then this will be 1. */
          size_t i,
          /** [in] Row that contains the data. */
          const Row &row) -> Cell {
    const auto &indexed_column = index.indexed_column(i);

    /* In the case of the above example, this will be 2. */
    const size_t cell_index_in_row = indexed_column.field_index();

    const auto &column = index.table().columns().at(cell_index_in_row);

    const Cell &row_cell = row.cell(column, cell_index_in_row);

    /* Lower row_cell.data_length() in case we have a prefix index, e.g.:
     * CREATE TABLE t (c CHAR(16), INDEX c(10)); */
    const uint32_t data_length =
        std::min(row_cell.data_length(), indexed_column.prefix_length());

    return Cell{row_cell.is_null(), data_length, row_cell.data()};
  };

  /*
  switch (m_data_location) {
    case Data_location::MYSQL_BUF_INDEX_READ:
      return ...;
    case Data_location::MYSQL_BUF_WRITE_ROW:
      return ...;
    case Data_location::ROW:
      return ...;
  }
  abort();  <-- this is executed when m_data_location == Data_location::ROW
  and compiled with "Studio 12.5 Sun C++ 5.14 SunOS_sparc 2016/05/31" !!!
  So we use if-else instead of switch below. */
  if (m_data_location == Data_location::MYSQL_BUF_INDEX_READ) {
    return cell_from_mysql_buf_index_read(i, index);

  } else if (m_data_location == Data_location::MYSQL_BUF_WRITE_ROW) {
    return indexed_cell_from_row(i, Row(m_mysql_buf, nullptr));

  } else if (m_data_location == Data_location::ROW) {
    return indexed_cell_from_row(i, *m_row);
  }

  /* Not reached. */
  abort();
  return Cell{false, 0, nullptr};
}

int Indexed_cells::compare(const Indexed_cells &rhs, const Index &index) const {
  const Indexed_cells &lhs = *this;
  const size_t lhs_num = lhs.number_of_cells();
  const size_t rhs_num = rhs.number_of_cells();

  const size_t number_of_cells_to_compare = std::min(lhs_num, rhs_num);

  for (size_t i = 0; i < number_of_cells_to_compare; ++i) {
    const Cell &lhs_cell = lhs.cell(i, index);
    const Cell &rhs_cell = rhs.cell(i, index);
    const auto &calculator = index.indexed_column(i).cell_calculator();

    const int cmp_result = calculator.compare(lhs_cell, rhs_cell);

    if (cmp_result != 0) {
      return cmp_result;
    }
  }

  /* `lhs` == `rhs` for the first `number_of_cells_to_compare` cells. Consider
   * them equal even though one of `lhs` or `rhs` may contain more cells than
   * the other. This is part of how prefix search works. */
  return 0;
}

Cell Indexed_cells::cell_from_mysql_buf_index_read(size_t i,
                                                   const Index &index) const {
  if (m_length == 0) {
    return Cell{false, 0, nullptr};
  }

  const KEY &mysql_index = index.mysql_index();

  KEY_PART_INFO *mysql_key_part = &mysql_index.key_part[i];
  Field *mysql_field = mysql_key_part->field;

  const unsigned char *p = m_mysql_buf;
  for (size_t j = 0; j < i; ++j) {
    p += mysql_index.key_part[j].store_length;
  }
  DBUG_ASSERT(p - m_mysql_buf < m_length);

  bool is_null;
  if (mysql_field->real_maybe_null()) {
    is_null = p[0] != '\0';
  } else {
    is_null = false;
  }

  const size_t user_data_offset_in_cell =
      mysql_key_part->store_length - mysql_key_part->length;

  uint16_t data_length;

  switch (user_data_offset_in_cell) {
    case 0:
      /* No is-NULL-byte (defined as NOT NULL), no length bytes. */
      DBUG_ASSERT(!mysql_field->real_maybe_null());
      data_length = mysql_key_part->length;
      break;
    case 1:
      /* is-NULL-byte (can be NULL), no length bytes. */
      DBUG_ASSERT(mysql_field->real_maybe_null());
      data_length = mysql_key_part->length;
      break;
    case 2:
      /* No is-NULL-byte (defined as NOT NULL), 2 bytes for length. */
      DBUG_ASSERT(!mysql_field->real_maybe_null());
      data_length = p[0] | (static_cast<uint16_t>(p[1]) << 8);
      break;
    case 3:
      /* is-NULL-byte (can be NULL), 2 bytes for length. */
      DBUG_ASSERT(mysql_field->real_maybe_null());
      data_length = p[1] | (static_cast<uint16_t>(p[2]) << 8);
      break;
    default:
      /* Don't know how to handle this. */
      abort();
  }

  const unsigned char *data = p + user_data_offset_in_cell;

  /* User data offset from the beginning of the search cells buffer. */
  DBUG_ASSERT(data >= m_mysql_buf);
  DBUG_ASSERT(data - m_mysql_buf <= std::numeric_limits<uint16_t>::max());
  const uint16_t user_data_offset_in_buf =
      static_cast<uint16_t>(data - m_mysql_buf);

  /* Bytes remaining from the search cells buffer. For example - we may
   * have an index on two columns (c1 CHAR(4), c2 CHAR(8)), but the
   * mysql search cells may only contain '_aaaa_bb' for c1='aaaa' and
   * c2='bb%' (_ designates some metadata bytes). In other words - the
   * last cell in the mysql buffer may be incomplete. */
  DBUG_ASSERT(m_length >= user_data_offset_in_buf);
  DBUG_ASSERT(m_length - user_data_offset_in_buf <=
              std::numeric_limits<uint16_t>::max());
  const uint16_t remaining = m_length - user_data_offset_in_buf;

  if (data_length > remaining) {
    data_length = remaining;
  }

  return Cell{is_null, data_length, data};
}

size_t Indexed_cells_hash::operator()(
    const Indexed_cells &indexed_cells) const {
  size_t h = 0;

  const size_t number_of_cells = indexed_cells.number_of_cells();

  for (size_t i = 0; i < number_of_cells; ++i) {
    const Cell &cell = indexed_cells.cell(i, m_index);
    const auto &calculator = m_index.indexed_column(i).cell_calculator();
    const size_t cell_hash = calculator.hash(cell);
    my_hash_combine(h, cell_hash);
  }

  return h;
}

} /* namespace temptable */
