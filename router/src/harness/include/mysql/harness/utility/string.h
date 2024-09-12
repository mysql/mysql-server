/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#ifndef HARNESS_STRING_INCLUDED
#define HARNESS_STRING_INCLUDED

#include "harness_export.h"

#include <string>
#include <vector>

#include "my_compiler.h"  // MY_ATTRIBUTE

namespace mysql_harness {
namespace utility {
std::vector<std::string> HARNESS_EXPORT wrap_string(const std::string &to_wrap,
                                                    std::size_t width,
                                                    std::size_t indent_size);

HARNESS_EXPORT
MY_ATTRIBUTE((format(printf, 1, 2)))
std::string string_format(const char *format, ...);

}  // namespace utility

namespace detail {

// a simplified variant of the std::ranges::range concept
//
// C++20 ranges lib isn't fully available yet.

template <class R>
concept range = requires(const R &rng) {
                  std::begin(rng);
                  std::end(rng);
                };

}  // namespace detail

/**
 * join elements of a range into a string separated by a delimiter.
 *
 * works with:
 *
 * - std::vector, std::array, c-array, list, forward_list, deque
 * - and std::string, c-string, std::string_view
 *
 * @param rng a range of strings
 * @param delim delimiter
 * @returns string of elements of the range separated by delim
 */
std::string join(const detail::range auto &rng, std::string_view delim)
  requires(std::constructible_from<std::string, decltype(*std::begin(rng))>)
{
  auto cur = std::begin(rng);
  const auto end = std::end(rng);

  if (cur == end) return {};  // empty

  std::string joined(*cur);  // first element.

  // append delim + element
  for (cur = std::next(cur); cur != end; cur = std::next(cur)) {
    joined.append(delim).append(*cur);
  }

  return joined;
}

/* Checks that given string belongs to the collection of strings */
template <class T>
constexpr bool str_in_collection(const T &t, const std::string_view &k) {
  for (auto v : t) {
    if (v == k) return true;
  }
  return false;
}

}  // namespace mysql_harness

#endif
