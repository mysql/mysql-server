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

#include "mysqlrouter/log_filter.h"
#include <stdexcept>

#include <algorithm>
#include <iterator>
#include <sstream>

namespace mysqlrouter {

const char LogFilter::kFillCharacter = '*';

std::string LogFilter::filter(std::string statement) const {
  for (const auto &each : patterns_) {
    statement = std::regex_replace(statement, each.first, each.second);
  }
  return statement;
}

void LogFilter::add_pattern(const std::string &pattern,
                            const std::string &replacement) {
  patterns_.push_back(std::make_pair(
      std::regex(pattern, std::regex_constants::icase), replacement));
}

void SQLLogFilter::add_default_sql_patterns() {
  // Add pattern for replacing passwords in 'CREATE USER [IF NOT EXISTS] ...'.
  // Works for both mysql_native_password and plaintext authentication methods.
  //
  // Below example showcases mysql_native_password method; lines are wrapped
  // for easier viewing (in real life they're a single line).
  //
  // clang-format off
  // before:
  //   CREATE USER IF NOT EXISTS
  //     'some_user'@'h1' IDENTIFIED WITH mysql_native_password AS '*FF1D4A27A543DD464A5FFA210278E604979F781B',
  //     'some_user'@'h2' IDENTIFIED WITH mysql_native_password AS '*FF1D4A27A543DD464A5FFA210278E604979F781B',
  //     'some_user'@'h3' IDENTIFIED WITH mysql_native_password AS '*FF1D4A27A543DD464A5FFA210278E604979F781B'
  // after:
  //   CREATE USER IF NOT EXISTS
  //     'some_user'@'h1' IDENTIFIED WITH mysql_native_password AS ***,
  //     'some_user'@'h2' IDENTIFIED WITH mysql_native_password AS ***,
  //     'some_user'@'h3' IDENTIFIED WITH mysql_native_password AS ***
  // clang-format on
  add_pattern("(IDENTIFIED\\s+(WITH\\s+[a-z_]+\\s+)?(BY|AS))\\s+'[^']*'",
              "$1 ***");
}

}  // namespace mysqlrouter
