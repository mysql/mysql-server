/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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
#include <functional>
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
  int16_t m_type{};

  /** Column data length. */
  uint16_t m_data_len{};

  /** If column is NULL. */
  bool m_is_null{false};

  /** Column data */
  char *m_data_ptr{};

  /** Column data in integer format. Used only for specific datatype. */
  uint64_t m_int_data;
};

/** Implements the row and column memory management for parse and load
operations. We try to pre-allocate the memory contiguously as much as we can to
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

    auto row_offset = row_index * m_num_columns;
    return process_columns_by_offset(row_offset, std::move(cbk));
  }

  template <typename F>
  bool process_columns_by_offset(size_t row_offset, F &&cbk) {
    assert(row_offset + m_num_columns <= m_columns.size());

    for (size_t index = 0; index < m_num_columns; ++index) {
      bool last_col = (index == m_num_columns - 1);
      if (!cbk(m_columns[row_offset + index], last_col)) {
        return false;
      }
    }
    return true;
  }

  /** Get current row offset to access columns.
  @param[in]  row_index  row index
  @return row offset in column vector. */
  size_t get_row_offset(size_t row_index) const {
    assert(row_index < m_num_rows);
    return row_index * m_num_columns;
  }

  /** Get next row offset from current row offset.
  @param[in,out]  offset  row offset
  @return true if there is a next row. */
  size_t get_next_row_offset(size_t &offset) const {
    offset += m_num_columns;
    return (offset < m_columns.size());
  }

  /** Get column using row offset and column index.
  @param[in]  row_offset  row offset in column vector
  @param[in]  col_index   index of the column within row
  @return column data */
  Column_type &get_column(size_t row_offset, size_t col_index) {
    assert(col_index < m_num_columns);
    assert(row_offset + col_index < m_columns.size());
    return m_columns[row_offset + col_index];
  }

  /** Get constant column for reading using row offset and column index.
  @param[in]  row_offset  row offset in column vector
  @param[in]  col_index   index of the column within row
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

  /** Limit allocation up to 600M columns. This number is rounded up from an
   * estimate of the number of columns with the max chunk size (1024M). In the
   * worst case we can have 2 bytes per column so a chunk can contain around
   * 512M columns, and because of rows that spill over chunk boundaries we
   * assume we can append a full additional row (which should have at most 4096
   * columns). Rounded up to 600M. */
  const static size_t S_MAX_TOTAL_COLS = 600 * 1024 * 1024;

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

/** Column metadata information. */
struct Column_meta {
  /** Data comparison method. */
  enum class Compare {
    /* Integer comparison */
    INTEGER_SIGNED,
    /* Unsigned Integer comparison */
    INTEGER_UNSIGNED,
    /* Binary comparison (memcmp) */
    BINARY,
    /* Need to callback to use appropriate comparison function in server. */
    MYSQL
  };

  /** @return true if integer type. */
  bool is_integer() const {
    return (m_compare == Compare::INTEGER_SIGNED ||
            m_compare == Compare::INTEGER_UNSIGNED);
  }

  /** Field type */
  int m_type;

  /** If column could be NULL. */
  bool m_is_nullable;

  /** If column is part of primary key. */
  bool m_is_key;

  /** If the key is descending. */
  bool m_is_desc_key;

  /** If the key is prefix of the column. */
  bool m_is_prefix_key;

  /** If it is fixed length type. */
  bool m_is_fixed_len;

  /** If it is integer type. */
  Compare m_compare;

  /** If it is unsigned integer type. */
  bool m_is_unsigned;

  /** Check the row header to find out if it is fixed length. For
  character data type the row header indicates fixed length. */
  bool m_fixed_len_if_set_in_row;

  /** If character column length can be kept in one byte. */
  bool m_is_single_byte_len;

  /** The length of column data if fixed. */
  uint16_t m_fixed_len;

  /** Maximum length of data in bytes. */
  uint16_t m_max_len;

  /** Index of column in row. */
  uint16_t m_index;

  /** Byte index in NULL bitmap. */
  uint16_t m_null_byte;

  /** BIT number in NULL bitmap. */
  uint16_t m_null_bit;

  /** Character set for char & varchar columns. */
  const void *m_charset;
};

/** Row metadata */
struct Row_meta {
  /** Key type for fast comparison. */
  enum class Key_type {
    /* All Keys are signed integer an ascending. */
    INT_SIGNED_ASC,
    /* All keys are integer. */
    INT,
    /* Keys are of any supported type. */
    ANY
  };
  /** All columns in a row are arranged with key columns first. */
  std::vector<Column_meta> m_columns;

  /** Total bitmap header length for the row. */
  size_t m_bitmap_length = 0;

  /** Total header length. */
  size_t m_header_length = 0;

  /** Length of the first key column. Helps to get the row pointer from first
  key data pointer. */
  size_t m_first_key_len = 0;

  /** Key length in bytes for non-integer keys. This is required to estimate
  the space required to save keys. */
  size_t m_key_length = 0;

  /** Number of columns used in primary key. */
  uint32_t m_keys = 0;

  /** Number of columns not used in primary Key. */
  uint32_t m_non_keys = 0;

  /** Key type for comparison. */
  Key_type m_key_type = Key_type::ANY;

  /** Total number of columns. A key could be on a column prefix.
  m_columns <= m_keys + m_non_keys */
  uint32_t m_num_columns = 0;

  /** Approximate row length. */
  size_t m_approx_row_len = 0;
};

namespace Bulk_load {

/** Callbacks for collecting time statistics */
struct Stat_callbacks {
  /* Operation begin. */
  std::function<void()> m_fn_begin;
  /* Operation end. */
  std::function<void()> m_fn_end;
};

}  // namespace Bulk_load

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
@param[in]      metadata       row metadata
@param[out]     sql_rows       rows with column in MySQL column format
@return error code. */
DECLARE_METHOD(int, mysql_format,
               (THD * thd, const TABLE *table, const Rows_text &text_rows,
                size_t &next_index, char *buffer, size_t &buffer_length,
                const CHARSET_INFO *charset, const Row_meta &metadata,
                Rows_mysql &sql_rows,
                Bulk_load_error_location_details &error_details));

/** Convert row to MySQL column format from raw form
@param[in,out] buffer          input raw data buffer
@param[in]     buffer_length   buffer length
@param[in]     metadata        row metadata
@param[in]     start_index     start row index in row bunch
@param[out]    consumed_length length of buffer consumed
@param[in,out] sql_rows        row bunch to fill data
@return error code. */
DECLARE_METHOD(int, mysql_format_from_raw,
               (char *buffer, size_t buffer_length, const Row_meta &metadata,
                size_t start_index, size_t &consumed_length,
                Rows_mysql &sql_rows));

/** Convert row to MySQL column format using the key
@param[in]     metadata   row metadata
@param[in]     sql_keys   Key bunch
@param[in]     key_offset offset for the key
@param[in,out] sql_rows   row bunch to fill data
@param[in]     sql_index  index of the row to be filled
@return error code. */
DECLARE_METHOD(int, mysql_format_using_key,
               (const Row_meta &metadata, const Rows_mysql &sql_keys,
                size_t key_offset, Rows_mysql &sql_rows, size_t sql_index));

/** Check if session is interrupted.
@param[in,out]  thd        session THD
@return true if connection or statement is killed. */
DECLARE_METHOD(bool, is_killed, (THD * thd));

/** Compare two key columns
@param[in]  key1      first key
@param[in]  key2      second key
@param[in]  col_meta  column meta information
@return positive, 0, negative, if key_1 is greater, equal, less than key_2 */
DECLARE_METHOD(int, compare_keys,
               (const Column_mysql &key1, const Column_mysql &key2,
                const Column_meta &col_meta));

/** Get Table row metadata.
@param[in,out]  thd       session THD
@param[in]      table     MySQL TABLE
@param[in]      have_key  include Primary Key metadata
@param[out]     metadata  Metadata
@return true if successful. */
DECLARE_METHOD(bool, get_row_metadata,
               (THD * thd, const TABLE *table, bool have_key,
                Row_meta &metadata));

END_SERVICE_DEFINITION(bulk_data_convert)

/** Column metadata information. */
/* Bulk data load to SE. */
BEGIN_SERVICE_DEFINITION(bulk_data_load)
/** Begin Loading bulk data to SE.
@param[in,out]  thd          session THD
@param[in]      table        MySQL TABLE
@param[in]      data_size    total data size to load
@param[in]      memory       SE memory to be used
@param[in]      num_threads  Number of concurrent threads
@return SE bulk load context or nullptr in case of an error. */
DECLARE_METHOD(void *, begin,
               (THD * thd, const TABLE *table, size_t data_size, size_t memory,
                size_t num_threads));

/** Load a set of rows to SE table by one thread.
@param[in,out]  thd    session THD
@param[in,out]  ctx    SE load context returned by begin()
@param[in]      table  MySQL TABLE
@param[in]      sql_rows  row data to load
@param[in]      thread  current thread number
@param[in] wait_cbks wait stat callbacks
@return true if successful. */
DECLARE_METHOD(bool, load,
               (THD * thd, void *ctx, const TABLE *table,
                const Rows_mysql &sql_rows, size_t thread,
                Bulk_load::Stat_callbacks &wait_cbks));

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
@param[in,out]  thd    session THD
@param[in]      table  MySQL TABLE
@return true if table is supported. */
DECLARE_METHOD(bool, is_table_supported, (THD * thd, const TABLE *table));

/** Get available buffer pool memory for bulk load operations.
@param[in,out]  thd    session THD
@param[in]      table  MySQL TABLE
@return buffer pool memory available for bulk load. */
DECLARE_METHOD(size_t, get_se_memory_size, (THD * thd, const TABLE *table));

END_SERVICE_DEFINITION(bulk_data_load)
