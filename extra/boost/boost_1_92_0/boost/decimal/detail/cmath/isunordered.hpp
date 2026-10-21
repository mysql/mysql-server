// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_ISUNORDERED_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_ISUNORDERED_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <cmath>
#endif

namespace boost {
namespace decimal {

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto isunordered(const T lhs, const T rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, T, bool)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    return isnan(lhs) || isnan(rhs);
    #else
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return false;
    #endif
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_ISUNORDERED_HPP
