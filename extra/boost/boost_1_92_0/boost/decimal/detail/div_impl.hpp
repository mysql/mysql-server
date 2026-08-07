// Copyright 2023 - 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_DIV_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_DIV_IMPL_HPP

#include <boost/decimal/detail/attributes.hpp>
#include <boost/decimal/detail/add_impl.hpp>
#include <boost/decimal/detail/fenv_rounding.hpp>
#include <boost/decimal/detail/normalize.hpp>
#include <boost/decimal/detail/power_tables.hpp>
#include <boost/decimal/detail/components.hpp>
#include <boost/decimal/detail/u256.hpp>
#include "int128.hpp"

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>
#include <cstdint>
#endif

namespace boost {
namespace decimal {
namespace detail {

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4127) // Conditional expression is constant (pre-C++17 if-constexpr fallback)
#endif

namespace impl {

// Default-rounding gate. The inline round-half-to-even (RHTE) in the
// div_finalize_* helpers assumes the active rounding mode is
// fe_dec_to_nearest. Other modes route through the constructor handoff
// (which dispatches to fenv_round honoring the runtime mode).
template <typename Anchor>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto div_default_rounding(const Anchor& anchor) noexcept -> bool
{
    static_cast<void>(anchor);
    bool default_rounding {_boost_decimal_global_rounding_mode == rounding_mode::fe_dec_to_nearest};
    #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION
    if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(anchor))
    {
        default_rounding = (_boost_decimal_global_runtime_rounding_mode == rounding_mode::fe_dec_to_nearest);
    }
    #endif
    return default_rounding;
}

// Division finalization with inline round-half-to-even. The driver computes
// q = (lhs.sig * 10^p) / rhs.sig and r = (lhs.sig * 10^p) % rhs.sig where
// both operand significands are at full precision p. The natural remainder
// r encodes the exact sticky bit needed for correct RHTE.
//
// With both operands in [10^(p-1), 10^p), the quotient is in
// [10^(p-1), 10^(p+1)), i.e. exactly p or p+1 digits.
//
// Case 1 (q < 10^p): q already has p digits. Round up iff
//   2r > divisor                            (computed as r > divisor - r to
//                                            avoid overflow when divisor is
//                                            near the type's max)
//   OR (2r == divisor AND q is odd).
//
// Case 2 (q >= 10^p): q has p+1 digits, shrink by one digit. Let
// q' = q/10, r_q = q%10. The true fractional part is
// f = r_q/10 + r/(10*divisor). Round up iff
//   r_q > 5
//   OR (r_q == 5 AND r > 0)
//   OR (r_q == 5 AND r == 0 AND q' is odd).
//
// In either case the post-rounding carry q' == 10^p shifts down to 10^(p-1)
// and bumps the exponent by one more digit.

// d32/fast32 finalizer. Dividend = lhs.sig * 10^7 fits in uint64; quotient is
// at most 10^8 - 1 which fits in uint32; remainder is bounded by the divisor
// which fits in uint32 (rhs.sig < 10^7).
template <typename ReturnType, typename ExpType>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto div_finalize_u64(
    std::uint64_t q, std::uint64_t r, std::uint32_t divisor,
    ExpType result_exp, bool sign) noexcept -> ReturnType
{
    constexpr auto ten_to_p {pow10(static_cast<std::uint64_t>(detail::precision_v<ReturnType>))};
    constexpr auto ten_to_p_minus_1 {pow10(static_cast<std::uint64_t>(detail::precision_v<ReturnType> - 1))};

    int extra {0};

    if (q >= ten_to_p)
    {
        extra = 1;
        const auto r_q {static_cast<unsigned>(q % UINT64_C(10))};
        q /= UINT64_C(10);

        const bool round_up {r_q > 5U ||
                             (r_q == 5U && (r != UINT64_C(0) || (q & UINT64_C(1)) != UINT64_C(0)))};
        if (round_up)
        {
            ++q;
            if (BOOST_DECIMAL_UNLIKELY(q == ten_to_p))
            {
                q = ten_to_p_minus_1;
                ++extra;
            }
        }
    }
    else
    {
        const auto half_div {static_cast<std::uint64_t>(divisor) - r};
        const bool round_up {r > half_div || (r == half_div && (q & UINT64_C(1)) != UINT64_C(0))};
        if (round_up)
        {
            ++q;
            if (BOOST_DECIMAL_UNLIKELY(q == ten_to_p))
            {
                q = ten_to_p_minus_1;
                ++extra;
            }
        }
    }

    using sig_type = typename ReturnType::significand_type;
    return pack_in_range<ReturnType>(static_cast<sig_type>(q),
                                     result_exp + static_cast<ExpType>(extra),
                                     sign);
}

// d64/fast64 finalizer. Quotient and remainder both fit in uint64 because
// quotient is at most 10^17 - 1 < 2^57 and remainder is bounded by the
// uint64 divisor.
template <typename ReturnType, typename ExpType>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto div_finalize_u128(
    std::uint64_t q, std::uint64_t r, std::uint64_t divisor,
    ExpType result_exp, bool sign) noexcept -> ReturnType
{
    constexpr auto ten_to_p {pow10(static_cast<std::uint64_t>(detail::precision_v<ReturnType>))};
    constexpr auto ten_to_p_minus_1 {pow10(static_cast<std::uint64_t>(detail::precision_v<ReturnType> - 1))};

    int extra {0};

    if (q >= ten_to_p)
    {
        extra = 1;
        const auto r_q {static_cast<unsigned>(q % UINT64_C(10))};
        q /= UINT64_C(10);

        const bool round_up {r_q > 5U ||
                             (r_q == 5U && (r != UINT64_C(0) || (q & UINT64_C(1)) != UINT64_C(0)))};
        if (round_up)
        {
            ++q;
            if (BOOST_DECIMAL_UNLIKELY(q == ten_to_p))
            {
                q = ten_to_p_minus_1;
                ++extra;
            }
        }
    }
    else
    {
        const auto half_div {divisor - r};
        const bool round_up {r > half_div || (r == half_div && (q & UINT64_C(1)) != UINT64_C(0))};
        if (round_up)
        {
            ++q;
            if (BOOST_DECIMAL_UNLIKELY(q == ten_to_p))
            {
                q = ten_to_p_minus_1;
                ++extra;
            }
        }
    }

    using sig_type = typename ReturnType::significand_type;
    return pack_in_range<ReturnType>(static_cast<sig_type>(q),
                                     result_exp + static_cast<ExpType>(extra),
                                     sign);
}

// d128/fast128 finalizer. Quotient and remainder both fit in uint128 because
// quotient is at most 10^35 - 1 (< 2^117) and remainder is bounded by the
// uint128 divisor (< 10^34).
template <typename ReturnType, typename ExpType>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto div_finalize_u256(
    int128::uint128_t q, int128::uint128_t r, int128::uint128_t divisor,
    ExpType result_exp, bool sign) noexcept -> ReturnType
{
    constexpr auto ten_to_p {pow10(int128::uint128_t{static_cast<std::uint64_t>(detail::precision_v<ReturnType>)})};
    constexpr auto ten_to_p_minus_1 {pow10(int128::uint128_t{static_cast<std::uint64_t>(detail::precision_v<ReturnType> - 1)})};

    int extra {0};

    if (q >= ten_to_p)
    {
        extra = 1;
        const auto dr_q {divmod10(q)};
        const auto r_q {static_cast<unsigned>(dr_q.remainder)};
        q = dr_q.quotient;

        const bool round_up {r_q > 5U ||
                             (r_q == 5U && (r != int128::uint128_t{0U} || (q.low & UINT64_C(1)) != UINT64_C(0)))};
        if (round_up)
        {
            ++q;
            if (BOOST_DECIMAL_UNLIKELY(q == ten_to_p))
            {
                q = ten_to_p_minus_1;
                ++extra;
            }
        }
    }
    else
    {
        const auto half_div {divisor - r};
        const bool round_up {r > half_div || (r == half_div && (q.low & UINT64_C(1)) != UINT64_C(0))};
        if (round_up)
        {
            ++q;
            if (BOOST_DECIMAL_UNLIKELY(q == ten_to_p))
            {
                q = ten_to_p_minus_1;
                ++extra;
            }
        }
    }

    return pack_in_range<ReturnType>(q,
                                     result_exp + static_cast<ExpType>(extra),
                                     sign);
}

} // namespace impl

// d32/fast32 division driver. Accepts a decimal type or a components struct
// (both expose to_components()) and dispatches to the inline RHTE fast path
// when the active rounding mode is fe_dec_to_nearest. Non-default rounding
// modes fall back to the original wide-divide + constructor handoff so the
// constructor's coefficient_rounding can dispatch to fenv_round.
template <typename DecimalType, typename T>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto generic_div_impl(const T& lhs, const T& rhs) noexcept -> DecimalType
{
    auto lhs_c {lhs.to_components()};
    auto rhs_c {rhs.to_components()};

    detail::expand_significand<DecimalType>(lhs_c.sig, lhs_c.exp);
    detail::expand_significand<DecimalType>(rhs_c.sig, rhs_c.exp);

    const bool sign {lhs_c.sign != rhs_c.sign};

    if (BOOST_DECIMAL_UNLIKELY(lhs_c.sig == 0U))
    {
        using sig_type = typename decltype(lhs_c)::significand_type;
        return DecimalType{sig_type{0U}, lhs_c.exp - rhs_c.exp, sign};
    }

    if (BOOST_DECIMAL_UNLIKELY(!impl::div_default_rounding(lhs_c.sig)))
    {
        // Non-default rounding mode: hand the wide quotient to the
        // constructor's coefficient_rounding so fenv_round honors the
        // runtime mode. The wider offset preserves the original behavior.
        constexpr auto wide_offset {std::numeric_limits<std::uint64_t>::digits10 - precision_v<DecimalType>};
        constexpr auto wide_tens {pow10(static_cast<std::uint64_t>(wide_offset))};
        const auto wide_sig {static_cast<std::uint64_t>(lhs_c.sig) * wide_tens};
        const auto wide_q {wide_sig / static_cast<std::uint64_t>(rhs_c.sig)};
        const auto wide_exp {(lhs_c.exp - static_cast<int>(wide_offset)) - rhs_c.exp};
        return DecimalType{wide_q, wide_exp, sign};
    }

    constexpr auto ten_to_p {pow10(static_cast<std::uint64_t>(precision_v<DecimalType>))};
    const auto big_sig {static_cast<std::uint64_t>(lhs_c.sig) * ten_to_p};
    const auto divisor {static_cast<std::uint64_t>(rhs_c.sig)};
    const auto q {big_sig / divisor};
    const auto r {big_sig - q * divisor};
    const auto res_exp {(lhs_c.exp - static_cast<int>(precision_v<DecimalType>)) - rhs_c.exp};

    return impl::div_finalize_u64<DecimalType>(q, r, static_cast<std::uint32_t>(divisor), res_exp, sign);
}

// d64/fast64 division driver. Same structure as the d32 driver but uses
// uint128 for the pre-scaled dividend (lhs.sig * 10^16 overflows uint64).
// The quotient is at most 10^17 - 1 < 2^57 so it narrows safely to uint64
// for the finalizer.
template <typename DecimalType, typename T>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto d64_generic_div_impl(const T& lhs, const T& rhs, const bool sign) noexcept -> DecimalType
{
    using unsigned_int128_type = boost::int128::uint128_t;

    auto lhs_c {lhs.to_components()};
    auto rhs_c {rhs.to_components()};

    detail::expand_significand<DecimalType>(lhs_c.sig, lhs_c.exp);
    detail::expand_significand<DecimalType>(rhs_c.sig, rhs_c.exp);

    if (BOOST_DECIMAL_UNLIKELY(lhs_c.sig == 0U))
    {
        using sig_type = typename decltype(lhs_c)::significand_type;
        return DecimalType{sig_type{0U}, lhs_c.exp - rhs_c.exp, sign};
    }

    if (BOOST_DECIMAL_UNLIKELY(!impl::div_default_rounding(lhs_c.sig)))
    {
        constexpr auto wide_offset {std::numeric_limits<unsigned_int128_type>::digits10 - precision_v<DecimalType>};
        const auto wide_tens {pow10(static_cast<unsigned_int128_type>(wide_offset))};
        const auto wide_sig {static_cast<unsigned_int128_type>(lhs_c.sig) * wide_tens};
        const auto wide_q {wide_sig / static_cast<unsigned_int128_type>(rhs_c.sig)};
        const auto wide_exp {(lhs_c.exp - static_cast<int>(wide_offset)) - rhs_c.exp};
        return DecimalType{wide_q, wide_exp, sign};
    }

    constexpr auto ten_to_p {static_cast<unsigned_int128_type>(pow10(static_cast<std::uint64_t>(precision_v<DecimalType>)))};
    const auto big_sig {static_cast<unsigned_int128_type>(lhs_c.sig) * ten_to_p};
    const auto divisor {static_cast<std::uint64_t>(rhs_c.sig)};
    const auto q_wide {big_sig / static_cast<unsigned_int128_type>(divisor)};
    const auto r_wide {big_sig - q_wide * static_cast<unsigned_int128_type>(divisor)};
    const auto res_exp {(lhs_c.exp - static_cast<int>(precision_v<DecimalType>)) - rhs_c.exp};

    return impl::div_finalize_u128<DecimalType>(static_cast<std::uint64_t>(q_wide.low),
                                                static_cast<std::uint64_t>(r_wide.low),
                                                divisor,
                                                res_exp,
                                                sign);
}

// d128/fast128 division driver. The pre-scaled dividend lhs.sig * 10^34
// needs u256 because the product may reach ~10^68 (well above uint128 max).
// The quotient itself fits in uint128 (<= 10^35) so we narrow before the
// finalizer.
template <typename DecimalType, typename T>
BOOST_DECIMAL_CUDA_CONSTEXPR auto d128_generic_div_impl(const T& lhs, const T& rhs, const bool sign) noexcept -> DecimalType
{
    auto lhs_c {lhs.to_components()};
    auto rhs_c {rhs.to_components()};

    detail::expand_significand<DecimalType>(lhs_c.sig, lhs_c.exp);
    detail::expand_significand<DecimalType>(rhs_c.sig, rhs_c.exp);

    if (BOOST_DECIMAL_UNLIKELY(lhs_c.sig == int128::uint128_t{0U}))
    {
        return DecimalType{int128::uint128_t{0U}, lhs_c.exp - rhs_c.exp, sign};
    }

    if (BOOST_DECIMAL_UNLIKELY(!impl::div_default_rounding(lhs_c.sig)))
    {
        const auto wide_tens {pow10(int128::uint128_t{static_cast<std::uint64_t>(precision_v<DecimalType>)})};
        const auto wide_sig {detail::umul256(lhs_c.sig, wide_tens)};
        auto wide_q {wide_sig / rhs_c.sig};
        auto wide_exp {lhs_c.exp - rhs_c.exp - static_cast<int>(precision_v<DecimalType>)};

        if (wide_q[3] != 0U || wide_q[2] != 0U)
        {
            const auto sig_dig {detail::num_digits(wide_q)};
            const auto digit_delta {sig_dig - std::numeric_limits<int128::uint128_t>::digits10};
            wide_q /= pow10(int128::uint128_t{static_cast<std::uint64_t>(digit_delta)});
            wide_exp += digit_delta;
        }

        BOOST_DECIMAL_ASSERT((wide_q[3] | wide_q[2]) == 0U);
        return DecimalType{int128::uint128_t{wide_q[1], wide_q[0]}, wide_exp, sign};
    }

    constexpr auto ten_to_p {pow10(int128::uint128_t{static_cast<std::uint64_t>(precision_v<DecimalType>)})};
    const auto big_sig {detail::umul256(lhs_c.sig, ten_to_p)};
    const auto divisor {rhs_c.sig};
    const auto dr {impl::div_mod(big_sig, divisor)};

    const int128::uint128_t q {dr.quotient.bytes[1], dr.quotient.bytes[0]};
    const int128::uint128_t r {dr.remainder.bytes[1], dr.remainder.bytes[0]};
    const auto res_exp {lhs_c.exp - rhs_c.exp - static_cast<int>(precision_v<DecimalType>)};

    return impl::div_finalize_u256<DecimalType>(q, r, divisor, res_exp, sign);
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

} // namespace detail
} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_DIV_IMPL_HPP
