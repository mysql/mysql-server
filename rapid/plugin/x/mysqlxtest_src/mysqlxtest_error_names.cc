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

#include <stdexcept>
#include <stdlib.h>
#include <sstream>
#include "ngs_common/to_string.h"
#include "mysqlxtest_error_names.h"
#include "errmsg.h"

namespace mysqlxtest {

static Error_entry global_error_names[] =
{
  { "<No error>", (int)-1, "" },
  { "ER_SUCCESS", (int)0, "Success" },
#include <mysqld_ername.h>
#include "mysqlx_ername.h"
  { 0, 0, 0 }
};

namespace {

int try_to_interpret_text_as_error_code(const std::string &error_code_in_text_format) {
  if (error_code_in_text_format.empty())
    throw std::logic_error("Error text/code is empty");

  for(std::string::size_type i = 0; i < error_code_in_text_format.length(); ++i) {
    const char element = error_code_in_text_format[i];

    if (!isdigit(element)) {
      std::stringstream error_message;
      error_message << "Error text should contain error name or number (only digits) "
                    << "was expecting digit at position " << i << " but received "
                    << "'" << element << "'";
      throw std::logic_error(error_message.str());
    }
  }

  const int error_code = ngs::stoi(error_code_in_text_format.c_str());

  if (0 == error_code &&
      1 == error_code_in_text_format.length())
    return 0;

  // Ignore client error, we do not have description
  // for those
  if (error_code >= CR_ERROR_FIRST &&
      error_code <= CR_ERROR_LAST)
    return error_code;

  if (NULL == get_error_entry_by_id(error_code)) {
    throw std::logic_error("Error code is unknown, got " + ngs::to_string(error_code));
  }

  return error_code;
}

} // namespace

int get_error_code_by_text(const std::string &error_name_or_code) {
  if ('E' == error_name_or_code.at(0)) {
    const mysqlxtest::Error_entry* entry = mysqlxtest::get_error_entry_by_name(error_name_or_code);

    if (NULL == entry) {
      throw std::logic_error("Error name not found: \"" + error_name_or_code + "\"");
    }

    return entry->error_code;
  }

  return try_to_interpret_text_as_error_code(error_name_or_code);
}

const Error_entry *get_error_entry_by_id(const int error_code) {
  Error_entry *error = global_error_names;

  while (error->name) {
    if (error_code == error->error_code)
      return error;

    ++error;
  }

  return NULL;
}

const Error_entry *get_error_entry_by_name(const std::string &name) {
  Error_entry *error = global_error_names;

  while (error->name) {
    if (name == error->name)
      return error;

    ++error;
  }

  return NULL;
}

} //namespace mysqlxtest

