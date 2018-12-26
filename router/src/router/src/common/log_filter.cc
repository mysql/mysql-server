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

#include "mysqlrouter/log_filter.h"
#include <stdexcept>

#ifdef _WIN32
#define USE_STD_REGEX
#endif

#include <algorithm>
#include <iterator>
#include <sstream>

namespace mysqlrouter {

const char LogFilter::kFillCharacter = '*';

namespace {
#ifdef USE_STD_REGEX
size_t get_result_size(size_t original_size, const std::smatch &matches,
                       const std::vector<size_t> &group_index_vector) {
  size_t hash_groups_size = 0;
  for (size_t group_index : group_index_vector) {
    hash_groups_size += matches[group_index].length();
  }
  return original_size - hash_groups_size +
         group_index_vector.size() * LogFilter::kFillSize;
}
#else
size_t get_result_size(size_t original_size, regmatch_t *matches,
                       const std::vector<size_t> &group_index_vector) {
  size_t replaced_text_size = 0;
  for (size_t group_index : group_index_vector) {
    replaced_text_size +=
        (matches[group_index].rm_eo - matches[group_index].rm_so);
  }

  return original_size - replaced_text_size +
         group_index_vector.size() * LogFilter::kFillSize;
}
#endif
}  // namespace

std::string LogFilter::filter(const std::string &statement) const {
#ifdef USE_STD_REGEX
  for (const auto &each : patterns_) {
    std::smatch matches;
    if (std::regex_search(statement, matches, each.first)) {
      const std::vector<size_t> &group_index_vector = each.second;
      std::string result;
      result.reserve(
          get_result_size(statement.size(), matches, group_index_vector));
      auto statement_iterator = statement.begin();
      for (size_t group_index : group_index_vector) {
        if (matches[group_index].matched) {
          std::copy(statement_iterator, matches[group_index].first,
                    std::back_inserter(result));
          std::fill_n(std::back_inserter(result), LogFilter::kFillSize,
                      LogFilter::kFillCharacter);
          statement_iterator = matches[group_index].second;
        } else {
          // This should never happen
          throw std::logic_error("regex group is NOT matched");
        }
      }
      std::copy(statement_iterator, statement.end(),
                std::back_inserter(result));
      return result;
    }
  }
#else
  for (const auto &each : patterns_) {
    int r_err;
    constexpr size_t kMaxExpectedGroups = 5;
    regmatch_t matches[kMaxExpectedGroups];
    r_err = regexec(&each.first, statement.c_str(),
                    sizeof(matches) / sizeof(matches[0]), matches, 0);
    if (r_err == 0) {
      const std::vector<size_t> &group_index_vector = each.second;
      std::string result;
      result.reserve(
          get_result_size(statement.size(), matches, group_index_vector));
      auto statement_iterator = statement.begin();
      for (size_t group_index : group_index_vector) {
        std::copy(statement_iterator,
                  statement.begin() + matches[group_index].rm_so,
                  std::back_inserter(result));
        std::fill_n(std::back_inserter(result), LogFilter::kFillSize,
                    LogFilter::kFillCharacter);
        statement_iterator = statement.begin() + matches[group_index].rm_eo;
      }
      std::copy(statement_iterator, statement.end(),
                std::back_inserter(result));
      return result;
    }
  }
#endif
  return statement;
}

void LogFilter::add_pattern(const std::string &pattern, size_t group_index) {
  add_pattern(pattern, std::vector<size_t>(1, group_index));
}

void LogFilter::add_pattern(const std::string &pattern,
                            const std::vector<size_t> &group_indices) {
#ifdef USE_STD_REGEX
  patterns_.push_back(std::make_pair(std::regex(pattern), group_indices));
#else
  regex_t compiled_pattern;
  int r_err;
  char r_errbuf[256];

  r_err = regcomp(&compiled_pattern, pattern.c_str(), REG_EXTENDED);

  if (r_err) {
    regerror(r_err, NULL, r_errbuf, sizeof(r_errbuf));
    throw std::runtime_error("Failed to compile pattern" +
                             std::string(r_errbuf));
  }
  patterns_.push_back(
      std::make_pair(std::move(compiled_pattern), group_indices));
#endif
}

LogFilter::~LogFilter() {
#ifndef USE_STD_REGEX
  for (auto &each : patterns_) {
    regfree(&(each.first));
  }
#endif
}

void SQLLogFilter::add_default_sql_patterns() {
  add_pattern(
      "^CREATE USER ([[:graph:]]+) IDENTIFIED WITH mysql_native_password AS "
      "([[:graph:]]*)",
      2);
  add_pattern("^CREATE USER ([[:graph:]]+) IDENTIFIED BY ([[:graph:]]*)", 2);
}

}  // namespace mysqlrouter
