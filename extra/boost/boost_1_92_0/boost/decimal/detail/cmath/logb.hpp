// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_LOGB_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_LOGB_HPP

#include <boost/decimal/fwd.hpp> // NOLINT(llvm-include-order)
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cmath>
#include <type_traits>
#endif

namespace boost {
namespace decimal {

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto logb(const T num) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    const auto fpc {fpclassify(num)};

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (fpc == FP_ZERO)
    {
        return -std::numeric_limits<T>::infinity();
    }
    else if (fpc == FP_INFINITE)
    {
        return std::numeric_limits<T>::infinity();
    }
    else if (fpc == FP_NAN)
    {
        return num;
    }
    #else
    if (fpc == FP_ZERO)
    {
        return T{0};
    }
    #endif

    const auto offset = detail::num_digits(num.full_significand()) - 1;
    const auto expval = static_cast<int>(static_cast<int>(num.unbiased_exponent()) + offset);

    return static_cast<T>(expval);
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_LOGB_HPP
