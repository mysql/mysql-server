/*****************************************************************************
Copyright (c) 2023, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ut0expected.h
 Minimal implementation of C++23 std::expected.

 https://en.cppreference.com/w/cpp/utility/expected

 ****************************************************************************/

#ifndef ut0expected_h
#define ut0expected_h

#include <type_traits>
#include <utility>
#include <variant>

#include "db0err.h"  // dberr_t
#include "ut0dbg.h"

namespace ut {

/** C++23 std::unexpected. */
template <class E>
class Unexpected {
 public:
  template <class Err = E>
  constexpr explicit Unexpected(Err &&e) : m_error(std::forward<Err>(e)) {}

  constexpr const E &error() const &noexcept { return m_error; }

  constexpr E &error() &noexcept { return m_error; }

  constexpr const E &&error() const &&noexcept { return std::move(m_error); }

  constexpr E &&error() &&noexcept { return std::move(m_error); }

 private:
  E m_error;
};

template <class E>
Unexpected(E) -> Unexpected<E>;

/* Replace with std::expected once it is available in all supported compilers -
the support for C++23 is not enough for some older gcc and clang that we still
support. */

/** C++23 std::expected. */
template <class T, class E = dberr_t>
class Expected : public std::variant<T, E> {
 public:
  using value_type = T;
  using error_type = E;

 public:
  constexpr Expected(const Expected &other) = default;

  constexpr Expected(Expected &&other) noexcept
      : std::variant<T, E>(std::move(other)) {}

  template <class U, std::enable_if_t<
                         !std::is_same_v<std::decay_t<U>, Expected>, int> = 0>
  constexpr Expected(U &&u) noexcept
      : std::variant<T, E>(std::in_place_index<0>, std::forward<U>(u)) {}

  template <class U>
  constexpr Expected(Unexpected<U> &&u)
      : std::variant<T, E>(std::in_place_index<1>, std::move(u).error()) {}

  constexpr Expected &operator=(const Expected &) = default;
  constexpr Expected &operator=(Expected &&) = default;

  constexpr const T *operator->() const noexcept {
    ut_a(has_value());
    return &std::get<0>(*this);
  }

  constexpr T *operator->() noexcept {
    ut_a(has_value());
    return &std::get<0>(*this);
  }

  constexpr const T &operator*() const &noexcept {
    ut_a(has_value());
    return std::get<0>(*this);
  }

  constexpr T &operator*() &noexcept {
    ut_a(has_value());
    return std::get<0>(*this);
  }

  constexpr const T &&operator*() const &&noexcept {
    ut_a(has_value());
    return std::move(std::get<0>(*this));
  }

  constexpr T &&operator*() &&noexcept {
    ut_a(has_value());
    return std::move(std::get<0>(*this));
  }

  constexpr explicit operator bool() const noexcept { return has_value(); }

  constexpr bool has_value() const noexcept { return this->index() == 0; }

  constexpr const T &value() const & {
    ut_a(has_value());
    return std::get<0>(*this);
  }

  constexpr T &value() & {
    ut_a(has_value());
    return std::get<0>(*this);
  }

  constexpr const T &&value() const && {
    ut_a(has_value());
    return std::move(std::get<0>(*this));
  }

  constexpr T &&value() && {
    ut_a(has_value());
    return std::move(std::get<0>(*this));
  }

  constexpr const E &error() const &noexcept {
    ut_a(!has_value());
    return std::get<1>(*this);
  }

  constexpr E &error() &noexcept {
    ut_a(!has_value());
    return std::get<1>(*this);
  }

  constexpr const E &&error() const &&noexcept {
    ut_a(!has_value());
    return std::move(std::get<1>(*this));
  }

  constexpr E &&error() &&noexcept {
    ut_a(!has_value());
    return std::move(std::get<1>(*this));
  }
};

} /* namespace ut */

#endif /* !ut0expected_h */
