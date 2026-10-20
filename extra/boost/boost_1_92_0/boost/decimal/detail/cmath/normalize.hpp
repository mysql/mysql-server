// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_NORMALIZE_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_NORMALIZE_HPP

#include <boost/decimal/detail/cmath/frexp10.hpp>
#include <boost/decimal/detail/attributes.hpp>
#include <boost/decimal/detail/concepts.hpp>

namespace boost {
namespace decimal {

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, std::enable_if_t<detail::is_fast_type_v<DecimalType>, bool> = true>
constexpr DecimalType normalize(const DecimalType& value) noexcept
{
    return value;
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType, std::enable_if_t<!detail::is_fast_type_v<DecimalType>, bool> = true>
constexpr DecimalType normalize(const DecimalType value) noexcept
{
    int exp {};
    const auto significand {frexp10(value, &exp)};
    return DecimalType{significand, exp, signbit(value)};
}

} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_CMATH_NORMALIZE_HPP
