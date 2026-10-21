// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Copyright 2025 - 2026 Justin Zhu
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_SQRT128_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_SQRT128_IMPL_HPP

// ============================================================================
// decimal128 sqrt: SoftFloat f128_sqrt style with INTEGER remainder arithmetic
//
// Algorithm (inspired by SoftFloat f128_sqrt):
// 1. Caller passes gx in [1, 10); get sig_gx = gx * 10^33 as u256
// 2. Use approx_recip_sqrt64 to get initial r ≈ 10^16/sqrt(gx) (~48 bits)
// 3. Compute sig_z = sig_gx * r / scale as initial sqrt approximation
// 4. Remainder-based refinement using u256 arithmetic:
//    rem = sig_gx * scale - sig_z²
//    q = rem / (2 * sig_z)  (next correction)
//    sig_z = sig_z + q
// 5. Repeat refinement to reach 34 decimal digits
// 6. Final rounding check (ensure sig_z² ≤ sig_gx * scale)
// 7. Rescale by 10^(exp/2) and ×√10 if exp was odd
//
// Key: ALL arithmetic uses u256/i256_sub, no floating-point rounding errors
// ============================================================================

#include <boost/decimal/detail/cmath/impl/approx_recip_sqrt_impl.hpp>
#include <boost/decimal/detail/cmath/frexp10.hpp>
#include <boost/decimal/detail/remove_trailing_zeros.hpp>
#include <boost/decimal/detail/u256.hpp>
#include <boost/decimal/detail/i256.hpp>
#include <boost/decimal/numbers.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>
#include <cstdint>
#endif

namespace boost {
namespace decimal {
namespace detail {

// sqrt for decimal128 (34 decimal digits) with u256 integer arithmetic
template <typename T>
constexpr auto sqrt128_impl(T x, int exp10val) noexcept -> T
{
    constexpr int digits10 = std::numeric_limits<T>::digits10;
    static_assert(digits10 > 16, "sqrt128_impl is for decimal128 (34 digits)");

    // Caller (sqrt_impl) already passes gx in [1, 10), no normalization needed
    T gx{x};

    // ---------- Convert to u256 integer representation ----------
    // Use frexp10 to get the exact significand from gx (no floating-point loss)
    // gx is in [1, 10) with a 34-digit significand
    
    // Scale constants
    constexpr std::uint64_t scale17 = 100000000000000000ULL;  // 10^17
    constexpr std::uint64_t scale16 = 10000000000000000ULL;   // 10^16
    // scale33 = 10^33 = 10^17 * 10^16, 64×64→128 suffices
    const int128::uint128_t scale33_128{static_cast<int128::uint128_t>(scale17) * scale16};
    
    // ---------- Get exact significand using frexp10 ----------
    // frexp10 returns the full 34-digit significand directly from decimal128
    int gx_exp{};
    auto gx_sig = frexp10(gx, &gx_exp);  // gx_sig is uint128, in [10^33, 10^34)
    
    // gx = gx_sig * 10^gx_exp, and gx is in [1, 10)
    // So gx_exp = -(digits10-1) = -33; gx_sig * 10^33 is our scaled significand
    
    // Get high 16 digits for initial approximation
    // sig_gx_approx = gx * 10^15 = gx_sig / 10^18
    constexpr std::uint64_t scale18 = 1000000000000000000ULL;  // 10^18
    std::uint64_t sig_gx_approx = static_cast<std::uint64_t>(gx_sig / scale18);
    
    // ---------- Get 1/sqrt approximation ----------
    std::uint64_t r_scaled = approx_recip_sqrt64(sig_gx_approx, 0);
    
    // ---------- Compute initial sig_z = sig_gx * r / 10^16 ----------
    // sig_z ≈ sqrt(gx) * 10^33
    // sig_z = sig_gx * r_scaled / 10^16
    // r_scaled is 64-bit; use mul128_by_64 (SoftFloat-style) instead of full umul256
    u256 sig_z = mul128_by_64(gx_sig, r_scaled) / scale16;

    // Precompute target = sig_gx * 10^33 (avoids recomputing in each Newton iteration)
    const u256 target = umul256(gx_sig, scale33_128);
    
    // ---------- Newton corrections using u256 ----------
    // Newton: sig_z_new = sig_z + (sig_gx * 10^33 - sig_z²) / (2 * sig_z)
    // Note: sig_gx is scaled by 10^33, sig_z² is scaled by 10^66
    // So we compare sig_gx * 10^33 vs sig_z²
    
    // First Newton iteration
    // sig_z < sqrt(10)*10^33 always fits in 128 bits → use umul256 (4 muls) not u256×u256 (64 muls)
    {
        u256 sig_z_sq = umul256(static_cast<int128::uint128_t>(sig_z), static_cast<int128::uint128_t>(sig_z));
        
        u256 rem_abs;
        bool is_neg = i256_sub(target, sig_z_sq, rem_abs);
        
        // correction = rem / (2 * sig_z)
        u256 divisor = sig_z + sig_z;  // 2 * sig_z
        u256 correction = rem_abs / divisor;
        
        if (is_neg)
        {
            u256 new_sig_z;
            i256_sub(sig_z, correction, new_sig_z);
            sig_z = new_sig_z;
        }
        else
        {
            sig_z = sig_z + correction;
        }
    }
    
    // Second Newton iteration
    {
        u256 sig_z_sq = umul256(static_cast<int128::uint128_t>(sig_z), static_cast<int128::uint128_t>(sig_z));
        
        u256 rem_abs;
        bool is_neg = i256_sub(target, sig_z_sq, rem_abs);
        
        u256 divisor = sig_z + sig_z;
        u256 correction = rem_abs / divisor;
        
        if (is_neg)
        {
            u256 new_sig_z;
            i256_sub(sig_z, correction, new_sig_z);
            sig_z = new_sig_z;
        }
        else
        {
            sig_z = sig_z + correction;
        }
    }
    
    // Third Newton iteration for full precision
    {
        u256 sig_z_sq = umul256(static_cast<int128::uint128_t>(sig_z), static_cast<int128::uint128_t>(sig_z));
        
        u256 rem_abs;
        bool is_neg = i256_sub(target, sig_z_sq, rem_abs);
        
        u256 divisor = sig_z + sig_z;
        u256 correction = rem_abs / divisor;
        
        if (is_neg)
        {
            u256 new_sig_z;
            i256_sub(sig_z, correction, new_sig_z);
            sig_z = new_sig_z;
        }
        else
        {
            sig_z = sig_z + correction;
        }
    }

    // ---------- Final rounding (round-to-nearest) ----------
    // Find sig_z such that sig_z is the closest integer to sqrt(target)
    // First ensure sig_z² ≤ target, then check if sig_z+1 is closer
    {
        u256 sig_z_sq = umul256(static_cast<int128::uint128_t>(sig_z), static_cast<int128::uint128_t>(sig_z));
        
        // Step 1: If sig_z² > target, decrement until sig_z² ≤ target
        while (sig_z_sq > target)
        {
            u256 one{1};
            u256 new_sig_z;
            i256_sub(sig_z, one, new_sig_z);
            sig_z = new_sig_z;
            sig_z_sq = umul256(static_cast<int128::uint128_t>(sig_z), static_cast<int128::uint128_t>(sig_z));
        }
        
        // Step 2: Round-to-nearest check
        // If (sig_z + 0.5)² < target, then sig_z+1 is closer
        // Equivalent: sig_z² + sig_z + 0.25 < target
        // Since we work with integers: if target - sig_z² > sig_z, round up
        u256 rem;
        i256_sub(target, sig_z_sq, rem);  // rem = target - sig_z², guaranteed non-negative
        
        if (rem > sig_z)
        {
            u256 one{1};
            sig_z = sig_z + one;
        }
    }
    
    // ---------- Convert back to decimal type ----------
    // sig_z is scaled by 10^33, so z = sig_z * 10^-33
    // Extract high and low parts: sig_z = sig_z_hi * 10^17 + sig_z_lo
    u256 scale17_u256{scale17};
    u256 sig_z_hi_256 = sig_z / scale17_u256;
    u256 sig_z_lo_256 = sig_z % scale17_u256;
    
    std::uint64_t sig_z_hi = static_cast<std::uint64_t>(sig_z_hi_256);
    std::uint64_t sig_z_lo = static_cast<std::uint64_t>(sig_z_lo_256);
    
    // z = (sig_z_hi * 10^17 + sig_z_lo) * 10^-33
    //   = sig_z_hi * 10^-16 + sig_z_lo * 10^-33
    T z = T{sig_z_hi, -16} + T{sig_z_lo, -33};

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

#endif // BOOST_DECIMAL_DETAIL_CMATH_IMPL_SQRT128_IMPL_HPP
