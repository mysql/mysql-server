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

#ifndef MYSQL_SETS_STRCONV_ENCODE_H
#define MYSQL_SETS_STRCONV_ENCODE_H

/// @file
/// Experimental API header

#include <concepts>                             // integral
#include "mysql/sets/boundary_set_meta.h"       // Is_boundary_set
#include "mysql/sets/interval.h"                // Interval
#include "mysql/sets/interval_set_interface.h"  // make_interval_set_view
#include "mysql/sets/interval_set_meta.h"       // Is_interval_set
#include "mysql/sets/nested_set_meta.h"         // Is_nested_set
#include "mysql/sets/strconv/boundary_set_text_format.h"  // Boundary_set_text_format
#include "mysql/sets/strconv/nested_set_text_format.h"  // Nested_set_text_format
#include "mysql/strconv/strconv.h"                      // Is_string_target

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::strconv {

using mysql::strconv::Is_format;
using mysql::strconv::Is_string_target;

/// Write an Interval in text format to the given Is_string_target. The Set
/// traits must be _discrete_ (see Is_discrete_set_traits).
///
/// The format is "start" if the start is equal to the inclusive end, and
/// "start<format.boundary_separator>inclusive_end" otherwise.
///
/// @param format Format tag used to write boundary values.
///
/// @param[out] target String_writer or String_counter to write to.
///
/// @param interval Interval to write.
template <mysql::sets::Is_discrete_set_traits Set_traits_t>
void interval_to_text(const Is_format auto &format,
                      Is_string_target auto &target,
                      const mysql::sets::Interval<Set_traits_t> &interval) {
  target.write(format, interval.start());
  if (interval.exclusive_end() != Set_traits_t::next(interval.start())) {
    target.concat(format, format.m_boundary_separator,
                  Set_traits_t::prev(interval.exclusive_end()));
  }
}

/// Write a Boundary_set in text format to the given Is_string_target. The Set
/// traits must be _discrete_ (see Is_discrete_set_traits).
///
/// The format is a sequence of intervals, formatted according to @c
/// interval_to_text, where adjacent pairs of intervals are separated by
/// format.interval_separator.
///
/// @param format Format tag used to write intervals.
///
/// @param[out] target String_writer or String_counter to write to.
///
/// @param boundary_set Boundary set to write.
template <mysql::sets::Is_boundary_set Boundary_set_t>
  requires mysql::sets::Is_discrete_set_traits<
      typename Boundary_set_t::Set_traits_t>
void boundary_set_to_text(const Is_format auto &format,
                          Is_string_target auto &target,
                          const Boundary_set_t &boundary_set) {
  bool first = true;
  for (const auto &interval :
       mysql::sets::make_interval_set_view(boundary_set)) {
    if (first)
      first = false;
    else
      target.write(format, format.m_interval_separator);
    target.write(format, interval);
  }
}

/// Write a boundary set in a space-efficient binary format that uses
/// variable-length integers, delta-compression, and an optimization to
/// sometimes omit the first value. The Set traits must be _discrete_, _metric_
/// (see Is_discrete_metric_set_traits).
///
/// The format has one integer for the sequence length, followed by a sequence
/// where each element is the difference between adjacent boundaries, minus 1.
/// Both the length and the boundaries are encoded according to format.
/// Normally, the sequence contains all boundaries, but in the special case that
/// the first boundary equals the minimum value, we omit it from the sequence.
/// The decoder detects this case by observing that the sequence length is odd,
/// in which case it assumes the start boundary of the first interval is the
/// minimum value, and the exclusive_end is given by the first boundary stored
/// explicitly.
///
/// If boundaries are integers and format is Binary_format, all numbers are
/// stored in variable-length format, as specified by the `serialization`
/// library. (We store differences rather than boundary values because that
/// minimizes integer magnitudes, which saves space.)
///
/// @param format Format tag used to write boundary values.
///
/// @param[out] target String_writer or String_counter to write to.
///
/// @param boundary_set Boundary set to write.
template <mysql::sets::Is_boundary_set Boundary_set_t>
  requires mysql::sets::Is_discrete_metric_set_traits<
      typename Boundary_set_t::Set_traits_t>
void boundary_set_to_binary(const Is_format auto &format,
                            Is_string_target auto &target,
                            const Boundary_set_t &boundary_set) {
  using Set_traits_t = Boundary_set_t::Set_traits_t;
  auto size = boundary_set.size();
  auto next_min = Set_traits_t::min();
  auto it = boundary_set.begin();
  // Skip first boundary if it equals to min
  if (it != boundary_set.end() && *it == next_min) {
    --size;
    ++it;
  }
  next_min = Set_traits_t::next(next_min);
  // Write size
  target.write(format, uint64_t(size));
  // Write all remaining boundaries
  for (; it != boundary_set.end(); ++it) {
    auto boundary = *it;
    target.write(format, Set_traits_t::sub(boundary, next_min));
    next_min = Set_traits_t::next(boundary);
  }
}

/// Write boundary sets in binary format with fixed-width integers to the given
/// Is_string_target. The Set traits must be _bounded_ (see
/// Is_bounded_set_traits).
///
/// The format consists of one integer for the number of intervals (i.e., number
/// of boundaries divided by 2), followed by the sequence of all boundaries.
/// Both the number and the boundaries are encoded according to format.
///
/// If boundaries are integers and format is Format_binary_fixint, all numbers
/// are 64-bit little-endian integers.
///
/// @param format Format tag used to write boundary values.
///
/// @param[out] target String_writer or String_counter to write to.
///
/// @param boundary_set Boundary set to write.
template <mysql::sets::Is_boundary_set Boundary_set_t>
  requires mysql::sets::Is_bounded_set_traits<
      typename Boundary_set_t::Set_traits_t>
void boundary_set_to_binary_fixint(const Is_format auto &format,
                                   Is_string_target auto &target,
                                   const Boundary_set_t &boundary_set) {
  target.write(format, boundary_set.size() / 2);
  for (auto boundary : boundary_set) {
    target.write(format, boundary);
  }
}

/// Write an interval set to the given Is_string_target.
///
/// This delegates the work to the formatter for boundary sets.
///
/// @param format Format tag used to write boundary sets.
///
/// @param[out] target String_writer or String_counter to write to.
///
/// @param interval_set Interval set to write.
void encode_interval_set(
    const Is_format auto &format, Is_string_target auto &target,
    const mysql::sets::Is_interval_set auto &interval_set) {
  target.write(format, interval_set.boundaries());
}

/// Write a nested set to the given Is_string_target.
///
/// This writes each key using the format format.m_nested_set_key_format, each
/// mapped set using the format format.mnested_set_mapped_format, uses
/// format.m_nested_set_key_mapped_separator to separate key and mapped set, and
/// uses format.m_nested_set_item_separator to separate different pairs.
///
/// @param format Format tag.
///
/// @param[out] target String_writer or String_counter to write to.
///
/// @param nested_set Nested set to write.
void nested_set_to_text(
    const mysql::strconv::Is_nested_set_text_format auto &format,
    Is_string_target auto &target,
    const mysql::sets::Is_nested_set auto &nested_set) {
  bool first = true;
  for (auto &&pair : nested_set) {
    if (first)
      first = false;
    else
      target.write_raw(format.m_item_separator);
    target.write(format.m_key_format, pair.first);
    target.write(format, format.m_key_mapped_separator);
    assert(!pair.second.empty());
    target.write(format.m_mapped_format, pair.second);
  }
}

}  // namespace mysql::sets::strconv

// Glue to make encode find the formatters.
namespace mysql::strconv {

/// Enable mysql::strconv::encode(Boundary_set_text_format, Interval), for
/// intervals whose Set traits that are _discrete_.
template <mysql::sets::Is_bounded_set_traits Set_traits_t>
void encode_impl(const Boundary_set_text_format &format,
                 Is_string_target auto &target,
                 const mysql::sets::Interval<Set_traits_t> &interval) {
  mysql::sets::strconv::interval_to_text(format, target, interval);
}

/// Enable mysql::strconv::encode(Boundary_set_text_format, Interval), for
/// boundary sets whose Set traits that are _discrete_.
template <mysql::sets::Is_boundary_set Boundary_set_t>
void encode_impl(const Boundary_set_text_format &format,
                 Is_string_target auto &target,
                 const Boundary_set_t &boundary_set) {
  mysql::sets::strconv::boundary_set_to_text(format, target, boundary_set);
}

/// Enable mysql::strconv::encode(Fixint_binary_format, Boundary_set), for
/// boundary sets whose Set traits that are _discrete_.
template <mysql::sets::Is_boundary_set Boundary_set_t>
void encode_impl(const Fixint_binary_format &format,
                 Is_string_target auto &target,
                 const Boundary_set_t &boundary_set) {
  mysql::sets::strconv::boundary_set_to_binary_fixint(format, target,
                                                      boundary_set);
}

/// Enable mysql::strconv::encode(Binary_format, Boundary_set), for boundary
/// sets whose Set traits that are _discrete_ and _metric_.
template <mysql::sets::Is_boundary_set Boundary_set_t>
  requires mysql::sets::Is_discrete_metric_set_traits<
      typename Boundary_set_t::Set_traits_t>
void encode_impl(const Binary_format &format, Is_string_target auto &target,
                 const Boundary_set_t &boundary_set) {
  mysql::sets::strconv::boundary_set_to_binary(format, target, boundary_set);
}

/// Enable mysql::strconv::encode(/*Format*/, Interval_set), by
/// writing the boundaries in the given format.
void encode_impl(const Is_format auto &format, Is_string_target auto &target,
                 const mysql::sets::Is_interval_set auto &interval_set) {
  mysql::sets::strconv::encode_interval_set(format, target, interval_set);
}

/// Enable mysql::strconv::encode(/*Format*/, Nested_set), by
/// writing the boundaries in the given format.
void encode_impl(const mysql::strconv::Is_nested_set_text_format auto &format,
                 Is_string_target auto &target,
                 const mysql::sets::Is_nested_set auto &nested_set) {
  mysql::sets::strconv::nested_set_to_text(format, target, nested_set);
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_STRCONV_ENCODE_H
