// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_TGAMMA_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_TGAMMA_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/detail/cmath/impl/tgamma_impl.hpp>
#include <boost/decimal/detail/cmath/sin.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/numbers.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <iterator>
#include <limits>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <typename T>
constexpr auto tgamma_impl(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    T result { };

    const auto nx = static_cast<int>(x);

    const auto is_pure_int = ((nx == x) && fabs(x) < T { (std::numeric_limits<int>::max)() });

    const bool is_neg = signbit(x);

    const auto fpc = fpclassify(x);

    if (fpc != FP_NORMAL)
    {
        if (fpc == FP_ZERO)
        {
            result = (is_neg ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity());
        }
        #ifndef BOOST_DECIMAL_FAST_MATH
        else if(fpc == FP_INFINITE)
        {
            result = (is_neg ? std::numeric_limits<T>::quiet_NaN() : std::numeric_limits<T>::infinity());
        }
        else
        {
            result = x;
        }
        #endif
    }
    else if (is_pure_int && is_neg)
    {
        // Pure negative integer argument.
        #ifndef BOOST_DECIMAL_FAST_MATH
        result = std::numeric_limits<T>::quiet_NaN();
        #else
        result = T{0};
        #endif
    }
    else
    {
        if (is_neg)
        {
            // Reflection for negative argument.

            result = -numbers::pi_v<T> / ((x * tgamma(-x)) * sin(numbers::pi_v<T> * x));
        }
        else
        {
            constexpr T one { 1, 0 };

            if (is_pure_int)
            {
                result = one; // LCOV_EXCL_LINE

                for(auto index = 2; index < nx; ++index)
                {
                    result *= index; // LCOV_EXCL_LINE
                }
            }
            else
            {
                constexpr int asymp_cutoff
                {
                      std::numeric_limits<T>::digits10 < 10 ? T { 2, 1 } // 20
                    : std::numeric_limits<T>::digits10 < 20 ? T { 5, 1 } // 50
                    :                                         T { 9, 1 } // 90
                };

                if (x < T { asymp_cutoff })
                {
                    T r { one };

                    T z { x };

                    // Use small-argument Taylor series expansion
                    // (with scaling for arguments greater than one).

                    for(auto k = 1; k <= nx; ++k)
                    {
                        r *= (z - k);
                    }

                    z -= nx;

                    result = r / (z * fma(detail::tgamma_series_expansion(z), z, one));
                }
                else
                {
                    // Use large-argument asymptotic expansion.

                    const T prefix { exp(((x  - T { 5, -1 }) * ::boost::decimal::log(x)) - x) };

                    result =  prefix * detail::tgamma_series_expansion_asymp(one / x);
                }
            }
        }
    }

    return result;
}

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto tgamma(const T x) noexcept                           // LCOV_EXCL_LINE
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)  // LCOV_EXCL_LINE
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::tgamma_impl(static_cast<evaluation_type>(x)));
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_TGAMMA_HPP
