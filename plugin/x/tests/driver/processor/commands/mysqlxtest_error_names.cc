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

#include "plugin/x/tests/driver/processor/commands/mysqlxtest_error_names.h"

#include <stdlib.h>
#include <sstream>
#include <stdexcept>

#include "errmsg.h"
#include "plugin/x/client/mysqlxclient/mysqlxclient_error.h"
#include "plugin/x/ngs/include/ngs_common/to_string.h"

namespace mysqlxtest {

static Error_entry global_error_names[] = {
    {"<No error>", static_cast<int>(-1), "", NULL, NULL, 0},
    {"ER_SUCCESS", static_cast<int>(0), "Success", NULL, NULL, 0},
#ifndef IN_DOXYGEN
#include <mysqld_ername.h>

#include "plugin/x/generated/mysqlx_ername.h"
#endif /* IN_DOXYGEN */
    {0, 0, 0, NULL, NULL, 0}};

namespace {

int try_to_interpret_text_as_error_code(
    const std::string &error_code_in_text_format) {
  if (error_code_in_text_format.empty())
    throw std::logic_error("Error text/code is empty");

  for (std::string::size_type i = 0; i < error_code_in_text_format.length();
       ++i) {
    const char element = error_code_in_text_format[i];

    if (!isdigit(element)) {
      std::stringstream error_message;
      error_message
          << "Error text should contain error name or number (only digits) "
          << "was expecting digit at position " << i << " but received "
          << "'" << element << "'";
      throw std::logic_error(error_message.str());
    }
  }

  const int error_code = ngs::stoi(error_code_in_text_format.c_str());

  if (0 == error_code && 1 == error_code_in_text_format.length()) return 0;

  // Ignore client error, we do not have description
  // for those
  if (error_code >= CR_ERROR_FIRST && error_code <= CR_ERROR_LAST)
    return error_code;

  if (error_code >= CR_X_ERROR_FIRST && error_code <= CR_X_ERROR_LAST)
    return error_code;

  if (NULL == get_error_entry_by_id(error_code)) {
    throw std::logic_error("Error code is unknown, got " +
                           ngs::to_string(error_code));
  }

  return error_code;
}

}  // namespace

int get_error_code_by_text(const std::string &error_name_or_code) {
  if ('E' == error_name_or_code.at(0)) {
    const mysqlxtest::Error_entry *entry =
        mysqlxtest::get_error_entry_by_name(error_name_or_code);

    if (NULL == entry) {
      throw std::logic_error("Error name not found: \"" + error_name_or_code +
                             "\"");
    }

    return entry->error_code;
  }

  return try_to_interpret_text_as_error_code(error_name_or_code);
}

const Error_entry *get_error_entry_by_id(const int error_code) {
  Error_entry *error = global_error_names;

  while (error->name) {
    if (error_code == error->error_code) return error;

    ++error;
  }

  return NULL;
}

const Error_entry *get_error_entry_by_name(const std::string &name) {
  Error_entry *error = global_error_names;

  while (error->name) {
    if (name == error->name) return error;

    ++error;
  }

  return NULL;
}

}  // namespace mysqlxtest
