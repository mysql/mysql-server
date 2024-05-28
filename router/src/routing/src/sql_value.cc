/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "sql_value.h"

#include <iomanip>  // quoted
#include <sstream>  // stringstream

namespace {
constexpr bool is_number(std::string_view s) {
  bool at_least_one_digit{false};
  auto cur = s.begin();
  const auto end = s.end();

  if (cur == end) return false;  // empty.

  if (*cur == '-') ++cur;  // leading minus

  for (; cur != end; ++cur) {
    auto ch = *cur;

    if (ch == '.') {
      ++cur;
      break;
    } else if (ch < '0' || ch > '9') {
      // not a digit, fail
      return false;
    }

    at_least_one_digit = true;
  }

  for (; cur != end; ++cur) {
    auto ch = *cur;

    // not a digit, fail
    if (ch < '0' || ch > '9') return false;

    at_least_one_digit = true;
  }

  // at least one digit must have been seen to avoid accepting:
  //
  // - "."
  // - "-"
  // - "-."
  //
  // but  "1.",  ".1",
  // and "-1.", "-.1" is ok.

  return at_least_one_digit;
}

static_assert(is_number("1"));
static_assert(is_number("1."));
static_assert(is_number("1.1"));
static_assert(is_number(".1"));

static_assert(is_number("-1"));
static_assert(is_number("-1."));
static_assert(is_number("-1.1"));
static_assert(is_number("-.1"));

static_assert(!is_number(""));
static_assert(!is_number("."));
static_assert(!is_number("-"));
static_assert(!is_number("-."));

}  // namespace

std::string sql_value_to_string(const std::optional<std::string> &val) {
  if (!val.has_value()) return "NULL";

  if (is_number(*val)) return *val;

  std::ostringstream oss;

  oss << std::quoted(*val);

  return oss.str();
}
