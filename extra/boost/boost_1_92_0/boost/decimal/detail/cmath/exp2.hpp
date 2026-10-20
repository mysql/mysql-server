// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_EXP2_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_EXP2_HPP

#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/cmath/pow.hpp>

namespace boost {
namespace decimal {

namespace detail {

template <typename T>
constexpr auto exp2_impl(const T num) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    constexpr T two {2, 0};

    return pow(two, num);
}

} //namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto exp2(const T num) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;

    return static_cast<T>(detail::exp2_impl(static_cast<evaluation_type>(num)));
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_EXP2_HPP
