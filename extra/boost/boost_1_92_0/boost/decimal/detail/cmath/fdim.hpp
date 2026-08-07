// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_FDIM_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_FDIM_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <limits>
#include <cmath>
#endif

namespace boost {
namespace decimal {

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto fdim(const T x, const T y) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    constexpr T zero {0, 0};

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(x) || isinf(x))
    {
        return x;
    }
    else if (isnan(y))
    {
        return y;
    }
    #endif

    if (y >= x)
    {
        return zero;
    }

    return x - y;
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_FDIM_HPP
