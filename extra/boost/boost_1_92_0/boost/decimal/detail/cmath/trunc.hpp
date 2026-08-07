// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_TRUNC_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_TRUNC_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/fenv_rounding.hpp>
#include <boost/decimal/detail/cmath/floor.hpp>
#include <boost/decimal/detail/cmath/ceil.hpp>
#include <boost/decimal/detail/cmath/frexp10.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <limits>
#endif

namespace boost {
namespace decimal {

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto trunc(const T val) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    return (val > 0) ? floor(val) : ceil(val);
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_TRUNC_HPP
