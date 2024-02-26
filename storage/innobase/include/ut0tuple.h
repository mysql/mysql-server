/*****************************************************************************

Copyright (c) 2020, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ut0tuple.h
  std::tuple helper utilities. */

#ifndef ut0tuple_h
#define ut0tuple_h

#include <tuple>
#include <utility>

// std::index_sequence can only generate a sequence of indices starting from 0.
// This utility, index_sequence_with_offset, enables us to generate a
// sequence of indices with the offset.
//
// E.g.
//  index_sequence_with_offset_t<10, std::make_index_sequence<5>{}>
//  generates a sequence of {10, 11, 12, 13, 14} integers.
template <std::size_t N, typename Seq>
struct index_sequence_with_offset;

template <std::size_t N, std::size_t... Ints>
struct index_sequence_with_offset<N, std::index_sequence<Ints...>> {
  using type = std::index_sequence<Ints + N...>;
};

// Helper alias which saves us from typing ::type
template <std::size_t N, typename Seq>
using index_sequence_with_offset_t =
    typename index_sequence_with_offset<N, Seq>::type;

namespace detail {
template <typename Tuple, size_t... Is>
constexpr auto select_from_tuple_impl(Tuple &&t, std::index_sequence<Is...>) {
  return std::make_tuple(std::get<Is>(t)...);
}
}  // namespace detail

// Utility function which returns a new tuple which is a [Begin, End> subset of
// given tuple.
// E.g.
//  auto t = select_from_tuple<1, 3>(make_tuple(1, 2, 3, 4, 5));
//  assert(t == make_tuple(2, 3));
template <size_t Begin, size_t End, typename Tuple>
constexpr auto select_from_tuple(Tuple &&t) {
  return detail::select_from_tuple_impl(
      std::forward<Tuple>(t),
      index_sequence_with_offset_t<Begin,
                                   std::make_index_sequence<End - Begin>>{});
}

#endif /* ut0tuple_h */
