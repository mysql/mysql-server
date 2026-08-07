// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Copyright 2025 - 2026 Justin Zhu
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_SQRT64_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_SQRT64_IMPL_HPP

// ============================================================================
// decimal64 sqrt: SoftFloat f64_sqrt style with INTEGER arithmetic throughout
//
// Algorithm:
// 1. Caller passes gx in [1, 10); get sig_gx = gx * 10^15 as integer
// 2. Call approx_recip_sqrt64 → r_scaled ≈ 10^16 / sqrt(gx) (integer, ~48 bits)
// 3. Compute sig_z = sig_gx * r_scaled / 10^16 ≈ sqrt(gx) * 10^15
// 4. Newton corrections using exact integer remainder (needs 128-bit)
// 5. Final rounding check (integer)
// 6. Rescale by 10^(exp/2) and ×√10 if exp was odd
//
// Key improvement: ALL arithmetic is integer, no floating-point until final result
// Requires: 128-bit integer support (__uint128_t or equivalent) for full precision
// ============================================================================

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/int128.hpp>
#include <boost/decimal/detail/cmath/impl/approx_recip_sqrt_impl.hpp>
#include <boost/decimal/detail/cmath/frexp10.hpp>
#include <boost/decimal/detail/remove_trailing_zeros.hpp>
#include <boost/decimal/numbers.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>
#include <cstdint>
#endif

namespace boost {
namespace decimal {
namespace detail {

// sqrt for decimal64 (16 decimal digits) with pure integer arithmetic
template <typename T>
constexpr auto sqrt64_impl(T x, int exp10val) noexcept -> T
{
    constexpr int digits10 = std::numeric_limits<T>::digits10;
    static_assert(digits10 > 7 && digits10 <= 16, "sqrt64_impl is for decimal64 (16 digits)");

    // Caller (sqrt_impl) already passes gx in [1, 10), no normalization needed
    T gx{x};

    // ---------- Convert to integer representation ----------
    // gx in [1, 10), sig_gx = gx * 10^15 in [10^15, 10^16)
    constexpr std::uint64_t scale15 = 1000000000000000ULL;   // 10^15
    constexpr std::uint64_t scale16 = 10000000000000000ULL;  // 10^16
    
    std::uint64_t sig_gx = static_cast<std::uint64_t>(gx * T{scale15});
    
    // ---------- Get 1/sqrt approximation using integer function ----------
    // r_scaled = approx_recip_sqrt64(sig_gx) ≈ 10^16 / sqrt(gx)
    // This has ~48 bits precision after internal Newton iterations
    std::uint64_t r_scaled = approx_recip_sqrt64(sig_gx, static_cast<unsigned int>(exp10val & 1));

    // ---------- Compute initial z = sqrt(gx) using 128-bit ----------
    // sig_z = sig_gx * r_scaled / 10^16
    //       = (gx * 10^15) * (10^16 / sqrt(gx)) / 10^16
    //       = gx * 10^15 / sqrt(gx) = sqrt(gx) * 10^15
    // Using int128::uint128_t for portability (works on all platforms including MSVC 32-bit)
    int128::uint128_t product = static_cast<int128::uint128_t>(sig_gx) * r_scaled;
    std::uint64_t sig_z = static_cast<std::uint64_t>(product / scale16);

    // Precompute target = sig_gx * 10^15 (avoids recomputing in each Newton iteration and rounding)
    const int128::uint128_t target = static_cast<int128::uint128_t>(sig_gx) * scale15;
    
    // ---------- Newton correction with exact integer remainder ----------
    // rem = target - sig_z² (exact 128-bit integer)
    {
        int128::uint128_t z_squared = static_cast<int128::uint128_t>(sig_z) * sig_z;
        int128::int128_t rem = static_cast<int128::int128_t>(target) - static_cast<int128::int128_t>(z_squared);
        
        if (rem != 0 && sig_z > 0)
        {
            int128::int128_t correction = rem / (2 * static_cast<int128::int128_t>(sig_z));
            sig_z = static_cast<std::uint64_t>(static_cast<int128::int128_t>(sig_z) + correction);
        }
    }
    
    // Second Newton correction for full precision
    {
        int128::uint128_t z_squared = static_cast<int128::uint128_t>(sig_z) * sig_z;
        int128::int128_t rem = static_cast<int128::int128_t>(target) - static_cast<int128::int128_t>(z_squared);
        
        if (rem != 0 && sig_z > 0)
        {
            int128::int128_t correction = rem / (2 * static_cast<int128::int128_t>(sig_z));
            sig_z = static_cast<std::uint64_t>(static_cast<int128::int128_t>(sig_z) + correction);
        }
    }
    
    // Final rounding check
    {
        int128::uint128_t z_squared = static_cast<int128::uint128_t>(sig_z) * sig_z;
        int128::int128_t rem = static_cast<int128::int128_t>(target) - static_cast<int128::int128_t>(z_squared);
        
        if (rem < 0)
        {
            --sig_z;
        }
    }
    
    // Convert back to decimal type
    T z{sig_z, -15};  // sig_z * 10^-15

    // ---------- Rescale: sqrt(x) = z × 10^(e/2), ×√10 when e odd ----------
    const int half_exp = (exp10val >= 0) ? (exp10val / 2) : ((exp10val - 1) / 2);
    if (half_exp != 0)
    {
        z *= T{1, half_exp};
    }
    if ((exp10val & 1) != 0)
    {
        z *= numbers::sqrt10_v<T>;
    }

    return z;
}

} // namespace detail
} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_IMPL_SQRT64_IMPL_HPP
