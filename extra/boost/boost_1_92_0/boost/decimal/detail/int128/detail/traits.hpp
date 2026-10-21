// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_DETAIL_TRAITS_HPP
#define BOOST_DECIMAL_DETAIL_INT128_DETAIL_TRAITS_HPP

#include <boost/decimal/detail/int128/detail/config.hpp>

#ifndef BOOST_DECIMAL_DETAIL_INT128_BUILD_MODULE

#include <type_traits>
#include <cstdint>

#endif

namespace boost {
namespace int128 {
namespace detail {

template <typename T>
struct signed_integer
{
    static constexpr bool value = (std::is_signed<T>::value && std::is_integral<T>::value)
    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_INT128
    || std::is_same<T, builtin_i128>::value;
    #else
    ;
    #endif
};

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_INLINE_CONSTEXPR bool is_signed_integer_v = signed_integer<T>::value;

template <typename T>
struct unsigned_integer
{
    static constexpr bool value = (std::is_unsigned<T>::value && std::is_integral<T>::value)
    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_INT128
    || std::is_same<T, builtin_u128>::value;
    #else
    ;
    #endif
};

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_INLINE_CONSTEXPR bool is_unsigned_integer_v = unsigned_integer<T>::value;

template <typename T>
BOOST_DECIMAL_DETAIL_INT128_INLINE_CONSTEXPR bool is_any_integer_v = signed_integer<T>::value || unsigned_integer<T>::value;

// Decides if we can use a u32 or u64 implementation for some operations

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

template <typename T>
using evaluation_type_t = std::conditional_t<sizeof(T) <= sizeof(std::uint32_t), std::uint32_t,
                            std::conditional_t<sizeof(T) <= sizeof(std::uint64_t), std::uint64_t, builtin_u128>>;

#else

template <typename T>
using evaluation_type_t = std::conditional_t<sizeof(T) <= sizeof(std::uint32_t), std::uint32_t, std::uint64_t>;

#endif

} // namespace detail
} // namespace int128
} // namespace boost

#define BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT typename SignedInteger, std::enable_if_t<detail::is_signed_integer_v<SignedInteger>, bool> = true
#define BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT typename UnsignedInteger, std::enable_if_t<detail::is_unsigned_integer_v<UnsignedInteger>, bool> = true
#define BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT typename Integer, std::enable_if_t<detail::is_any_integer_v<Integer>, bool> = true

#define BOOST_DECIMAL_DETAIL_INT128_SIGNED_INTEGER_CONCEPT typename SignedInteger, std::enable_if_t<detail::is_signed_integer_v<SignedInteger>, bool>
#define BOOST_DECIMAL_DETAIL_INT128_UNSIGNED_INTEGER_CONCEPT typename UnsignedInteger, std::enable_if_t<detail::is_unsigned_integer_v<UnsignedInteger>, bool>
#define BOOST_DECIMAL_DETAIL_INT128_INTEGER_CONCEPT typename Integer, std::enable_if_t<detail::is_any_integer_v<Integer>, bool>

#if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) || defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)

#define BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_128BIT_INTEGER_CONCEPT typename SignedInteger, std::enable_if_t<std::is_same<SignedInteger, detail::builtin_i128>::value, bool> = true
#define BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_128BIT_INTEGER_CONCEPT typename UnsignedInteger, std::enable_if_t<std::is_same<UnsignedInteger, detail::builtin_u128>::value, bool> = true
#define BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_128BIT_INTEGER_CONCEPT typename Integer, std::enable_if_t<std::is_same<Integer, detail::builtin_u128>::value || std::is_same<Integer, detail::builtin_i128>::value, bool> = true

#define BOOST_DECIMAL_DETAIL_INT128_SIGNED_128BIT_INTEGER_CONCEPT typename SignedInteger, std::enable_if_t<std::is_same<SignedInteger, detail::builtin_i128>::value, bool>
#define BOOST_DECIMAL_DETAIL_INT128_UNSIGNED_128BIT_INTEGER_CONCEPT typename UnsignedInteger, std::enable_if_t<std::is_same<UnsignedInteger, detail::builtin_u128>::value, bool>
#define BOOST_DECIMAL_DETAIL_INT128_128BIT_INTEGER_CONCEPT typename Integer, std::enable_if_t<std::is_same<Integer, detail::builtin_u128>::value || std::is_same<Integer, detail::builtin_i128>::value, bool>

#endif

#endif // BOOST_DECIMAL_DETAIL_INT128_DETAIL_TRAITS_HPP
