// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CHECK_NON_FINITE_HPP
#define BOOST_DECIMAL_DETAIL_CHECK_NON_FINITE_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/type_traits.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#endif

namespace boost {
namespace decimal {
namespace detail {

// Prioritizes checking for nans and then checks for infs
// Per IEEE 754 section 7.2 any operation on a sNaN returns qNaN
template <typename Decimal>
BOOST_DECIMAL_CUDA_CONSTEXPR Decimal check_non_finite(Decimal lhs, Decimal rhs) noexcept
{
    static_assert(is_decimal_floating_point_v<Decimal>, "Types must both be decimal types");

    if (isnan(lhs))
    {
        // 3 Cases:
        // 1) LHS is QNAN and RHS is SNAN -> Return RHS payload as QNAN
        // 2) LHS is SNAN and RHS is QNAN -> Return LHS payload as QNAN
        // 3) LHS is NAN and RHS is NAN -> Return LHS payload as QNAN

        const bool lhs_signaling {issignaling(lhs)};
        const bool rhs_signaling {issignaling(rhs)};

        if (!lhs_signaling && rhs_signaling)
        {
            return nan_conversion(rhs);
        }

        return lhs_signaling ? nan_conversion(lhs) : lhs;

    }
    else if (isnan(rhs))
    {
        return issignaling(rhs) ? nan_conversion(rhs) : rhs;
    }

    if (isinf(lhs))
    {
        return lhs;
    }
    else
    {
        BOOST_DECIMAL_ASSERT(isinf(rhs));
        return rhs;
    }
}

template <typename Decimal>
BOOST_DECIMAL_CUDA_CONSTEXPR Decimal check_non_finite(Decimal x) noexcept
{
    static_assert(is_decimal_floating_point_v<Decimal>, "Types must be a decimal type");

    if (isnan(x))
    {
        return issignaling(x) ? nan_conversion(x) : x;
    }

    BOOST_DECIMAL_ASSERT(isinf(x));
    return x;
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CHECK_NON_FINITE_HPP
