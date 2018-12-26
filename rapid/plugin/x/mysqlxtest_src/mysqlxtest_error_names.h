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

#ifndef MYSQLXTEST_ERROR_NAMES_H_
#define MYSQLXTEST_ERROR_NAMES_H_

#include <string>


namespace mysqlxtest {

struct Error_entry {
  const char *name;
  int error_code;
  const char *description;
};

int get_error_code_by_text(const std::string &argument);
const Error_entry *get_error_entry_by_id(const int error_code);
const Error_entry *get_error_entry_by_name(const std::string &name);

} //namespace mysqlxtest

#endif // MYSQLXTEST_ERROR_NAMES_H_
