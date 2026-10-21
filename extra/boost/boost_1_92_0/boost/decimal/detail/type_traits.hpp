// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_TYPE_TRAITS
#define BOOST_DECIMAL_DETAIL_TYPE_TRAITS

// Extends the current type traits to include our types and __int128s
#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/u256.hpp>
#include <boost/decimal/detail/components.hpp>
#include "int128.hpp"

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#endif

namespace boost {
namespace decimal {
namespace detail {
template <typename T>
struct is_signed { static constexpr bool value = std::is_signed<T>::value; };

template <>
struct is_signed<boost::int128::int128_t> { static constexpr bool value = true; };

template <>
struct is_signed<boost::int128::uint128_t> { static constexpr bool value = false; };

#ifdef BOOST_DECIMAL_HAS_INT128

template <>
struct is_signed<builtin_int128_t> { static constexpr bool value = true; };

template <>
struct is_signed<builtin_uint128_t> { static constexpr bool value = false;};

#endif

template <>
struct is_signed<detail::u256> { static constexpr bool value = false; };

template <typename T>
constexpr bool is_signed<T>::value;

template <typename T>
constexpr bool is_signed_v = is_signed<T>::value;

template <typename T>
constexpr bool is_unsigned_v = !is_signed_v<T>;

template <typename T>
struct make_unsigned { using type = std::make_unsigned_t<T>; };

template <>
struct make_unsigned<bool> { using type = std::uint8_t; };

template <>
struct make_unsigned<boost::int128::uint128_t> { using type = boost::int128::uint128_t; };

template <>
struct make_unsigned<boost::int128::int128_t> { using type = boost::int128::uint128_t; };

#ifdef BOOST_DECIMAL_HAS_INT128

template <>
struct make_unsigned<builtin_int128_t> { using type = builtin_uint128_t; };

template <>
struct make_unsigned<builtin_uint128_t> { using type = builtin_uint128_t; };

#endif

template <typename T>
using make_unsigned_t = typename make_unsigned<T>::type;

template <typename T>
struct make_signed { using type = std::make_signed_t<T>; };

template <>
struct make_signed<boost::int128::int128_t> { using type = boost::int128::int128_t; };

template <>
struct make_signed<boost::int128::uint128_t> { using type = boost::int128::int128_t; };

#ifdef BOOST_DECIMAL_HAS_INT128

template <>
struct make_signed<builtin_int128_t> { using type = builtin_int128_t; };

template <>
struct make_signed<builtin_uint128_t> { using type = builtin_int128_t; };

#endif

template <typename T>
using make_signed_t = typename make_signed<T>::type;

template <typename T>
struct is_integral { static constexpr bool value = std::is_integral<T>::value;};

template <>
struct is_integral<boost::int128::int128_t> { static constexpr bool value = true; };

template <>
struct is_integral<boost::int128::uint128_t> { static constexpr bool value = true; };

#ifdef BOOST_DECIMAL_HAS_INT128

template <>
struct is_integral<builtin_int128_t> { static constexpr bool value = true; };

template <>
struct is_integral<builtin_uint128_t> { static constexpr bool value = true; };

#endif

template <>
struct is_integral<detail::u256> { static constexpr bool value = true; };

template <typename T>
constexpr bool is_integral<T>::value;

template <typename T>
constexpr bool is_integral_v = is_integral<T>::value;

template <typename T>
struct is_floating_point { static constexpr bool value = std::is_floating_point<T>::value; };

#if defined(BOOST_DECIMAL_HAS_FLOAT128) && !defined(BOOST_DECIMAL_LDBL_IS_FLOAT128)
template <>
struct is_floating_point<__float128> { static constexpr bool value = true; };
#endif

template <typename T>
constexpr bool is_floating_point<T>::value;

template <typename T>
constexpr bool is_floating_point_v = is_floating_point<T>::value;

template <typename T>
struct is_decimal_floating_point { static constexpr bool value = false; };

template <>
struct is_decimal_floating_point<decimal32_t> { static constexpr bool value = true; };

template <>
struct is_decimal_floating_point<decimal64_t> { static constexpr bool value = true; };

template <>
struct is_decimal_floating_point<decimal128_t> { static constexpr bool value = true; };

template <>
struct is_decimal_floating_point<decimal_fast32_t> { static constexpr bool value = true; };

template <>
struct is_decimal_floating_point<decimal_fast64_t> { static constexpr bool value = true; };

template <>
struct is_decimal_floating_point<decimal_fast128_t> { static constexpr bool value = true; };

template <typename T>
constexpr bool is_decimal_floating_point<T>::value;

template <typename T>
constexpr bool is_decimal_floating_point_v = is_decimal_floating_point<T>::value;

template <typename T>
struct is_decimal_floating_point_components { static constexpr bool value = false; };

template <>
struct is_decimal_floating_point_components<decimal32_t_components> { static constexpr bool value = true; };

template <>
struct is_decimal_floating_point_components<decimal64_t_components> { static constexpr bool value = true; };

template <>
struct is_decimal_floating_point_components<decimal128_t_components> { static constexpr bool value = true; };

template <typename T>
constexpr bool is_decimal_floating_point_components<T>::value;

template <typename T>
constexpr bool is_decimal_floating_point_components_v =  is_decimal_floating_point_components<T>::value;

template <typename T>
struct is_ieee_type { static constexpr bool value = false; };

template <>
struct is_ieee_type<decimal32_t> { static constexpr bool value = true; };

template <>
struct is_ieee_type<decimal64_t> { static constexpr bool value = true; };

template <>
struct is_ieee_type<decimal128_t> { static constexpr bool value = true; };

template <>
struct is_ieee_type<decimal_fast32_t> { static constexpr bool value = false; };

template <>
struct is_ieee_type<decimal_fast64_t> { static constexpr bool value = false; };

template <>
struct is_ieee_type<decimal_fast128_t> { static constexpr bool value = false; };

template <typename T>
constexpr bool is_ieee_type_v = is_ieee_type<T>::value;

template <typename T>
constexpr bool is_fast_type_v = !is_ieee_type_v<T>;

template <typename...>
struct conjunction : std::true_type {};

template <typename B1>
struct conjunction<B1> : B1 {};

template <typename B1, typename... Bn>
struct conjunction<B1, Bn...>
        : std::conditional_t<static_cast<bool>(B1::value), conjunction<Bn...>, B1> {};

template <typename... B>
BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE bool conjunction_v = conjunction<B...>::value;

} // namespace detail
} // namespace decimal
} // namespace boost

#endif
