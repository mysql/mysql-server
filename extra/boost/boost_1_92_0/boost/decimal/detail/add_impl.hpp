// Copyright 2023 - 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_ADD_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_ADD_IMPL_HPP

#include <boost/decimal/detail/attributes.hpp>
#include <boost/decimal/detail/apply_sign.hpp>
#include <boost/decimal/detail/fenv_rounding.hpp>
#include <boost/decimal/detail/components.hpp>
#include <boost/decimal/detail/power_tables.hpp>
#include <boost/decimal/detail/promotion.hpp>
#include <boost/decimal/detail/is_power_of_10.hpp>
#include <boost/decimal/detail/i256.hpp>
#include "int128.hpp"

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cstdint>
#include <type_traits>
#endif

namespace boost {
namespace decimal {
namespace detail {

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4127) // Conditional expression is constant
#endif

// Returns the low 64 bits of an integer for parity testing. Specialized so the
// templated kernel below can use one expression regardless of SigType width.
BOOST_DECIMAL_CUDA_CONSTEXPR inline auto low64(std::uint64_t v) noexcept -> std::uint64_t { return v; }
BOOST_DECIMAL_CUDA_CONSTEXPR inline auto low64(const int128::uint128_t& v) noexcept -> std::uint64_t { return v.low; }

// Forward declarations of the per-type IEEE direct-pack helpers (defined in
// decimal{32,64,128}_t.hpp after the bit-mask constants). These skip the
// generic constructor's bounds check + dead-branch handling for known-in-range
// inputs, saving ~5-10 cycles per call.
template <typename T1, typename T2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto direct_pack_d32(T1 coeff, T2 exp, bool sign) noexcept -> decimal32_t;
template <typename T1, typename T2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto direct_pack_d64(T1 coeff, T2 exp, bool sign) noexcept -> decimal64_t;
template <typename T2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto direct_pack_d128(int128::uint128_t coeff, T2 exp, bool sign) noexcept -> decimal128_t;

// Dispatch helper: for IEEE return types use the corresponding direct_pack
// helper if (exp + bias) is in the valid biased-exponent range; otherwise fall
// back to the regular constructor which handles overflow-to-infinity and
// subnormal flushing/re-canonicalization. For fast types always uses the
// regular constructor.
//
// SFINAE-disjoint overloads rather than an `if constexpr` chain because
// BOOST_DECIMAL_IF_CONSTEXPR falls back to plain `if` in C++14, which would
// force the d128 branch's `direct_pack_d128` body to be type-checked from
// add_impl.hpp's include site (where decimal128_t is still forward-declared).
//
// The IEEE-type overloads are declared here but DEFINED in their respective
// decimal*_t.hpp headers, after each class is complete. This avoids parsing
// `decimal32_t{coeff, exp, sign}` (etc.) constructor calls in an environment
// where the type is incomplete -- both C++14 (regular if) and nvcc, which
// eagerly compiles function template bodies for device codegen, would
// otherwise reject the incomplete-type expressions.
template <typename ReturnType, typename SigType, typename ExpType>
BOOST_DECIMAL_CUDA_CONSTEXPR auto pack_in_range(SigType coeff, ExpType exp, bool sign) noexcept
    -> std::enable_if_t<std::is_same<ReturnType, decimal32_t>::value, decimal32_t>;

template <typename ReturnType, typename SigType, typename ExpType>
BOOST_DECIMAL_CUDA_CONSTEXPR auto pack_in_range(SigType coeff, ExpType exp, bool sign) noexcept
    -> std::enable_if_t<std::is_same<ReturnType, decimal64_t>::value, decimal64_t>;

template <typename ReturnType, typename SigType, typename ExpType>
BOOST_DECIMAL_CUDA_CONSTEXPR auto pack_in_range(SigType coeff, ExpType exp, bool sign) noexcept
    -> std::enable_if_t<std::is_same<ReturnType, decimal128_t>::value, decimal128_t>;

// Fallback for fast types and components -- always uses the regular constructor.
// Defined here because its body doesn't reference any specific IEEE decimal type.
template <typename ReturnType, typename SigType, typename ExpType>
BOOST_DECIMAL_CUDA_CONSTEXPR auto pack_in_range(SigType coeff, ExpType exp, bool sign) noexcept
    -> std::enable_if_t<!std::is_same<ReturnType, decimal32_t>::value
                        && !std::is_same<ReturnType, decimal64_t>::value
                        && !std::is_same<ReturnType, decimal128_t>::value, ReturnType>
{
    return ReturnType{coeff, exp, sign};
}

// Inline shrink + pack for a sum/diff that exceeds max_significand_v. For 1-3
// extra digits the bounds are determined by a chained comparison against the
// precomputed thresholds. For >3 extra digits (post-alignment shifts up to ~21
// for d64), a single num_digits call computes extra and pow10 looks up the
// divisor. Either way the divide is one divmod_pow10 + round-half-to-even,
// avoiding the constructor's coefficient_rounding dispatch.
template <typename ReturnType, typename SigType, typename ExpType>
BOOST_DECIMAL_CUDA_CONSTEXPR auto aligned_shrink_and_pack(
    SigType mag, ExpType result_exp, bool result_sign) noexcept -> ReturnType
{
    constexpr SigType ten_to_p {
        static_cast<SigType>(max_significand_v<ReturnType>) + SigType{1}};
    constexpr SigType ten_to_p_plus_1 {static_cast<SigType>(ten_to_p * SigType{10U})};
    constexpr SigType ten_to_p_plus_2 {static_cast<SigType>(ten_to_p_plus_1 * SigType{10U})};
    constexpr SigType ten_to_p_plus_3 {static_cast<SigType>(ten_to_p_plus_2 * SigType{10U})};

    unsigned extra {1U};
    SigType pow_extra {SigType{10U}};
    SigType half {SigType{5U}};
    if (mag >= ten_to_p_plus_1)
    {
        extra = 2U;
        pow_extra = SigType{100U};
        half = SigType{50U};
        if (mag >= ten_to_p_plus_2)
        {
            extra = 3U;
            pow_extra = SigType{1000U};
            half = SigType{500U};
            if (BOOST_DECIMAL_UNLIKELY(mag >= ten_to_p_plus_3))
            {
                // Tall overflow (more than 3 extra digits): post-alignment
                // shifts up to ~21 can land here for d64. Compute extra via
                // num_digits and look up pow10.
                const auto digits {num_digits(mag)};
                extra = static_cast<unsigned>(digits - detail::precision_v<ReturnType>);
                pow_extra = detail::pow10<SigType>(static_cast<SigType>(extra));
                half = static_cast<SigType>(pow_extra / SigType{2U});
            }
        }
    }

    const auto dr {impl::divmod_pow10_dispatch(mag, static_cast<int>(extra), pow_extra)};
    auto q {static_cast<SigType>(dr.quotient)};
    const auto r {static_cast<SigType>(dr.remainder)};

    if (r > half || (r == half && (low64(q) & UINT64_C(1)) != 0U))
    {
        ++q;
        if (BOOST_DECIMAL_UNLIKELY(q == ten_to_p))
        {
            q = static_cast<SigType>(ten_to_p / SigType{10U});
            ++extra;
        }
    }
    return pack_in_range<ReturnType>(q, result_exp + static_cast<ExpType>(extra), result_sign);
}

// Aligned-add kernel that stays in the narrower integer width (uint64 for d64,
// uint128 for d128) instead of the kernel's normal promoted width (uint128 or
// u256). Two precision-p operands shifted by up to `narrow_max_shift` digits
// still fit in the narrow type. The kernel also handles the overflow shrink
// inline via Wang/Schulte injection-style rounding (precomputed half-constants,
// single divmod, round-half-to-even), avoiding the constructor's
// num_digits + coefficient_rounding dispatch (~50 cycles per overflow case).
//
// Caller must guarantee:
//   - default rounding mode is fe_dec_to_nearest
//   - shift in [0, 3] where 0 means same-exponent (no multiply)
//   - both operands non-zero (zero short-circuit happens upstream)
template <typename ReturnType, typename SigType, typename ExpType>
BOOST_DECIMAL_CUDA_CONSTEXPR auto aligned_add_kernel(
    SigType big_lhs, SigType big_rhs,
    ExpType lhs_exp, ExpType rhs_exp, unsigned shift,
    bool lhs_sign, bool rhs_sign) noexcept -> ReturnType
{
    SigType a {};
    SigType b {};
    ExpType result_exp {};
    if (shift == 0U)
    {
        a = big_lhs;
        b = big_rhs;
        result_exp = lhs_exp;
    }
    else
    {
        const auto pow_shift {detail::pow10<SigType>(static_cast<SigType>(shift))};
        if (lhs_exp < rhs_exp)
        {
            a = big_lhs;
            b = static_cast<SigType>(big_rhs * pow_shift);
            result_exp = lhs_exp;
        }
        else
        {
            a = static_cast<SigType>(big_lhs * pow_shift);
            b = big_rhs;
            result_exp = rhs_exp;
        }
    }

    // Same-sign: magnitudes add. Sum can overflow by up to `shift` digits
    // (1 for shift=0 same-exp, up to `shift` for small-diff).
    if (lhs_sign == rhs_sign)
    {
        const SigType sum {static_cast<SigType>(a + b)};
        constexpr SigType ten_to_p_ss {
            static_cast<SigType>(max_significand_v<ReturnType>) + SigType{1}};
        if (sum < ten_to_p_ss)
        {
            return pack_in_range<ReturnType>(sum, result_exp, lhs_sign);
        }
        return aligned_shrink_and_pack<ReturnType>(sum, result_exp, lhs_sign);
    }

    // Opposite signs: magnitudes subtract. For shift=0 the result <= max(a, b) <= max_sig
    // so no overflow check is needed; for shift>0 the result can be up to
    // max_sig * 10^shift and may need shrink.
    SigType mag {};
    bool result_sign {};
    if (a >= b)
    {
        mag = static_cast<SigType>(a - b);
        result_sign = lhs_sign;
    }
    else
    {
        mag = static_cast<SigType>(b - a);
        result_sign = rhs_sign;
    }
    if (shift == 0U)
    {
        // No overflow possible in same-exp opposite-sign subtraction.
        return pack_in_range<ReturnType>(mag, result_exp, result_sign);
    }
    constexpr SigType ten_to_p_os {
        static_cast<SigType>(max_significand_v<ReturnType>) + SigType{1}};
    if (mag < ten_to_p_os)
    {
        return pack_in_range<ReturnType>(mag, result_exp, result_sign);
    }
    return aligned_shrink_and_pack<ReturnType>(mag, result_exp, result_sign);
}

// Backwards-compatible aliases for the d128 callers that still use the old name.
template <typename ReturnType, typename ExpType>
BOOST_DECIMAL_CUDA_CONSTEXPR auto d128_uint128_aligned_kernel(
    int128::uint128_t big_lhs, int128::uint128_t big_rhs,
    ExpType lhs_exp, ExpType rhs_exp, unsigned shift,
    bool lhs_sign, bool rhs_sign) noexcept -> ReturnType
{
    return aligned_add_kernel<ReturnType>(big_lhs, big_rhs, lhs_exp, rhs_exp, shift, lhs_sign, rhs_sign);
}

template <typename ReturnType, typename T>
BOOST_DECIMAL_CUDA_CONSTEXPR auto add_impl(const T& lhs, const T& rhs) noexcept -> ReturnType
{
    // Each of the significands is maximally 23 bits.
    // Rather than doing division to get proper alignment we will promote to 64 bits
    // And do a single mul followed by an add
    using add_type = std::conditional_t<decimal_val_v<T> < 64, std::int_fast64_t, int128::int128_t>;
    using promoted_sig_type = std::conditional_t<decimal_val_v<T> < 64, std::uint_fast64_t, int128::uint128_t>;

    promoted_sig_type big_lhs {lhs.full_significand()};
    promoted_sig_type big_rhs {rhs.full_significand()};
    auto lhs_exp {lhs.biased_exponent()};
    auto rhs_exp {rhs.biased_exponent()};

    // IEEE 754-2008 3.5.1: zero has a cohort with one representation per exponent.
    // The exponent-comparison logic below assumes both operands have full precision
    // (which expand_significand normalizes), but a cohorted zero has no precision to
    // expand and its exponent can be arbitrarily large or small. Short-circuit so the
    // result is the non-zero operand (or zero with a sensible cohort if both are zero).
    if (big_lhs == 0U && big_rhs == 0U)
    {
        // IEEE 754-2008 6.1: preferred quantum for the sum of two zeros is min(exp_x, exp_y).
        // IEEE 754-2008 6.3: sum of opposite-sign zeros is +0 in default rounding.
        const auto result_exp {lhs_exp < rhs_exp ? lhs_exp : rhs_exp};
        const bool result_sign {lhs.isneg() && rhs.isneg()};
        return ReturnType{lhs.full_significand(), result_exp, result_sign};
    }
    if (big_lhs == 0U)
    {
        return ReturnType{rhs.full_significand(), rhs.biased_exponent(), rhs.isneg()};
    }
    if (big_rhs == 0U)
    {
        return ReturnType{lhs.full_significand(), lhs.biased_exponent(), lhs.isneg()};
    }

    // Phase 3 fast paths for d64 (uint128 promoted) and d32 (uint64 promoted):
    // dispatch via aligned_add_kernel which handles same-exp + small-diff + the
    // overflow shrink inline (avoiding the constructor's coefficient_rounding).
    //
    // Two width-buckets:
    //   - shift <= 3: stay in uint64 (max_sig 16 digits * 10^3 = 19 digits < 2^64).
    //   - shift in [4, max_shift]: stay in uint128 (the existing promoted width,
    //     but with inline shrink instead of the signed-add-then-construct path).
    //
    // Guard: only enter when inputs are at the return type's precision
    // (operator+/- callers); FMA passes wider promoted-precision components and
    // must use the slow path.
    BOOST_DECIMAL_IF_CONSTEXPR (detail::precision_v<ReturnType> <= 16
        && std::is_same<typename T::significand_type, typename ReturnType::significand_type>::value)
    {
        constexpr unsigned u64_small_diff_limit {3U};
        constexpr auto u128_max_shift {detail::make_positive_unsigned(
            std::numeric_limits<promoted_sig_type>::digits10 - detail::precision_v<ReturnType> - 1)};
        const auto exp_diff {lhs_exp - rhs_exp};
        const auto shift_abs {static_cast<unsigned>(exp_diff < 0 ? -exp_diff : exp_diff)};
        if (shift_abs <= static_cast<unsigned>(u128_max_shift))
        {
            bool default_rounding {_boost_decimal_global_rounding_mode == rounding_mode::fe_dec_to_nearest};
            #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION
            if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
            {
                default_rounding = (_boost_decimal_global_runtime_rounding_mode == rounding_mode::fe_dec_to_nearest);
            }
            #endif
            if (BOOST_DECIMAL_LIKELY(default_rounding))
            {
                if (shift_abs <= u64_small_diff_limit)
                {
                    return aligned_add_kernel<ReturnType, std::uint64_t>(
                        static_cast<std::uint64_t>(big_lhs),
                        static_cast<std::uint64_t>(big_rhs),
                        lhs_exp, rhs_exp, shift_abs,
                        lhs.isneg(), rhs.isneg());
                }
                return aligned_add_kernel<ReturnType, promoted_sig_type>(
                    big_lhs, big_rhs,
                    lhs_exp, rhs_exp, shift_abs,
                    lhs.isneg(), rhs.isneg());
            }
        }
    }

    // Align to larger exponent
    if (lhs_exp != rhs_exp)
    {
        constexpr auto max_shift {detail::make_positive_unsigned(std::numeric_limits<promoted_sig_type>::digits10 - detail::precision_v<ReturnType> - 1)};
        const auto shift {detail::make_positive_unsigned(lhs_exp - rhs_exp)};

        if (shift > max_shift)
        {
            auto round {_boost_decimal_global_rounding_mode};

            #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION

            if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
            {
                round = fegetround();
            }

            #endif

            // Resolve fe_dec_toward_zero into the equivalent directional mode
            // for the result's sign so the down/up paths below cover it too.
            // (toward zero == toward -inf for positive results, == toward +inf
            // for negative results.)
            if (round == rounding_mode::fe_dec_toward_zero)
            {
                const bool resolved_use_lhs {big_lhs != 0U && (lhs_exp > rhs_exp)};
                const bool result_is_neg {resolved_use_lhs ? lhs.isneg() : rhs.isneg()};
                round = result_is_neg ? rounding_mode::fe_dec_upward : rounding_mode::fe_dec_downward;
            }

            if (BOOST_DECIMAL_LIKELY(round != rounding_mode::fe_dec_downward && round != rounding_mode::fe_dec_upward))
            {
                return big_lhs != 0U && (lhs_exp > rhs_exp) ?
                                    ReturnType{lhs.full_significand(), lhs.biased_exponent(), lhs.isneg()} :
                                    ReturnType{rhs.full_significand(), rhs.biased_exponent(), rhs.isneg()};
            }
            else if (round == rounding_mode::fe_dec_downward)
            {
                // If we are subtracting even disparate numbers we need to round down
                // E.g. "5e+95"_DF - "4e-100"_DF == "4.999999e+95"_DF
                const auto use_lhs {big_lhs != 0U && (lhs_exp > rhs_exp)};

                // Need to check for the case where we have 1e+95 - anything = 9.99999... without losing a nine
                if (use_lhs)
                {
                    if (big_rhs != 0U && (lhs.isneg() != rhs.isneg()))
                    {
                        if (is_power_of_10(big_lhs))
                        {
                            --big_lhs;
                            big_lhs *= 10U;
                            big_lhs += 9U;
                            --lhs_exp;
                        }
                        else
                        {
                            --big_lhs;
                        }
                    }

                    return ReturnType{big_lhs, lhs_exp, lhs.isneg()};
                }
                else
                {
                    if (big_lhs != 0U && (lhs.isneg() != rhs.isneg()))
                    {
                        if (is_power_of_10(big_rhs))
                        {
                            --big_rhs;
                            big_rhs *= 10U;
                            big_rhs += 9U;
                            --rhs_exp;
                        }
                        else
                        {
                            --big_rhs;
                        }
                    }

                    return ReturnType{big_rhs, rhs_exp, rhs.isneg()};
                }
            }
            else
            {
                // rounding mode == fe_dec_upward
                // Unconditionally round up. Could be 5e+95 + 4e-100 -> 5.000001e+95
                const bool use_lhs {big_lhs != 0U && (lhs_exp > rhs_exp)};

                if (use_lhs)
                {
                    if (big_rhs != 0U)
                    {
                        if (lhs.isneg() != rhs.isneg())
                        {
                            if (is_power_of_10(big_lhs))
                            {
                                --big_lhs;
                                big_lhs *= 10U;
                                big_lhs += 9U;
                                --lhs_exp;
                            }
                            else
                            {
                                --big_lhs;
                            }
                        }
                        else
                        {
                            ++big_lhs;
                        }
                    }

                    return ReturnType{big_lhs, lhs_exp, lhs.isneg()} ;
                }
                else
                {
                    if (big_lhs != 0U)
                    {
                        if (rhs.isneg() != lhs.isneg())
                        {
                            --big_rhs;
                            big_rhs *= 10U;
                            big_rhs += 9U;
                            --rhs_exp;
                        }
                        else
                        {
                            ++big_rhs;
                        }
                    }

                    return ReturnType{big_rhs, rhs_exp, rhs.isneg()};
                }
            }
        }

        if (lhs_exp < rhs_exp)
        {
            big_rhs *= detail::pow10<promoted_sig_type>(shift);
            lhs_exp = rhs_exp - static_cast<decltype(lhs_exp)>(shift);
        }
        else
        {
            big_lhs *= detail::pow10<promoted_sig_type>(shift);
            lhs_exp -= static_cast<decltype(lhs_exp)>(shift);
        }
    }

    // Perform signed addition with overflow protection
    const auto signed_lhs {detail::make_signed_value<add_type>(static_cast<add_type>(big_lhs), lhs.isneg())};
    const auto signed_rhs {detail::make_signed_value<add_type>(static_cast<add_type>(big_rhs), rhs.isneg())};

    const auto new_sig {signed_lhs + signed_rhs};
    const auto return_sig {detail::make_positive_unsigned(new_sig)};

    return ReturnType{return_sig, lhs_exp, new_sig < 0};
}

template <typename ReturnType, typename T>
BOOST_DECIMAL_CUDA_CONSTEXPR auto d128_add_impl_new(const T& lhs, const T& rhs) noexcept -> ReturnType
{
    using promoted_sig_type = u256;

    auto big_lhs {lhs.full_significand()};
    auto big_rhs {rhs.full_significand()};
    auto lhs_exp {lhs.biased_exponent()};
    auto rhs_exp {rhs.biased_exponent()};
    promoted_sig_type promoted_lhs {big_lhs};
    promoted_sig_type promoted_rhs {big_rhs};

    // IEEE 754-2008 3.5.1: zero has a cohort with one representation per exponent.
    // The exponent-comparison alignment below mishandles a cohorted zero whose exp
    // is far from the non-zero operand's exp. Short-circuit so the result is the
    // non-zero operand (or zero with a sensible cohort if both are zero).
    if (big_lhs == typename T::significand_type{0} && big_rhs == typename T::significand_type{0})
    {
        // IEEE 754-2008 6.1: preferred quantum for the sum of two zeros is min(exp_x, exp_y).
        // IEEE 754-2008 6.3: sum of opposite-sign zeros is +0 in default rounding.
        const auto result_exp {lhs_exp < rhs_exp ? lhs_exp : rhs_exp};
        const bool result_sign {lhs.isneg() && rhs.isneg()};
        return ReturnType{lhs.full_significand(), result_exp, result_sign};
    }
    if (big_lhs == typename T::significand_type{0})
    {
        return ReturnType{rhs.full_significand(), rhs.biased_exponent(), rhs.isneg()};
    }
    if (big_rhs == typename T::significand_type{0})
    {
        return ReturnType{lhs.full_significand(), lhs.biased_exponent(), lhs.isneg()};
    }

    // Phase 1 fast path guard: only enter when inputs are at the return type's
    // precision (operator+/- callers); FMA passes promoted components (wider sigs).
    constexpr bool fast_path_eligible {
        std::is_same<typename T::significand_type, typename ReturnType::significand_type>::value};

    // Phase 1 same-exp fast path: stays in uint128 (no u256 promotion, no u256 trailing add).
    // Default rounding only; non-default rounding falls through to the existing u256 path.
    BOOST_DECIMAL_IF_CONSTEXPR (fast_path_eligible)
    {
        if (lhs_exp == rhs_exp)
        {
            bool default_rounding {_boost_decimal_global_rounding_mode == rounding_mode::fe_dec_to_nearest};
            #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION
            if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
            {
                default_rounding = (_boost_decimal_global_runtime_rounding_mode == rounding_mode::fe_dec_to_nearest);
            }
            #endif
            if (BOOST_DECIMAL_LIKELY(default_rounding))
            {
                return d128_uint128_aligned_kernel<ReturnType>(
                    big_lhs, big_rhs, lhs_exp, rhs_exp, 0U,
                    lhs.isneg(), rhs.isneg());
            }
        }
    }

    // Align to larger exponent
    if (lhs_exp != rhs_exp)
    {
        constexpr auto max_shift {detail::make_positive_unsigned(std::numeric_limits<promoted_sig_type>::digits10 - detail::precision_v<ReturnType> - 1)};
        const auto shift {detail::make_positive_unsigned(lhs_exp - rhs_exp)};

        // Phase 1 small-diff fast path: for shifts in [1, 3], the aligned multiply
        // still fits in uint128 (precision 34 + shift 3 = 37 <= digits10(uint128) = 38).
        // Skipping u256 saves ~50 cycles per op vs the u256 path below. Only taken
        // under default rounding; the u256 slow path covers other modes.
        // No LIKELY hint: random-exp workloads have shift >> 3, accumulation has
        // shift <= 3, so neither prediction wins universally.
        BOOST_DECIMAL_IF_CONSTEXPR (fast_path_eligible)
        {
            constexpr auto u128_small_diff_limit {3U};
            if (shift <= u128_small_diff_limit)
            {
                bool default_rounding {_boost_decimal_global_rounding_mode == rounding_mode::fe_dec_to_nearest};
                #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION
                if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
                {
                    default_rounding = (_boost_decimal_global_runtime_rounding_mode == rounding_mode::fe_dec_to_nearest);
                }
                #endif
                if (BOOST_DECIMAL_LIKELY(default_rounding))
                {
                    return d128_uint128_aligned_kernel<ReturnType>(
                        big_lhs, big_rhs, lhs_exp, rhs_exp, static_cast<unsigned>(shift),
                        lhs.isneg(), rhs.isneg());
                }
            }
        }

        if (shift > max_shift)
        {
            auto round {_boost_decimal_global_rounding_mode};

            #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION

            if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
            {
                round = fegetround();
            }

            #endif

            // Resolve fe_dec_toward_zero into the equivalent directional mode
            // for the result's sign so the down/up paths below cover it too.
            // (toward zero == toward -inf for positive results, == toward +inf
            // for negative results.)
            if (round == rounding_mode::fe_dec_toward_zero)
            {
                const bool resolved_use_lhs {big_lhs != 0U && (lhs_exp > rhs_exp)};
                const bool result_is_neg {resolved_use_lhs ? lhs.isneg() : rhs.isneg()};
                round = result_is_neg ? rounding_mode::fe_dec_upward : rounding_mode::fe_dec_downward;
            }

            if (BOOST_DECIMAL_LIKELY(round != rounding_mode::fe_dec_downward && round != rounding_mode::fe_dec_upward))
            {
                return big_lhs != 0U && (lhs_exp > rhs_exp) ?
                                    ReturnType{lhs.full_significand(), lhs.biased_exponent(), lhs.isneg()} :
                                    ReturnType{rhs.full_significand(), rhs.biased_exponent(), rhs.isneg()};
            }
            else if (round == rounding_mode::fe_dec_downward)
            {
                // If we are subtracting even disparate numbers we need to round down
                // E.g. "5e+95"_DF - "4e-100"_DF == "4.999999e+95"_DF
                const auto use_lhs {big_lhs != 0U && (lhs_exp > rhs_exp)};

                // Need to check for the case where we have 1e+95 - anything = 9.99999... without losing a nine
                if (use_lhs)
                {
                    if (big_rhs != 0U && (lhs.isneg() != rhs.isneg()))
                    {
                        if (is_power_of_10(big_lhs))
                        {
                            --big_lhs;
                            big_lhs *= 10U;
                            big_lhs += 9U;
                            --lhs_exp;
                        }
                        else
                        {
                            --big_lhs;
                        }
                    }

                    return ReturnType{big_lhs, lhs_exp, lhs.isneg()};
                }
                else
                {
                    if (big_lhs != 0U && (lhs.isneg() != rhs.isneg()))
                    {
                        if (is_power_of_10(big_rhs))
                        {
                            --big_rhs;
                            big_rhs *= 10U;
                            big_rhs += 9U;
                            --rhs_exp;
                        }
                        else
                        {
                            --big_rhs;
                        }
                    }

                    return ReturnType{big_rhs, rhs_exp, rhs.isneg()};
                }
            }
            else
            {
                // rounding mode == fe_dec_upward
                // Unconditionally round up. Could be 5e+95 + 4e-100 -> 5.000001e+95
                const bool use_lhs {big_lhs != 0U && (lhs_exp > rhs_exp)};

                if (use_lhs)
                {
                    if (big_rhs != 0U)
                    {
                        if (lhs.isneg() != rhs.isneg())
                        {
                            if (is_power_of_10(big_lhs))
                            {
                                --big_lhs;
                                big_lhs *= 10U;
                                big_lhs += 9U;
                                --lhs_exp;
                            }
                            else
                            {
                                --big_lhs;
                            }
                        }
                        else
                        {
                            ++big_lhs;
                        }
                    }

                    return ReturnType{big_lhs, lhs_exp, lhs.isneg()} ;
                }
                else
                {
                    if (big_lhs != 0U)
                    {
                        if (rhs.isneg() != lhs.isneg())
                        {
                            --big_rhs;
                            big_rhs *= 10U;
                            big_rhs += 9U;
                            --rhs_exp;
                        }
                        else
                        {
                            ++big_rhs;
                        }
                    }

                    return ReturnType{big_rhs, rhs_exp, rhs.isneg()};
                }
            }
        }

        const auto shift_pow10 {detail::pow10_256(shift)};

        if (lhs_exp < rhs_exp)
        {
            promoted_rhs *= shift_pow10;
            lhs_exp = rhs_exp - static_cast<decltype(lhs_exp)>(shift);
        }
        else
        {
            promoted_lhs *= shift_pow10;
            lhs_exp -= static_cast<decltype(lhs_exp)>(shift);
        }
    }
    
    u256 return_sig {};
    bool return_sign {};
    const auto lhs_sign {lhs.isneg()};
    const auto rhs_sign {rhs.isneg()};

    if (lhs_sign && !rhs_sign)
    {
        // -lhs + rhs = rhs - lhs
        return_sign = i256_sub(promoted_rhs, promoted_lhs, return_sig);
    }
    else if (!lhs_sign && rhs_sign)
    {
        // lhs - rhs
        return_sign = i256_sub(promoted_lhs, promoted_rhs, return_sig);
    }
    else
    {
        // lhs + rhs or -lhs + -rhs
        return_sig = promoted_lhs + promoted_rhs;
        return_sign = lhs_sign && rhs_sign;
    }

    BOOST_DECIMAL_IF_CONSTEXPR (detail::decimal_val_v<ReturnType> == 128)
    {
        // In the regular 128-bit case there's a chance the high words are empty,
        // and we can just convert to 128-bit arithmetic now
        if (return_sig[2] == 0U && return_sig[3] == 0U)
        {
            return ReturnType{static_cast<int128::uint128_t>(return_sig), lhs_exp, return_sign};
        }
    }

    return ReturnType{return_sig, lhs_exp, return_sign};
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

} // namespace detail
} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_ADD_IMPL_HPP
