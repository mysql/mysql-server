// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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

#ifndef MYSQL_GTIDS_STRCONV_GTID_TEXT_FORMAT_H
#define MYSQL_GTIDS_STRCONV_GTID_TEXT_FORMAT_H

/// @file
/// Experimental API header

#include <string_view>                                    // string_view
#include "mysql/gtids/gtid.h"                             // Is_gtid
#include "mysql/gtids/gtid_set.h"                         // Is_gtid_set
#include "mysql/gtids/tag.h"                              // Is_tag
#include "mysql/gtids/tsid.h"                             // Is_tsid
#include "mysql/sets/nested_set_meta.h"                   // Is_nested_set
#include "mysql/sets/strconv/boundary_set_text_format.h"  // Boundary_set_text_format
#include "mysql/strconv/strconv.h"                        // Format_base

/// @addtogroup GroupLibsMysqlGtids
/// @{

namespace mysql::strconv {

struct Gtid_text_format : public Format_base {
  // Separator after the set associated with one UUID, before the following
  // UUID.
  static constexpr std::string_view m_uuid_uuid_separator{","};

  // Separator after the set associated with one UUID, before the following
  // UUID. This is used when formatting an object into a string and may contain
  // redundant whitespace.
  static constexpr std::string_view m_uuid_uuid_separator_for_output{",\n"};

  // Separator between UUID and tag, between UUID and interval, between tag and
  // interval, or between interval and interval.
  static constexpr std::string_view m_uuid_tag_number_separator{":"};

  // Separator between UUID and tag, between UUID and interval, between tag and
  // interval, or between interval and interval. This is used when formatting an
  // object into string and may contain redundant whitespace.
  static constexpr std::string_view m_uuid_tag_number_separator_for_output{":"};

  // Format object to generate/parse interval sets.
  static inline const Boundary_set_text_format m_boundary_set_text_format{
      "-", ":", Allow_redundant_separators::no, Allow_empty::no,
      Skip_whitespace::yes};

  // Make the parser auto-skip whitespace before every token.
  static void before_token(Parser &parser) { skip_whitespace(parser); }

  // Make the parser auto-skip whitespace after every token.
  static void after_token(Parser &parser) { skip_whitespace(parser); }

  // Fall back on m_boundary_set_text_format when writing primitive types.
  [[nodiscard]] auto parent() const { return m_boundary_set_text_format; }
};

template <class Object_t>
  requires mysql::gtids::Is_tag<Object_t> || mysql::gtids::Is_gtid<Object_t> ||
           mysql::gtids::Is_tsid<Object_t>
auto get_default_format(const Text_format &, const Object_t &) {
  return Gtid_text_format{};
}

// Must use the redundant `Is_nested_set` constraint just to force this to be
// more constrained than `get_default_format(Text_format, Is_nested_set)` which
// is defined for all nested sets.
template <class Object_t>
  requires mysql::sets::Is_nested_set<Object_t> &&
           mysql::gtids::Is_gtid_set<Object_t>
auto get_default_format(const Text_format &, const Object_t &) {
  return Gtid_text_format{};
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlGtids
/// @}

#endif  // ifndef MYSQL_GTIDS_STRCONV_GTID_TEXT_FORMAT_H
