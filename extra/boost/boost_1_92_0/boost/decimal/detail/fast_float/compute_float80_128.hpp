// Copyright 2020-2023 Daniel Lemire
// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_FAST_FLOAT_COMPUTE_FLOAT80_128_HPP
#define BOOST_DECIMAL_DETAIL_FAST_FLOAT_COMPUTE_FLOAT80_128_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/bit_layouts.hpp>
#include <boost/decimal/detail/apply_sign.hpp>
#include <boost/decimal/detail/integer_search_trees.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <array>
#include <climits>
#include <limits>
#include <cmath>
#endif

namespace boost {
namespace decimal {
namespace detail {
namespace fast_float {

BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE std::array<long double, 56> powers_of_ten_ld = {
        1e0L, 1e1L, 1e2L, 1e3L, 1e4L, 1e5L, 1e6L,
        1e7L, 1e8L, 1e9L, 1e10L, 1e11L, 1e12L, 1e13L,
        1e14L, 1e15L, 1e16L, 1e17L, 1e18L, 1e19L, 1e20L,
        1e21L, 1e22L, 1e23L, 1e24L, 1e25L, 1e26L, 1e27L,
        1e28L, 1e29L, 1e30L, 1e31L, 1e32L, 1e33L, 1e34L,
        1e35L, 1e36L, 1e37L, 1e38L, 1e39L, 1e40L, 1e41L,
        1e42L, 1e43L, 1e44L, 1e45L, 1e46L, 1e47L, 1e48L,
        1e49L, 1e50L, 1e51L, 1e52L, 1e53L, 1e54L, 1e55L
};

template <typename Unsigned_Integer>
constexpr auto fast_path(const std::int64_t q, const Unsigned_Integer &w, bool negative) noexcept -> long double
{
    // The general idea is as follows.
    // if 0 <= s <= 2^64 and if 10^0 <= p <= 10^27
    // Both s and p can be represented exactly
    // because of this s*p and s/p will produce
    // correctly rounded values

    auto ld = static_cast<long double>(w);

    if (q < 0)
    {
        ld /= powers_of_ten_ld[static_cast<std::size_t>(-q)];
    }
    else
    {
        ld *= powers_of_ten_ld[static_cast<std::size_t>(q)];
    }

    if (negative)
    {
        ld = -ld;
    }

    return ld;
}

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

template <typename Unsigned_Integer>
constexpr auto compute_float80_128(std::int64_t q, const Unsigned_Integer &w,
                                   const bool negative, bool &success) noexcept -> long double
{
    // GLIBC uses 2^-16444 but MPFR uses 2^-16445 as the smallest subnormal value for 80 bit
    // 39 is the max number of digits in an builtin_uint128_t
    constexpr auto smallest_power {-4951 - 39};
    constexpr auto largest_power {4932};

    // We start with a fast path
    // It is an extension of what was described in Clinger WD.
    // How to read floating point numbers accurately.
    // ACM SIGPLAN Notices. 1990
    // https://dl.acm.org/doi/pdf/10.1145/93542.93557
    constexpr auto clinger_max_exp {BOOST_DECIMAL_LDBL_BITS == 80 ? 27 : 48};   // NOLINT : Only changes by platform
    constexpr auto clinger_min_exp {BOOST_DECIMAL_LDBL_BITS == 80 ? -34 : -55}; // NOLINT

    constexpr std::uint32_t shift_length {sizeof(Unsigned_Integer) * CHAR_BIT - 10};

    if (clinger_min_exp <= q && q <= clinger_max_exp && w <= static_cast<Unsigned_Integer>(1) << shift_length)
    {
        success = true;
        return fast_path(q, w, negative);
    }

    if (w == 0U || q < smallest_power)
    {
        success = true;
        return negative ? -0.0L : 0.0L;
    }
    else if (q > largest_power)
    {
        success = true;
        return negative ? -HUGE_VALL : HUGE_VALL;
    }

    // We take the best guess
    success = true;
    auto ld {static_cast<long double>(w)};
    const auto sig_num_dig {detail::num_digits(w)};
    if (sig_num_dig > std::numeric_limits<long double>::digits10)
    {
        q += sig_num_dig - std::numeric_limits<long double>::digits10 - 1;
    }

    // Calculate (b ^ p) using the ladder method for powers.

    long double result {1};
    long double y {10};

    std::uint64_t p_local = detail::make_positive_unsigned(q);

    for (;;)
    {
        const auto do_power_multiply {(p_local & 1U) != 0U};

        if (do_power_multiply)
        {
            result *= y;
        }

        p_local >>= 1U;

        if (p_local == 0U)
        {
            break;
        }

        y *= y;
    }

    if (q < 0)
    {
        ld /= result;
    }
    else
    {
        ld *= result;
    }

    if (BOOST_DECIMAL_UNLIKELY(ld == std::numeric_limits<long double>::infinity()))
    {
        success = false;
        ld = 0.0L;
    }

    return ld;
}

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif


} //namespace fast_float
} //namespace detail
} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_FAST_FLOAT_COMPUTE_FLOAT80_128_HPP
