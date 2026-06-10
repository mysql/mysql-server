// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_SETS_STRCONV_BOUNDARY_SET_TEXT_FORMAT_H
#define MYSQL_SETS_STRCONV_BOUNDARY_SET_TEXT_FORMAT_H

/// @file
/// Experimental API header

#include <string_view>                     // string_view
#include "mysql/meta/is_specialization.h"  // Is_specialization
#include "mysql/sets/boundary_set_meta.h"  // Is_boundary_set
#include "mysql/sets/interval.h"           // Interval
#include "mysql/sets/interval_set_meta.h"  // Is_interval_set
#include "mysql/strconv/strconv.h"         // Format_base

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::strconv {

// NOLINTBEGIN(performance-enum-size): silence clang-tidy's pointless hint
enum class Skip_whitespace { no, yes };
enum class Allow_redundant_separators { no, yes };
enum class Allow_empty { no, yes };
// NOLINTEND(performance-enum-size)

/// Tag to identify the formatting algorithm for boundary sets of
/// integrals, and provide the separator strings and logic to skip whitespace
/// around tokens.
struct Boundary_set_text_format : public Format_base {
  explicit Boundary_set_text_format(
      const std::string_view &boundary_separator = "-",
      const std::string_view &interval_separator = ",",
      Allow_redundant_separators allow_redundant_separators =
          Allow_redundant_separators::yes,
      Allow_empty allow_empty = Allow_empty::yes,
      Skip_whitespace skip_whitespace_arg = Skip_whitespace::yes) noexcept
      : m_boundary_separator(boundary_separator),
        m_interval_separator(interval_separator),
        m_allow_redundant_separators(allow_redundant_separators),
        m_allow_empty(allow_empty),
        m_skip_whitespace(skip_whitespace_arg) {}

  explicit Boundary_set_text_format(const Text_format &) noexcept
      : Boundary_set_text_format() {}

  /// Fallback to Text_format to read/write types that don't have
  /// encode_impl/decode_impl implemented for Boundary_set_text_format.
  [[nodiscard]] constexpr auto parent() const { return Text_format{}; }

  /// Separator between start and end of a single interval.
  std::string_view m_boundary_separator;

  /// Separator between end of one interval and start of next interval.
  std::string_view m_interval_separator;

  /// When true, accept and skip extra interval separators before and after
  /// intervals.
  Allow_redundant_separators m_allow_redundant_separators;

  /// When true, accept the empty set.
  Allow_empty m_allow_empty;

  /// When true, accept and skip whitespace between tokens.
  Skip_whitespace m_skip_whitespace;

  /// Skip whitespace before tokens, if m_skip_whitespace == true.
  void before_token(Parser &parser) const {
    if (m_skip_whitespace == Skip_whitespace::yes) skip_whitespace(parser);
  }

  /// Skip whitespace after tokens, if m_skip_whitespace == true.
  void after_token(Parser &parser) const {
    if (m_skip_whitespace == Skip_whitespace::yes) skip_whitespace(parser);
  }
};

/// Make `mysql::strconv::encode_text` (and `encode(Text_format{}, ...)` use
/// `Boundary_set_text_format` when the object to format is an Interval,
/// Boundary sets, or Interval set.
template <class Object_t>
  requires mysql::meta::Is_specialization<Object_t, mysql::sets::Interval> ||
           mysql::sets::Is_boundary_set<Object_t> ||
           mysql::sets::Is_interval_set<Object_t>
auto get_default_format(const Text_format &, const Object_t &) {
  return Boundary_set_text_format{};
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_STRCONV_BOUNDARY_SET_TEXT_FORMAT_H
