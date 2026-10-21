// Copyright 2022 Peter Dimov
// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_LITERALS_HPP
#define BOOST_DECIMAL_DETAIL_INT128_LITERALS_HPP

#include <boost/decimal/detail/int128/int128.hpp>
#include <boost/decimal/detail/int128/detail/mini_from_chars.hpp>
#include <boost/decimal/detail/int128/detail/utilities.hpp>
#include <boost/decimal/detail/int128/detail/config.hpp>

namespace boost {
namespace int128 {
namespace literals {

BOOST_DECIMAL_DETAIL_INT128_EXPORT BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr uint128_t operator ""_u128(const char* str) noexcept
{
    uint128_t result {};
    detail::from_chars(str, str + detail::strlen(str), result);
    return result;
}

BOOST_DECIMAL_DETAIL_INT128_EXPORT BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr uint128_t operator ""_U128(const char* str) noexcept
{
    uint128_t result {};
    detail::from_chars(str, str + detail::strlen(str), result);
    return result;
}

BOOST_DECIMAL_DETAIL_INT128_EXPORT BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr uint128_t operator ""_u128(const char* str, std::size_t len) noexcept
{
    uint128_t result {};
    detail::from_chars(str, str + len, result);
    return result;
}

BOOST_DECIMAL_DETAIL_INT128_EXPORT BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr uint128_t operator ""_U128(const char* str, std::size_t len) noexcept
{
    uint128_t result {};
    detail::from_chars(str, str + len, result);
    return result;
}

BOOST_DECIMAL_DETAIL_INT128_EXPORT BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr uint128_t operator ""_u128(unsigned long long v) noexcept
{
    return uint128_t{v};
}

BOOST_DECIMAL_DETAIL_INT128_EXPORT BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr uint128_t operator ""_U128(unsigned long long v) noexcept
{
    return uint128_t{v};
}

BOOST_DECIMAL_DETAIL_INT128_EXPORT BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int128_t operator ""_i128(const char* str) noexcept
{
    int128_t result {};
    detail::from_chars(str, str + detail::strlen(str), result);
    return result;
}

BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int128_t operator ""_I128(const char* str) noexcept
{
    int128_t result {};
    detail::from_chars(str, str + detail::strlen(str), result);
    return result;
}

BOOST_DECIMAL_DETAIL_INT128_EXPORT BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int128_t operator ""_i128(unsigned long long v) noexcept
{
    return int128_t{v};
}

BOOST_DECIMAL_DETAIL_INT128_EXPORT BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int128_t operator ""_I128(unsigned long long v) noexcept
{
    return int128_t{v};
}

BOOST_DECIMAL_DETAIL_INT128_EXPORT BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int128_t operator ""_i128(const char* str, std::size_t len) noexcept
{
    int128_t result {};
    detail::from_chars(str, str + len, result);
    return result;
}

BOOST_DECIMAL_DETAIL_INT128_EXPORT BOOST_DECIMAL_DETAIL_INT128_HOST_DEVICE constexpr int128_t operator ""_I128(const char* str, std::size_t len) noexcept
{
    int128_t result {};
    detail::from_chars(str, str + len, result);
    return result;
}

} // namespace literals
} // namespace int128
} // namespace boost

#define BOOST_DECIMAL_DETAIL_INT128_STRINGIFY(x) #x
#define BOOST_DECIMAL_DETAIL_INT128_UINT128_C(x) boost::int128::literals::operator""_u128(BOOST_DECIMAL_DETAIL_INT128_STRINGIFY(x))
#define BOOST_DECIMAL_DETAIL_INT128_INT128_C(x) boost::int128::literals::operator""_i128(BOOST_DECIMAL_DETAIL_INT128_STRINGIFY(x))

#endif // BOOST_DECIMAL_DETAIL_INT128_LITERALS_HPP
