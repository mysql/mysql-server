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
#ifndef PLUGIN_X_TESTS_DRIVER_COMMON_UTILS_MYSQL_PARSING_H_
#define PLUGIN_X_TESTS_DRIVER_COMMON_UTILS_MYSQL_PARSING_H_

#define SPACES " \t\r\n"

#include <stack>
#include <string>
#include <vector>

namespace shcore {
namespace mysql {
namespace splitter {
// String SQL parsing functions (from WB)
const unsigned char *skip_leading_whitespace(const unsigned char *head,
                                             const unsigned char *tail);
bool is_line_break(const unsigned char *head, const unsigned char *line_break);
size_t determineStatementRanges(const char *sql, size_t length,
                                std::string &delimiter,
                                std::vector<std::pair<size_t, size_t>> &ranges,
                                const std::string &line_break,
                                std::stack<std::string> &input_context_stack);
}  // namespace splitter
}  // namespace mysql
}  // namespace shcore

#endif  // PLUGIN_X_TESTS_DRIVER_COMMON_UTILS_MYSQL_PARSING_H_
