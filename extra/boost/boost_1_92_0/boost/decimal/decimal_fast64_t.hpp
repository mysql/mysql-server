// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_decimal_fast64_t_HPP
#define BOOST_DECIMAL_decimal_fast64_t_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/attributes.hpp>
#include <boost/decimal/detail/apply_sign.hpp>
#include <boost/decimal/detail/bit_cast.hpp>
#include <boost/decimal/detail/config.hpp>
#include "detail/int128.hpp"
#include <boost/decimal/detail/fenv_rounding.hpp>
#include <boost/decimal/detail/integer_search_trees.hpp>
#include <boost/decimal/detail/parser.hpp>
#include <boost/decimal/detail/power_tables.hpp>
#include <boost/decimal/detail/ryu/ryu_generic_128.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/utilities.hpp>
#include <boost/decimal/detail/normalize.hpp>
#include <boost/decimal/detail/comparison.hpp>
#include <boost/decimal/detail/mixed_decimal_arithmetic.hpp>
#include <boost/decimal/detail/to_integral.hpp>
#include <boost/decimal/detail/to_float.hpp>
#include <boost/decimal/detail/to_decimal.hpp>
#include <boost/decimal/detail/promotion.hpp>
#include <boost/decimal/detail/check_non_finite.hpp>
#include <boost/decimal/detail/quantize_impl.hpp>
#include <boost/decimal/detail/shrink_significand.hpp>
#include <boost/decimal/detail/cmath/isfinite.hpp>
#include <boost/decimal/detail/cmath/fpclassify.hpp>
#include <boost/decimal/detail/cmath/abs.hpp>
#include <boost/decimal/detail/cmath/floor.hpp>
#include <boost/decimal/detail/cmath/ceil.hpp>
#include <boost/decimal/detail/add_impl.hpp>
#include <boost/decimal/detail/mul_impl.hpp>
#include <boost/decimal/detail/div_impl.hpp>
#include <boost/decimal/detail/promote_significand.hpp>
#include <boost/decimal/detail/components.hpp>
#include <boost/decimal/detail/cmath/next.hpp>
#include <boost/decimal/detail/to_chars_result.hpp>
#include <boost/decimal/detail/chars_format.hpp>
#include <boost/decimal/detail/construction_sign.hpp>
#include <boost/decimal/detail/from_chars_impl.hpp>
#include <boost/decimal/detail/mod_impl.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE

#include <limits>
#include <cstdint>

#endif

namespace boost {
namespace decimal {

namespace detail {

BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE auto d64_fast_inf {UINT64_C(0b1) << 61U};
BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE auto d64_fast_qnan {UINT64_C(0b11) << 61U};
BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE auto d64_fast_snan {UINT64_C(0b111) << 61U};

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
constexpr auto to_chars_scientific_impl(char* first, char* last, const TargetDecimalType& value, chars_format fmt) noexcept -> to_chars_result;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
constexpr auto to_chars_fixed_impl(char* first, char* last, const TargetDecimalType& value, chars_format fmt) noexcept -> to_chars_result;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
constexpr auto to_chars_hex_impl(char* first, char* last, const TargetDecimalType& value) noexcept -> to_chars_result;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
constexpr auto to_chars_cohort_preserving_scientific(char* first, char* last, const TargetDecimalType& value) noexcept -> to_chars_result;

template <bool checked, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto d64_fma_impl(T x, T y, T z) noexcept -> T;

template <typename TargetDecimalType, bool is_snan>
constexpr auto write_payload(typename TargetDecimalType::significand_type payload_value)
    BOOST_DECIMAL_REQUIRES(detail::is_fast_type_v, TargetDecimalType);

} // namespace detail

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4324) // Structure was padded due to alignment specifier
#endif

// 32-bit MSVC mis-codegens reads of bool members in over-aligned structs; use natural alignment there.
#if defined(_MSC_VER) && !defined(_M_X64) && !defined(_M_ARM64)
BOOST_DECIMAL_EXPORT class decimal_fast64_t final
#else
BOOST_DECIMAL_EXPORT class alignas(8) decimal_fast64_t final
#endif
{
public:
    using significand_type = std::uint64_t;
    using exponent_type = std::uint16_t;
    using biased_exponent_type = std::int32_t;

private:
    // In regular decimal64_t we have to decode the significand end exponent
    // Here we will store them directly to avoid the overhead of decoding

    significand_type significand_ {};
    exponent_type exponent_ {};
    bool sign_ {};
    char pad_[5] {};

    constexpr auto isneg() const noexcept -> bool
    {
        return sign_;
    }

    constexpr auto full_significand() const noexcept -> significand_type
    {
        return significand_;
    }

    constexpr auto unbiased_exponent() const noexcept -> exponent_type
    {
        return exponent_;
    }

    constexpr auto biased_exponent() const noexcept -> biased_exponent_type
    {
        return static_cast<biased_exponent_type>(exponent_) - detail::bias_v<decimal64_t>;
    }

    constexpr auto to_components() const noexcept -> detail::decimal_fast64_t_components
    {
        return {full_significand(), biased_exponent(), isneg()};
    }

    // Equality template between any integer type and decimal32_t
    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal, BOOST_DECIMAL_INTEGRAL Integer>
    friend constexpr auto mixed_equality_impl(Decimal lhs, Integer rhs) noexcept
        -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal> && detail::is_integral_v<Integer>), bool>;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal1, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal2>
    friend constexpr auto mixed_decimal_equality_impl(Decimal1 lhs, Decimal2 rhs) noexcept
        -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal1> &&
                             detail::is_decimal_floating_point_v<Decimal2>), bool>;

    // Template to compare operator< for any integer type and decimal32_t
    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal, BOOST_DECIMAL_INTEGRAL Integer>
    friend constexpr auto less_impl(Decimal lhs, Integer rhs) noexcept
        -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal> && detail::is_integral_v<Integer>), bool>;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal1, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal2>
    friend constexpr auto mixed_decimal_less_impl(Decimal1 lhs, Decimal2 rhs) noexcept
        -> std::enable_if_t<(detail::is_decimal_floating_point_v<Decimal1> &&
                             detail::is_decimal_floating_point_v<Decimal2>), bool>;

    template <typename Decimal, typename TargetType>
    friend constexpr auto to_integral(Decimal val) noexcept
        BOOST_DECIMAL_REQUIRES_TWO_RETURN(detail::is_decimal_floating_point_v, Decimal, detail::is_integral_v, TargetType, TargetType);

    template <typename Decimal, typename TargetType>
    friend BOOST_DECIMAL_CXX20_CONSTEXPR auto to_float(Decimal val) noexcept
        BOOST_DECIMAL_REQUIRES_TWO_RETURN(detail::is_decimal_floating_point_v, Decimal, detail::is_floating_point_v, TargetType, TargetType);

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetType, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal>
    friend constexpr auto to_decimal(Decimal val) noexcept -> TargetType;

    friend constexpr auto d64_fast_div_impl(const decimal_fast64_t& lhs, const decimal_fast64_t& rhs, decimal_fast64_t& q, decimal_fast64_t& r) noexcept -> void;

    template <typename T>
    friend constexpr auto ilogb(T d) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, T, int);

    template <typename T>
    friend constexpr auto logb(T num) noexcept
        BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T);

    friend constexpr auto not_finite(decimal_fast64_t val) noexcept -> bool;

    template <typename DecimalType>
    friend constexpr auto to_dpd_d64(DecimalType val) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, DecimalType, std::uint64_t);

    template <typename ReturnType, typename T>
    friend constexpr auto detail::add_impl(const T& lhs, const T& rhs) noexcept -> ReturnType;

    template <typename ReturnType, typename T>
    friend constexpr auto detail::d64_mul_impl(const T& lhs, const T& rhs) noexcept -> ReturnType;

    template <BOOST_DECIMAL_FAST_DECIMAL_FLOATING_TYPE DecimalType>
    BOOST_DECIMAL_FORCE_INLINE friend constexpr auto fast_equality_impl(const DecimalType& lhs, const DecimalType& rhs) noexcept -> bool;

    template <BOOST_DECIMAL_FAST_DECIMAL_FLOATING_TYPE DecimalType>
    BOOST_DECIMAL_FORCE_INLINE friend constexpr auto fast_inequality_impl(const DecimalType& lhs, const DecimalType& rhs) noexcept -> bool;

    template <BOOST_DECIMAL_FAST_DECIMAL_FLOATING_TYPE DecimalType>
    BOOST_DECIMAL_FORCE_INLINE friend constexpr auto fast_less_impl(const DecimalType& lhs, const DecimalType& rhs) noexcept -> bool;

    template <BOOST_DECIMAL_FAST_DECIMAL_FLOATING_TYPE DecimalType>
    BOOST_DECIMAL_FORCE_INLINE friend constexpr auto fast_less_equal_impl(const DecimalType& lhs, const DecimalType& rhs) noexcept -> bool;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType>
    friend constexpr auto detail::nextafter_impl(DecimalType val, bool direction) noexcept -> DecimalType;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
    friend constexpr auto detail::to_chars_scientific_impl(char* first, char* last, const TargetDecimalType& value, chars_format fmt) noexcept -> to_chars_result;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
    friend constexpr auto detail::to_chars_fixed_impl(char* first, char* last, const TargetDecimalType& value, const chars_format fmt) noexcept -> to_chars_result;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
    friend constexpr auto detail::to_chars_hex_impl(char* first, char* last, const TargetDecimalType& value) noexcept -> to_chars_result;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
    friend constexpr auto detail::to_chars_cohort_preserving_scientific(char* first, char* last, const TargetDecimalType& value) noexcept -> to_chars_result;

    template <bool checked, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
    friend constexpr auto detail::d64_fma_impl(T x, T y, T z) noexcept -> T;

    template <typename TargetDecimalType, bool is_snan>
    friend constexpr auto detail::write_payload(typename TargetDecimalType::significand_type payload_value)
        BOOST_DECIMAL_REQUIRES(detail::is_fast_type_v, TargetDecimalType);

    template <typename T>
    friend constexpr auto read_payload(T value) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_fast_type_v, T, typename T::significand_type);

    #if !defined(BOOST_DECIMAL_DISABLE_CLIB)
    constexpr decimal_fast64_t(const char* str, std::size_t len);
    #endif

    friend constexpr auto nan_conversion(const decimal_fast64_t value) noexcept -> decimal_fast64_t
    {
        constexpr auto convert_nan_mask {detail::d64_fast_qnan ^ detail::d64_fast_snan};

        decimal_fast64_t return_value {value};
        return_value.significand_ ^= convert_nan_mask;

        return return_value;
    }

    template <typename Decimal>
    friend constexpr Decimal detail::check_non_finite(Decimal lhs, Decimal rhs) noexcept;

    template <typename Decimal>
    friend constexpr Decimal detail::check_non_finite(Decimal x) noexcept;

public:
    constexpr decimal_fast64_t() noexcept = default;

    #ifdef BOOST_DECIMAL_HAS_CONCEPTS
    template <BOOST_DECIMAL_UNSIGNED_INTEGRAL T1, BOOST_DECIMAL_INTEGRAL T2>
    #else
    template <typename T1, typename T2, std::enable_if_t<detail::is_unsigned_v<T1> && detail::is_integral_v<T2>, bool> = true>
    #endif
    constexpr decimal_fast64_t(T1 coeff, T2 exp, detail::construction_sign_wrapper resultant_sign = construction_sign::positive) noexcept;

    #ifdef BOOST_DECIMAL_HAS_CONCEPTS
    template <BOOST_DECIMAL_SIGNED_INTEGRAL T1, BOOST_DECIMAL_INTEGRAL T2>
    #else
    template <typename T1, typename T2, std::enable_if_t<!detail::is_unsigned_v<T1> && detail::is_integral_v<T2>, bool> = true>
    #endif
    constexpr decimal_fast64_t(T1, T2, bool) noexcept { static_assert(detail::is_unsigned_v<T1>, "Construction from signed integer, exponent, and sign is ambiguous, so it is disallowed. You must use an Unsigned Integer for the coefficient to construct from {coefficient, exponent, sign}"); }

    #ifdef BOOST_DECIMAL_HAS_CONCEPTS
    template <BOOST_DECIMAL_SIGNED_INTEGRAL T1, BOOST_DECIMAL_INTEGRAL T2>
    #else
    template <typename T1, typename T2, std::enable_if_t<!detail::is_unsigned_v<T1> && detail::is_integral_v<T2>, bool> = true>
    #endif
    constexpr decimal_fast64_t(T1 coeff, T2 exp) noexcept;

    explicit constexpr decimal_fast64_t(bool value) noexcept;

    #ifdef BOOST_DECIMAL_HAS_CONCEPTS
    template <BOOST_DECIMAL_INTEGRAL Integer>
    #else
    template <typename Integer, std::enable_if_t<detail::is_integral_v<Integer>, bool> = true>
    #endif
    #if !defined(BOOST_DECIMAL_ALLOW_IMPLICIT_CONVERSIONS) && !defined(BOOST_DECIMAL_ALLOW_IMPLICIT_INTEGER_CONVERSIONS)
    explicit
    #endif
    constexpr decimal_fast64_t(Integer val) noexcept;

    #ifdef BOOST_DECIMAL_HAS_CONCEPTS
    template <BOOST_DECIMAL_REAL Float>
    #else
    template <typename Float, std::enable_if_t<detail::is_floating_point_v<Float>, bool> = true>
    #endif
    #if !defined(BOOST_DECIMAL_ALLOW_IMPLICIT_CONVERSIONS) && !defined(BOOST_DECIMAL_ALLOW_IMPLICIT_FLOAT_CONVERSIONS)
    explicit
    #endif
    BOOST_DECIMAL_CXX20_CONSTEXPR decimal_fast64_t(Float val) noexcept;

    #ifdef BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE
    explicit constexpr decimal_fast64_t(long double val) noexcept = delete;
    #endif

    friend constexpr auto direct_init_d64(decimal_fast64_t::significand_type significand, decimal_fast64_t::exponent_type exponent, bool sign) noexcept -> decimal_fast64_t;

    #if !defined(BOOST_DECIMAL_DISABLE_CLIB)

    explicit constexpr decimal_fast64_t(const char* str);

    #ifndef BOOST_DECIMAL_HAS_STD_STRING_VIEW
    explicit inline decimal_fast64_t(const std::string& str);
    #else
    explicit constexpr decimal_fast64_t(std::string_view str);
    #endif

    #endif // BOOST_DECIMAL_DISABLE_CLIB

    // Classification functions
    friend constexpr auto signbit(decimal_fast64_t val) noexcept -> bool;
    friend constexpr auto isinf(decimal_fast64_t val) noexcept -> bool;
    friend constexpr auto isnan(decimal_fast64_t val) noexcept -> bool;
    friend constexpr auto issignaling(decimal_fast64_t val) noexcept -> bool;
    friend constexpr auto isnormal(decimal_fast64_t val) noexcept -> bool;
    friend constexpr auto isfinite(decimal_fast64_t val) noexcept -> bool;

    // Comparison operator
    friend constexpr auto operator==(decimal_fast64_t lhs, decimal_fast64_t rhs) noexcept -> bool;
    friend constexpr auto operator!=(decimal_fast64_t lhs, decimal_fast64_t rhs) noexcept -> bool;
    friend constexpr auto operator<(decimal_fast64_t lhs, decimal_fast64_t rhs) noexcept -> bool;
    friend constexpr auto operator<=(decimal_fast64_t lhs, decimal_fast64_t rhs) noexcept -> bool;
    friend constexpr auto operator>(decimal_fast64_t lhs, decimal_fast64_t rhs) noexcept -> bool;
    friend constexpr auto operator>=(decimal_fast64_t lhs, decimal_fast64_t rhs) noexcept -> bool;

    // Mixed type comparison operators
    template <typename Integer>
    friend constexpr auto operator==(decimal_fast64_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator==(Integer lhs, decimal_fast64_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator!=(decimal_fast64_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator!=(Integer lhs, decimal_fast64_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator<(decimal_fast64_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator<(Integer lhs, decimal_fast64_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator<=(decimal_fast64_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator<=(Integer lhs, decimal_fast64_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator>(decimal_fast64_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator>(Integer lhs, decimal_fast64_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator>=(decimal_fast64_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator>=(Integer lhs, decimal_fast64_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    // C++20 Spaceship operator
    #ifdef BOOST_DECIMAL_HAS_SPACESHIP_OPERATOR
    friend constexpr auto operator<=>(decimal_fast64_t lhs, decimal_fast64_t rhs) noexcept -> std::partial_ordering;

    template <typename Integer>
    friend constexpr auto operator<=>(decimal_fast64_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, std::partial_ordering);

    template <typename Integer>
    friend constexpr auto operator<=>(Integer lhs, decimal_fast64_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, std::partial_ordering);
    #endif

    // Conversions
    explicit constexpr operator bool() const noexcept;
    explicit constexpr operator int() const noexcept;
    explicit constexpr operator unsigned() const noexcept;
    explicit constexpr operator long() const noexcept;
    explicit constexpr operator unsigned long() const noexcept;
    explicit constexpr operator long long() const noexcept;
    explicit constexpr operator unsigned long long() const noexcept;

    #ifdef BOOST_DECIMAL_HAS_INT128
    explicit constexpr operator detail::builtin_int128_t() const noexcept;
    explicit constexpr operator detail::builtin_uint128_t() const noexcept;
    #endif

    explicit BOOST_DECIMAL_CXX20_CONSTEXPR operator float() const noexcept;
    explicit BOOST_DECIMAL_CXX20_CONSTEXPR operator double() const noexcept;
    #ifndef BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE
    explicit BOOST_DECIMAL_CXX20_CONSTEXPR operator long double() const noexcept;
    #endif

    #ifdef BOOST_DECIMAL_HAS_FLOAT16
    explicit constexpr operator std::float16_t() const noexcept;
    #endif
    #ifdef BOOST_DECIMAL_HAS_FLOAT32
    explicit constexpr operator std::float32_t() const noexcept;
    #endif
    #ifdef BOOST_DECIMAL_HAS_FLOAT64
    explicit constexpr operator std::float64_t() const noexcept;
    #endif
    #ifdef BOOST_DECIMAL_HAS_BRAINFLOAT16
    explicit constexpr operator std::bfloat16_t() const noexcept;
    #endif

    // Conversion to other decimal type
    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal, std::enable_if_t<detail::is_decimal_floating_point_v<Decimal> && (detail::decimal_val_v<Decimal> > detail::decimal_val_v<decimal_fast64_t>), bool> = true>
    constexpr operator Decimal() const noexcept;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal, std::enable_if_t<detail::is_decimal_floating_point_v<Decimal> && (detail::decimal_val_v<Decimal> <= detail::decimal_val_v<decimal_fast64_t>), bool> = true>
    explicit constexpr operator Decimal() const noexcept;

    // Unary Operators
    friend constexpr auto operator+(decimal_fast64_t val) noexcept -> decimal_fast64_t;
    friend constexpr auto operator-(decimal_fast64_t val) noexcept -> decimal_fast64_t;

    // Basic arithmetic operators
    friend constexpr auto operator+(decimal_fast64_t lhs, decimal_fast64_t rhs) noexcept -> decimal_fast64_t;
    friend constexpr auto operator-(decimal_fast64_t lhs, decimal_fast64_t rhs) noexcept -> decimal_fast64_t;
    friend constexpr auto operator*(decimal_fast64_t lhs, decimal_fast64_t rhs) noexcept -> decimal_fast64_t;
    friend constexpr auto operator/(const decimal_fast64_t& lhs, const decimal_fast64_t& rhs) noexcept -> decimal_fast64_t;
    friend constexpr auto operator%(decimal_fast64_t lhs, decimal_fast64_t rhs) noexcept -> decimal_fast64_t;

    // Mixed type arithmetic operators
    template <typename Integer>
    friend constexpr auto operator+(decimal_fast64_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t);

    template <typename Integer>
    friend constexpr auto operator+(Integer lhs, decimal_fast64_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t);

    template <typename Integer>
    friend constexpr auto operator-(decimal_fast64_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t);

    template <typename Integer>
    friend constexpr auto operator-(Integer lhs, decimal_fast64_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t);

    template <typename Integer>
    friend constexpr auto operator*(decimal_fast64_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t);

    template <typename Integer>
    friend constexpr auto operator*(Integer lhs, decimal_fast64_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t);

    template <typename Integer>
    friend constexpr auto operator/(decimal_fast64_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t);

    template <typename Integer>
    friend constexpr auto operator/(Integer lhs, decimal_fast64_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t);

    // Compound Operators
    constexpr auto operator+=(decimal_fast64_t rhs) noexcept -> decimal_fast64_t&;
    constexpr auto operator-=(decimal_fast64_t rhs) noexcept -> decimal_fast64_t&;
    constexpr auto operator*=(decimal_fast64_t rhs) noexcept -> decimal_fast64_t&;
    constexpr auto operator/=(decimal_fast64_t rhs) noexcept -> decimal_fast64_t&;

    // Mixed type compound operators
    template <typename Integer>
    constexpr auto operator+=(Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t&);

    template <typename Integer>
    constexpr auto operator-=(Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t&);

    template <typename Integer>
    constexpr auto operator*=(Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t&);

    template <typename Integer>
    constexpr auto operator/=(Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t&);

    // Increment and decrement
    constexpr auto operator++() noexcept -> decimal_fast64_t&;
    constexpr auto operator++(int) noexcept -> decimal_fast64_t;
    constexpr auto operator--() noexcept -> decimal_fast64_t&;
    constexpr auto operator--(int) noexcept -> decimal_fast64_t;

    // Cmath friend functions
    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
    friend constexpr auto frexp10(T num, int* expptr) noexcept -> typename T::significand_type;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
    friend BOOST_DECIMAL_CUDA_CONSTEXPR auto decompose(const T x) noexcept;

    friend constexpr auto copysignd64f(decimal_fast64_t mag, decimal_fast64_t sgn) noexcept -> decimal_fast64_t;
    friend constexpr auto scalbnd64f(decimal_fast64_t num, int exp) noexcept -> decimal_fast64_t;
    friend constexpr auto scalblnd64f(decimal_fast64_t num, long exp) noexcept -> decimal_fast64_t;
    friend constexpr auto samequantumd64f(decimal_fast64_t lhs, decimal_fast64_t rhs) noexcept -> bool;
    friend constexpr auto quantexpd64f(decimal_fast64_t x) noexcept -> int;
    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
    friend BOOST_DECIMAL_CUDA_CONSTEXPR auto quantize(T lhs, T rhs) noexcept -> T;
};

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#ifdef BOOST_DECIMAL_HAS_CONCEPTS
template <BOOST_DECIMAL_UNSIGNED_INTEGRAL T1, BOOST_DECIMAL_INTEGRAL T2>
#else
template <typename T1, typename T2, std::enable_if_t<detail::is_unsigned_v<T1> && detail::is_integral_v<T2>, bool>>
#endif
constexpr decimal_fast64_t::decimal_fast64_t(T1 coeff, T2 exp, const detail::construction_sign_wrapper resultant_sign) noexcept
{
    using minimum_coefficient_size = std::conditional_t<(sizeof(T1) > sizeof(significand_type)), T1, significand_type>;

    minimum_coefficient_size min_coeff {coeff};

    const auto is_negative {static_cast<bool>(resultant_sign)};
    sign_ = is_negative;

    // IEEE 754-2008 3.5.1: zero has a cohort with one representation per exponent.
    // Skip normalization for zero (which would otherwise expand the significand and shift the exponent)
    // and clamp the requested exponent to the representable range.
    if (min_coeff == minimum_coefficient_size{0})
    {
        significand_ = static_cast<significand_type>(0);
        auto biased_exp {static_cast<int>(exp) + detail::bias_v<decimal64_t>};
        if (biased_exp < 0)
        {
            biased_exp = 0;
        }
        else if (biased_exp > detail::max_biased_exp_v<decimal64_t>)
        {
            biased_exp = detail::max_biased_exp_v<decimal64_t>;
        }
        exponent_ = static_cast<exponent_type>(biased_exp);
        return;
    }

    // Fast path: if the coefficient already has exactly precision-many digits,
    // normalize is a no-op apart from a num_digits call. Skip the call entirely.
    constexpr minimum_coefficient_size min_normal_significand {
        detail::pow10(static_cast<minimum_coefficient_size>(detail::precision_v<decimal_fast64_t> - 1))};
    constexpr minimum_coefficient_size max_normal_significand {
        static_cast<minimum_coefficient_size>(detail::max_significand_v<decimal_fast64_t>)};

    if (min_coeff < min_normal_significand || min_coeff > max_normal_significand)
    {
        // Normalize the value, so we don't have to worry about it with operations
        detail::normalize<decimal_fast64_t>(min_coeff, exp, is_negative);
    }

    significand_ = static_cast<significand_type>(min_coeff);

    const auto biased_exp {static_cast<int>(exp) + detail::bias_v<decimal64_t>};

    if (biased_exp > detail::max_biased_exp_v<decimal64_t>)
    {
        significand_ = detail::d64_fast_inf;
    }
    else if (biased_exp >= 0)
    {
        exponent_ = static_cast<exponent_type>(biased_exp);
    }
    else
    {
        // Flush denorms to zero, preserving sign per IEEE 754-2008 3.5.1
        significand_ = static_cast<significand_type>(0);
        exponent_ = static_cast<exponent_type>(0);
    }
}

#ifdef BOOST_DECIMAL_HAS_CONCEPTS
template <BOOST_DECIMAL_SIGNED_INTEGRAL T1, BOOST_DECIMAL_INTEGRAL T2>
#else
template <typename T1, typename T2, std::enable_if_t<!detail::is_unsigned_v<T1> && detail::is_integral_v<T2>, bool>>
#endif
constexpr decimal_fast64_t::decimal_fast64_t(const T1 coeff, const T2 exp) noexcept : decimal_fast64_t(detail::make_positive_unsigned(coeff), exp, coeff < 0) {}

constexpr decimal_fast64_t::decimal_fast64_t(const bool value) noexcept : decimal_fast64_t(static_cast<significand_type>(value), 0, false) {}

#ifdef BOOST_DECIMAL_HAS_CONCEPTS
template <BOOST_DECIMAL_INTEGRAL Integer>
#else
template <typename Integer, std::enable_if_t<detail::is_integral_v<Integer>, bool>>
#endif
constexpr decimal_fast64_t::decimal_fast64_t(const Integer val) noexcept : decimal_fast64_t{val, 0} {}

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#ifdef BOOST_DECIMAL_HAS_CONCEPTS
template <BOOST_DECIMAL_REAL Float>
#else
template <typename Float, std::enable_if_t<detail::is_floating_point_v<Float>, bool>>
#endif
BOOST_DECIMAL_CXX20_CONSTEXPR decimal_fast64_t::decimal_fast64_t(const Float val) noexcept
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (val != val)
    {
        significand_ = detail::d64_fast_qnan;
    }
    else if (val == std::numeric_limits<Float>::infinity())
    {
        significand_ = detail::d64_fast_inf;
    }
    else if (val == -std::numeric_limits<Float>::infinity())
    {
        significand_ = detail::d64_fast_inf;
        sign_ = true;
    }
    else
    #endif
    {
        const auto components {detail::ryu::floating_point_to_fd128(val)};
        *this = decimal_fast64_t {components.mantissa, components.exponent, components.sign};
    }
}

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

constexpr auto direct_init_d64(const decimal_fast64_t::significand_type significand,
                               const decimal_fast64_t::exponent_type exponent,
                               const bool sign) noexcept -> decimal_fast64_t
{
    decimal_fast64_t val {};
    val.significand_ = significand;
    val.exponent_ = exponent;
    val.sign_ = sign;

    return val;
}

namespace detail {

template <bool>
class numeric_limits_impl64f
{
public:

    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool has_signaling_NaN = true;

    // These members were deprecated in C++23
    #if ((!defined(_MSC_VER) && (__cplusplus <= 202002L)) || (defined(_MSC_VER) && (_MSVC_LANG <= 202002L)))
    static constexpr std::float_denorm_style has_denorm = std::denorm_present;
    static constexpr bool has_denorm_loss = true;
    #endif

    static constexpr std::float_round_style round_style = std::round_indeterminate;
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr int  digits = 16;
    static constexpr int  digits10 = digits;
    static constexpr int  max_digits10 = digits;
    static constexpr int  radix = 10;
    static constexpr int  min_exponent = -383;
    static constexpr int  min_exponent10 = min_exponent;
    static constexpr int  max_exponent = 384;
    static constexpr int  max_exponent10 = max_exponent;
    static constexpr bool traps = std::numeric_limits<std::uint64_t>::traps;
    static constexpr bool tinyness_before = true;

    // Member functions
    static constexpr auto (min)        () -> boost::decimal::decimal_fast64_t { return {UINT32_C(1), min_exponent}; }
    static constexpr auto (max)        () -> boost::decimal::decimal_fast64_t { return {UINT64_C(9'999'999'999'999'999), max_exponent - digits + 1}; }
    static constexpr auto lowest       () -> boost::decimal::decimal_fast64_t { return {UINT64_C(9'999'999'999'999'999), max_exponent - digits + 1, construction_sign::negative}; }
    static constexpr auto epsilon      () -> boost::decimal::decimal_fast64_t { return {UINT32_C(1), -digits + 1}; }
    static constexpr auto round_error  () -> boost::decimal::decimal_fast64_t { return epsilon(); }
    static constexpr auto infinity     () -> boost::decimal::decimal_fast64_t { return boost::decimal::direct_init_d64(
                boost::decimal::detail::d64_fast_inf, 0, false); }
    static constexpr auto quiet_NaN    () -> boost::decimal::decimal_fast64_t { return boost::decimal::direct_init_d64(
                boost::decimal::detail::d64_fast_qnan, 0, false); }
    static constexpr auto signaling_NaN() -> boost::decimal::decimal_fast64_t { return boost::decimal::direct_init_d64(
                boost::decimal::detail::d64_fast_snan, 0, false); }
    static constexpr auto denorm_min   () -> boost::decimal::decimal_fast64_t { return min(); }
};

#if !defined(__cpp_inline_variables) || __cpp_inline_variables < 201606L

template <bool b> constexpr bool numeric_limits_impl64f<b>::is_specialized;
template <bool b> constexpr bool numeric_limits_impl64f<b>::is_signed;
template <bool b> constexpr bool numeric_limits_impl64f<b>::is_integer;
template <bool b> constexpr bool numeric_limits_impl64f<b>::is_exact;
template <bool b> constexpr bool numeric_limits_impl64f<b>::has_infinity;
template <bool b> constexpr bool numeric_limits_impl64f<b>::has_quiet_NaN;
template <bool b> constexpr bool numeric_limits_impl64f<b>::has_signaling_NaN;

// These members were deprecated in C++23
#if ((!defined(_MSC_VER) && (__cplusplus <= 202002L)) || (defined(_MSC_VER) && (_MSVC_LANG <= 202002L)))
template <bool b> constexpr std::float_denorm_style numeric_limits_impl64f<b>::has_denorm;
template <bool b> constexpr bool numeric_limits_impl64f<b>::has_denorm_loss;
#endif

template <bool b> constexpr std::float_round_style numeric_limits_impl64f<b>::round_style;
template <bool b> constexpr bool numeric_limits_impl64f<b>::is_iec559;
template <bool b> constexpr bool numeric_limits_impl64f<b>::is_bounded;
template <bool b> constexpr bool numeric_limits_impl64f<b>::is_modulo;
template <bool b> constexpr int numeric_limits_impl64f<b>::digits;
template <bool b> constexpr int numeric_limits_impl64f<b>::digits10;
template <bool b> constexpr int numeric_limits_impl64f<b>::max_digits10;
template <bool b> constexpr int numeric_limits_impl64f<b>::radix;
template <bool b> constexpr int numeric_limits_impl64f<b>::min_exponent;
template <bool b> constexpr int numeric_limits_impl64f<b>::min_exponent10;
template <bool b> constexpr int numeric_limits_impl64f<b>::max_exponent;
template <bool b> constexpr int numeric_limits_impl64f<b>::max_exponent10;
template <bool b> constexpr bool numeric_limits_impl64f<b>::traps;
template <bool b> constexpr bool numeric_limits_impl64f<b>::tinyness_before;

#endif // !defined(__cpp_inline_variables) || __cpp_inline_variables < 201606L

} // namespace detail

} // namespace decimal
} // namespace boost

namespace std {

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wmismatched-tags"
#endif

template <>
class numeric_limits<boost::decimal::decimal_fast64_t> :
    public boost::decimal::detail::numeric_limits_impl64f<true> {};

#ifdef __clang__
#  pragma clang diagnostic pop
#endif

} // namespace std

namespace boost {
namespace decimal {

constexpr auto signbit(const decimal_fast64_t val) noexcept -> bool
{
    return val.sign_;
}

constexpr auto isinf(const decimal_fast64_t val) noexcept -> bool
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    return val.significand_ == detail::d64_fast_inf;
    #else
    static_cast<void>(val);
    return false;
    #endif
}

constexpr auto isnan(const decimal_fast64_t val) noexcept -> bool
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    return val.significand_ >= detail::d64_fast_qnan;
    #else
    static_cast<void>(val);
    return false;
    #endif
}

constexpr auto issignaling(const decimal_fast64_t val) noexcept -> bool
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    return val.significand_ >= detail::d64_fast_snan;
    #else
    static_cast<void>(val);
    return false;
    #endif
}

constexpr auto isnormal(const decimal_fast64_t val) noexcept -> bool
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (val.exponent_ <= static_cast<decimal_fast64_t::exponent_type>(detail::precision_v<decimal64_t> - 1))
    {
        return false;
    }

    return (val.significand_ != 0) && isfinite(val);
    #else
    return val.significand_ != 0;
    #endif
}

constexpr auto isfinite(const decimal_fast64_t val) noexcept -> bool
{
    return val.significand_ < detail::d64_fast_inf;
}

BOOST_DECIMAL_FORCE_INLINE constexpr auto not_finite(const decimal_fast64_t val) noexcept -> bool
{
    return val.significand_ >= detail::d64_fast_inf;
}

constexpr auto operator==(const decimal_fast64_t lhs, const decimal_fast64_t rhs) noexcept -> bool
{
    return fast_equality_impl(lhs, rhs);
}

template <typename Integer>
constexpr auto operator==(const decimal_fast64_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    return mixed_equality_impl(lhs, rhs);
}

template <typename Integer>
constexpr auto operator==(const Integer lhs, const decimal_fast64_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    return mixed_equality_impl(rhs, lhs);
}

constexpr auto operator!=(const decimal_fast64_t lhs, const decimal_fast64_t rhs) noexcept -> bool
{
    return fast_inequality_impl(lhs, rhs);
}

template <typename Integer>
constexpr auto operator!=(const decimal_fast64_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    return !(lhs == rhs);
}

template <typename Integer>
constexpr auto operator!=(const Integer lhs, const decimal_fast64_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    return !(lhs == rhs);
}

constexpr auto operator<(const decimal_fast64_t lhs, const decimal_fast64_t rhs) noexcept -> bool
{
    return fast_less_impl(lhs, rhs);
}

template <typename Integer>
constexpr auto operator<(const decimal_fast64_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    return less_impl(lhs, rhs);
}

template <typename Integer>
constexpr auto operator<(const Integer lhs, const decimal_fast64_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(rhs))
    {
        return false;
    }
    #endif

    return !less_impl(rhs, lhs) && lhs != rhs;
}

constexpr auto operator<=(const decimal_fast64_t lhs, const decimal_fast64_t rhs) noexcept -> bool
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(lhs) || isnan(rhs))
    {
        return false;
    }
    #endif

    return !(rhs < lhs);
}

template <typename Integer>
constexpr auto operator<=(const decimal_fast64_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(lhs))
    {
        return false;
    }
    #endif

    return !(rhs < lhs);
}

template <typename Integer>
constexpr auto operator<=(const Integer lhs, const decimal_fast64_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(rhs))
    {
        return false;
    }
    #endif

    return !(rhs < lhs);
}

constexpr auto operator>(const decimal_fast64_t lhs, const decimal_fast64_t rhs) noexcept -> bool
{
    return rhs < lhs;
}

template <typename Integer>
constexpr auto operator>(const decimal_fast64_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    return rhs < lhs;
}

template <typename Integer>
constexpr auto operator>(const Integer lhs, const decimal_fast64_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    return rhs < lhs;
}

constexpr auto operator>=(const decimal_fast64_t lhs, const decimal_fast64_t rhs) noexcept -> bool
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(lhs) || isnan(rhs))
    {
        return false;
    }
    #endif

    return !(lhs < rhs);
}

template <typename Integer>
constexpr auto operator>=(const decimal_fast64_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(lhs))
    {
        return false;
    }
    #endif

    return !(lhs < rhs);
}

template <typename Integer>
constexpr auto operator>=(const Integer lhs, const decimal_fast64_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(rhs))
    {
        return false;
    }
    #endif

    return !(lhs < rhs);
}

#ifdef BOOST_DECIMAL_HAS_SPACESHIP_OPERATOR

constexpr auto operator<=>(const decimal_fast64_t lhs, const decimal_fast64_t rhs) noexcept -> std::partial_ordering
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

template <typename Integer>
constexpr auto operator<=>(const decimal_fast64_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, std::partial_ordering)
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

template <typename Integer>
constexpr auto operator<=>(const Integer lhs, const decimal_fast64_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, std::partial_ordering)
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

#endif // BOOST_DECIMAL_HAS_SPACESHIP_OPERATOR

constexpr auto operator+(const decimal_fast64_t val) noexcept -> decimal_fast64_t
{
    return val;
}

constexpr auto operator-(decimal_fast64_t val) noexcept -> decimal_fast64_t
{
    val.sign_ = !val.sign_;
    return val;
}

constexpr decimal_fast64_t::operator bool() const noexcept
{
    constexpr decimal_fast64_t zero {0, 0};
    return *this != zero;
}

constexpr decimal_fast64_t::operator int() const noexcept
{
    return to_integral<decimal_fast64_t, int>(*this);
}

constexpr decimal_fast64_t::operator unsigned() const noexcept
{
    return to_integral<decimal_fast64_t, unsigned>(*this);
}

constexpr decimal_fast64_t::operator long() const noexcept
{
    return to_integral<decimal_fast64_t, long>(*this);
}

constexpr decimal_fast64_t::operator unsigned long() const noexcept
{
    return to_integral<decimal_fast64_t, unsigned long>(*this);
}

constexpr decimal_fast64_t::operator long long() const noexcept
{
    return to_integral<decimal_fast64_t, long long>(*this);
}

constexpr decimal_fast64_t::operator unsigned long long() const noexcept
{
    return to_integral<decimal_fast64_t, unsigned long long>(*this);
}

#ifdef BOOST_DECIMAL_HAS_INT128

constexpr decimal_fast64_t::operator detail::builtin_int128_t() const noexcept
{
    return to_integral<decimal_fast64_t, detail::builtin_int128_t>(*this);
}

constexpr decimal_fast64_t::operator detail::builtin_uint128_t() const noexcept
{
    return to_integral<decimal_fast64_t, detail::builtin_uint128_t>(*this);
}

#endif // BOOST_DECIMAL_HAS_INT128

BOOST_DECIMAL_CXX20_CONSTEXPR decimal_fast64_t::operator float() const noexcept
{
    return to_float<decimal_fast64_t, float>(*this);
}

BOOST_DECIMAL_CXX20_CONSTEXPR decimal_fast64_t::operator double() const noexcept
{
    return to_float<decimal_fast64_t, double>(*this);
}

#ifndef BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE
BOOST_DECIMAL_CXX20_CONSTEXPR decimal_fast64_t::operator long double() const noexcept
{
    return to_float<decimal_fast64_t, long double>(*this);
}
#endif

#ifdef BOOST_DECIMAL_HAS_FLOAT16
constexpr decimal_fast64_t::operator std::float16_t() const noexcept
{
    return static_cast<std::float16_t>(to_float<decimal_fast64_t, float>(*this));
}
#endif
#ifdef BOOST_DECIMAL_HAS_FLOAT32
constexpr decimal_fast64_t::operator std::float32_t() const noexcept
{
    return static_cast<std::float32_t>(to_float<decimal_fast64_t, float>(*this));
}
#endif
#ifdef BOOST_DECIMAL_HAS_FLOAT64
constexpr decimal_fast64_t::operator std::float64_t() const noexcept
{
    return static_cast<std::float64_t>(to_float<decimal_fast64_t, double>(*this));
}
#endif
#ifdef BOOST_DECIMAL_HAS_BRAINFLOAT16
constexpr decimal_fast64_t::operator std::bfloat16_t() const noexcept
{
    return static_cast<std::bfloat16_t>(to_float<decimal_fast64_t, float>(*this));
}
#endif

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal, std::enable_if_t<detail::is_decimal_floating_point_v<Decimal> && (detail::decimal_val_v<Decimal> > detail::decimal_val_v<decimal_fast64_t>), bool>>
constexpr decimal_fast64_t::operator Decimal() const noexcept
{
    return to_decimal<Decimal>(*this);
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal, std::enable_if_t<detail::is_decimal_floating_point_v<Decimal> && (detail::decimal_val_v<Decimal> <= detail::decimal_val_v<decimal_fast64_t>), bool>>
constexpr decimal_fast64_t::operator Decimal() const noexcept
{
    return to_decimal<Decimal>(*this);
}

constexpr auto operator+(const decimal_fast64_t lhs, const decimal_fast64_t rhs) noexcept -> decimal_fast64_t
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (not_finite(lhs) || not_finite(rhs))
    {
        if (isinf(lhs) && isinf(rhs) && signbit(lhs) != signbit(rhs))
        {
            return direct_init_d64(detail::d64_fast_qnan, 0, false);
        }

        return detail::check_non_finite(lhs, rhs);
    }
    #endif

    // Dominant-operand short-circuit: when exp_diff exceeds max_shift in
    // add_impl (= digits10(uint128) - precision - 1 = 21 for d64) the slow path
    // returns one operand unchanged anyway. Skip the whole add_impl call. Fast
    // types maintain canonical significands so the threshold has no
    // worst-case-expansion buffer (unlike the IEEE 36 threshold).
    {
        const auto lhs_sig {lhs.full_significand()};
        const auto rhs_sig {rhs.full_significand()};
        if (BOOST_DECIMAL_LIKELY(lhs_sig != 0U && rhs_sig != 0U))
        {
            const auto lhs_exp {lhs.biased_exponent()};
            const auto rhs_exp {rhs.biased_exponent()};
            const auto exp_diff {lhs_exp > rhs_exp ? lhs_exp - rhs_exp : rhs_exp - lhs_exp};
            if (exp_diff > 21)
            {
                auto round {_boost_decimal_global_rounding_mode};
                #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION
                if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
                {
                    round = _boost_decimal_global_runtime_rounding_mode;
                }
                #endif
                if (BOOST_DECIMAL_LIKELY(round == rounding_mode::fe_dec_to_nearest))
                {
                    return lhs_exp > rhs_exp ? lhs : rhs;
                }
            }
        }
    }

    return detail::add_impl<decimal_fast64_t>(lhs, rhs);
}

template <typename Integer>
constexpr auto operator+(const decimal_fast64_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t)
{
    using promoted_significand_type = detail::promote_significand_t<decimal_fast64_t, Integer>;
    using exp_type = decimal_fast64_t::biased_exponent_type;

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (not_finite(lhs))
    {
        return detail::check_non_finite(lhs);
    }
    #endif

    auto sig_rhs {static_cast<promoted_significand_type>(detail::make_positive_unsigned(rhs))};

    exp_type exp_rhs {0};
    detail::normalize<decimal64_t>(sig_rhs, exp_rhs);
    const auto final_sig_rhs {static_cast<decimal_fast64_t::significand_type>(sig_rhs)};

    return detail::add_impl<decimal_fast64_t>(
        detail::decimal_fast64_t_components{lhs.significand_, lhs.biased_exponent(), lhs.sign_},
        detail::decimal_fast64_t_components{final_sig_rhs, exp_rhs, (rhs < 0)}
    );
}

template <typename Integer>
constexpr auto operator+(const Integer lhs, const decimal_fast64_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t)
{
    return rhs + lhs;
}

constexpr auto operator-(const decimal_fast64_t lhs, decimal_fast64_t rhs) noexcept -> decimal_fast64_t
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (not_finite(lhs) || not_finite(rhs))
    {
        if (isinf(lhs) && isinf(rhs) && signbit(lhs) == signbit(rhs))
        {
            return direct_init_d64(detail::d64_fast_qnan, 0, false);
        }
        if (isinf(rhs) && !isnan(lhs))
        {
            return -rhs;
        }

        return detail::check_non_finite(lhs, rhs);
    }
    #endif

    // Dominant-operand short-circuit; see operator+ above.
    {
        const auto lhs_sig {lhs.full_significand()};
        const auto rhs_sig {rhs.full_significand()};
        if (BOOST_DECIMAL_LIKELY(lhs_sig != 0U && rhs_sig != 0U))
        {
            const auto lhs_exp {lhs.biased_exponent()};
            const auto rhs_exp {rhs.biased_exponent()};
            const auto exp_diff {lhs_exp > rhs_exp ? lhs_exp - rhs_exp : rhs_exp - lhs_exp};
            if (exp_diff > 21)
            {
                auto round {_boost_decimal_global_rounding_mode};
                #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION
                if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
                {
                    round = _boost_decimal_global_runtime_rounding_mode;
                }
                #endif
                if (BOOST_DECIMAL_LIKELY(round == rounding_mode::fe_dec_to_nearest))
                {
                    return lhs_exp > rhs_exp ? lhs : -rhs;
                }
            }
        }
    }

    rhs.sign_ = !rhs.sign_;

    return detail::add_impl<decimal_fast64_t>(lhs, rhs);
}

template <typename Integer>
constexpr auto operator-(const decimal_fast64_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t)
{
    using promoted_significand_type = detail::promote_significand_t<decimal_fast64_t, Integer>;
    using exp_type = decimal_fast64_t::biased_exponent_type;

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (not_finite(lhs))
    {
        return detail::check_non_finite(lhs);
    }
    #endif

    auto sig_rhs {static_cast<promoted_significand_type>(detail::make_positive_unsigned(rhs))};

    exp_type exp_rhs {0};
    detail::normalize<decimal64_t>(sig_rhs, exp_rhs);
    const auto final_sig_rhs {static_cast<decimal_fast64_t::significand_type>(sig_rhs)};

    return detail::add_impl<decimal_fast64_t>(
        detail::decimal_fast64_t_components{lhs.significand_, lhs.biased_exponent(), lhs.sign_},
        detail::decimal_fast64_t_components{final_sig_rhs, exp_rhs, !(rhs < 0)}
    );
}

template <typename Integer>
constexpr auto operator-(const Integer lhs, const decimal_fast64_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t)
{
    using promoted_significand_type = detail::promote_significand_t<decimal_fast64_t, Integer>;
    using exp_type = decimal_fast64_t::biased_exponent_type;

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (not_finite(rhs))
    {
        if (isinf(rhs))
        {
            return -rhs;
        }

        return detail::check_non_finite(rhs);
    }
    #endif

    auto sig_lhs {static_cast<promoted_significand_type>(detail::make_positive_unsigned(lhs))};

    exp_type exp_lhs {0};
    detail::normalize<decimal64_t>(sig_lhs, exp_lhs);
    const auto final_sig_lhs {static_cast<decimal_fast64_t::significand_type>(sig_lhs)};

    return detail::add_impl<decimal_fast64_t>(
        detail::decimal_fast64_t_components{final_sig_lhs, exp_lhs, (lhs < 0)},
        detail::decimal_fast64_t_components{rhs.significand_, rhs.biased_exponent(), !rhs.sign_}
    );
}

constexpr auto operator*(const decimal_fast64_t lhs, const decimal_fast64_t rhs) noexcept -> decimal_fast64_t
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (not_finite(lhs) || not_finite(rhs))
    {
        if ((isinf(lhs) && rhs == 0) || (isinf(rhs) && lhs == 0))
        {
            return direct_init_d64(detail::d64_fast_qnan, 0, false);
        }
        else if (isinf(lhs) && !isnan(rhs) && (signbit(lhs) != signbit(rhs)))
        {
            return signbit(lhs) ? lhs : -lhs;
        }
        else if (isinf(lhs) && !isnan(rhs) && (signbit(lhs) == signbit(rhs)))
        {
            return signbit(lhs) ? -lhs : lhs;
        }
        else if (isinf(rhs) && !isnan(lhs) && (signbit(rhs) != signbit(lhs)))
        {
            return signbit(rhs) ? rhs : -rhs;
        }
        else if (isinf(rhs) && !isnan(lhs) && (signbit(rhs) == signbit(lhs)))
        {
            return signbit(rhs) ? -rhs : rhs;
        }

        return detail::check_non_finite(lhs, rhs);
    }
    #endif

    return detail::d64_mul_impl<decimal_fast64_t>(lhs, rhs);
}

template <typename Integer>
constexpr auto operator*(const decimal_fast64_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t)
{
    using promoted_significand_type = detail::promote_significand_t<decimal64_t, Integer>;
    using exp_type = decimal_fast64_t::biased_exponent_type;

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (not_finite(lhs))
    {
        if (isinf(lhs) && (signbit(lhs) != (rhs < 0)))
        {
            return signbit(lhs) ? lhs : -lhs;
        }
        else if (isinf(lhs) && (signbit(lhs) == (rhs < 0)))
        {
            return signbit(lhs) ? -lhs : lhs;
        }

        return detail::check_non_finite(lhs);
    }
    #endif

    auto rhs_sig {static_cast<promoted_significand_type>(detail::make_positive_unsigned(rhs))};
    exp_type rhs_exp {0};
    detail::normalize<decimal64_t>(rhs_sig, rhs_exp);
    auto final_rhs_sig {static_cast<decimal_fast64_t::significand_type>(rhs_sig)};

    return detail::d64_mul_impl<decimal_fast64_t>(
            lhs.significand_, lhs.biased_exponent(), lhs.sign_,
            final_rhs_sig, rhs_exp, (rhs < 0)
            );
}

template <typename Integer>
constexpr auto operator*(const Integer lhs, const decimal_fast64_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t)
{
    return rhs * lhs;
}

constexpr auto d64_fast_div_impl(const decimal_fast64_t& lhs, const decimal_fast64_t& rhs, decimal_fast64_t& q, decimal_fast64_t& r) noexcept -> void
{
    const bool sign {lhs.isneg() != rhs.isneg()};

    #ifndef BOOST_DECIMAL_FAST_MATH
    // Check pre-conditions
    constexpr decimal_fast64_t zero {0, 0};
    constexpr decimal_fast64_t nan {direct_init_d64(detail::d64_fast_qnan, 0, false)};
    constexpr decimal_fast64_t inf {direct_init_d64(detail::d64_fast_inf, 0, false)};

    const auto lhs_fp {fpclassify(lhs)};
    const auto rhs_fp {fpclassify(rhs)};

    if (lhs_fp != FP_NORMAL || rhs_fp != FP_NORMAL)
    {
        if (lhs_fp == FP_NAN || rhs_fp == FP_NAN)
        {
            // Operations on an SNAN return a QNAN with the same payload
            decimal_fast64_t return_nan {};
            if (lhs_fp == rhs_fp)
            {
                // They are both NANs
                const bool lhs_signaling {issignaling(lhs)};
                const bool rhs_signaling {issignaling(rhs)};

                if (!lhs_signaling && rhs_signaling)
                {
                    return_nan = nan_conversion(rhs);
                }
                else
                {
                    return_nan = lhs_signaling ? nan_conversion(lhs) : lhs;
                }
            }
            else if (lhs_fp == FP_NAN)
            {
                return_nan = issignaling(lhs) ? nan_conversion(lhs) : lhs;
            }
            else
            {
                return_nan = issignaling(rhs) ? nan_conversion(rhs) : rhs;
            }

            q = return_nan;
            r = return_nan;

            return;
        }

        switch (lhs_fp)
        {
            case FP_INFINITE:
                if (rhs_fp == FP_INFINITE)
                {
                    q = nan;
                    r = nan;
                }
                else
                {
                    q = sign ? -inf : inf;
                    r = zero;
                }
                return;
            case FP_ZERO:
                if (rhs_fp == FP_ZERO)
                {
                    q = nan;
                    r = nan;
                }
                else
                {
                    q = sign ? -zero : zero;
                    r = sign ? -zero : zero;
                }
                return;
            default:
                static_cast<void>(lhs);
        }

        switch (rhs_fp)
        {
            case FP_ZERO:
                q = inf;
                r = zero;
                return;
            case FP_INFINITE:
                q = sign ? -zero : zero;
                r = lhs;
                return;
            default:
                static_cast<void>(rhs);
        }
    }
    #else
    static_cast<void>(r);
    #endif

    q = detail::d64_generic_div_impl<decimal_fast64_t>(lhs.to_components(), rhs.to_components(), sign);
}

constexpr auto operator/(const decimal_fast64_t& lhs, const decimal_fast64_t& rhs) noexcept -> decimal_fast64_t
{
    decimal_fast64_t q {};
    decimal_fast64_t r {};

    d64_fast_div_impl(lhs, rhs, q, r);

    return q;
}

template <typename Integer>
constexpr auto operator/(const decimal_fast64_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t)
{
    using promoted_significand_type = detail::promote_significand_t<decimal_fast64_t, Integer>;
    using exp_type = detail::decimal_fast64_t_components::biased_exponent_type;

    const bool sign {lhs.isneg() != (rhs < 0)};

    #ifndef BOOST_DECIMAL_FAST_MATH
    // Check pre-conditions
    constexpr decimal_fast64_t zero {0, 0};
    constexpr decimal_fast64_t inf {direct_init_d64(detail::d64_fast_inf, 0, false)};

    const auto lhs_fp {fpclassify(lhs)};

    switch (lhs_fp)
    {
        case FP_NAN:
            return issignaling(lhs) ? nan_conversion(lhs) : lhs;;
        case FP_INFINITE:
            return sign ? -lhs : lhs;
        case FP_ZERO:
            return sign ? -zero : zero;
        default:
            static_cast<void>(lhs);
    }

    if (rhs == 0)
    {
        return sign ? -inf : inf;
    }
    #endif

    const detail::decimal_fast64_t_components lhs_components {lhs.full_significand(), lhs.biased_exponent(), lhs.isneg()};

    auto rhs_sig {static_cast<promoted_significand_type>(detail::make_positive_unsigned(rhs))};
    exp_type rhs_exp {};
    detail::decimal_fast64_t_components rhs_components {detail::shrink_significand<decimal_fast64_t::significand_type>(rhs_sig, rhs_exp), rhs_exp, rhs < 0};

    return detail::d64_generic_div_impl<decimal_fast64_t>(lhs_components, rhs_components, sign);
}

template <typename Integer>
constexpr auto operator/(const Integer lhs, const decimal_fast64_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t)
{
    using promoted_significand_type = detail::promote_significand_t<decimal_fast64_t, Integer>;
    using exp_type = detail::decimal_fast64_t_components::biased_exponent_type;

    const bool sign {(lhs < 0) != rhs.isneg()};

    #ifndef BOOST_DECIMAL_FAST_MATH
    // Check pre-conditions
    constexpr decimal_fast64_t zero {0, 0};
    constexpr decimal_fast64_t inf {direct_init_d64(detail::d64_fast_inf, 0, false)};

    const auto rhs_fp {fpclassify(rhs)};

    switch (rhs_fp)
    {
        case FP_NAN:
            return issignaling(rhs) ? nan_conversion(rhs) : rhs;
        case FP_INFINITE:
            return sign ? -zero : zero;
        case FP_ZERO:
            return sign ? -inf : inf;
        default:
            static_cast<void>(lhs);
    }
    #endif

    const detail::decimal_fast64_t_components rhs_components {rhs.full_significand(), rhs.biased_exponent(), rhs.isneg()};

    auto lhs_sig {static_cast<promoted_significand_type>(detail::make_positive_unsigned(lhs))};
    exp_type lhs_exp {};
    const detail::decimal_fast64_t_components lhs_components {detail::shrink_significand<decimal_fast64_t::significand_type>(lhs_sig, lhs_exp), lhs_exp, lhs < 0};

    return detail::d64_generic_div_impl<decimal_fast64_t>(lhs_components, rhs_components, sign);
}

constexpr auto operator%(const decimal_fast64_t lhs, const decimal_fast64_t rhs) noexcept -> decimal_fast64_t
{
    decimal_fast64_t q {};
    decimal_fast64_t r {};
    d64_fast_div_impl(lhs, rhs, q, r);

    if (BOOST_DECIMAL_LIKELY(isfinite(lhs) && isfinite(rhs)))
    {
        if (rhs == 0 || isinf(q))
        {
            r = std::numeric_limits<decimal_fast64_t>::quiet_NaN();
        }
        else
        {
            detail::generic_mod_impl(lhs, lhs.to_components(), rhs, rhs.to_components(), q, r);
        }
    }
    else if (isinf(lhs) && !isnan(rhs))
    {
        // Modulo of inf is undefined
        r = std::numeric_limits<decimal_fast64_t>::quiet_NaN();
    }
    else if (issignaling(lhs))
    {
        r = nan_conversion(lhs);
    }
    else if (issignaling(rhs))
    {
        r = nan_conversion(rhs);
    }
    else if (isnan(lhs))
    {
        r = lhs;
    }
    else if (isnan(rhs))
    {
        r = rhs;
    }
    else if (isinf(rhs))
    {
        r = lhs;
    }

    return r;
}

constexpr auto decimal_fast64_t::operator+=(const decimal_fast64_t rhs) noexcept -> decimal_fast64_t &
{
    *this = *this + rhs;
    return *this;
}

constexpr auto decimal_fast64_t::operator-=(const decimal_fast64_t rhs) noexcept -> decimal_fast64_t &
{
    *this = *this - rhs;
    return *this;
}

constexpr auto decimal_fast64_t::operator*=(const decimal_fast64_t rhs) noexcept -> decimal_fast64_t &
{
    *this = *this * rhs;
    return *this;
}

constexpr auto decimal_fast64_t::operator/=(const decimal_fast64_t rhs) noexcept -> decimal_fast64_t &
{
    *this = *this / rhs;
    return *this;
}

template <typename Integer>
constexpr auto decimal_fast64_t::operator+=(const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t&)
{
    *this = *this + rhs;
    return *this;
}

template <typename Integer>
constexpr auto decimal_fast64_t::operator-=(const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t&)
{
    *this = *this - rhs;
    return *this;
}

template <typename Integer>
constexpr auto decimal_fast64_t::operator*=(const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t&)
{
    *this = *this * rhs;
    return *this;
}

template <typename Integer>
constexpr auto decimal_fast64_t::operator/=(const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast64_t&)
{
    *this = *this / rhs;
    return *this;
}

constexpr auto decimal_fast64_t::operator++() noexcept -> decimal_fast64_t&
{
    constexpr decimal_fast64_t one {1, 0};
    *this = *this + one;
    return *this;
}

constexpr auto decimal_fast64_t::operator++(int) noexcept -> decimal_fast64_t
{
    const auto temp {*this};
    ++(*this);
    return temp;
}

constexpr auto decimal_fast64_t::operator--() noexcept -> decimal_fast64_t&
{
    constexpr decimal_fast64_t one {1, 0};
    *this = *this - one;
    return *this;
}

constexpr auto decimal_fast64_t::operator--(int) noexcept -> decimal_fast64_t
{
    const auto temp {*this};
    --(*this);
    return temp;
}

// Effects: determines if the quantum exponents of x and y are the same.
// If both x and y are NaN, or infinity, they have the same quantum exponents;
// if exactly one operand is infinity or exactly one operand is NaN, they do not have the same quantum exponents.
// The samequantum functions raise no exception.
constexpr auto samequantumd64f(const decimal_fast64_t lhs, const decimal_fast64_t rhs) noexcept -> bool
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    const auto lhs_fp {fpclassify(lhs)};
    const auto rhs_fp {fpclassify(rhs)};

    if ((lhs_fp == FP_NAN && rhs_fp == FP_NAN) || (lhs_fp == FP_INFINITE && rhs_fp == FP_INFINITE))
    {
        return true;
    }
    if ((lhs_fp == FP_NAN || rhs_fp == FP_INFINITE) || (rhs_fp == FP_NAN || lhs_fp == FP_INFINITE))
    {
        return false;
    }
    #endif

    return lhs.unbiased_exponent() == rhs.unbiased_exponent();
}

// Effects: if x is finite, returns its quantum exponent.
// Otherwise, a domain error occurs and INT_MIN is returned.
constexpr auto quantexpd64f(const decimal_fast64_t x) noexcept -> int
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (!isfinite(x))
    {
        return INT_MIN;
    }
    #endif

    return static_cast<int>(x.unbiased_exponent());
}

constexpr auto scalblnd64f(decimal_fast64_t num, const long exp) noexcept -> decimal_fast64_t
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    constexpr decimal_fast64_t zero {0, 0};

    if (num == zero || exp == 0 || not_finite(num))
    {
        return num;
    }
    #endif

    num = decimal_fast64_t(num.significand_, num.biased_exponent() + exp, num.sign_);

    return num;
}

constexpr auto scalbnd64f(const decimal_fast64_t num, const int expval) noexcept -> decimal_fast64_t
{
    return scalblnd64f(num, static_cast<long>(expval));
}

constexpr auto copysignd64f(decimal_fast64_t mag, const decimal_fast64_t sgn) noexcept -> decimal_fast64_t
{
    mag.sign_ = sgn.sign_;
    return mag;
}

#if !defined(BOOST_DECIMAL_DISABLE_CLIB)

constexpr decimal_fast64_t::decimal_fast64_t(const char* str, const std::size_t len)
{
    if (str == nullptr || len == 0)
    {
        *this = direct_init_d64(detail::d64_fast_qnan, 0, false);
        BOOST_DECIMAL_THROW_EXCEPTION(std::runtime_error("Can not construct from invalid string"));
        return; // LCOV_EXCL_LINE
    }

    // Normally plus signs aren't allowed
    auto first {str};
    if (*first == '+')
    {
        ++first;
    }

    decimal_fast64_t v;
    const auto r {detail::from_chars_general_impl(first, str + len, v, chars_format::general)};
    if (r)
    {
        *this = v;
    }
    else
    {
        *this = direct_init_d64(detail::d64_fast_qnan, 0, false);
        BOOST_DECIMAL_THROW_EXCEPTION(std::runtime_error("Can not construct from invalid string"));
    }
}

constexpr decimal_fast64_t::decimal_fast64_t(const char* str) : decimal_fast64_t(str, detail::strlen(str)) {}

#ifndef BOOST_DECIMAL_HAS_STD_STRING_VIEW
inline decimal_fast64_t::decimal_fast64_t(const std::string& str) : decimal_fast64_t(str.c_str(), str.size()) {}
#else
constexpr decimal_fast64_t::decimal_fast64_t(std::string_view str) : decimal_fast64_t(str.data(), str.size()) {}
#endif

#endif // BOOST_DECIMAL_DISABLE_CLIB

} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_decimal_fast64_t_HPP
