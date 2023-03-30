/*
  Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_UTILITIES_INCLUDED
#define MYSQL_HARNESS_UTILITIES_INCLUDED

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "harness_export.h"

namespace mysql_harness {

namespace utility {

/**
 * Class to turn C array into range.
 *
 * @see make_range
 */

template <class Type>
class Range {
 public:
  class iterator {
   public:
    explicit iterator(Type *ptr) : ptr_(ptr) {}

    iterator &operator++() {
      ++ptr_;
      return *this;
    }

    bool operator==(const iterator &rhs) { return ptr_ == rhs.ptr_; }

    bool operator!=(const iterator &rhs) { return ptr_ != rhs.ptr_; }

    Type &operator*() { return *ptr_; }

    const Type &operator*() const { return *ptr_; }

   private:
    Type *ptr_;
  };

  Range(Type *ptr, size_t length) : start_(ptr), finish_(ptr + length) {}

  iterator begin() { return iterator(start_); }

  iterator end() { return iterator(finish_); }

 private:
  Type *start_;
  Type *finish_;
};

/**
 * Create a range from a plain C array.
 *
 * This function create a range from a plain array so that arrays can
 * be used in range-based loops.
 *
 * @see Range
 */

template <class Type>
Range<Type> make_range(Type *ptr, size_t length) {
  return Range<Type>(ptr, length);
}

/**
 * Class for creating a reverse range from another range.
 */

template <typename Range>
class RangeReverse {
 public:
  explicit RangeReverse(Range &range) : range_(range) {}

  typename Range::reverse_iterator begin() { return range_.rbegin(); }

  typename Range::const_reverse_iterator begin() const {
    return range_.rbegin();
  }

  typename Range::reverse_iterator end() { return range_.rend(); }

  typename Range::const_reverse_iterator end() const { return range_.rend(); }

 private:
  Range &range_;
};

/**
 * Iterate over a range in reverse.
 *
 * Function take a range, which can be any sequence container, and
 * return a reverse range that iterate the sequence in reverse.
 *
 * Typical use-case is:
 * @code
 * for (auto item : reverse_iterate(my_list)) {
 *   ...
 * }
 * @endcode
 */
template <typename Range>
RangeReverse<Range> reverse(Range &x) {  // NOLINT(runtime/references)
  return RangeReverse<Range>(x);
}

template <class Map>
std::pair<typename Map::iterator, typename Map::iterator> find_range_first(
    Map &assoc,  // NOLINT(runtime/references)
    const typename Map::key_type::first_type &first,
    typename Map::iterator start) {
  typename Map::iterator finish = start;
  while (finish != assoc.end() && finish->first.first == first) ++finish;
  return make_pair(start, finish);
}

template <class Map>
std::pair<typename Map::iterator, typename Map::iterator> find_range_first(
    Map &assoc,  // NOLINT(runtime/references)
    const typename Map::key_type::first_type &first) {
  typedef typename Map::key_type::second_type SType;
  return find_range_first(assoc, first,
                          assoc.lower_bound(make_pair(first, SType())));
}

template <class Map>
std::pair<typename Map::const_iterator, typename Map::const_iterator>
find_range_first(const Map &assoc,
                 const typename Map::key_type::first_type &first,
                 typename Map::const_iterator start) {
  typename Map::const_iterator finish = start;
  while (finish != assoc.end() && finish->first.first == first) ++finish;
  return make_pair(start, finish);
}

template <class Map>
std::pair<typename Map::const_iterator, typename Map::const_iterator>
find_range_first(const Map &assoc,
                 const typename Map::key_type::first_type &first) {
  typedef typename Map::key_type::second_type SType;
  return find_range_first(assoc, first,
                          assoc.lower_bound(make_pair(first, SType())));
}

std::string dirname(const std::string &path);
std::string basename(const std::string &path);

/**
 * Remove starting and trailing delimiters from string.
 */
void strip(std::string *str, const char *chars = " \t\n\r\f\v");
HARNESS_EXPORT
std::string strip_copy(std::string str, const char *chars = " \t\n\r\f\v");

bool matches_glob(const std::string &word, const std::string &pattern);

/*
 * Checks whether given string matches the pattern using extended posix regex.
 */
bool regex_pattern_matches(const std::string &s, const std::string &pattern);

}  // namespace utility

}  // namespace mysql_harness
#endif /* MYSQL_HARNESS_UTILITIES_INCLUDED */
