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

#ifndef MYSQL_SETS_ORDERED_SET_TRAITS_INTERFACE_H
#define MYSQL_SETS_ORDERED_SET_TRAITS_INTERFACE_H

/// @file
/// Experimental API header

#include <compare>                  // strong_ordering
#include "mysql/sets/set_traits.h"  // Base_set_traits

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// Helper class to define the `Ordered_set_traits_interface::Less_t` member.
///
/// This can be used by standard algorithms/classes such as std::set/std::sort
/// that need a function object to compare elements.
///
/// @tparam Element_tp Type of values.
///
/// @tparam Impl_tp Set traits implementation class.
template <class Impl_tp, class Element_tp>
struct Less {
  /// Return `Impl_tp::lt(left, right)`.
  [[nodiscard]] constexpr bool operator()(const Element_tp &left,
                                          const Element_tp &right) const {
    return Impl_tp::lt(left, right);
  }
};

}  // namespace mysql::sets::detail

namespace mysql::sets {

template <class Test, class Element_t>
concept Is_ordered_set_implementation =
    requires(Element_t e1, Element_t e2) {
      { Test::lt_impl(e1, e2) } -> std::convertible_to<bool>;
    } ||  // this comment helps clang-format
    requires(Element_t e1, Element_t e2) {
      { Test::cmp_impl(e1, e2) } -> std::convertible_to<std::strong_ordering>;
    };

/// Helper CRTP base class to define Ordered Set traits classes, which are
/// optionally Bounded and/or Metric (cf
/// Is_bounded_set_traits/Is_metric_set_traits).
///
/// The subclass is required to define the static member function `lt`, and this
/// class provides the static member functions `gt`, `le`, `ge`, and the member
/// type Less_t, defined in terms of `lt`.
///
/// If the user defines `min` and `max_exclusive`, this provides `in_range`,
/// making the class satisfy Is_bounded_set_traits.
///
/// If the template parameter Difference_tp is given, this class also provides
/// `add` and `sub`, defined in terms of + and -, making the class satisfy
/// Is_metric_set_traits.
///
/// @tparam Self_tp The subclass that defines `lt`.
///
/// @tparam Element_tp The element type.
///
/// @tparam Difference_tp The difference type. This may be omitted, in which
/// case this becomes a non-metric Set traits class.
template <class Self_tp, class Element_tp, class Difference_tp = void>
struct Ordered_set_traits_interface : public Base_set_traits {
 private:
  using Self_t = Self_tp;
  using This_t =
      Ordered_set_traits_interface<Self_t, Element_tp, Difference_tp>;
  // We allow Difference_t to be void, and then functions operating on
  // Difference_t are disabled. However, that doesn't stop the compiler from
  // emitting an error when the parameter type becomes reference to void. So we
  // use this type instead, which is Difference_t whenever that is defined and
  // (the arbitrarily chosen type) int when Difference_t is void.
  using Difference_proxy_t =
      std::conditional_t<std::same_as<Difference_tp, void>, int, Difference_tp>;

 public:
  using Element_t = Element_tp;
  using Difference_t = Difference_tp;
  using Less_t = detail::Less<Self_tp, Element_tp>;

  /// @return true if min <= element < max_exclusive.
  [[nodiscard]] static constexpr bool in_range(const Element_t &element) {
    return ge(element, Self_t::min()) &&
           Self_t::lt(element, Self_t::max_exclusive());
  }

  [[nodiscard]] static constexpr std::strong_ordering cmp(
      const Element_t &left, const Element_t &right) {
    if constexpr (requires { Self_t::cmp_impl(left, right); }) {
      return Self_t::cmp_impl(left, right);
    } else {
      if (Self_t::lt(left, right)) return std::strong_ordering::less;
      if (left == right) return std::strong_ordering::equal;
      return std::strong_ordering::greater;
    }
  }

  [[nodiscard]] static constexpr bool lt(const Element_t &left,
                                         const Element_t &right) {
    if constexpr (requires { Self_t::lt_impl(left, right); }) {
      return Self_t::lt_impl(left, right);
    } else {
      return Self_t::cmp_impl(left, right) < 0;
    }
  }

  /// @return true if left <= right
  [[nodiscard]] static constexpr bool le(const Element_t &left,
                                         const Element_t &right) {
    // parameter order intentionally swapped
    // NOLINTNEXTLINE(readability-suspicious-call-argument)
    return !This_t::lt(right, left);
  }

  /// @return true if left > right
  [[nodiscard]] static constexpr bool gt(const Element_t &left,
                                         const Element_t &right) {
    // parameter order intentionally swapped
    // NOLINTNEXTLINE(readability-suspicious-call-argument)
    return This_t::lt(right, left);
  }

  /// @return true if left >= right
  [[nodiscard]] static constexpr bool ge(const Element_t &left,
                                         const Element_t &right) {
    return !This_t::lt(left, right);
  }

  /// @return The difference between two boundaries.
  [[nodiscard]] static constexpr Difference_t sub(const Element_t &left,
                                                  const Element_t &right)
    requires(!std::same_as<Difference_t, void>)
  {
    return Difference_t(left - right);
  }

  /// @return The sum of two boundary differences.
  ///
  /// We use nodiscard instead of nodiscard because nodiscard works when the
  /// return type is void.
  [[nodiscard]] static constexpr Difference_t add(
      const Difference_proxy_t &left, const Difference_proxy_t &right)
    requires(!std::same_as<Difference_t, void>)
  {
    return left + right;
  }

  /// @return The sum of a boundary and a boundary difference.
  [[nodiscard]] static constexpr Element_t add(const Element_t &left,
                                               const Difference_proxy_t &right)
    requires(!std::same_as<Difference_t, void> &&
             !std::same_as<Element_t, Difference_t>)
  {
    return Element_t(left + right);
  }

  /// @return The sum of a boundary difference and a boundary.
  [[nodiscard]] static constexpr Element_t add(const Difference_proxy_t &left,
                                               const Element_t &right)
    requires(!std::same_as<Difference_t, void> &&
             !std::same_as<Element_t, Difference_t> &&
             // dummy 'true' constraint, just to make the prototypes different
             // in case Element_t and Difference_t are the same types.
             // NOLINTNEXTLINE(readability-simplify-boolean-expr)
             true)
  {
    return Element_t(left + right);
  }
};  // struct Ordered_set_traits_interface

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_ORDERED_SET_TRAITS_INTERFACE_H
