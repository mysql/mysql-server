// Copyright 2023 Matt Borland
// Copyright 2023 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_ATANH_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_ATANH_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <limits>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <typename T>
constexpr auto atanh_impl(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    T result { };

    if (fpclassify(x) != FP_NORMAL)
    {
        result = x;
    }
    else
    {
        constexpr T zero { 0, 0 };

        const auto b_neg = x < zero;
        const auto xx = abs(x);

        constexpr T one  { 1, 0 };

        if (xx > one)
        {
            #ifndef BOOST_DECIMAL_FAST_MATH
            result = std::numeric_limits<T>::quiet_NaN();
            #else
            result = zero;
            #endif
        }
        else if (xx < one)
        {
            // Use (parts of) the implementation of atanh from Boost.Math.

            constexpr T fourth_root_epsilon { 1, -((std::numeric_limits<T>::digits10 + 1) / 4) };

            if (xx >= fourth_root_epsilon)
            {
                constexpr T half { 5, -1 };

                // http://functions.wolfram.com/ElementaryFunctions/ArcTanh/02/

                if(xx < half)
                {
                    result = (::boost::decimal::log1p(xx) - ::boost::decimal::log1p(-xx)) / 2;
                }
                else
                {
                    result = (::boost::decimal::log((one + xx) / (one - xx)) / 2);
                }
            }
            else
            {
                // http://functions.wolfram.com/ElementaryFunctions/ArcTanh/06/01/03/01/
                // approximation by taylor series in x at 0 up to order 2
                result = xx;

                constexpr T root_epsilon { 1, -((std::numeric_limits<T>::digits10 + 1) / 2) };

                if (xx >= root_epsilon)
                {
                    const T x3 = (xx * xx) * xx;

                    // approximation by taylor series in x at 0 up to order 4
                    result += x3 / T { 3, 0 };
                }
            }

            if (b_neg) { result = -result; }
        }
        else
        {
            #ifndef BOOST_DECIMAL_FAST_MATH
            result = ((!b_neg) ? std::numeric_limits<T>::infinity() : -std::numeric_limits<T>::infinity());
            #else
            result = zero;
            #endif
        }
    }

    return result;
}

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto atanh(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::atanh_impl(static_cast<evaluation_type>(x)));
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_ATANH_HPP
