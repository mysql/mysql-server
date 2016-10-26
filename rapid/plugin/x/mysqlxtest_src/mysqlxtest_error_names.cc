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
#include "ngs_common/to_string.h"
#include "mysqlxtest_error_names.h"


namespace mysqlxtest {

static Error_entry global_error_names[] =
{
  { "<No error>", (int)-1, "" },
#ifndef IN_DOXYGEN
#include <mysqld_ername.h>
#include "mysqlx_ername.h"
#endif /* IN_DOXYGEN */
  { 0, 0, 0 }
};

int get_error_code_by_text(const std::string &argument)
{
  if ('E' == argument.at(0))
  {
    const mysqlxtest::Error_entry* entry = mysqlxtest::get_error_entry_by_name(argument);

    if (NULL == entry)
    {
      std::string error_message = "Error name not found: \"";
      error_message += argument;
      error_message += "\"";
      throw std::logic_error(error_message);
    }

    return entry->error_code;
  }

  return ngs::stoi(argument.c_str());
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

