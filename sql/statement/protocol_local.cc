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

#include "sql/statement/protocol_local.h"

#include "sql-common/my_decimal.h"
#include "sql/field.h"
#include "sql/statement/ed_connection.h"

/***************************************************************************
 * Ed_result_set
 ***************************************************************************/
/**
  Initialize an instance of Ed_result_set.

  Instances of the class, as well as all result set rows, are
  always allocated in the memory root passed over as the third
  argument. In the constructor, we take over ownership of the
  memory root. It will be freed when the class is destroyed.

  sic: Ed_result_est is not designed to be allocated on stack.
*/

Ed_result_set::Ed_result_set(List<Ed_row> *rows_arg, Ed_row *fields,
                             size_t column_count_arg, MEM_ROOT *mem_root_arg)
    : m_mem_root(std::move(*mem_root_arg)),
      m_column_count(column_count_arg),
      m_rows(rows_arg),
      m_fields(fields),
      m_next_rset(nullptr) {}

/***************************************************************************
 * Ed_result_set
 ***************************************************************************/

/*************************************************************************
 * Protocol_local
 **************************************************************************/

Protocol_local::Protocol_local(THD *thd, Ed_connection *ed_connection)
    : m_connection(ed_connection),
      m_rset(nullptr),
      m_column_count(0),
      m_current_row(nullptr),
      m_current_column(nullptr),
      m_send_metadata(false),
      m_thd(thd) {}

/**
  A helper function to add the current row to the current result
  set. Called in @sa start_row(), when a new row is started,
  and in send_eof(), when the result set is finished.
*/

void Protocol_local::opt_add_row_to_rset() {
  if (m_current_row) {
    /* Add the old row to the result set */
    Ed_row *ed_row = new (&m_rset_root) Ed_row(m_current_row, m_column_count);
    if (ed_row) m_rset->push_back(ed_row, &m_rset_root);
  }
}

/**
  Add a NULL column to the current row.
*/

bool Protocol_local::store_null() {
  if (m_current_column == nullptr)
    return true; /* start_row() failed to allocate memory. */

  memset(m_current_column, 0, sizeof(*m_current_column));
  ++m_current_column;
  return false;
}

/**
  A helper method to add any column to the current row
  in its binary form.

  Allocates memory for the data in the result set memory root.
*/

bool Protocol_local::store_column(const void *data, size_t length) {
  if (m_current_column == nullptr)
    return true; /* start_row() failed to allocate memory. */

  m_current_column->str = new (&m_rset_root) char[length + 1];
  if (!m_current_column->str) return true;
  memcpy(m_current_column->str, data, length);
  m_current_column->str[length + 1] = '\0'; /* Safety */
  m_current_column->length = length;
  ++m_current_column;
  return false;
}

/**
  Store a string value in a result set column, optionally
  having converted it to character_set_results.
*/

bool Protocol_local::store_string(const char *str, size_t length,
                                  const CHARSET_INFO *src_cs,
                                  const CHARSET_INFO *dst_cs) {
  /* Store with conversion */
  String convert;
  uint error_unused;

  if (dst_cs && !my_charset_same(src_cs, dst_cs) && src_cs != &my_charset_bin &&
      dst_cs != &my_charset_bin) {
    if (convert.copy(str, length, src_cs, dst_cs, &error_unused)) return true;
    str = convert.ptr();
    length = convert.length();
  }

  if (m_current_column == nullptr)
    return true; /* start_row() failed to allocate memory. */

  m_current_column->str = strmake_root(&m_rset_root, str, length);
  if (!m_current_column->str) return true;
  m_current_column->length = length;
  ++m_current_column;
  return false;
}

/** Store a tiny int as is (1 byte) in a result set column. */

bool Protocol_local::store_tiny(longlong value, uint32) {
  char v = (char)value;
  return store_column(&v, 1);
}

/** Store a short as is (2 bytes, host order) in a result set column. */

bool Protocol_local::store_short(longlong value, uint32) {
  int16 v = (int16)value;
  return store_column(&v, 2);
}

/** Store a "long" as is (4 bytes, host order) in a result set column.  */

bool Protocol_local::store_long(longlong value, uint32) {
  int32 v = (int32)value;
  return store_column(&v, 4);
}

/** Store a "longlong" as is (8 bytes, host order) in a result set column. */

bool Protocol_local::store_longlong(longlong value, bool, uint32) {
  int64 v = (int64)value;
  return store_column(&v, 8);
}

/** Store a decimal in string format in a result set column */

bool Protocol_local::store_decimal(const my_decimal *value, uint prec,
                                   uint dec) {
  StringBuffer<DECIMAL_MAX_STR_LENGTH> str;
  const int rc = my_decimal2string(E_DEC_FATAL_ERROR, value, prec, dec, &str);

  if (rc) return true;

  return store_column(str.ptr(), str.length());
}

/** Convert to cs_results and store a string. */

bool Protocol_local::store_string(const char *str, size_t length,
                                  const CHARSET_INFO *src_cs) {
  const CHARSET_INFO *dst_cs;

  dst_cs = m_connection->m_thd->variables.character_set_results;
  return store_string(str, length, src_cs, dst_cs);
}

/* Store MYSQL_TIME (in binary format) */

bool Protocol_local::store_datetime(const MYSQL_TIME &time, uint) {
  return store_column(&time, sizeof(MYSQL_TIME));
}

/** Store MYSQL_TIME (in binary format) */

bool Protocol_local::store_date(const MYSQL_TIME &time) {
  return store_column(&time, sizeof(MYSQL_TIME));
}

/** Store MYSQL_TIME (in binary format) */

bool Protocol_local::store_time(const MYSQL_TIME &time, uint) {
  return store_column(&time, sizeof(MYSQL_TIME));
}

/* Store a floating point number, as is. */

bool Protocol_local::store_float(float value, uint32, uint32) {
  return store_column(&value, sizeof(float));
}

/* Store a double precision number, as is. */

bool Protocol_local::store_double(double value, uint32, uint32) {
  return store_column(&value, sizeof(double));
}

/* Store a Field. */

bool Protocol_local::store_field(const Field *field) {
  return field->send_to_protocol(this);
}

/** Called for statements that don't have a result set, at statement end. */

bool Protocol_local::send_ok(uint, uint, ulonglong, ulonglong, const char *) {
  /*
    Just make sure nothing is sent to the client, we have grabbed
    the status information in the connection Diagnostics Area.
  */
  m_column_count = 0;
  return false;
}

/**
  Called at the end of a result set. Append a complete
  result set to the list in Ed_connection.

  Don't send anything to the client, but instead finish
  building of the result set at hand.
*/

bool Protocol_local::send_eof(uint, uint) {
  Ed_result_set *ed_result_set;

  assert(m_rset);
  m_current_row = nullptr;

  ed_result_set = new (&m_rset_root)
      Ed_result_set(m_rset, m_fields, m_column_count, &m_rset_root);

  m_rset = nullptr;
  m_fields = nullptr;

  if (!ed_result_set) return true;

  /*
    Link the created Ed_result_set instance into the list of connection
    result sets. Never fails.
  */
  m_connection->add_result_set(ed_result_set);
  m_column_count = 0;
  return false;
}

/** Called to send an error to the client at the end of a statement. */

bool Protocol_local::send_error(uint, const char *, const char *) {
  /*
    Just make sure that nothing is sent to the client (default
    implementation).
  */
  m_column_count = 0;
  return false;
}

int Protocol_local::read_packet() { return 0; }

ulong Protocol_local::get_client_capabilities() { return 0; }

bool Protocol_local::has_client_capability(unsigned long) { return false; }

bool Protocol_local::connection_alive() const { return false; }

void Protocol_local::end_partial_result_set() {}

int Protocol_local::shutdown(bool) { return 0; }

/**
  Called between two result set rows.

  Prepare structures to fill result set rows.
  Unfortunately, we can't return an error here. If memory allocation
  fails, we'll have to return an error later. And so is done
  in methods such as @sa store_column().
*/
void Protocol_local::start_row() {
  DBUG_TRACE;

  if (m_send_metadata) return;
  assert(alloc_root_inited(&m_rset_root));

  /* Start a new row. */
  m_current_row =
      (Ed_column *)m_rset_root.Alloc(sizeof(Ed_column) * m_column_count);
  m_current_column = m_current_row;
}

/**
  Add the current row to the result set
*/
bool Protocol_local::end_row() {
  DBUG_TRACE;
  if (m_send_metadata) return false;

  assert(m_rset);
  opt_add_row_to_rset();
  m_current_row = nullptr;

  return false;
}

uint Protocol_local::get_rw_status() { return 0; }

bool Protocol_local::start_result_metadata(uint elements, uint,
                                           const CHARSET_INFO *) {
  m_column_count = elements;
  start_row();
  m_send_metadata = true;
  m_rset = new (&m_rset_root) List<Ed_row>;
  return false;
}

bool Protocol_local::end_result_metadata() {
  m_send_metadata = false;
  m_fields = new (&m_rset_root) Ed_row(m_current_row, m_column_count);
  m_current_row = nullptr;
  return false;
}

bool Protocol_local::send_field_metadata(Send_field *field,
                                         const CHARSET_INFO *cs) {
  store_string(field->col_name, strlen(field->col_name), cs);
  return false;
}

bool Protocol_local::get_compression() { return false; }

char *Protocol_local::get_compression_algorithm() { return nullptr; }

uint Protocol_local::get_compression_level() { return 0; }

int Protocol_local::get_command(COM_DATA *, enum_server_command *) {
  return -1;
}
