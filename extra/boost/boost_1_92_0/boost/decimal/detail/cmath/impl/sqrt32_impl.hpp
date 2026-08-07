// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Copyright 2025 - 2026 Justin Zhu
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_IMPL_SQRT32_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_IMPL_SQRT32_IMPL_HPP

// ============================================================================
// decimal32 sqrt: SoftFloat f32_sqrt style with INTEGER arithmetic throughout
//
// Algorithm:
// 1. Caller passes gx in [1, 10); get sig_gx = gx * 10^6 as integer
// 2. Call approx_recip_sqrt32 → r_scaled ≈ 10^7 / sqrt(gx) (integer, ~24 bits)
// 3. Compute sig_z = sig_gx * r_scaled / 10^7 ≈ sqrt(gx) * 10^6
// 4. Newton correction using exact integer remainder
// 5. Final rounding check (integer)
// 6. Rescale by 10^(exp/2) and ×√10 if exp was odd
//
// Key improvement: ALL arithmetic is integer, no floating-point until final result
// ============================================================================

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

// sqrt for decimal32 (7 decimal digits) with pure integer arithmetic
template <typename T>
constexpr auto sqrt32_impl(T x, int exp10val) noexcept -> T
{
    constexpr int digits10 = std::numeric_limits<T>::digits10;
    static_assert(digits10 <= 7, "sqrt32_impl is for decimal32 (7 digits)");

    // Caller (sqrt_impl) already passes gx in [1, 10), no normalization needed
    T gx{x};

    // ---------- Convert to integer representation ----------
    // gx in [1, 10), sig_gx = gx * 10^6 in [10^6, 10^7)
    constexpr std::uint64_t scale6 = 1000000ULL;   // 10^6
    constexpr std::uint64_t scale7 = 10000000ULL;  // 10^7
    
    std::uint32_t sig_gx = static_cast<std::uint32_t>(gx * T{scale6});
    
    // ---------- Get 1/sqrt approximation using integer function ----------
    // r_scaled = approx_recip_sqrt32(sig_gx) ≈ 10^7 / sqrt(gx)
    // This has ~24 bits precision after internal Newton iterations
    std::uint32_t r_scaled = approx_recip_sqrt32(sig_gx, static_cast<unsigned int>(exp10val & 1));
    
    // ---------- Compute initial z = sqrt(gx) ----------
    // sig_z = sig_gx * r_scaled / 10^7
    //       = (gx * 10^6) * (10^7 / sqrt(gx)) / 10^7
    //       = gx * 10^6 / sqrt(gx) = sqrt(gx) * 10^6
    std::uint64_t product = static_cast<std::uint64_t>(sig_gx) * r_scaled;
    std::uint32_t sig_z = static_cast<std::uint32_t>(product / scale7);

    // Precompute target = sig_gx * 10^6 (avoids recomputing in Newton and rounding)
    const std::uint64_t target = static_cast<std::uint64_t>(sig_gx) * scale6;
    
    // ---------- Newton correction with exact integer remainder ----------
    // rem = target - sig_z² (exact integer)
    std::uint64_t z_squared = static_cast<std::uint64_t>(sig_z) * sig_z;
    std::int64_t rem = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(z_squared);
    
    // Newton correction: correction = rem / (2 * sig_z)
    if (rem != 0 && sig_z > 0)
    {
        std::int64_t correction = rem / (2 * static_cast<std::int64_t>(sig_z));
        sig_z = static_cast<std::uint32_t>(static_cast<std::int64_t>(sig_z) + correction);
        
        // Recompute remainder
        z_squared = static_cast<std::uint64_t>(sig_z) * sig_z;
        rem = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(z_squared);
    }

    // ---------- Final rounding check ----------
    // Ensure z² ≤ gx (z is a lower bound)
    if (rem < 0)
    {
        --sig_z;
    }
    
    // Convert back to decimal type
    T z{sig_z, -6};  // sig_z * 10^-6

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

#endif // BOOST_DECIMAL_DETAIL_CMATH_IMPL_SQRT32_IMPL_HPP
