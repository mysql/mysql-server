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

#ifndef MYSQL_SETS_INTERVAL_H
#define MYSQL_SETS_INTERVAL_H

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlSets
/// @{

#include <cassert>                       // assert
#include <stdexcept>                     // domain_error
#include <utility>                       // forward
#include "mysql/meta/not_decayed.h"      // Not_decayed
#include "mysql/sets/set_traits.h"       // Is_bounded_set_traits
#include "mysql/utils/call_and_catch.h"  // call_and_catch
#include "mysql/utils/return_status.h"   // Return_status

namespace mysql::sets::detail {

/// Holds the start boundary and endpoint boundary of an interval.  The endpoint
/// is always exclusive.
///
/// This base class stores the two values and provides getters, but has all
/// setter functions protected. The protected setter functions do not check that
/// values are in order or in range.
///
/// @tparam Set_traits_tp Bounded set traits describing properties of the
/// element type.
template <Is_bounded_set_traits Set_traits_tp>
class Interval_base {
 public:
  using Set_traits_t = Set_traits_tp;
  using Element_t = Set_traits_t::Element_t;

 protected:
  // Default-construct an interval. The resulting interval has a single element,
  // the smallest value in the Set traits.
  ///
  /// This is enabled if Set_traits_t are *discrete* set traits.
  constexpr Interval_base()
    requires Is_discrete_set_traits<Set_traits_t>
      : Interval_base(Set_traits_t::min()) {}

  /// Construct an interval with the given inclusive start and exclusive end.
  constexpr Interval_base(const Element_t &start,
                          const Element_t &exclusive_end)
      : m_values{start, exclusive_end} {}

  /// Construct a singleton interval.
  ///
  /// This is enabled if Set_traits_t are *discrete* set traits.
  explicit constexpr Interval_base(Element_t singleton)
    requires Is_discrete_set_traits<Set_traits_t>
      : Interval_base(singleton, Set_traits_t::next(singleton)) {}

  // Default rule-of-5

  /// Set both boundaries to the given values, without validating the range.
  ///
  /// @param start_arg The new value for the start boundary.
  ///
  /// @param exclusive_end_arg The new value for the exclusive_end boundary.
  void assign(const Element_t &start_arg, const Element_t &exclusive_end_arg) {
    m_values[0] = start_arg;
    m_values[1] = exclusive_end_arg;
  }

  /// Set the value for the start boundary, without validating the range.
  ///
  /// @param start_arg The new value for the start boundary.
  void set_start(Element_t start_arg) { m_values[0] = start_arg; }

  /// Set the value for the exclusive_end boundary, without validating the
  /// range.
  ///
  /// @param exclusive_end_arg The new value for the exclusive_end boundary.
  void set_exclusive_end(Element_t exclusive_end_arg) {
    m_values[1] = exclusive_end_arg;
  }

 public:
  /// Return const reference to the starting point of the interval
  /// (inclusive).
  [[nodiscard]] constexpr const Element_t &start() const { return m_values[0]; }

  /// Return const reference to the exclusive endpoint of the interval.
  [[nodiscard]] constexpr const Element_t &exclusive_end() const {
    return m_values[1];
  }

 private:
  /// The start boundary at index 0, the exclusive end boundary at index 1.
  Element_t m_values[2];
};  // class Interval_base

/// Return true if both start-point and end-point are equal for the
/// intervals.
template <Is_bounded_set_traits Set_traits_tp>
[[nodiscard]] bool operator==(const Interval_base<Set_traits_tp> &a,
                              const Interval_base<Set_traits_tp> &b) {
  return a.start() == b.start() && a.exclusive_end() == b.exclusive_end();
}

/// Holds the start boundary and endpoint boundary of an interval. The endpoint
/// is always exclusive. The boundaries do not have to be in range and do not
/// have to be in order.
///
/// This is mainly intended for internal use while handling user input. All data
/// structures should contain consistent intervals only.
///
/// This is implemented by inheriting from Interval_base and making the setters
/// public.
///
/// @tparam Set_traits_tp Bounded set traits describing properties of the
/// element type.
template <Is_bounded_set_traits Set_traits_tp>
class Relaxed_interval : public Interval_base<Set_traits_tp> {
 private:
  using Base_t = Interval_base<Set_traits_tp>;
  using This_t = Relaxed_interval<Set_traits_tp>;

 public:
  /// Enable all the (protected) constructors from the base class.
  template <class... Args_t>
    requires mysql::meta::Not_decayed<This_t, Args_t...>
  explicit Relaxed_interval(Args_t &&...args)
      : Base_t(std::forward<Args_t>(args)...) {}

  // Use unchecked versions from the base class.
  using Base_t::assign;
  using Base_t::set_exclusive_end;
  using Base_t::set_start;
};

}  // namespace mysql::sets::detail

namespace mysql::sets {

/// Holds the start boundary and endpoint boundary of an interval.  The endpoint
/// is always exclusive. This class maintains that the boundaries are in range
/// and in order, i.e.:
///
///   Set_traits_tp::min() <= start <  max_exclusive <=
///   Set_traits_tp::max_exclusive()
///
/// The setter functions come in two flavors: one whose name begins with
/// `throwing_`, which throws an exception if the boundaries are out-of-range or
/// out-of-order; one without the prefix that returns a success status. There
/// are also two factory functions, `throwing_make`, which create an interval
/// and throws an exception if the boundaries are out-of-range or out-of-order.
///
/// @tparam Set_traits_tp Bounded set traits describing properties of the
/// element type.
template <Is_bounded_set_traits Set_traits_tp>
class Interval : public detail::Interval_base<Set_traits_tp> {
 public:
  using Set_traits_t = Set_traits_tp;
  using Element_t = Set_traits_t::Element_t;
  using Return_status_t = mysql::utils::Return_status;

 private:
  using Base_t = detail::Interval_base<Set_traits_tp>;

  // NOLINTBEGIN(performance-enum-size): silence clang-tidy's pointless hint

  /// Whether assert_consistent needs to check the start boundary
  enum class Check_start { no, yes };

  /// Whether assert_consistent needs to check the end boundary
  enum class Check_end { no, yes };

  // NOLINTEND(performance-enum-size)

 protected:
  /// Construct an interval with the given values for start and exclusive end.
  ///
  /// @throw std::domain_error if the values are out of range or out of order.
  explicit Interval(const Element_t &start_arg,
                    const Element_t &exclusive_end_arg)
      : Base_t(start_arg, exclusive_end_arg) {
    assert_consistent(this->start(), this->exclusive_end());
  }

  /// Construct a singleton interval.
  ///
  /// @throw std::domain_error if the value is out of range.
  explicit Interval(const Element_t &singleton) : Base_t(singleton) {
    assert_consistent(this->start(), this->exclusive_end());
  }

 public:
  // Default-construct an interval. The resulting interval has a single element,
  // the smallest value in the Set traits.
  Interval() : Base_t() {}

  // default rule-of-5

  /// Construct an interval with the given values for start and exclusive end.
  ///
  /// @throw std::domain_error if the values are out of range or out of order.
  static Interval throwing_make(const Element_t &start_arg,
                                const Element_t &exclusive_end_arg) {
    return Interval(start_arg, exclusive_end_arg);
  }

  /// Construct a singleton interval.
  ///
  /// @throw std::domain_error if the value is out of range.
  static Interval throwing_make(const Element_t &singleton) {
    return Interval(singleton);
  }

  /// Set the start and exclusive end to the given values.
  ///
  /// @throw std::domain_error if the values are out of range or out of order.
  void throwing_assign(const Element_t &start_arg,
                       const Element_t &exclusive_end_arg) {
    assert_consistent(start_arg, exclusive_end_arg);
    Base_t::assign(start_arg, exclusive_end_arg);
  }

  /// Set the start to the given value.
  ///
  /// @throw std::domain_error if the value is out of range or out of order.
  void throwing_set_start(const Element_t &start_arg) {
    assert_consistent<Check_start::yes, Check_end::no>(start_arg,
                                                       this->exclusive_end());
    Base_t::set_start(start_arg);
  }

  /// Set the exclusive end to the given value.
  ///
  /// @throw std::domain_error if the value is out of range or out of order.
  void throwing_set_exclusive_end(const Element_t &exclusive_end_arg) {
    assert_consistent<Check_start::no, Check_end::yes>(this->start(),
                                                       exclusive_end_arg);
    Base_t::set_exclusive_end(exclusive_end_arg);
  }

  /// Set the start and exclusive end to the given values.
  ///
  /// @return ok on success, error if the values are out of range or out of
  /// order.
  [[nodiscard]] Return_status_t assign(const Element_t &start_arg,
                                       const Element_t &exclusive_end_arg) {
    return mysql::utils::call_and_catch(
        [&] { throwing_assign(start_arg, exclusive_end_arg); });
  }

  /// Set the start to the given value.
  ///
  /// @return ok on success, error if the value is out of range or out of
  /// order.
  [[nodiscard]] Return_status_t set_start(const Element_t &start_arg) {
    return mysql::utils::call_and_catch([&] { throwing_set_start(start_arg); });
  }

  /// Set the exclusive end to the given value.
  ///
  /// @return ok on success, error if the value is out of range or out of
  /// order.
  [[nodiscard]] Return_status_t set_exclusive_end(
      const Element_t &exclusive_end_arg) {
    return mysql::utils::call_and_catch(
        [&] { throwing_set_exclusive_end(exclusive_end_arg); });
  }

 private:
  /// Check that the given values are in range and in order.
  ///
  /// @tparam check_start If yes, check that the start value is in range.
  ///
  /// @tparam check_end If yes, check that the end value is in range.
  ///
  /// @throw std::domain_error if the values are out of range or out of order.
  template <Check_start check_start = Check_start::yes,
            Check_end check_end = Check_end::yes>
  void assert_consistent(Element_t start_arg, Element_t exclusive_end_arg) {
    if constexpr (check_start == Check_start::yes) {
      if (!Set_traits_t::le(Set_traits_t::min(), start_arg)) {
        throw std::domain_error{"Out-of-range: start < minimum"};
      }
    }
    if (!Set_traits_t::lt(start_arg, exclusive_end_arg)) {
      throw std::domain_error{"Out-of-order: end <= start"};
    }
    if constexpr (check_end == Check_end::yes) {
      if (!Set_traits_t::le(exclusive_end_arg, Set_traits_t::max_exclusive())) {
        throw std::domain_error{"Out-of-range: end > maximum"};
      }
    }
  }
};

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_INTERVAL_H
