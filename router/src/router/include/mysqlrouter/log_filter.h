/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef LOG_FILTER_INCLUDED
#define LOG_FILTER_INCLUDED

#include <string>
#include <vector>

#ifdef _WIN32
#include <regex>
using regex_and_group_indices = std::pair<std::regex, std::vector<size_t>>;
#else
#include <regex.h>
using regex_and_group_indices = std::pair<regex_t, std::vector<size_t>>;
#endif

namespace mysqlrouter {

/*
 * A LogFilter allows to replace substring with '***'.
 */
class LogFilter {
 public:
  static const char kFillCharacter;
  static const unsigned int kFillSize = 3;

  /*
   * @param statement The string to be filtered.
   *
   * @return filtered string
   */
  std::string filter(const std::string &statement) const;

  /*
   * @param pattern The string with pattern to match
   * @param group_indices The vector with indices of groups that will be
   * replaced with '***'
   */
  void add_pattern(const std::string &pattern,
                   const std::vector<size_t> &group_indices);

  /*
   * @param pattern The string with pattern to match
   * @param group_index The index of the group that will be replaced with '***'
   */
  void add_pattern(const std::string &pattern, size_t group_index);

  virtual ~LogFilter();

 private:
  std::vector<regex_and_group_indices> patterns_;
};

/**
 * A SQLLogFilter allows to replace substrings defined by a set of hardcoded
 * regular expressions with '***'.
 */
class SQLLogFilter : public LogFilter {
 public:
  /*
   * Adds default patterns defined as regular expressions.
   */
  void add_default_sql_patterns();
};

}  // namespace mysqlrouter

#endif  // LOG_FILTER_INCLUDED
