// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_TO_DECIMAL_HPP
#define BOOST_DECIMAL_DETAIL_TO_DECIMAL_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/attributes.hpp>
#include <boost/decimal/detail/concepts.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cerrno>
#include <limits>
#include <type_traits>
#endif

namespace boost {
namespace decimal {

#ifndef BOOST_DECIMAL_NO_CXX17_IF_CONSTEXPR

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetType, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal>
BOOST_DECIMAL_CUDA_CONSTEXPR auto to_decimal(Decimal val) noexcept -> TargetType
{
    if (isinf(val))
    {
        return val.isneg() ? -std::numeric_limits<TargetType>::infinity() :
                              std::numeric_limits<TargetType>::infinity();
    }
    else if (issignaling(val))
    {
        return val.isneg() ? -std::numeric_limits<TargetType>::signaling_NaN() :
                              std::numeric_limits<TargetType>::signaling_NaN();
    }
    else if (isnan(val))
    {
        return val.isneg() ? -std::numeric_limits<TargetType>::quiet_NaN() :
                              std::numeric_limits<TargetType>::quiet_NaN();
    }

    BOOST_DECIMAL_IF_CONSTEXPR (std::is_same<TargetType, Decimal>::value)
    {
        return val;
    }
    else
    {
        return TargetType{val.full_significand(), val.biased_exponent(), val.isneg()};
    }
}

#else

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetType, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal>
BOOST_DECIMAL_CUDA_CONSTEXPR auto to_decimal(Decimal val) noexcept -> TargetType
{
    if (isinf(val))
    {
        return val.isneg() ? -std::numeric_limits<TargetType>::infinity() :
                              std::numeric_limits<TargetType>::infinity();
    }
    else if (issignaling(val))
    {
        return val.isneg() ? -std::numeric_limits<TargetType>::signaling_NaN() :
                              std::numeric_limits<TargetType>::signaling_NaN();
    }
    else if (isnan(val))
    {
        return val.isneg() ? -std::numeric_limits<TargetType>::quiet_NaN() :
                              std::numeric_limits<TargetType>::quiet_NaN();
    }

    return TargetType{val.full_significand(), val.biased_exponent(), val.isneg()};
}

#endif

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_TO_DECIMAL_HPP
