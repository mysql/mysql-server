/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "sql_resultset.h"
#include "plugin_log.h"

Field_value::Field_value()
: is_unsigned(false), has_ptr(false)
{}

Field_value::Field_value(const Field_value& other):
    value(other.value),
    v_string_length(other.v_string_length),
    is_unsigned(other.is_unsigned),
    has_ptr(other.has_ptr)
{
  if (other.has_ptr)
  {
    copy_string(other.value.v_string, other.v_string_length);
  }
}

Field_value& Field_value::operator = (const Field_value& other)
{
  if (&other != this)
  {
    this->~Field_value();

    value = other.value;
    v_string_length = other.v_string_length;
    is_unsigned = other.is_unsigned;
    has_ptr = other.has_ptr;

    if (other.has_ptr)
    {
      copy_string(other.value.v_string, other.v_string_length);
    }
  }

  return *this;
}

Field_value::Field_value(const longlong &num, bool unsign)
{
  value.v_long = num;
  is_unsigned = unsign;
  has_ptr = false;
}

Field_value::Field_value(const double num)
{
  value.v_double = num;
  has_ptr = false;
}

Field_value::Field_value(const decimal_t &decimal)
{
  value.v_decimal = decimal;
  has_ptr = false;
}


Field_value::Field_value(const MYSQL_TIME &time)
{
  value.v_time = time;
  has_ptr = false;
}

void Field_value::copy_string(const char *str, size_t length)
{
  value.v_string = (char*)malloc(length + 1);
  if (value.v_string)
  {
    value.v_string[length] = '\0';
    memcpy(value.v_string, str, length);
    v_string_length = length;
    has_ptr = true;
  }
  else
  {
    log_message(MY_ERROR_LEVEL, "Error copying from empty string "); /* purecov: inspected */
  }
}

Field_value::Field_value(const char *str, size_t length)
{
  copy_string(str, length);
}


Field_value::~Field_value()
{
  if (has_ptr && value.v_string)
  {
    free(value.v_string);
  }
}

/** resultset class **/

void Sql_resultset::clear()
{
  while(!result_value.empty())
  {
    std::vector<Field_value*> fld_val= result_value.back();
    result_value.pop_back();
    while(!fld_val.empty())
    {
      Field_value *fld= fld_val.back();
      fld_val.pop_back();
      delete fld;
    }
  }
  result_value.clear();
  result_meta.clear();

  current_row= 0;
  num_cols= 0;
  num_rows= 0;
  num_metarow= 0;
  m_resultcs= NULL;
  m_server_status= 0;
  m_warn_count= 0;
  m_affected_rows= 0;
  m_last_insert_id= 0;
  m_sql_errno= 0;
  m_killed= false;
}


void Sql_resultset::new_row()
{
  result_value.push_back(std::vector< Field_value* >());
}


void Sql_resultset::new_field(Field_value *val)
{
  result_value[num_rows].push_back(val);
}


bool Sql_resultset::next()
{
  if (current_row < (int)num_rows - 1)
  {
    current_row++;
    return true;
  }

  return false;
}
