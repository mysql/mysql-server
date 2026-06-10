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

#ifndef MYSQL_SETS_STRCONV_NESTED_SET_TEXT_FORMAT_H
#define MYSQL_SETS_STRCONV_NESTED_SET_TEXT_FORMAT_H

/// @file
/// Experimental API header

#include <string_view>                          // string_view
#include "mysql/meta/is_specialization.h"       // Is_specialization
#include "mysql/sets/nested_set_meta.h"         // Is_nested_set
#include "mysql/strconv/formats/format.h"       // Format_base
#include "mysql/strconv/formats/text_format.h"  // Text_format

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::strconv {

/// Tag to identify the formatting algorithm for nested sets, using the given
/// format tags for the key type and the mapped type, respectively. Also holds
/// the separator strings used between key and mapped object, and between one
/// key-mapped pair and another key-mapped pair.
template <class Key_format_tp, class Mapped_format_tp>
struct Nested_set_text_format : public Format_base {
  using Key_format_t = Key_format_tp;
  using Mapped_format_t = Mapped_format_tp;
  explicit Nested_set_text_format(
      const Key_format_t &key_format = Key_format_t{},
      const Mapped_format_t &mapped_format = Mapped_format_t{},
      std::string_view item_separator = ",",
      std::string_view key_mapped_separator = ":")
      : m_key_format(key_format),
        m_mapped_format(mapped_format),
        m_item_separator(item_separator),
        m_key_mapped_separator(key_mapped_separator) {}
  explicit Nested_set_text_format(const Text_format &)
      : Nested_set_text_format() {}
  const Key_format_t m_key_format;
  const Mapped_format_t m_mapped_format;
  std::string_view m_item_separator;
  std::string_view m_key_mapped_separator;
  [[nodiscard]] constexpr auto parent() const { return Text_format{}; }
};

template <class Test>
concept Is_nested_set_text_format =
    mysql::meta::Is_specialization<Test, Nested_set_text_format>;

/// Make `mysql::strconv::encode_text` (and `encode(Text_format{}, ...)` use
/// `Nested_set_text_format` when the object to format is a Nested set.
auto get_default_format(const Text_format &,
                        const mysql::sets::Is_nested_set auto &) {
  return Nested_set_text_format(Text_format{}, Text_format{});
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_STRCONV_NESTED_SET_TEXT_FORMAT_H
