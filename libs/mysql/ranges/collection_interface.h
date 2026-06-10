// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_RANGES_COLLECTION_INTERFACE_H
#define MYSQL_RANGES_COLLECTION_INTERFACE_H
/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlRanges
/// @{

#include <iterator>             // contiguous_iterator
#include "mysql/ranges/meta.h"  // Range_iterator_type

namespace mysql::ranges {

/// CRTP base class to provide members of a *collection* based on an
/// implementation that provides begin/end iterators.
///
/// The implementation must implement either `begin()` and `end()`, or `begin()
/// const` and `end() const` (or all four). It may also override `size() const`
/// and `empty() const`, if that is faster than the default implementations,
/// `std::ranges::distance(begin(), end())` and `begin()==end()`.
///
/// The collection provides the following functionality:
///
/// @code
/// begin();                    // from the implementation
/// begin() const;              // from the implementation
/// end();                      // from the implementation
/// end() const;                // from the implementation
/// cbegin() const;             // begin() const
/// cend() const;               // end() const
///
/// front() const;              // *begin()
/// size() const;               // (size_t)std::ranges::distance(begin(), end())
/// ssize() const;              // (ptrdiff_t)size()
/// empty() const;              // begin() == end()
/// operator!() const;          // empty()
/// operator bool() const;      // !empty()
///
/// // If the iterators are bidirectional:
/// rbegin();                   // make_reverse_iterator(end())
/// rbegin() const;             // make_reverse_iterator(end())
/// rend();                     // make_reverse_iterator(begin())
/// rend() const;               // make_reverse_iterator(begin())
/// rcbegin() const;            // make_reverse_iterator(end())
/// rcend() const;              // make_reverse_iterator(begin())
/// back() const;               // *std::ranges::prev(end())
///
/// // If the iterators are random_access:
/// operator[](size_t n);       // begin()[n]
/// operator[](size_t n) const; // begin()[n]
///
/// // If the iterators are contiguous:
/// data();                     // &*begin()
/// data() const;               // &*begin()
/// @endcode
///
/// This is similar to the C++20 feature `std::ranges::view_interface`. However,
/// not all compilers we build on as of 2025 had implemented view_interface even
/// in C++20 mode. Also, `cbegin` and `cend` are C++23 features, and
/// rbegin/rend/rcbegin/rcend/ssize are non-standard.
///
/// Despite the similarity with `std::ranges::view_interface`, we avoid word
/// "view" because C++ defines that to be objects for which copy and move is
/// cheap (https://en.cppreference.com/w/cpp/ranges/view), whereas this class is
/// usable for anything that provides iterators, cheap-copyable or not.
template <class Self_tp>
class Collection_interface {
  using Self_t = Self_tp;
  template <class Range_t>
  using Iterator_for = mysql::ranges::Range_iterator_type<Range_t>;

 public:
  /// Return constant iterator to the beginning.
  [[nodiscard]] constexpr auto cbegin() const { return self().begin(); }

  /// Return constant iterator to the end.
  [[nodiscard]] constexpr auto cend() const { return self().end(); }

  /// Return reverse iterator to the beginning.
  [[nodiscard]] constexpr auto rbegin() {
    return std::make_reverse_iterator(self().end());
  }

  /// Return reverse iterator to the end.
  [[nodiscard]] constexpr auto rend() {
    return std::make_reverse_iterator(self().begin());
  }

  /// Return const reverse iterator to the beginning.
  [[nodiscard]] constexpr auto rbegin() const {
    return std::make_reverse_iterator(self().end());
  }

  /// Return const reverse iterator to the end.
  [[nodiscard]] constexpr auto rend() const {
    return std::make_reverse_iterator(self().begin());
  }

  /// Return const reverse iterator to the beginning.
  [[nodiscard]] constexpr auto crbegin() const {
    return std::make_reverse_iterator(self().end());
  }

  /// Return const reverse iterator to the end.
  [[nodiscard]] constexpr auto crend() const {
    return std::make_reverse_iterator(self().begin());
  }

  /// Return true if the range is empty, i.e., begin() == end().
  [[nodiscard]] constexpr bool empty() const {
    return self().begin() == self().end();
  }

  /// Return true if the range is non-empty, i.e., begin() != end().
  [[nodiscard]] constexpr explicit operator bool() const { return !empty(); }

  /// Return true if the range is empty, i.e., begin() == end().
  [[nodiscard]] constexpr bool operator!() const { return empty(); }

  /// Return the number of elements in this view, unsigned (size_t), by
  /// computing std::ranges::distance(begin, end)
  [[nodiscard]] constexpr auto size() const {
    // Not std::distance, since that would take linear time for iterators whose
    // value type is not a reference, since they don't model
    // LegacyRandomAccessIterator.
    return std::size_t(std::ranges::distance(self().begin(), self().end()));
  }

  /// Return the number of elements in this view, signed (ptrdiff_t).
  [[nodiscard]] constexpr auto ssize() const { return std::ptrdiff_t(size()); }

  /// Return the first element.
  [[nodiscard]] constexpr decltype(auto) front() const {
    return *self().begin();
  }

  /// Return the last element. Enabled if we have bidirectional iterators.
  [[nodiscard]] constexpr decltype(auto) back() const
    requires(std::bidirectional_iterator<Iterator_for<Self_t>> &&
             std::same_as<Iterator_for<Self_t>,
                          decltype(std::declval<Self_t>().end())>)
  {
    // Not std::prev, since that is undefined for iterators whose value type is
    // not a reference, since they don't model LegacyBidirectionalIterator.
    return *std::ranges::prev(self().end());
  }

  /// Return the n'th element, possibly mutable. Enabled if we have random
  /// access iterators.
  [[nodiscard]] constexpr decltype(auto) operator[](std::ptrdiff_t n)
    requires std::random_access_iterator<Iterator_for<Self_t>>
  {
    return self().begin()[n];
  }

  /// Return the n'th element, const. Enabled if we have random access
  /// iterators.
  [[nodiscard]] constexpr decltype(auto) operator[](std::ptrdiff_t n) const
    requires std::random_access_iterator<Iterator_for<Self_t>>
  {
    return self().begin()[n];
  }

  /// Return pointer to underlying contiguous memory. Enabled if we have
  /// contiguous iterators.
  [[nodiscard]] constexpr auto *data()
    requires std::contiguous_iterator<Iterator_for<Self_t>>
  {
    return &*self().begin();
  }

  /// Return const pointer to underlying contiguous memory. Enabled if we have
  /// contiguous iterators.
  [[nodiscard]] constexpr auto *data() const
    requires std::contiguous_iterator<Iterator_for<Self_t>>
  {
    return &*self().begin();
  }

 private:
  /// Return reference to the implementation class.
  [[nodiscard]] Self_t &self() { return static_cast<Self_t &>(*this); }

  /// Return const reference to the implementation class.
  [[nodiscard]] const Self_t &self() const {
    return static_cast<const Self_t &>(*this);
  }
};  // class Collection_interface

}  // namespace mysql::ranges

// addtogroup GroupLibsMysqlRanges
/// @}

#endif  // ifndef MYSQL_RANGES_COLLECTION_INTERFACE_H
