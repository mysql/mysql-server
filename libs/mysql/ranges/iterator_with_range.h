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

#ifndef MYSQL_RANGES_ITERATOR_WITH_RANGE_H
#define MYSQL_RANGES_ITERATOR_WITH_RANGE_H

/// @file
/// Experimental API header

#include <cstddef>                               // ptrdiff_t
#include <iterator>                              // bidirectional_iterator
#include <ranges>                                // range
#include "mysql/iterators/iterator_interface.h"  // Iterator_interface
#include "mysql/ranges/meta.h"                   // Range_const_iterator_type
#include "mysql/ranges/view_sources.h"           // View_source

/// @addtogroup GroupLibsMysqlRanges
/// @{

namespace mysql::ranges {

/// Iterator that holds a reference to its source range.
///
/// Since the iterator knows its range, it always knows whether it is positioned
/// at the end, without having to compare with other objects.
template <std::ranges::range Range_tp>
class Iterator_with_range : public mysql::iterators::Iterator_interface<
                                Iterator_with_range<Range_tp>> {
 public:
  using Range_t = Range_tp;
  using Iterator_t = Range_const_iterator_type<Range_t>;

  /// Construct a singular object.
  Iterator_with_range() = default;

  /// Construct a new object from the given range, and set the iterator to
  /// the beginning.
  explicit Iterator_with_range(const Range_t &range, const Iterator_t &iterator)
      : m_range(range), m_iterator(iterator) {}
  /// Construct a new object from the given range, and set the iterator to
  /// the beginning.
  explicit Iterator_with_range(const Range_t &range)
      : m_range(range), m_iterator(range.begin()) {}

  [[nodiscard]] decltype(auto) get() const { return *m_iterator; }

  [[nodiscard]] decltype(auto) get_pointer() const
    requires std::contiguous_iterator<Range_tp>
  {
    return std::to_address(m_iterator);
  }

  void next() { ++m_iterator; }

  void prev()
    requires std::bidirectional_iterator<Iterator_t>
  {
    --m_iterator;
  }

  void advance(std::ptrdiff_t delta)
    requires std::random_access_iterator<Iterator_t>
  {
    m_iterator += delta;
  }

  [[nodiscard]] bool is_equal(const Iterator_with_range &other) const {
    return m_iterator == other.m_iterator;
  }

  [[nodiscard]] std::ptrdiff_t distance_from(
      const Iterator_with_range &other) const
    requires std::random_access_iterator<Iterator_t>
  {
    return m_iterator - other.m_iterator;
  }

  [[nodiscard]] auto &range() { return m_range; }
  [[nodiscard]] const auto &range() const { return m_range; }
  [[nodiscard]] auto &iterator() { return m_iterator; }
  [[nodiscard]] const auto &iterator() const { return m_iterator; }

  [[nodiscard]] bool is_end() const { return m_iterator == m_range.end(); }

 private:
  /// Range object.
  View_source<Range_t> m_range{};

  /// Iterator into the range.
  Iterator_t m_iterator{};
};  // class Iterator_with_range

}  // namespace mysql::ranges

// addtogroup GroupLibsMysqlRanges
/// @}

#endif  // ifndef MYSQL_RANGES_ITERATOR_WITH_RANGE_H
