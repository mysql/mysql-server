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

#ifndef MYSQL_RANGES_DISJOINT_PAIRS_H
#define MYSQL_RANGES_DISJOINT_PAIRS_H

/// @file
/// Experimental API header

/// @addtogroup GroupLibsMysqlRanges
/// @{

#include <iterator>                              // iterator_traits
#include <ranges>                                // view_base
#include <utility>                               // declval
#include "my_compiler.h"                         // MY_COMPILER_DIAGNOSTIC_PUSH
#include "mysql/iterators/iterator_interface.h"  // Iterator_interface
#include "mysql/ranges/collection_interface.h"   // Collection_interface
#include "mysql/ranges/view_sources.h"           // View_source

namespace mysql::ranges::detail {

struct Make_pair {
  /// Return a pair from the given arguments.
  ///
  /// The component types will be references if the arguments are lvalue
  /// references, and values if the arguments are rvalue references or values.
  template <class Type_t>
  constexpr static std::pair<Type_t &, Type_t &> make_pair(Type_t &first,
                                                           Type_t &second) {
    return {first, second};
  }

  template <class Type_t>
  constexpr static std::pair<Type_t, Type_t> make_pair(const Type_t &first,
                                                       const Type_t &second) {
    return {first, second};
  }
};

}  // namespace mysql::ranges::detail
namespace mysql::ranges {

/// Iterator used by Disjoint_pairs_interface and Disjoint_pairs_view: this
/// yields the disjoint, adjacent pairs of values from the source iterator.
///
/// This caches two iterator positions internally. It returns the pairs by
/// value.
///
/// @tparam Source_iterator_tp The source iterator.
///
/// @tparam Make_pair_tp Function object that creates a pair from two input
/// values. By default, uses a function object that creates std::pair objects.
template <std::forward_iterator Source_iterator_tp,
          class Make_pair_tp = detail::Make_pair>
class Disjoint_pairs_iterator
    : public mysql::iterators::Iterator_interface<
          Disjoint_pairs_iterator<Source_iterator_tp, Make_pair_tp>> {
 public:
  using Source_iterator_t = Source_iterator_tp;
  using Make_pair_t = Make_pair_tp;
  using Source_value_t = mysql::ranges::Iterator_value_type<Source_iterator_t>;
  using Value_t = decltype(Make_pair_t::make_pair(
      std::declval<Source_value_t>(), std::declval<Source_value_t>()));

  /// Default constructor, which leaves the object in a state that is not
  /// usable, except it can be assigned to. Provided because std::input_iterator
  /// requires std::default_initializable.
  Disjoint_pairs_iterator() noexcept = default;

  /// Construct a new iterator, where the first component points to the given
  /// position.
  explicit Disjoint_pairs_iterator(const Source_iterator_t &position) noexcept
      : m_first(position), m_second{} {}

  /// @return the current value.
  [[nodiscard]] Value_t get() const {
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=119592
    MY_COMPILER_DIAGNOSTIC_PUSH()
    MY_COMPILER_GCC_DIAGNOSTIC_IGNORE("-Wmaybe-uninitialized")
    if (!m_second.has_value()) {
      m_second = std::ranges::next(m_first);
    }
    return Make_pair_t::make_pair(*m_first, *m_second.value());
    MY_COMPILER_DIAGNOSTIC_POP()
  }

  /// Move to the next position.
  void next() {
// If we enable this optimization in gcc, it produces spurious
// -Wmaybe-uninitialized warnings, likely due to
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=119592 . The warnings are in
// functions that use classes that inherit from Disjoint_pairs_iterator, and
// MY_COMPILER_GCC_DIAGNOSTIC_IGNORE in the current function does not
// silence the warning. Therefore, enabling the optimization in gcc would
// make the class hard to use, so instead we disable the optimization until
// gcc has been fixed. Unfortunately, clang defines __GNUC__, so in order to
// not disable the optimization on clang we also check if __clang__ is
// defined.
#if !defined(__GNUC__) || defined(__clang__)
    if (m_second.has_value()) {
      m_first = std::next(m_second.value());
    } else {
      std::ranges::advance(m_first, 2);
    }
#else
    std::ranges::advance(m_first, 2);
#endif
    m_second.reset();
  }

  /// Move to the previous position.
  void prev()
    requires std::bidirectional_iterator<Source_iterator_t>
  {
    // Not std::advance, since that is undefined, since this iterator's value
    // type is not a reference, so that it does not model
    // LegacyBidirectionalIterator.
    std::ranges::advance(m_first, -2);
    m_second.reset();
  }

  /// Move the iterator the given number of steps.
  void advance(std::ptrdiff_t delta)
    requires std::random_access_iterator<Source_iterator_t>
  {
    m_first += 2 * delta;
    m_second.reset();
  }

  /// @return the number of steps from the other iterator to this iterator.
  [[nodiscard]] std::ptrdiff_t distance_from(
      const Disjoint_pairs_iterator &other) const
    requires std::random_access_iterator<Source_iterator_t>
  {
    return (m_first - other.m_first) >> 1;
  }

  /// @return true if this iterator equals the other iterator.
  [[nodiscard]] bool is_equal(const Disjoint_pairs_iterator &other) const {
    return m_first == other.m_first;
  }

 private:
  /// Iterator to the first position.
  Source_iterator_t m_first;

  /// Iterator to the second position.
  ///
  /// This is mutable because it is only a cache and gets updated lazily.
  ///
  /// (We cannot update this member when advancing the iterator, because that
  /// would make it advance past the past-the-end iterator, which is undefined
  /// behavior. Instead we unset it, and initialize it on the next dereference
  /// operation.)
  mutable std::optional<Source_iterator_t> m_second;
};  // class Disjoint_pairs_iterator

/// Factory function to create a Disjoint_pairs_iterator.
template <class Pair_t = detail::Make_pair, class Iterator_t>
[[nodiscard]] auto make_disjoint_pairs_iterator(const Iterator_t &position) {
  return Disjoint_pairs_iterator<Iterator_t, Pair_t>(position);
}

/// CRTP base used to define classes that yield disjoint, adjacent pairs of
/// elements from an even-length source sequence.
///
/// For example, if the source sequence is 2, 3, 5, 7, 11, 13, then this
/// yields the pairs <2,3>, <5,7>, <11,13>. It is conceptually similar to
/// std::stride_view<std::slide_view<Source_tp>>, except the window
/// width is defined at compile time and each window is wrapped in an object.
///
/// This is for objects that own the source. If you need objects that
/// do not own the source - a *view* - use Disjoint_pairs_view.
///
/// @tparam Self_tp Class that derives from this class. It must implement the
/// member function disjoint_pairs_source(), returning a reference to an object
/// with the member functions `begin`, `end`, `empty`, and `size`. The distance
/// between `begin` and `end` must be even.
///
/// @tparam Make_pair_tp Class containing a static member function `make_pair`
/// that defines how to construct a pair from two adjacent elements. By default,
/// uses `detail::Make_pair`, for which `make_pair` constructs and returns a
/// `std::pair` object.
template <class Self_tp, class Make_pair_tp>
class Disjoint_pairs_interface
    : public mysql::ranges::Collection_interface<Self_tp> {
  using Self_t = Self_tp;
  using Base_t = mysql::ranges::Collection_interface<Self_tp>;

 public:
  using Make_pair_t = Make_pair_tp;

  /// Return const iterator to the beginning.
  [[nodiscard]] auto begin() const {
    return make_disjoint_pairs_iterator<Make_pair_t>(source().begin());
  }

  /// Return const iterator to sentinel.
  [[nodiscard]] auto end() const {
    return make_disjoint_pairs_iterator<Make_pair_t>(source().end());
  }

  /// Return iterator to the beginning.
  [[nodiscard]] auto begin() {
    return make_disjoint_pairs_iterator<Make_pair_t>(source().begin());
  }

  /// Return iterator to sentinel.
  [[nodiscard]] auto end() {
    return make_disjoint_pairs_iterator<Make_pair_t>(source().end());
  }

  /// Return true if the range is empty.
  ///
  /// Use `Collection_interface::empty` if
  /// `Self_t::disjoint_pairs_source().empty()` does not exist.
  [[nodiscard]] bool empty() const {
    // On older versions of gcc and clang we could use `using Base_t::empty`
    // instead of defining this function, but not on gcc > 14 and clang > 17:
    // see
    // https://stackoverflow.com/questions/79552469/constraint-does-not-disambiguate-function-in-base-class-from-function-in-derived
    return Base_t::empty();
  }

  /// Return true if the range is empty.
  ///
  /// Use `Self_t::disjoint_pairs_source().empty()` if it exists.
  [[nodiscard]] bool empty() const
    requires requires(Self_t s) { s.disjoint_pairs_source().empty(); }
  {
    return source().empty();
  }

  /// Return the number of pairs, i.e., half the size of the source.
  ///
  /// Use `Collection_interface::size` if
  /// `Self_t::disjoint_pairs_source().size()` does not exist.
  [[nodiscard]] auto size() const {
    // See comment in empty above.
    return Base_t::size();
  }

  /// Return the number of pairs, i.e., half the size of the source.
  ///
  /// Use `Self_t::disjoint_pairs_source().size()` if it exists.
  [[nodiscard]] auto size() const
    requires requires(Self_t s) { s.disjoint_pairs_source().size(); }
  {
    return source().size() >> 1;
  }

 private:
  [[nodiscard]] const auto &source() const {
    return static_cast<const Self_t *>(this)->disjoint_pairs_source();
  }
  [[nodiscard]] auto &source() {
    return static_cast<Self_t *>(this)->disjoint_pairs_source();
  }
};  // class Disjoint_pairs_interface

/// View over an even-length sequence, yielding the disjoint, adjacent pairs of
/// elements.
///
/// For example, if the source sequence is 2, 3, 5, 7, 11, 13, then this
/// yields the pairs <2,3>, <5,7>, <11,13>. It is conceptually similar to
/// std::stride_view<std::slide_view<Source_tp>>, except the window
/// width is defined at compile time and each window is wrapped in an object.
///
/// This a view, which does not own the source. If you need to define
/// a class that owns the source, use Disjoint_pairs_interface.
///
/// @tparam Source_tp Range that this is a view over. It must have the
/// member functions `begin`, `end`, `empty`, and `size`. The distance between
/// `begin` and `end` must be even.
///
/// @tparam Make_pair_tp Class containing a static member function `make_pair`
/// that defines how to construct a pair from two adjacent elements. By default,
/// uses `detail::Make_pair`, for which `make_pair` constructs and returns a
/// `std::pair` object.
template <class Source_tp, class Make_pair_tp = detail::Make_pair>
class Disjoint_pairs_view
    : public Disjoint_pairs_interface<
          Disjoint_pairs_view<Source_tp, Make_pair_tp>, Make_pair_tp>,
      public std::ranges::view_base {
  using Source_ref_t = mysql::ranges::View_source<Source_tp>;

 public:
  using Source_t = Source_tp;
  using Make_pair_t = Make_pair_tp;
  using Source_iterator_t = mysql::ranges::Range_iterator_type<Source_t>;
  using Source_const_iterator_t =
      mysql::ranges::Range_const_iterator_type<Source_t>;
  using Iterator_t = Disjoint_pairs_iterator<Source_iterator_t, Make_pair_t>;
  using Const_iterator_t =
      Disjoint_pairs_iterator<Source_const_iterator_t, Make_pair_t>;

  // Cannot default-construct in case the source is a view type (since
  // we use View_source).
  Disjoint_pairs_view() = default;

  /// Construct a view that over the given source.
  ///
  /// The caller must ensure that the source outlives this object.
  explicit Disjoint_pairs_view(const Source_t &source) : m_source(source) {}

  // Defaults for rule-of-5 members
  Disjoint_pairs_view(const Disjoint_pairs_view &) = default;
  Disjoint_pairs_view(Disjoint_pairs_view &&) = default;
  Disjoint_pairs_view &operator=(const Disjoint_pairs_view &) = default;
  Disjoint_pairs_view &operator=(Disjoint_pairs_view &&) = default;
  ~Disjoint_pairs_view() = default;

  /// Return const reference to the source.
  const Source_t &disjoint_pairs_source() const { return m_source.reference(); }

 private:
  /// source, either owned or not.
  Source_ref_t m_source{};
};  // Disjoint_pairs_view

/// Factory to construct a Disjoint_pairs_view from a given range.
///
/// @tparam Make_pair_t Class containing a static member function `make_pair`
/// that defines how to construct a pair from two adjacent elements. By default,
/// uses `detail::Make_pair`, for which `make_pair` constructs and returns a
/// `std::pair` object.
///
/// @tparam Source_t Range that this is a view over. It must have the
/// member functions `begin`, `end`, `empty`, and `size`. The distance between
/// `begin` and `end` must be even.
///
/// @return New Disjoint_pairs_view object.
template <class Make_pair_t = detail::Make_pair, class Source_t>
[[nodiscard]] auto make_disjoint_pairs_view(const Source_t &source) {
  return Disjoint_pairs_view<Source_t, Make_pair_t>(source);
}

}  // namespace mysql::ranges

// addtogroup GroupLibsMysqlRanges
/// @}

#endif  // ifndef MYSQL_RANGES_DISJOINT_PAIRS_H
