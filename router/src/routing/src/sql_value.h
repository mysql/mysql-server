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

#ifndef ROUTING_SQL_VALUE_INCLUDED
#define ROUTING_SQL_VALUE_INCLUDED

#include <optional>
#include <string>

/**
 * a nullable SQL value.
 *
 * For now, supports NULL and strings.
 *
 * Note: In the future, may switch to std::variant<> or
 * similar to cover more types if needed.
 */
class Value {
 public:
  using value_type = std::optional<std::string>;

  Value(value_type v) : value_{std::move(v)} {}

  const value_type &value() const & { return value_; }

  /**
   * "NULL" or the quoted string.
   */
  std::string to_string() const;

 private:
  value_type value_;
};

inline bool operator==(const Value &lhs, const Value &rhs) {
  return lhs.value() == rhs.value();
}

inline bool operator==(const Value &lhs, const std::string_view &rhs) {
  if (!lhs.value()) return false;

  return std::string_view(*(lhs.value())) == rhs;
}

inline bool operator!=(const Value &lhs, const Value &rhs) {
  return !(lhs == rhs);
}

inline bool operator!=(const Value &lhs, const std::string_view &rhs) {
  return !(lhs == rhs);
}

#endif
