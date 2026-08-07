// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_ROUND_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_ROUND_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/cmath/modf.hpp>
#include <boost/decimal/detail/cmath/abs.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <limits>
#include <cstdint>
#endif

namespace boost {
namespace decimal {

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto round(const T num) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    constexpr T zero {0, 0};
    constexpr T half {5, -1};

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(num) || isinf(num) || abs(num) == zero)
    {
        return num;
    }
    #else
    if (abs(num) == zero)
    {
        return num;
    }
    #endif

    T iptr {};
    const auto x {modf(num, &iptr)};

    if (x >= half)
    {
        ++iptr;
    }
    else if (x <= -half)
    {
        --iptr;
    }

    return iptr;
}

namespace detail {

// MSVC 14.1 warns of unary minus being applied to unsigned type from numeric_limits::min
// 14.2 and on get it right
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4146)
#endif

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T, BOOST_DECIMAL_INTEGRAL Int>
constexpr auto int_round_impl(const T num) noexcept -> Int
{
    constexpr T zero {0, 0};
    constexpr T lmax {(std::numeric_limits<Int>::max)()};
    constexpr T lmin {(std::numeric_limits<Int>::min)()};

    const auto rounded_val {round(num)};

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isinf(num) || isnan(num))
    {
        return std::numeric_limits<Int>::min();
    }
    else if (abs(num) == zero)
    {
        return 0;
    }
    #else
    if (abs(num) == zero)
    {
        return 0;
    }
    #endif

    if (rounded_val > lmax)
    {
        return (std::numeric_limits<Int>::max)();
    }
    else if (rounded_val < lmin)
    {
        return (std::numeric_limits<Int>::min)();
    }

    return static_cast<Int>(rounded_val);
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

} //namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto lround(const T num) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, T, long)
{
    return detail::int_round_impl<T, long>(num);
}

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto llround(const T num) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, T, long long)
{
    return detail::int_round_impl<T, long long>(num);
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_ROUND_HPP
