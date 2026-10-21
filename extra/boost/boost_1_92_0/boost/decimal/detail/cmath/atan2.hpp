// Copyright 2024 Matt Borland
// Copyright 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_ATAN2_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_ATAN2_HPP

#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/cmath/atan.hpp>
#include <boost/decimal/detail/cmath/fabs.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/numbers.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <cstdint>
#include <cmath>
#endif

namespace boost {
namespace decimal {

namespace detail {

namespace atan2_detail {

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
struct pi_constants
{
    static constexpr T pi_over_two        = numbers::pi_v<T> / 2;
    static constexpr T three_pi_over_four = 3 * numbers::pi_over_four_v<T>;
};

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T> constexpr T pi_constants<T>::pi_over_two;
template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T> constexpr T pi_constants<T>::three_pi_over_four;

} // namespace atan2_detail

template <typename T>
constexpr auto atan2_impl(const T y, const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    const auto fpcx {fpclassify(x)};
    const auto fpcy {fpclassify(y)};
    const auto signx {signbit(x)}; // True if neg
    const auto signy {signbit(y)};
    const auto isfinitex {fpcx != FP_INFINITE && fpcx != FP_NAN};
    const auto isfinitey {fpcy != FP_INFINITE && fpcy != FP_NAN};

    T result { };

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (fpcx == FP_NAN)
    {
        result = x;
    }
    else if (fpcy == FP_NAN)
    {
        result = y;
    }
    else
    #endif
    if (fpcy == FP_ZERO && signx)
    {
        result = signy ? -numbers::pi_v<T> : numbers::pi_v<T>;
    }
    else if (fpcy == FP_ZERO && !signx)
    {
        result = y;
    }
    #ifndef BOOST_DECIMAL_FAST_MATH
    else if (fpcy == FP_INFINITE && isfinitex)
    {
        result = atan2_detail::pi_constants<T>::pi_over_two;

        if (signy) { result = -result; }
    }
    else if (fpcy == FP_INFINITE && fpcx == FP_INFINITE && signx)
    {
        result = atan2_detail::pi_constants<T>::three_pi_over_four;

        if (signy)
        {
            result = -result; // LCOV_EXCL_LINE : False Negative
        }
    }
    else if (fpcy == FP_INFINITE && fpcx == FP_INFINITE && !signx)
    {
        result = numbers::pi_over_four_v<T>;

        if (signy)
        {
            result = -result; // LCOV_EXCL_LINE : False Negative
        }
    }
    #endif
    else if (fpcx == FP_ZERO)
    {
        result = atan2_detail::pi_constants<T>::pi_over_two;

        if (signy) { result = -result; }
    }
    #ifndef BOOST_DECIMAL_FAST_MATH
    else if (fpcx == FP_INFINITE && signx && isfinitey)
    {
        result = signy ? -numbers::pi_v<T> : numbers::pi_v<T>;
    }
    else if (fpcx == FP_INFINITE && !signx && isfinitey)
    {
        constexpr T zero { 0, 0 };

        result = signy ? -zero : zero;
    }
    #endif
    else
    {
        if (x == T{1, 0})
        {
            result = atan(y);
        }
        else
        {
            const auto ret_val {atan(fabs(y / x))};

            if (!signy && !signx)
            {
                result = ret_val;
            }
            else if (signy && !signx)
            {
                result = -ret_val;
            }
            else if (!signy && signx)
            {
                result = numbers::pi_v<T> - ret_val;
            }
            else
            {
                result = ret_val - numbers::pi_v<T>;
            }
        }
    }

    return result;
}

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto atan2(const T y, const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::atan2_impl(static_cast<evaluation_type>(y), static_cast<evaluation_type>(x)));
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_ATAN2_HPP
