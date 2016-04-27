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
#ifndef _UTILS_MYSQL_PARSING_H_
#define _UTILS_MYSQL_PARSING_H_

#define SPACES " \t\r\n"

#include <string>
#include <vector>
#include <stack>

namespace shcore
{
  namespace mysql
  {
    namespace splitter
    {
      // String SQL parsing functions (from WB)
      const unsigned char* skip_leading_whitespace(const unsigned char *head, const unsigned char *tail);
      bool is_line_break(const unsigned char *head, const unsigned char *line_break);
      size_t determineStatementRanges(const char *sql, size_t length, std::string &delimiter,
                                      std::vector<std::pair<size_t, size_t> > &ranges,
                                      const std::string &line_break, std::stack<std::string> &input_context_stack);
    }
  }
}

#endif