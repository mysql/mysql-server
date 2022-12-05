/* Copyright (c) 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file
  Services for bulk data conversion and load to SE.
*/

#pragma once

#include <assert.h>
#include <mysql/components/service.h>
#include <stddef.h>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

class THD;
struct TABLE;
struct CHARSET_INFO;

struct Bulk_load_error_location_details {
  std::string filename;
  size_t row_number;
  std::string column_name;
  std::string column_type;
  std::string column_input_data;
};

struct Column_text {
  /** Column data. */
  const char *m_data_ptr{};

  /** Column data length. */
  size_t m_data_len{};
};

struct Column_mysql {
  /** Column Data Type */
  int m_type{};

  bool m_is_null{false};

  /** Column data */
  char *m_data_ptr{};

  /** Column data length. */
  size_t m_data_len{};

  /** Column data in integer format. Used only for specific datatype. */
  uint64_t m_int_data;
};

/** Implements the row and column memory management for parse and load
operations. We try to pre-allocate the memory contigously as much as we can to
maximize the performance.

@tparam Column_type Column_text when used in the CSV context, Column_sql when
used in the InnoDB context.
*/
template <typename Column_type>
class Row_bunch {
 public:
  /** Create a new row bunch.
  @param[in]  n_cols  number of columns */
  Row_bunch(size_t n_cols) : m_num_columns(n_cols) {}

  /** @return return number of rows in the bunch. */
  size_t get_num_rows() const { return m_num_rows; }

  /** @return return number of columns in each row. */
  size_t get_num_cols() const { return m_num_columns; }

  /** Process all columns, invoking callback for each.
  @param[in]  row_index  index of the row
  @param[in]  cbk        callback function
  @return true if successful */
  template <typename F>
  bool process_columns(size_t row_index, F &&cbk) {
    assert(row_index < m_num_rows);

    auto start_index = row_index * m_num_columns;
    assert(start_index + m_num_columns <= m_columns.size());

    for (size_t index = 0; index < m_num_columns; ++index) {
      bool last_col = (index == m_num_columns - 1);
      if (!cbk(m_columns[start_index + index], last_col)) {
        return false;
      }
    }
    return true;
  }

  /** Get current row offset to access columns.
  @param[in]  row_index  row index
  @return row offset in column vector. */
  size_t get_row_offset(size_t row_index) const {
    assert(row_index <= m_num_rows);
    return row_index * m_num_columns;
  }

  /** Get colum using row offset and column index.
  @param[in]  row_offset  row offset in column vector
  @param[in]  col_index   index of the colum within row
  @return column data */
  Column_type &get_column(size_t row_offset, size_t col_index) {
    assert(col_index < m_num_columns);
    assert(row_offset + col_index < m_columns.size());
    return m_columns[row_offset + col_index];
  }

  /** Get constant colum for reading using row offset and column index.
  @param[in]  row_offset  row offset in column vector
  @param[in]  col_index   index of the colum within row
  @return column data */
  const Column_type &read_column(size_t row_offset, size_t col_index) const {
    assert(col_index < m_num_columns);
    assert(row_offset + col_index < m_columns.size());
    return m_columns[row_offset + col_index];
  }

  /** Set the number of rows. Adjust number of rows base on maximum column
  storage limit.
  @param[in,out]  n_rows  number of rows
  @return true if successful, false if too many rows or columns. */
  bool set_num_rows(size_t n_rows) {
    /* Avoid any overflow during multiplication. */
    if (n_rows > std::numeric_limits<uint32_t>::max() ||
        m_num_columns > std::numeric_limits<uint32_t>::max()) {
      return false;
    }
    auto total_cols = (uint64_t)n_rows * m_num_columns;

    if (total_cols > S_MAX_TOTAL_COLS) {
      return false;
    }

    m_num_rows = n_rows;

    /* Extend columns if needed. */
    if (m_columns.size() < total_cols) {
      m_columns.resize(total_cols);
    }
    return true;
  }

  /** Limit allocation upto 64 M columns. */
  const static size_t S_MAX_TOTAL_COLS = 64 * 1024 * 1024;

 private:
  /** All the columns. */
  std::vector<Column_type> m_columns;

  /** Number of rows. */
  size_t m_num_rows{};

  /** Number of columns in each row. */
  size_t m_num_columns{};
};

using Rows_text = Row_bunch<Column_text>;
using Rows_mysql = Row_bunch<Column_mysql>;

/** Bulk Data conversion. */
BEGIN_SERVICE_DEFINITION(bulk_data_convert)
/** Convert row from text format for MySQL column format. Convert as many
rows as possible consuming the data buffer starting form next_index. On
output next_index is the next row index that is not yet consumed. If it
matches the size of input text_rows, then all rows are consumed.
@param[in,out]  thd            session THD
@param[in]      table          MySQL TABLE
@param[in]      text_rows      rows with column in text
@param[in,out]  next_index     next_index in text_rows to be processed
@param[in,out]  buffer         data buffer for keeping sql row data
@param[in,out]  buffer_length  length of the data buffer
@param[in]      charset        input row data character set
@param[out]     sql_rows       rows with colum in MySQL column format
@return error code. */
DECLARE_METHOD(int, mysql_format,
               (THD * thd, const TABLE *table, const Rows_text &text_rows,
                size_t &next_index, char *buffer, size_t buffer_length,
                const CHARSET_INFO *charset, Rows_mysql &sql_rows,
                Bulk_load_error_location_details &error_details));

/** Check if session is interrupted.
@param[in,out]  thd        session THD
@return true if connection or statement is killed. */
DECLARE_METHOD(bool, is_killed, (THD * thd));

END_SERVICE_DEFINITION(bulk_data_convert)

/* Bulk data load to SE. */
BEGIN_SERVICE_DEFINITION(bulk_data_load)
/** Begin Loading bulk data to SE.
@param[in,out]  thd          session THD
@param[in]      table        MySQL TABLE
@param[in,out]  num_threads  Number of concurrent threads
@return SE bulk load context or nullptr in case of an error. */
DECLARE_METHOD(void *, begin,
               (THD * thd, const TABLE *table, size_t &num_threads));

/** Load a set of rows to SE table by one thread.
@param[in,out]  thd    session THD
@param[in,out]  ctx    SE load context returned by begin()
@param[in]      table  MySQL TABLE
@param[in]      sql_rows  row data to load
@param[in]      thrad  current thread number
@return true if successful. */
DECLARE_METHOD(bool, load,
               (THD * thd, void *ctx, const TABLE *table,
                const Rows_mysql &sql_rows, size_t thread));

/** End Loading bulk data to SE.

Called at the end of bulk load execution, even if begin or load calls failed.

@param[in,out]  thd    session THD
@param[in,out]  ctx    SE load context
@param[in]      table  MySQL TABLE
@param[in]      error  true, if exiting after error
@return true if successful. */
DECLARE_METHOD(bool, end,
               (THD * thd, void *ctx, const TABLE *table, bool error));

/** Check if a table is supported by the bulk load implementation.
@param[in]      table  MySQL TABLE
@return true if table is supported. */
DECLARE_METHOD(bool, is_table_supported, (const TABLE *table));

END_SERVICE_DEFINITION(bulk_data_load)
