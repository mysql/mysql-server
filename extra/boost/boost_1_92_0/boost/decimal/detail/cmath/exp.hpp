// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_EXP_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_EXP_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/detail/cmath/impl/expm1_impl.hpp>
#include <boost/decimal/detail/cmath/impl/pow_impl.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/numbers.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <array>
#include <type_traits>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <typename T>
constexpr auto exp_impl(T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    const auto fpc = fpclassify(x);

    constexpr T zero { 0, 0 };
    constexpr T one  { 1, 0 };

    auto result = zero;

    if (fpc == FP_ZERO)
    {
        result = one;
    }
    #ifndef BOOST_DECIMAL_FAST_MATH
    else if (fpc != FP_NORMAL)
    {
        if (fpc == FP_INFINITE)
        {
            result = (signbit(x) ? zero : std::numeric_limits<T>::infinity());
        }
        else if (fpc == FP_NAN)
        {
            result = x;
        }
    } // LCOV_EXCL_LINE
    #endif
    else
    {
        if (signbit(x))
        {
            result = one / exp(-x);
        }
        else if(x == one)
        {
            result = numbers::e_v<T>;
        }
        else
        {
            // Scale the argument to 0 < x < log(2).

            int nf2 { };

            if (x > numbers::ln2_v<T>)
            {
                nf2 = static_cast<int>(x / numbers::ln2_v<T>);

                x -= numbers::ln2_v<T> * nf2;
            }

            result = fma(x, detail::expm1_series_expansion(x), one);

            if (nf2 > 0)
            {
                if (nf2 < 64)
                {
                    result *= T { UINT64_C(1) << static_cast<unsigned>(nf2), 0 };
                }
                else
                {
                    result *= detail::pow_2_impl<T>(nf2);
                }
            }
        }
    }

    return result;
}

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto exp(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::exp_impl(static_cast<evaluation_type>(x)));
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_EXP_HPP
