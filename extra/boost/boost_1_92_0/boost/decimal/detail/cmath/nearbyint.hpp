// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_NEARBYINT_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_NEARBYINT_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/cmath/rint.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#endif

namespace boost {
namespace decimal {

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto nearbyint(const T num) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    return rint(num);
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_NEARBYINT_HPP
