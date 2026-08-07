// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_LGAMMA_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_LGAMMA_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/detail/cmath/impl/lgamma_impl.hpp>
#include <boost/decimal/detail/cmath/impl/tgamma_impl.hpp>
#include <boost/decimal/detail/cmath/log.hpp>
#include <boost/decimal/detail/cmath/sin.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/numbers.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <array>
#include <type_traits>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <typename T>
constexpr auto lgamma_impl(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    T result { };

    const auto nx = static_cast<int>(x);

    const auto is_pure_int = (nx == x);

    const auto fpc = fpclassify(x);

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (fpc != FP_NORMAL)
    {
        if ((fpc == FP_ZERO) || (fpc == FP_INFINITE))
        {
            result = std::numeric_limits<T>::infinity();
        }
        else
        {
            result = x;
        }
    }
    #else
    if (fpc == FP_ZERO)
    {
        result = std::numeric_limits<T>::max();
    }
    #endif
    else if ((is_pure_int) && (nx < 0))
    {
        // Pure negative integer argument.
        #ifndef BOOST_DECIMAL_FAST_MATH
        result = std::numeric_limits<T>::infinity();
        #else
        result = T{0};
        #endif
    }
    else if ((is_pure_int) && ((nx == 1) || (nx == 2)))
    {
        constexpr T zero { 0, 0 };

        result = zero;
    }
    else
    {
        constexpr T one { 1, 0 };

        if (signbit(x))
        {
            // Reflection for negative argument.
            const auto za = -x + one;

            const auto phase = sin(numbers::pi_v<T> * za);

            result = ::boost::decimal::log(numbers::pi_v<T>) - ::boost::decimal::log(abs(phase)) - lgamma(za);
        }
        else
        {
            constexpr int asymp_cutoff
            {
                  std::numeric_limits<T>::digits10 < 10 ? T {  2, 1 } //  20
                : std::numeric_limits<T>::digits10 < 20 ? T {  5, 1 } //  50
                :                                         T { 15, 1 } // 150
            };

            if (x < T { 2, -1 })
            {
                // Perform the Taylor series expansion.

                result =   (x * fma(detail::lgamma_taylor_series_expansion(x), x, -numbers::egamma_v<T>))
                         - ::boost::decimal::log(x);
            }
            else if (x < T { asymp_cutoff })
            {
                result = ::boost::decimal::log(tgamma(x));
            }
            else
            {
                // Perform the Laurent series expansion. Do note, however, that
                // the coefficients of the Laurent asymptotic expansion are exactly
                // the same as those used for the tgamma() function.

                constexpr T half { 5, -1 };

                result = (((x - half) * (::boost::decimal::log(x)) - x)) + ::boost::decimal::log(detail::tgamma_series_expansion_asymp(one / x));
            }
        }
    }

    return result;
}

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto lgamma(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::lgamma_impl(static_cast<evaluation_type>(x)));
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_LGAMMA_HPP
