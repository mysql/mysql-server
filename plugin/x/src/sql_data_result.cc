/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/sql_data_result.h"

#include <algorithm>
#include <cstddef>

#include "plugin/x/src/ngs/memory.h"

namespace xpl {

Sql_data_result::Sql_data_result(iface::Sql_session *context)
    : m_field_index(0), m_context(context) {}

void Sql_data_result::disable_binlog() {
  // save original value of binary logging
  query("SET @MYSQLX_OLD_LOG_BIN=@@SQL_LOG_BIN");
  // disable binary logging
  query("SET SESSION SQL_LOG_BIN=0;");
}

void Sql_data_result::restore_binlog() {
  query("SET SESSION SQL_LOG_BIN=@MYSQLX_OLD_LOG_BIN;");
}

void Sql_data_result::query(const ngs::PFS_string &query) {
  m_field_index = 0;
  m_resultset.reset();
  ngs::Error_code error =
      m_context->execute(query.data(), query.length(), &m_resultset);

  if (error) throw error;

  m_row_index = m_resultset.get_row_list().begin();
}

void Sql_data_result::get_next_field(bool *value) {
  Field_value &field_value =
      validate_field_index_no_null({MYSQL_TYPE_LONGLONG});

  *value = field_value.value.v_long;
}

void Sql_data_result::get_next_field(std::string *value) {
  validate_field_index({MYSQL_TYPE_VARCHAR, MYSQL_TYPE_STRING,
                        MYSQL_TYPE_MEDIUM_BLOB, MYSQL_TYPE_BLOB,
                        MYSQL_TYPE_LONG_BLOB});

  Field_value *field_value = get_value();

  *value = "";
  if (field_value && field_value->is_string)
    *value = *field_value->value.v_string;
}

void Sql_data_result::get_next_field(char **value) {
  validate_field_index({MYSQL_TYPE_VARCHAR});

  Field_value *field_value = get_value();

  if (field_value && field_value->is_string)
    *value = &(*field_value->value.v_string)[0];
  else
    *value = nullptr;
}

bool Sql_data_result::next_row() {
  ++m_row_index;
  m_field_index = 0;

  return m_resultset.get_row_list().end() != m_row_index;
}

Sql_data_result::Field_value &Sql_data_result::validate_field_index_no_null(
    const std::vector<enum_field_types> &field_types) {
  validate_field_index(field_types);
  Field_value *result = get_value();
  if (nullptr == result)
    throw ngs::Error(ER_DATA_OUT_OF_RANGE, "Null values received");
  return *result;
}

void Sql_data_result::validate_field_index(
    const std::vector<enum_field_types> &field_types) const {
  if (0 == m_resultset.get_row_list().size())
    throw ngs::Error(ER_DATA_OUT_OF_RANGE, "Resultset doesn't contain data");

  if (m_row_index == m_resultset.get_row_list().end())
    throw ngs::Error(ER_DATA_OUT_OF_RANGE, "No more rows in resultset");

  if (m_field_index >= (*m_row_index).fields.size())
    throw ngs::Error(
        ER_DATA_OUT_OF_RANGE,
        "Field index of of range. Request index: %u, last index: %u",
        (unsigned int)m_field_index,
        (unsigned int)(*m_row_index).fields.size() - 1);

  const Collect_resultset::Field_types &rset_types =
      m_resultset.get_field_types();
  if (m_field_index >= rset_types.size())
    throw ngs::Error(
        ER_DATA_OUT_OF_RANGE,
        "Type field index of of range. Request index: %u, last index: %u",
        (unsigned int)m_field_index, (unsigned int)rset_types.size() - 1);

  if (std::find(field_types.begin(), field_types.end(),
                rset_types[m_field_index].type) == field_types.end())
    throw ngs::Error(ER_DATA_OUT_OF_RANGE,
                     "Invalid column type (%u) for index %u",
                     (unsigned int)rset_types[m_field_index].type,
                     (unsigned int)m_field_index);
}

}  // namespace xpl
