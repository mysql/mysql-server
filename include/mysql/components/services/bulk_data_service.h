/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include "field_types.h"
#include "sql-common/json_error_handler.h"

class THD;
struct TABLE;
struct CHARSET_INFO;
using Blob_context = void *;

/** The blob reference size.  Refer to lob::ref_t::SIZE or FIELD_REF_SIZE. */
constexpr size_t BLOB_REF_SIZE = 20;

struct Bulk_load_error_location_details {
  std::string filename;
  size_t row_number;
  std::string column_name;
  std::string column_type;
  std::string column_input_data;
  std::string m_error_mesg{};
  std::string m_table_name{};
  size_t m_bytes;
  size_t m_column_length;

  std::ostream &print(std::ostream &out) const;
};

inline std::ostream &Bulk_load_error_location_details::print(
    std::ostream &out) const {
  out << "[Bulk_load_error_location_details: filename=" << filename
      << ", column_name=" << column_name << "]";
  return out;
}

/** Overloading the global output operator to print objects of type
Bulk_load_error_location_details.
@param[in]  out  output stream
@param[in]  obj  object to be printed
@return given output stream. */
inline std::ostream &operator<<(std::ostream &out,
                                const Bulk_load_error_location_details &obj) {
  return obj.print(out);
}

struct Column_text {
  /** Column data. */
  const char *m_data_ptr{};

  /** Column data length. */
  size_t m_data_len{};

  /** Check if it is DB_ROW_ID column based on the value it contains.
  @return true if it is DB_ROW_ID column, false otherwise */
  bool is_row_id() const { return m_row_id != UINT64_MAX; }

  /** The generated DB_ROW_ID value */
  uint64_t m_row_id{UINT64_MAX};

  /** Mark the column to be null, by setting length to a special value. This is
  only used for columns whose state is maintained across chunks
  (aka fragmented columns). */
  void set_null() {
    assert(m_data_ptr == nullptr);
    m_data_len = std::numeric_limits<size_t>::max();
  }

  /** Check if the column is null, by checking special value for length.
  @return true if the column is null, false otherwise. */
  bool is_null() const {
    assert(m_data_len != std::numeric_limits<size_t>::max() ||
           m_data_ptr == nullptr);
    return m_data_len == std::numeric_limits<size_t>::max();
  }

  /** Check if the column data is stored externally.  If the data is stored
  externally, then the data length (m_data_len) would be equal to the
  BLOB_REF_SIZE and the column data (m_data_ptr) will contain the lob
  reference.
  @return true if data is stored externally, false otherwise. */
  bool is_ext() const {
    assert(!m_is_ext || m_data_len == BLOB_REF_SIZE);
    return m_is_ext;
  }

  /** Check if the column data is stored externally. It is called relaxed,
  because the column length might not be equal to BLOB_REF_SIZE.  Only to
  be used while the blob is being processed by the CSV parser.
  @return true if data is stored externally, false otherwise. */
  bool is_ext_relaxed() const {
    assert(!m_is_ext || m_data_len >= BLOB_REF_SIZE);
    return m_is_ext;
  }

  /** Mark that the column data has been stored externally. */
  void set_ext() {
    assert(m_data_len == BLOB_REF_SIZE);
    m_is_ext = true;
  }

  /** Initialize the members */
  void init() {
    m_data_ptr = nullptr;
    m_data_len = 0;
    m_is_ext = false;
    m_row_id = UINT64_MAX;
  }

  /** Print this object into the given output stream.
  @param[in] out  output stream into which this object will be printed.
  @return given output stream */
  std::ostream &print(std::ostream &out) const;

  std::string to_string() const;

 private:
  /** If true, the column data is stored externally. */
  bool m_is_ext{false};
};

inline std::string Column_text::to_string() const {
  std::ostringstream sout;
  sout << "[Column_text: len=" << m_data_len;
  sout << ", val=";

  if (m_data_ptr == nullptr) {
    sout << "nullptr";
  } else {
    for (size_t i = 0; i < m_data_len; ++i) {
      const char c = m_data_ptr[i];
      if (isalnum(c)) {
        sout << c;
      } else {
        sout << ".";
      }
    }
    sout << "[hex=";
    for (size_t i = 0; i < m_data_len; ++i) {
      sout << std::setfill('0') << std::setw(2) << std::hex
           << (int)*(&m_data_ptr[i]);
    }
  }
  sout << "]";
  return sout.str();
}

inline std::ostream &Column_text::print(std::ostream &out) const {
  out << "[Column_text: this=" << static_cast<const void *>(this)
      << ", m_data_ptr=" << static_cast<const void *>(m_data_ptr)
      << ", m_data_len=" << m_data_len << ", m_is_ext=" << m_is_ext << "]";
  return out;
}

/** Overloading the global output operator to print objects of type
Column_text.
@param[in]  out  output stream
@param[in]  obj  object to be printed
@return given output stream. */
inline std::ostream &operator<<(std::ostream &out, const Column_text &obj) {
  return obj.print(out);
}

struct Row_meta;

struct Column_mysql {
  /** Column Data Type */
  int16_t m_type{};

  /** Column data length. */
  uint16_t m_data_len{};

  /** If column is NULL. */
  bool m_is_null{false};

  char *get_data() const { return m_is_null ? nullptr : m_data_ptr; }

  void set_data(char *ptr) { m_data_ptr = ptr; }

  /** Save the beginning of the row pointer in this object.  This should be
  called only when the column is null.
  @param[in]  row_begin  pointer to beginning of row.*/
  void row(char *row_begin) {
    assert(m_is_null);
    m_data_len = 0;
    m_data_ptr = row_begin;
  }

  /** Get the pointer to the beginning of row. This is valid only if the
  column is null. This should be called on the first column of the row. There
  is no need to call this on other columns.
  @param[in]  row_meta meta data information about the row
  @param[in]  col_index  Index of the first column which is 0.
  @return pointer to row beginning. */
  char *get_row_begin(const Row_meta &row_meta,
                      size_t col_index [[maybe_unused]]) const;

  /** Column data in integer format. Used only for specific datatype. */
  uint64_t m_int_data;

  void init() {
    m_type = 0;
    m_data_len = 0;
    m_is_null = false;
    m_data_ptr = nullptr;
    m_int_data = 0;
  }

  std::string to_string() const;

 private:
  /** Column data or row begin.  There is a need to fetch the beginning of
  the row from the vector of Column_mysql.  But in the case of secondary
  indexes, all the keys could be null and it becomes impossible to obtain
  the pointer to beginning of the row.  To solve this problem, I am re-using
  this pointer to hold the row begin when the column is null.  So it becomes
  important to make use of m_is_null to check if the column is null. It is NOT
  correct to check this pointer against nullptr to confirm if column is null.*/
  char *m_data_ptr{nullptr};
};

inline std::string Column_mysql::to_string() const {
  std::ostringstream sout;
  sout << "[Column_mysql: type=" << m_type << ", len=" << m_data_len
       << ", m_int_data=" << m_int_data;
  sout << ", val=";

  switch (m_type) {
    case MYSQL_TYPE_LONG: {
      sout << m_int_data;
    } break;
    default: {
      for (size_t i = 0; i < m_data_len; ++i) {
        const char c = m_data_ptr[i];
        if (isalnum(c)) {
          sout << c;
        } else {
          sout << ".";
        }
      }

    } break;
  }
  if (m_type != MYSQL_TYPE_LONG) {
    sout << "[hex=";
    for (size_t i = 0; i < m_data_len; ++i) {
      sout << std::setfill('0') << std::setw(2) << std::hex
           << (int)*(&m_data_ptr[i]);
    }
    sout << "]";
  }
  return sout.str();
}

/** Implements the row and column memory management for parse and load
operations. We try to pre-allocate the memory contiguously as much as we can
to maximize the performance.

@tparam Column_type Column_text when used in the CSV context, Column_sql when
used in the InnoDB context.
*/
template <typename Column_type>
class Row_bunch {
 public:
  /** Create a new row bunch.
  @param[in]  n_cols  number of columns */
  explicit Row_bunch(size_t n_cols) : m_num_columns(n_cols) {}

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

  void reset() {
    for (auto &col : m_columns) {
      col.init();
    }
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

  /** Get column using row index and column index.
  @param[in]  row_index   index of the row in the bunch
  @param[in]  col_index   index of the column within row
  @return column data */
  Column_type &get_col(size_t row_index, size_t col_index) {
    return get_column(get_row_offset(row_index), col_index);
  }

  /** Get column using the column offset.
  @param[in]  col_offset  column offset
  @return column data */
  Column_type &get_col(size_t col_offset) { return m_columns[col_offset]; }

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
   * assume we can append a full additional row (which should have at most
   * 4096 columns). Rounded up to 600M. */
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

  std::string get_compare_string() const {
    switch (m_compare) {
      case Compare::INTEGER_SIGNED:
        return "INTEGER_SIGNED";
      case Compare::INTEGER_UNSIGNED:
        return "INTEGER_UNSIGNED";
      case Compare::BINARY:
        return "BINARY";
      case Compare::MYSQL:
        return "MYSQL";
    }
    assert(0);
    return "INVALID";
  }

  /** @return true if integer type. */
  bool is_integer() const {
    return (m_compare == Compare::INTEGER_SIGNED ||
            m_compare == Compare::INTEGER_UNSIGNED);
  }

  /** Based on the column data type check if it can be stored externally.
  @return true if the column data can be stored externally
  @return false if the column data cannot be stored externally */
  bool can_be_stored_externally() const;

  /** true if this column is part of secondary index. */
  bool m_is_part_of_sk{false};

  /** Field type. (@ref enum_field_types) */
  enum_field_types m_type;

  /** If column could be NULL. */
  bool m_is_nullable{false};

  /** true if column belongs to primary index (key or non-key) */
  bool m_is_pk{false};

  /** true if column is a key for primary or secondary index. */
  bool m_is_key{false};

  /** If the key is descending. */
  bool m_is_desc_key{false};

  /** If the key is prefix of the column. */
  bool m_is_prefix_key{false};

  /** If it is fixed length type. */
  bool m_is_fixed_len{false};

  /** If it is integer type. */
  Compare m_compare;

  /** If it is unsigned integer type. */
  bool m_is_unsigned{false};

  /** Check the row header to find out if it is fixed length. For
  character data type the row header indicates fixed length. */
  bool m_fixed_len_if_set_in_row{false};

  /** If character column length can be kept in one byte. */
  bool m_is_single_byte_len{false};

  /** The length of column data if fixed. */
  uint16_t m_fixed_len;

  /** Maximum length of data in bytes. */
  uint16_t m_max_len;

  /** Index of column in row. */
  uint16_t m_index;

  /** Position of column in table. Refer to Field::field_index() */
  uint16_t m_field_index{UINT16_MAX};

  /** Byte index in NULL bitmap. */
  uint16_t m_null_byte;

  /** BIT number in NULL bitmap. */
  uint16_t m_null_bit;

  /** Character set for char & varchar columns. */
  const void *m_charset;

  /** Field name */
  std::string m_field_name;

  /** Get a string representation of Column_meta object. Useful only for
  debugging purposes.
  @see Column_meta
  @return string representation of this object. */
  std::string to_string() const;

  /** Print this object into the given output stream.
  @param[in]  out  output stream into which object will be printed
  @return given output stream. */
  std::ostream &print(std::ostream &out) const;

  /** Get the data type of the column as a string.
  @return data type of the column as a string. */
  std::string get_type_string() const;
};

inline std::string Column_meta::get_type_string() const {
  switch (m_type) {
    case MYSQL_TYPE_DECIMAL:
      return "decimal";
    case MYSQL_TYPE_TINY:
      return "tiny";
    case MYSQL_TYPE_SHORT:
      return "short";
    case MYSQL_TYPE_LONG:
      return "long";
    case MYSQL_TYPE_FLOAT:
      return "float";
    case MYSQL_TYPE_DOUBLE:
      return "double";
    case MYSQL_TYPE_NULL:
      return "null";
    case MYSQL_TYPE_TIMESTAMP:
      return "timestamp";
    case MYSQL_TYPE_LONGLONG:
      return "longlong";
    case MYSQL_TYPE_INT24:
      return "int";
    case MYSQL_TYPE_DATE:
      return "date";
    case MYSQL_TYPE_TIME:
      return "time";
    case MYSQL_TYPE_DATETIME:
      return "datetime";
    case MYSQL_TYPE_YEAR:
      return "year";
    case MYSQL_TYPE_NEWDATE:
      return "date";
    case MYSQL_TYPE_VARCHAR:
      return "varchar";
    case MYSQL_TYPE_BIT:
      return "bit";
    case MYSQL_TYPE_TIMESTAMP2:
      return "timestamp";
    case MYSQL_TYPE_DATETIME2:
      return "datetime";
    case MYSQL_TYPE_TIME2:
      return "time";
    case MYSQL_TYPE_TYPED_ARRAY:
      return "typed_array";
    case MYSQL_TYPE_VECTOR:
      return "vector";
    case MYSQL_TYPE_INVALID:
      return "invalid";
    case MYSQL_TYPE_BOOL:
      return "bool";
    case MYSQL_TYPE_JSON:
      return "json";
    case MYSQL_TYPE_NEWDECIMAL:
      return "decimal";
    case MYSQL_TYPE_ENUM:
      return "enum";
    case MYSQL_TYPE_SET:
      return "set";
    case MYSQL_TYPE_TINY_BLOB:
      return "tiny_blob";
    case MYSQL_TYPE_MEDIUM_BLOB:
      return "medium_blob";
    case MYSQL_TYPE_LONG_BLOB:
      return "long_blob";
    case MYSQL_TYPE_BLOB:
      return "blob";
    case MYSQL_TYPE_VAR_STRING:
      return "var_string";
    case MYSQL_TYPE_STRING:
      return "string";
    case MYSQL_TYPE_GEOMETRY:
      return "geometry";
  }
  return "invalid";
}

inline bool Column_meta::can_be_stored_externally() const {
  switch (m_type) {
    case MYSQL_TYPE_JSON:
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_VECTOR:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB: {
      return true;
    }
    default:
      break;
  }
  return false;
}

inline std::string Column_meta::to_string() const {
  std::ostringstream out;
  out << "[Column_meta: m_type=" << get_type_string()
      << ", m_field_name=" << m_field_name << ", m_index=" << m_index
      << ", m_field_index=" << m_field_index
      << ", m_is_single_byte_len=" << m_is_single_byte_len
      << ", m_is_fixed_len=" << m_is_fixed_len
      << ", m_fixed_len=" << m_fixed_len << ", m_null_byte=" << m_null_byte
      << ", m_null_bit=" << m_null_bit << ", m_compare=" << get_compare_string()
      << ", m_is_desc_key=" << m_is_desc_key << ", m_is_key=" << m_is_key
      << ", m_max_len=" << m_max_len << ", m_is_prefix_key=" << m_is_prefix_key
      << "]";
  return out.str();
}

inline std::ostream &Column_meta::print(std::ostream &out) const {
  out << to_string();
  return out;
}

/** Overloading the global output operator to print objects of type
Column_meta.
@param[in]  out  output stream
@param[in]  obj  object to be printed
@return given output stream. */
inline std::ostream &operator<<(std::ostream &out, const Column_meta &obj) {
  return obj.print(out);
}

/** Table metadata. */
struct Table_meta {
  /** Number of keys/indexes the table has. */
  size_t m_n_keys;

  /** Key number of the primary key. */
  size_t m_keynr_pk;

  /** True if generated DB_ROW_ID is the pk. */
  bool dbrowid_is_pk{false};

  size_t min_row_id_value{0};
  size_t max_row_id_value{0};

  /** Table being bulk loaded. */
  std::string m_table_name;
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

  /** All columns in a row arranged as per col_index. */
  std::vector<const Column_meta *> m_columns_text_order;

  /** Get a string representation of this Row_meta object.
  @see Row_meta
  @return string representation of this object. */
  std::string to_string() const;

  /** Get the metadata of the given column.
  @param[in] col_index  position of the column in the index.
  @return metadata of the requested column. */
  const Column_meta &get_column_meta_index_order(size_t col_index) const {
    assert(col_index < m_columns.size());
    return m_columns[col_index];
  }

  /** Get the meta data of the column.
  @param[in]  col_index  the index of the column as it appears in CSV file.
  @return a reference to the column meta data.*/
  const Column_meta &get_column_meta(size_t col_index) const {
    assert(col_index < m_columns_text_order.size());
    assert(col_index == m_columns_text_order[col_index]->m_index);
    return *m_columns_text_order[col_index];
  }

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

  /** Number of columns that can be stored externally. */
  size_t m_n_blob_cols{0};

  /** Name of the key */
  std::string m_name;

  /** true if primary key, false if secondary key. */
  bool is_pk{false};

  /** true if DB_ROW_ID is the pk, false otherwise. */
  bool dbrowid_is_pk{false};
};

inline std::ostream &operator<<(std::ostream &os,
                                const Row_meta::Key_type &key_type) {
  switch (key_type) {
    case Row_meta::Key_type::ANY:
      os << "ANY";
      break;
    case Row_meta::Key_type::INT_SIGNED_ASC:
      os << "INT_SIGNED_ASC";
      break;
    case Row_meta::Key_type::INT:
      os << "INT";
      break;
  }
  return os;
}

inline std::string Row_meta::to_string() const {
  std::ostringstream out;
  out << "[Row_meta: m_name=" << m_name << ", m_num_columns=" << m_num_columns
      << ", m_keys=" << m_keys << ", m_non_keys=" << m_non_keys
      << ", m_key_length=" << m_key_length << ", m_key_type=" << m_key_type
      << ", m_approx_row_len=" << m_approx_row_len;
  for (auto &col_meta : m_columns) {
    out << col_meta.to_string() << ", ";
  }
  out << "]";
  return out.str();
}

inline char *Column_mysql::get_row_begin(const Row_meta &row_meta,
                                         size_t col_index
                                         [[maybe_unused]]) const {
  assert(m_is_null || col_index == 0);
  return m_is_null ? m_data_ptr
                   : (m_data_ptr - row_meta.m_first_key_len -
                      row_meta.m_header_length);
}

namespace Bulk_load {

class Json_serialization_error_handler final
    : public JsonSerializationErrorHandler {
 public:
  void KeyTooBig() const override;
  void ValueTooBig() const override;
  void TooDeep() const override;
  void InvalidJson() const override;
  void InternalError(const char *message) const override;
  bool CheckStack() const override;

  const char *c_str() const { return m_error.c_str(); }

  std::string get_error() const { return m_error; }

 private:
  mutable std::string m_error{};
};

inline void Json_serialization_error_handler::KeyTooBig() const {
  m_error = "Key is too big";
}

inline void Json_serialization_error_handler::ValueTooBig() const {
  m_error = "Value is too big";
}

inline void Json_serialization_error_handler::TooDeep() const {
  m_error = "JSON document has more nesting levels than supported";
}
inline void Json_serialization_error_handler::InvalidJson() const {
  m_error = "Invalid JSON value is encountered";
}
inline void Json_serialization_error_handler::InternalError(
    const char *message [[maybe_unused]]) const {
  m_error = message;
  m_error += " (Internal Error)";
}

inline bool Json_serialization_error_handler::CheckStack() const {
  return false;
}

/** Callbacks for collecting time statistics */
struct Stat_callbacks {
  /* Operation begin. */
  std::function<void()> m_fn_begin;
  /* Operation end. */
  std::function<void()> m_fn_end;
};

using Read_range =
    std::pair<std::optional<Rows_mysql>, std::optional<Rows_mysql>>;

struct Source_table_data {
  std::string schema;
  std::string table;
  Read_range range;
};

/** Contains the data needed for the ROW_ID generation for tables without
explicit primary key */
struct Row_id_context {
  /** Get an estimate of the number of rows to be handled by each thread.
  This will give the number of row ids to be generated by each thread.
  @return number of rows to be handled by each thread. */
  size_t get_rows_per_thread() const {
    size_t estimated_total_rows = m_total_size / m_avg_row_len.load();
    size_t min_total_rows = 1000;

    if (estimated_total_rows < min_total_rows) {
      estimated_total_rows = min_total_rows;
    }

    return (estimated_total_rows + m_n_loaders) / m_n_loaders;
  }

  /** Get the next available range for generating DB_ROW_ID. The range includes
  the begin value but excludes the end value.
  @return the range for row ids for exclusive use by the calling thread. */
  inline std::pair<uint64_t, uint64_t> get_next_rowid_range() const {
    std::unique_lock<std::mutex> lock(m_rowid_mutex);

    const uint64_t range_begin = m_next_rowid_range;
    m_next_rowid_range += get_rows_per_thread();

    return std::make_pair(range_begin, m_next_rowid_range);
  }

  void set_begin_rowid_value(size_t row_id) { m_next_rowid_range = row_id; }
  /* Total data size of CSV files (in bytes) */
  size_t m_total_size;
  /* Average row length, updated by the CSV parsing threads for each batch. */
  std::atomic<size_t> m_avg_row_len;
  /* Number of loaders aka concurrency in phase 1. */
  size_t m_n_loaders;

 private:
  /** Protects the member m_next_rowid_range */
  mutable std::mutex m_rowid_mutex;

  /* Number of rows per subtree. */
  mutable size_t m_next_rowid_range{0};
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

/** Get row metadata information for all the indexes.
@param[in,out]  thd       session THD
@param[in]      table     MySQL TABLE
@param[in]      have_key  include Primary Key metadata
@param[out]     metadata  Metadata for each of the indexes.
@return true if successful. */
DECLARE_METHOD(bool, get_row_metadata_all,
               (THD * thd, const TABLE *table, bool have_key,
                std::vector<Row_meta> &metadata));

/** Get table metadata information for the table being bulk loaded.
@param[in,out]  thd       session THD
@param[in]      table     MySQL TABLE
@param[out]     metadata  Metadata of the table.
@return true if successful. */
DECLARE_METHOD(bool, get_table_metadata,
               (THD * thd, const TABLE *table, Table_meta &metadata));

END_SERVICE_DEFINITION(bulk_data_convert)

/** Column metadata information. */
/* Bulk data load to SE. */
BEGIN_SERVICE_DEFINITION(bulk_data_load)
/** Begin Loading bulk data to SE.
@param[in,out]  thd          session THD
@param[in]      table        MySQL TABLE
@param[in]      keynr        key number, identifying the index being loaded.
@param[in]      data_size    total data size to load
@param[in]      memory       SE memory to be used
@param[in]      num_threads  Number of concurrent threads
@return SE bulk load context or nullptr in case of an error. */
DECLARE_METHOD(void *, begin,
               (THD * thd, const TABLE *table, size_t keynr, size_t data_size,
                size_t memory, size_t num_threads));

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

/** Create a blob context object to insert a blob.
@param[in,out]  thd    session THD
@param[in,out]  load_ctx    SE load context returned by begin()
@param[in]      table  MySQL TABLE
@param[out]     blob_ctx  a blob context object to insert a blob.
@param[out]     blobref   buffer to hold blob reference
@param[in]      thread  current thread number
@return true if successful. */
DECLARE_METHOD(bool, open_blob,
               (THD * thd, void *load_ctx, const TABLE *table,
                Blob_context &blob_ctx, unsigned char *blobref, size_t thread));

/** Write data into a blob
@param[in,out]  thd    session THD
@param[in,out]  load_ctx    SE load context returned by begin()
@param[in]      table  MySQL TABLE
@param[in]      blob_ctx  a blob context object to insert a blob.
@param[out]     blobref   buffer to hold blob reference
@param[in]      thread  current thread number
@param[in]      data  blob data to be written
@param[in]      data_len  length of blob data to be written (in bytes);
@return true if successful. */
DECLARE_METHOD(bool, write_blob,
               (THD * thd, void *load_ctx, const TABLE *table,
                Blob_context blob_ctx, unsigned char *blobref, size_t thread,
                const unsigned char *data, size_t data_len));

/** Close the blob
@param[in,out]  thd      session THD
@param[in,out]  load_ctx    SE load context returned by begin()
@param[in]      table    MySQL TABLE
@param[in]      blob_ctx  a blob context object to insert a blob.
@param[out]     blobref   buffer to hold blob reference
@param[in]      thread  current thread number
@return true if successful. */
DECLARE_METHOD(bool, close_blob,
               (THD * thd, void *load_ctx, const TABLE *table,
                Blob_context blob_ctx, unsigned char *blobref, size_t thread));

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

/** Copies data from existing table into the duplicated table during incremental
load. This is called after the bulk load component detects we reached the end of
the CSV input for the respective sub-loader and it signals that the loader
should now iterate through the remainded or the existing data in the original
table and migrate it.
@param[in,out]  ctx             SE load context
@param[in]      table           MySQL TABLE
@param[in]      thread          loader thread index
@param[in,out]  wait_cbks       wait stat callbacks
@return true if successful, false otherwise. */
DECLARE_METHOD(bool, copy_existing_data,
               (void *ctx, const TABLE *table, size_t thread,
                Bulk_load::Stat_callbacks &wait_cbks));

/** Sets the source table data (table name and key range boundaries) for all
loaders.
@param[in,out]  ctx                SE load context
@param[in]      table              MySQL TABLE
@param[in]      source_table_data  vector containing the source table data
@return true if successful, false otherwise. */
DECLARE_METHOD(
    bool, set_source_table_data,
    (void *ctx, const TABLE *table,
     const std::vector<Bulk_load::Source_table_data> &source_table_data));

END_SERVICE_DEFINITION(bulk_data_load)
