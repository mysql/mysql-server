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

#ifndef MYSQL_RANGES_PROJECTION_VIEWS_H
#define MYSQL_RANGES_PROJECTION_VIEWS_H

/// @file
/// Experimental API header

#include <cstddef>                        // size_t
#include <iterator>                       // input_iterator
#include <utility>                        // get(pair)
#include "mysql/ranges/transform_view.h"  // Transform_view

/// @addtogroup GroupLibsMysqlRanges
/// @{

namespace mysql::ranges::detail {

/// Function-like class that projects a tuple-like object to the Nth element.
///
/// @tparam index_tp The index of the element to project on.
template <std::size_t index_tp>
struct Projection_transform {
  /// Select component number `index_tp` from `tuple`.
  ///
  /// The element type must not be an rvalue reference.
  ///
  /// The returned type is:
  ///
  /// - a reference, if either the tuple is an lvalue reference, or the element
  ///   type is an lvalue reference.
  ///
  /// - a non-reference, otherwise, i.e., when the element type is a
  ///   non-reference and the tuple type is an rvalue reference. In this case,
  ///   the returned value is move-constructed from the tuple element.
  template <class Tuple_t>
  static decltype(auto) transform(Tuple_t &&tuple) {
    using Noref_tuple_t = std::remove_reference_t<Tuple_t>;
    using Element_t = std::tuple_element_t<index_tp, Noref_tuple_t>;
    static_assert(
        !std::is_rvalue_reference_v<Element_t>,
        "Cannot project to tuple component of rvalue reference type.");
    if constexpr (std::is_rvalue_reference_v<decltype(tuple)> &&
                  !std::is_reference_v<Element_t>) {
      // The parameter is like `std::tuple<T> &&`, where T is not a reference.
      // Thus, the tuple will expire, so we should return a copy of the value
      // to avoid dangling reference after expiration.
      return Element_t(std::move(std::get<index_tp>(tuple)));
    } else {
      return std::get<index_tp>(std::forward<Tuple_t>(tuple));
    }
  }
};

}  // namespace mysql::ranges::detail
namespace mysql::ranges {

/// Projection iterator adaptor: given an iterator over tuple-like objects, this
/// is an iterator over the Nth component.
///
/// @tparam index_tp The index of the element to project on.
///
/// @tparam Source_iterator_tp The source iterator, which should yield
/// tuple-like objects.
template <std::size_t index_tp, std::input_iterator Source_iterator_tp>
using Projection_iterator =
    Transform_iterator<detail::Projection_transform<index_tp>,
                       Source_iterator_tp>;

/// Factory function to create a Projection_iterator.
///
/// @tparam index_t The component index.
///
/// @tparam Tuple_iterator_t The source iterator type.
template <std::size_t index_t, std::input_iterator Tuple_iterator_t>
[[nodiscard]] auto make_projection_iterator(const Tuple_iterator_t &iterator) {
  return Projection_iterator<index_t, Tuple_iterator_t>(iterator);
}

/// Projection view: given a range over tuple-like objects, this is a range over
/// the N'th component of those tuples.
///
/// @tparam index_tp The component index.
///
/// @tparam Source_tp The source type, which should be a range over tuple-like
/// objects.
template <std::size_t index_tp, std::ranges::range Source_tp>
using Projection_view =
    Transform_view<detail::Projection_transform<index_tp>, Source_tp>;

/// Factory function to create a Projection_view.
///
/// @tparam index_t The component index.
///
/// @tparam Source_t The source type.
template <std::size_t index_t, std::ranges::range Source_t>
[[nodiscard]] auto make_projection_view(const Source_t &source) {
  return Projection_view<index_t, Source_t>(source);
}

/// Iterator adaptor that extracts the first component from tuple-like value
/// types. For example, Key_iterator<std::map::iterator> is an iterator over the
/// keys in the map.
///
/// @tparam Value_iterator_t Source iterator that yields value pairs.
template <class Value_iterator_t>
using Key_iterator = Projection_iterator<0, Value_iterator_t>;

/// Factory function to create a new iterator from a given iterator over pairs.
///
/// @param[in] iterator Source iterator that yields value pairs.
[[nodiscard]] auto make_key_iterator(const auto &iterator) {
  return make_projection_iterator<0>(iterator);
}

/// View over the keys of a range of pairs.
///
/// @tparam Source_t Source range.
template <class Source_t>
using Key_view = Projection_view<0, Source_t>;

/// Factory function to create a new view over the keys in a range of pairs.
///
/// @param[in] source Source range.
[[nodiscard]] auto make_key_view(const auto &source) {
  return make_projection_view<0>(source);
}

/// Iterator adaptor that extracts the second component from tuple-like value
/// types. For example, Mapped_iterator<std::map::iterator> is an iterator over
/// the mapped values in the map.
///
/// @tparam Value_iterator_t Source iterator that yields value pairs.
template <class Value_iterator_t>
using Mapped_iterator = Projection_iterator<1, Value_iterator_t>;

/// Factory function to create a new iterator from a given iterator over pairs.
///
/// @param[in] iterator Source iterator that yields value pairs.
[[nodiscard]] auto make_mapped_iterator(const auto &iterator) {
  return make_projection_iterator<1>(iterator);
}

/// View over the mapped values in a range of pairs.
///
/// @tparam Source_t Source range.
template <class Source_t>
using Mapped_view = Projection_view<1, Source_t>;

/// Factory function to create a new view over the mapped values in a range of
/// pairs.
///
/// @param[in] source Source range.
[[nodiscard]] auto make_mapped_view(const auto &source) {
  return make_projection_view<1>(source);
}

}  // namespace mysql::ranges

// addtogroup GroupLibsMysqlRanges
/// @}

#endif  // ifndef MYSQL_RANGES_PROJECTION_VIEWS_H
