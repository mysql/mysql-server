// Copyright 2023 Matt Borland
// Copyright 2023 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_ASINH_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_ASINH_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/numbers.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <array>
#include <type_traits>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <typename T>
constexpr auto asinh_impl(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    T result { };

    if (fpclassify(x) != FP_NORMAL)
    {
        result = x;
    }
    else
    {
        // Use (parts of) the implementation of asinh from Boost.Math.

        constexpr T zero { 0, 0 };
        constexpr T one  { 1, 0 };

        if (x < zero)
        {
            result = -asinh_impl(-x);
        }
        else if (x > zero)
        {
            constexpr T fourth_root_epsilon { 1, -((std::numeric_limits<T>::digits10 + 1) / 4) };

            const auto xsq = x * x;

            if (x > one / fourth_root_epsilon)
            {
                // http://functions.wolfram.com/ElementaryFunctions/ArcSinh/06/01/06/01/0001/
                // approximation by laurent series in 1/x at 0+ order from -1 to 1
                result = numbers::ln2_v<T> + ::boost::decimal::log(x) + one / (T { 4, 0 } * xsq);
            }
            else if(x >= T { 5 , -1 })
            {
                // http://functions.wolfram.com/ElementaryFunctions/ArcSinh/02/
                result = ::boost::decimal::log(x + sqrt(xsq + one));
            }
            else if (x >= fourth_root_epsilon)
            {
                // As below, but rearranged to preserve digits:
                result = ::boost::decimal::log1p(x + (sqrt(one + xsq) - one));
            }
            else
            {
                // http://functions.wolfram.com/ElementaryFunctions/ArcSinh/06/01/03/01/0001/
                // approximation by taylor series in x at 0 up to order 2
                result = x;

                const T x3 = xsq * x;

                // approximation by taylor series in x at 0 up to order 4
                result -= x3 / T { 6, 0 };
            }
        }
    }

    return result;
}

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto asinh(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::asinh_impl(static_cast<evaluation_type>(x)));
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_ASINH_HPP
