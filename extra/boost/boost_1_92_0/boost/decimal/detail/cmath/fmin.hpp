// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_FMIN_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_FMIN_HPP

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

BOOST_DECIMAL_EXPORT template <typename T1, typename T2>
constexpr auto fmin(const T1 lhs, const T2 rhs) noexcept
    BOOST_DECIMAL_REQUIRES_TWO(detail::is_decimal_floating_point_v, T1, detail::is_decimal_floating_point_v, T2)
{
    using promoted_type = detail::promote_args_t<T1, T2>;

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(lhs) && !isnan(rhs))
    {
        if (issignaling(lhs))
        {
            return nan_conversion(lhs);
        }
        else
        {
            return static_cast<promoted_type>(rhs);
        }
    }
    else if ((!isnan(lhs) && isnan(rhs)) || (isnan(lhs) && isnan(rhs)))
    {
        if (issignaling(lhs))
        {
            return nan_conversion(lhs);
        }
        else if (issignaling(rhs))
        {
            return nan_conversion(rhs);
        }
        else
        {
            return static_cast<promoted_type>(lhs);
        }
    }
    #endif

    return lhs < rhs ? static_cast<promoted_type>(lhs) : static_cast<promoted_type>(rhs);
}

} // namespace decimal
} // namespace boost

#endif //BOOST_FMIN_HPP
