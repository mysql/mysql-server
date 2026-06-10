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

#ifndef MYSQL_ITERATORS_ITERATOR_INTERFACE_H
#define MYSQL_ITERATORS_ITERATOR_INTERFACE_H

/// @file
/// Experimental API header

#include <cstddef>                   // ptrdiff_t
#include <iterator>                  // input_iterator_tag
#include <type_traits>               // remove_cvref
#include "mysql/meta/is_pointer.h"   // Is_pointer
#include "mysql/meta/not_decayed.h"  // Not_decayed

/// @addtogroup GroupLibsMysqlIterators
/// @{

namespace mysql::iterators::detail {

// ==== Concepts to detect specific member functions ====
//
// These concepts are used to deduce the iterator category and iterator
// concepts.

/// Helper: returns true if Type has a get function, regardless if it returns
/// void or not.
template <class Type>
concept Has_member_get_maybe_void = requires(Type t) { t.get(); };

/// Helper: returns true if Type has a get function that returns void (which
/// does not make it an iterator).
template <class Type>
concept Has_member_get_void = requires(Type t) {
                                { t.get() } -> std::same_as<void>;
                              };

/// true if `Type` has a `get` member that does not return void.
template <class Type>
concept Has_member_get = Has_member_get_maybe_void<Type> &&
                         (!Has_member_get_void<Type>);

/// true if `Type` has a `get` member that returns a pointer.
template <class Type>
concept Has_member_get_pointer =  // this comment helps clang-format
    requires(Type t) {
      { t.get_pointer() } -> mysql::meta::Is_pointer;
    };

/// true if `Type` has a `get` member that returns a reference type.
template <class Type>
concept Has_member_get_reference =
    std::is_reference_v<decltype(std::declval<Type>().get())>;

/// true if `Type` has a `next` member.
template <class Type>
concept Has_member_next = requires(Type t) { t.next(); };

/// true if `Type` has an `is_equal` member.
template <class Type>
concept Has_member_is_equal = requires(Type t, const Type &other) {
                                { t.is_equal(other) } -> std::same_as<bool>;
                              };

/// true if `Type` has an `is_sentinel` member.
template <class Type>
concept Has_member_is_sentinel = requires(Type t) {
                                   { t.is_sentinel() } -> std::same_as<bool>;
                                 };

/// true if `Type` has a `prev` member
template <class Type>
concept Has_member_prev = requires(Type t) { t.prev(); };

/// true if `Type` has an `advance` member
template <class Type>
concept Has_member_advance =
    requires(Type t, std::ptrdiff_t x) { t.advance(x); };

/// true if `Type` has a `distance_from` member
template <class Type>
concept Has_member_distance_from =  // this comment helps clang-format
    requires(Type t, const Type &other) {
      { t.distance_from(other) } -> std::same_as<std::ptrdiff_t>;
    };

/// true if `Type` has a `distance_from` member
template <class Type>
concept Has_member_distance_from_sentinel =  // this comment helps clang-format
    requires(Type t) {
      { t.distance_from_sentinel() } -> std::same_as<std::ptrdiff_t>;
    };

/// true if `Type` has an `is_equal` or `distance_from` member.
template <class Type>
concept Has_equality_member =
    Has_member_is_equal<Type> || Has_member_distance_from<Type>;

}  // namespace mysql::iterators::detail

// ==== Concepts to check member functions defining iterator categories ====

namespace mysql::iterators {

/// True if `Type` has the members required for `Iterator_interface<Type>` to
/// satisfy `std::input_iterator`. Note that this is weaker than
/// LegacyInputIterator; see Is_legacy_input_iterator_impl.
template <class Type>
concept Is_input_iterator_impl =  // this comment helps clang-format
    (detail::Has_member_get<Type> || detail::Has_member_get_pointer<Type>) &&
    (detail::Has_member_advance<Type> || detail::Has_member_next<Type>);

/// True if `Type` has the members required for `Iterator_interface<Type>` to
/// meet the syntactic requirements for LegacyInputIterator.
template <class Type>
concept Is_legacy_input_iterator_impl =
    Is_input_iterator_impl<Type> && detail::Has_equality_member<Type>;

/// True if `Type` has the members and the value type for required for
/// `Iterator_interface<Type>` to meet the syntactic requirements for
/// LegacyForwardIterator.
template <class Type>
concept Is_legacy_forward_iterator_impl =
    Is_legacy_input_iterator_impl<Type> &&
    (detail::Has_member_get_reference<Type> ||
     detail::Has_member_get_pointer<Type>) &&
    std::copyable<Type>;

/// true if `Type` has the members required for `Iterator_interface<Type>` to
/// meet the syntactic requirements for `LegacyBidirectionalIterator`.
template <class Type>
concept Is_legacy_bidirectional_iterator_impl =
    Is_legacy_forward_iterator_impl<Type> &&
    (detail::Has_member_advance<Type> || detail::Has_member_prev<Type>);

/// true if `Type` has the members required for `Iterator_interface<Type>` to
/// meet the syntactic requirements for `LegacyRandomAccessIterator`.
template <class Type>
concept Is_legacy_random_access_iterator_impl =
    Is_legacy_bidirectional_iterator_impl<Type> &&
    detail::Has_member_advance<Type> && detail::Has_member_distance_from<Type>;

/// true if `Type` has the members required for `Iterator_interface<Type>` to
/// meet the syntactic requirements for `LegacyContiguousIterator`.
template <class Type>
concept Is_legacy_contiguous_iterator_impl =
    Is_legacy_random_access_iterator_impl<Type> &&
    detail::Has_member_get_pointer<Type>;

}  // namespace mysql::iterators

// ==== Arrow proxy ====

namespace mysql::iterators::detail {

/// Auxiliary object that holds a value internally, and operator-> returns a
/// pointer to the value. This is also called an "arrow proxy".
///
/// This is useful to implement the `operator->` for iterator classes for which
/// `operator*` returns by value, rather than by reference.
template <class Value_tp>
class Dereferenceable_wrapper {
 public:
  using Value_t = Value_tp;

  template <class... Args_t>
    requires mysql::meta::Not_decayed<Dereferenceable_wrapper<Value_t>,
                                      Args_t...>
  explicit Dereferenceable_wrapper(Args_t &&...args)
      : m_value(std::forward<Args_t>(args)...) {}

  Value_t *operator->() { return &m_value; }

 private:
  Value_t m_value;
};  // class Dereferenceable_wrapper

}  // namespace mysql::iterators::detail

// ==== Default_sentinel ====

namespace mysql::iterators {

/// Used like std::default_sentinel_t / std::default_sentinel.
///
/// Currently we support compiler versions that have not yet implemented
/// std::default_sentinel_t / std::default_sentinel, so we define our own
/// sentinel type.
///
/// Once we drop support for such old compilers, please remove this and use
/// std::default_sentinel_t / std::default_sentinel instead.
class Default_sentinel {
 public:
  auto operator<=>(const Default_sentinel &) const = default;
  bool operator==(const Default_sentinel &) const = default;
};
inline constexpr Default_sentinel default_sentinel;

// ==== Iterator_interface ====

/// CRTP base class (mixin) that makes your class a standard-compliant iterator,
/// given only a minimal set of functions to read, move and compare iterators.
///
/// Based on the member functions you define, this class deduces all the
/// operators, as well as the iterator traits. This ensures that your class will
/// meet the appropriate named requirement, `LegacyInputIterator`,
/// `LegacyForwardIterator`, `LegacyBidirectionalIterator`,
/// `LegacyRandomAccessIterator`, or `LegacyContiguousIterator`, as well as
/// satisfy the appropriate concept, `std::input_iterator`,
/// `std::forward_iterator`, `std::bidirectional_iterator`,
/// `std::random_access_iterator`, or `std::contiguous_iterator`.
///
/// Defining member functions
/// =========================
///
/// To make `It` an iterator that iterates over values of type `V`, inherit
/// like:
///
/// @code
///   class It : public Iterator_interface<It> {
///    public:
///     /* define member functions as described below */
///   };
/// @endcode
///
/// `It` must be default-constructible, copy/move-constructible, and
/// copy/move-assignable. In addition, define a subset of the following
/// member functions:
///
/// @code
/// // Exactly one of the following, to read the current value:
/// V get() const;
/// V &get() const;
/// V *get_pointer() const;
///
/// // At least one of next and advance to move the position; prev is optional.
/// void next();
/// void prev();
/// void advance(std::ptrdiff_t);
///
/// // Optionally one of the following, to compare iterators:
/// bool is_equal(const It &) const;
/// std::ptrdiff_t distance_from(const It &) const;
/// bool is_sentinel() const;
/// std::ptrdiff_t distance_from_sentinel() const;
/// @endcode
///
/// C++ defines two hierarchies of iterators, which are confusingly similar and
/// subtly different:
///
/// - Pre-C++20 named requirements such as `LegacyInputIterator`, which can be
///   queried at compile-time using
///   `std::iterator_traits<It>::iterator_category`.
///
/// - C++20 concepts such as `std::input_iterator`, which can be queried at
///   compile-time using these concepts.
///
/// This class deduces both the iterator category and the iterator concept,
/// as follows:
///
/// - `LegacyInputIterator` requries any `get` or `get_pointer` function, either
///   `next` or `advance`, and either `is_equal` or `distance_from`.
///
/// - `LegacyForwardIterator` requries `LegacyInputIterator`, and that `get`
///   returns by reference, and the class must be copyable.
///
/// - `LegacyBidirectionalIterator` requires `LegacyForwardIterator`, and either
///   `prev` or `advance`.
///
/// - `LegacyRandomAccessIterator` requries `LegacyBidirectionalIterator`, and
///   `advance`, and `distance_from`.
///
/// - `LegacyContiguousIterator` requries `LegacyRandomAccessIterator`, and
///   requires `get_pointer`.
///
/// - `std::input_iterator` requries any `get` or `get_pointer` function, and
///   either `next` or `advance`. (It is weaker than LegacyInputIterator because
///   it does not require that iterators can be compared.)
///
/// - `std::forward_iterator` requries `std::input_iterator`, and either
///   `is_equal` or `distance_from`, and the class must be copyable. (It is
///   weaker than LegacyForwardIterator because it does not require that `get`
///   returns by reference.)
///
/// - `std::bidirectional_iterator` requries `std::forward_iterator`, and either
///   `prev` or `advance`. (It is weaker than LegacyBidirectionalIterator
///   because it does not require that `get` returns by reference.)
///
/// - `std::random_access_iterator` requires `std::bidirectional_iterator`, and
///   `advance` and `distance_from`. (It is weaker than
///   LegacyRandomAccessIterator because it does not require that `get` returns
///   by reference.)
///
/// - `std::contiguous_iterator` requires `std::random_access_iterator`, and
///   requires `get_pointer`. This coincides with the requirements for
///  `LegacyContiguousIterator`.
///
/// * Diagram of concept/category strengths
///
/// To summarize the previous section, the following diagram illustrates the
/// relative strengths between deduced iterator concepts and categories for
/// types derived from this class. The notation A-->B indicates that B is
/// stronger that A, i.e., requires everything that A requires and more. The
/// abbreviations I/F/B/R/C/ct/cy mean
/// input/forward/bidirectional/random_access/contiguous/concept/category,
/// respectively.
///
///```
///            1          2          4          5
///     I_ct ----> I_cy ----> F_ct ----> B_ct ----> R_ct
///                            |          |          |
///                           3|         3|         3|
///                            V     4    V     5    V     6
///                           F_cy ----> B_cy ----> R_cy ----> C_ct==C_cy
///```
///
/// Each arrow is annotated by a number that refers to the following list,
/// indicating what you need to implement to "follow the arrow":
/// (1) Implement `is_equal` to make iterators equality-comparable.
/// (2) Implement the copy constructor (actually, just don't delete it).
/// (3) Make `get` return by reference.
/// (4) Implement `prev` to enable moving backwards.
/// (5) Implement `advance` and `distance_from` instead of
///     `get`/`next`/`is_equal`, to make it possible to take long steps.
/// (6) Implement `get_pointer` and ensure that returned objects are adjacent
///     in memory.
///
/// * Iterators returning values, not references
///
/// Iterators that return value rather than reference can't meet the
/// `LegacyForwardIterator` requirement. This determines the behavior of
/// standard algorithms like `std::prev`, `std::advance`, and `std::distance`.
/// Thus, just because the iterator returns by value, both `std::advance(it,
/// negative_number)` and `std::prev` produce undefined behavior (typically an
/// infinite loop), and `std::distance` is linear-time (even if the iterator
/// satisfies `std::random_access_iterator`). Use `std::ranges::prev`,
/// `std::ranges::advance`, and `std::ranges::distance` instead.
///
/// * Sentinel types
///
/// If your iterator needs a sentinel type, this class limits it to be the
/// mysql::iterator::Default_sentinel type. Define one or both of the member
/// functions `is_sentinel` or `distance_from_sentinel`.
///
/// @note Thanks for inspiration from
/// https://vector-of-bool.github.io/2020/06/13/cpp20-iter-facade.html
///
/// @tparam Self_tp The subclass that inherits from this class.
template <class Self_tp>
class Iterator_interface {
 protected:
  using Self_t = Self_tp;

 public:
  // ==== input_iterator members ====

  /// Dereference operator, which returns the current value.
  ///
  /// This delegates the work to `get`.
  [[nodiscard]] decltype(auto) operator*() const {
    if constexpr (detail::Has_member_get<Self_t>) {
      return self().get();
    } else {
      return *self().get_pointer();
    }
  }

  /// Arrow operator, return a pointer (possibly a fancy pointer) to the current
  /// element.
  ///
  /// This delegates work to `get_pointer` if that is defined. Otherwise, if
  /// `get` returns a reference, returns the address of `get()`. Otherwise,
  /// returns an "arrow proxy": an object that stores a copy of the value and
  /// for which the arrow operator returns the address of the stored value. Note
  /// that the pointer returned from the arrow proxy only lives as long as the
  /// arrow proxy lives.
  [[nodiscard]] auto operator->() const {
    if constexpr (detail::Has_member_get_pointer<Self_t>) {
      return self().get_pointer();
    } else {
      decltype(auto) ret = **this;
      if constexpr (std::is_reference_v<decltype(ret)>) {
        return std::addressof(ret);
      } else {
        return detail::Dereferenceable_wrapper<decltype(**this)>(
            std::move(ret));
      }
    }
  }

  /// Pre-increment operator, which advances the position one step and returns a
  /// reference to the iterator itself.
  ///
  /// This delegates the work to the `next` member function if there is one;
  /// otherwise to the `advance` member function.
  Self_t &operator++() {
    if constexpr (detail::Has_member_next<Self_t>) {
      self().next();
    } else {
      self().advance(1);
    }
    return self();
  }

  /// Post-increment operator, which advances the position one step. For forward
  /// iterators and higher, returns a copy of the iterator before the increment.
  /// For input iterators, returns a reference to the iterator itself.
  ///
  /// This delegates the work to the `next` member function if there is one;
  /// otherwise to the `advance` member function.
  auto operator++(int) {
    if constexpr (std::copyable<Self_t>) {
      auto ret = self();
      ++*this;
      return ret;
    } else {
      // For non-copyable iterators (which are necessarily single-pass, hence
      // at most input iterators), just return void.
      ++*this;
    }
  }

  // ===== bidirectional_iterator members ====

  /// Pre-decrement iterator, which moves one step back and returns a reference
  /// to the iterator itself.
  ///
  /// This delegates the work to the `prev` member function if there is one;
  /// otherwise to the `advance` member function.
  Self_t &operator--()
    requires(detail::Has_member_prev<Self_t> ||
             detail::Has_member_advance<Self_t>)
  {
    if constexpr (detail::Has_member_prev<Self_t>) {
      self().prev();
    } else {
      self().advance(-1);
    }
    return self();
  }

  /// Post-decrement operator, which moves one step back and returns a copy of
  /// the iterator before the decrement.
  ///
  /// This delegates the work to the `prev` member function if there is one;
  /// otherwise to the `advance` member function.
  auto operator--(int)
    requires(detail::Has_member_prev<Self_t> ||
             detail::Has_member_advance<Self_t>)
  {
    if constexpr (std::copyable<Self_t>) {
      auto ret = self();
      --*this;
      return ret;
    } else {
      // For non-copyable iterators (which are necessarily single-pass, hence
      // at most input iterators), just return void.
      --*this;
    }
  }

  // ===== random_access_iterator members ====

  /// Addition assignment operator, which moves the iterator forward by the
  /// given number of steps, and returns a reference to the iterator itself.
  ///
  /// This delegates the work to `advance`.
  Self_t &operator+=(std::ptrdiff_t delta)
    requires detail::Has_member_advance<Self_t>
  {
    self().advance(delta);
    return self();
  }

  /// Subtraction assignment operator, which moves the iterator backward by the
  /// given number of steps, and returns a reference to the iterator itself.
  ///
  /// This delegates the work to `advance`.
  Self_t &operator-=(std::ptrdiff_t delta)
    requires detail::Has_member_advance<Self_t>
  {
    return (*this += -delta);
  }

  /// Addition operator, which returns a new iterator that is the given number
  /// of steps ahead of the current iterator.
  ///
  /// This delegates the work to `advance`.
  [[nodiscard]] Self_t operator+(std::ptrdiff_t delta) const
    requires detail::Has_member_advance<Self_t>
  {
    Self_t ret(self());
    ret.advance(delta);
    return ret;
  }

  /// Subtraction-of-integer operator, which returns a new iterator that is the
  /// given number of steps behind of the current iterator.
  ///
  /// This delegates the work to `advance`.
  [[nodiscard]] Self_t operator-(std::ptrdiff_t delta) const
    requires detail::Has_member_advance<Self_t>
  {
    return (*this + -delta);
  }

  /// Subtraction-of-iterator operator, which returns the number of steps from
  /// other this.
  ///
  /// This delegates the work to `distance_from`.
  [[nodiscard]] std::ptrdiff_t operator-(const Self_t &other) const
    requires detail::Has_member_distance_from<Self_t>
  {
    return self().distance_from(other);
  }

  /// Subscript operator, which returns a new iterator that is the given number
  /// of steps ahead of the current iterator.
  ///
  /// This delegates the work to `advance`.
  [[nodiscard]] decltype(auto) operator[](std::ptrdiff_t delta) const
    requires detail::Has_member_advance<Self_t>
  {
    return *(*this + delta);
  }

 private:
  /// Return a const reference to the subclass.
  [[nodiscard]] const Self_t &self() const {
    return static_cast<const Self_t &>(*this);
  }

  /// Return a non-const reference to the subclass.
  [[nodiscard]] Self_t &self() { return static_cast<Self_t &>(*this); }
};  // class Iterator_interface

// ==== Out-of-class operator definitions ====

/// Equality operator, which returns true if the two iterators are equal.
///
/// This delegates the work to the member function `is_equal` if there is one;
/// otherwise to `distance_from`.
template <class Iterator_t>
  requires std::derived_from<Iterator_t, Iterator_interface<Iterator_t>> &&
           detail::Has_equality_member<Iterator_t>
[[nodiscard]] bool operator==(const Iterator_t &a, const Iterator_t &b) {
  if constexpr (detail::Has_member_is_equal<Iterator_t>) {
    return a.is_equal(b);
  } else {
    return a.distance_from(b) == 0;
  }
}

/// Equality operator, which returns true if the iterator is equal to the
/// sentinel.
///
/// This delegates the work to the member function `is_equal` if there is one;
/// otherwise to `distance_from`.
template <class Iterator_t>
  requires std::derived_from<Iterator_t, Iterator_interface<Iterator_t>> &&
           (
               requires { Iterator_t().distance_from_sentinel(); } ||
               requires { Iterator_t().is_sentinel(); })
[[nodiscard]] bool operator==(const Iterator_t &it, const Default_sentinel &) {
  if constexpr (detail::Has_member_is_sentinel<Iterator_t>) {
    return it.is_sentinel();
  } else {
    return it.distance_from_sentinel() == 0;
  }
}

/// Addition operator with the left-hand-side of type ptrdiff_t and the
/// right-hand-side of Iterator type, returning a new iterator that is ahead of
/// the given iterator by the given number of steps.
///
/// This delegates work to `advance`.
template <class Iterator_t>
  requires std::derived_from<Iterator_t, Iterator_interface<Iterator_t>> &&
           detail::Has_member_advance<Iterator_t>
[[nodiscard]] Iterator_t operator+(std::ptrdiff_t delta,
                                   const Iterator_t &iterator) {
  return iterator + delta;
}

/// Subtraction operator with the left-hand-side of Iterator type and the
/// right-hand-side of Sentinel type, returning the number of steps from the
/// sentinel to the iterator (which is non-positive).
///
/// This delegates work to `distance_from_sentinel`.
template <class Iterator_t>
  requires std::derived_from<Iterator_t, Iterator_interface<Iterator_t>> &&
           requires { Iterator_t().distance_from_sentinel(); }
[[nodiscard]] std::ptrdiff_t operator-(const Iterator_t &iterator,
                                       const Default_sentinel &) {
  return iterator.distance_from_sentinel();
}

/// Subtraction operator with the left-hand-side of Sentinel type and the
/// right-hand-side of Iterator type, returning the number of steps from the
/// iterator to the sentinel (which is non-negative).
///
/// This delegates work to `distance_from_sentinel()`.
template <class Iterator_t>
  requires std::derived_from<Iterator_t, Iterator_interface<Iterator_t>> &&
           requires { Iterator_t().distance_from_sentinel(); }
[[nodiscard]] std::ptrdiff_t operator-(const Default_sentinel &,
                                       const Iterator_t &iterator) {
  return -iterator.distance_from_sentinel();
}

/// Three-way comparison operator which compares two Iterator objects and
/// returns a std::strong_ordering object.
///
/// This delegates work to `distance_from`.
template <class Iterator_t>
  requires std::derived_from<Iterator_t, Iterator_interface<Iterator_t>> &&
           detail::Has_member_distance_from<Iterator_t>
[[nodiscard]] std::strong_ordering operator<=>(const Iterator_t &it1,
                                               const Iterator_t &it2) {
  return it1.distance_from(it2) <=> 0;
}

/// Three-way comparison operator which compares an Iterator object and a
/// Sentinel object and returns a std::strong_ordering object.
///
/// This delegates work to `distance_from_sentinel()`.
template <class Iterator_t>
  requires std::derived_from<Iterator_t, Iterator_interface<Iterator_t>> &&
           detail::Has_member_distance_from_sentinel<Iterator_t>
[[nodiscard]] std::strong_ordering operator<=>(const Iterator_t &it1,
                                               const Default_sentinel &) {
  return it1.distance_from_sentinel() <=> 0;
}

}  // namespace mysql::iterators

// ==== Specialization of iterator_traits ====

/// Helper type to declare that an iterator is not a legacy iterator.
struct Not_a_legacy_iterator {};

/// Specialization of the standard library customization point
/// std::iterator_traits, to subclasses of Iterator_interface.
///
/// This deduces the iterator category and defines other type members required
/// for iterators to satisfy the standard library requirements for iterators.
/// See also the "Specializations" section at
/// https://en.cppreference.com/w/cpp/iterator/iterator_traits
template <class Iterator_t>
  requires std::derived_from<
               Iterator_t, mysql::iterators::Iterator_interface<Iterator_t>> &&
           mysql::iterators::Is_legacy_input_iterator_impl<Iterator_t>
// The C++ standard explicitly allows specialization of `std::iterator_traits`,
// but clang-tidy complains about extending `std`. Suppress that check.
//
// NOLINTNEXTLINE(cert-dcl58-cpp)
struct std::iterator_traits<Iterator_t> {
  using reference = decltype(*std::declval<Iterator_t>());
  using pointer = decltype(std::declval<Iterator_t>().operator->());
  using value_type = std::remove_cvref_t<reference>;
  using difference_type = std::ptrdiff_t;

  // Classes deriving from Iterator_interface must provide at least members
  // that make it satisfy the `std::input_iterator` concept.
  static_assert(mysql::iterators::Is_input_iterator_impl<Iterator_t>);

  // Deduce the iterator category based on the existence of members. To define
  // the type using compile-time conditions, we take the decltype of the return
  // type of a lambda function, whose body consists of `if constexpr`
  // expressions that decide what type the lambda function returns.

  /// @cond DOXYGEN_DOES_NOT_UNDERSTAND_THIS
  using iterator_category = decltype([] {
    if constexpr (mysql::iterators::Is_legacy_contiguous_iterator_impl<
                      Iterator_t>)
      return std::contiguous_iterator_tag{};
    else if constexpr (mysql::iterators::Is_legacy_random_access_iterator_impl<
                           Iterator_t>)
      return std::random_access_iterator_tag{};
    else if constexpr (mysql::iterators::Is_legacy_bidirectional_iterator_impl<
                           Iterator_t>)
      return std::bidirectional_iterator_tag{};
    else if constexpr (mysql::iterators::Is_legacy_forward_iterator_impl<
                           Iterator_t>)
      return std::forward_iterator_tag{};
    else if constexpr (mysql::iterators::Is_legacy_input_iterator_impl<
                           Iterator_t>)
      return std::input_iterator_tag{};
    else
      return Not_a_legacy_iterator{};
  }());
  /// @endcond

  // Deduce the iterator concept.
  //
  // Note that "each concept is not satisfied if the required operations are not
  // supported, regardless of the tag", according to
  // https://en.cppreference.com/w/cpp/iterator/iterator_tags. In other words,
  // the actual iterator concepts satisfied may be weaker than the tag we
  // specify here.
  using iterator_concept = decltype([] {
    // For the weaker iterator concepts `std::input_iterator`,
    // `std::forward_iterator`, `std::bidirectional_iterator`, and
    // `std::random_access_iterator`, we may use either
    // `contiguous_iterator_tag` or `random_access_iterator_tag`. Concepts such
    // as `std::bidirectional_iterator` hold if *both* the tag derives from
    // bidirectional_iterator_tag *and* the class meets syntactic requirements;
    // thus it is ok for this declaration to use a tag that refers to a too
    // strong iterator conept.
    //
    // However, the syntactic requirements for `std::contiguous_iterator` may be
    // satisfied also for `random_access_iterators` (even if they do not model
    // all the semantic requirements of `contiguous_iterator`). Therefore we
    // distingiush them by the existence of `get_pointer()` in the
    // implementation.
    if constexpr (mysql::iterators::detail::Has_member_get_pointer<
                      Iterator_t>) {
      return std::contiguous_iterator_tag{};
    } else {
      return std::random_access_iterator_tag{};
    }
  }());
};  // struct std::iterator_traits<Iterator_t>

/// Specialization of the standard library customization point
/// std::pointer_traits, to subclasses of Iterator_interface which declare
/// themselves to be contiguous iterators.
template <class Iterator_t>
  requires std::derived_from<
               Iterator_t, mysql::iterators::Iterator_interface<Iterator_t>> &&
           mysql::iterators::Is_legacy_contiguous_iterator_impl<Iterator_t>
// The C++ standard explicitly allows specialization of `std::pointer_traits`,
// but clang-tidy complains about extending `std`. Suppress that check.
//
// NOLINTNEXTLINE(cert-dcl58-cpp)
struct std::pointer_traits<Iterator_t> {
  using pointer = decltype(std::declval<Iterator_t>().operator->());
  using element_type = decltype(std::declval<Iterator_t>().operator*());
  using difference_type = std::ptrdiff_t;
  template <class Other>
  using rebind = Other *;
  static pointer pointer_to(element_type &r) { return &r; }
};  // struct std::pointer_traits<Iterator_t>

/// Specialization of the standard library customization points
/// std::indirectly_readable_traits and std::incrementable_traits. This is
/// needed for iterators that satisfy the input iterator concept but not input
/// iterator category (as they don't have a specialization of iterator_traits).
//
// The recommended way to do this is to use a syntax that specifies the `std`
// namespace as a name qualifier, like `struct std::hash<Gtid_t>`, rather than
// enclose the entire struct in a namespace block.
//
// However, gcc 11.4.0 on ARM has a bug that makes it produce "error:
// redefinition of 'struct std::indirectly_readable_traits<_Iter>'" when using
// that syntax. See https://godbolt.org/z/Yd13x8M56 vs
// https://godbolt.org/z/Ys8hqbevs .
//
// Todo: Switch to the recommended syntax once we drop support for compilers
// having this bug.
namespace std {
template <class Iterator_t>
  requires std::derived_from<Iterator_t,
                             mysql::iterators::Iterator_interface<Iterator_t>>
// The C++ standard explicitly allows specialization of
// `std::indirectly_readable_traits`, but clang-tidy complains about extending
// `std`. Suppress that check.
//
// NOLINTNEXTLINE(cert-dcl58-cpp)
struct indirectly_readable_traits<Iterator_t> {
  using value_type = std::remove_cvref_t<decltype(*std::declval<Iterator_t>())>;
};

template <class Iterator_t>
  requires std::derived_from<Iterator_t,
                             mysql::iterators::Iterator_interface<Iterator_t>>
// The C++ standard explicitly allows specialization of
// `std::incrementable_traits`, but clang-tidy complains about extending `std`.
// Suppress that check.
//
// NOLINTNEXTLINE(cert-dcl58-cpp)
struct incrementable_traits<Iterator_t> {
  using difference_type = std::ptrdiff_t;
};

}  // namespace std

// addtogroup GroupLibsMysqlIterators
/// @}

#endif  // ifndef MYSQL_ITERATORS_ITERATOR_INTERFACE_H
