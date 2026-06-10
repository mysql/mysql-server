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

#ifndef STORAGE_NDB_PLUGIN_NDB_STRING_CONV_H_
#define STORAGE_NDB_PLUGIN_NDB_STRING_CONV_H_

#include <charconv>
#include <concepts>
#include <string_view>
#include <system_error>

namespace ndbcluster {

/**
 * @brief Convert a std::string_view to an integral value using std::from_chars.
 * @tparam I Integral type (constrained by std::integral).
 * @param sv  Input character view to parse (no trimming is performed).
 * @param out Reference to receive the parsed value on success. Unmodified on
 *            failure.
 * @return true if parsing succeeded and the entire view was consumed; false
 *              otherwise.
 * @note Whitespace is not ignored; callers should trim beforehand if needed.
 */
template <std::integral I>
inline bool from_chars_to(std::string_view sv, I &out) noexcept {
  I tmp{};
  const char *const first = sv.data();
  const char *const last = sv.data() + sv.size();
  auto res = std::from_chars(first, last, tmp);
  if (res.ec != std::errc{} || res.ptr != last) {
    return false;
  }
  out = tmp;
  return true;
}

}  // namespace ndbcluster

#endif  // STORAGE_NDB_PLUGIN_NDB_STRING_CONV_H_
