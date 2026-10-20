// Copyright 2025 - 2026 Justin Zhu
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_APPROX_RECIP_SQRT_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_APPROX_RECIP_SQRT_IMPL_HPP

// ============================================================================
// Integer-based 1/sqrt(x) approximation with Newton refinement
//
// Adapted from SoftFloat's softfloat_approxRecipSqrt32_1 for base-10.
//
// Key differences from binary SoftFloat:
// - Input range is [1, 10) instead of [1, 4)
// - Uses 90-entry table (step=0.1) instead of 16-entry
// - Scaling is powers of 10 instead of powers of 2
//
// Algorithm:
// 1. Table lookup + linear interpolation -> r0 (~10 bits)
// 2. Newton refinement: r = r0 * (1 + sigma/2 + 3*sigma^2/8) where sigma = 1 - x*r0^2
// 3. Second Newton step for full precision
//
// Precision: ~24 bits for decimal32, ~48 bits for decimal64
// ============================================================================

#include <boost/decimal/detail/cmath/impl/sqrt_lookup.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cstdint>
#endif

namespace boost {
namespace decimal {
namespace detail {

// ============================================================================
// approx_recip_sqrt32: for decimal32 (7 digits, ~24 bits)
//
// Input:
//   sig: significand in [10^6, 10^7) representing value x in [1, 10)
//   odd_exp: unused for decimal (kept for API compatibility)
//
// Output:
//   Approximation of 1/sqrt(x) scaled by 10^7, range [3162278, 10000000]
//
// Design rationale (SoftFloat-aligned):
// - Working scale R ~ 10^5/sqrt(x) chosen so sig*R^2 <= 10^17 (fits 64-bit)
// - sigma = (10^16 - sig*R^2) / 10^16 computed as integer
// - Newton: R_new = R + R*sigma/2, with sigma scaled by 10^16
// - Two iterations for full 7-digit precision
// - Final conversion to 10^7 scale via R * 100
// ============================================================================
inline constexpr std::uint32_t approx_recip_sqrt32(std::uint32_t sig, unsigned int /* odd_exp */) noexcept
{
    // sig is in [10^6, 10^7), representing x = sig/10^6 in [1, 10)
    
    // ---- Step 1: Table lookup with linear interpolation ----
    // 90-entry table covers [1.0, 9.9] with step 0.1
    // index = floor((x - 1) * 10) = sig/10^5 - 10
    
    int index = static_cast<int>(sig / 100000U) - 10;
    if (index < 0) index = 0;
    if (index >= sqrt_lookup::table_size) index = sqrt_lookup::table_size - 1;
    
    // Interpolation factor eps in [0, 65536)
    // eps = (sig - base) / 10^5 * 65536
    // Use fixed-point: 65536/100000 ~ 42950/65536 to avoid division
    std::uint32_t base_sig = static_cast<std::uint32_t>(index + 10) * 100000U;
    std::uint32_t sig_in_bin = sig - base_sig;
    std::uint32_t eps16 = (sig_in_bin * 42950U) >> 16;
    
    // r0_scaled ~ 10^16 / sqrt(x) from table
    std::uint64_t r0_scaled = sqrt_lookup::recip_sqrt_k0s[index]
        - ((sqrt_lookup::recip_sqrt_k1s[index] * static_cast<std::uint64_t>(eps16)) >> 16);
    
    // ---- Step 2: Convert to working scale ----
    // R ~ 10^5 / sqrt(x), range [31623, 100000]
    // This ensures sig * R^2 <= 10^17, safely within 64-bit
    std::uint32_t r_val = static_cast<std::uint32_t>(r0_scaled / 100000000000ULL);
    
    // ---- Step 3: First Newton iteration ----
    // Target: sig * R^2 = 10^16 when R is accurate
    // sigma_16 = 10^16 - sig * R^2 (sigma scaled by 10^16)
    // R_new = R + R * sigma_16 / (2 * 10^16)
    
    constexpr std::uint64_t target16 = 10000000000000000ULL;  // 10^16
    
    std::uint64_t r_sq = static_cast<std::uint64_t>(r_val) * r_val;
    std::uint64_t sig_r_sq = static_cast<std::uint64_t>(sig) * r_sq;
    std::int64_t sigma_16 = static_cast<std::int64_t>(target16) - static_cast<std::int64_t>(sig_r_sq);
    
    // Compute correction = R * sigma_16 / (2 * 10^16)
    // Simplified: correction = (R * (sigma_16 / 10^8)) / (2 * 10^8)
    std::int64_t sigma_8 = sigma_16 / 100000000LL;
    std::int64_t correction = (static_cast<std::int64_t>(r_val) * sigma_8) / 200000000LL;
    r_val = static_cast<std::uint32_t>(static_cast<std::int64_t>(r_val) + correction);
    
    // ---- Step 4: Second Newton iteration ----
    r_sq = static_cast<std::uint64_t>(r_val) * r_val;
    sig_r_sq = static_cast<std::uint64_t>(sig) * r_sq;
    sigma_16 = static_cast<std::int64_t>(target16) - static_cast<std::int64_t>(sig_r_sq);
    sigma_8 = sigma_16 / 100000000LL;
    correction = (static_cast<std::int64_t>(r_val) * sigma_8) / 200000000LL;
    r_val = static_cast<std::uint32_t>(static_cast<std::int64_t>(r_val) + correction);
    
    // ---- Step 5: Convert to output scale ----
    // R is 10^5/sqrt(x), output is 10^7/sqrt(x)
    std::uint32_t result = r_val * 100U;
    
    // Clamp to valid range [10^7/sqrt(10), 10^7]
    if (result < 3162278U) result = 3162278U;
    if (result > 10000000U) result = 10000000U;
    
    return result;
}

// ============================================================================
// approx_recip_sqrt64: for decimal64 (16 digits, ~53 bits)
//
// Input:
//   sig: significand in [10^15, 10^16) representing value x in [1, 10)
//   odd_exp: unused for decimal (kept for API compatibility)
//
// Output:
//   Approximation of 1/sqrt(x) scaled by 10^16, range [3162277660168379, 10^16]
//
// Design rationale (SoftFloat-aligned):
// - Table gives r0 ~ 10^16/sqrt(x) with ~10 bits precision
// - Newton iteration doubles precision each step
// - Three iterations: 10 -> 20 -> 40 -> 80 bits (exceeds 53-bit requirement)
//
// Key formula derivation:
// - x = sig / 10^15, r0 ~ 10^16 / sqrt(x)
// - When accurate: sig * r0^2 / 10^47 = 1, i.e., sig * r0^2 = 10^47
// - We compute y = (sig / 10^8) * (r0^2 / 10^24), target = 10^15
// - sigma_15 = 10^15 - y
// - delta_r0 = r0 * sigma_15 / (2 * 10^15) ~ (r0/10^8) * (sigma_15/10^8) * 5
// ============================================================================
inline constexpr std::uint64_t approx_recip_sqrt64(std::uint64_t sig, unsigned int /* odd_exp */) noexcept
{
    // sig is in [10^15, 10^16), representing x = sig/10^15 in [1, 10)
    
    // ---- Step 1: Table lookup with linear interpolation ----
    int index = static_cast<int>(sig / 100000000000000ULL) - 10;
    if (index < 0) index = 0;
    if (index >= sqrt_lookup::table_size) index = sqrt_lookup::table_size - 1;
    
    // Interpolation factor eps in [0, 65536)
    // Fixed-point approximation to avoid division
    // Cast via unsigned to avoid -Wsign-conversion (index is clamped to [0, table_size-1])
    std::uint64_t base_sig = static_cast<std::uint64_t>(static_cast<unsigned int>(index) + 10U) * 100000000000000ULL;
    std::uint64_t sig_in_bin = sig - base_sig;
    // 65536 / 10^14 ~ 6.5536e-10 ~ 2814749767 / 2^52
    // Simplified: (sig_in_bin * 2815) >> 42 ~ sig_in_bin * 65536 / 10^14
    std::uint32_t eps16 = static_cast<std::uint32_t>((sig_in_bin * 2815ULL) >> 42);
    
    // r0 ~ 10^16 / sqrt(x) from table
    std::uint64_t r0 = sqrt_lookup::recip_sqrt_k0s[index]
        - ((sqrt_lookup::recip_sqrt_k1s[index] * static_cast<std::uint64_t>(eps16)) >> 16);
    
    // ---- Step 2: Newton iterations ----
    // Newton: r_new = r + r * (1 - x*r^2) / 2
    //
    // Working formula:
    // y = (sig / 10^8) * (r0^2 / 10^24) represents sig * r0^2 / 10^32
    // When r0 is accurate: y = 10^15 (since sig * r0^2 = 10^47 => /10^32 = 10^15)
    // sigma_15 = 10^15 - y
    // delta_r0 = r0 * sigma_15 / (2 * 10^15)
    //          ~ (r0_hi * sigma_hi) * 5  where r0_hi = r0/10^8, sigma_hi = sigma_15/10^8
    
    constexpr std::uint64_t target = 1000000000000000ULL;  // 10^15
    
    for (int iter = 0; iter < 3; ++iter)
    {
        // r0^2 / 10^24 = (r0 / 10^8)^2 / 10^8
        std::uint64_t r0_hi = r0 / 100000000ULL;
        std::uint64_t r0_lo = r0 % 100000000ULL;
        
        // Include cross term for better precision:
        // r0^2 / 10^24 = r0_hi^2 / 10^8 + 2*r0_hi*r0_lo / 10^16
        std::uint64_t r_sq_term1 = (r0_hi * r0_hi) / 100000000ULL;
        std::uint64_t r_sq_term2 = (2 * r0_hi * r0_lo) / 10000000000000000ULL;
        std::uint64_t r_sq_24 = r_sq_term1 + r_sq_term2;
        
        // y = (sig / 10^8) * r_sq_24
        std::uint64_t sig_8 = sig / 100000000ULL;
        std::uint64_t y = sig_8 * r_sq_24;
        
        // sigma_15 = 10^15 - y
        std::int64_t sigma_15 = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(y);
        
        // delta_r0 = r0 * sigma_15 / (2 * 10^15)
        //          = (r0 / 10^8) * (sigma_15 / 10^8) * 10^16 / (2 * 10^15)
        //          = (r0_hi * sigma_hi) * 5
        std::int64_t sigma_hi = sigma_15 / 100000000LL;
        std::int64_t delta_r0 = static_cast<std::int64_t>(r0_hi) * sigma_hi * 5;
        
        r0 = static_cast<std::uint64_t>(static_cast<std::int64_t>(r0) + delta_r0);
    }
    
    // Clamp to valid range [10^16/sqrt(10), 10^16]
    if (r0 < 3162277660168379ULL) r0 = 3162277660168379ULL;
    if (r0 > 10000000000000000ULL) r0 = 10000000000000000ULL;
    
    return r0;
}

} // namespace detail
} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_IMPL_APPROX_RECIP_SQRT_IMPL_HPP
