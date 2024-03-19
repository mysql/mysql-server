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

#include <cstddef>

#include "mysql/components/services/log_builtins.h"
#include "sql-common/my_decimal.h"
#include "sql/sql_class.h"
#include "sql/statement/statement.h"

Result_set::Result_set() {} /* purecov: inspected */

Result_set::Result_set(List<Row<value_t>> *rows_arg,
                       Row<Column_metadata> *fields, size_t column_count,
                       ulonglong affected_rows, ulonglong last_insert_id)
    : m_fields(fields),
      m_column_count(column_count),
      m_rows(rows_arg),
      m_next(nullptr),
      m_affected_row(affected_rows),
      m_last_insert_id(last_insert_id) {
  if (m_rows != nullptr) m_row_iterator.init(*m_rows);
}

Protocol_local_v2::Protocol_local_v2(THD *thd,
                                     Statement_handle *execute_statement)
    : m_execute_statement(execute_statement),
      m_result_set_mem_root(key_memory_prepared_statement_main_mem_root,
                            thd->variables.query_alloc_block_size),
      m_data_rows(nullptr),
      m_current_row(nullptr),
      m_current_column(nullptr),
      m_send_metadata(false),
      m_column_count(0),
      m_thd(thd) {}

void Protocol_local_v2::add_row_to_result_set() {
  DBUG_PRINT("Protocol_local_v2",
             ("m_current_row = %llx m_current_row_index = %d \n",
              (unsigned long long)m_current_row, m_current_row_index));
  if (m_result_set_capacity.has_capacity() == false) return;
  if (m_current_row != nullptr && m_current_row_index > m_data_rows->size()) {
    /* Add the old row to the result set */
    Row<value_t> *ed_row =
        new (&m_result_set_mem_root) Row(m_current_row, m_column_count);
    if (ed_row != nullptr)
      m_data_rows->push_back(ed_row, &m_result_set_mem_root);
  }
}

/**
  Add a NULL column to the current row.
*/

bool Protocol_local_v2::store_null() {
  if (m_current_column == nullptr) {
    /* start_row() failed to allocate memory. */
    return true; /* purecov: inspected */
  }
  *m_current_column = std::monostate{};
  ++m_current_column;
  return false;
}

template <typename T, typename V>
bool Protocol_local_v2::allocate_type(V *value) {
  if (m_current_column == nullptr) return true;

  *m_current_column = new (&m_result_set_mem_root) T;

  auto *pointer = std::get_if<T *>(m_current_column);
  if (pointer == nullptr || *pointer == nullptr) return true;

  **pointer = *(V *)value;
  ++m_current_column;
  m_result_set_capacity.add_bytes(sizeof(T));
  return false;
}

bool Protocol_local_v2::store_longlong(longlong value) {
  if (m_current_column == nullptr) return true;

  auto temp = m_fields->get_column(get_current_column_number());
  bool is_unsigned = (temp->flags & UNSIGNED_FLAG) != 0U;

  if (is_unsigned) {
    allocate_type<uint64_t>(&value);
  } else {
    allocate_type<int64_t>(&value);
  }
  return false;
}

/**
  Store a string value in a result set column, optionally
  having converted it to character_set_results.
*/

bool Protocol_local_v2::store_string(const char *str, size_t length,
                                     const CHARSET_INFO *src_cs,
                                     const CHARSET_INFO *dst_cs) {
  auto converted =
      convert_and_store(&m_result_set_mem_root, str, length, src_cs, dst_cs);

  if (converted.str == nullptr) return true;

  *m_current_column = converted;

  auto *pointer = std::get_if<LEX_CSTRING>(m_current_column);
  if (pointer == nullptr) return true;

  ++m_current_column;
  m_result_set_capacity.add_bytes(length);
  return false;
}

bool Protocol_local_v2::store_tiny(longlong value, uint32) {
  return store_longlong(value);
}

bool Protocol_local_v2::store_short(longlong value, uint32) {
  return store_longlong(value);
}

bool Protocol_local_v2::store_long(longlong value, uint32) {
  return store_longlong(value);
}

bool Protocol_local_v2::store_longlong(longlong value, bool, uint32) {
  return store_longlong(value);
}

/* purecov: begin inspected */
bool Protocol_local_v2::store_decimal(const my_decimal *value, uint prec,
                                      uint dec) {
  decimal d = {*value, prec, dec};
  allocate_type<decimal>(&d);
  return false;
}
/* purecov: end */

/** Convert to cs_results and store a string. */

bool Protocol_local_v2::store_string(const char *str, size_t length,
                                     const CHARSET_INFO *src_cs) {
  const CHARSET_INFO *dst_cs = src_cs;
  /*
  If the source charset is not binary and expected (destination) charset is
  set, then convert string to destination charset and store.
 */
  if (m_execute_statement->m_expected_charset != nullptr &&
      src_cs != &my_charset_bin)
    dst_cs = m_execute_statement->m_expected_charset;

  return store_string(str, length, src_cs, dst_cs);
}

bool Protocol_local_v2::store_temporal(const MYSQL_TIME &time) {
  allocate_type<MYSQL_TIME>(&time);
  return false;
}

bool Protocol_local_v2::store_datetime(const MYSQL_TIME &time, uint) {
  return store_temporal(time);
}

bool Protocol_local_v2::store_date(const MYSQL_TIME &time) {
  return store_temporal(time);
}

bool Protocol_local_v2::store_time(const MYSQL_TIME &time, uint) {
  return store_temporal(time);
}

bool Protocol_local_v2::store_floating_type(double value) {
  allocate_type<double>(&value);
  return false;
}

bool Protocol_local_v2::store_float(float value, uint32, uint32) {
  return store_floating_type(value);
}

bool Protocol_local_v2::store_double(double value, uint32, uint32) {
  return store_floating_type(value);
}

/* Store a Field. */

bool Protocol_local_v2::store_field(const Field *field) {
  // Do not store more data if capacity has been exceeded
  if (m_result_set_capacity.has_capacity() == false) return false;
  return field->send_to_protocol(this);
}

/** Called for statements that don't have a result set, at statement end. */

bool Protocol_local_v2::send_ok(uint, uint, ulonglong affected_rows,
                                ulonglong last_insert_id, const char *) {
  auto ed_result_set = static_cast<Result_set *>(nullptr);
  m_current_row = nullptr;

  ed_result_set = new (&m_result_set_mem_root)
      Result_set(nullptr, nullptr, 0, affected_rows, last_insert_id);

  m_data_rows = nullptr;
  m_fields = nullptr;

  if (ed_result_set == nullptr) return true;
  m_execute_statement->add_result_set(ed_result_set);

  m_column_count = 0;
  m_result_set_capacity.reset();
  return false;
}

/**
  Called at the end of a result set. Append a complete
  result set to the list.

  Don't send anything to the client, but instead finish
  building of the result set at hand.
*/

/** Called to send an error to the client at the end of a statement. */

bool Protocol_local_v2::send_error(uint, const char *, const char *) {
  /*
    Just make sure that nothing is sent to the client (default
    implementation).
  */
  m_column_count = 0;
  return false;
}

int Protocol_local_v2::read_packet() { return 0; /* purecov: inspected */ }

ulong Protocol_local_v2::get_client_capabilities() {
  return CLIENT_MULTI_RESULTS;
}

bool Protocol_local_v2::has_client_capability(unsigned long capability) {
  return CLIENT_MULTI_RESULTS & capability;
}

bool Protocol_local_v2::connection_alive() const {
  // Returns true if user connection is bound.
  return (m_thd->get_net()->vio != nullptr);
}

void Protocol_local_v2::end_partial_result_set() { /* purecov: inspected */
}

int Protocol_local_v2::shutdown(bool) { return 0; /* purecov: inspected */ }

/**
  Called between two result set rows.

  Prepare structures to fill result set rows.
  Unfortunately, we can't return an error here. If memory allocation
  fails, we'll have to return an error later. And so is done
  in methods such as @sa store_column().
*/
void Protocol_local_v2::start_row() {
  DBUG_TRACE;
  DBUG_PRINT("Protocol_local_v2",
             ("m_data_rows = %llx\n", (unsigned long long)m_data_rows));

  assert(alloc_root_inited(&m_result_set_mem_root));

  if (m_send_metadata) return;

  if (m_data_rows != nullptr &&
      m_current_row_index + 1 <= m_data_rows->size()) {
    /* Reuse row. */
    Row<value_t> *row = (*m_data_rows)[m_current_row_index];
    m_current_row = row->get_column_array();
    memset((void *)m_current_row, 0, sizeof(value_t) * m_column_count);
    m_current_column = m_current_row;

  } else {
    /* Start a new row. */
    m_current_row = static_cast<value_t *>(
        m_result_set_mem_root.Alloc(sizeof(value_t) * m_column_count));
    m_current_column = m_current_row;
  }

  m_current_row_index++;
}

/**
  Add the current row to the result set
*/
bool Protocol_local_v2::end_row() {
  DBUG_TRACE;
  DBUG_PRINT("Protocol_local_v2", ("m_send_metadata = %d\n", m_send_metadata));

  if (m_send_metadata) return false;

  assert(m_data_rows);
  add_row_to_result_set();
  m_current_row = nullptr;
  m_current_column = nullptr;

  return false;
}

uint Protocol_local_v2::get_rw_status() { return 0; /* purecov: inspected */ }

bool Protocol_local_v2::get_compression() { return false; }

char *Protocol_local_v2::get_compression_algorithm() { return nullptr; }

uint Protocol_local_v2::get_compression_level() { return 0; }

bool Protocol_local_v2::start_result_metadata(uint elements, uint,
                                              const CHARSET_INFO *) {
  DBUG_TRACE;
  DBUG_PRINT("Protocol_local_v2", ("elements = %d\n", elements));

  m_column_count = elements;

  // If this is the first time we receive metadata, we create a new row to
  // store metadata
  m_metadata_row = static_cast<Column_metadata *>(
      m_result_set_mem_root.Alloc(sizeof(Column_metadata) * m_column_count));
  m_current_metadata_column = m_metadata_row;

  m_send_metadata = true;
  m_data_rows = new (&m_result_set_mem_root) List<Row<value_t>>;
  return false;
}

bool Protocol_local_v2::end_result_metadata() {
  DBUG_TRACE;
  DBUG_PRINT("Protocol_local_v2",
             ("Got %lu columns in result\n", (unsigned long)m_column_count));

  m_send_metadata = false;
  m_fields = new (&m_result_set_mem_root) Row(m_metadata_row, m_column_count);
  m_metadata_row = nullptr;
  return false;
}

bool Protocol_local_v2::send_eof(uint, uint) {
  DBUG_TRACE;

  assert(m_data_rows);
  DBUG_PRINT("Protocol_local_v2",
             ("Result contains %d rows with current index %u\n",
              m_data_rows->size(), m_current_row_index));

  m_current_row = nullptr;
  m_current_column = nullptr;

  bool reuse_result_set = false;
  if (m_execute_statement->is_prepared_statement()) {
    reuse_result_set =
        dynamic_cast<Prepared_statement_handle *>(m_execute_statement)
            ->uses_cursor();
  }

  if (reuse_result_set == false || m_result_set == nullptr) {
    // Create last collected result_set
    m_result_set = new (&m_result_set_mem_root)
        Result_set(m_data_rows, m_fields, m_column_count, 0, 0);

    if (m_result_set == nullptr) return true;
  } else {
    // TODO: HCS-9478 only do this one time. If num_rows_per_fetch=1, every time
    // fetch is called, this is called.
    m_result_set->update(m_data_rows, m_fields, m_column_count, 0, 0);
  }

  if (reuse_result_set == true) {
    if (m_current_row_index < m_data_rows->size()) {
      List_iterator<Row<value_t>> it(*m_result_set);

      uint i = 0;
      while (it++) {
        if (i >= m_current_row_index) {
          it.remove();
        }
        i++;
      }
    }

    /*
      We reuse the m_fields, m_row_list buffers and m_column_count, when
      using Prepared statements.
      Multiple resultsets are not allowed, with prepared statements.
    */
    m_execute_statement->set_result_set(
        m_result_set);  // Always keep 1 resultset.

  } else {
    /*
      Link the created Ed_result_set instance into the list of connection
      result sets. Never fails.
    */
    m_execute_statement->add_result_set(
        m_result_set);  // Append the result sets.

    m_data_rows = nullptr;
    m_result_set = nullptr;
    m_fields = nullptr;
    m_metadata_row = nullptr;
    m_current_metadata_column = nullptr;
    m_column_count = 0;
  }
  m_current_row_index = 0;
  m_result_set_capacity.reset();

  return false;
}

bool Protocol_local_v2::send_field_metadata(Send_field *field,
                                            const CHARSET_INFO *cs) {
  DBUG_TRACE;
  DBUG_PRINT("Protocol_local_v2", ("Got '%s' field\n", field->col_name));

  if (m_current_metadata_column == nullptr) return true;

  // Note: since database, column and table name cannot contain \0
  // (https://dev.mysql.com/doc/refman/8.0/en/identifiers.html), strlen can be
  // used here.
  auto database_name = convert_and_store(
      &m_result_set_mem_root, field->db_name, strlen(field->db_name),
      system_charset_info, m_execute_statement->m_expected_charset);
  if (database_name.str == nullptr) return true;
  m_current_metadata_column->database_name = database_name.str;

  auto table_name = convert_and_store(
      &m_result_set_mem_root, field->table_name, strlen(field->table_name),
      system_charset_info, m_execute_statement->m_expected_charset);
  if (table_name.str == nullptr) return true;
  m_current_metadata_column->table_name = table_name.str;

  auto original_table_name =
      convert_and_store(&m_result_set_mem_root, field->org_table_name,
                        strlen(field->org_table_name), system_charset_info,
                        m_execute_statement->m_expected_charset);
  if (original_table_name.str == nullptr) return true;
  m_current_metadata_column->original_table_name = original_table_name.str;

  auto column_name = convert_and_store(
      &m_result_set_mem_root, field->col_name, strlen(field->col_name),
      system_charset_info, m_execute_statement->m_expected_charset);
  if (column_name.str == nullptr) return true;
  m_current_metadata_column->column_name = column_name.str;

  auto original_col_name = convert_and_store(
      &m_result_set_mem_root, field->org_col_name, strlen(field->org_col_name),
      system_charset_info, m_execute_statement->m_expected_charset);
  if (original_col_name.str == nullptr) return true;
  m_current_metadata_column->original_col_name = original_col_name.str;

  /*
   If there is no expected charset or if the source charset is binary, use the
   default charset specified by the source charset_info cs
  */
  if (m_execute_statement->m_expected_charset == nullptr ||
      cs == &my_charset_bin) {
    // Charset Number.
    m_current_metadata_column->charsetnr = cs->number;
    // Column Length.
    m_current_metadata_column->length = field->length;
  } else {
    // Charset Number.
    m_current_metadata_column->charsetnr =
        m_execute_statement->m_expected_charset->number;
    // Column Length.
    uint32 max_length =
        (field->type >= MYSQL_TYPE_TINY_BLOB && field->type <= MYSQL_TYPE_BLOB)
            ? field->length / cs->mbminlen
            : field->length / cs->mbmaxlen;
    m_current_metadata_column->length =
        (max_length * m_execute_statement->m_expected_charset->mbmaxlen);
  }

  m_current_metadata_column->flags = field->flags;
  m_current_metadata_column->decimals = field->decimals;
  m_current_metadata_column->type = field->type;

  DBUG_EXECUTE_IF("log_column_metadata", {
    char error_msg[1024];
    sprintf(error_msg,
            "Column_metadata: {Column Name:%s Type:%d Length: %lu, Flags: %u, "
            "Decimals: %u charsetnr:%u}",
            m_current_metadata_column->column_name,
            m_current_metadata_column->type, m_current_metadata_column->length,
            m_current_metadata_column->flags,
            m_current_metadata_column->decimals,
            m_current_metadata_column->charsetnr);
    LogErr(ERROR_LEVEL, ER_CONDITIONAL_DEBUG, error_msg);
  });

  ++m_current_metadata_column;
  return false;
}

void Protocol_local_v2::clear_resultset_mem_root() {
  m_result_set_mem_root.Clear();
  m_result_set = nullptr;
  m_data_rows = nullptr;
  m_fields = nullptr;
  m_current_row = nullptr;
  m_current_column = nullptr;
  m_metadata_row = nullptr;
  m_current_metadata_column = nullptr;
  m_column_count = 0;
}
