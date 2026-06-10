/*
  Copyright (c) 2017, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysqlrouter/log_filter.h"

#include "mysql/harness/regex_matcher.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace mysqlrouter {

using mysql_harness::RegexMatcher;

const char LogFilter::kFillCharacter = '*';

struct LogFilter::Impl {
  void add_pattern(const std::string &pattern, const std::string &replacement) {
    patterns_.emplace_back(std::make_unique<RegexMatcher>(pattern),
                           replacement);
  }

  using regex_search_and_replace_patterns =
      std::pair<std::unique_ptr<RegexMatcher>, std::string>;

  std::vector<regex_search_and_replace_patterns> patterns_;
};

std::string LogFilter::filter(std::string statement) const {
  if (impl_->patterns_.size() == 0) {
    return statement;
  }

  for (const auto &p : impl_->patterns_) {
    const auto &matcher = p.first;
    const auto &pattern = p.second;
    statement = matcher->replace_all(statement, pattern);
  }

  return statement;
}

void LogFilter::add_pattern(const std::string &pattern,
                            const std::string &replacement) {
  impl_->add_pattern(pattern, replacement);
}

void SQLLogFilter::add_default_sql_patterns() {
  // Add pattern for replacing passwords in 'CREATE USER [IF NOT EXISTS] ...'.
  // Works for mysql_native_password, plaintext authentication and other
  // auth_plugin methods.
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
  add_pattern("(IDENTIFIED\\s+(WITH\\s+[a-z0-9_`]+\\s+)?(BY|AS))\\s+'[^']*'",
              "$1 ***");
}

LogFilter::LogFilter() { impl_ = std::make_unique<Impl>(); }

LogFilter::~LogFilter() = default;

}  // namespace mysqlrouter
