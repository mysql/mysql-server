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

#ifndef MYSQL_SETS_STRCONV_DECODE_H
#define MYSQL_SETS_STRCONV_DECODE_H

/// @file
/// Experimental API header

#include "mysql/sets/boundary_set_meta.h"  // Is_discrete_set_traits
#include "mysql/sets/interval.h"           // detail::Relaxed_interval
#include "mysql/sets/interval_set_meta.h"  // Is_interval_container
#include "mysql/sets/strconv/boundary_set_text_format.h"  // Boundary_set_text_format
#include "mysql/strconv/strconv.h"                        // Parser

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::strconv {

/// Read an interval into the output Relaxed_interval, checking that the
/// boundaries are in range, but not that they are in order. The Set traits must
/// be _discrete_ (see Is_discrete_set_traits).
///
/// The format is one of "start<boundary_separator>inclusive_end" or "start".
/// Note that the text format, contrary to all other places, stores the
/// *inclusive* end value. If the end value is omitted, the inclusive end is
/// equal to start, i.e., a singleton interval.
///
/// This function requires that the start boundary is in the range defined in
/// Set_traits_t. If an end value is given, it requires that that the exclusive
/// endpoint is not less than the minimum inclusive value, and not greater than
/// the maximum exclusive value. It does not require that the start boundary is
/// smaller than the end boundary.
///
/// @param format Format type tag used to read boundary values.
///
/// @param[in,out] parser reference to Parser.
///
/// @param[out] out Destination interval.
template <mysql::sets::Is_discrete_set_traits Set_traits_t>
void interval_from_text(
    const Is_format auto &format, mysql::strconv::Parser &parser,
    mysql::sets::detail::Relaxed_interval<Set_traits_t> &out) {
  using Element_t = typename Set_traits_t::Element_t;
  using mysql::utils::Return_status;

  Element_t start{};
  Element_t inclusive_end{};
  parser
      .fluent(format)          //
      .read(start)             // START
      .check_prev_token([&] {  // check START
        if (!Set_traits_t::in_range(start)) {
          parser.set_parse_error("Interval start out of range");
        }
        inclusive_end = start;
      })                                     //
      .end_optional()                        // String may end here
      .literal(format.m_boundary_separator)  // "-"
      .read(inclusive_end)                   // END
      .check_prev_token([&] {                // check END
        if (Set_traits_t::ge(inclusive_end, Set_traits_t::max_exclusive()) ||
            Set_traits_t::lt(Set_traits_t::next(inclusive_end),
                             Set_traits_t::min())) {
          parser.set_parse_error("Interval end out of range");
        }
      });
  if (parser.is_ok()) {
    out.assign(start, Set_traits_t::next(inclusive_end));
  }
}

/// Read one interval in text format into an output container with a cursor. The
/// Set traits must be _discrete_ (see Is_discrete_set_traits).
///
/// The format is one of "start<boundary_separator>inclusive_end" or "start".
/// Note that the text format, contrary to all other places, stores the
/// *inclusive* end value. If the end value is omitted, the inclusive end is
/// equal to start.
///
/// This function requires that the start boundary is in the range defined in
/// Set_traits_t. If an (inclusive) end value is given, it also requires that
/// the corresponding *exclusive* end is not less than the minimum value. If the
/// start boundary is not less than the end boundary, the interval counts as
/// valid and empty and will not be inserted in the output.
///
/// @param format Format type tag used to read Relaxed_interval objects.
///
/// @param[in,out] parser reference to Parser.
///
/// @param[in,out] out Pair where the first component is a reference to the
/// destination container and the second component is a reference to a cursor to
/// the suggested insertion position.
template <mysql::sets::Is_boundary_container Boundary_container_t>
  requires mysql::sets::Is_discrete_set_traits<
      typename Boundary_container_t::Set_traits_t>
void interval_from_text(
    const Is_format auto &format, mysql::strconv::Parser &parser,
    const std::pair<Boundary_container_t &,
                    typename Boundary_container_t::Iterator_t &> &out) {
  using Set_traits_t = typename Boundary_container_t::Set_traits_t;
  Boundary_container_t &boundary_container = out.first;
  auto &cursor = out.second;
  mysql::sets::detail::Relaxed_interval<Set_traits_t> interval;
  if (parser.read(format, interval) != mysql::utils::Return_status::ok) return;
  if (Set_traits_t::gt(interval.exclusive_end(), interval.start())) {
    if (mysql::utils::call_and_catch([&]() DEDUCED_NOEXCEPT_FUNCTION(
            boundary_container.inplace_union(cursor, interval.start(),
                                             interval.exclusive_end()))) ==
        mysql::utils::Return_status::error) {
      parser.set_oom();
    }
  }
}

/// Parse from text format into a boundary container. The Set traits must be
/// _discrete_ (see Is_discrete_set_traits).
///
/// The format is "((<interval>)?<interval_separator>)*". See @c
/// interval_from_text for the format of `interval`.
///
/// @param format Format type tag used to read intervals into the container.
///
/// @param[in,out] parser reference to Parser.
///
/// @param[in,out] out Destination container.
template <mysql::sets::Is_boundary_container Boundary_container_t>
  requires mysql::sets::Is_discrete_set_traits<
      typename Boundary_container_t::Set_traits_t>
void boundary_set_from_text(const Is_format auto &format,
                            mysql::strconv::Parser &parser,
                            Boundary_container_t &out) {
  // Require fast insertions to avoid degenerate quadratic execution time if
  // intervals are out of order.
  static_assert(
      Boundary_container_t::has_fast_insertion,
      "Use a boundary container type that supports less-than-linear-time "
      "insertion operations, such as Map_boundary_container.");
  auto separator = format.m_interval_separator;
  auto position = out.begin();
  std::pair<Boundary_container_t &, decltype(position) &> interval_cursor(
      out, position);

  auto repeated{mysql::strconv::Allow_repeated_separators::yes};
  auto leading{mysql::strconv::Leading_separators::optional};
  auto trailing{mysql::strconv::Trailing_separators::optional};
  if (format.m_allow_redundant_separators ==
      mysql::strconv::Allow_redundant_separators::no) {
    repeated = mysql::strconv::Allow_repeated_separators::no;
    leading = mysql::strconv::Leading_separators::no;
    trailing = mysql::strconv::Trailing_separators::no;
  }
  std::size_t min_count =
      (format.m_allow_empty == mysql::strconv::Allow_empty::yes) ? 0 : 1;
  parser.fluent(format).read_repeated_with_separators(
      interval_cursor, separator, mysql::strconv::Repeat::at_least(min_count),
      repeated, leading, trailing);
}

/// Parse from binary format (with variable-length integers) into a boundary
/// container. The Set traits must be _discrete_ and _metric_ (see
/// Is_discrete_metric_set_traits).
///
/// The format is equal to that described for boundary_set_to_binary.
///
/// @param format Format type tag used to read boundary values.
///
/// @param[in,out] parser reference to Parser.
///
/// @param[out] out Destination container.
template <mysql::sets::Is_boundary_container Boundary_container_t>
  requires mysql::sets::Is_discrete_metric_set_traits<
      typename Boundary_container_t::Set_traits_t>
void boundary_set_from_binary(const Is_format auto &format,
                              mysql::strconv::Parser &parser,
                              Boundary_container_t &out) {
  using Set_traits_t = typename Boundary_container_t::Set_traits_t;
  using Element_t = typename Set_traits_t::Element_t;
  using Difference_t = typename Set_traits_t::Difference_t;
  using mysql::utils::Return_status;

  // The number of remaining boundaries to read.
  uint64_t remaining{};

  // Sanity-check: the number of boundaries can't be bigger than the remaining
  // bytes.
  auto check_remaining = mysql::strconv::Checker([&] {
    if (remaining > parser.remaining_size()) {
      parser.set_parse_error(
          "The value stored in the size field exceeds the number of remaining "
          "bytes");
    }
  });

  // Read the number of boundaries.
  if (parser.read(format | check_remaining, remaining) != Return_status::ok)
    return;

  // Cursor to output container.
  auto cursor = out.begin();

  // Smallest allowed value for next boundary we read from the container.
  // Because of the optimization that skips the first value if it equals
  // Set_traits_t::min(), the smallest allowed first boundary stored explicitly
  // is min+1.
  Element_t next_min = Set_traits_t::next(Set_traits_t::min());

  // Value to read.
  Difference_t delta{};

  // Check that the delta is in range.
  auto check_delta = mysql::strconv::Checker([&] {
    if (delta > Set_traits_t::sub(Set_traits_t::max_exclusive(), next_min)) {
      parser.set_parse_error("Value exceeds maximum");
    }
  });

  // Read one boundary into out. Return ok or error.
  auto read_next = [&](Element_t &element) {
    if (parser.read(format | check_delta, delta) != Return_status::ok)
      return Return_status::error;
    element = Set_traits_t::add(next_min, delta);
    next_min = Set_traits_t::next(element);
    return Return_status::ok;
  };

  // Insert the given interval into the container. Return ok or error.
  auto insert = [&](const Element_t &start, const Element_t &exclusive_end) {
    auto ret = mysql::utils::call_and_catch([&]() DEDUCED_NOEXCEPT_FUNCTION(
        out.inplace_union(cursor, start, exclusive_end)));
    if (ret != Return_status::ok) parser.set_oom();
    return ret;
  };

  // Special case to read the endpoint of the first interval, in case
  // 'remaining' is odd.
  if ((remaining & 1) == 1) {
    Element_t exclusive_end{Set_traits_t::min()};
    if (read_next(exclusive_end) != Return_status::ok) return;
    if (insert(Set_traits_t::min(), exclusive_end) != Return_status::ok) return;
    --remaining;
  }

  // Read two values at a time and insert them into the container.
  while (remaining != 0) {
    Element_t start;
    if (read_next(start) != Return_status::ok) return;
    Element_t exclusive_end;
    if (read_next(exclusive_end) != Return_status::ok) return;
    if (insert(start, exclusive_end) != Return_status::ok) return;
    remaining -= 2;
  }
}

/// Parse from binary format (with fixed-length integers) into a boundary
/// container. The Set traits must be _bounded_ (see Is_bounded_set_traits).
///
/// The format is equal to that described for boundary_set_to_binary.
///
/// @param format Format type tag used to read boundary values.
///
/// @param[in,out] parser reference to Parser.
///
/// @param[out] out Destination container.
template <mysql::sets::Is_boundary_container Boundary_container_t>
  requires mysql::sets::Is_bounded_set_traits<
      typename Boundary_container_t::Set_traits_t>
void boundary_set_from_binary_fixint(const Is_format auto &format,
                                     mysql::strconv::Parser &parser,
                                     Boundary_container_t &out) {
  using Set_traits_t = typename Boundary_container_t::Set_traits_t;
  using Element_t = typename Set_traits_t::Element_t;
  using mysql::utils::Return_status;

  // The number of boundaries to read.
  uint64_t remaining_intervals{};

  // Sanity-check: the number of intervals times the encoded size of an interval
  // can't be bigger than the remaining bytes.
  auto check_remaining = mysql::strconv::Checker([&] {
    if (remaining_intervals * 8 * 2 > parser.remaining_size()) {
      parser.set_parse_error(
          "The value stored in the size field exceeds the number of values "
          "that fit in the remaining string");
    }
  });

  // Read the number of boundaries.
  if (parser.read(format | check_remaining, remaining_intervals) !=
      Return_status::ok)
    return;

  // Cursor to output container.
  auto cursor = out.begin();
  // Smallest allowed value for next boundary.
  Element_t last_value{};
  // True only while processing the first boundary.
  bool first{true};

  // Read one boundary into `element`. Return ok or error.
  auto read_next = [&](Element_t &value) {
    auto check_value = mysql::strconv::Checker([&] {
      if (first) {
        if (Set_traits_t::lt(value, Set_traits_t::min()))
          parser.set_parse_error("Value is less than minimum");
      } else {
        if (Set_traits_t::le(value, last_value))
          parser.set_parse_error(
              "Value is less than or equal to previous value");
      }
      if (Set_traits_t::gt(value, Set_traits_t::max_exclusive()))
        parser.set_parse_error("Value exceeds maximum");
    });

    if (parser.read(format | check_value, value) != Return_status::ok)
      return Return_status::error;
    last_value = value;
    first = false;
    return Return_status::ok;
  };

  // Insert the given interval into the container. Return ok or error.
  auto insert = [&](Element_t start, Element_t exclusive_end) {
    auto ret = mysql::utils::call_and_catch([&]() DEDUCED_NOEXCEPT_FUNCTION(
        out.inplace_union(cursor, start, exclusive_end)));
    if (ret != Return_status::ok) parser.set_oom();
    return ret;
  };

  // Read two values at a time and insert them into the container.
  while (remaining_intervals) {
    Element_t start;
    if (read_next(start) != Return_status::ok) return;
    Element_t exclusive_end;
    if (read_next(exclusive_end) != Return_status::ok) return;
    if (insert(start, exclusive_end) != Return_status::ok) return;
    --remaining_intervals;
  }
}

/// Parse into an interval container, advance the position, and return the
/// status.
///
/// See above for details.
template <mysql::sets::Is_interval_container Interval_container_t>
void decode_interval_set(const Is_format auto &format,
                         mysql::strconv::Parser &parser,
                         Interval_container_t &out) {
  std::ignore = parser.read(format, out.boundaries());
}

}  // namespace mysql::sets::strconv

// Glue to make mysql::strconv find the parsers.
namespace mysql::strconv {

/// Enable mysql::strconv::decode(Text_format, Relaxed_interval), for
/// boundary containers whose Set traits that are _discrete_.
template <mysql::sets::Is_discrete_set_traits Set_traits_t>
void decode_impl(const Boundary_set_text_format &format,
                 mysql::strconv::Parser &parser,
                 mysql::sets::detail::Relaxed_interval<Set_traits_t> &out) {
  mysql::sets::strconv::interval_from_text(format, parser, out);
}

/// Enable mysql::strconv::decode(Text_format,
/// std::pair<Boundary_container, cursor>), for boundary containers whose Set
/// traits that are _discrete_.
template <mysql::sets::Is_boundary_container Boundary_container_t>
  requires mysql::sets::Is_discrete_set_traits<
      typename Boundary_container_t::Set_traits_t>
void decode_impl(
    const Boundary_set_text_format &format, mysql::strconv::Parser &parser,
    const std::pair<Boundary_container_t &,
                    typename Boundary_container_t::Iterator_t &> &out) {
  mysql::sets::strconv::interval_from_text(format, parser, out);
}

/// Enable mysql::strconv::decode(Text_format, Boundary_container), for
/// boundary containers whose Set traits that are _discrete_.
template <mysql::sets::Is_boundary_container Boundary_container_t>
  requires mysql::sets::Is_discrete_set_traits<
      typename Boundary_container_t::Set_traits_t>
void decode_impl(const Boundary_set_text_format &format,
                 mysql::strconv::Parser &parser, Boundary_container_t &out) {
  mysql::sets::strconv::boundary_set_from_text(format, parser, out);
}

/// Enable mysql::strconv::decode(Binary_format, Boundary_container),
/// for boundary containers that are _discrete_ and _metric_.
template <mysql::sets::Is_boundary_container Boundary_container_t>
  requires mysql::sets::Is_discrete_metric_set_traits<
      typename Boundary_container_t::Set_traits_t>
void decode_impl(const Binary_format &format, mysql::strconv::Parser &parser,
                 Boundary_container_t &out) {
  mysql::sets::strconv::boundary_set_from_binary(format, parser, out);
}

/// Enable mysql::strconv::decode(Fixint_binary_format,
/// Boundary_container), for boundary containers that are _discrete_.
template <mysql::sets::Is_boundary_container Boundary_container_t>
  requires mysql::sets::Is_discrete_set_traits<
      typename Boundary_container_t::Set_traits_t>
void decode_impl(const Fixint_binary_format &format,
                 mysql::strconv::Parser &parser, Boundary_container_t &out) {
  mysql::sets::strconv::boundary_set_from_binary_fixint(format, parser, out);
}

/// Enable mysql::strconv::decode(/*Format*/, Interval_container), by
/// reading the boundaries in the given format.
template <mysql::sets::Is_interval_container Interval_container_t>
void decode_impl(const Is_format auto &format, mysql::strconv::Parser &parser,
                 Interval_container_t &out) {
  mysql::sets::strconv::decode_interval_set(format, parser, out);
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_STRCONV_DECODE_H
