/*
  Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_LOG_FILTER_INCLUDED
#define MYSQLROUTER_LOG_FILTER_INCLUDED

#include "mysqlrouter/router_export.h"

#include <string>
#include <vector>

#include <regex>
using regex_search_and_replace_patterns = std::pair<std::regex, std::string>;

namespace mysqlrouter {

/*
 * A LogFilter allows to replace substring with '***'.
 */
class ROUTER_LIB_EXPORT LogFilter {
 public:
  static const char kFillCharacter;
  static const unsigned int kFillSize = 3;

  /*
   * @param statement The string to be filtered.
   *
   * @return filtered string
   */
  std::string filter(std::string statement) const;

  /*
   * @param pattern The string with pattern to match
   * @param group_indices The vector with indices of groups that will be
   * replaced with '***'
   */
  void add_pattern(const std::string &pattern,
                   const std::vector<size_t> &group_indices);

  /*
   * @param pattern The string with regex pattern to match
   * @param replacement Replacement string for matched pattern.  You can use
   *        $<nr> notation to insert captured groups from regex search pattern
   */
  void add_pattern(const std::string &pattern, const std::string &replacement);

 private:
  std::vector<regex_search_and_replace_patterns> patterns_;
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
