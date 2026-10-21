// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_MODF_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_MODF_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/cmath/floor.hpp>
#include <boost/decimal/detail/cmath/ceil.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <cmath>
#endif

namespace boost {
namespace decimal {

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto modf(const T x, T* iptr) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    constexpr T zero {0, 0};
    const auto is_neg {x < zero};

    if (abs(x) == zero || isinf(x))
    {
        *iptr = x;
        return is_neg ? -zero : zero;
    }
    #ifndef BOOST_DECIMAL_FAST_MATH
    else if (isnan(x))
    {
        *iptr = x;
        return x;
    }
    #endif

    *iptr = (x > zero) ? floor(x) : ceil(x);
    return (x - *iptr);
}

} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_MODF_HPP
