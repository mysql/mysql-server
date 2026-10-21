// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_COMPARISON_HPP
#define BOOST_DECIMAL_DETAIL_COMPARISON_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/apply_sign.hpp>
#include <boost/decimal/detail/normalize.hpp>
#include <boost/decimal/detail/promotion.hpp>
#include <boost/decimal/detail/to_decimal.hpp>
#include <boost/decimal/detail/shrink_significand.hpp>
#include <boost/decimal/detail/cmath/isfinite.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/power_tables.hpp>
#include "int128.hpp"
#include <boost/decimal/detail/attributes.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>
#include <cstdint>
#endif

namespace boost {
namespace decimal {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4127)
#endif

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto equality_impl(DecimalType lhs, DecimalType rhs) noexcept -> bool
{
    using comp_type = typename DecimalType::significand_type;

    // Step 1: Check for NANs per IEEE 754
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(lhs) || isnan(rhs))
    {
        return false;
    }
    #endif

    // Step 2: Fast path
    if (lhs.bits_ == rhs.bits_)
    {
        return true;
    }

    const auto lhs_components {lhs.to_components()};
    const auto rhs_components {rhs.to_components()};

    auto lhs_sig {lhs_components.sig};
    auto rhs_sig {rhs_components.sig};

    // Step 4: Check signs
    if (lhs_components.sign != rhs_components.sign)
    {
        return (lhs_sig == 0U && rhs_sig == 0U);
    }

    // Step 4b: Same-sign zeros from any cohort compare equal regardless of their exponents
    // (IEEE 754-2008 3.5.1). Without this, two zeros with delta_exp greater than the type's
    // precision would fall through to the early-return below and wrongly compare unequal.
    if (lhs_sig == 0U && rhs_sig == 0U)
    {
        return true;
    }

    // Step 5: Check the exponents
    // If the difference is greater than we can represent in the significand than we can assume they are different
    const auto lhs_exp {lhs_components.exp};
    const auto rhs_exp {rhs_components.exp};

    const auto delta_exp {lhs_exp - rhs_exp};

    if (BOOST_DECIMAL_UNLIKELY(delta_exp > detail::precision_v<DecimalType> || delta_exp < -detail::precision_v<DecimalType>))
    {
        return false;
    }

    // Step 6: Normalize the significand and compare
    BOOST_DECIMAL_IF_CONSTEXPR (detail::decimal_val_v<DecimalType> >= 128)
    {
        // Instead of multiplying the larger number, divide the smaller one
        //
        // We try for multiplication even though it's a small range
        // Since it's an order of magnitude faster
        if (delta_exp <= 4 && delta_exp >= -4)
        {
            if (delta_exp > 0)
            {
                lhs_sig *= detail::pow10(static_cast<comp_type>(delta_exp));
            }
            else if (delta_exp < 0)
            {
                rhs_sig *= detail::pow10(static_cast<comp_type>(-delta_exp));
            }
        }
        else if (delta_exp > 0)
        {
            // Check if we can divide rhs_sig safely
            // E.g. 9e0 != 90000000204928e-13 but if we just did division we would falsely get 9 ?= 9
            if (rhs_sig % detail::pow10(static_cast<comp_type>(delta_exp)) != 0U)
            {
                return false;
            }
            rhs_sig /= detail::pow10(static_cast<comp_type>(delta_exp));
        }
        else if (delta_exp < 0)
        {
            // Check if we can divide lhs_sig safely
            if (lhs_sig % detail::pow10(static_cast<comp_type>(-delta_exp)) != 0U)
            {
                return false;
            }
            lhs_sig /= detail::pow10(static_cast<comp_type>(-delta_exp));
        }

        return lhs_sig == rhs_sig;
    }
    else
    {
        using promoted_sig_type = std::conditional_t<std::is_same<typename DecimalType::significand_type, std::uint32_t>::value, std::uint64_t, int128::uint128_t>;
        promoted_sig_type promotes_lhs {lhs_sig};
        promoted_sig_type promotes_rhs {rhs_sig};

        if (delta_exp > 0)
        {
            promotes_lhs *= detail::pow10(static_cast<promoted_sig_type>(delta_exp));
        }
        else if (delta_exp < 0)
        {
            promotes_rhs *= detail::pow10(static_cast<promoted_sig_type>(-delta_exp));
        }

        return promotes_lhs == promotes_rhs;
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

template <BOOST_DECIMAL_FAST_DECIMAL_FLOATING_TYPE DecimalType>
BOOST_DECIMAL_FORCE_INLINE constexpr auto fast_equality_impl(const DecimalType& lhs, const DecimalType& rhs) noexcept -> bool
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(lhs) || isnan(rhs))
    {
        return false;
    }
    #endif

    const auto lhs_sig {lhs.significand_};
    const auto rhs_sig {rhs.significand_};

    if (lhs_sig == 0U && rhs_sig == 0U)
    {
        // -0 == +0
        return true;
    }

    return  lhs_sig == rhs_sig &&
            lhs.exponent_ == rhs.exponent_ &&
            lhs.sign_ == rhs.sign_;
}

template <BOOST_DECIMAL_FAST_DECIMAL_FLOATING_TYPE DecimalType>
BOOST_DECIMAL_FORCE_INLINE constexpr auto fast_inequality_impl(const DecimalType& lhs, const DecimalType& rhs) noexcept -> bool
{
    return !fast_equality_impl(lhs, rhs);
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4127) // BOOST_DECIMAL_IF_CONSTEXPR prior to C++17 is constant conditional expression
#endif

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType = decimal32_t, BOOST_DECIMAL_INTEGRAL T1,
          BOOST_DECIMAL_INTEGRAL U1, BOOST_DECIMAL_INTEGRAL T2, BOOST_DECIMAL_INTEGRAL U2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto equal_parts_impl(T1 lhs_sig, U1 lhs_exp, bool lhs_sign,
                                T2 rhs_sig, U2 rhs_exp, bool rhs_sign) noexcept -> std::enable_if_t<detail::is_ieee_type_v<DecimalType>, bool>
{
    using comp_type = detail::make_unsigned_t<std::conditional_t<(std::numeric_limits<T1>::digits10 > std::numeric_limits<T2>::digits10), T1, T2>>;

    BOOST_DECIMAL_ASSERT(lhs_sig >= 0U);
    BOOST_DECIMAL_ASSERT(rhs_sig >= 0U);

    if (lhs_sig == 0U && rhs_sig == 0U)
    {
        // +0 == -0
        return true;
    }

    // We con compare signs right away
    if (lhs_sign != rhs_sign)
    {
        return false;
    }

    const auto delta_exp {lhs_exp - rhs_exp};

    // Check the value of delta exp to avoid to large a value for pow10
    // Also if only one of the significands is 0 then we know the values have to be mismatched
    if (BOOST_DECIMAL_UNLIKELY(delta_exp > detail::precision_v<DecimalType> || delta_exp < -detail::precision_v<DecimalType>))
    {
        return false;
    }

    auto new_lhs_sig {static_cast<comp_type>(lhs_sig)};
    auto new_rhs_sig {static_cast<comp_type>(rhs_sig)};

    // Step 5: Normalize the significand and compare
    // Instead of multiplying the larger number, divide the smaller one
    BOOST_DECIMAL_IF_CONSTEXPR (detail::decimal_val_v<DecimalType> >= 128)
    {
        // Instead of multiplying the larger number, divide the smaller one
        //
        // We try for multiplication even though it's a small range
        // Since it's an order of magnitude faster
        if (delta_exp <= 4 && delta_exp >= -4)
        {
            if (delta_exp > 0)
            {
                new_lhs_sig *= detail::pow10(static_cast<comp_type>(delta_exp));
            }
            else if (delta_exp < 0)
            {
                new_rhs_sig *= detail::pow10(static_cast<comp_type>(-delta_exp));
            }
        }
        else if (delta_exp > 0)
        {
            // Check if we can divide rhs_sig safely
            // E.g. 9e0 != 90000000204928e-13 but if we just did division we would falsely get 9 ?= 9
            if (new_rhs_sig % detail::pow10(static_cast<comp_type>(delta_exp)) != 0U)
            {
                return false;
            }
            new_rhs_sig /= detail::pow10(static_cast<comp_type>(delta_exp));
        }
        else if (delta_exp < 0)
        {
            // Check if we can divide lhs_sig safely
            if (new_lhs_sig % detail::pow10(static_cast<comp_type>(-delta_exp)) != 0U)
            {
                return false;
            }
            new_lhs_sig /= detail::pow10(static_cast<comp_type>(-delta_exp));
        }

        return new_lhs_sig == new_rhs_sig;
    }
    else
    {
        using promoted_sig_type = std::conditional_t<std::is_same<comp_type, std::uint32_t>::value, std::uint64_t, int128::uint128_t>;
        promoted_sig_type promotes_lhs {new_lhs_sig};
        promoted_sig_type promotes_rhs {new_rhs_sig};

        if (delta_exp > 0)
        {
            promotes_lhs *= detail::pow10(static_cast<promoted_sig_type>(delta_exp));
        }
        else if (delta_exp < 0)
        {
            promotes_rhs *= detail::pow10(static_cast<promoted_sig_type>(-delta_exp));
        }

        return promotes_lhs == promotes_rhs;
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType = decimal32_t, BOOST_DECIMAL_INTEGRAL T1,
          BOOST_DECIMAL_INTEGRAL U1, BOOST_DECIMAL_INTEGRAL T2, BOOST_DECIMAL_INTEGRAL U2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto equal_parts_impl(T1 lhs_sig, U1 lhs_exp, const bool lhs_sign,
                                T2 rhs_sig, U2 rhs_exp, const bool rhs_sign) noexcept -> std::enable_if_t<detail::is_fast_type_v<DecimalType>, bool>
{
    using comp_type = std::conditional_t<(std::numeric_limits<T1>::digits10 > std::numeric_limits<T2>::digits10), T1, T2>;

    BOOST_DECIMAL_ASSERT(lhs_sig >= 0U);
    BOOST_DECIMAL_ASSERT(rhs_sig >= 0U);

    if (lhs_sig == 0U && rhs_sig == 0U)
    {
        // +0 == -0
        return true;
    }

    auto new_lhs_sig {static_cast<comp_type>(lhs_sig)};
    auto new_rhs_sig {static_cast<comp_type>(rhs_sig)};

    detail::normalize<DecimalType>(new_lhs_sig, lhs_exp);
    detail::normalize<DecimalType>(new_rhs_sig, rhs_exp);

    #ifdef BOOST_DECIMAL_DEBUG_EQUAL
    std::cerr << "Normalized Values"
              << "\nlhs_sig: " << new_lhs_sig
              << "\nlhs_exp: " << lhs_exp
              << "\nrhs_sig: " << new_rhs_sig
              << "\nrhs_exp: " << rhs_exp << std::endl;
    #endif

    return lhs_sign == rhs_sign &&
           lhs_exp == rhs_exp &&
           new_lhs_sig == new_rhs_sig;
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal, BOOST_DECIMAL_INTEGRAL Integer>
BOOST_DECIMAL_CUDA_CONSTEXPR auto mixed_equality_impl(Decimal lhs, Integer rhs) noexcept
    -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal> && detail::is_integral_v<Integer>), bool>
{
    using exp_type = typename Decimal::biased_exponent_type;

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(lhs) || isinf(lhs))
    {
        return false;
    }
    #endif

    bool rhs_isneg {false};
    BOOST_DECIMAL_IF_CONSTEXPR (detail::is_signed_v<Integer>)
    {
        if (rhs < static_cast<Integer>(0))
        {
            rhs_isneg = true;
        }
    }

    const auto rhs_significand {detail::make_positive_unsigned(rhs)};

    return equal_parts_impl<Decimal>(lhs.full_significand(), lhs.biased_exponent(), lhs.isneg(),
                                     rhs_significand, static_cast<exp_type>(0), rhs_isneg);
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal1, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto mixed_decimal_equality_impl(Decimal1 lhs, Decimal2 rhs) noexcept
    -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal1> &&
                         detail::is_decimal_floating_point_v<Decimal2>), bool>
{
    static_assert(!std::is_same<Decimal1, Decimal2>::value, "Equality of same type exists in simpler form");
    using Bigger_Decimal_Type = std::conditional_t<(sizeof(lhs) > sizeof(rhs)), Decimal1, Decimal2>;

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(lhs) || isnan(rhs))
    {
        return false;
    }
    #endif

    const auto new_lhs = to_decimal<Bigger_Decimal_Type>(lhs);
    const auto new_rhs = to_decimal<Bigger_Decimal_Type>(rhs);

    return new_lhs == new_rhs;
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal1, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto operator==(Decimal1 lhs, Decimal2 rhs) noexcept
    -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal1> &&
                         detail::is_decimal_floating_point_v<Decimal2>), bool>
{
    return mixed_decimal_equality_impl(lhs, rhs);
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal1, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto operator!=(Decimal1 lhs, Decimal2 rhs) noexcept
    -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal1> &&
                         detail::is_decimal_floating_point_v<Decimal2>), bool>
{
    return !(mixed_decimal_equality_impl(lhs, rhs));
}

template <BOOST_DECIMAL_FAST_DECIMAL_FLOATING_TYPE DecimalType>
BOOST_DECIMAL_FORCE_INLINE constexpr auto fast_less_impl(const DecimalType& lhs, const DecimalType& rhs) noexcept -> bool
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (not_finite(lhs) || not_finite(rhs))
    {
        if (isnan(lhs) || isnan(rhs) ||
            (!lhs.isneg() && rhs.isneg()))
        {
            return false;
        }
        else if (lhs.isneg() && !rhs.isneg())
        {
            return true;
        }
        else if (isfinite(lhs) && isinf(rhs))
        {
            return !signbit(rhs);
        }
        else if (isinf(lhs) && isfinite(rhs))
        {
            return signbit(rhs);
        }
    }
    #endif

    // Needed to correctly compare signed and unsigned zeros
    if (lhs.significand_ == 0U || rhs.significand_ == 0U)
    {
        if (lhs.significand_ == 0U && rhs.significand_ == 0U)
        {
            #ifndef BOOST_DECIMAL_FAST_MATH
            return lhs.sign_ && !rhs.sign_;
            #else
            return false;
            #endif
        }
        return lhs.significand_ == 0U ? !rhs.sign_ : lhs.sign_;
    }

    if (lhs.sign_ != rhs.sign_)
    {
        return lhs.sign_;
    }

    if (lhs.exponent_ != rhs.exponent_)
    {
        return lhs.sign_ ? lhs.exponent_ > rhs.exponent_ : lhs.exponent_ < rhs.exponent_;
    }

    return lhs.sign_ ? lhs.significand_ > rhs.significand_ : lhs.significand_ < rhs.significand_;
}

template <BOOST_DECIMAL_INTEGRAL T, BOOST_DECIMAL_INTEGRAL U>
BOOST_DECIMAL_FORCE_INLINE constexpr auto fast_type_less_parts_impl(T lhs_sig, U lhs_exp, bool lhs_sign,
                                                                    T rhs_sig, U rhs_exp, bool rhs_sign) noexcept -> bool
{
    if (lhs_sig == 0U || rhs_sig == 0U)
    {
        if (lhs_sig == 0U && rhs_sig == 0U)
        {
            #ifndef BOOST_DECIMAL_FAST_MATH
            return lhs_sign && !rhs_sign;
            #else
            return false;
            #endif
        }
        return lhs_sig == 0U ? !rhs_sign : lhs_sign;
    }

    if (lhs_sign != rhs_sign)
    {
        return lhs_sign;
    }

    if (lhs_exp != rhs_exp)
    {
        return lhs_sign ? lhs_exp > rhs_exp : lhs_exp < rhs_exp;
    }

    return lhs_sign ? lhs_sig > rhs_sig : lhs_sig < rhs_sig;
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType>
BOOST_DECIMAL_FORCE_INLINE BOOST_DECIMAL_CUDA_CONSTEXPR auto sequential_less_impl(DecimalType lhs, DecimalType rhs) noexcept -> bool
{
    using comp_type = std::conditional_t<detail::decimal_val_v<DecimalType> < 64, std::uint_fast64_t, int128::uint128_t>;

    // Non-finite handling is performed by the callers. With NaN ruled out,
    // identical bit patterns encode the same finite value, so lhs < rhs is false.
    if (lhs.bits_ == rhs.bits_)
    {
        return false;
    }

    // One combination-field branch per operand instead of three.
    const auto lhs_components {lhs.to_components()};
    const auto rhs_components {rhs.to_components()};

    const auto lhs_sign {lhs_components.sign};
    const auto rhs_sign {rhs_components.sign};

    auto lhs_sig {static_cast<comp_type>(lhs_components.sig)};
    auto rhs_sig {static_cast<comp_type>(rhs_components.sig)};

    // Signed zeros: +0 == -0, neither is less.
    if (lhs_sig == static_cast<comp_type>(0) || rhs_sig == static_cast<comp_type>(0))
    {
        if (lhs_sig == rhs_sig)
        {
            return false;
        }
        return lhs_sig == static_cast<comp_type>(0) ? !rhs_sign : lhs_sign;
    }

    if (lhs_sign != rhs_sign)
    {
        return lhs_sign;
    }

    const auto lhs_exp {lhs_components.exp};
    const auto rhs_exp {rhs_components.exp};
    const auto delta_exp {lhs_exp - rhs_exp};

    constexpr auto max_delta_diff {std::numeric_limits<comp_type>::digits10 - detail::precision_v<DecimalType>};

    if (BOOST_DECIMAL_UNLIKELY(delta_exp > max_delta_diff || delta_exp < -max_delta_diff))
    {
        return lhs_sign ? lhs_exp > rhs_exp : lhs_exp < rhs_exp;
    }

    if (delta_exp > 0)
    {
        lhs_sig *= detail::pow10(static_cast<comp_type>(delta_exp));
    }
    else if (delta_exp < 0)
    {
        rhs_sig *= detail::pow10(static_cast<comp_type>(-delta_exp));
    }

    return lhs_sign ? lhs_sig > rhs_sig : lhs_sig < rhs_sig;
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType = decimal32_t, BOOST_DECIMAL_INTEGRAL T1,
        BOOST_DECIMAL_INTEGRAL U1, BOOST_DECIMAL_INTEGRAL T2, BOOST_DECIMAL_INTEGRAL U2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto less_parts_impl(T1 lhs_sig, U1 lhs_exp, bool lhs_sign,
                               T2 rhs_sig, U2 rhs_exp, bool rhs_sign, bool normalized = false) noexcept -> std::enable_if_t<detail::decimal_val_v<DecimalType> == 32, bool>
{
    using comp_type = std::uint_fast64_t;

    BOOST_DECIMAL_ASSERT(lhs_sig >= 0);
    BOOST_DECIMAL_ASSERT(rhs_sig >= 0);

    if (lhs_sign != rhs_sign)
    {
        return lhs_sign;
    }

    auto new_lhs_sig {static_cast<comp_type>(lhs_sig)};
    auto new_rhs_sig {static_cast<comp_type>(rhs_sig)};

    if (new_lhs_sig == UINT64_C(0) || new_rhs_sig == UINT64_C(0))
    {
        return (new_lhs_sig == new_rhs_sig) ? false : (new_lhs_sig == 0U ? !rhs_sign : lhs_sign);
    }

    const auto delta_exp {lhs_exp - rhs_exp};
    constexpr auto max_delta_diff {std::numeric_limits<std::uint_fast64_t>::digits10 - detail::precision_v<DecimalType>};

    // If we can't do this correctly without normalization then do it and try again
    if (delta_exp > max_delta_diff || delta_exp < -max_delta_diff)
    {
        if (!normalized)
        {
            detail::normalize(lhs_sig, lhs_exp);
            detail::normalize(rhs_sig, rhs_exp);
            return less_parts_impl(lhs_sig, lhs_exp, lhs_sign,
                                   rhs_sig, rhs_exp, rhs_sign, true);
        }
        else
        {
            return rhs_sign ? rhs_exp < lhs_exp : rhs_exp > lhs_exp;
        }
    }

    if (delta_exp >= 0)
    {
        new_lhs_sig *= detail::pow10(static_cast<comp_type>(delta_exp));
        lhs_exp -= delta_exp;
    }
    else
    {
        new_rhs_sig *= detail::pow10(static_cast<comp_type>(-delta_exp));
        rhs_exp += delta_exp;
    }

    if (lhs_exp != rhs_exp)
    {
        return lhs_sign ? lhs_exp > rhs_exp : lhs_exp < rhs_exp;
    }

    return lhs_sign ? new_lhs_sig > new_rhs_sig : new_lhs_sig < new_rhs_sig;
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType = decimal32_t, BOOST_DECIMAL_INTEGRAL T1,
        BOOST_DECIMAL_INTEGRAL U1, BOOST_DECIMAL_INTEGRAL T2, BOOST_DECIMAL_INTEGRAL U2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto less_parts_impl(T1 lhs_sig, U1 lhs_exp, bool lhs_sign,
                               T2 rhs_sig, U2 rhs_exp, bool rhs_sign) noexcept -> std::enable_if_t<detail::decimal_val_v<DecimalType> == 64 || detail::decimal_val_v<DecimalType> == 128, bool>
{
    using comp_type = typename DecimalType::significand_type;

    BOOST_DECIMAL_ASSERT(lhs_sig >= 0U);
    BOOST_DECIMAL_ASSERT(rhs_sig >= 0U);

    if (lhs_sign != rhs_sign)
    {
        return lhs_sign;
    }

    auto new_lhs_sig {static_cast<comp_type>(lhs_sig)};
    auto new_rhs_sig {static_cast<comp_type>(rhs_sig)};

    if (new_lhs_sig == UINT64_C(0) || new_rhs_sig == UINT64_C(0))
    {
        return (new_lhs_sig == new_rhs_sig) ? false : (new_lhs_sig == 0U ? !rhs_sign : lhs_sign);
    }

    detail::normalize<DecimalType>(new_lhs_sig, lhs_exp);
    detail::normalize<DecimalType>(new_rhs_sig, rhs_exp);

    if (lhs_exp != rhs_exp)
    {
        return lhs_sign ? lhs_exp > rhs_exp : lhs_exp < rhs_exp;
    }

    return lhs_sign ? new_lhs_sig > new_rhs_sig : new_lhs_sig < new_rhs_sig;
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType = decimal32_t, BOOST_DECIMAL_INTEGRAL T1,
          BOOST_DECIMAL_INTEGRAL U1, BOOST_DECIMAL_INTEGRAL T2, BOOST_DECIMAL_INTEGRAL U2>
constexpr auto less_parts_impl(T1 lhs_sig, U1 lhs_exp, bool lhs_sign,
                               T2 rhs_sig, U2 rhs_exp, bool rhs_sign) noexcept -> std::enable_if_t<detail::is_fast_type_v<DecimalType>, bool>
{
    using comp_type = std::conditional_t<(std::numeric_limits<T1>::digits10 > std::numeric_limits<T2>::digits10), T1, T2>;

    BOOST_DECIMAL_ASSERT(lhs_sig >= 0U);
    BOOST_DECIMAL_ASSERT(rhs_sig >= 0U);

    auto new_lhs_sig {static_cast<comp_type>(lhs_sig)};
    auto new_rhs_sig {static_cast<comp_type>(rhs_sig)};

    detail::normalize<DecimalType>(new_lhs_sig, lhs_exp);
    detail::normalize<DecimalType>(new_rhs_sig, rhs_exp);

    if (new_lhs_sig == 0U || new_rhs_sig == 0U)
    {
        if (new_lhs_sig == 0U && new_rhs_sig == 0U)
        {
            return false;
        }
        return new_lhs_sig == 0U ? !rhs_sign : lhs_sign;
    }

    if (lhs_sign != rhs_sign)
    {
        return lhs_sign;
    }

    if (lhs_exp != rhs_exp)
    {
        return lhs_sign ? lhs_exp > rhs_exp : lhs_exp < rhs_exp;
    }

    return lhs_sign ? new_lhs_sig > new_rhs_sig : new_lhs_sig < new_rhs_sig;
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal, BOOST_DECIMAL_INTEGRAL Integer>
BOOST_DECIMAL_CUDA_CONSTEXPR auto less_impl(Decimal lhs, Integer rhs) noexcept
    -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal> && detail::is_integral_v<Integer>), bool>
{
    using exp_type = typename Decimal::biased_exponent_type;

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(lhs))
    {
        return false;
    }
    else if (isinf(lhs))
    {
        return lhs.isneg();
    }
    #endif

    bool lhs_sign {lhs.isneg()};
    bool rhs_sign {false};

    BOOST_DECIMAL_IF_CONSTEXPR (detail::is_signed_v<Integer>)
    {
        if (rhs < static_cast<Integer>(0))
        {
            rhs_sign = true;
        }

        if (lhs_sign && !rhs_sign)
        {
            return true;
        }
        else if (!lhs_sign && rhs_sign)
        {
            return false;
        }
    }
    else
    {
        if (lhs_sign)
        {
            return true;
        }
    }

    const auto rhs_significand {detail::make_positive_unsigned(rhs)};

    return less_parts_impl<Decimal>(lhs.full_significand(), lhs.biased_exponent(), lhs_sign,
                                    rhs_significand, static_cast<exp_type>(0), rhs_sign);
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal1, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto mixed_decimal_less_impl(Decimal1 lhs, Decimal2 rhs) noexcept
    -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal1> &&
                         detail::is_decimal_floating_point_v<Decimal2>), bool>
{
    using Bigger_Decimal_Type = std::conditional_t<(sizeof(lhs) > sizeof(rhs)), Decimal1, Decimal2>;


    if (
            #ifndef BOOST_DECIMAL_FAST_MATH
            isnan(lhs) || isnan(rhs) ||
            #endif
            (!lhs.isneg() && rhs.isneg())
        )
    {
        return false;
    }
    else if (lhs.isneg() && !rhs.isneg())
    {
        return true;
    }
    #ifndef BOOST_DECIMAL_FAST_MATH
    else if (boost::decimal::isfinite(lhs) && isinf(rhs))
    {
        if (!signbit(rhs))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    #endif

    return less_parts_impl<Bigger_Decimal_Type>(lhs.full_significand(), lhs.biased_exponent(), lhs.isneg(),
                                                rhs.full_significand(), rhs.biased_exponent(), rhs.isneg());
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal1, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto operator<(Decimal1 lhs, Decimal2 rhs) noexcept
    -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal1> &&
                         detail::is_decimal_floating_point_v<Decimal2>), bool>
{
    return mixed_decimal_less_impl(lhs, rhs);
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal1, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto operator<=(Decimal1 lhs, Decimal2 rhs) noexcept
    -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal1> &&
                         detail::is_decimal_floating_point_v<Decimal2>), bool>
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(lhs) || isnan(rhs))
    {
        return false;
    }
    #endif

    return !(rhs < lhs);
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal1, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto operator>(Decimal1 lhs, Decimal2 rhs) noexcept
    -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal1> &&
                         detail::is_decimal_floating_point_v<Decimal2>), bool>
{
    return rhs < lhs;
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal1, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto operator>=(Decimal1 lhs, Decimal2 rhs) noexcept
    -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal1> &&
                         detail::is_decimal_floating_point_v<Decimal2>), bool>
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(lhs) || isnan(rhs))
    {
        return false;
    }
    #endif

    return !(lhs < rhs);
}

#ifdef BOOST_DECIMAL_HAS_SPACESHIP_OPERATOR

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal1, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal2>
BOOST_DECIMAL_CUDA_CONSTEXPR auto operator<=>(Decimal1 lhs, Decimal2 rhs) noexcept
    -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal1> &&
                         detail::is_decimal_floating_point_v<Decimal2>), std::partial_ordering>
{
    if (lhs < rhs)
    {
        return std::partial_ordering::less;
    }
    else if (lhs > rhs)
    {
        return std::partial_ordering::greater;
    }
    else if (lhs == rhs)
    {
        return std::partial_ordering::equivalent;
    }

    return std::partial_ordering::unordered;
}

#endif

} //namespace decimal
} //namespace boost

#endif //BOOST_DECIMAL_DETAIL_COMPARISON_HPP
