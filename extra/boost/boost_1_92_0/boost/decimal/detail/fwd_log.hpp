// Copyright 2026 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_FWD_LOG_HPP
#define BOOST_DECIMAL_FWD_LOG_HPP

#include <boost/decimal/detail/concepts.hpp>

namespace boost {
namespace decimal {

// Forward declarationf of logarithmic functions.
// See also: https://github.com/boostorg/decimal/issues/1384

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto ilogb(const T d) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, T, int);

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto log(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T);

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto log1p(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T);

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto log10(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T);

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto log2(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T);

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_FWD_LOG_HPP
