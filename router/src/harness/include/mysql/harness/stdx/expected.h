/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_STDX_EXPECTED_H_
#define MYSQL_HARNESS_STDX_EXPECTED_H_

// implementation of C++23's std::expected<> in C++20
//
// and http://wg21.link/p2505 (r0) for .and_then(), .or_else() and .transform()
//
// See http://wg21.link/p0323

#include <cassert>
#include <functional>  // invoke
#include <initializer_list>
#include <memory>  // construct_at, destroy_at
#include <type_traits>
#include <utility>  // std::forward

#include "my_compiler.h"

#if defined(__cpp_concepts)
#if __cpp_concepts >= 202002L
#define CXX_HAS_CONDITIONAL_TRIVIAL_DESTRUCTOR
#elif defined(__clang_major__)
#if __clang_major__ >= 15
// clang added partial support for overloading destructors (P0848) in clang-15
// ... but didn't bump the concepts version yet.
#define CXX_HAS_CONDITIONAL_TRIVIAL_DESTRUCTOR
#endif
#elif defined(__GNUC_MAJOR__)
#if __GNUC_MAJOR__ >= 10
// gcc added partial support for overloading destructors (P0848) in gcc-10
// ... but didn't bump the concepts version yet.
#define CXX_HAS_CONDITIONAL_TRIVIAL_DESTRUCTOR
#endif
#else  // msvc
#define CXX_HAS_CONDITIONAL_TRIVIAL_DESTRUCTOR
#endif
#endif

namespace stdx {

#ifndef IN_DOXYGEN
// doxygen gets confused by the inheritence structure of bad_expected_access.

template <class E>
class bad_expected_access;

template <>
class bad_expected_access<void> : public std::exception {
 protected:
  bad_expected_access() {}
  bad_expected_access(const bad_expected_access &) = default;
  bad_expected_access(bad_expected_access &&) = default;

  bad_expected_access &operator=(const bad_expected_access &) = default;
  bad_expected_access &operator=(bad_expected_access &&) = default;

  ~bad_expected_access() override = default;

 public:
  const char *what() const noexcept override { return "bad expected access"; }
};

template <class E>
class bad_expected_access : public bad_expected_access<void> {
 public:
  explicit bad_expected_access(E e) : e_(std::move(e)) {}

  // error accessors
  const E &error() const &noexcept { return e_; }
  const E &&error() const &&noexcept { return e_; }
  E &error() &noexcept { return e_; }
  E &&error() &&noexcept { return e_; }

 private:
  E e_;
};
#endif

// inplace construction of unexpected values.
struct unexpect_t {
  explicit unexpect_t() = default;
};

inline constexpr unexpect_t unexpect{};

template <typename E>
class unexpected {
 public:
  static_assert(!std::is_same<E, void>::value, "E must not be void");

  using error_type = E;

  // constructor

  constexpr unexpected(const unexpected &) = default;
  constexpr unexpected(unexpected &&) noexcept = default;

  template <class Err = E>
  constexpr explicit unexpected(Err &&err)  //
      noexcept(std::is_nothrow_constructible_v<E, Err>)
    requires(!std::is_same_v<std::remove_cvref_t<Err>, unexpected> &&
             !std::is_same_v<std::remove_cvref_t<Err>, std::in_place_t> &&
             std::is_constructible_v<E, Err>)  //
      : error_(std::forward<Err>(err)) {}

  template <class... Args>
  constexpr explicit unexpected(std::in_place_t, Args &&...args)  //
      noexcept(std::is_nothrow_constructible_v<E, Args...>)
    requires std::constructible_from<E, Args &&...>
      : error_(std::forward<Args>(args)...) {}

  template <class U, class... Args>
  constexpr explicit unexpected(std::in_place_t, std::initializer_list<U> il,
                                Args &&...args)  //
      noexcept(std::is_nothrow_constructible_v<E, std::initializer_list<U> &,
                                               Args...>)
    requires(std::constructible_from<E, std::initializer_list<U>,
                                     Args && ...>)  //
      : error_(il, std::forward<Args>(args)...) {}

  // assignment

  constexpr unexpected &operator=(const unexpected &) = default;
  constexpr unexpected &operator=(unexpected &&) noexcept = default;

  // value access

  constexpr error_type &error() &noexcept { return error_; }
  constexpr const error_type &error() const &noexcept { return error_; }
  constexpr error_type &&error() &&noexcept { return std::move(error_); }
  constexpr const error_type &&error() const &&noexcept {
    return std::move(error_);
  }

  // swap

  constexpr void swap(unexpected &other)  //
      noexcept(std::is_nothrow_swappable_v<E>)
    requires(std::is_swappable_v<E>)
  {
    using std::swap;

    swap(error_, other.error_);
  }

  // comparison

  template <class E2>
  friend constexpr bool operator==(const unexpected &lhs,
                                   const stdx::unexpected<E2> &rhs) {
    return lhs.error() == rhs.error();
  }

  friend constexpr void swap(unexpected &lhs, unexpected &rhs) noexcept(
      noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
  }

 private:
  error_type error_;
};

// deduction guide
template <class E>
unexpected(E) -> unexpected<E>;

template <class T, class E>
class expected;

namespace impl {

template <class T>
constexpr bool is_expected = false;

template <class T, class E>
constexpr bool is_expected<expected<T, E>> = true;

template <class T>
constexpr bool is_unexpected = false;

template <class T>
constexpr bool is_unexpected<unexpected<T>> = true;

template <class NewT, class OldT, class... Args>
constexpr void reinit_expected(NewT *new_val, OldT *old_val, Args &&...args) {
  if constexpr (std::is_nothrow_constructible_v<NewT, Args...>) {
    std::destroy_at(old_val);
    std::construct_at(new_val, std::forward<Args>(args)...);
  } else if constexpr (std::is_nothrow_move_constructible_v<NewT>) {
    NewT tmp(std::forward<Args>(args)...);

    std::destroy_at(old_val);
    std::construct_at(new_val, std::move(tmp));
  } else {
    OldT tmp(std::move(*old_val));

    std::destroy_at(old_val);
    try {
      std::construct_at(new_val, std::forward<Args>(args)...);
    } catch (...) {
      std::construct_at(old_val, std::move(tmp));
      throw;
    }
  }
}

}  // namespace impl

namespace base {

template <class Exp, class Func,
          typename value_type = typename std::decay_t<Exp>::value_type>
  requires(std::is_void_v<value_type>
               ? std::is_invocable_v<Func>
               : std::is_invocable_v<Func, value_type>)  //
constexpr auto and_then_impl(Exp &&exp, Func &&func) {
  if constexpr (std::is_void_v<value_type>) {
    using Ret = std::invoke_result_t<Func>;

    static_assert(stdx::impl::is_expected<Ret>,
                  "Func must return a stdx::expected<>");

    if (exp.has_value()) {
      return std::invoke(func);
    } else {
      return Ret{stdx::unexpect, std::forward<Exp>(exp).error()};
    }
  } else {
    using Ret = std::invoke_result_t<Func, value_type>;

    static_assert(stdx::impl::is_expected<Ret>,
                  "Func must return a stdx::expected<>");

    if (exp.has_value()) {
      return std::invoke(func, *std::forward<Exp>(exp));
    } else {
      return Ret{stdx::unexpect, std::forward<Exp>(exp).error()};
    }
  }
}

template <class Exp, class Func,
          typename error_type = typename std::decay_t<Exp>::error_type>
  requires std::is_invocable_v<Func, error_type>
constexpr auto or_else_impl(Exp &&exp, Func &&func) {
  static_assert(
      std::is_same_v<
          std::remove_cvref_t<std::invoke_result_t<Func, error_type>>, Exp>,
      "Func must return an expected<>");

  if (exp.has_value()) {
    return std::forward<Exp>(exp);
  }

  return std::invoke(std::forward<Func>(func), std::forward<Exp>(exp).error());
}

}  // namespace base

template <class T, class E>
class expected {
 public:
  static_assert(!std::is_void_v<E>, "E must not be void");
  static_assert(!std::is_reference_v<T>, "T must not be a reference");
  static_assert(!std::is_function_v<T>, "T must not be a function");
  static_assert(!std::is_same_v<std::remove_cv_t<T>, std::in_place_t>,
                "T must not be std::in_place_t");
  static_assert(!std::is_same_v<std::remove_cv_t<T>, unexpect_t>,
                "T must not be stdx::unexpect_t");
  static_assert(!std::is_same_v<T, std::remove_cv<unexpected<E>>>,
                "T must not be unexpected<E>");

  using value_type = T;
  using error_type = E;
  using unexpected_type = unexpected<E>;

  template <typename U>
  using rebind = expected<U, error_type>;

  // (1)
  constexpr expected()  //
      noexcept(std::is_nothrow_default_constructible_v<T>)
    requires std::is_default_constructible_v<T>
      : val_{}, has_value_(true) {}

  // (2)
  constexpr expected(const expected &other) = default;

  constexpr expected(const expected &other)  //
      noexcept((std::is_nothrow_copy_constructible_v<T> &&
                std::is_nothrow_copy_constructible_v<E>))
    requires((std::is_copy_constructible_v<T> &&
              std::is_copy_constructible_v<E> &&
              (!std::is_trivially_copy_constructible_v<T> ||
               !std::is_trivially_copy_constructible_v<E>)))
      : has_value_{other.has_value()} {
    if (has_value()) {
      std::construct_at(std::addressof(val_), other.val_);
    } else {
      std::construct_at(std::addressof(unex_), other.unex_);
    }
  }

  // (3)
  constexpr expected(expected &&other) = default;

  constexpr expected(expected &&other)  //
      noexcept((std::is_nothrow_move_constructible_v<E> &&
                std::is_nothrow_move_constructible_v<T>))
    requires((std::is_move_constructible_v<T> &&
              std::is_move_constructible_v<E> &&
              (!std::is_trivially_move_constructible_v<T> ||
               !std::is_trivially_move_constructible_v<E>)))  //
      : has_value_{other.has_value()} {
    if (has_value()) {
      std::construct_at(std::addressof(val_), std::move(other.val_));
    } else {
      std::construct_at(std::addressof(unex_), std::move(other.unex_));
    }
  }

  template <class UF, class GF>
  static constexpr bool constructor_is_explicit =
      !std::is_convertible_v<UF, T> || !std::is_convertible_v<GF, E>;

  //
  template <class U, class G, class UF, class GF>
  static constexpr bool can_value_convert_construct =
      (std::is_constructible_v<T, UF> &&  //
       std::is_constructible_v<E, GF> &&
       !std::is_constructible_v<T, expected<U, G> &> &&
       !std::is_constructible_v<T, expected<U, G>> &&
       !std::is_constructible_v<T, const expected<U, G> &> &&
       !std::is_constructible_v<T, const expected<U, G>> &&
       !std::is_convertible_v<expected<U, G> &, T> &&
       !std::is_convertible_v<expected<U, G>, T> &&
       !std::is_convertible_v<const expected<U, G> &, T> &&
       !std::is_convertible_v<const expected<U, G>, T> &&
       !std::is_constructible_v<unexpected<E>, expected<U, G> &> &&
       !std::is_constructible_v<unexpected<E>, expected<U, G>> &&
       !std::is_constructible_v<unexpected<E>, const expected<U, G> &> &&
       !std::is_constructible_v<unexpected<E>, const expected<U, G>>);

  // (4)
  template <class U, class G, class UF = std::add_lvalue_reference_t<const U>,
            class GF = const G &>
  constexpr explicit(constructor_is_explicit<UF, GF>)
      expected(const expected<U, G> &rhs)
    requires can_value_convert_construct<U, G, UF, GF>
      : has_value_{rhs.has_value()} {
    if (rhs.has_value()) {
      std::construct_at(std::addressof(val_), std::forward<UF>(*rhs));
    } else {
      std::construct_at(std::addressof(unex_), std::forward<GF>(rhs.error()));
    }
  }

  // (5)
  template <class U, class G, class UF = U, class GF = G>
  constexpr explicit(constructor_is_explicit<UF, GF>)
      expected(expected<U, G> &&rhs)
    requires can_value_convert_construct<U, G, UF, GF>
      : has_value_{rhs.has_value()} {
    if (rhs.has_value()) {
      std::construct_at(std::addressof(val_), std::forward<UF>(*rhs));
    } else {
      std::construct_at(std::addressof(unex_), std::forward<GF>(rhs.error()));
    }
  }

  template <class U>
  static constexpr bool can_construct_from_value_type =
      !std::is_same_v<std::in_place_t, std::remove_cvref_t<U>> &&
      !std::is_same_v<expected, std::remove_cvref_t<U>> &&
      std::is_constructible_v<T, U> &&
      !impl::is_unexpected<std::remove_cvref_t<U>> &&
      !impl::is_expected<std::remove_cvref_t<T>>;

  // (6)
  template <class U = T>
  constexpr explicit(!std::is_convertible_v<U, T>) expected(U &&val)
    requires can_construct_from_value_type<U>
      : val_(std::forward<U>(val)), has_value_{true} {}

  // (7)
  template <class G>
  constexpr explicit(!std::is_convertible_v<const G &, E>)
      expected(const unexpected<G> &err)
    requires std::is_constructible_v<E, const G &>
      : unex_(err.error()), has_value_{false} {}

  // (8)
  template <class G>
  constexpr explicit(!std::is_convertible_v<G, E>) expected(unexpected<G> &&err)
    requires std::is_constructible_v<E, G>
      : unex_(std::move(err.error())), has_value_{false} {}

  // (9)
  template <class... Args>
  constexpr explicit expected(std::in_place_t, Args &&...args)
    requires std::is_constructible_v<T, Args...>
      : val_(std::forward<Args>(args)...), has_value_{true} {}

  // (10)
  template <class U, class... Args>
  constexpr explicit expected(std::in_place_t, std::initializer_list<U> il,
                              Args &&...args)
    requires std::is_constructible_v<T, std::initializer_list<U> &, Args...>
      : val_(il, std::forward<Args>(args)...), has_value_{true} {}

  // (11) skipped, only for T-is-void

  // (12)
  template <class... Args>
  constexpr explicit expected(stdx::unexpect_t, Args &&...args)
    requires std::is_constructible_v<E, Args &&...>
      : unex_(std::forward<Args>(args)...), has_value_{false} {}

  // (13)
  template <class U, class... Args>
  constexpr explicit expected(stdx::unexpect_t, std::initializer_list<U> il,
                              Args &&...args)
    requires std::is_constructible_v<E, std::initializer_list<U> &, Args...>
      : unex_(il, std::forward<Args>(args)...), has_value_{false} {}

  // assignment

  // (1)
  constexpr expected &operator=(expected const &other)
    requires((std::is_copy_assignable_v<T> &&     //
              std::is_copy_constructible_v<T> &&  //
              std::is_copy_assignable_v<E> &&     //
              std::is_copy_constructible_v<E> &&  //
              (std::is_nothrow_move_constructible_v<T> ||
               std::is_nothrow_move_constructible_v<E>)))
  {
    if (other.has_value()) {
      assign_val(other.val_);
    } else {
      assign_unex(other.unex_);
    }

    return *this;
  }

  // (2)
  constexpr expected &operator=(expected &&other)
    requires((std::is_move_assignable_v<T> &&     //
              std::is_move_constructible_v<T> &&  //
              std::is_move_assignable_v<E> &&     //
              std::is_move_constructible_v<E> &&  //
              (std::is_nothrow_move_constructible_v<T> ||
               std::is_nothrow_move_constructible_v<E>)))
  {
    if (other.has_value()) {
      assign_val(std::move(other.val_));
    } else {
      assign_unex(std::move(other.unex_));
    }

    return *this;
  }

  // (3) assign from expected value
  template <class U = T>
  constexpr expected &operator=(U &&val)
    requires((!std::is_same_v<expected<T, E>, std::remove_cvref_t<U>> &&  //
              !impl::is_unexpected<std::remove_cvref_t<U>> &&             //
              std::is_constructible_v<T, U> &&                            //
              std::is_assignable_v<T &, U> &&                             //
              (std::is_nothrow_constructible_v<T, U> ||                   //
               std::is_nothrow_move_constructible_v<T> ||                 //
               std::is_nothrow_move_constructible_v<E>)))
  {
    assign_val(std::forward<U>(val));

    return *this;
  }

  // (4)
  template <class G, class GF = const G &>
  constexpr expected &operator=(const unexpected<G> &other)
    requires((std::is_constructible_v<E, GF> &&            //
              std::is_assignable_v<E &, GF> &&             //
              (std::is_nothrow_constructible_v<E, GF> ||   //
               std::is_nothrow_move_constructible_v<T> ||  //
               std::is_nothrow_move_constructible_v<E>)))
  {
    assign_unex(other.error());

    return *this;
  }

  // (5)
  template <class G, class GF = G>
  constexpr expected &operator=(unexpected<G> &&other)
    requires((std::is_constructible_v<E, GF> &&            //
              std::is_assignable_v<E &, GF> &&             //
              (std::is_nothrow_constructible_v<E, GF> ||   //
               std::is_nothrow_move_constructible_v<T> ||  //
               std::is_nothrow_move_constructible_v<E>)))
  {
    assign_unex(std::move(other.error()));

    return *this;
  }

  // destruct
#if defined(CXX_HAS_CONDITIONAL_TRIVIAL_DESTRUCTOR)
  constexpr ~expected()
    requires((std::is_trivially_destructible_v<T> &&
              std::is_trivially_destructible_v<E>))
  = default;
#endif

  constexpr ~expected() {
    if (has_value()) {
      std::destroy_at(std::addressof(val_));
    } else {
      std::destroy_at(std::addressof(unex_));
    }
  }

  // emplace
  template <class... Args>
  constexpr T &emplace(Args &&...args) noexcept
    requires(std::is_nothrow_constructible_v<T, Args...>)
  {
    if (has_value()) {
      std::destroy_at(std::addressof(val_));
    } else {
      std::destroy_at(std::addressof(unex_));
      has_value_ = true;
    }

    return *std::construct_at(std::addressof(val_),
                              std::forward<Args>(args)...);
  }

  template <class U, class... Args>
  constexpr T &emplace(std::initializer_list<U> il, Args &&...args) noexcept
    requires(
        std::is_nothrow_constructible_v<T, std::initializer_list<U> &, Args...>)
  {
    if (has_value()) {
      std::destroy_at(std::addressof(val_));
    } else {
      std::destroy_at(std::addressof(unex_));
      has_value_ = true;
    }

    return *std::construct_at(std::addressof(val_), il,
                              std::forward<Args>(args)...);
  }

  // swap
  constexpr void swap(expected &other)                      //
      noexcept((std::is_nothrow_move_constructible_v<T> &&  //
                std::is_nothrow_move_constructible_v<E> &&  //
                std::is_nothrow_swappable_v<T> &&           //
                std::is_nothrow_swappable_v<E>))
    requires((std::is_swappable_v<T> &&                    //
              std::is_swappable_v<E> &&                    //
              std::is_move_constructible_v<T> &&           //
              std::is_move_constructible_v<E> &&           //
              (std::is_nothrow_move_constructible_v<T> ||  //
               std::is_nothrow_move_constructible_v<E>)))
  {
    using std::swap;

    if (bool(*this) && bool(other)) {
      swap(val_, other.val_);
    } else if (!bool(*this) && !bool(other)) {
      swap(unex_, other.unex_);
    } else if (bool(*this) && !bool(other)) {
      if constexpr (std::is_nothrow_move_constructible_v<E>) {
        error_type tmp(std::move(other.error()));

        std::destroy_at(std::addressof(other.unex_));
        try {
          std::construct_at(std::addressof(other.val_), std::move(val_));
          std::destroy_at(std::addressof(val_));
          std::construct_at(std::addressof(unex_), std::move(tmp));
        } catch (...) {
          // restore old value.
          std::construct_at(std::addressof(other.unex_), std::move(tmp));
          throw;
        }
      } else {
        value_type tmp(std::move(val_));

        std::destroy_at(std::addressof(val_));
        try {
          std::construct_at(std::addressof(unex_), std::move(other.unex_));
          std::destroy_at(std::addressof(other.unex_));
          std::construct_at(std::addressof(val_), std::move(tmp));
        } catch (...) {
          // restore old value.
          std::construct_at(std::addressof(val_), std::move(tmp));
          throw;
        }
      }

      has_value_ = false;
      other.has_value_ = true;
    } else if (!bool(*this) && bool(other)) {
      other.swap(*this);
    }
  }

  constexpr bool has_value() const { return has_value_; }
  constexpr explicit operator bool() const noexcept { return has_value(); }

  // value accessors

  // the macro only exists for clang-format-10 to not get the formatting wrong.
#define LIKELY(x) (x) [[likely]]

  // (1)
  constexpr value_type &value() & {
    static_assert(std::is_copy_constructible_v<E>,
                  "error-type must be copy-constructible");
    if LIKELY (has_value()) {
      return val_;
    }

    throw bad_expected_access(std::as_const(error()));
  }

  // (2)
  constexpr const value_type &value() const & {
    static_assert(std::is_copy_constructible_v<E>,
                  "error-type must be copy-constructible");

    if LIKELY (has_value()) {
      return val_;
    }

    throw bad_expected_access(std::as_const(error()));
  }

  // (3)
  constexpr value_type &&value() && {
    static_assert(std::is_copy_constructible_v<E> &&
                  std::is_constructible_v<E, decltype(std::move(error()))>);

    if LIKELY (has_value()) {
      return std::move(val_);
    }

    throw bad_expected_access(std::move(error()));
  }

  // (4)
  constexpr const value_type &&value() const && {
    static_assert(std::is_copy_constructible_v<E> &&
                  std::is_constructible_v<E, decltype(std::move(error()))>);

    if LIKELY (has_value()) {
      return std::move(val_);
    }

    throw bad_expected_access(std::move(error()));
  }

#undef LIKELY

  // unchecked value access

  // (1)
  constexpr const value_type *operator->() const noexcept {
    assert(has_value());

    return std::addressof(val_);
  }

  // (2)
  constexpr value_type *operator->() noexcept {
    assert(has_value());

    return std::addressof(val_);
  }

  // (3)
  constexpr const value_type &operator*() const &noexcept {
    assert(has_value());

    return val_;
  }

  // (4)
  constexpr value_type &operator*() &noexcept {
    assert(has_value());

    return val_;
  }

  // (5)
  constexpr const value_type &&operator*() const &&noexcept {
    assert(has_value());

    return std::move(val_);
  }

  // (6)
  constexpr value_type &&operator*() &&noexcept {
    assert(has_value());

    return std::move(val_);
  }

  // value_or

  // (1)
  template <class U>
  constexpr value_type value_or(U &&v) const & {
    static_assert(
        std::is_copy_constructible_v<T> && std::is_convertible_v<U &&, T>,
        "T must be copy-constructible and convertible from U&&");

    return has_value() ? **this : static_cast<T>(std::forward<U>(v));
  }

  // (2)
  template <class U>
  constexpr value_type value_or(U &&v) && {
    static_assert(
        std::is_move_constructible_v<T> && std::is_convertible_v<U &&, T>,
        "T must be move-constructible and convertible from U&&");

    return has_value() ? std::move(**this) : static_cast<T>(std::forward<U>(v));
  }

  // error accessors
  constexpr const error_type &error() const & { return unex_; }
  constexpr const error_type &&error() const && { return std::move(unex_); }
  constexpr error_type &error() & { return unex_; }
  constexpr error_type &&error() && { return std::move(unex_); }

  // operator==

  // (1)
  template <class T2, class E2>
    requires(!std::is_void_v<T2>)
  friend constexpr bool operator==(const expected &lhs,
                                   const stdx::expected<T2, E2> &rhs) {
    if (lhs.has_value() != rhs.has_value()) return false;

    if (!lhs.has_value()) return lhs.error() == rhs.error();
    return static_cast<bool>(*lhs == *rhs);
  }

  // (3)
  template <class T2>
    requires(!impl::is_expected<T2>)
  friend constexpr bool operator==(const expected &lhs, const T2 &rhs) {
    return lhs.has_value() && static_cast<bool>(*lhs == rhs);
  }

  // (4)
  template <class E2>
  friend constexpr bool operator==(const expected &lhs,
                                   const unexpected<E2> &rhs) {
    if (lhs.has_value()) return false;

    return static_cast<bool>(lhs.error() == rhs.error());
  }

  //
  // and_then
  //

  template <class Func>
  constexpr auto and_then(Func &&func) & {
    return base::and_then_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto and_then(Func &&func) && {
    return base::and_then_impl(std::move(*this), std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto and_then(Func &&func) const & {
    return base::and_then_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto and_then(Func &&func) const && {
    return base::and_then_impl(std::move(*this), std::forward<Func>(func));
  }

  //
  // or_else
  //

  template <class Func>
  constexpr auto or_else(Func &&func) & {
    return base::or_else_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto or_else(Func &&func) && {
    return base::or_else_impl(std::move(*this), std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto or_else(Func &&func) const & {
    return base::or_else_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto or_else(Func &&func) const && {
    return base::or_else_impl(std::move(*this), std::forward<Func>(func));
  }

  //
  // transform
  //

  template <class Func>
  constexpr auto transform(Func &&func) & {
    return expected_transform_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto transform(Func &&func) && {
    return expected_transform_impl(std::move(*this), std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto transform(Func &&func) const & {
    return expected_transform_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto transform(Func &&func) const && {
    return expected_transform_impl(std::move(*this), std::forward<Func>(func));
  }

 private:
  template <class U>
  void assign_val(U &&u) {
    if (has_value_) {
      val_ = std::forward<U>(u);
    } else {
      impl::reinit_expected(std::addressof(val_), std::addressof(unex_),
                            std::forward<U>(u));
      has_value_ = true;
    }
  }

  template <class U>
  void assign_unex(U &&u) {
    if (has_value_) {
      impl::reinit_expected(std::addressof(unex_), std::addressof(val_),
                            std::forward<U>(u));
      has_value_ = false;
    } else {
      unex_ = std::forward<U>(u);
    }
  }

  union {
    T val_;
    E unex_;
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 12
    // workaround GCC 10/11's "maybe-uninitialized"
    // GCC 12 and 13 do not report it.
    volatile char gcc_pr80635_workaround_maybe_uninitialized_;
#endif
  };

  bool has_value_;
};

template <class T, class E>
  requires(std::is_void_v<T>)
class expected<T, E> {
 public:
  static_assert(!std::is_void_v<E>, "E must not be void");
  static_assert(!std::is_reference_v<T>, "T must not be a reference");
  static_assert(!std::is_same_v<T, std::remove_cv<std::in_place_t>>,
                "T must not be std::in_place_t");
  static_assert(!std::is_same_v<T, std::remove_cv<unexpected<E>>>,
                "T must not be unexpected<E>");
  static_assert(!std::is_reference_v<E>, "E must not be a reference");

  using value_type = T;
  using error_type = E;
  using unexpected_type = unexpected<E>;

  // (1)
  constexpr expected() noexcept : dummy_(), has_value_(true) {}

  // (2)
  constexpr expected(const expected &other) = default;

  constexpr expected(const expected &other)
    requires((std::is_copy_constructible_v<E> &&
              !std::is_trivially_copy_constructible_v<E>))
      : dummy_(), has_value_{other.has_value()} {
    if (!has_value()) {
      std::construct_at(std::addressof(unex_), other.unex_);
    }
  }

  // (3)
  constexpr expected(expected &&other) = default;

  constexpr expected(expected &&other)  //
      noexcept((std::is_nothrow_move_constructible_v<T>))
    requires((std::is_move_constructible_v<E> &&
              !std::is_trivially_move_constructible_v<E>))
      : dummy_(), has_value_{other.has_value()} {
    if (!has_value()) {
      std::construct_at(std::addressof(unex_), std::move(other.unex_));
    }
  }

  template <class UF, class GF>
  static constexpr bool constructor_is_explicit =
      (!std::is_convertible_v<UF, T> || !std::is_convertible_v<GF, E>);

  template <class U, class G, class UF, class GF>
  static constexpr bool can_value_convert_construct =
      (std::is_void_v<U> && std::is_constructible_v<E, GF> &&
       !std::is_constructible_v<unexpected<E>, expected<U, G> &> &&
       !std::is_constructible_v<unexpected<E>, expected<U, G>> &&
       !std::is_constructible_v<unexpected<E>, const expected<U, G> &> &&
       !std::is_constructible_v<unexpected<E>, const expected<U, G>>);

  // (4)
  template <class U, class G, class UF = std::add_lvalue_reference_t<const U>,
            class GF = const G &>
  constexpr explicit(constructor_is_explicit<UF, GF>)
      expected(const expected<U, G> &rhs)
    requires can_value_convert_construct<U, G, UF, GF>
      : dummy_(), has_value_{rhs.has_value()} {
    if (!rhs.has_value()) {
      std::construct_at(std::addressof(unex_), rhs.error());
    }
  }

  // (5)
  template <class U, class G, class UF = U, class GF = G>
  constexpr explicit(constructor_is_explicit<UF, GF>)  //
      expected(expected<U, G> &&rhs)                   //
    requires can_value_convert_construct<U, G, UF, GF>
      : dummy_(), has_value_{rhs.has_value()} {
    if (!rhs.has_value()) {
      std::construct_at(std::addressof(unex_), std::move(rhs.error()));
    }
  }

  // (6) requires T is not cv void.

  // (7)
  template <class G>
  constexpr explicit(!std::is_convertible_v<const G &, E>)
      expected(const unexpected<G> &err)
    requires std::is_constructible_v<E, const G &>
      : unex_(err.error()), has_value_{false} {}

  // (8)
  template <class G>
  constexpr explicit(!std::is_convertible_v<G, E>)  //
      expected(unexpected<G> &&err)
    requires std::is_constructible_v<E, G>
      : unex_(std::move(err.error())), has_value_{false} {}

  // (9) requires T is not cv void.

  // (10) requires T is not cv void.

  // (11)
  template <class... Args>
  constexpr explicit expected(std::in_place_t) : has_value_{true} {}

  // (12)
  template <class... Args>
  constexpr explicit expected(stdx::unexpect_t, Args &&...args)
    requires std::is_constructible_v<E, Args...>
      : unex_(std::forward<Args>(args)...), has_value_{false} {}

  // (13)
  template <class U, class... Args>
  constexpr explicit expected(stdx::unexpect_t, std::initializer_list<U> il,
                              Args &&...args)  //
    requires std::is_constructible_v<E, std::initializer_list<U> &, Args...>
      : unex_(il, std::forward<Args>(args)...), has_value_{false} {}

  // assignment

  // (1)
  constexpr expected &operator=(expected const &other)
    requires((std::is_copy_assignable_v<E> &&  //
              std::is_copy_constructible_v<E>))
  {
    if (other.has_value()) {
      emplace();  // destroys unex_ and sets has_value_ = true
    } else {
      assign_unex(other.unex_);
    }

    return *this;
  }

  // (2)
  constexpr expected &operator=(expected &&other)
    requires((std::is_move_assignable_v<E> &&  //
              std::is_move_constructible_v<E>))
  {
    if (other.has_value()) {
      emplace();
    } else {
      assign_unex(other.unex_);
    }
    return *this;
  }

  // (3) requires T is not cv void.

  // (4)
  template <class G, class GF = const G &>
  constexpr expected &operator=(const unexpected<G> &other)
    requires((std::is_constructible_v<E, GF> &&  //
              std::is_assignable_v<E &, GF>))
  {
    assign_unex(other.error());

    return *this;
  }

  // (5)
  template <class G, class GF = G>
  constexpr expected &operator=(unexpected<G> &&other)
    requires(std::is_constructible_v<E, GF> &&  //
             std::is_assignable_v<E &, GF>)
  {
    assign_unex(std::move(other.error()));

    return *this;
  }

  // destruct
#if defined(CXX_HAS_CONDITIONAL_TRIVIAL_DESTRUCTOR)
  constexpr ~expected()
    requires((std::is_trivially_destructible_v<E>))
  = default;
#endif

  constexpr ~expected() {
    if (!has_value()) {
      std::destroy_at(std::addressof(unex_));
    }
  }

  // emplace
  constexpr void emplace() noexcept {
    if (!has_value()) {
      std::destroy_at(std::addressof(unex_));
      has_value_ = true;
    }
  }

  // swap
  constexpr void swap(expected &other)                      //
      noexcept((std::is_nothrow_move_constructible_v<E> &&  //
                std::is_nothrow_swappable_v<E>))
    requires((std::is_swappable_v<E> &&          //
              std::is_move_constructible_v<E>))  //
  {
    using std::swap;

    if (bool(*this) && bool(other)) {
      // is void
    } else if (!bool(*this) && !bool(other)) {
      swap(unex_, other.unex_);
    } else if (bool(*this) && !bool(other)) {
      std::construct_at(std::addressof(unex_), std::move(other.unex_));
      std::destroy_at(std::addressof(other.unex_));

      swap(has_value_, other.has_value_);
    } else if (!bool(*this) && bool(other)) {
      other.swap(*this);
    }
  }

  constexpr bool has_value() const { return has_value_; }
  constexpr explicit operator bool() const noexcept { return has_value(); }

  // value accessors

  constexpr void value() const & {
    static_assert(std::is_copy_constructible_v<E>);

    if (!has_value()) {
      throw bad_expected_access(std::as_const(error()));
    }
  }

  constexpr void value() && {
    static_assert(std::is_move_constructible_v<E>);

    if (!has_value()) {
      throw bad_expected_access(std::as_const(error()));
    }
  }

  // unchecked value access

  // (3)
  constexpr void operator*() const noexcept { assert(has_value()); }

  template <class U>
  constexpr value_type value_or(U &&v) const & {
    static_assert(
        std::is_copy_constructible_v<T> && std::is_convertible_v<U &&, T>,
        "T must be copy-constructible and convertible from U&&");

    return has_value() ? **this : static_cast<T>(std::forward<U>(v));
  }

  template <class U>
  constexpr value_type value_or(U &&v) && {
    static_assert(
        std::is_move_constructible_v<T> && std::is_convertible_v<U &&, T>,
        "T must be move-constructible and convertible from U&&");

    return has_value() ? std::move(**this) : static_cast<T>(std::forward<U>(v));
  }

  // error accessors
  constexpr const error_type &error() const & { return unex_; }
  constexpr const error_type &&error() const && { return std::move(unex_); }
  constexpr error_type &error() & { return unex_; }
  constexpr error_type &&error() && { return std::move(unex_); }

  // operator==

  // (1) requires T is not cv void

  // (2)
  template <class T2, class E2>
  friend constexpr bool operator==(const expected &lhs,
                                   const stdx::expected<T2, E2> &rhs)
    requires(std::is_void_v<T2>)
  {
    if (lhs.has_value() != rhs.has_value()) return false;

    if (lhs.has_value()) return true;

    return lhs.error() == rhs.error();
  }

  // (3) requires T is not cv void

  // (4)
  template <class E2>
  friend constexpr bool operator==(const expected &lhs,
                                   const unexpected<E2> &rhs) {
    if (lhs.has_value()) return false;

    return static_cast<bool>(lhs.error() == rhs.error());
  }
  //
  // and_then
  //

  template <class Func>
  constexpr auto and_then(Func &&func) & {
    return base::and_then_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto and_then(Func &&func) && {
    return base::and_then_impl(std::move(*this), std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto and_then(Func &&func) const & {
    return base::and_then_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto and_then(Func &&func) const && {
    return base::and_then_impl(std::move(*this), std::forward<Func>(func));
  }

  //
  // or_else
  //

  template <class Func>
  constexpr auto or_else(Func &&func) & {
    return base::or_else_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto or_else(Func &&func) && {
    return base::or_else_impl(std::move(*this), std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto or_else(Func &&func) const & {
    return base::or_else_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto or_else(Func &&func) const && {
    return base::or_else_impl(std::move(*this), std::forward<Func>(func));
  }

  //
  // transform
  //

  template <class Func>
  constexpr auto transform(Func &&func) & {
    return expected_transform_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto transform(Func &&func) && {
    return expected_transform_impl(std::move(*this), std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto transform(Func &&func) const & {
    return expected_transform_impl(*this, std::forward<Func>(func));
  }

  template <class Func>
  constexpr auto transform(Func &&func) const && {
    return expected_transform_impl(std::move(*this), std::forward<Func>(func));
  }

 private:
  template <class U>
  void assign_unex(U &&u) {
    if (has_value_) {
      std::construct_at(std::addressof(unex_), std::forward<U>(u));
      has_value_ = false;
    } else {
      unex_ = std::forward<U>(u);
    }
  }

  union {
    struct {
    } dummy_;
    E unex_;
  };

  bool has_value_;
};

template <class Exp, class Func>
constexpr auto expected_transform_impl(Exp &&exp, Func &&func) {
  // type of the value that's passed to func
  using func_value_type = typename std::decay_t<Exp>::value_type;

  if constexpr (std::is_void_v<func_value_type>) {
    using func_return_type = std::invoke_result_t<Func>;
    using result_type =
        stdx::expected<func_return_type, typename Exp::error_type>;

    if (!exp.has_value()) {
      return result_type{stdx::unexpect, std::forward<Exp>(exp).error()};
    }

    if constexpr (std::is_void_v<func_return_type>) {
      std::invoke(func);
      return result_type();
    } else {
      return result_type(std::invoke(func));
    }
  } else {
    using func_return_type = std::invoke_result_t<Func, func_value_type>;
    using result_type =
        stdx::expected<func_return_type, typename Exp::error_type>;

    if (!exp.has_value()) {
      return result_type{stdx::unexpect, std::forward<Exp>(exp).error()};
    }

    if constexpr (std::is_void_v<func_return_type>) {
      std::invoke(func, *std::forward<Exp>(exp));
      return result_type();
    } else {
      return result_type(std::invoke(func, *std::forward<Exp>(exp)));
    }
  }
}

}  // namespace stdx

#endif
