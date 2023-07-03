/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/sql_service/sql_resultset.h"

#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"

Field_value::Field_value() : is_unsigned(false), has_ptr(false) {}

Field_value::Field_value(const Field_value &other)
    : value(other.value),
      v_string_length(other.v_string_length),
      is_unsigned(other.is_unsigned),
      has_ptr(other.has_ptr) {
  if (other.has_ptr) {
    copy_string(other.value.v_string, other.v_string_length);
  }
}

Field_value &Field_value::operator=(const Field_value &other) {
  if (&other != this) {
    this->~Field_value();

    value = other.value;
    v_string_length = other.v_string_length;
    is_unsigned = other.is_unsigned;
    has_ptr = other.has_ptr;

    if (other.has_ptr) {
      copy_string(other.value.v_string, other.v_string_length);
    }
  }

  return *this;
}

Field_value::Field_value(const longlong &num, bool unsign) {
  value.v_long = num;
  is_unsigned = unsign;
  has_ptr = false;
}

Field_value::Field_value(const double num) {
  value.v_double = num;
  has_ptr = false;
}

Field_value::Field_value(const decimal_t &decimal) {
  value.v_decimal = decimal;
  has_ptr = false;
}

Field_value::Field_value(const MYSQL_TIME &time) {
  value.v_time = time;
  has_ptr = false;
}

void Field_value::copy_string(const char *str, size_t length) {
  value.v_string = (char *)malloc(length + 1);
  if (value.v_string) {
    value.v_string[length] = '\0';
    memcpy(value.v_string, str, length);
    v_string_length = length;
    has_ptr = true;
  } else {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_COPY_FROM_EMPTY_STRING); /* purecov: inspected */
  }
}

Field_value::Field_value(const char *str, size_t length) {
  copy_string(str, length);
}

Field_value::~Field_value() {
  if (has_ptr && value.v_string) {
    free(value.v_string);
  }
}

/** resultset class **/

void Sql_resultset::clear() {
  while (!result_value.empty()) {
    std::vector<Field_value *> fld_val = result_value.back();
    result_value.pop_back();
    while (!fld_val.empty()) {
      Field_value *fld = fld_val.back();
      fld_val.pop_back();
      delete fld;
    }
  }
  result_value.clear();
  result_meta.clear();

  current_row = 0;
  num_cols = 0;
  num_rows = 0;
  num_metarow = 0;
  m_resultcs = nullptr;
  m_server_status = 0;
  m_warn_count = 0;
  m_affected_rows = 0;
  m_last_insert_id = 0;
  m_sql_errno = 0;
  m_killed = false;
}

void Sql_resultset::new_row() {
  result_value.push_back(std::vector<Field_value *>());
}

void Sql_resultset::new_field(Field_value *val) {
  result_value[num_rows].push_back(val);
}

bool Sql_resultset::next() {
  if (current_row < (int)num_rows - 1) {
    current_row++;
    return true;
  }

  return false;
}
