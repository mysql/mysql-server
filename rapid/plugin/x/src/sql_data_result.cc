/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "sql_data_result.h"
#include "ngs/memory.h"

xpl::Sql_data_result::Sql_data_result(Sql_data_context &context)
: m_field_index(0), m_context(context)
{
}


void xpl::Sql_data_result::disable_binlog()
{
  // save original value of binary logging
  query("SET @MYSQLX_OLD_LOG_BIN=@@SQL_LOG_BIN");
  // disable binary logging
  query("SET SESSION SQL_LOG_BIN=0;");
}

void xpl::Sql_data_result::restore_binlog()
{
  query("SET SESSION SQL_LOG_BIN=@MYSQLX_OLD_LOG_BIN;");
}

void xpl::Sql_data_result::query(const ngs::PFS_string &query)
{
  m_result_set.clear();

  m_field_index = 0;

  ngs::Error_code error = m_context.execute_sql_and_collect_results(query.data(), query.length(), m_field_types, m_result_set, m_result_info);

  if (error)
  {
    throw error;
  }

  m_row_index = m_result_set.begin();
}


void xpl::Sql_data_result::get_next_field(long &value)
{
  Field_value &field_value = validate_field_index_no_null(MYSQL_TYPE_LONGLONG);

  value = static_cast<long>(field_value.value.v_long);
}


void xpl::Sql_data_result::get_next_field(bool &value)
{
  Field_value &field_value = validate_field_index_no_null(MYSQL_TYPE_LONGLONG);

  value = field_value.value.v_long;
}


void xpl::Sql_data_result::get_next_field(std::string &value)
{
  validate_field_index(MYSQL_TYPE_VARCHAR, MYSQL_TYPE_STRING);

  Field_value *field_value = get_value();

  value = "";
  if (field_value && field_value->is_string)
    value = *field_value->value.v_string;
}


/*
NOTE: Commented for coverage. Uncomment when needed.

void xpl::Sql_data_result::get_next_field(const char * &value)
{
  validate_field_index(MYSQL_TYPE_VARCHAR);

  Field_value *field_value = get_value();

  if (field_value && field_value->is_string)
    value = field_value->value.v_string->c_str();
  else
    value = NULL;
}
*/


void xpl::Sql_data_result::get_next_field(char * &value)
{
  validate_field_index(MYSQL_TYPE_VARCHAR);

  Field_value *field_value = get_value();

  if (field_value && field_value->is_string)
    value = &(*field_value->value.v_string)[0];
  else
    value = NULL;
}


long xpl::Sql_data_result::statement_warn_count()
{
  return m_result_info.num_warnings;
}


xpl::Sql_data_result::Field_value *xpl::Sql_data_result::get_value()
{
  Callback_command_delegate::Field_value *result = (*m_row_index).fields[m_field_index++];

  return result;
}


bool xpl::Sql_data_result::next_row()
{
  ++m_row_index;
  m_field_index = 0;

  return m_result_set.end() != m_row_index;
}


xpl::Sql_data_result::Field_value &xpl::Sql_data_result::validate_field_index_no_null(const enum_field_types field_type)
{
  validate_field_index(field_type);

  Callback_command_delegate::Field_value *result = get_value();

  if (NULL == result)
  {
    throw ngs::Error(ER_DATA_OUT_OF_RANGE, "Null values received");
  }

  return *result;
}


void xpl::Sql_data_result::validate_field_index_common() const
{
  if (0 == m_result_set.size())
  {
    throw ngs::Error(ER_DATA_OUT_OF_RANGE, "Resultset doesn't contain data");
  }

  if (m_row_index == m_result_set.end())
  {
    throw ngs::Error(ER_DATA_OUT_OF_RANGE, "No more rows in resultset");
  }

  if (m_field_index >= (*m_row_index).fields.size())
  {
    throw ngs::Error(ER_DATA_OUT_OF_RANGE, "Field index of of range. Request index: %u, last index: %u", (unsigned int)m_field_index, (unsigned int)(*m_row_index).fields.size()- 1);
  }

  if (m_field_index >= m_field_types.size())
  {
    throw ngs::Error(ER_DATA_OUT_OF_RANGE, "Type field index of of range. Request index: %u, last index: %u", (unsigned int)m_field_index, (unsigned int)m_field_types.size() - 1);
  }
}


void xpl::Sql_data_result::validate_field_index(const enum_field_types field_type) const
{
  validate_field_index_common();

  if (m_field_types[m_field_index].type != field_type)
  {
    throw ngs::Error(ER_DATA_OUT_OF_RANGE, "Invalid column type. Request type: %u, last type: %u", (unsigned int)field_type, (unsigned int)m_field_types[m_field_index].type);
  }
}


void xpl::Sql_data_result::validate_field_index(const enum_field_types field_type1, const enum_field_types field_type2) const
{
  validate_field_index_common();

  if (m_field_types[m_field_index].type != field_type1 && m_field_types[m_field_index].type != field_type2)
  {
    throw ngs::Error(ER_DATA_OUT_OF_RANGE, "Invalid column type. Request types: %u and %u, last type: %u",
                     (unsigned int)field_type1, (unsigned int)field_type2, (unsigned int)m_field_types[m_field_index].type);
  }
}
