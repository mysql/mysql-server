/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef PROTOCOL_LOCAL_V2_H
#define PROTOCOL_LOCAL_V2_H

#include <variant>

#include "my_sys.h"
#include "mysqld_error.h"
#include "sql-common/my_decimal.h"
#include "sql/field.h"
#include "sql/protocol.h"
#include "sql/sql_class.h"

class Statement_handle;

/**
 * @brief A structure to store a decimal value together with its precision and
 * number of decimals
 * TODO: HCS-10094 - Do we really need this struct?
 */
struct decimal {
  my_decimal decimal;
  uint prec;
  uint dec;
};

/**
 * @brief A type to store the value of a Column which can be one of all the
 * types that are supported by the Protocol_local_v2. Variant of pointers is
 * used instead of variant of values as the values are stored and managed by
 * mem_root. The pointers point to these values on mem_root.
 */
using value_t = std::variant<std::monostate, int64_t *, uint64_t *, double *,
                             MYSQL_TIME *, LEX_CSTRING, decimal *>;

/**
 * @brief The metadata of a Column
 *        Need to keep in-sync with Send_field class.
 *
 */
struct Column_metadata {
  const char *database_name;
  const char *table_name, *original_table_name;
  const char *column_name, *original_col_name;
  ulong length;
  uint charsetnr, flags, decimals;
  enum_field_types type;
};

/**
 * @brief A structure to store all information about a warning/error
 *
 */
struct Warning {
  Warning(const uint32 level, const uint32_t code, LEX_CSTRING message)
      : m_level(level), m_code(code), m_message(message) {}

  uint32_t m_level;
  uint32_t m_code;
  LEX_CSTRING m_message;
};

/**
 * @brief A row of result or a row of metadata
 *        A row is a collection of Column values or Column metadata.
 *
 * @tparam T type of the result set
 */
template <typename T>
class Row {
 private:
  T *m_column_array;
  size_t m_column_count; /* TODO: HCS-9478 change to point to metadata */

 public:
  const T &operator[](const unsigned int column_index) const {
    return *get_column(column_index);
  }
  T *get_column(const unsigned int column_index) const {
    assert(column_index < size());
    return m_column_array + column_index;
  }
  size_t size() const { return m_column_count; }

  Row(T *column_array_arg, size_t column_count_arg)
      : m_column_array(column_array_arg), m_column_count(column_count_arg) {}

  T *get_column_array() { return m_column_array; }
};

/**
 * @brief This class is used to limit the bytes collected in Result_set.
 * this is required to avoid exhausting MEM_ROOT, when fetch large set of rows.
 */
class Result_set_capacity {
 private:
  // Max capacity in bytes
  static constexpr auto MAX_CAPACITY{500};

  // Configured capacity in bytes
  uint64_t m_configured_capacity{MAX_CAPACITY};

  uint64_t m_current_capacity{0};

 public:
  // Add bytes to capacity.
  // Note: since add_bytes is called after calling new in the store_ functions,
  // the bytes have been allocated but they might not be shown to the end-users.
  // See add_row_to_result_set
  void add_bytes(uint64_t amend) {
    m_current_capacity += amend;

    if (!has_capacity()) my_error(ER_RESULT_SIZE_LIMIT_EXCEEDED, MYF(0));
  }

  // Check if the result set can have more bytes.
  bool has_capacity() { return m_current_capacity < m_configured_capacity; }

  // Reset the limit. When we are done fetching all rows.
  void reset() { m_current_capacity = 0; }

  void set_capacity(uint64_t cap) { m_configured_capacity = cap; }

  uint64_t get_capacity() { return m_configured_capacity; }
};

/**
 * @brief A result set contains the result of a query. It is a collection of
 *        Rows and its metadata
 *
 */
class Result_set {
 private:
  // Metadata
  Row<Column_metadata> *m_fields{nullptr};
  size_t m_column_count{0};

  // Rows
  List<Row<value_t>> *m_rows{nullptr};

  // Row iterator
  List_iterator_fast<Row<value_t>> m_row_iterator;

  // Next result set
  Result_set *m_next{nullptr};

  ulonglong m_affected_row{0};
  ulonglong m_last_insert_id{0};

  // Avoid copying the object.
  Result_set(const Result_set &) = delete;
  Result_set &operator=(Result_set &) = delete;
  Result_set(const Result_set &&) = delete;
  Result_set &operator=(Result_set &&) = delete;

 public:
  Result_set();

  Result_set(List<Row<value_t>> *rows_arg, Row<Column_metadata> *fields,
             size_t column_count, ulonglong affected_rows,
             ulonglong last_insert_id);

  ~Result_set() = default;

  /**
   * @brief Get the next row
   *
   * @return void*
   */
  Row<value_t> *get_next_row() {
    return static_cast<Row<value_t> *>(m_row_iterator.next());
  }
  /**
   * @brief Check if the iterator is at the last row
   *
   * @return true
   * @return false
   */
  bool is_last_row() { return m_row_iterator.is_last(); }

  /**
   * @brief Get the num affected row count
   *
   * @return uint
   */
  ulonglong get_num_affected_rows() const { return m_affected_row; }

  /**
   * @brief Get the last insert id
   *
   * @return uint
   */
  ulonglong get_last_insert_id() const { return m_last_insert_id; }

  /**
   * @brief Get the list of rows from result set
   *
   * @return List<Row<value_t>>*
   */
  List<Row<value_t>> *get_rows() { return m_rows; }

  /**
   * @brief Get reference for rows in results set.
   *
   * @return List<Row<value_t>> &
   */
  operator List<Row<value_t>> &() { return *m_rows; }

  /**
   * @brief Get number for rows in result set.
   *
   * @return unsigned int
   */
  unsigned int size() const { return m_rows->elements; }

  /**
   * @brief Get the field metadata
   *
   * @return Row<Column_metadata>*
   */
  Row<Column_metadata> *get_fields() { return m_fields; }

  /**
   * @brief Get the field count in result set.
   *
   * @return size_t
   */
  size_t get_field_count() const { return m_column_count; }

  /**
   * @brief Custom memory allocation for Result set object.
   *
   * @param size size of the memory
   * @param mem_root mem root object pointer
   * @return void*
   */
  static void *operator new(size_t size, MEM_ROOT *mem_root,
                            const std::nothrow_t & = std::nothrow) noexcept {
    return mem_root->Alloc(size);
  }

  static void operator delete(void *) noexcept {}

  static void operator delete(void *, MEM_ROOT *,
                              const std::nothrow_t &) noexcept {
    /* purecov: inspected */
  }

  /**
   * @brief Check if we have more than one result set.
   *
   * @return true
   * @return false
   */
  bool has_next() { return m_next != nullptr; }

  /**
   * @brief Get the next result set object
   *
   * @return Result_set*
   */
  Result_set *get_next() { return m_next; }

  /**
   * @brief Set the current result set pointer.
   * Generally used when traversing through all result set.
   *
   * @param ptr pointer to result set
   */
  void set_next(Result_set *ptr) { m_next = ptr; }

  // TODO: HCS-9478 remove this method after the hcs is resolved
  /**
   * @brief Update the members of result set
   *
   */
  void update(List<Row<value_t>> *rows_arg, Row<Column_metadata> *fields,
              size_t column_count, ulonglong affected_rows,
              ulonglong last_insert_id) {
    m_rows = rows_arg;
    m_row_iterator.init(*m_rows);
    m_fields = fields;
    m_column_count = column_count;
    m_affected_row = affected_rows;
    m_last_insert_id = last_insert_id;
  }
};

/**
 * @brief This is extention of Protocol_local. We support reading field
 * metadata and field data attributes.
 */
class Protocol_local_v2 final : public Protocol {
 private:
  bool store_string(const char *str, size_t length, const CHARSET_INFO *src_cs,
                    const CHARSET_INFO *dst_cs);
  bool store_longlong(longlong value);
  bool store_floating_type(double value);
  bool store_temporal(const MYSQL_TIME &time);
  void add_row_to_result_set();

  Statement_handle *m_execute_statement;

  MEM_ROOT m_result_set_mem_root;

  // Memory for following data members are allocated in above MEM_ROOT.
  Result_set *m_result_set = nullptr;

  // TODO: HCS-9478: These are duplicate members as similar as in Result_set
  // class. we can probably use object of Result_set here, instead.
  List<Row<value_t>> *m_data_rows;
  Row<Column_metadata> *m_fields;

  value_t *m_current_row;
  value_t *m_current_column;

  Column_metadata *m_metadata_row;
  Column_metadata *m_current_metadata_column;

  // End of MEM_ROOT members.

  // Whether we are receiving metadata or data
  bool m_send_metadata;
  size_t m_column_count;

  THD *m_thd;

  // Max rows in resultset
  uint m_current_row_index = 0;

  Result_set_capacity m_result_set_capacity;

  template <typename T, typename V>
  bool allocate_type(V *value);

 protected:
  bool store_null() override;
  bool store_tiny(longlong from, uint32) override;
  bool store_short(longlong from, uint32) override;
  bool store_long(longlong from, uint32) override;
  bool store_longlong(longlong from, bool unsigned_flag, uint32) override;
  bool store_decimal(const my_decimal *, uint, uint) override;
  bool store_string(const char *from, size_t length,
                    const CHARSET_INFO *cs) override;
  bool store_datetime(const MYSQL_TIME &time, uint precision) override;
  bool store_date(const MYSQL_TIME &time) override;
  bool store_time(const MYSQL_TIME &time, uint precision) override;
  bool store_float(float value, uint32 decimals, uint32 zerofill) override;
  bool store_double(double value, uint32 decimals, uint32 zerofill) override;
  bool store_field(const Field *field) override;

  enum enum_protocol_type type() const override { return PROTOCOL_LOCAL; }
  enum enum_vio_type connection_type() const override {
    return VIO_TYPE_LOCAL; /* purecov: inspected */
  }

  bool send_ok(uint server_status, uint statement_warn_count,
               ulonglong affected_rows, ulonglong last_insert_id,
               const char *message) override;

  bool send_eof(uint server_status, uint statement_warn_count) override;
  bool send_error(uint sql_errno, const char *err_msg,
                  const char *sqlstate) override;

  uint get_current_column_number() {
    if (!m_current_column) return 0;
    assert(m_current_row);
    return m_current_column - m_current_row;
  }

 public:
  Protocol_local_v2(THD *thd, Statement_handle *execute_statement);
  ~Protocol_local_v2() override { m_result_set_mem_root.Clear(); }

  int read_packet() override;

  int get_command(COM_DATA *, enum_server_command *) override {
    return -1; /* purecov: inspected */
  }
  ulong get_client_capabilities() override;
  bool has_client_capability(unsigned long client_capability) override;
  void end_partial_result_set() override;
  int shutdown(bool server_shutdown = false) override;
  bool connection_alive() const override;
  void start_row() override;
  bool end_row() override;
  void abort_row() override {}
  uint get_rw_status() override;
  bool get_compression() override;

  char *get_compression_algorithm() override;
  uint get_compression_level() override;

  bool start_result_metadata(uint num_cols, uint flags,
                             const CHARSET_INFO *resultcs) override;
  bool end_result_metadata() override;
  bool send_field_metadata(Send_field *field,
                           const CHARSET_INFO *charset) override;
  bool flush() override { return true; /* purecov: inspected */ }
  bool send_parameters(List<Item_param> *, bool) override { return false; }
  bool store_ps_status(ulong, uint, uint, ulong) override {
    return false; /* purecov: inspected */
  }

  /**
   * @brief Set the capacity in bytes allowed for caching results.
   *
   * @param capacity capacity of the result set
   */
  void set_result_set_capacity(size_t capacity) {
    m_result_set_capacity.set_capacity(capacity);
  }

  /**
   * @brief Get the capacity in bytes allowed for caching results.
   */
  size_t get_result_set_capacity() {
    return m_result_set_capacity.get_capacity();
  }

  /**
   * @brief Clear MEM_ROOT and related members.
   *
   */
  void clear_resultset_mem_root();
};

#endif
