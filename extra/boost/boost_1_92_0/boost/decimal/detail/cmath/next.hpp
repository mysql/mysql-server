//  (C) Copyright John Maddock 2008 - 2023.
//  (C) Copyright Matt Borland 2023.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DECIMAL_DETAIL_CMATH_NEXT_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_NEXT_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/promotion.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/attributes.hpp>
#include <boost/decimal/detail/cmath/modf.hpp>
#include <boost/decimal/detail/cmath/abs.hpp>
#include <boost/decimal/detail/cmath/round.hpp>
#include <boost/decimal/detail/cmath/ilogb.hpp>
#include <boost/decimal/detail/cmath/fpclassify.hpp>
#include <boost/decimal/detail/cmath/frexp10.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType>
constexpr auto nextafter_impl(const DecimalType val, const bool direction) noexcept -> DecimalType
{
    constexpr DecimalType zero {0};

    const bool is_neg {val < 0};

    if (val == zero)
    {
        const auto min_val {direction ? std::numeric_limits<DecimalType>::denorm_min() :
                                        -std::numeric_limits<DecimalType>::denorm_min()};
        return min_val;
    }

    int exp {};
    auto sig {frexp10(val, &exp)};
    const auto removed_zeros(remove_trailing_zeros(sig));

    // Our two boundaries
    const bool is_pow_10 {removed_zeros.trimmed_number == 1U};
    const bool is_max_sig {sig == detail::max_significand_v<DecimalType>};

    if (!isnormal(val))
    {
        // Denorms need separate handling
        sig = removed_zeros.trimmed_number;
        exp += static_cast<int>(removed_zeros.number_of_removed_zeros);

        if (exp == detail::etiny_v<DecimalType>)
        {
            // If we are at the absolute minimum just add to the sig
            // e.g. 2e-101 < 1.1e-100
            ++sig;
        }
        else if (removed_zeros.number_of_removed_zeros > 0)
        {
            // We need to shift an add
            // 1 -> 11 instead of 2 since 11e-101 < 2e-100 starting at 1e-100
            sig *= 10U;
            ++sig;
            --exp;
        }
        else
        {
            ++sig;
        }

        return DecimalType{sig, exp, is_neg};
    }

    if (direction)
    {
        // Val < direction = +
        ++sig;
        if (is_max_sig)
        {
            sig /= 10u;
            ++exp;
        }
    }
    else
    {
        // Val > direction = -
        --sig;
        if (is_pow_10)
        {
            // 1000 becomes 999 but needs to be 9999
            sig *= 10u;
            sig += 9u;
            --exp;
        }
    }

    return DecimalType{sig, exp, is_neg};
}

} // namespace detail

BOOST_DECIMAL_EXPORT
template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T1,
          BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T2>
constexpr auto nextafter(const T1 val, const T2 direction) noexcept
    BOOST_DECIMAL_REQUIRES_TWO(detail::is_decimal_floating_point_v, T1, detail::is_decimal_floating_point_v, T2)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(val) || isinf(val))
    {
        return val;
    }
    else if (isnan(direction) || val == direction)
    {
        return direction;
    }
    #else
    if (val == direction)
    {
        return direction;
    }
    #endif
    else
    {
        return detail::nextafter_impl(val, val < direction);
    }
}

BOOST_DECIMAL_EXPORT template <typename T>
BOOST_DECIMAL_CXX20_CONSTEXPR auto nexttoward(const T val, const long double direction) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    const auto dec_direction {static_cast<T>(direction)};
    return nextafter(val, dec_direction);
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_NEXT_HPP
