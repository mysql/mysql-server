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

#ifndef MYSQL_RANGES_META_H
#define MYSQL_RANGES_META_H

/// @file
/// Experimental API header

#include <concepts>  // same_as
#include <iterator>  // forward_iterator
#include <utility>   // declval

/// @addtogroup GroupLibsMysqlRanges
/// @{

namespace mysql::ranges {

/// Gives the iterator type, deduced from the `begin()` member.
template <class Range_t>
using Range_iterator_type =
    std::remove_cvref_t<decltype(std::declval<Range_t>().begin())>;

/// Gives the const_iterator type, deduced from the `begin() const` member.
template <class Range_t>
using Range_const_iterator_type =
    std::remove_cvref_t<decltype(std::declval<const Range_t>().begin())>;

/// Gives the iterator type, deduced from the `end()` member.
template <class Range_t>
using Range_sentinel_type =
    std::remove_cvref_t<decltype(std::declval<Range_t>().end())>;

/// True if Iterator is either the iterator or the const iterator for
/// Range_t.
template <class Iterator_t, class Range_t>
concept Is_iterator_for_range =
    std::same_as<Iterator_t, Range_iterator_type<Range_t>>;

/// Gives the value type for any iterator type, deduced from `operator *`.
template <class Iterator_t>
using Iterator_value_type =
    std::remove_cvref_t<decltype(*std::declval<Iterator_t>())>;

/// Gives the value type for any collection, deduced from `*begin()`.
template <class Range_t>
using Range_value_type = Iterator_value_type<Range_iterator_type<Range_t>>;

/// Gives the key type for any collection, deduced from `begin()->first`.
///
/// For standard types such as `std::map`, the member type `std::map::key_type`
/// can be used instead. This deduced type is usable in other cases, for
/// example, when a map is "emulated" using `std::vector<std::pair<K, V>>`, or
/// for user-defined types that do no have a `key_type` member.
template <class Map_t>
using Map_key_type =
    std::remove_cvref_t<decltype(std::declval<Map_t>().begin()->first)>;

/// Gives the mapped type for any collection, deduced from `begin()->first`.
///
/// For standard types such as `std::map`, the member type
/// `std::map::mapped_type` can be used instead. This deduced type is usable in
/// other cases, for example, when a map is "emulated" using
/// `std::vector<std::pair<K, V>>`, or for user-defined types that do no have a
/// `key_type` member.
template <class Map_t>
using Map_mapped_type =
    std::remove_cvref_t<decltype(std::declval<Map_t>().begin()->second)>;

/// True if `Test` has the properties of a "collection", i.e., a range with
/// member functions to query size and emptiness.
///
/// It requires the following:
///
/// - t.begin() returns a forward iterator
///
/// - t.end() returns a sentinel for t.begin()
///
/// - t.size() returns std::size_t
///
/// - t.ssize() returns std::ptrdiff_t
///
/// - t.empty() and (bool)t return bool
///
/// In addition, the following semantic requirements must hold:
///
/// - t.size() == std::ranges::distance(t.begin(), t.end())
///
/// - t.ssize() == std::ptrdiff_t(t.size())
///
/// - t.empty() == (t.size() == 0)
///
/// - t.empty() == !(bool)t
template <class Test>
concept Is_collection =
    requires(const Test ct, Test t)  // this comment helps clang-format
{
  { t.begin() } -> std::forward_iterator;
  { t.end() } -> std::sentinel_for<decltype(t.begin())>;
  { ct.begin() } -> std::forward_iterator;
  { ct.end() } -> std::sentinel_for<decltype(ct.begin())>;
  { ct.size() } -> std::same_as<std::size_t>;
  { ct.ssize() } -> std::same_as<std::ptrdiff_t>;
  { ct.empty() } -> std::same_as<bool>;
  { (bool)ct } -> std::same_as<bool>;
};

/// True if `Test` models `Is_collection`, with `Value_t` as its value
/// type.
template <class Test, class Value_t>
concept Is_collection_over =
    Is_collection<Test> &&
    std::same_as<mysql::ranges::Range_value_type<Test>, Value_t>;

}  // namespace mysql::ranges

// addtogroup GroupLibsMysqlContainers
/// @}

#endif  // ifndef MYSQL_RANGES_META_H
