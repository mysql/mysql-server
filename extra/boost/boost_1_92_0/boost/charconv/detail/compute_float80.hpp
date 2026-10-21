// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_CHARCONV_DETAIL_COMPUTE_FLOAT80_HPP
#define BOOST_CHARCONV_DETAIL_COMPUTE_FLOAT80_HPP

#include <boost/charconv/detail/config.hpp>
#include <boost/charconv/detail/emulated128.hpp>
#include <boost/charconv/detail/bit_layouts.hpp>
#include <system_error>
#include <type_traits>
#include <limits>
#include <cstdint>
#include <cmath>
#include <climits>
#include <cfloat>

#ifdef BOOST_CHARCONV_DEBUG_FLOAT128
#include <iostream>
#include <iomanip>
#include <boost/charconv/detail/to_chars_integer_impl.hpp>
#endif

namespace boost { namespace charconv { namespace detail {

#if BOOST_CHARCONV_LDBL_BITS > 64

static constexpr long double powers_of_ten_ld[] = {
    1e0L,  1e1L,  1e2L,  1e3L,  1e4L,  1e5L,  1e6L,
    1e7L,  1e8L,  1e9L,  1e10L, 1e11L, 1e12L, 1e13L,
    1e14L, 1e15L, 1e16L, 1e17L, 1e18L, 1e19L, 1e20L,
    1e21L, 1e22L, 1e23L, 1e24L, 1e25L, 1e26L, 1e27L,
    1e28L, 1e29L, 1e30L, 1e31L, 1e32L, 1e33L, 1e34L,
    1e35L, 1e36L, 1e37L, 1e38L, 1e39L, 1e40L, 1e41L,
    1e42L, 1e43L, 1e44L, 1e45L, 1e46L, 1e47L, 1e48L,
    1e49L, 1e50L, 1e51L, 1e52L, 1e53L, 1e54L, 1e55L
};

template <typename ResultType, typename Unsigned_Integer, typename ArrayPtr>
inline ResultType fast_path(std::int64_t q, Unsigned_Integer w, bool negative, ArrayPtr table) noexcept
{
    // The general idea is as follows.
    // if 0 <= s <= 2^64 and if 10^0 <= p <= 10^27
    // Both s and p can be represented exactly
    // because of this s*p and s/p will produce
    // correctly rounded values

    auto ld = static_cast<ResultType>(w);

    if (q < 0)
    {
        ld /= table[-q];
    }
    else
    {
        ld *= table[q];
    }

    if (negative)
    {
        ld = -ld;
    }

    return ld;
}

template <typename ResultType, typename Unsigned_Integer>
inline ResultType compute_float80(std::int64_t q, Unsigned_Integer w, bool negative, std::errc& success) noexcept
{
    // GLIBC uses 2^-16444 but MPFR uses 2^-16445 as the smallest subnormal value for 80 bit
    // 39 is the max number of digits in an uint128_t
    static constexpr auto smallest_power = -4951 - 39;
    static constexpr auto largest_power = 4932;

    // We start with a fast path
    // It is an extension of what was described in Clinger WD.
    // How to read floating point numbers accurately.
    // ACM SIGPLAN Notices. 1990
    // https://dl.acm.org/doi/pdf/10.1145/93542.93557
    static constexpr auto clinger_max_exp = BOOST_CHARCONV_LDBL_BITS == 80 ? 27 : 48;   // NOLINT : Only changes by platform
    static constexpr auto clinger_min_exp = BOOST_CHARCONV_LDBL_BITS == 80 ? -34 : -55; // NOLINT

    if (clinger_min_exp <= q && q <= clinger_max_exp && w <= static_cast<Unsigned_Integer>(1) << 113)
    {
        success = std::errc();
        return fast_path<ResultType>(q, w, negative, powers_of_ten_ld);
    }

    if (w == 0)
    {
        success = std::errc();
        return negative ? -0.0L : 0.0L;
    }
    else if (q > largest_power)
    {
        success = std::errc::result_out_of_range;
        return negative ? -HUGE_VALL : HUGE_VALL;
    }
    else if (q < smallest_power)
    {
        success = std::errc::result_out_of_range;
        return negative ? -0.0L : 0.0L;
    }

    success = std::errc::not_supported;
    return 0;
}

#endif // BOOST_CHARCONV_LDBL_BITS > 64

}}} // Namespaces

#endif // BOOST_CHARCONV_DETAIL_COMPUTE_FLOAT80_HPP
