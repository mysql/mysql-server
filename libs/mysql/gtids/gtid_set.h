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

#ifndef MYSQL_GTIDS_GTID_SET_H
#define MYSQL_GTIDS_GTID_SET_H

/// @file
/// Experimental API header

#include <concepts>                                   // same_as
#include "mysql/gtids/gtid.h"                         // Gtid
#include "mysql/gtids/sequence_number.h"              // Sequence_number
#include "mysql/gtids/tsid.h"                         // Tsid
#include "mysql/meta/not_decayed.h"                   // Not_decayed
#include "mysql/sets/aliases.h"                       // Map_interval_container
#include "mysql/sets/int_set_traits.h"                // Int_set_traits
#include "mysql/sets/nested_set_predicates.h"         // contains_element
#include "mysql/sets/ordered_set_traits_interface.h"  // Ordered_set_traits_interface

/// @addtogroup GroupLibsMysqlGtids
/// @{

namespace mysql::gtids {

/// Set_traits used when TSIDs are stored in sets.
struct Tsid_traits
    : public mysql::sets::Ordered_set_traits_interface<Tsid_traits, Tsid> {
  [[nodiscard]] static bool lt(const Tsid &left, const Tsid &right) {
    return left < right;
  }
};

namespace detail {
/// Type alias that defines Set_traits for gtids::Sequence_number.
///
/// Use Sequence_number_traits instead: that is less verbose when it appears
/// in error messages or stack traces.
using Sequence_number_traits_alias =
    mysql::sets::Int_set_traits<Sequence_number, sequence_number_min,
                                sequence_number_max_exclusive>;
}  // namespace detail

/// Class that defines Set_traits for gtids::Sequence_number.
struct Sequence_number_traits : public detail::Sequence_number_traits_alias {};

namespace detail {
/// Type alias that defines the Interval type used for Gtid intervals.
///
/// Use Gtid_interval instead: that is less verbose when it appears in error
/// messages or stack traces.
using Gtid_interval_alias = mysql::sets::Interval<Sequence_number_traits>;
}  // namespace detail

/// Class that defines the Interval type used for Gtid intervals.
class Gtid_interval : public detail::Gtid_interval_alias {
  using Base_t = detail::Gtid_interval_alias;

  /// Construct an interval with the given values for start and exclusive end.
  ///
  /// @throw std::domain_error if the values are out of range or out of order.
  explicit Gtid_interval(const Element_t &start_arg,
                         const Element_t &exclusive_end_arg)
      : Base_t(start_arg, exclusive_end_arg) {}

  /// Construct a singleton interval.
  ///
  /// @throw std::domain_error if the value is out of range.
  explicit Gtid_interval(const Element_t &singleton) : Base_t(singleton) {}

 public:
  // Default-construct an interval. The resulting interval has a single element,
  // the smallest value in the Set traits.
  Gtid_interval() : Base_t() {}

  // default rule-of-5

  /// Construct an interval with the given values for start and exclusive end.
  ///
  /// @throw std::domain_error if the values are out of range or out of order.
  static Gtid_interval throwing_make(const Element_t &start_arg,
                                     const Element_t &exclusive_end_arg) {
    return Gtid_interval(start_arg, exclusive_end_arg);
  }

  /// Construct a singleton interval.
  ///
  /// @throw std::domain_error if the value is out of range.
  static Gtid_interval throwing_make(const Element_t &singleton) {
    return Gtid_interval(singleton);
  }
};

namespace detail {
/// Type alias that defines the Interval set type used for Gtid intervals.
///
/// Use Gtid_interval_set instead: that is less verbose when it appears in error
/// messages or stack traces.
using Gtid_interval_set_alias =
    mysql::sets::Map_interval_container<Sequence_number_traits>;
}  // namespace detail

/// Class that defines the Interval set type used for Gtid intervals.
class Gtid_interval_set : public detail::Gtid_interval_set_alias {
  using Base_t = detail::Gtid_interval_set_alias;

 public:
  using Base_t::Set_category_t;
  using Base_t::Set_traits_t;

  /// Enable all constructors from Map_interval_container.
  template <class... Args_t>
    requires mysql::meta::Not_decayed<Gtid_interval_set, Args_t...>
  explicit Gtid_interval_set(Args_t &&...args) noexcept
      : detail::Gtid_interval_set_alias(std::forward<Args_t>(args)...) {}
};

namespace detail {
/// Type alias that defines the Set type used for Gtid sets.
///
/// Use Gtid_set instead: that is less verbose when it appears in error messages
/// or stack traces.
using Gtid_set_alias =
    mysql::sets::Map_nested_container<Tsid_traits, Gtid_interval_set>;
}  // namespace detail

/// Class that defines the Set type used for Gtid sets.
class Gtid_set : public detail::Gtid_set_alias {
  using Base_t = detail::Gtid_set_alias;

 public:
  /// Enable all constructors from Map_nested_container.
  template <class... Args_t>
    requires mysql::meta::Not_decayed<Gtid_set, Args_t...>
  explicit Gtid_set(Args_t &&...args) noexcept
      : Base_t(std::forward<Args_t>(args)...) {}

  /// Enable all `insert` overloads from the base class, as well as the one
  /// defined below.
  using Base_t::insert;

  /// `insert` taking a Gtid argument.
  [[nodiscard]] auto insert(const Is_gtid auto &gtid) {
    return Base_t::insert(gtid.tsid(), gtid.get_sequence_number());
  }

  /// Enable all `remove` overloads from the base class, as well as the one
  /// defined below.
  using Base_t::remove;

  /// `remove` taking a Gtid argument.
  [[nodiscard]] auto remove(const Is_gtid auto &gtid) noexcept {
    return Base_t::remove(gtid.tsid(), gtid.get_sequence_number());
  }
};

/// True for all Gtid set types.
template <class Test>
concept Is_gtid_set =
    mysql::sets::Is_nested_set_over_traits<Test, Gtid_set::Set_traits_t>;

}  // namespace mysql::gtids

namespace mysql::sets {

/// `contains_element` for Gtid_sets, accepting a Gtid for the element.
[[nodiscard]] bool contains_element(
    const mysql::gtids::Is_gtid_set auto &gtid_set,
    const mysql::gtids::Is_gtid auto &gtid) noexcept {
  return contains_element(gtid_set, gtid.tsid(), gtid.get_sequence_number());
}

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlGtids
/// @}

#endif  // ifndef MYSQL_GTIDS_GTID_SET_H
