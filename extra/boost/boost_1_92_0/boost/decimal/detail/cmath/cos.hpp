// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_COS_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_COS_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/numbers.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/cmath/cos.hpp>
#include <boost/decimal/detail/cmath/remquo.hpp>
#include <boost/decimal/detail/cmath/impl/sin_impl.hpp>
#include <boost/decimal/detail/cmath/impl/cos_impl.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <cstdint>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <typename T>
constexpr auto cos_impl(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    T result { };

    #ifndef BOOST_DECIMAL_FAST_MATH
    const auto fpc = fpclassify(x);

    // First check non-finite values and small angles

    if ((fpc == FP_INFINITE) || (fpc == FP_NAN))
    {
        result = x;
    }
    else
    #endif
    if (signbit(x))
    {
        result = cos(-x);
    }
    else
    {
        if (x > std::numeric_limits<T>::epsilon())
        {
            // Perform argument reduction and subsequent scaling of the result.

            // Given x = k * (pi/2) + r, compute n = (k % 4).

            // | n |  sin(x) |  cos(x) |  sin(x)/cos(x) |
            // |----------------------------------------|
            // | 0 |  sin(r) |  cos(r) |  sin(r)/cos(r) |
            // | 1 |  cos(r) | -sin(r) | -cos(r)/sin(r) |
            // | 2 | -sin(r) | -cos(r) |  sin(r)/cos(r) |
            // | 3 | -cos(r) |  sin(r) | -cos(r)/sin(r) |

            const T two_x = x * 2;

            const auto k = static_cast<unsigned>(two_x / numbers::pi_v<T>);
            const auto n = k % static_cast<unsigned>(UINT8_C(4));

            const T two_r { two_x - (numbers::pi_v<T> * k) };

            T r { two_r / 2 };

            constexpr T half { 5, -1 };

            const bool do_scaling { r > half };

            if(do_scaling)
            {
                // Reduce the argument with factors of three.
                r /= static_cast<unsigned>(UINT8_C(3));
            }

            switch(n)
            {
              case static_cast<unsigned>(UINT8_C(1)):
              case static_cast<unsigned>(UINT8_C(3)):
                result = detail::sin_series_expansion(r);
                break;
              case static_cast<unsigned>(UINT8_C(0)):
              case static_cast<unsigned>(UINT8_C(2)):
              default:
                result = detail::cos_series_expansion(r);
                break;
            }

            if(do_scaling)
            {
                result *= (((result * result) * static_cast<unsigned>(UINT8_C(4))) - static_cast<unsigned>(UINT8_C(3)));
            }

            if(signbit(result)) { result = -result; }

            const auto b_neg = ((n == static_cast<unsigned>(UINT8_C(1))) || (n == static_cast<unsigned>(UINT8_C(2))));

            if(b_neg) { result = -result; }
        }
        else
        {
            constexpr T one { 1 };

            result = one;
        }
    }

    return result;
}

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto cos(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::cos_impl(static_cast<evaluation_type>(x)));
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_COS_HPP
