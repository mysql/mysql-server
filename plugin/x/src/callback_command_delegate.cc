/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#include "plugin/x/src/callback_command_delegate.h"

#include <stddef.h>

#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

Callback_command_delegate::Field_value::Field_value()
    : is_unsigned(false), is_string(false) {}

Callback_command_delegate::Field_value::Field_value(const Field_value &other)
    : value(other.value),
      is_unsigned(other.is_unsigned),
      is_string(other.is_string) {
  if (other.is_string) value.v_string = new std::string(*other.value.v_string);
}

/*
NOTE: Commented for coverage. Uncomment when needed.

Callback_command_delegate::Field_value& Callback_command_delegate::Field_value::operator = (const Field_value& other)
{
  if (&other != this)
  {
    this->~Field_value();

    value = other.value;
    is_unsigned = other.is_unsigned;
    is_string = other.is_string;

    if (other.is_string)
      value.v_string = new std::string(*other.value.v_string);
  }

  return *this;
}
*/

Callback_command_delegate::Field_value::Field_value(const longlong &num,
                                                    bool unsign) {
  value.v_long = num;
  is_unsigned = unsign;
  is_string = false;
}

Callback_command_delegate::Field_value::Field_value(const double num)
    : is_unsigned(false) {
  value.v_double = num;
  is_string = false;
}

Callback_command_delegate::Field_value::Field_value(const decimal_t &decimal)
    : is_unsigned(false) {
  value.v_decimal = decimal;
  is_string = false;
}

Callback_command_delegate::Field_value::Field_value(const MYSQL_TIME &time)
    : is_unsigned(false) {
  value.v_time = time;
  is_string = false;
}

Callback_command_delegate::Field_value::Field_value(const char *str,
                                                    size_t length)
    : is_unsigned(false) {
  value.v_string = new std::string(str, length);
  is_string = true;
}

Callback_command_delegate::Field_value::~Field_value() {
  if (is_string) delete value.v_string;
}

Callback_command_delegate::Row_data::~Row_data() { clear(); }

void Callback_command_delegate::Row_data::clear() {
  std::vector<Field_value *>::iterator i = fields.begin();

  for (; i != fields.end(); ++i) ngs::free_object(*i);

  fields.clear();
}

Callback_command_delegate::Row_data::Row_data(const Row_data &other) {
  clone_fields(other);
}

Callback_command_delegate::Row_data &Callback_command_delegate::Row_data::
operator=(const Row_data &other) {
  if (&other != this) {
    clear();
    clone_fields(other);
  }

  return *this;
}

void Callback_command_delegate::Row_data::clone_fields(const Row_data &other) {
  fields.reserve(other.fields.size());
  std::vector<Field_value *>::const_iterator i = other.fields.begin();
  for (; i != other.fields.end(); ++i) {
    this->fields.push_back((*i) ? ngs::allocate_object<Field_value>(**i)
                                : NULL);
  }
}

Callback_command_delegate::Callback_command_delegate() : m_current_row(NULL) {}

Callback_command_delegate::Callback_command_delegate(
    Start_row_callback start_row, End_row_callback end_row)
    : m_start_row(start_row), m_end_row(end_row), m_current_row(NULL) {}

void Callback_command_delegate::set_callbacks(Start_row_callback start_row,
                                              End_row_callback end_row) {
  m_start_row = start_row;
  m_end_row = end_row;
}

void Callback_command_delegate::reset() {
  m_current_row = NULL;
  Command_delegate::reset();
}

int Callback_command_delegate::start_row() {
  if (m_start_row) {
    m_current_row = m_start_row();
    if (!m_current_row) return true;
  } else
    m_current_row = NULL;
  return false;
}

int Callback_command_delegate::end_row() {
  if (m_end_row && !m_end_row(m_current_row)) return true;
  return false;
}

void Callback_command_delegate::abort_row() {}

ulong Callback_command_delegate::get_client_capabilities() {
  return CLIENT_DEPRECATE_EOF;
}

/****** Getting data ******/
int Callback_command_delegate::get_null() {
  try {
    if (m_current_row) m_current_row->fields.push_back(NULL);
  }
  catch (std::exception &e) {
    log_error("Error getting result data: %s", e.what());
    return true;
  }
  return false;
}

int Callback_command_delegate::get_integer(longlong value) {
  try {
    if (m_current_row)
      m_current_row->fields.push_back(ngs::allocate_object<Field_value>(value));
  }
  catch (std::exception &e) {
    log_error("Error getting result data: %s", e.what());
    return true;
  }
  return false;
}

int Callback_command_delegate::get_longlong(longlong value,
                                            uint unsigned_flag) {
  try {
    if (m_current_row)
      m_current_row->fields.push_back(
          ngs::allocate_object<Field_value>(value, unsigned_flag));
  }
  catch (std::exception &e) {
    log_error("Error getting result data: %s", e.what());
    return true;
  }
  return false;
}

int Callback_command_delegate::get_decimal(const decimal_t *value) {
  try {
    if (m_current_row)
      m_current_row->fields.push_back(
          ngs::allocate_object<Field_value>(*value));
  }
  catch (std::exception &e) {
    log_error("Error getting result data: %s", e.what());
    return true;
  }
  return false;
}

int Callback_command_delegate::get_double(double value, uint32) {
  try {
    if (m_current_row)
      m_current_row->fields.push_back(ngs::allocate_object<Field_value>(value));
  }
  catch (std::exception &e) {
    log_error("Error getting result data: %s", e.what());
    return true;
  }
  return false;
}

int Callback_command_delegate::get_date(const MYSQL_TIME *value) {
  try {
    if (m_current_row)
      m_current_row->fields.push_back(
          ngs::allocate_object<Field_value>(*value));
  }
  catch (std::exception &e) {
    log_error("Error getting result data: %s", e.what());
    return true;
  }
  return false;
}

int Callback_command_delegate::get_time(const MYSQL_TIME *value,
                                        uint) {
  try {
    if (m_current_row)
      m_current_row->fields.push_back(
          ngs::allocate_object<Field_value>(*value));
  }
  catch (std::exception &e) {
    log_error("Error getting result data: %s", e.what());
    return true;
  }
  return false;
}

int Callback_command_delegate::get_datetime(const MYSQL_TIME *value,
                                            uint) {
  try {
    if (m_current_row)
      m_current_row->fields.push_back(
          ngs::allocate_object<Field_value>(*value));
  }
  catch (std::exception &e) {
    log_error("Error getting result data: %s", e.what());
    return true;
  }
  return false;
}

int Callback_command_delegate::get_string(const char *const value,
                                          size_t length,
                                          const CHARSET_INFO *const) {
  try {
    if (m_current_row)
      m_current_row->fields.push_back(
          ngs::allocate_object<Field_value>(value, length));
  }
  catch (std::exception &e) {
    log_error("Error getting result data: %s", e.what());
    return true;
  }
  return false;
}

}  // namespace xpl
