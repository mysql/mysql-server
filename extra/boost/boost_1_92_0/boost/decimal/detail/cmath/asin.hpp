// Copyright 2024 - 2025 Matt Borland
// Copyright 2024 - 2025 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_ASIN_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_ASIN_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/numbers.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/cmath/fpclassify.hpp>
#include <boost/decimal/detail/cmath/fabs.hpp>
#include <boost/decimal/detail/cmath/sqrt.hpp>
#include <boost/decimal/detail/cmath/impl/asin_impl.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <cstdint>
#endif

namespace boost {
namespace decimal {

namespace detail {

template <typename T>
constexpr auto asin_impl(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    const auto fpc {fpclassify(x)};
    const auto isneg {signbit(x)};

    if (fpc == FP_ZERO
        #ifndef BOOST_DECIMAL_FAST_MATH
        || fpc == FP_NAN
        #endif
        )
    {
        return x;
    }

    const auto absx { fabs(x) };

    T result { };

    constexpr T cbrt_eps { cbrt(std::numeric_limits<T>::epsilon()) };

    constexpr T one { 1 };

    if (absx <= cbrt_eps)
    {
        result = absx * (one + (absx / 6) * absx);
    }
    else if (absx <= T { 5, -1 })
    {
        result = asin_series(absx);
    }
    else
    {
        constexpr T half_pi { numbers::pi_v<T> / 2 };

        if (absx < one)
        {
            result = half_pi - 2 * asin_series(sqrt((1 - absx) / 2));
        }
        else if (absx > one)
        {
            #ifndef BOOST_DECIMAL_FAST_MATH
            result = std::numeric_limits<T>::quiet_NaN();
            #else
            result = T{0};
            #endif
        }
        else
        {
            result = half_pi;
        }
    }

    // arcsin(-x) == -arcsin(x)
    if (isneg)
    {
        result = -result;
    }

    return result;
}

} //namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto asin(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::asin_impl(static_cast<evaluation_type>(x)));
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_ASIN_HPP
