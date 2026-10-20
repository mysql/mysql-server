// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_ACOS_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_ACOS_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/numbers.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/promotion.hpp>
#include <boost/decimal/detail/config.hpp>
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
constexpr auto acos_impl(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(x))
    {
        return x;
    }
    #endif

    constexpr auto half_pi {numbers::pi_v<T> / 2};
    const auto absx {fabs(static_cast<T>(x))};

    T result {};

    if (absx > 1)
    {
        result = std::numeric_limits<T>::quiet_NaN();
    }
    else if (x < T{-5, -1})
    {
        result = numbers::pi_v<T> - 2 * detail::asin_series(sqrt((1 - absx) / 2));
    }
    else if (x < -std::numeric_limits<T>::epsilon())
    {
        result = half_pi + detail::asin_series(absx);
    }
    else if (x < T{5, -1})
    {
        result = half_pi - detail::asin_series(x);
    }
    else
    {
        result = half_pi - (half_pi - 2 * detail::asin_series(sqrt((1 - x) / 2)));
    }

    return result;
}

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto acos(const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::acos_impl(static_cast<evaluation_type>(x)));
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_ACOS_HPP
