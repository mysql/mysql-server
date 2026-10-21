// Copyright 2023 - 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_MUL_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_MUL_IMPL_HPP

#include <boost/decimal/detail/attributes.hpp>
#include <boost/decimal/detail/apply_sign.hpp>
#include <boost/decimal/detail/fenv_rounding.hpp>
#include <boost/decimal/detail/add_impl.hpp>
#include <boost/decimal/detail/normalize.hpp>
#include <boost/decimal/detail/u256.hpp>
#include <boost/decimal/detail/power_tables.hpp>
#include <boost/decimal/detail/components.hpp>
#include "int128.hpp"

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cstdint>
#endif

namespace boost {
namespace decimal {
namespace detail {

namespace impl {

// Wide-product shrink + round-half-to-even + pack helpers used by the IEEE
// finite multiplication path under default rounding (fe_dec_to_nearest).
//
// After multiplying two precision-p significands (both expanded to full
// precision), the wide product has either 2p-1 or 2p digits. We shrink to
// exactly p digits by branching on the threshold 10^(2p-1), then dividing by
// 10^(p-1) or 10^p with a round-half-to-even rule and a carry handler for the
// rare q == 10^p case. Finally pack_in_range emits the IEEE bit pattern via
// direct_pack_d* when the biased exponent is in range, falling back to the
// constructor for overflow/underflow paths.
//
// Caller guarantees that both operand significands are non-zero (zero needs
// the constructor's cohort-canonicalization path), at full precision p, and
// that the active rounding mode is fe_dec_to_nearest. Non-default rounding
// modes go through the constructor path which honors fenv_round dispatch.

// uint64 product -> decimal{32,fast32}_t. Product spans [10^12, ~10^14).
template <typename ReturnType, typename ExpType>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto mul_finalize_u64(
    std::uint_fast64_t product, ExpType result_exp, bool result_sign) noexcept -> ReturnType
{
    int extra {6};
    if (product >= UINT64_C(10000000000000)) // 10^13
    {
        extra = 7;
    }

    const auto pow_extra {detail::pow10<std::uint64_t>(static_cast<std::uint64_t>(extra))};
    auto q {product / pow_extra};
    const auto r {product - q * pow_extra};
    const auto half {pow_extra >> 1};

    if (r > half || (r == half && (q & UINT64_C(1)) != 0U))
    {
        ++q;
        if (BOOST_DECIMAL_UNLIKELY(q == UINT64_C(10000000))) // 10^7
        {
            q = UINT64_C(1000000); // 10^6
            ++extra;
        }
    }

    return detail::pack_in_range<ReturnType>(static_cast<std::uint32_t>(q),
                                             result_exp + static_cast<ExpType>(extra),
                                             result_sign);
}

// uint128 product -> decimal{64,fast64}_t. Product spans [10^30, ~10^32).
template <typename ReturnType, typename ExpType>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto mul_finalize_u128(
    int128::uint128_t product, ExpType result_exp, bool result_sign) noexcept -> ReturnType
{
    constexpr auto threshold {detail::pow10<int128::uint128_t>(int128::uint128_t{UINT64_C(31)})};

    int extra {15};
    if (product >= threshold)
    {
        extra = 16;
    }

    const auto pow_extra {detail::pow10<int128::uint128_t>(int128::uint128_t{static_cast<std::uint64_t>(extra)})};
    const auto dr {detail::impl::divmod_pow10_dispatch(product, extra, pow_extra)};
    auto q {dr.quotient};
    const auto r {dr.remainder};
    const auto half {pow_extra >> 1};

    if (r > half || (r == half && (q.low & UINT64_C(1)) != 0U))
    {
        ++q;
        constexpr auto ten_p {detail::pow10<int128::uint128_t>(int128::uint128_t{UINT64_C(16)})};
        if (BOOST_DECIMAL_UNLIKELY(q == ten_p))
        {
            q = detail::pow10<int128::uint128_t>(int128::uint128_t{UINT64_C(15)});
            ++extra;
        }
    }

    return detail::pack_in_range<ReturnType>(q.low,
                                             result_exp + static_cast<ExpType>(extra),
                                             result_sign);
}

// u256 product -> decimal{128,fast128}_t. Product spans [10^66, ~10^68).
template <typename ReturnType, typename ExpType>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto mul_finalize_u256(
    const u256& product, ExpType result_exp, bool result_sign) noexcept -> ReturnType
{
    int extra {33};
    if (product >= detail::pow10(u256{UINT64_C(67)}))
    {
        extra = 34;
    }

    const auto pow_extra {detail::pow10(u256{static_cast<std::uint64_t>(extra)})};
    const auto dr {detail::impl::divmod_pow10_dispatch(product, extra, pow_extra)};
    auto q {dr.quotient};
    const auto r {dr.remainder};
    const auto half {pow_extra >> 1};

    if (r > half || (r == half && (q.bytes[0] & UINT64_C(1)) != 0U))
    {
        ++q;
        constexpr auto ten_p {detail::pow10(u256{UINT64_C(34)})};
        if (BOOST_DECIMAL_UNLIKELY(q == ten_p))
        {
            q = detail::pow10(u256{UINT64_C(33)});
            ++extra;
        }
    }

    const int128::uint128_t q_u128 {q.bytes[1], q.bytes[0]};
    return detail::pack_in_range<ReturnType>(q_u128,
                                             result_exp + static_cast<ExpType>(extra),
                                             result_sign);
}

// Default-rounding gate matching add_impl's pattern: check the global rounding
// mode at compile-time and runtime; the fast path is only safe when the active
// mode is fe_dec_to_nearest (the round-half-to-even logic in mul_finalize_*).
// For any other mode, callers fall back to the constructor (which dispatches
// to fenv_round, honoring the runtime mode correctly).
template <typename Anchor>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto mul_default_rounding(const Anchor& anchor) noexcept -> bool
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

} // namespace impl

// Generic multiplication kernel. The body dispatches via SFINAE-disjoint
// helpers (impl::mul_impl_ieee / impl::mul_impl_components) so each helper's
// body is type-checked only for the ReturnType it actually serves
// (BOOST_DECIMAL_IF_CONSTEXPR falls back to plain `if` in C++14 and would
// otherwise force the FMA-only branch to resolve precision_v /
// expand_significand / mul_finalize_* against component structs that don't
// satisfy is_decimal_floating_point_v). The outer mul_impl signature stays
// `-> ReturnType` so existing friend declarations in decimal_fast*_t headers
// still match.
//
//   (a) ReturnType is one of decimal{32,64,128}_t or decimal_fast{32,64,128}_t.
//       Expands both operands to full precision (no-op if already canonical),
//       multiplies, then shrinks to exactly precision digits with
//       round-half-to-even and packs via pack_in_range (direct_pack fast path
//       for IEEE types; constructor fallback for fast types).
//   (b) ReturnType is a *_components struct (FMA consumes the unrounded wider
//       product before its add step). Returns the raw product packed into the
//       wider components type without shrinking.

namespace impl {

// Tag-dispatch helpers receive already-extracted components (so they don't
// need friend access to the fast types' private to_components). The outer
// mul_impl in the detail namespace below provides that access. Tag dispatch
// (std::integral_constant) is used instead of `if constexpr` so the wrong
// dispatcher isn't instantiated for any given ReturnType -- BOOST_DECIMAL_IF_CONSTEXPR
// falls back to plain `if` in C++14 and would otherwise force both branches'
// bodies to type-check.

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4127)
#  pragma warning(disable : 4702) // Complains that unlikely branches are unreachable
#endif

template <typename ReturnType, std::int32_t TVal, typename Components>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto mul_impl_dispatch(
    Components lhs_c, Components rhs_c, std::true_type /*is_decimal_floating_point*/) noexcept -> ReturnType
{
    using mul_type = std::conditional_t<TVal < 64, std::uint_fast64_t, int128::uint128_t>;
    const bool sign {lhs_c.sign != rhs_c.sign};

    // d32 IEEE: revert to the simple multiply + constructor handoff. The
    // hardware uint64 divide the constructor's coefficient_rounding does
    // is already cheap enough that the expand + pre-shrink + direct_pack
    // path's overhead doesn't pay off here. d32_fast still benefits and
    // continues to use the optimized path below.
    BOOST_DECIMAL_IF_CONSTEXPR (std::is_same<ReturnType, decimal32_t>::value)
    {
        const auto res_sig {static_cast<mul_type>(lhs_c.sig) * static_cast<mul_type>(rhs_c.sig)};
        const auto res_exp {lhs_c.exp + rhs_c.exp};
        return ReturnType{res_sig, res_exp, sign};
    }
    else
    {
        // Small-product fast path: when at least one operand is
        // non-canonical (sig below 10^(p-1)) AND the un-expanded product
        // fits in p digits, the product itself is the result. Skip
        // expand+shrink entirely and let pack_in_range route to direct_pack
        // (or to the constructor for out-of-range biased_exp). Gated on
        // sig comparisons so the canonical path pays only two compares.
        using sig_t = typename ReturnType::significand_type;
        constexpr auto min_normal_sig {detail::pow10<sig_t>(static_cast<sig_t>(detail::precision_v<ReturnType> - 1))};
        if (BOOST_DECIMAL_UNLIKELY(lhs_c.sig < min_normal_sig || rhs_c.sig < min_normal_sig))
        {
            const auto small_prod {static_cast<mul_type>(lhs_c.sig) * static_cast<mul_type>(rhs_c.sig)};
            constexpr mul_type small_threshold {static_cast<mul_type>(detail::max_significand_v<ReturnType>) + mul_type{1U}};
            if (small_prod < small_threshold)
            {
                return detail::pack_in_range<ReturnType>(static_cast<sig_t>(small_prod),
                                                         lhs_c.exp + rhs_c.exp, sign);
            }
        }

        // Fast types store significands at full precision already; expansion
        // is a no-op for them so we skip the calls entirely. IEEE types can
        // hold non-canonical cohorts and need the expansion to bring the
        // significand up to precision digits before the threshold-based
        // shrink in mul_finalize_*.
        BOOST_DECIMAL_IF_CONSTEXPR (!detail::is_fast_type_v<ReturnType>)
        {
            detail::expand_significand<ReturnType>(lhs_c.sig, lhs_c.exp);
            detail::expand_significand<ReturnType>(rhs_c.sig, rhs_c.exp);
        }

        const auto res_sig {static_cast<mul_type>(lhs_c.sig) * static_cast<mul_type>(rhs_c.sig)};
        const auto res_exp {lhs_c.exp + rhs_c.exp};

        // Zero canonicalization: a zero significand must go through the
        // constructor so the IEEE zero-cohort encoding is emitted.
        if (BOOST_DECIMAL_UNLIKELY(lhs_c.sig == 0U || rhs_c.sig == 0U))
        {
            return ReturnType{res_sig, res_exp, sign};
        }
        if (BOOST_DECIMAL_UNLIKELY(!impl::mul_default_rounding(lhs_c.sig)))
        {
            // Non-default rounding mode: constructor's coefficient_rounding
            // dispatches to fenv_round which honors the runtime mode.
            return ReturnType{res_sig, res_exp, sign};
        }
        BOOST_DECIMAL_IF_CONSTEXPR (TVal < 64)
        {
            // The static_cast is identity in this branch (mul_type is uint_fast64_t
            // when TVal < 64) but is required to make the d64/d128 instantiation
            // type-check in C++14, where BOOST_DECIMAL_IF_CONSTEXPR is plain `if`
            // and both branches must be valid even though the wide-width branch
            // is unreachable at runtime.
            return impl::mul_finalize_u64<ReturnType>(static_cast<std::uint_fast64_t>(res_sig), res_exp, sign);
        }
        else
        {
            return impl::mul_finalize_u128<ReturnType>(res_sig, res_exp, sign);
        }
    }
}

template <typename ReturnType, std::int32_t TVal, typename Components>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto mul_impl_dispatch(
    Components lhs_c, Components rhs_c, std::false_type /*is_decimal_floating_point*/) noexcept -> ReturnType
{
    // FMA components path: return wide product unrounded. ReturnType is a
    // decimal_components<SigType, BiasedExpType> struct (decimal_val_v defined,
    // but no precision_v / max_significand_v / direct_pack_*), so the
    // optimized path's helpers above don't apply.
    using mul_type = std::conditional_t<TVal < 64, std::uint_fast64_t, int128::uint128_t>;

    const bool sign {lhs_c.sign != rhs_c.sign};
    const auto res_sig {static_cast<mul_type>(lhs_c.sig) * static_cast<mul_type>(rhs_c.sig)};
    const auto res_exp {lhs_c.exp + rhs_c.exp};

    return {res_sig, res_exp, sign};
}

} // namespace impl

// Outer mul_impl: keeps the original `-> ReturnType` signature so friend
// declarations in decimal_fast*_t headers match. Extracts components via the
// (friend-accessible) to_components() and tag-dispatches to SFINAE-disjoint
// impl helpers. Tag dispatch (rather than if-constexpr) ensures only the
// matching helper body is instantiated for any given ReturnType -- BOOST_DECIMAL_IF_CONSTEXPR
// falls back to plain `if` in C++14, which would force both branches to
// type-check.
template <typename ReturnType, typename T>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto mul_impl(const T& lhs, const T& rhs) noexcept -> ReturnType
{
    return impl::mul_impl_dispatch<ReturnType, decimal_val_v<T>>(
        lhs.to_components(), rhs.to_components(),
        std::integral_constant<bool, detail::is_decimal_floating_point_v<ReturnType>>{});
}

template <typename ReturnType, typename T, typename U>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto mul_impl(T lhs_sig, U lhs_exp, const bool lhs_sign,
                                                   T rhs_sig, U rhs_exp, const bool rhs_sign) noexcept -> ReturnType
{
    // d32 * Integer (sole caller) always passes an IEEE/fast decimal ReturnType.
    // The body references mul_finalize_u64<ReturnType> and expand_significand,
    // both valid for is_decimal_floating_point_v<ReturnType>==true types. No
    // SFINAE constraint needed because no FMA path uses this tuple form
    // (FMA's mul_impl call passes pre-extracted components to the dec-ref form).
    using mul_type = std::uint_fast64_t;

    const bool sign {lhs_sign != rhs_sign};

    // d32 IEEE: revert to the simple multiply + constructor handoff (see
    // generic mul_impl above for rationale).
    BOOST_DECIMAL_IF_CONSTEXPR (std::is_same<ReturnType, decimal32_t>::value)
    {
        const auto res_sig {static_cast<mul_type>(lhs_sig) * static_cast<mul_type>(rhs_sig)};
        const auto res_exp {lhs_exp + rhs_exp};
        return ReturnType{res_sig, res_exp, sign};
    }

    BOOST_DECIMAL_IF_CONSTEXPR (!detail::is_fast_type_v<ReturnType>)
    {
        detail::expand_significand<ReturnType>(lhs_sig, lhs_exp);
        detail::expand_significand<ReturnType>(rhs_sig, rhs_exp);
    }

    const auto res_sig {static_cast<mul_type>(lhs_sig) * static_cast<mul_type>(rhs_sig)};
    const auto res_exp {lhs_exp + rhs_exp};

    if (BOOST_DECIMAL_UNLIKELY(lhs_sig == 0U || rhs_sig == 0U))
    {
        return ReturnType{res_sig, res_exp, sign};
    }
    if (BOOST_DECIMAL_UNLIKELY(!impl::mul_default_rounding(lhs_sig)))
    {
        return ReturnType{res_sig, res_exp, sign};
    }

    return impl::mul_finalize_u64<ReturnType>(res_sig, res_exp, sign);
}

// d64 specialization. Used by `decimal_fast64_t * decimal_fast64_t` (which
// passes decimal types directly without to_components conversion) and by the
// integer-rhs overloads of `decimal64_t * Integer` and
// `decimal_fast64_t * Integer`.

template <typename ReturnType, typename T>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto d64_mul_impl(const T& lhs, const T& rhs) noexcept -> ReturnType
{
    using unsigned_int128_type = boost::int128::uint128_t;

    auto lhs_c {lhs.to_components()};
    auto rhs_c {rhs.to_components()};
    const bool sign {lhs_c.sign != rhs_c.sign};

    BOOST_DECIMAL_IF_CONSTEXPR (!detail::is_fast_type_v<ReturnType>)
    {
        detail::expand_significand<ReturnType>(lhs_c.sig, lhs_c.exp);
        detail::expand_significand<ReturnType>(rhs_c.sig, rhs_c.exp);
    }

    const auto res_sig {static_cast<unsigned_int128_type>(lhs_c.sig) * static_cast<unsigned_int128_type>(rhs_c.sig)};
    const auto res_exp {lhs_c.exp + rhs_c.exp};

    if (BOOST_DECIMAL_UNLIKELY(lhs_c.sig == 0U || rhs_c.sig == 0U))
    {
        return ReturnType{res_sig, res_exp, sign};
    }
    if (BOOST_DECIMAL_UNLIKELY(!impl::mul_default_rounding(lhs)))
    {
        return ReturnType{res_sig, res_exp, sign};
    }

    return impl::mul_finalize_u128<ReturnType>(res_sig, res_exp, sign);
}

template <typename ReturnType, BOOST_DECIMAL_INTEGRAL T, BOOST_DECIMAL_INTEGRAL U>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto d64_mul_impl(T lhs_sig, U lhs_exp, bool lhs_sign,
                                                       T rhs_sig, U rhs_exp, bool rhs_sign) noexcept
                                                       -> std::enable_if_t<detail::is_decimal_floating_point_v<ReturnType>, ReturnType>
{
    using unsigned_int128_type = boost::int128::uint128_t;

    const bool sign {lhs_sign != rhs_sign};

    BOOST_DECIMAL_IF_CONSTEXPR (!detail::is_fast_type_v<ReturnType>)
    {
        detail::expand_significand<ReturnType>(lhs_sig, lhs_exp);
        detail::expand_significand<ReturnType>(rhs_sig, rhs_exp);
    }

    const auto res_sig {static_cast<unsigned_int128_type>(lhs_sig) * static_cast<unsigned_int128_type>(rhs_sig)};
    const auto res_exp {lhs_exp + rhs_exp};

    if (BOOST_DECIMAL_UNLIKELY(lhs_sig == 0U || rhs_sig == 0U))
    {
        return ReturnType{res_sig, res_exp, sign};
    }
    if (BOOST_DECIMAL_UNLIKELY(!impl::mul_default_rounding(lhs_sig)))
    {
        return ReturnType{res_sig, res_exp, sign};
    }

    return impl::mul_finalize_u128<ReturnType>(res_sig, res_exp, sign);
}

template <typename ReturnType, BOOST_DECIMAL_INTEGRAL T1, BOOST_DECIMAL_INTEGRAL U1,
                               BOOST_DECIMAL_INTEGRAL T2, BOOST_DECIMAL_INTEGRAL U2>
BOOST_DECIMAL_FORCE_INLINE
BOOST_DECIMAL_CUDA_CONSTEXPR auto d128_mul_impl(const T1& lhs_sig_in, const U1 lhs_exp_in, const bool lhs_sign,
                             const T2& rhs_sig_in, const U2 rhs_exp_in, const bool rhs_sign) noexcept -> ReturnType
{
    using sig_type = T1;

    static_assert(std::is_same<sig_type, T2>::value, "Should have a common type by this point");

    const bool sign {lhs_sign != rhs_sign};

    // Small-product fast path: when both operands have at most precision/2
    // digits, the un-expanded product is guaranteed to fit in uint128 (no
    // umul256 needed) and may even fit in p digits, in which case the
    // product itself is the result. Gated on both sigs being below the
    // sqrt(max_sig) threshold so the uint128 multiply cannot overflow.
    constexpr auto small_sig_threshold {detail::pow10<sig_type>(static_cast<sig_type>(detail::precision_v<ReturnType> / 2))};
    if (BOOST_DECIMAL_UNLIKELY(lhs_sig_in <= small_sig_threshold && rhs_sig_in <= small_sig_threshold))
    {
        const auto small_prod {lhs_sig_in * rhs_sig_in};
        constexpr auto small_threshold {static_cast<sig_type>(detail::max_significand_v<ReturnType>) + sig_type{1U}};
        if (small_prod < small_threshold)
        {
            const auto small_exp {static_cast<typename ReturnType::biased_exponent_type>(lhs_exp_in + rhs_exp_in)};
            return detail::pack_in_range<ReturnType>(small_prod, small_exp, sign);
        }
    }

    // Subnormal/underflow fast-bailout: when the operand exponents already sum
    // to deep into the subnormal range, the optimized path below wastes a u256
    // divmod_pow10 (16 multiplies) producing a value the constructor would
    // flush to zero anyway. The legacy umul256 + num_digits + coefficient_rounding
    // path has an early-out (shift > digits10 -> coeff=0) that skips the
    // divmod. Benchmarks show this saves ~7% on the d128 IEEE multiplication
    // hot loop where ~25% of random inputs fall into this range. Kept for
    // d128 only; the d64/d32 paths don't see the same benefit because
    // divmod_pow10_uint128 is much cheaper than divmod_pow10_u256.
    constexpr int subnormal_guard {-detail::bias_v<ReturnType> + detail::precision_v<ReturnType>};
    if (BOOST_DECIMAL_UNLIKELY(static_cast<int>(lhs_exp_in) + static_cast<int>(rhs_exp_in) < subnormal_guard))
    {
        auto res_sig {detail::umul256(lhs_sig_in, rhs_sig_in)};
        auto res_exp_mut {static_cast<typename ReturnType::biased_exponent_type>(lhs_exp_in + rhs_exp_in)};
        const auto sig_dig {detail::num_digits(res_sig)};
        const auto digit_delta {sig_dig - std::numeric_limits<sig_type>::digits10};
        if (BOOST_DECIMAL_LIKELY(digit_delta > 0))
        {
            auto biased_exp {res_exp_mut + detail::bias_v<ReturnType>};
            detail::coefficient_rounding<ReturnType>(res_sig, res_exp_mut, biased_exp, sign, sig_dig);
        }
        // coefficient_rounding leaves res_sig at exactly precision digits (<= max_significand_v),
        // so pack_in_range can use direct_pack when biased_exp is in [0, max] and skip the
        // constructor's redundant num_digits + branch tree. Falls back to the constructor
        // for the (rarer) genuinely-subnormal output where its boundary handling is needed.
        return detail::pack_in_range<ReturnType>(int128::uint128_t{res_sig[1], res_sig[0]}, res_exp_mut, sign);
    }

    auto lhs_sig {lhs_sig_in};
    auto lhs_exp {lhs_exp_in};
    auto rhs_sig {rhs_sig_in};
    auto rhs_exp {rhs_exp_in};

    BOOST_DECIMAL_IF_CONSTEXPR (!detail::is_fast_type_v<ReturnType>)
    {
        detail::expand_significand<ReturnType>(lhs_sig, lhs_exp);
        detail::expand_significand<ReturnType>(rhs_sig, rhs_exp);
    }

    const auto res_exp {static_cast<typename ReturnType::biased_exponent_type>(lhs_exp + rhs_exp)};

    if (BOOST_DECIMAL_UNLIKELY(lhs_sig == 0U || rhs_sig == 0U))
    {
        return ReturnType{int128::uint128_t{0}, res_exp, sign};
    }

    auto res_sig {detail::umul256(lhs_sig, rhs_sig)};

    if (BOOST_DECIMAL_UNLIKELY(!impl::mul_default_rounding(lhs_sig)))
    {
        // Non-default rounding mode: use coefficient_rounding which dispatches
        // to fenv_round and honors the runtime mode. Pack via pack_in_range so
        // the in-range case takes direct_pack (no redundant num_digits in the
        // constructor); out-of-range biased_exp falls back to the constructor.
        const auto sig_dig {detail::num_digits(res_sig)};
        const auto digit_delta {sig_dig - std::numeric_limits<sig_type>::digits10};
        auto res_exp_mut {res_exp};

        if (BOOST_DECIMAL_LIKELY(digit_delta > 0))
        {
            auto biased_exp {res_exp_mut + detail::bias_v<ReturnType>};
            detail::coefficient_rounding<ReturnType>(res_sig, res_exp_mut, biased_exp, sign, sig_dig);
        }

        return detail::pack_in_range<ReturnType>(int128::uint128_t{res_sig[1], res_sig[0]}, res_exp_mut, sign);
    }

    return impl::mul_finalize_u256<ReturnType>(res_sig, res_exp, sign);
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

} // namespace detail
} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_MUL_IMPL_HPP
