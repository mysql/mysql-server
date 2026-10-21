// Copyright 2023 - 2026 Matt Borland
// Copyright 2023 - 2026 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_CBRT_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_CBRT_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/cmath/abs.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <cstdint>
#include <cmath>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <typename T>
constexpr auto cbrt_impl(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    const auto fpc = fpclassify(x);

    T result { };

    if ((fpc == FP_ZERO)
        #ifndef BOOST_DECIMAL_FAST_MATH
        || (fpc == FP_NAN)
        #endif
        )
    {
        result = x;
    }
    else if (signbit(x))
    {
        result = -cbrt(-x);
    }
    #ifndef BOOST_DECIMAL_FAST_MATH
    else if (fpc == FP_INFINITE)
    {
        result = std::numeric_limits<T>::infinity();
    }
    #endif
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
            // Here, a pure power-of-10 argument gets a straightforward result.
            // For argument 10^n where n is a multiple of 3, the result is exact.

            const int p10 { exp10val + static_cast<int>(zeros_removal.number_of_removed_zeros) };

            if (p10 == 0)
            {
                result = T { 1 };
            }
            else
            {
                const int p10_mod3 = (p10 % 3);
                const int p10_div3 = (p10 / 3);

                result = T { 1, p10_div3 };

                switch (p10_mod3)
                {
                    case 2:
                        result *= numbers::cbrt10_v<T>;
                        // fallthrough

                    case 1:
                        result *= numbers::cbrt10_v<T>;
                        break;

                    case -2:
                        result /= numbers::cbrt10_v<T>;
                        // fallthrough

                    case -1:
                        result /= numbers::cbrt10_v<T>;
                        break;
                }
            }
        }
        else
        {
            // Scale the argument to the interval 1/64 < x <= 1/8.
            T gx { gn, -std::numeric_limits<T>::digits10 };

            const bool is_scaled_by_eight { (gx > T { 125, -3 }) };

            if(is_scaled_by_eight)
            {
                gx /= 8;
            }

            exp10val += std::numeric_limits<T>::digits10;

            // For this work we perform an order-2 Pade approximation of the cube-root
            // at argument x = 1/16. This results in slightly more than 4 decimal digits
            // of accuracy over the interval 1/64 < x <= 1/8.

            // PadeApproximant[x^(1/3), {x, 1/16, {2, 2}}]
            // FullSimplify[%]

            // HornerForm[Numerator[Out[2]]]
            // Results in:
            //   5 + x (560 + 3584 x)

            // HornerForm[Denominator[Out[2]]]
            // Results in:
            //   2^(1/3) (28 + x (1120 + 2560 x))

            constexpr T five { 5 };

            result =
                  (five + gx * (560 + 3584 * gx))
                / (numbers::cbrt2_v<T> * (28 + gx * (1120 + 2560 * gx)));

            // Perform 2, 3 or 4 Newton-Raphson iterations depending on precision.
            // Note from above, we start with slightly more than 4 decimal digits
            // of accuracy.

            constexpr int iter_loops
            {
                  std::numeric_limits<T>::digits10 < 10 ? 2
                : std::numeric_limits<T>::digits10 < 20 ? 3 : 4
            };

            for (int idx = 0; idx < iter_loops; ++idx)
            {
                result = ((result + result) + gx / (result * result)) / 3;
            }

            if (exp10val != 0)
            {
                const int exp10val_mod3 = (exp10val % 3);
                const int exp10val_div3 = (exp10val / 3);

                result *= T { 1, exp10val_div3 };

                switch (exp10val_mod3)
                {
                    case 2:
                        result *= numbers::cbrt10_v<T>;
                        // fallthrough

                    case 1:
                        result *= numbers::cbrt10_v<T>;
                        break;

                    case -2:
                        result /= numbers::cbrt10_v<T>;
                        // fallthrough

                    case -1:
                        result /= numbers::cbrt10_v<T>;
                        break;
                }
            }

            if(is_scaled_by_eight)
            {
                result *= 2;
            }
        }
    }

    return result;
}

} //namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto cbrt(const T val) noexcept // LCOV_EXCL_LINE
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::cbrt_impl(static_cast<evaluation_type>(val)));
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_CBRT_HPP
