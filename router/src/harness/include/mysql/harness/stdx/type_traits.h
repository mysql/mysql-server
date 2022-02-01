/*
  Copyright (c) 2019, 2021, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_STDX_TYPE_TRAITS_H_
#define MYSQL_HARNESS_STDX_TYPE_TRAITS_H_

#include <type_traits>

namespace stdx {

// from http://wg21.link/P0463 (part of C++20)
enum class endian {
#ifdef _WIN32
  little = 0,
  big = 1,
  native = little
#else
  little = __ORDER_LITTLE_ENDIAN__,
  big = __ORDER_BIG_ENDIAN__,
  native = __BYTE_ORDER__,
#endif
};

// from http://wg21.link/p0504r0
#if __cplusplus >= 201703L
using in_place_t = std::in_place_t;
inline constexpr in_place_t in_place{};
#else
struct in_place_t {
  explicit in_place_t() = default;
};
static constexpr in_place_t in_place{};
#endif

// std::negation from C++17
template <class B>
struct negation : std::integral_constant<bool, !bool(B::value)> {};

// std::conjuntion from C++17
template <class...>
struct conjunction;

template <>
struct conjunction<> : std::true_type {};

template <class P1, class... Pn>
struct conjunction<P1, Pn...>
    : std::conditional_t<P1::value, conjunction<Pn...>, std::false_type> {};

// std::disjunction from C++17
template <class...>
struct disjunction : std::false_type {};

template <class P1>
struct disjunction<P1> : P1 {};

template <class P1, class... Pn>
struct disjunction<P1, Pn...>
    : std::conditional_t<P1::value, P1, disjunction<Pn...>> {};

// void_t from C++17
//
// see: https://en.cppreference.com/w/cpp/types/void_t
// see: http::/wg21.link/n3911
// seealso: http::/wg21.link/n4436

#if defined(__GNUC__) && __GNUC__ < 5
// GCC 4.x needs this verbose form, GCC 5.0 has the fix applied
template <typename... Ts>
struct make_void {
  using type = void;
};
template <typename... Ts>
using void_t = typename make_void<Ts...>::type;
#else
template <class...>
using void_t = void;
#endif

}  // namespace stdx

#endif
