// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_FPCLASSIFY_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_FPCLASSIFY_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <cmath>
#endif

namespace boost {
namespace decimal {

BOOST_DECIMAL_EXPORT template <typename T>
BOOST_DECIMAL_CUDA_CONSTEXPR auto fpclassify BOOST_DECIMAL_PREVENT_MACRO_SUBSTITUTION (const T rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, T, int)
{
    constexpr T zero {0, 0};

    #ifdef BOOST_DECIMAL_FAST_MATH

    return rhs == zero ? FP_ZERO : FP_NORMAL;

    #else

    // Mark the normal branch as likely because even if we have a branch miss the non-finite code paths
    // do very little whereas the FP_NORMAL case always proceeds further calculations
    if (BOOST_DECIMAL_LIKELY(isnormal(rhs)))
    {
        return FP_NORMAL;
    }
    if (isinf(rhs))
    {
        return FP_INFINITE;
    }
    if (isnan(rhs))
    {
        return FP_NAN;
    }
    if (abs(rhs) == zero)
    {
        return FP_ZERO;
    }

    return FP_SUBNORMAL;

    #endif
}

} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_FPCLASSIFY_HPP
