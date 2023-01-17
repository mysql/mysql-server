/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_STRING_UTILS_INCLUDED
#define MYSQL_HARNESS_STRING_UTILS_INCLUDED

#include "harness_export.h"

#include <string>
#include <string_view>
#include <vector>

namespace mysql_harness {

/** @brief Splits a string using a delimiter
 *
 * Splits a string using the given delimiter. When allow_empty
 * is true (default), tokens can be empty, and will be included
 * as empty in the result.
 *
 * @param data a string to split
 * @param delimiter a char used as delimiter
 * @param allow_empty whether to allow empty tokens or not (default true)
 * @return std::vector<string> containing tokens
 */
HARNESS_EXPORT
std::vector<std::string> split_string(const std::string_view &data,
                                      const char delimiter,
                                      bool allow_empty = true);

/**
 * Removes leading whitespaces from the string
 *
 * @param str the string to be trimmed
 */
HARNESS_EXPORT
void left_trim(std::string &str);

/**
 * Removes trailing whitespaces from the string
 *
 * @param str the string to be trimmed
 */
HARNESS_EXPORT
void right_trim(std::string &str);

/**
 * Removes both leading and trailing whitespaces from the string
 *
 * @param str the string to be trimmed
 */
HARNESS_EXPORT
void trim(std::string &str);

/**
 * Returns the input string with number of lines reduced to selected value.
 *
 * @param str input string
 * @param limit maximum number of lines of the returned string
 * @param replace_with string that should be used in place of the removed lines
 *
 * @returns If the input string contains more than 'limit' number of lines, it
 * removes the lines from the middle leaving only 'limit' number of lines
 * (limit/2 of the first lines and limit/2 of the last lines). Otherwise it
 * returns the whole input string.
 */
HARNESS_EXPORT
std::string limit_lines(const std::string &str, const size_t limit,
                        const std::string &replace_with = "");

}  // namespace mysql_harness

#endif /* MYSQL_HARNESS_STRING_UTILS_INCLUDED */
