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

#ifndef MYSQL_RANGES_FLAT_VIEW_H
#define MYSQL_RANGES_FLAT_VIEW_H

/// @file
/// Experimental API header

#include <ranges>                                // view_base
#include "mysql/iterators/iterator_interface.h"  // Iterator_interface
#include "mysql/ranges/collection_interface.h"   // Collection_interface
#include "mysql/ranges/iterator_with_range.h"    // Iterator_with_range
#include "mysql/ranges/meta.h"                   // Iterator_value_type
#include "mysql/ranges/view_sources.h"           // View_source

/// @addtogroup GroupLibsMysqlRanges
/// @{

namespace mysql::ranges {

/// Default Unfold class.
///
/// In general, an *Unfold class* has a static member function `unfold` that
/// takes an object as argument and returns a `range` (satisfying
/// `std::ranges::range`). `T` can be thought of as a "placeholder" from which
/// an appropriate Unfold class can extract a range.
///
/// This Unfold class is defined for the special case that `T` is a range
/// already, and its `unfold` member function returns the parameter unchanged.
struct Default_unfold {
  [[nodiscard]] static constexpr const auto &unfold(
      const std::ranges::range auto &source) {
    return source;
  }
};

/// True if `Unfold_t<Source_t>::unfold(Source_t)` is defined and returns a
/// range.
template <class Source_t, class Unfold_t>
concept Can_unfold_with = requires(Source_t source) {
                            { Unfold_t::unfold(source) } -> std::ranges::range;
                          };

/// Provides the type of the range returned from `unfold<Unfold_t>(Source_t)`.
template <class Source_t, class Unfold_t>
using Unfolded_type =
    std::remove_cvref_t<decltype(Unfold_t::unfold(std::declval<Source_t>()))>;

/// True if Source_t can be unfolded, and also the value type for the unfolded
/// range can be unfolded, using the given Unfold_t function.
template <class Source_t, class Unfold_t>
concept Can_unfold_twice_with =
    Can_unfold_with<Source_t, Unfold_t> &&
    Can_unfold_with<Range_value_type<Unfolded_type<Source_t, Unfold_t>>,
                    Unfold_t>;

/// Forward declaration.
template <class Source_tp, class Unfold_tp>
class Flat_view;

/// Factory function to create a range view over a flattened sequence of
/// elements from given source.
///
/// For example, if `source` is of type `std::vector<std::map<A,
/// std::list<B>>>`, `make_flat_view(source)` is a range view over `B` objects.
///
/// In case any map or list is empty, it is only skipped over; the resulting
/// range contains only valid elements and no "gaps".
///
/// @tparam Unfold_t Unfold class. This is used to obtain ranges from the
/// source, and from its sub-objects. For example, we expect
/// `make_flat_view(std::map<A, std::map<B, C>>&)` to provide iterators over
/// `C`. While unfolding the outer map, the implementation needs a range over
/// `std::map<B, C>` objects, and while unfolding each such inner `std::map<B,
/// C>` object, the implementation needs a range over `C` objects. But
/// `std::map` does not give that; it only gives ranges over `std::pair<A,
/// std::map<B, C>>` and `std::pair<B, C>`. In such cases, the user needs to
/// provide a custom Unfold class through this parameter, which should have a
/// member function `unfold(std::map<X, Y>)` returning a range over `Y` objects.
///
/// @tparam Source_t Type of source.
///
/// @param source Source object.
template <class Unfold_t = Default_unfold, Can_unfold_with<Unfold_t> Source_t>
[[nodiscard]] decltype(auto) make_flat_view(const Source_t &source) {
  if constexpr (Can_unfold_twice_with<Source_t, Unfold_t>) {
    return Flat_view<Source_t, Unfold_t>(source);
  } else {
    return Unfold_t::unfold(source);
  }
}

/// Type of the `flat view` returned from `make_flat_view<Unfold_t>(Source_T&)`.
template <class Source_t, class Unfold_t>
  requires Can_unfold_with<Source_t, Unfold_t>
using Flat_view_type = std::remove_cvref_t<decltype(make_flat_view<Unfold_t>(
    std::declval<Source_t>()))>;

/// Iterator adaptor that recursively flattens the sequence of a given iterator
/// over a nested sequence.
///
/// For each value `v` yielded by iterators of the range unfolded from the given
/// source, this iterator recursively flattens the range given by
/// Unfold_t::unfold(v), and yields all elements in that flattened sequence.
///
/// This iterator implements the recursive step of procedure to flatten a range.
/// The base case occurs when `Unfold::unfold(v)` is not defined, in which case
/// make_flat_view provides the range unfolded from the source without using
/// this class to attempt to flatten it.
///
/// @tparam Outer_range_tp Type of the outermost range. It is required that
/// `Unfold_tp::unfold(v)` is defined, where `v` is an object of the range's
/// value type.
///
/// @tparam Unfold_tp Class to obtain range views from the range's values; from
/// the values of those range views, and so on, recursively.
template <class Outer_range_tp, class Unfold_tp>
class Flat_iterator : public mysql::iterators::Iterator_interface<
                          Flat_iterator<Outer_range_tp, Unfold_tp>> {
  // Example: Suppose we have the nested structure `set<map<int, float>>` and we
  // wish to get a linear sequence of all `float` values. Then we instantiate
  // this class with the following template parameters:
  //
  // - Outer_range_tp = set<map<int float>>
  //
  // - Unfold_tp is class such that `Unfold_tp::unfold(x)` returns
  //   `make_value_view(x)` whenever `x` is of type `map<int, float>`.
  //
  // The member types are then as follows:
  //
  // Outer_iterator_t = set<map<int, float>>::iterator
  // Inner_source_t = map<int, float>
  // Inner_range_t = Value_view<map<int, float>>
  using Outer_range_t = Outer_range_tp;
  using Unfold_t = Unfold_tp;
  using Outer_iterator_t =
      mysql::ranges::Range_const_iterator_type<Outer_range_t>;
  using Inner_source_t = Iterator_value_type<Outer_iterator_t>;
  using Inner_range_t = Flat_view_type<Inner_source_t, Unfold_t>;

  using Outer_t = Iterator_with_range<Outer_range_t>;
  using Inner_t = Iterator_with_range<Inner_range_t>;

 public:
  Flat_iterator() = default;
  explicit Flat_iterator(const Outer_range_t &outer_range,
                         const Outer_iterator_t &outer_iterator)
      : m_outer(outer_range, outer_iterator) {
    if (!m_outer.is_end()) {
      reset_inner();
      fix_position();
    }
  }
  explicit Flat_iterator(const Outer_range_t &outer_range)
      : Flat_iterator(outer_range, outer_range.begin()) {}

  [[nodiscard]] decltype(auto) get() const { return *m_inner; }

  void next() {
    ++m_inner;
    fix_position();
  }

  [[nodiscard]] bool is_equal(const Flat_iterator &other) const {
    if (m_outer != other.m_outer) return false;
    if (m_outer.is_end()) return true;
    return m_inner == other.m_inner;
  }

 private:
  void reset_inner() { m_inner = Inner_t(make_flat_view<Unfold_t>(*m_outer)); }

  /// While not at a valid position, advance the positions.
  ///
  /// A "valid position" is one where either the outer iterator points to the
  /// end, or the inner iterator does not point to the end.
  void fix_position() {
    while (m_inner.is_end()) {
      ++m_outer;
      if (m_outer.is_end()) return;
      reset_inner();
    }
  }

  /// The outer range and current iterator to it.
  Outer_t m_outer;

  /// The range and iterator that m_outer.iterator() currently points to.
  Inner_t m_inner;
};  // class Flat_iterator

/// Returns a flat iterator over the range starting at `iterator`. This is the
/// appropriate `Flat_iterator` wrapper if `Can_unfold_with` is true for
/// `Iterator_t`'s value type; otherwise it is `iterator` itself.
template <class Unfold_t, class Range_t>
[[nodiscard]] auto make_flat_iterator(
    const Range_t &range, const Range_const_iterator_type<Range_t> &iterator) {
  constexpr bool has_inner_range =
      Can_unfold_with<Range_value_type<Range_t>, Unfold_t>;
  if constexpr (has_inner_range) {
    return Flat_iterator<Range_t, Unfold_t>(range, iterator);
  } else {
    return iterator;
  }
}

/// Flat view over the innermost elements of a type that unfolds to a range,
/// whose value type also unfolds to a range (possibly recursively).
///
/// @tparam Source_tp Type that can be unfolded to a range.
///
/// @tparam Unfold_tp Unfold function.
template <class Source_tp, class Unfold_tp>
class Flat_view : public Collection_interface<Flat_view<Source_tp, Unfold_tp>>,
                  public std::ranges::view_base {
  using Source_t = Source_tp;
  using Unfold_t = Unfold_tp;

  using Range_t = Unfolded_type<Source_t, Unfold_t>;
  using Range_ref_t = View_source<Range_t>;

 public:
  Flat_view() = default;
  explicit Flat_view(const Source_t &source)
      : m_range(Unfold_t::unfold(source)) {}

  [[nodiscard]] auto begin() const {
    decltype(auto) range = m_range.get();
    return make_flat_iterator<Unfold_t>(range, range.begin());
  }
  [[nodiscard]] auto end() const {
    decltype(auto) range = m_range.get();
    return make_flat_iterator<Unfold_t>(range, range.end());
  }

 private:
  Range_ref_t m_range;
};  // class Flat_view

}  // namespace mysql::ranges

// addtogroup GroupLibsMysqlRanges
/// @}

#endif  // ifndef MYSQL_RANGES_FLAT_VIEW_H
