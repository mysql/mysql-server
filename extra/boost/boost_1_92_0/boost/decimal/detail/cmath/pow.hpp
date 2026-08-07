// Copyright 2023 - 2026 Matt Borland
// Copyright 2023 - 2026 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_POW_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_POW_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/detail/cmath/impl/pow_impl.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <algorithm>
#include <cmath>
#include <type_traits>
#endif

namespace boost {
namespace decimal {

BOOST_DECIMAL_EXPORT template <typename T, typename IntegralType>
constexpr auto pow(const T b, const IntegralType p) noexcept
    BOOST_DECIMAL_REQUIRES_TWO_RETURN(detail::is_decimal_floating_point_v, T, detail::is_integral_v, IntegralType, T)
{
    using local_integral_type = IntegralType;

    constexpr T zero { 0, 0 };
    constexpr T one  { 1, 0 };

    T result { };

    const auto fpc_x = fpclassify(b);

    const auto p_is_odd = (static_cast<local_integral_type>(p & 1) != static_cast<local_integral_type>(0));

    if (p == static_cast<local_integral_type>(0))
    {
        // pow(base, +/-0) returns 1 for any base, even when base is NaN.

        // Excluded from LCOV since it's apparently optimized away or otherwise
        // missing from LCOV. Verified this line is covered in the unit tests.

        result = one; // LCOV_EXCL_LINE
    }
    else if (fpc_x == FP_ZERO)
    {
        // pow(  +0, exp), where exp is a negative odd  integer, returns +infinity.
        // pow(  -0, exp), where exp is a negative odd  integer, returns +infinity.
        // pow(+/-0, exp), where exp is a negative even integer, returns +infinity.

        // pow(  +0, exp), where exp is a positive odd  integer, returns +0.
        // pow(  -0, exp), where exp is a positive odd  integer, returns -0.
        // pow(+/-0, exp), where exp is a positive even integer, returns +0.

        result = ((p < static_cast<local_integral_type>(0)) ? std::numeric_limits<T>::infinity() : ((p_is_odd && signbit(b)) ? -zero : zero));
    }
    #ifndef BOOST_DECIMAL_FAST_MATH
    else if (fpc_x == FP_INFINITE)
    {
        if (signbit(b))
        {
            if (p < static_cast<local_integral_type>(0))
            {
                // pow(-infinity, exp) returns -0 if exp is a negative odd integer.
                // pow(-infinity, exp) returns +0 if exp is a negative even integer.

                result = (p_is_odd ? -zero : zero);
            }
            else
            {
                // pow(-infinity, exp) returns -infinity if exp is a positive odd integer.
                // pow(-infinity, exp) returns +infinity if exp is a positive even integer.

                result = (p_is_odd ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity());
            }
        }
        else
        {
            // pow(+infinity, exp) returns +0 for any negative exp.
            // pow(+infinity, exp) returns +infinity for any positive exp.

            result = ((p < static_cast<local_integral_type>(0)) ? zero : std::numeric_limits<T>::infinity());
        }
    }
    else if (fpc_x != FP_NORMAL)
    {
        // Excluded from LCOV since it's apparently optimized away or otherwise
        // missing from LCOV. Verified this line is covered in the unit tests.

        result = std::numeric_limits<T>::quiet_NaN(); // LCOV_EXCL_LINE
    }
    #endif
    else
    {
        using local_unsigned_integral_type = std::make_unsigned_t<IntegralType>;

        int exp10val { };

        const auto bn { frexp10(b, &exp10val) };

        const auto
            zeros_removal
            {
                detail::remove_trailing_zeros(bn)
            };

        const bool is_pure { static_cast<int>(zeros_removal.trimmed_number) == 1 };

        if(is_pure)
        {
            // Here, a pure power-of-10 argument (b) gets a pure integral result.
            const int log10_val { exp10val + static_cast<int>(zeros_removal.number_of_removed_zeros) };

            result = T { 1, log10_val * static_cast<int>(p) };
        }
        else
        {
            BOOST_DECIMAL_IF_CONSTEXPR (std::is_signed<local_integral_type>::value)
            {
                if(p < static_cast<local_integral_type>(UINT8_C(0)))
                {
                    const auto up =
                        static_cast<local_unsigned_integral_type>
                        (
                              static_cast<local_unsigned_integral_type>(~p)
                            + static_cast<local_unsigned_integral_type>(UINT8_C(1))
                        );

                    result = one / detail::pow_n_impl(b, up);
                }
                else
                {
                    result = detail::pow_n_impl(b, static_cast<local_unsigned_integral_type>(p));
                }
            }
            else
            {
                result = detail::pow_n_impl(b, static_cast<local_unsigned_integral_type>(p));
            }
        }
    }

    return result;
}

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto pow(const T x, const T a) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    constexpr T zero { 0, 0 };
    constexpr T one  { 1, 0 };

    auto result = zero;

    const auto fpc_a = fpclassify(a);

    const auto na = static_cast<int>(a);

    if (fpc_a == FP_ZERO)
    {
        // pow(base, +/-0) returns 1 for any base, even when base is NaN.

        result = one;
    }
    else if (na == a)
    {
        result = pow(x, na);
    }
    else
    {
        const auto fpc_x = fpclassify(x);

        if ((fpc_x == FP_NAN) || (fpc_a == FP_NAN))
        {
            // This line is known to be covered by tests.
            result = std::numeric_limits<T>::quiet_NaN(); // LCOV_EXCL_LINE
        }
        else if (fpc_x == FP_ZERO)
        {
            #ifndef BOOST_DECIMAL_FAST_MATH
            if ((fpc_a == FP_NORMAL) || (fpc_a == FP_INFINITE))
            {
                // pow(+/-0, exp), where exp is negative and finite, returns +infinity.
                // pow(+/-0, exp), where exp is positive non-integer, returns +0.

                // pow(+/-0, -infinity) returns +infinity.
                // pow(+/-0, +infinity) returns +0.

                result = (signbit(a) ? std::numeric_limits<T>::infinity() : zero);
            }
            #else
            if (fpc_a == FP_NORMAL)
            {
                result = T{0};
            }
            #endif
        }
        #ifndef BOOST_DECIMAL_FAST_MATH
        else if (fpc_x == FP_INFINITE)
        {
            if ((fpc_a == FP_NORMAL) || (fpc_a == FP_INFINITE))
            {
                // pow(+infinity, exp) returns +0 for any negative exp.
                // pow(-infinity, exp) returns +infinity for any positive exp.

                result = (signbit(a) ? zero : std::numeric_limits<T>::infinity());
            }
        }
        #endif
        else
        {
            #ifndef BOOST_DECIMAL_FAST_MATH
            if (fpc_a == FP_INFINITE)
            {
                result =
                    (
                          (fabs(x) < one) ? (signbit(a) ? std::numeric_limits<T>::infinity() : zero)
                        : (fabs(x) > one) ? (signbit(a) ? zero : std::numeric_limits<T>::infinity())
                        : one
                    );
            }
            else
            #endif
            {
                const T a_log_x { a * ::boost::decimal::log(x) };

                result = exp(a_log_x);
            }
        }
    }

    return result;
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_POW_HPP
