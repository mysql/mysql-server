// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_TOTAL_ORDER_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_TOTAL_ORDER_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/promotion.hpp>
#include <boost/decimal/detail/cmath/nan.hpp>

namespace boost {
namespace decimal {

BOOST_DECIMAL_EXPORT constexpr auto quantexp(decimal32_t x) noexcept -> int;
BOOST_DECIMAL_EXPORT constexpr auto quantexp(decimal_fast32_t x) noexcept -> int;
BOOST_DECIMAL_EXPORT constexpr auto quantexp(decimal64_t x) noexcept -> int;
BOOST_DECIMAL_EXPORT constexpr auto quantexp(decimal_fast64_t x) noexcept -> int;
BOOST_DECIMAL_EXPORT constexpr auto quantexp(decimal128_t x) noexcept -> int;
BOOST_DECIMAL_EXPORT constexpr auto quantexp(decimal_fast128_t x) noexcept -> int;

namespace detail {

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4127) // Conditional expression is constant
#endif

template <typename T>
constexpr auto total_ordering_impl(const T x, const T y) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, T, bool)
{
    // Part d: Check for unordered values
    const auto x_nan {isnan(x)};
    const auto x_neg {signbit(x)};
    const auto y_nan {isnan(y)};
    const auto y_neg {signbit(y)};

    if (x_nan && x_neg && !y_nan)
    {
        // d.1
        return true;
    }
    if (!x_nan && y_nan && !y_neg)
    {
        // d.2
        return true;
    }
    if (x_nan && y_nan)
    {
        // d.3.i
        if (x_neg && !y_neg)
        {
            return true;
        }
        // d.3.ii
        const auto x_signaling {issignaling(x)};
        const auto y_signaling {issignaling(y)};
        if (x_signaling || y_signaling)
        {
            if (x_signaling && !y_signaling)
            {
                return !x_neg;
            }
            else if (!x_signaling && y_signaling)
            {
                return y_neg;
            }
        }
        // d.3.iii
        const auto x_payload {read_payload(x)};
        const auto y_payload {read_payload(y)};

        if (x_payload != y_payload)
        {
            if (!x_neg && !y_neg)
            {
                return x_payload < y_payload;
            }
            else if (x_neg && y_neg)
            {
                return x_payload > y_payload;
            }
            else if (x_neg && !y_neg)
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        return false;
    }

    if (x < y)
    {
        // part a
        return true;
    }
    else if (x > y)
    {
        // part b
        return false;
    }
    else
    {
        if (x == 0 && y == 0)
        {
            if (x_neg && !y_neg)
            {
                // c.1
                return true;
            }
            else if (!x_neg && y_neg)
            {
                // c.2
                return false;
            }
        }

        BOOST_DECIMAL_IF_CONSTEXPR (detail::is_ieee_type_v<T>)
        {
            if (x_neg && y_neg)
            {
                // c.3.i
                return quantexp(x) >= quantexp(y);
            }
            else
            {
                // c.3.ii
                return quantexp(x) <= quantexp(y);
            }
        }
        else
        {
            // Since things are normalized this will always be true
            return true;
        }
    }
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename T1, typename T2>
constexpr auto comparetotal(const T1 lhs, const T2 rhs) noexcept
    BOOST_DECIMAL_REQUIRES_TWO_RETURN(detail::is_decimal_floating_point_v, T1, detail::is_decimal_floating_point_v, T2, bool)
{
    using larger_type = std::conditional_t<detail::decimal_val_v<T1> >= detail::decimal_val_v<T2>, T1, T2>;
    return detail::total_ordering_impl(static_cast<larger_type>(lhs), static_cast<larger_type>(rhs));
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_TOTAL_ORDER_HPP
