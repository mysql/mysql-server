/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef HARNESS_STRING_INCLUDED
#define HARNESS_STRING_INCLUDED

#include "harness_export.h"

#include <numeric>  // accumulate
#include <string>
#include <vector>

namespace mysql_harness {
namespace utility {
std::vector<std::string> HARNESS_EXPORT wrap_string(const std::string &to_wrap,
                                                    std::size_t width,
                                                    std::size_t indent_size);
}

namespace detail {
template <class Container, class T>
struct Join {
  static std::string impl(Container, const std::string &);
};

template <class Container>
struct Join<Container, std::string> {
  static std::string impl(Container cont, const std::string &delim) {
    if (cont.begin() == cont.end()) return {};

    std::string o(*(cont.begin()));

    // if T::value_type has a .size() method reallocs can be avoided
    // when joining the strings by calculating the size upfront
    {
      const size_t delim_size = delim.size();
      size_t space =
          std::accumulate(std::next(cont.begin()), cont.end(), o.size(),
                          [delim_size](size_t sum, const std::string &b) {
                            return sum + delim_size + b.size();
                          });
      o.reserve(space);
    }

#if 0
    // once benchmarked that this is equivalent ot the hand-rolled version
    // (number of allocs, ...) this implmentation could be used.
    return std::accumulate(std::next(cont.begin()), cont.end(), o,
                           [&delim](std::string a, const std::string &b) {
                             return a.append(delim).append(b);
                           });
#else
    // add the first element directly
    auto it = std::next(cont.begin());
    const auto last = cont.end();

    // all other elements with delim
    for (; it != last; ++it) {
      o += delim;

      o += *it;
    }

    return o;
#endif
  }
};

template <class Container>
struct Join<Container, const char *> {
  static std::string impl(Container cont, const std::string &delim) {
    if (cont.begin() == cont.end()) return {};

    return std::accumulate(std::next(cont.begin()), cont.end(),
                           std::string(*(cont.begin())),
                           [&delim](const std::string &a, const char *b) {
                             return a + delim + b;
                           });
  }
};

}  // namespace detail

/**
 * join elements of an container into a string seperated by a delimiter.
 *
 * Container MUST:
 *
 * - have .begin() and end()
 * - ::iterator must be ForwardIterator + InputIterator
 * - ::value_type must be appendable to std::string
 *
 * should work with:
 *
 * - std::vector<const char *|std::string>
 * - std::array<const char *|std::string, N>
 * - std::list<const char *|std::string>
 *
 * @param cont a container
 * @param delim delimiter
 * @returns string of elements of container seperated by delim
 */
template <class Container>
std::string join(Container cont, const std::string &delim) {
  return detail::Join<Container, typename Container::value_type>::impl(cont,
                                                                       delim);
}

}  // namespace mysql_harness

#endif
