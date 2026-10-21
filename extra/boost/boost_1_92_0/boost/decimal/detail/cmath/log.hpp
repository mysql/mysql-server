// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_LOG_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_LOG_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
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
constexpr auto log_impl(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    constexpr T one { 1, 0 };

    T result { };

    const auto fpc = fpclassify(x);

    if (fpc == FP_ZERO)
    {
        result = -std::numeric_limits<T>::infinity();
    }
    else if (signbit(x) || (fpc == FP_NAN))
    {
        #ifndef BOOST_DECIMAL_FAST_MATH
        result = std::numeric_limits<T>::quiet_NaN();
        #else
        result = T{0};
        #endif
    }
    #ifndef BOOST_DECIMAL_FAST_MATH
    else if (fpc == FP_INFINITE)
    {
        result = std::numeric_limits<T>::infinity();
    }
    #endif
    else if (x < one)
    {
        // Handle reflection.
        result = -::boost::decimal::log(one / x);
    }
    else if(x > one)
    {
        // Use the implementation of ::boost::decimal::log10 in order to compute the natural
        // logarithm. The base of the boost::decimal library is, in fact,
        // base-10. And so, somewhat uncommonly, the fastest and most accurate
        // logarithm in this system is log10 in base-10.

        result = log10(x) * numbers::ln10_v<T>;
    }
    else
    {
      constexpr T zero { 0 };

      result = zero;
    }

    return result;
}

} //namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto log(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::log_impl(static_cast<evaluation_type>(x)));
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_LOG_HPP
