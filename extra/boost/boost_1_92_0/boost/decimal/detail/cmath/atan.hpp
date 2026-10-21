// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_ATAN_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_ATAN_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/detail/cmath/impl/atan_impl.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/numbers.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <cstdint>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <typename T>
constexpr auto atan_impl(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    const auto fpc { fpclassify(x) };

    T result { };

    constexpr T my_pi_half { numbers::pi_v<T> / 2 };

    if (fpc == FP_ZERO
        #ifndef BOOST_DECIMAL_FAST_MATH
        || fpc == FP_NAN
        #endif
        )
    {
        result = x;
    }
    else if (signbit(x))
    {
        result = -atan_impl(-x);
    }
    #ifndef BOOST_DECIMAL_FAST_MATH
    else if (fpc == FP_INFINITE)
    {
        result = my_pi_half;
    }
    #endif
    else
    {
        constexpr T one { 1 };

        if (x <= T { 48 })
        {
            // Define small-ish arguments to be less than 39/16.
            const bool is_smallish { x <= T { 24375, -4 } };

            // The portion of the algorithm for arc-tangent regarding scaling large-valued
            // argument is based on Chapter 11, page 194 of Cody and Waite, "Software Manual
            // for the Elementary Functions", Prentice Hall, 1980.

            const T
                fx_arg
                {
                    (!is_smallish)
                        ? ((x * numbers::sqrt3_v<T>) - one) / (numbers::sqrt3_v<T> + x)
                        :   x
                };

            constexpr T half         {  5, -1 };
            constexpr T three_halves { 15, -1 };

            result =   (fx_arg <= std::numeric_limits<T>::epsilon()) ? fx_arg
                     : (fx_arg <= T { 4375,  -4 })                   ?                              detail::atan_series (fx_arg)
                     : (fx_arg <= T { 6875,  -4 })                   ? detail::atan_values<T>(0U) + detail::atan_series((fx_arg - half) / (one + fx_arg / 2))
                     : (fx_arg <= T { 11875, -4 })                   ? detail::atan_values<T>(1U) + detail::atan_series((fx_arg - one) / (fx_arg + one))
                     :                                                 detail::atan_values<T>(2U) + detail::atan_series((fx_arg - three_halves) / (one + three_halves * fx_arg))
                     ;

            if(!is_smallish)
            {
                constexpr T my_pi_over_six { numbers::pi_v<T> / 6 };

                result += my_pi_over_six;
            }
        }
        else
        {
            result = my_pi_half - detail::atan_series(one / x);
        }
    }

    return result;
}

} //namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto atan(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::atan_impl(static_cast<evaluation_type>(x)));
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_ATAN_HPP
