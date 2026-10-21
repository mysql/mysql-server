// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_TANH_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_TANH_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/detail/cmath/impl/tanh_impl.hpp>
#include <boost/decimal/detail/concepts.hpp>
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
constexpr auto tanh_impl(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    const auto fpc = fpclassify(x);

    constexpr T zero { 0, 0 };
    constexpr T one  { 1, 0 };

    auto result = zero;

    if (fpc == FP_ZERO)
    {
        result = x;
    }
    #ifndef BOOST_DECIMAL_FAST_MATH
    else if (fpc != FP_NORMAL)
    {
        if (fpc == FP_INFINITE)
        {
            if (signbit(x))
            {
                result = -one;
            }
            else
            {
                result = one;
            }
        }
        else if (fpc == FP_NAN)
        {
            result = x;
        }
    }
    #endif
    else
    {
        if (signbit(x))
        {
            result = -tanh(-x);
        }
        else
        {
            constexpr T quarter { 25, -2 };

            if (x < quarter)
            {
                const auto xsq = x * x;

                result = detail::tanh_series_expansion(xsq);

                result = x * fma(result, xsq, one);
            }
            else
            {
                const auto exp_pos_val = exp(x);
                const auto exp_neg_val = one / exp_pos_val;

                result = (exp_pos_val - exp_neg_val) / (exp_pos_val + exp_neg_val);
            }
        }
    }

    return result;
}

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto tanh(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::tanh_impl(static_cast<evaluation_type>(x)));
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_TANH_HPP
