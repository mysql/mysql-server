// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_LITERALS_HPP
#define BOOST_DECIMAL_LITERALS_HPP

#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/decimal_fast32_t.hpp>
#include <boost/decimal/decimal_fast64_t.hpp>
#include <boost/decimal/decimal_fast128_t.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/utilities.hpp>
#include <boost/decimal/charconv.hpp>

#if !defined(BOOST_DECIMAL_DISABLE_CLIB)

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <cstring>
#endif

namespace boost {
namespace decimal {
namespace literals {

BOOST_DECIMAL_EXPORT constexpr auto operator ""_DF(const char *str) -> decimal32_t
{
    decimal32_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_df(const char *str) -> decimal32_t
{
    decimal32_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_DF(const char *str, const std::size_t len) -> decimal32_t
{
    decimal32_t d;
    from_chars(str, str + len, d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_df(const char *str, const std::size_t len) -> decimal32_t
{
    decimal32_t d;
    from_chars(str, str + len, d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_DFF(const char *str) -> decimal_fast32_t
{
    decimal_fast32_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_dff(const char *str) -> decimal_fast32_t
{
    decimal_fast32_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_DFF(const char *str, const std::size_t len) -> decimal_fast32_t
{
    decimal_fast32_t d;
    from_chars(str, str + len, d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_dff(const char *str, const std::size_t len) -> decimal_fast32_t
{
    decimal_fast32_t d;
    from_chars(str, str + len, d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_DD(const char *str) -> decimal64_t
{
    decimal64_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_dd(const char *str) -> decimal64_t
{
    decimal64_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_DD(const char *str, std::size_t) -> decimal64_t
{
    decimal64_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_dd(const char *str, std::size_t) -> decimal64_t
{
    decimal64_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_DDF(const char *str) -> decimal_fast64_t
{
    decimal_fast64_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_ddf(const char *str) -> decimal_fast64_t
{
    decimal_fast64_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_DDF(const char *str, const std::size_t len) -> decimal_fast64_t
{
    decimal_fast64_t d;
    from_chars(str, str + len, d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_ddf(const char *str, const std::size_t len) -> decimal_fast64_t
{
    decimal_fast64_t d;
    from_chars(str, str + len, d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_DL(const char *str) -> decimal128_t
{
    decimal128_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_dl(const char *str) -> decimal128_t
{
    decimal128_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_DL(const char *str, std::size_t) -> decimal128_t
{
    decimal128_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_dl(const char *str, std::size_t) -> decimal128_t
{
    decimal128_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_DLF(const char *str) -> decimal_fast128_t
{
    decimal_fast128_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_dlf(const char *str) -> decimal_fast128_t
{
    decimal_fast128_t d;
    from_chars(str, str + detail::strlen(str), d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_DLF(const char *str, const std::size_t len) -> decimal_fast128_t
{
    decimal_fast128_t d;
    from_chars(str, str + len, d);
    return d;
}

BOOST_DECIMAL_EXPORT constexpr auto operator ""_dlf(const char *str, const std::size_t len) -> decimal_fast128_t
{
    decimal_fast128_t d;
    from_chars(str, str + len, d);
    return d;
}

} // namespace literals
} // namespace decimal
} // namespace boost

#endif

#endif // BOOST_DECIMAL_LITERALS_HPP
