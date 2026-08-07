// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_FREXP10_HPP
#define BOOST_DECIMAL_DETAIL_FREXP10_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/normalize.hpp>
#include "../int128.hpp"
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>
#include <type_traits>
#endif

namespace boost {
namespace decimal {

namespace detail {

// Hopefully the compiler makes this NOOP for the fast case
template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetType, BOOST_DECIMAL_INTEGRAL T1, BOOST_DECIMAL_INTEGRAL T2>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto frexp10_normalize(T1&, T2&) noexcept -> std::enable_if_t<is_fast_type_v<TargetType>, void> {}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetType, BOOST_DECIMAL_INTEGRAL T1, BOOST_DECIMAL_INTEGRAL T2>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto frexp10_normalize(T1& sig, T2& exp) noexcept -> std::enable_if_t<!is_fast_type_v<TargetType>, void>
{
    detail::normalize<TargetType>(sig, exp);
}

} // namespace detail

// Returns the normalized significand and exponent to be cohort agnostic
// Returns num in the range
//   [1e06, 1e06 - 1] for decimal32_t
//   [1e15, 1e15 - 1] for decimal64_t
// If the conversion can not be performed returns UINT32_MAX and exp = 0
BOOST_DECIMAL_EXPORT template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
BOOST_DECIMAL_CUDA_CONSTEXPR auto frexp10(const T num, int* expptr) noexcept -> typename T::significand_type
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isinf(num) || isnan(num))
    {
        *expptr = 0;
        return (std::numeric_limits<typename T::significand_type>::max)();
    }
    #endif

    auto num_exp {num.biased_exponent()};
    auto num_sig {num.full_significand()};

    // Normalize the handling of zeros
    if (num_sig == 0U)
    {
        *expptr = 0;
        return 0U;
    }

    detail::frexp10_normalize<T>(num_sig, num_exp);

    *expptr = static_cast<int>(num_exp);

    return num_sig;
}

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_FREXP10_HPP
