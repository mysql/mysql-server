/*
   Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef STORAGE_NDB_PLUGIN_NDB_STRING_TRIM_H_
#define STORAGE_NDB_PLUGIN_NDB_STRING_TRIM_H_

#include <string_view>

namespace ndbcluster {

/**
 * @brief Trim whitespace from both ends of a string_view.
 *
 * Removes leading and trailing characters found in `whitespace` from the given
 * string_view. The returned view refers to the original input.
 *
 * @param sv Input string view.
 * @param whitespace Characters considered whitespace (default: " \\t").
 * @return A subview of sv with whitespace removed from both ends.
 */
inline constexpr std::string_view trim(
    std::string_view sv, std::string_view whitespace = " \t") noexcept {
  const auto start = sv.find_first_not_of(whitespace);
  if (start == std::string_view::npos) {
    return {};
  }
  const auto end = sv.find_last_not_of(whitespace);
  return sv.substr(start, end - start + 1);
}

}  // namespace ndbcluster

#endif  // STORAGE_NDB_PLUGIN_NDB_STRING_TRIM_H_
