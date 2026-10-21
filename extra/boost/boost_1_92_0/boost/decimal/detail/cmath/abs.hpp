// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_ABS_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_ABS_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <cmath>
#endif

namespace boost {
namespace decimal {

BOOST_DECIMAL_EXPORT template <typename T>
BOOST_DECIMAL_CUDA_CONSTEXPR auto abs BOOST_DECIMAL_PREVENT_MACRO_SUBSTITUTION (const T rhs) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    if (BOOST_DECIMAL_LIKELY(!isnan(rhs)))
    {
        return signbit(rhs) ? -rhs : rhs;
    }
    else
    {
        return issignaling(rhs) ? nan_conversion(rhs) : rhs;
    }
}

} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_ABS_HPP
