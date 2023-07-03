/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_STDX_RANGES_ENUMERATE_H
#define MYSQL_HARNESS_STDX_RANGES_ENUMERATE_H

//
// From C++23's P2164
//
// http://wg21.link/p2164

#include <iterator>
#include <tuple>
#include <utility>  // declval, forward

#include "mysql/harness/stdx/iterator.h"  // iter_reference, iter_value

namespace stdx::ranges {

template <class T>
using iterator_t = decltype(std::begin(std::declval<T &>()));

template <class R>
using range_value_t = stdx::iter_value_t<ranges::iterator_t<R>>;

template <class R>
using range_reference_t = stdx::iter_reference_t<ranges::iterator_t<R>>;

/**
 * enumerate_view over a range.
 *
 * @note only implements the const-iterator parts.
 *
 * @tparam V a range to enumerate
 */
template <class V>
class enumerate_view {
 private:
  using Base = V;

  Base base_ = {};

  template <bool>
  class iterator;

 public:
  using value_type = stdx::iter_value_t<iterator<true>>;

  constexpr enumerate_view() = default;

  constexpr enumerate_view(V base) : base_(std::forward<Base>(base)) {}

  constexpr auto begin() const { return iterator<true>{std::begin(base_), 0}; }

  constexpr auto end() const { return iterator<true>{std::end(base_), 0}; }
};

template <class R>
enumerate_view(R &&) -> enumerate_view<R>;

template <class V>
template <bool Const>
class enumerate_view<V>::iterator {
 private:
  using Base = std::conditional_t<Const, const V, V>;

 public:
  using iterator_category = std::input_iterator_tag;

  using index_type = size_t;
  using reference = std::tuple<index_type, range_reference_t<Base>>;
  using value_type = std::tuple<index_type, range_value_t<Base>>;

  constexpr explicit iterator(iterator_t<Base> current, index_type pos)
      : pos_{pos}, current_{std::move(current)} {}

  constexpr bool operator!=(const iterator &other) const {
    return current_ != other.current_;
  }

  constexpr iterator &operator++() {
    ++pos_;
    ++current_;

    return *this;
  }

  constexpr decltype(auto) operator*() const {
    return reference{pos_, *current_};
  }

 private:
  index_type pos_;

  iterator_t<Base> current_;
};

namespace views {
/*
 * an iterator that wraps an iterable and returns a counter and the
 * deref'ed wrapped iterable.
 *
 * @tparam T a iterable
 *
 * @code
 * for (auto [ndx, vc]: enumerate(std::vector<int>{1, 23, 42})) {
 *   std::cerr << "[" << ndx << "] " << v << "\n";
 * }
 *
 * // [0] 1
 * // [1] 23
 * // [2] 42
 * @endcode
 *
 * modelled after P2164 from C++23, but implemented for C++17 (aka without
 * ranges and views)
 */
template <class T, class TIter = decltype(std::begin(std::declval<T>())),
          class = decltype(std::end(std::declval<T>()))>
constexpr auto enumerate(T &&iterable) {
  return enumerate_view{std::forward<T>(iterable)};
}
}  // namespace views
}  // namespace stdx::ranges

namespace stdx {
namespace views = ranges::views;
}
#endif
