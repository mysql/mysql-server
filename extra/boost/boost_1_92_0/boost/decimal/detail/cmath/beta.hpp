// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_BETA_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_BETA_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/cmath/lgamma.hpp>
#include <boost/decimal/detail/cmath/exp.hpp>

namespace boost {
namespace decimal {

namespace detail {

template <typename T>
constexpr auto beta_impl(const T x, const T y) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(x) || isnan(y))
    {
        return std::numeric_limits<T>::quiet_NaN();
    }
    #endif

    // The beta function is defined as tgamma(x) * tgamma(y) / tgamma(x + y)
    // If we use lgamma instead and then take the exp at the end we avoid
    // the easy case of numerical overflow
    const auto temp {lgamma(x) + lgamma(y) - lgamma(x + y)};
    return exp(temp);
}

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto beta(const T y, const T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::beta_impl(static_cast<evaluation_type>(y), static_cast<evaluation_type>(x)));
}

} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_BETA_HPP
