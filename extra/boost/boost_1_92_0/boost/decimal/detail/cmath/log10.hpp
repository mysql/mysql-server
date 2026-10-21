// Copyright 2023 Matt Borland
// Copyright 2023 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_LOG10_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_LOG10_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/detail/cmath/impl/log_impl.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/numbers.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cmath>
#include <type_traits>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <typename T>
constexpr auto log10_impl(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    T result { };

    const auto fpc = fpclassify(x);

    if (fpc == FP_ZERO)
    {
        #ifndef BOOST_DECIMAL_FAST_MATH
        result = -std::numeric_limits<T>::infinity();
        #else
        result = T{0};
        #endif
    }
    else if (signbit(x))
    {
        #ifndef BOOST_DECIMAL_FAST_MATH
        result = std::numeric_limits<T>::quiet_NaN();
        #else
        result = T{0};
        #endif
    }
    else if (fpc != FP_NORMAL)
    {
        result = x;
    }
    else
    {
        int exp10val { };

        const auto gn { frexp10(x, &exp10val) };

        const auto
            zeros_removal
            {
                remove_trailing_zeros(gn)
            };

        const bool is_pure { zeros_removal.trimmed_number == 1U };

        if(is_pure)
        {
            // Here, a pure power-of-10 argument gets a pure integral result.
            const int p10 { exp10val + static_cast<int>(zeros_removal.number_of_removed_zeros) };

            result = T { p10 };
        }
        else
        {
            constexpr T one  { 1 };

            if (x < one)
            {
                // Handle reflection.
                result = -::boost::decimal::log10(one / x);
            }
            else if(x > one)
            {
                // The algorithm for base-10 logarithm is based on Chapter 5,
                // pages 35-36 of Cody and Waite, "Software Manual for the
                // Elementary Functions", Prentice Hall, 1980.

                // In this implementation, however, we use 2s (as for natural
                // logarithm in Cody and Waite) even though we are computing
                // the base-10 logarithm.

                T g { gn, -std::numeric_limits<T>::digits10 };

                exp10val += std::numeric_limits<T>::digits10;

                int reduce_sqrt2 { };

                while (g < numbers::inv_sqrt2_v<T>)
                {
                    g += g;

                    ++reduce_sqrt2;
                }

                const T s   { (g - one) / (g + one) };
                const T z   { s + s };
                const T zsq { z * z };

                result = z * fma(detail::log_series_expansion(zsq), zsq, one);

                result /= numbers::ln10_v<T>;

                for(int i = 0; i < reduce_sqrt2; ++i)
                {
                    result -= numbers::log10_2_v<T>;
                }

                result += static_cast<T>(exp10val);
            }
        }
    }

    return result;
}

} //namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto log10(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::log10_impl(static_cast<evaluation_type>(x)));
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_LOG10_HPP
