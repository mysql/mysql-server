/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_COMMON_INCLUDED
#define MYSQL_HARNESS_COMMON_INCLUDED

#include <cstdlib>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include "harness_export.h"

/**
 * @defgroup Various operations
 *
 * This module contain various utility operations.
 */

namespace mysql_harness {

/**
 * Return a truncated version of input string.
 *
 * @param str input text
 * @param max_len maximum length after truncation
 * @return truncated string
 */
HARNESS_EXPORT
std::string truncate_string(const std::string &str, size_t max_len = 80);

/**
 * Emit a range of elements using the serial comma.
 *
 * This function can be used to output a range of elements using a
 * serial comma (also known as the Oxford comma). To emit a list of
 * the first five prime numbers as "The first five prime numbers are
 * 2, 3, 5, 7, and 11":
 *
 * @code
 * std::vector<int> primes{2, 3, 5, 7, 11};
 * std::cout << "The first five prime numbers are ";
 * serial_comma(std::cout, primes.begin(), primes.end());
 * std::cout << std::endl;
 * @endcode
 *
 * @param out Output stream
 * @param start Input iterator to start of range.
 * @param finish Input iterator to one-after-end of range.
 * @param delim Delimiter to use. Defaults to "and".
 */
template <class InputIt>
void serial_comma(std::ostream &out, InputIt start, InputIt finish,
                  const std::string &delim = "and") {
  auto elements = std::distance(start, finish);
  if (elements == 1) {
    out << *start;
  } else if (elements == 2) {
    out << *start++;
    out << " " << delim << " " << *start;
  } else {
    while (elements-- > 0) {
      out << *start++;
      if (elements > 0) out << ", ";
      if (elements == 1) out << delim << " ";
    }
  }
}

/**
 * Returns string containing list of the elements using the serial comma.
 *
 * This function can be used to output a range of elements using a
 * serial comma (also known as the Oxford comma). To return a list of
 * the first five prime numbers as "The first five prime numbers are
 * 2, 3, 5, 7, and 11":
 *
 * @code
 * std::vector<int> primes{2, 3, 5, 7, 11};
 * std::cout << "The first five prime numbers are "
 *           << serial_comma(primes.begin(), primes.end()) << std::endl;
 * @endcode
 *
 * @param start Input iterator to start of range.
 * @param finish Input iterator to one-after-end of range.
 * @param delim Delimiter to use. Defaults to "and".
 *
 * @return string containing list of the elements
 */
template <class InputIt>
std::string serial_comma(InputIt start, InputIt finish,
                         const std::string &delim = "and") {
  std::stringstream out;
  serial_comma(out, start, finish, delim);

  return out.str();
}

/**
 * Gets a Value from std::map for given Key. Returns provided default if the Key
 * is not in the map.
 */
template <class Key, class Value>
Value get_from_map(const std::map<Key, Value> &map, const Key &key,
                   const Value &default_value) {
  auto iter = map.find(key);
  if (iter == map.end()) return default_value;
  return iter->second;
}

}  // namespace mysql_harness

#endif /* MYSQL_HARNESS_COMMON_INCLUDED */
