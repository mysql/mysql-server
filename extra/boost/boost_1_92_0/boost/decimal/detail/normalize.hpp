// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_NORMALIZE_HPP
#define BOOST_DECIMAL_DETAIL_NORMALIZE_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/integer_search_trees.hpp>
#include <boost/decimal/detail/fenv_rounding.hpp>
#include <boost/decimal/detail/attributes.hpp>
#include <boost/decimal/detail/remove_trailing_zeros.hpp>

namespace boost {
namespace decimal {
namespace detail {

#if defined(__GNUC__) && __GNUC__ == 7
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

// Converts the significand to full precision to remove the effects of cohorts
template <typename TargetDecimalType = decimal32_t, typename T1, typename T2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto normalize(T1& significand, T2& exp, bool sign = false) noexcept -> void
{
    constexpr auto target_precision {detail::precision_v<TargetDecimalType>};
    const auto digits {num_digits(significand)};

    if (digits < target_precision)
    {
        const auto zeros_needed {target_precision - digits};
        BOOST_DECIMAL_ASSERT(zeros_needed >= 0);
        significand *= pow10(static_cast<T1>(zeros_needed));
        exp -= zeros_needed;
    }
    else if (digits > target_precision)
    {
        auto biased_exp {static_cast<int>(exp) + detail::bias_v<TargetDecimalType>};
        detail::coefficient_rounding<TargetDecimalType>(significand, exp, biased_exp, sign, digits);
    }
}

#if defined(__GNUC__) && __GNUC__ == 7
#  pragma GCC diagnostic pop
#endif

// This is a branchless version of the above which is used for implementing basic operations,
// since we know that the values in the decimal type are never larger than target_precision.
// Fast path: when significand is already in the normalized range [10^(p-1), 10^p - 1]
// no expansion is needed and we skip the num_digits + pow10 multiply (~20 cycles saved).
// This is the dominant case for results of prior arithmetic that were packed at full precision.
template <typename TargetDecimalType, typename T1, typename T2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto expand_significand(T1& significand, T2& exp) noexcept -> void
{
    constexpr auto target_precision {detail::precision_v<TargetDecimalType>};
    constexpr T1 min_normal_sig {static_cast<T1>(pow10(static_cast<T1>(target_precision - 1)))};

    if (significand >= min_normal_sig)
    {
        // Already has exactly target_precision digits; expansion is a no-op.
        return;
    }

    const auto digits {num_digits(significand)};
    const auto zeros_needed {target_precision - digits};
    significand *= pow10(static_cast<T1>(zeros_needed));
    exp -= zeros_needed;
}

} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_NORMALIZE_HPP
