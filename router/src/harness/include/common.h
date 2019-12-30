/*
  Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_HARNESS_COMMON_INCLUDED
#define MYSQL_HARNESS_COMMON_INCLUDED

#include <cstdlib>
#include <sstream>
#include <string>
#include "harness_export.h"

/**
 * @defgroup Various operations
 *
 * This module contain various utility operations.
 */

namespace mysql_harness {

/** @brief Wrapper for thread safe function returning error string.
 *
 * @param err error number
 * @return string describing the error
 */
std::string HARNESS_EXPORT get_strerror(int err);

/** @brief Rename a thread (useful for debugging purposes).
 *
 * @param thread_name thread name, 15 chars max
 */
void HARNESS_EXPORT rename_thread(const char thread_name[16]);

/** @brief Return a truncated version of input string (fast version)
 *
 * WARNING!
 * This function is optimised for speed, but see note below for use
 * restrictions. If these are a problem, use truncate_string_r() instead.
 *
 * This function returns a refernce to the input string if input.size() <=
 * max_len, otherwise it returns a reference to a truncated copy of input
 * string.
 *
 * @param input input text
 * @param max_len maximum length after truncation
 * @return const reference to truncated string
 *
 * @note This function may return a reference to a string allocated on
 * thread-local storage. Therefore, the resulting string reference is only valid
 * until another call to this function is made from caller's thread (other
 * threads calling this function have no impact), and by the same token,
 * dereferencing it outside of the caller's thread may lead to a race. If your
 * use case violates these limitations, you should use truncate_string_r()
 * instead to ensure safety.
 */
HARNESS_EXPORT
const std::string &truncate_string(const std::string &input,
                                   size_t max_len = 80);

/** @brief Return a truncated version of input string (reentrant version)
 *
 * This is a safe version of truncate_string(), which lifts its use restrictions
 * by always returning a copy of result string. Please see documentation of
 * truncate_string() for more information.
 */
HARNESS_EXPORT
std::string truncate_string_r(const std::string &input, size_t max_len = 80);

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
 * Returns string containing list of the elements separated by selected
 * delimiter.
 *
 * To return a list of the first five prime numbers as "The first five prime
 * numbers are 2, 3, 5, 7, 11":
 *
 * @code
 * std::vector<int> primes{2, 3, 5, 7, 11};
 * std::cout << "The first five prime numbers are "
 *           << list_elements(primes.begin(), primes.end()) << std::endl;
 * @endcode
 *
 * @param start Input iterator to start of range.
 * @param finish Input iterator to one-after-end of range.
 * @param delim Delimiter to use. Defaults to ",".
 *
 * @return string containing list of the elements
 */
template <class InputIt>
std::string list_elements(InputIt start, InputIt finish,
                          const std::string &delim = ",") {
  std::string result;
  for (auto cur = start; cur != finish; ++cur) {
    if (cur != start) result += delim;
    result += *cur;
  }

  return result;
}

/**
 * Returns string containing list of the elements separated by selected
 * delimiter.
 *
 * To return a list of the first five prime numbers as "The first five prime
 * numbers are 2, 3, 5, 7, 11":
 *
 * @code
 * std::vector<int> primes{2, 3, 5, 7, 11};
 * std::cout << "The first five prime numbers are "
 *           << list_elements(primes) << std::endl;
 * @endcode
 *
 * @param collection Collection of the elements to output.
 * @param delim Delimiter to use. Defaults to ",".
 *
 * @return string containing list of the elements
 */
template <class Collection>
std::string list_elements(Collection collection,
                          const std::string &delim = ",") {
  return list_elements(collection.begin(), collection.end(), delim);
}

}  // namespace mysql_harness

#endif /* MYSQL_HARNESS_COMMON_INCLUDED */
