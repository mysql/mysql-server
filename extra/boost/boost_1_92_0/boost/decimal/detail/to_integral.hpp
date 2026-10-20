// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_TO_INTEGRAL_HPP
#define BOOST_DECIMAL_DETAIL_TO_INTEGRAL_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/power_tables.hpp>
#include <boost/decimal/detail/apply_sign.hpp>
#include <boost/decimal/detail/concepts.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cerrno>
#include <limits>
#include <type_traits>
#endif

namespace boost {
namespace decimal {

// MSVC 14.1 warns of unary minus being applied to unsigned type from numeric_limits::min
// 14.2 and on get it right
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4146)
#endif

// Attempts conversion to integral type:
// If this is nan sets errno to EINVAL and returns 0
// If this is not representable sets errno to ERANGE and returns 0
template <typename Decimal, typename TargetType>
BOOST_DECIMAL_CUDA_CONSTEXPR auto to_integral(Decimal val) noexcept
    BOOST_DECIMAL_REQUIRES_TWO_RETURN(detail::is_decimal_floating_point_v, Decimal, detail::is_integral_v, TargetType, TargetType)
{
    using Conversion_Type = std::conditional_t<std::numeric_limits<TargetType>::is_signed, std::int64_t, std::uint64_t>;

    constexpr Decimal max_target_type { (std::numeric_limits<TargetType>::max)() };
    constexpr Decimal min_target_type { (std::numeric_limits<TargetType>::min)() };
    constexpr Decimal one {1};

    if (isnan(val))
    {
        #if defined(__clang__) && __clang_major__ >= 20
        if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(val))
        #endif
        {
            errno = EINVAL;
        }
        return static_cast<TargetType>(std::numeric_limits<TargetType>::max());
    }

    // Anything in [0, 1) should be flushed to 0
    if (abs(val) < one)
    {
        return static_cast<TargetType>(0);
    }

    if (val > max_target_type || val < min_target_type)
    {
        #if defined(__clang__) && __clang_major__ >= 20
        if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(val))
        #endif
        {
            errno = ERANGE;
        }

        return static_cast<TargetType>(std::numeric_limits<TargetType>::max());
    }

    auto result {static_cast<Conversion_Type>(val.full_significand())};
    auto expval {val.biased_exponent()};
    const auto abs_exp_val {detail::make_positive_unsigned(expval)};

    if (expval > 0)
    {
        if (expval > std::numeric_limits<Conversion_Type>::digits10 + 1)
        {
            return std::numeric_limits<TargetType>::max();
        }

        result *= detail::pow10<Conversion_Type>(static_cast<Conversion_Type>(expval));
    }
    else if (expval < 0)
    {
        if (abs_exp_val > std::numeric_limits<Conversion_Type>::digits10 + 1)
        {
            return static_cast<TargetType>(0);
        }

        result /= detail::pow10<Conversion_Type>(static_cast<Conversion_Type>(abs_exp_val));
    }

    BOOST_DECIMAL_IF_CONSTEXPR (std::is_signed<TargetType>::value)
    {
        result = static_cast<bool>(val.isneg()) ? static_cast<Conversion_Type>(detail::apply_sign(result)) : static_cast<Conversion_Type>(result);
    }

    return static_cast<TargetType>(result);
}

template <typename Decimal, typename TargetType>
constexpr auto to_integral_128(Decimal val) noexcept
    BOOST_DECIMAL_REQUIRES_TWO_RETURN(detail::is_decimal_floating_point_v, Decimal, detail::is_integral_v, TargetType, TargetType)
{
    constexpr Decimal max_target_type { (std::numeric_limits<TargetType>::max)() };
    constexpr Decimal min_target_type { (std::numeric_limits<TargetType>::min)() };
    constexpr Decimal one {1};

    if (isnan(val))
    {
        #if defined(__clang__) && __clang_major__ >= 20
        if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(val))
        #endif
        {
            errno = EINVAL;
        }

        return static_cast<TargetType>(std::numeric_limits<TargetType>::max());
    }

    // Anything in [0, 1) should be flushed to 0
    if (abs(val) < one)
    {
        return static_cast<TargetType>(0);
    }

    if (val > max_target_type || val < min_target_type)
    {
        #if defined(__clang__) && __clang_major__ >= 20
        if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(val))
        #endif
        {
            errno = ERANGE;
        }

        return static_cast<TargetType>(std::numeric_limits<TargetType>::max());
    }

    auto sig {val.full_significand()};
    auto expval {val.biased_exponent()};
    const auto abs_exp_val {detail::make_positive_unsigned(expval)};

    if (expval > 0)
    {
        if (expval > std::numeric_limits<int128::uint128_t>::digits)
        {
            return std::numeric_limits<TargetType>::max();
        }

        sig *= detail::pow10<int128::uint128_t>(expval);
    }
    else if (expval < 0)
    {
        if (abs_exp_val > std::numeric_limits<int128::uint128_t>::digits)
        {
            return static_cast<TargetType>(0);
        }

        sig /= detail::pow10<int128::uint128_t>(abs_exp_val);
    }

    auto result {static_cast<TargetType>(sig)};

    BOOST_DECIMAL_IF_CONSTEXPR (std::is_signed<TargetType>::value)
    {
        result = static_cast<bool>(val.isneg()) ? static_cast<TargetType>(detail::apply_sign(result)) :
                                                  static_cast<TargetType>(result);
    }

    return result;
}


#ifdef _MSC_VER
#  pragma warning(pop)
#endif

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_TO_INTEGRAL_HPP
