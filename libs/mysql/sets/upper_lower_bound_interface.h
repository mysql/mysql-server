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

#ifndef MYSQL_SETS_UPPER_LOWER_BOUND_INTERFACE_H
#define MYSQL_SETS_UPPER_LOWER_BOUND_INTERFACE_H

/// @file
/// Experimental API header

#include <concepts>                 // derived_from
#include <iterator>                 // forward_iterator
#include "mysql/ranges/meta.h"      // Is_iterator_for_range
#include "mysql/sets/set_traits.h"  // Is_ordered_set_traits

/// @addtogroup GroupLibsMysqlSets
/// @{

namespace mysql::sets::detail {

/// True if Test::lower_bound_impl(test, hint, element) is defined.
template <class Test, class Iterator_t, class Element_t>
concept Has_lower_bound_impl_with_hint =
    requires(Test test, Iterator_t hint, Element_t element) {
      {
        Test::lower_bound_impl(test, hint, element)
        } -> std::same_as<Iterator_t>;
    };

/// True if Test::lower_bound_impl(test, element) is defined.
template <class Test, class Iterator_t, class Element_t>
concept Has_lower_bound_impl_without_hint =  // this comment helps clang-format
    requires(Test test, Element_t element) {
      { Test::lower_bound_impl(test, element) } -> std::same_as<Iterator_t>;
    };

/// True if one of the lower bound functions is defined.
template <class Test, class Iterator_t, class Element_t>
concept Has_lower_bound_impl =
    Has_lower_bound_impl_with_hint<Test, Iterator_t, Element_t> ||
    Has_lower_bound_impl_without_hint<Test, Iterator_t, Element_t>;

/// True if Test::upper_bound_impl(test, hint, element) is defined.
template <class Test, class Iterator_t, class Element_t>
concept Has_upper_bound_impl_with_hint =
    requires(Test test, Iterator_t hint, Element_t element) {
      {
        Test::upper_bound_impl(test, hint, element)
        } -> std::same_as<Iterator_t>;
    };

/// True if Test::upper_bound_impl(test, element) is defined.
template <class Test, class Iterator_t, class Element_t>
concept Has_upper_bound_impl_without_hint =  // this comment helps clang-format
    requires(Test test, Element_t element) {
      { Test::upper_bound_impl(test, element) } -> std::same_as<Iterator_t>;
    };

/// True if one of the upper bound functions is defined.
template <class Test, class Iterator_t, class Element_t>
concept Has_upper_bound_impl =
    Has_upper_bound_impl_with_hint<Test, Iterator_t, Element_t> ||
    Has_upper_bound_impl_without_hint<Test, Iterator_t, Element_t>;

}  // namespace mysql::sets::detail

namespace mysql::sets {

/// True if Test is a *getter* for Iterator_t, i.e., Test::get(Iterator_t) is
/// defined.
///
/// An iterator getter returns a a value from an iterator. This is useful in
/// generic code that may need to either extract the value from an iterator over
/// a sequence container such as std::vector, or extract just the key or just
/// the mapped value from an iterator over an associative container such as
/// std::map.
///
/// @see Upper_lower_bound_interface
template <class Test, class Iterator_t>
concept Is_getter_for_iterator =
    requires(Iterator_t iterator) { Test::get(iterator); };

/// Iterator getter that returns `*iterator`.
struct Iterator_get_value {
  [[nodiscard]] static decltype(auto) get(
      const std::input_iterator auto &iterator) {
    return *iterator;
  }
};

/// Iterator getter that returns `iterator->first`.
struct Iterator_get_first {
  [[nodiscard]] static const auto &get(
      const std::input_iterator auto &iterator) {
    return iterator->first;
  }
};

/// True if Test satisfies the requirements for being a subclass of
/// Upper_lower_bound_interface.
///
/// It must define the following member functions. In each prototype, the hint
/// parameter may be omitted.
///
/// @code
///   // Iterator to the first element.
///   Iterator_t begin() const;
///
///   // Iterator to sentinel.
///   Iterator_t end() const;
///
///   // Return an iterator to the smallest element greater than or equal to
///   // `element` in `self`, with the hint that the correct answer is greater
///   // than or equal to `*hint`.
///   static constexpr Iterator_t
///   lower_bound_impl(Test &self, const Iterator_t &hint,
///                    const Element_t &element);
///
///   // See above. This is the const version. (Can usually have a common
///   // implementation using `auto` for the first parameter.)
///   static constexpr Const_iterator_t
///   lower_bound_impl(const Test &self, const Const_iterator_t &hint,
///                    const Element_t &element);
///
///   // Return an iterator to the smallest element strictly greater than
///   // `element` in `self`, with the hint that the correct answer is greater
///   // than or equal to `*hint`.
///   static constexpr Iterator_t
///   upper_bound_impl(Test &self, const Iterator_t &hint,
///                    const Element_t &element);
///
///   // See above. This is the const version. (Can usually have a common
///   // implementation using `auto` for the first parameter.)
///   static constexpr Const_iterator_t
///   upper_bound_impl(const Test &self, const Const_iterator_t &hint,
///                    const Element_t &element);
/// @endcode
template <class Test, class Iterator_t, class Const_iterator_t, class Element_t>
concept Is_upper_lower_bound_implementation =
    std::forward_iterator<Iterator_t> &&
    std::forward_iterator<Const_iterator_t> &&
    requires(Test implementation, const Test const_implementation,
             Iterator_t iterator, Const_iterator_t const_iterator,
             Element_t element) {
      { implementation.begin() } -> std::same_as<Iterator_t>;
      { implementation.end() } -> std::same_as<Iterator_t>;
      { const_implementation.begin() } -> std::same_as<Const_iterator_t>;
      { const_implementation.end() } -> std::same_as<Const_iterator_t>;
    } &&
    detail::Has_lower_bound_impl<Test, Iterator_t, Element_t> &&
    detail::Has_lower_bound_impl<const Test, Const_iterator_t, Element_t> &&
    detail::Has_upper_bound_impl<Test, Iterator_t, Element_t> &&
    detail::Has_upper_bound_impl<const Test, Const_iterator_t, Element_t>;

/// CRTP base class (mixin) to define a set that has upper_bound and lower_bound
/// members.
///
/// Typically, the implementation defines two member functions: the static
/// member functions upper_bound_impl and lower_bound_impl taking a hint, and
/// this class provides all the other 12 functions.
///
/// The implementation needs to satisfy Is_upper_lower_bound_implementation.
/// This class will provide const/non-const upper_bound/lower_bound member
/// functions that take/don't take iterator hints.
///
/// It also provides static member functions
/// upper_bound_dispatch/lower_bound_dispatch, which take either a const or a
/// non-const instance in the first argument, and which take/don't take iterator
/// hints.
template <class Self_tp, Is_ordered_set_traits Set_traits_tp,
          std::forward_iterator Iterator_tp,
          std::forward_iterator Const_iterator_tp,
          Is_getter_for_iterator<Iterator_tp> Iterator_getter_tp>
class Upper_lower_bound_interface {
  using Self_t = Self_tp;

 public:
  using Iterator_t = Iterator_tp;
  using Const_iterator_t = Const_iterator_tp;
  using Set_traits_t = Set_traits_tp;
  using Iterator_getter_t = Iterator_getter_tp;

  using Element_t = typename Set_traits_t::Element_t;

  /// Return the lower bound for @c element, using an iterator @c hint known to
  /// be less than or equal to the correct result. This is the non-const version
  /// of the function.
  ///
  /// The hint may allow for optimizations. If no hint is known, use the
  /// hint-less function instead.
  ///
  /// @param hint Iterator hint. This must be less than or equal to the correct
  /// result.
  ///
  /// @param element Value to query.
  ///
  /// @return Iterator to the first lower bound for @c element, i.e., the first
  /// element that is greater than or equal to @c element, or to the end if no
  /// such element exists.
  [[nodiscard]] constexpr Iterator_t lower_bound(const Iterator_t &hint,
                                                 const Element_t &element) {
    return lower_bound_dispatch(self(), hint, element);
  }

  /// Return the lower bound for @c element, using an iterator @c hint known to
  /// be less than or equal to the correct result. This is the const version of
  /// the function.
  ///
  /// The hint may allow for optimizations. If no hint is known, use the
  /// hint-less function instead.
  ///
  /// @param hint Iterator hint. This must be less than or equal to the correct
  /// result.
  ///
  /// @param element Value to query.
  ///
  /// @return Iterator to the first lower bound for @c element, i.e., the first
  /// element that is greater than or equal to @c element, or to the end if no
  /// such element exists.
  [[nodiscard]] constexpr Const_iterator_t lower_bound(
      const Const_iterator_t &hint, const Element_t &element) const {
    return lower_bound_dispatch(self(), hint, element);
  }

  /// Return the lower bound for @c element. This is the non-const version of
  /// the function.
  ///
  /// @param element Value to query.
  ///
  /// @return Iterator to the first lower bound for @c element, i.e., the first
  /// element that is greater than or equal to @c element, or to the end if no
  /// such element exists.
  [[nodiscard]] constexpr Iterator_t lower_bound(const Element_t &element) {
    return lower_bound_dispatch(self(), element);
  }

  /// Return the lower bound for @c element. This is the const version of the
  /// function.
  ///
  /// @param element Value to query.
  ///
  /// @return Iterator to the first lower bound for @c element, i.e., the first
  /// element that is greater than or equal to @c element, or to the end if no
  /// such element exists.
  [[nodiscard]] constexpr Const_iterator_t lower_bound(
      const Element_t &element) const {
    return lower_bound_dispatch(self(), element);
  }

  /// Return the upper bound for @c element, using an iterator @c hint known to
  /// be less than or equal to the correct result. This is the non-const version
  /// of the function.
  ///
  /// The hint may allow for optimizations. If no hint is known, use the
  /// hint-less function instead.
  ///
  /// @param hint Iterator hint. This must be less than or equal to the correct
  /// result.
  ///
  /// @param element Value to query.
  ///
  /// @return Iterator to the first upper bound for @c element, i.e., the first
  /// element that is strictly greater than @c element, or to the end if no
  /// such element exists.
  [[nodiscard]] constexpr Iterator_t upper_bound(const Iterator_t &hint,
                                                 const Element_t &element) {
    return upper_bound_dispatch(self(), hint, element);
  }

  /// Return the upper bound for @c element, using an iterator @c hint known to
  /// be less than or equal to the correct result. This is the const version of
  /// the function.
  ///
  /// The hint may allow for optimizations. If no hint is known, use the
  /// hint-less function instead.
  ///
  /// @param hint Iterator hint. This must be less than or equal to the correct
  /// result.
  ///
  /// @param element Value to query.
  ///
  /// @return Iterator to the first upper bound for @c element, i.e., the first
  /// element that is strictly greater than @c element, or to the end if no
  /// such element exists.
  [[nodiscard]] constexpr Const_iterator_t upper_bound(
      const Const_iterator_t &hint, const Element_t &element) const {
    return upper_bound_dispatch(self(), hint, element);
  }

  /// Return the upper bound for @c element. This is the non-const version of
  /// the function.
  ///
  /// @param element Value to query.
  ///
  /// @return Iterator to the first upper bound for @c element, i.e., the first
  /// element that is strictly greater than @c element, or to the end if no
  /// such element exists.
  [[nodiscard]] constexpr Iterator_t upper_bound(const Element_t &element) {
    return upper_bound_dispatch(self(), element);
  }

  /// Return the upper bound for @c element. This is the const version of
  /// the function.
  ///
  /// @param element Value to query.
  ///
  /// @return Iterator to the first upper bound for @c element, i.e., the first
  /// element that is strictly greater than @c element, or to the end if no
  /// such element exists.
  [[nodiscard]] constexpr Const_iterator_t upper_bound(
      const Element_t &element) const {
    return upper_bound_dispatch(self(), element);
  }

  /// Implements the lower_bound functions with hint defined above, checking if
  /// the hint is already the correct answer, and otherwise delegating to the
  /// implementing class.
  template <std::derived_from<Self_t> Self_arg_t,
            mysql::ranges::Is_iterator_for_range<Self_arg_t> Iter_t>
  [[nodiscard]] static constexpr Iter_t lower_bound_dispatch(
      Self_arg_t &self_arg, const Iter_t &hint, const Element_t &element) {
    if (hint == self_arg.end() ||
        Set_traits_t::ge(Iterator_getter_t::get(hint), element))
      return hint;
    if constexpr (detail::Has_lower_bound_impl_with_hint<Self_arg_t, Iter_t,
                                                         Element_t>) {
      return Self_arg_t::lower_bound_impl(self_arg, hint, element);
    } else {
      return Self_arg_t::lower_bound_impl(self_arg, element);
    }
  }

  /// Implements the lower_bound functions without hint defined above.
  template <std::derived_from<Self_t> Self_arg_t>
  [[nodiscard]] static constexpr auto lower_bound_dispatch(
      Self_arg_t &self_arg, const Element_t &element) {
    if constexpr (detail::Has_lower_bound_impl_without_hint<
                      Self_arg_t,
                      mysql::ranges::Range_iterator_type<Self_arg_t>,
                      Element_t>) {
      return Self_arg_t::lower_bound_impl(self_arg, element);
    } else {
      return Self_arg_t::lower_bound_impl(self_arg, self_arg.begin(), element);
    }
  }

  /// Implements the upper_bound functions with hint defined above, checking if
  /// the hint is already the correct answer, and otherwise delegating to the
  /// implementing class.
  template <std::derived_from<Self_t> Self_arg_t,
            mysql::ranges::Is_iterator_for_range<Self_arg_t> Iter_t>
  [[nodiscard]] static constexpr Iter_t upper_bound_dispatch(
      Self_arg_t &self_arg, const Iter_t &hint, const Element_t &element) {
    if (hint == self_arg.end() ||
        Set_traits_t::gt(Iterator_getter_t::get(hint), element))
      return hint;
    if constexpr (detail::Has_upper_bound_impl_with_hint<Self_arg_t, Iter_t,
                                                         Element_t>) {
      return Self_arg_t::upper_bound_impl(self_arg, hint, element);
    } else {
      return Self_arg_t::upper_bound_impl(self_arg, element);
    }
  }

  /// Implements the upper_bound functions without hint defined above.
  template <std::derived_from<Self_t> Self_arg_t>
  [[nodiscard]] static constexpr auto upper_bound_dispatch(
      Self_arg_t &self_arg, const Element_t &element) {
    if constexpr (detail::Has_upper_bound_impl_without_hint<
                      Self_arg_t,
                      mysql::ranges::Range_iterator_type<Self_arg_t>,
                      Element_t>) {
      return Self_arg_t::upper_bound_impl(self_arg, element);
    } else {
      return Self_arg_t::upper_bound_impl(self_arg, self_arg.begin(), element);
    }
  }

 private:
  /// CRTP helper to return a const reference to the subclass on which the
  /// function is invoked.
  [[nodiscard]] constexpr const Self_t &self() const {
    static_assert(
        Is_upper_lower_bound_implementation<Self_t, Iterator_t,
                                            Const_iterator_t, Element_t>);
    return static_cast<const Self_t &>(*this);
  }

  /// CRTP helper to return a non-const reference to the subclass on which the
  /// function is invoked.
  [[nodiscard]] constexpr Self_t &self() {
    return static_cast<Self_t &>(*this);
  }
};  // class Upper_lower_bound_interface

}  // namespace mysql::sets

// addtogroup GroupLibsMysqlSets
/// @}

#endif  // ifndef MYSQL_SETS_UPPER_LOWER_BOUND_INTERFACE_H
