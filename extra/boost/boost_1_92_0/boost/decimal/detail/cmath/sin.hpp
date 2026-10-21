// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_SIN_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_SIN_HPP

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
constexpr auto sin_impl(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    T result { };

    const auto fpc = fpclassify(x);

    // First check non-finite values and small angles.
    if (fabs(x) < std::numeric_limits<T>::epsilon()
        #ifndef BOOST_DECIMAL_FAST_MATH
        || (fpc == FP_INFINITE) || (fpc == FP_NAN)
        #endif
        )
    {
        result = x;
    }
    else if (signbit(x))
    {
        result = -sin(-x);
    }
    else
    {
        if(x > 0)
        {
            // Perform argument reduction and subsequent scaling of the result.

            // Given x = k * (pi/2) + r, compute n = (k % 4).

            // | n |  sin(x) |  cos(x) |  sin(x)/cos(x) |
            // |----------------------------------------|
            // | 0 |  sin(r) |  cos(r) |  sin(r)/cos(r) |
            // | 1 |  cos(r) | -sin(r) | -cos(r)/sin(r) |
            // | 2 | -sin(r) | -cos(r) |  sin(r)/cos(r) |
            // | 3 | -cos(r) |  sin(r) | -cos(r)/sin(r) |

            const T two_x { x * 2 };

            const unsigned k { static_cast<unsigned>(two_x / numbers::pi_v<T>) };
            const auto n = k % static_cast<unsigned>(4);

            const T two_r { two_x - (numbers::pi_v<T> * k) };

            T r { two_r / 2 };

            constexpr T one { 1 };

            bool do_scaling { two_r > one };

            constexpr T cbrt_epsilon { cbrt(std::numeric_limits<T>::epsilon()) };

            switch(n)
            {
                case static_cast<unsigned>(UINT8_C(1)):
                case static_cast<unsigned>(UINT8_C(3)):
                {
                    const T d2r { numbers::pi_v<T> - two_r };

                    if (d2r < cbrt_epsilon)
                    {
                        result = d2r * (one - (d2r * d2r) / 12) / 2;

                        do_scaling = false;
                    }
                    else
                    {
                        if(do_scaling)
                        {
                            // Reduce the argument with one single factor of three.
                            r /= static_cast<unsigned>(UINT8_C(3));
                        }

                        result = detail::cos_series_expansion(r);
                    }
                }
                break;

                case static_cast<unsigned>(UINT8_C(0)):
                case static_cast<unsigned>(UINT8_C(2)):
                default:
                {
                    if (two_r < cbrt_epsilon)
                    {
                        // Normal[Series[Sin[x/2], {x, 0, 3}]]
                        // FullSimplify[%]
                        // HornerForm[%]

                        result = (two_r * (one - (two_r * two_r) / 24)) / 2;
                    }
                    else
                    {
                        if(do_scaling)
                        {
                            // Reduce the argument with one single factor of three.
                            r /= static_cast<unsigned>(UINT8_C(3));
                        }

                        result = detail::sin_series_expansion(r);
                    }
                }
                break;
            }

            if(do_scaling)
            {
                result *= (static_cast<unsigned>(UINT8_C(3)) - ((result * result) * static_cast<unsigned>(UINT8_C(4))));
            }

            if(signbit(result))
            {
              result = -result;
            }

            const auto b_neg = (n > static_cast<unsigned>(UINT8_C(1)));

            if(b_neg) { result = -result; }
        }
    }

    return result;
}

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto sin(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::sin_impl(static_cast<evaluation_type>(x)));
}

} // namespace decimal
} // namespace boost


#endif // BOOST_DECIMAL_DETAIL_CMATH_SIN_HPP
