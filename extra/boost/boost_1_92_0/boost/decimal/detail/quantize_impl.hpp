// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_QUANTIZE_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_QUANTIZE_IMPL_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/attributes.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/fenv_rounding.hpp>
#include <boost/decimal/detail/integer_search_trees.hpp>
#include <boost/decimal/detail/power_tables.hpp>
#include <boost/decimal/detail/cmath/isfinite.hpp>
#include <boost/decimal/detail/cmath/fpclassify.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>
#endif

namespace boost {
namespace decimal {
namespace detail {

// Implements the IEEE 754-2008 5.3.2 quantize operation by rescaling
// the significand of x to match the quantum exponent of y.
template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, typename Significand>
BOOST_DECIMAL_CUDA_CONSTEXPR auto quantize_rescale(Significand& sig, const int delta, const bool sign) noexcept -> bool
{
    if (delta == 0 || sig == Significand{0})
    {
        return true;
    }

    if (delta > 0)
    {
        const auto sig_digits {num_digits(sig)};
        if (sig_digits + delta > precision_v<DecimalType>)
        {
            return false;
        }
        sig *= pow10(static_cast<Significand>(delta));
        return true;
    }

    const auto neg_delta {-delta};
    const auto sig_digits {num_digits(sig)};

    if (neg_delta > sig_digits)
    {
        const bool sticky {sig != Significand{0}};
        sig = Significand{0};
        fenv_round<DecimalType>(sig, sign, sticky);
    }
    else if (neg_delta > 1)
    {
        const auto pre_div {pow10(static_cast<Significand>(neg_delta - 1))};
        const auto remainder {sig % pre_div};
        sig /= pre_div;
        const bool sticky {remainder != Significand{0}};
        fenv_round<DecimalType>(sig, sign, sticky);
    }
    else
    {
        fenv_round<DecimalType>(sig, sign, false);
    }

    return true;
}

} // namespace detail

// IEEE 754-2008 3.6.6
// Returns: a number that is equal in value (except for any rounding) and sign to lhs,
// and which has an exponent set to be equal to the exponent of rhs.
// If the exponent is being increased, the value is correctly rounded according to the current rounding mode;
// if the result does not have the same value as lhs, the "inexact" floating-point exception is raised.
// If the exponent is being decreased and the significand of the result has more digits than the type would allow,
// the "invalid" floating-point exception is raised and the result is NaN.
// If one or both operands are NaN, the result is NaN.
// Otherwise, if only one operand is infinity, the "invalid" floating-point exception is raised and the result is NaN.
// If both operands are infinity, the result is DEC_INFINITY, with the same sign as lhs.
// The quantize functions do not signal underflow.
BOOST_DECIMAL_EXPORT template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType>
BOOST_DECIMAL_CUDA_CONSTEXPR auto quantize(const DecimalType lhs, const DecimalType rhs) noexcept -> DecimalType
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    // Return the correct type of nan
    if (isnan(lhs) || isnan(rhs))
    {
        if (issignaling(lhs))
        {
            return nan_conversion(lhs);
        }
        else if (issignaling(rhs))
        {
            return nan_conversion(rhs);
        }

        return isnan(lhs) ? lhs : rhs;
    }

    // If exactly one operand is infinity then return a NaN
    if (isinf(lhs) != isinf(rhs))
    {
        return std::numeric_limits<DecimalType>::quiet_NaN();
    }
    if (isinf(lhs) && isinf(rhs))
    {
        return lhs;
    }
    #endif

    auto components {lhs.to_components()};
    const auto rhs_exp {rhs.biased_exponent()};

    if (!detail::quantize_rescale<DecimalType>(components.sig, components.exp - rhs_exp, components.sign))
    {
        #ifndef BOOST_DECIMAL_FAST_MATH
        return std::numeric_limits<DecimalType>::quiet_NaN();
        #else
        return {components.sig, rhs_exp, components.sign};
        #endif
    }

    return {components.sig, rhs_exp, components.sign};
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_QUANTIZE_IMPL_HPP
