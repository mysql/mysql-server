// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
// Copyright 2025 - 2026 Justin Zhu
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_SQRT_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_SQRT_HPP

// ============================================================================
// Decimal sqrt: SoftFloat-style, split by precision
//
// Like SoftFloat's f32_sqrt.c / f64_sqrt.c / f128_sqrt.c:
// - sqrt32_impl.hpp  → decimal32  (7 digits,  ~23 bits,  f32-style)
// - sqrt64_impl.hpp  → decimal64  (16 digits, ~53 bits,  f64-style)
// - sqrt128_impl.hpp → decimal128 (34 digits, ~113 bits, f128-style)
//
// Each uses the shared tables from sqrt_tables.hpp.
// This file aggregates and dispatches by type.
// ============================================================================

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/cmath/frexp10.hpp>
#include <boost/decimal/detail/remove_trailing_zeros.hpp>
#include <boost/decimal/numbers.hpp>

// Implementation files (like SoftFloat's separate .c files)
#include <boost/decimal/detail/cmath/impl/sqrt_lookup.hpp>
#include <boost/decimal/detail/cmath/impl/sqrt32_impl.hpp>
#include <boost/decimal/detail/cmath/impl/sqrt64_impl.hpp>
#include <boost/decimal/detail/cmath/impl/sqrt128_impl.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <type_traits>
#include <cstdint>
#include <limits>
#endif

namespace boost {
namespace decimal {

namespace detail {

// Tag types for sqrt dispatch (C++14 compatible, avoids if constexpr)
struct sqrt_tag32 {};
struct sqrt_tag64 {};
struct sqrt_tag128 {};

template <typename T>
constexpr auto sqrt_impl_dispatch(T gx, int exp10val, sqrt_tag32) noexcept -> T;
template <typename T>
constexpr auto sqrt_impl_dispatch(T gx, int exp10val, sqrt_tag64) noexcept -> T;
template <typename T>
constexpr auto sqrt_impl_dispatch(T gx, int exp10val, sqrt_tag128) noexcept -> T;

// ============================================================================
// sqrt_impl: dispatch to precision-specific implementation
// ============================================================================

template <typename T>
constexpr auto sqrt_impl(T x) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    const auto fpc = fpclassify(x);

    // ---------- Special cases ----------
    #ifndef BOOST_DECIMAL_FAST_MATH
    if ((fpc == FP_NAN) || (fpc == FP_ZERO))
    {
        return x;
    }
    if (signbit(x))
    {
        return std::numeric_limits<T>::quiet_NaN();
    }
    if (fpc == FP_INFINITE)
    {
        return std::numeric_limits<T>::infinity();
    }
    #else
    if (signbit(x))
    {
        return T{0};
    }
    #endif

    // ---------- Extract significand and exponent (x = sig × 10^e) ----------
    int exp10val{};
    auto sig = frexp10(x, &exp10val);

    // ---------- Fast path: pure powers of 10 ----------
    const auto zeros_removal = remove_trailing_zeros(sig);
    const bool is_pure = (zeros_removal.trimmed_number == 1U);

    if (is_pure)
    {
        const int p10 = exp10val + static_cast<int>(zeros_removal.number_of_removed_zeros);

        if (p10 == 0)
        {
            return T{1};
        }

        const int p10_mod2 = (p10 % 2);
        T result = T{1, p10 / 2};

        if (p10_mod2 == 1)
        {
            result *= numbers::sqrt10_v<T>;
        }
        else if (p10_mod2 == -1)
        {
            result /= numbers::sqrt10_v<T>;
        }
        return result;
    }

    // ---------- Dispatch to precision-specific implementation (C++14 compatible) ----------
    constexpr int digits10 = std::numeric_limits<T>::digits10;

    // Create normalized value for the impl functions
    T gx{sig, -(digits10 - 1)};
    exp10val += (digits10 - 1);

    // Tag dispatch: avoids if constexpr for C++14 builds
    using dispatch_tag = typename std::conditional<
        (digits10 <= 7),
        sqrt_tag32,
        typename std::conditional<(digits10 <= 16), sqrt_tag64, sqrt_tag128>::type
    >::type;

    return sqrt_impl_dispatch(gx, exp10val, dispatch_tag());
}

template <typename T>
constexpr auto sqrt_impl_dispatch(T gx, int exp10val, sqrt_tag32) noexcept -> T
{
    return sqrt32_impl(gx, exp10val);
}

template <typename T>
constexpr auto sqrt_impl_dispatch(T gx, int exp10val, sqrt_tag64) noexcept -> T
{
    return sqrt64_impl(gx, exp10val);
}

template <typename T>
constexpr auto sqrt_impl_dispatch(T gx, int exp10val, sqrt_tag128) noexcept -> T
{
    return sqrt128_impl(gx, exp10val);
}

} // namespace detail

// ============================================================================
// Public sqrt function
// ============================================================================

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto sqrt(T val) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    using evaluation_type = detail::evaluation_type_t<T>;
    return static_cast<T>(detail::sqrt_impl(static_cast<evaluation_type>(val)));
}

} // namespace decimal
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_CMATH_SQRT_HPP
