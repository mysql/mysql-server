// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_decimal_fast32_t_HPP
#define BOOST_DECIMAL_decimal_fast32_t_HPP

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

BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE auto d32_fast_inf {UINT32_C(0b1) << 29U};
BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE auto d32_fast_qnan {UINT32_C(0b11) << 29U};
BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE auto d32_fast_snan {UINT32_C(0b111) << 29U};

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
constexpr auto to_chars_scientific_impl(char* first, char* last, const TargetDecimalType& value, chars_format fmt) noexcept -> to_chars_result;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
constexpr auto to_chars_fixed_impl(char* first, char* last, const TargetDecimalType& value, chars_format fmt) noexcept -> to_chars_result;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
constexpr auto to_chars_hex_impl(char* first, char* last, const TargetDecimalType& value) noexcept -> to_chars_result;

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
constexpr auto to_chars_cohort_preserving_scientific(char* first, char* last, const TargetDecimalType& value) noexcept -> to_chars_result;

template <bool checked, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
constexpr auto d32_fma_impl(T x, T y, T z) noexcept -> T;

template <typename TargetDecimalType, bool is_snan>
constexpr auto write_payload(typename TargetDecimalType::significand_type payload_value)
    BOOST_DECIMAL_REQUIRES(detail::is_fast_type_v, TargetDecimalType);

} // namespace detail

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4324) // Structure was padded due to alignment specifier
#endif

BOOST_DECIMAL_EXPORT class alignas(4) decimal_fast32_t final
{
public:
    using significand_type = std::uint32_t;
    using exponent_type = std::uint8_t;
    using biased_exponent_type = std::int32_t;

private:
    // In regular decimal32_t we have to decode the 24 bits of the significand and the 8 bits of the exp
    // Here we just use them directly at the cost of at least 2 extra bytes of internal state
    // since the fast integer types will be at least 32 and 8 bits respectively

    significand_type significand_ {};
    exponent_type exponent_ {};
    bool sign_ {};
    char pad_[2] {};

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
        return static_cast<biased_exponent_type>(exponent_) - detail::bias_v<decimal32_t>;
    }

    constexpr auto to_components() const noexcept -> detail::decimal_fast32_t_components
    {
        return {full_significand(), biased_exponent(), isneg()};
    }

    friend constexpr auto div_impl(decimal_fast32_t lhs, decimal_fast32_t rhs, decimal_fast32_t& q, decimal_fast32_t& r) noexcept -> void;

    // Attempts conversion to integral type:
    // If this is nan sets errno to EINVAL and returns 0
    // If this is not representable sets errno to ERANGE and returns 0
    template <typename Decimal, typename TargetType>
    friend constexpr auto to_integral(Decimal val) noexcept
        BOOST_DECIMAL_REQUIRES_TWO_RETURN(detail::is_decimal_floating_point_v, Decimal, detail::is_integral_v, TargetType, TargetType);

    template <typename Decimal, typename TargetType>
    friend BOOST_DECIMAL_CXX20_CONSTEXPR auto to_float(Decimal val) noexcept
        BOOST_DECIMAL_REQUIRES_TWO_RETURN(detail::is_decimal_floating_point_v, Decimal, detail::is_floating_point_v, TargetType, TargetType);

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetType, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal>
    friend constexpr auto to_decimal(Decimal val) noexcept -> TargetType;

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

    template <typename DecimalType>
    friend constexpr auto to_dpd_d32(DecimalType val) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, DecimalType, std::uint32_t);

    template <typename ReturnType, typename T>
    friend constexpr auto detail::add_impl(const T& lhs, const T& rhs) noexcept -> ReturnType;

    template <typename ReturnType, typename T>
    friend constexpr auto detail::mul_impl(const T& lhs, const T& rhs) noexcept -> ReturnType;

    template <BOOST_DECIMAL_FAST_DECIMAL_FLOATING_TYPE DecimalType>
    BOOST_DECIMAL_FORCE_INLINE friend constexpr auto fast_equality_impl(const DecimalType& lhs, const DecimalType& rhs) noexcept -> bool;

    template <BOOST_DECIMAL_FAST_DECIMAL_FLOATING_TYPE DecimalType>
    BOOST_DECIMAL_FORCE_INLINE friend constexpr auto fast_inequality_impl(const DecimalType& lhs, const DecimalType& rhs) noexcept -> bool;

    template <BOOST_DECIMAL_FAST_DECIMAL_FLOATING_TYPE DecimalType>
    BOOST_DECIMAL_FORCE_INLINE friend constexpr auto fast_less_impl(const DecimalType& lhs, const DecimalType& rhs) noexcept -> bool;

    friend constexpr auto not_finite(const decimal_fast32_t& val) noexcept -> bool;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE DecimalType>
    friend constexpr auto detail::nextafter_impl(DecimalType val, bool direction) noexcept -> DecimalType;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
    friend constexpr auto detail::to_chars_scientific_impl(char* first, char* last, const TargetDecimalType& value, const chars_format fmt) noexcept -> to_chars_result;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
    friend constexpr auto detail::to_chars_fixed_impl(char* first, char* last, const TargetDecimalType& value, const chars_format fmt) noexcept -> to_chars_result;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
    friend constexpr auto detail::to_chars_hex_impl(char* first, char* last, const TargetDecimalType& value) noexcept -> to_chars_result;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE TargetDecimalType>
    friend constexpr auto detail::to_chars_cohort_preserving_scientific(char* first, char* last, const TargetDecimalType& value) noexcept -> to_chars_result;

    template <typename DecimalType, typename T>
    friend constexpr auto detail::generic_div_impl(const T& lhs, const T& rhs) noexcept -> DecimalType;

    template <typename TargetDecimalType, bool is_snan>
    friend constexpr auto detail::write_payload(typename TargetDecimalType::significand_type payload_value)
        BOOST_DECIMAL_REQUIRES(detail::is_fast_type_v, TargetDecimalType);

    template <typename T>
    friend constexpr auto read_payload(T value) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_fast_type_v, T, typename T::significand_type);

    #if !defined(BOOST_DECIMAL_DISABLE_CLIB)
    constexpr decimal_fast32_t(const char* str, std::size_t len);
    #endif

    friend constexpr auto nan_conversion(const decimal_fast32_t value) noexcept -> decimal_fast32_t
    {
        constexpr auto convert_nan_mask {detail::d32_fast_qnan ^ detail::d32_fast_snan};

        decimal_fast32_t return_value {value};
        return_value.significand_ ^= convert_nan_mask;

        return return_value;
    }

    template <typename Decimal>
    friend constexpr Decimal detail::check_non_finite(Decimal lhs, Decimal rhs) noexcept;

    template <typename Decimal>
    friend constexpr Decimal detail::check_non_finite(Decimal x) noexcept;

public:
    constexpr decimal_fast32_t() noexcept = default;

    template <typename T1, typename T2, std::enable_if_t<detail::is_unsigned_v<T1> && detail::is_integral_v<T2>, bool> = true>
    constexpr decimal_fast32_t(T1 coeff, T2 exp, detail::construction_sign_wrapper resultant_sign = construction_sign::positive) noexcept;

    template <typename T1, typename T2, std::enable_if_t<!detail::is_unsigned_v<T1> && detail::is_integral_v<T2>, bool> = true>
    constexpr decimal_fast32_t(T1, T2, bool) noexcept { static_assert(detail::is_unsigned_v<T1>, "Construction from signed integer, exponent, and sign is ambiguous, so it is disallowed. You must use an Unsigned Integer for the coefficient to construct from {coefficient, exponent, sign}"); }

    template <typename T1, typename T2, std::enable_if_t<!detail::is_unsigned_v<T1> && detail::is_integral_v<T2>, bool> = true>
    constexpr decimal_fast32_t(T1 coeff, T2 exp) noexcept;

    explicit constexpr decimal_fast32_t(bool value) noexcept;

    template <typename Integer, std::enable_if_t<detail::is_integral_v<Integer>, bool> = true>
    #if !defined(BOOST_DECIMAL_ALLOW_IMPLICIT_CONVERSIONS) && !defined(BOOST_DECIMAL_ALLOW_IMPLICIT_INTEGER_CONVERSIONS)
    explicit
    #endif
    constexpr decimal_fast32_t(Integer coeff) noexcept;

    template <typename Float, std::enable_if_t<detail::is_floating_point_v<Float>, bool> = true>
    #if !defined(BOOST_DECIMAL_ALLOW_IMPLICIT_CONVERSIONS) && !defined(BOOST_DECIMAL_ALLOW_IMPLICIT_FLOAT_CONVERSIONS)
    explicit
    #endif
    BOOST_DECIMAL_CXX20_CONSTEXPR decimal_fast32_t(Float val) noexcept;

    #ifdef BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE
    explicit constexpr decimal_fast32_t(long double val) noexcept = delete;
    #endif

    #if !defined(BOOST_DECIMAL_DISABLE_CLIB)

    explicit constexpr decimal_fast32_t(const char* str);

    #ifndef BOOST_DECIMAL_HAS_STD_STRING_VIEW
    explicit inline decimal_fast32_t(const std::string& str);
    #else
    explicit constexpr decimal_fast32_t(std::string_view str);
    #endif

    #endif // BOOST_DECIMAL_DISABLE_CLIB

    constexpr decimal_fast32_t(const decimal_fast32_t& val) noexcept = default;
    constexpr decimal_fast32_t(decimal_fast32_t&& val) noexcept = default;
    constexpr auto operator=(const decimal_fast32_t& val) noexcept -> decimal_fast32_t& = default;
    constexpr auto operator=(decimal_fast32_t&& val) noexcept -> decimal_fast32_t& = default;

    // cmath functions that are easier as friends
    friend constexpr auto signbit(decimal_fast32_t val) noexcept -> bool;
    friend constexpr auto isinf(decimal_fast32_t val) noexcept -> bool;
    friend constexpr auto isnan(decimal_fast32_t val) noexcept -> bool;
    friend constexpr auto issignaling(decimal_fast32_t val) noexcept -> bool;
    friend constexpr auto isnormal(decimal_fast32_t val) noexcept -> bool;
    friend constexpr auto isfinite(decimal_fast32_t val) noexcept -> bool;

    // Comparison operators
    friend constexpr auto operator==(decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> bool;
    friend constexpr auto operator!=(decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> bool;
    friend constexpr auto operator<(decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> bool;
    friend constexpr auto operator<=(decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> bool;
    friend constexpr auto operator>(decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> bool;
    friend constexpr auto operator>=(decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> bool;

    // Mixed comparisons
    template <typename Integer>
    friend constexpr auto operator==(decimal_fast32_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator==(Integer lhs, decimal_fast32_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator!=(decimal_fast32_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator!=(Integer lhs, decimal_fast32_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator<(decimal_fast32_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator<(Integer lhs, decimal_fast32_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator<=(decimal_fast32_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator<=(Integer lhs, decimal_fast32_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator>(decimal_fast32_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator>(Integer lhs, decimal_fast32_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator>=(decimal_fast32_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    template <typename Integer>
    friend constexpr auto operator>=(Integer lhs, decimal_fast32_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool);

    #ifdef BOOST_DECIMAL_HAS_SPACESHIP_OPERATOR

    friend constexpr auto operator<=>(decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> std::partial_ordering;

    template <typename Integer>
    friend constexpr auto operator<=>(decimal_fast32_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, std::partial_ordering);

    template <typename Integer>
    friend constexpr auto operator<=>(Integer lhs, decimal_fast32_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, std::partial_ordering);

    #endif

    // Unary operators
    friend constexpr auto operator+(decimal_fast32_t rhs) noexcept -> decimal_fast32_t;
    friend constexpr auto operator-(decimal_fast32_t lhs) noexcept -> decimal_fast32_t;

    // Binary arithmetic
    friend constexpr auto operator+(decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> decimal_fast32_t;
    friend constexpr auto operator-(decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> decimal_fast32_t;
    friend constexpr auto operator*(decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> decimal_fast32_t;
    friend constexpr auto operator/(decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> decimal_fast32_t;
    friend constexpr auto operator%(decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> decimal_fast32_t;

    // Mixed type binary arithmetic
    template <typename Integer>
    friend constexpr auto operator+(decimal_fast32_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t);

    template <typename Integer>
    friend constexpr auto operator+(Integer lhs, decimal_fast32_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t);

    template <typename Integer>
    friend constexpr auto operator-(decimal_fast32_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t);

    template <typename Integer>
    friend constexpr auto operator-(Integer lhs, decimal_fast32_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t);

    template <typename Integer>
    friend constexpr auto operator*(decimal_fast32_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t);

    template <typename Integer>
    friend constexpr auto operator*(Integer lhs, decimal_fast32_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t);

    template <typename Integer>
    friend constexpr auto operator/(decimal_fast32_t lhs, Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t);

    template <typename Integer>
    friend constexpr auto operator/(Integer lhs, decimal_fast32_t rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t);

    // Compound operators
    constexpr auto operator+=(decimal_fast32_t rhs) noexcept -> decimal_fast32_t&;
    constexpr auto operator-=(decimal_fast32_t rhs) noexcept -> decimal_fast32_t&;
    constexpr auto operator*=(decimal_fast32_t rhs) noexcept -> decimal_fast32_t&;
    constexpr auto operator/=(decimal_fast32_t rhs) noexcept -> decimal_fast32_t&;
    constexpr auto operator%=(decimal_fast32_t rhs) noexcept -> decimal_fast32_t&;

    // Mixed type compound operators
    template <typename Integer>
    constexpr auto operator+=(Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t&);

    template <typename Integer>
    constexpr auto operator-=(Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t&);

    template <typename Integer>
    constexpr auto operator*=(Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t&);

    template <typename Integer>
    constexpr auto operator/=(Integer rhs) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t&);

    // Increment and decrement
    constexpr auto operator++() noexcept -> decimal_fast32_t&;
    constexpr auto operator++(int) noexcept -> decimal_fast32_t;
    constexpr auto operator--() noexcept -> decimal_fast32_t&;
    constexpr auto operator--(int) noexcept -> decimal_fast32_t;

    // 3.2.2.4 Conversion to integral type
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

    // 3.2.6 Conversion to a floating-point type
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


    // Conversion to another decimal type
    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal, std::enable_if_t<detail::is_decimal_floating_point_v<Decimal> && (detail::decimal_val_v<Decimal> > detail::decimal_val_v<decimal_fast32_t>), bool> = true>
    constexpr operator Decimal() const noexcept;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal, std::enable_if_t<detail::is_decimal_floating_point_v<Decimal> && (detail::decimal_val_v<Decimal> <= detail::decimal_val_v<decimal_fast32_t>), bool> = true>
    explicit constexpr operator Decimal() const noexcept;

    friend constexpr auto direct_init(significand_type significand, exponent_type exponent, bool sign) noexcept -> decimal_fast32_t;
    friend constexpr auto direct_init(const detail::decimal_fast32_t_components& x) noexcept -> decimal_fast32_t;

    // <cmath> or extensions that need to be friends
    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
    friend constexpr auto frexp10(T num, int* expptr) noexcept -> typename T::significand_type;

    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
    friend BOOST_DECIMAL_CUDA_CONSTEXPR auto decompose(const T x) noexcept;

    friend constexpr auto copysignd32f(decimal_fast32_t mag, decimal_fast32_t sgn) noexcept -> decimal_fast32_t;
    friend constexpr auto scalbnd32f(decimal_fast32_t num, int exp) noexcept -> decimal_fast32_t;
    friend constexpr auto scalblnd32f(decimal_fast32_t num, long exp) noexcept -> decimal_fast32_t;

    template <bool checked, BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
    friend constexpr auto detail::d32_fma_impl(T x, T y, T z) noexcept -> T;

    template <typename T>
    friend constexpr auto ilogb(T d) noexcept
        BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, T, int);

    template <typename T>
    friend constexpr auto logb(T num) noexcept
        BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T);

    // Specific decimal functionality
    friend constexpr auto samequantumd32f(decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> bool;
    friend constexpr auto quantexpd32f(decimal_fast32_t x) noexcept -> int;
    template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
    friend BOOST_DECIMAL_CUDA_CONSTEXPR auto quantize(T lhs, T rhs) noexcept -> T;
};

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

template <typename T1, typename T2, std::enable_if_t<detail::is_unsigned_v<T1> && detail::is_integral_v<T2>, bool>>
constexpr decimal_fast32_t::decimal_fast32_t(T1 coeff, T2 exp, const detail::construction_sign_wrapper resultant_sign) noexcept
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
        auto biased_exp {static_cast<int>(exp) + detail::bias};
        if (biased_exp < 0)
        {
            biased_exp = 0;
        }
        else if (biased_exp > detail::max_biased_exp_v<decimal_fast32_t>)
        {
            biased_exp = detail::max_biased_exp_v<decimal_fast32_t>;
        }
        exponent_ = static_cast<exponent_type>(biased_exp);
        return;
    }

    // Fast path: if the coefficient already has exactly precision-many digits,
    // normalize is a no-op apart from a num_digits call. Skip the call entirely.
    constexpr minimum_coefficient_size min_normal_significand {
        detail::pow10(static_cast<minimum_coefficient_size>(detail::precision_v<decimal_fast32_t> - 1))};
    constexpr minimum_coefficient_size max_normal_significand {
        static_cast<minimum_coefficient_size>(detail::max_significand_v<decimal_fast32_t>)};

    if (min_coeff < min_normal_significand || min_coeff > max_normal_significand)
    {
        // Normalize in the constructor, so we never have to worry about it again
        detail::normalize<decimal_fast32_t>(min_coeff, exp, is_negative);
    }

    significand_ = static_cast<significand_type>(min_coeff);

    const auto biased_exp {static_cast<int>(exp) + detail::bias};

    // decimal32_t exponent holds 8 bits
    if (biased_exp > detail::max_biased_exp_v<decimal_fast32_t>)
    {
        significand_ = detail::d32_fast_inf;
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

template <typename T1, typename T2, std::enable_if_t<!detail::is_unsigned_v<T1> && detail::is_integral_v<T2>, bool>>
constexpr decimal_fast32_t::decimal_fast32_t(const T1 coeff, const T2 exp) noexcept : decimal_fast32_t(detail::make_positive_unsigned(coeff), exp, coeff < 0) {}

constexpr decimal_fast32_t::decimal_fast32_t(const bool value) noexcept : decimal_fast32_t(static_cast<significand_type>(value), 0, false) {}

template <typename Integer, std::enable_if_t<detail::is_integral_v<Integer>, bool>>
constexpr decimal_fast32_t::decimal_fast32_t(const Integer val) noexcept : decimal_fast32_t{val, 0} {}

constexpr auto direct_init(const decimal_fast32_t::significand_type significand, const decimal_fast32_t::exponent_type exponent, const bool sign = false) noexcept -> decimal_fast32_t
{
    decimal_fast32_t val;
    val.significand_ = significand;
    val.exponent_ = exponent;
    val.sign_ = sign;

    return val;
}

constexpr auto direct_init(const detail::decimal_fast32_t_components& x) noexcept -> decimal_fast32_t
{
    decimal_fast32_t val;
    val.significand_ = x.sig;
    val.exponent_ = static_cast<typename decimal_fast32_t::exponent_type>(static_cast<int>(x.exp) + detail::bias_v<decimal_fast32_t>);
    val.sign_ = x.sign;

    return val;
}

namespace detail {

template <bool>
class numeric_limits_impl32f
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
    static constexpr std::float_denorm_style has_denorm = std::denorm_absent;
    static constexpr bool has_denorm_loss = false;
    #endif

    static constexpr std::float_round_style round_style = std::round_indeterminate;
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = false;
    static constexpr int digits = 7;
    static constexpr int digits10 = digits;
    static constexpr int max_digits10 = digits;
    static constexpr int radix = 10;
    static constexpr int min_exponent = -95;
    static constexpr int min_exponent10 = min_exponent;
    static constexpr int max_exponent = 96;
    static constexpr int max_exponent10 = max_exponent;
    static constexpr bool traps = std::numeric_limits<std::uint32_t>::traps;
    static constexpr bool tinyness_before = true;

    // Member functions
    static constexpr auto (min)        () -> boost::decimal::decimal_fast32_t { return {UINT32_C(1), min_exponent}; }
    static constexpr auto (max)        () -> boost::decimal::decimal_fast32_t { return {UINT32_C(9'999'999), max_exponent - digits + 1}; }
    static constexpr auto lowest       () -> boost::decimal::decimal_fast32_t { return {UINT32_C(9'999'999), max_exponent - digits + 1, construction_sign::negative}; }
    static constexpr auto epsilon      () -> boost::decimal::decimal_fast32_t { return {UINT32_C(1), -digits + 1}; }
    static constexpr auto round_error  () -> boost::decimal::decimal_fast32_t { return epsilon(); }
    static constexpr auto infinity     () -> boost::decimal::decimal_fast32_t { return boost::decimal::direct_init(boost::decimal::detail::d32_fast_inf, UINT8_C((0))); }
    static constexpr auto quiet_NaN    () -> boost::decimal::decimal_fast32_t { return boost::decimal::direct_init(boost::decimal::detail::d32_fast_qnan, UINT8_C((0))); }
    static constexpr auto signaling_NaN() -> boost::decimal::decimal_fast32_t { return boost::decimal::direct_init(boost::decimal::detail::d32_fast_snan, UINT8_C((0))); }

    // With denorm absent returns the same value as min
    static constexpr auto denorm_min   () -> boost::decimal::decimal_fast32_t { return min(); }
};

#if !defined(__cpp_inline_variables) || __cpp_inline_variables < 201606L

template <bool b> constexpr bool numeric_limits_impl32f<b>::is_specialized;
template <bool b> constexpr bool numeric_limits_impl32f<b>::is_signed;
template <bool b> constexpr bool numeric_limits_impl32f<b>::is_integer;
template <bool b> constexpr bool numeric_limits_impl32f<b>::is_exact;
template <bool b> constexpr bool numeric_limits_impl32f<b>::has_infinity;
template <bool b> constexpr bool numeric_limits_impl32f<b>::has_quiet_NaN;
template <bool b> constexpr bool numeric_limits_impl32f<b>::has_signaling_NaN;

// These members were deprecated in C++23
#if ((!defined(_MSC_VER) && (__cplusplus <= 202002L)) || (defined(_MSC_VER) && (_MSVC_LANG <= 202002L)))
template <bool b> constexpr std::float_denorm_style numeric_limits_impl32f<b>::has_denorm;
template <bool b> constexpr bool numeric_limits_impl32f<b>::has_denorm_loss;
#endif

template <bool b> constexpr std::float_round_style numeric_limits_impl32f<b>::round_style;
template <bool b> constexpr bool numeric_limits_impl32f<b>::is_iec559;
template <bool b> constexpr bool numeric_limits_impl32f<b>::is_bounded;
template <bool b> constexpr bool numeric_limits_impl32f<b>::is_modulo;
template <bool b> constexpr int numeric_limits_impl32f<b>::digits;
template <bool b> constexpr int numeric_limits_impl32f<b>::digits10;
template <bool b> constexpr int numeric_limits_impl32f<b>::max_digits10;
template <bool b> constexpr int numeric_limits_impl32f<b>::radix;
template <bool b> constexpr int numeric_limits_impl32f<b>::min_exponent;
template <bool b> constexpr int numeric_limits_impl32f<b>::min_exponent10;
template <bool b> constexpr int numeric_limits_impl32f<b>::max_exponent;
template <bool b> constexpr int numeric_limits_impl32f<b>::max_exponent10;
template <bool b> constexpr bool numeric_limits_impl32f<b>::traps;
template <bool b> constexpr bool numeric_limits_impl32f<b>::tinyness_before;

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
class numeric_limits<boost::decimal::decimal_fast32_t> :
    public boost::decimal::detail::numeric_limits_impl32f<true> {};

#ifdef __clang__
#  pragma clang diagnostic pop
#endif

} // Namespace std

namespace boost {
namespace decimal {

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

template <typename Float, std::enable_if_t<detail::is_floating_point_v<Float>, bool>>
BOOST_DECIMAL_CXX20_CONSTEXPR decimal_fast32_t::decimal_fast32_t(const Float val) noexcept
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (val != val)
    {
        significand_ = detail::d32_fast_qnan;
    }
    else if (val == std::numeric_limits<Float>::infinity())
    {
        significand_ = detail::d32_fast_inf;
    }
    else if (val == -std::numeric_limits<Float>::infinity())
    {
        significand_ = detail::d32_fast_inf;
        sign_ = true;
    }
    else
    #endif
    {
        const auto components {detail::ryu::floating_point_to_fd128(val)};
        *this = decimal_fast32_t {components.mantissa, components.exponent, components.sign};
    }
}

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

constexpr auto signbit(const decimal_fast32_t val) noexcept -> bool
{
    return val.sign_;
}

constexpr auto isinf(const decimal_fast32_t val) noexcept -> bool
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    return val.significand_ == detail::d32_fast_inf;
    #else
    static_cast<void>(val);
    return false;
    #endif
}

constexpr auto isnan(const decimal_fast32_t val) noexcept -> bool
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    return val.significand_ >= detail::d32_fast_qnan;
    #else
    static_cast<void>(val);
    return false;
    #endif
}

constexpr auto issignaling(const decimal_fast32_t val) noexcept -> bool
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    return val.significand_ >= detail::d32_fast_snan;
    #else
    static_cast<void>(val);
    return false;
    #endif
}

constexpr auto isnormal(const decimal_fast32_t val) noexcept -> bool
{
    return (val.significand_ != 0)
    #ifndef BOOST_DECIMAL_FAST_MATH
    && isfinite(val) && (val.exponent_ > static_cast<std::uint8_t>(detail::precision_v<decimal32_t> - 1))
    #endif
    ;
}

constexpr auto isfinite(const decimal_fast32_t val) noexcept -> bool
{
    return val.significand_ < detail::d32_fast_inf;
}

BOOST_DECIMAL_FORCE_INLINE constexpr auto not_finite(const decimal_fast32_t& val) noexcept -> bool
{
    return val.significand_ >= detail::d32_fast_inf;
}

constexpr auto operator==(const decimal_fast32_t lhs, const decimal_fast32_t rhs) noexcept -> bool
{
    return fast_equality_impl(lhs, rhs);
}

constexpr auto operator!=(const decimal_fast32_t lhs, const decimal_fast32_t rhs) noexcept -> bool
{
    return fast_inequality_impl(lhs, rhs);
}

constexpr auto operator<(const decimal_fast32_t lhs, const decimal_fast32_t rhs) noexcept -> bool
{
    return fast_less_impl(lhs, rhs);
}

constexpr auto operator<=(const decimal_fast32_t lhs, const decimal_fast32_t rhs) noexcept -> bool
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (isnan(lhs) || isnan(rhs))
    {
        return false;
    }
    #endif

    return !(rhs < lhs);
}

constexpr auto operator>(const decimal_fast32_t lhs, const decimal_fast32_t rhs) noexcept -> bool
{
    return rhs < lhs;
}

constexpr auto operator>=(const decimal_fast32_t lhs, const decimal_fast32_t rhs) noexcept -> bool
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
constexpr auto operator==(const decimal_fast32_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    return mixed_equality_impl(lhs, rhs);
}

template <typename Integer>
constexpr auto operator==(const Integer lhs, const decimal_fast32_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    return mixed_equality_impl(rhs, lhs);
}

template <typename Integer>
constexpr auto operator!=(const decimal_fast32_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    return !(lhs == rhs);
}

template <typename Integer>
constexpr auto operator!=(const Integer lhs, const decimal_fast32_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    return !(lhs == rhs);
}

template <typename Integer>
constexpr auto operator<(const decimal_fast32_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    return less_impl(lhs, rhs);
}

template <typename Integer>
constexpr auto operator<(const Integer lhs, const decimal_fast32_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    return isnan(rhs) ? false : !less_impl(rhs, lhs) && lhs != rhs;
    #else
    return !less_impl(rhs, lhs) && lhs != rhs;
    #endif
}

template <typename Integer>
constexpr auto operator<=(const decimal_fast32_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    return isnan(lhs) ? false : !(rhs < lhs);
    #else
    return !(rhs < lhs);
    #endif
}

template <typename Integer>
constexpr auto operator<=(const Integer lhs, const decimal_fast32_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    return isnan(rhs) ? false : !(rhs < lhs);
    #else
    return !(rhs < lhs);
    #endif
}

template <typename Integer>
constexpr auto operator>(const decimal_fast32_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    return isnan(lhs) ? false : rhs < lhs;
    #else
    return rhs < lhs;
    #endif
}

template <typename Integer>
constexpr auto operator>(const Integer lhs, const decimal_fast32_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    return isnan(rhs) ? false : rhs < lhs;
    #else
    return rhs < lhs;
    #endif
}

template <typename Integer>
constexpr auto operator>=(const decimal_fast32_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    return isnan(lhs) ? false : !(lhs < rhs);
    #else
    return !(lhs < rhs); // LCOV_EXCL_LINE
    #endif
}

template <typename Integer>
constexpr auto operator>=(const Integer lhs, const decimal_fast32_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, bool)
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    return isnan(rhs) ? false : !(lhs < rhs);
    #else
    return !(lhs < rhs);
    #endif
}

#ifdef BOOST_DECIMAL_HAS_SPACESHIP_OPERATOR

constexpr auto operator<=>(const decimal_fast32_t lhs, const decimal_fast32_t rhs) noexcept -> std::partial_ordering
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
constexpr auto operator<=>(const decimal_fast32_t lhs, const Integer rhs) noexcept
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
constexpr auto operator<=>(const Integer lhs, const decimal_fast32_t rhs) noexcept
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

#endif

constexpr auto operator+(const decimal_fast32_t rhs) noexcept -> decimal_fast32_t
{
    return rhs;
}

constexpr auto operator-(decimal_fast32_t lhs) noexcept -> decimal_fast32_t
{
    lhs.sign_ = !lhs.sign_;
    return lhs;
}

constexpr auto operator+(const decimal_fast32_t lhs, const decimal_fast32_t rhs) noexcept -> decimal_fast32_t
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (!isfinite(lhs) || !isfinite(rhs))
    {
        if (isinf(lhs) && isinf(rhs) && signbit(lhs) != signbit(rhs))
        {
            return direct_init(detail::d32_fast_qnan, UINT8_C((0)));
        }

        return detail::check_non_finite(lhs, rhs);
    }
    #endif

    // Dominant-operand short-circuit: for d32 add_impl uses uint64 promoted
    // type with max_shift = digits10(uint64) - precision - 1 = 19 - 7 - 1 = 11.
    // Above that the slow path returns one operand unchanged anyway. Fast
    // types maintain canonical significands so no expansion buffer is needed.
    {
        const auto lhs_sig {lhs.full_significand()};
        const auto rhs_sig {rhs.full_significand()};
        if (BOOST_DECIMAL_LIKELY(lhs_sig != 0U && rhs_sig != 0U))
        {
            const auto lhs_exp {lhs.biased_exponent()};
            const auto rhs_exp {rhs.biased_exponent()};
            const auto exp_diff {lhs_exp > rhs_exp ? lhs_exp - rhs_exp : rhs_exp - lhs_exp};
            if (exp_diff > 11)
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

    return detail::add_impl<decimal_fast32_t>(lhs, rhs);
}

template <typename Integer>
constexpr auto operator+(const decimal_fast32_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t)
{
    using promoted_significand_type = detail::promote_significand_t<decimal_fast32_t, Integer>;
    using exp_type = decimal_fast32_t::biased_exponent_type;

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (!isfinite(lhs))
    {
        return detail::check_non_finite(lhs);
    }
    #endif

    auto sig_rhs {static_cast<promoted_significand_type>(detail::make_positive_unsigned(rhs))};

    exp_type exp_rhs {0};
    detail::normalize(sig_rhs, exp_rhs);
    const auto final_sig_rhs {static_cast<detail::decimal_fast32_t_components::significand_type>(detail::make_positive_unsigned(sig_rhs))};

    return detail::add_impl<decimal_fast32_t>(
        detail::decimal_fast32_t_components{lhs.significand_, lhs.biased_exponent(), lhs.sign_},
        detail::decimal_fast32_t_components{final_sig_rhs, exp_rhs, (rhs < 0)}
    );
}

template <typename Integer>
constexpr auto operator+(const Integer lhs, const decimal_fast32_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t)
{
    return rhs + lhs;
}

constexpr auto operator-(const decimal_fast32_t lhs, decimal_fast32_t rhs) noexcept -> decimal_fast32_t
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (!isfinite(lhs) || !isfinite(rhs))
    {
        if (isinf(lhs) && isinf(rhs) && signbit(lhs) == signbit(rhs))
        {
            return direct_init(detail::d32_fast_qnan, UINT8_C((0)));
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
            if (exp_diff > 11)
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

    return detail::add_impl<decimal_fast32_t>(lhs, rhs);
}

template <typename Integer>
constexpr auto operator-(const decimal_fast32_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t)
{
    using promoted_significand_type = detail::promote_significand_t<decimal_fast32_t, Integer>;
    using exp_type = decimal_fast32_t::biased_exponent_type;

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (!isfinite(lhs))
    {
        return detail::check_non_finite(lhs);
    }
    #endif

    auto sig_rhs {static_cast<promoted_significand_type>(detail::make_positive_unsigned(rhs))};

    exp_type exp_rhs {0};
    detail::normalize(sig_rhs, exp_rhs);
    auto final_sig_rhs {static_cast<decimal_fast32_t::significand_type>(detail::make_positive_unsigned(sig_rhs))};

    return detail::add_impl<decimal_fast32_t>(
        detail::decimal_fast32_t_components{lhs.significand_, lhs.biased_exponent(), lhs.sign_},
        detail::decimal_fast32_t_components{final_sig_rhs, exp_rhs, !(rhs < 0)}
    );
}

template <typename Integer>
constexpr auto operator-(const Integer lhs, const decimal_fast32_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t)
{
    using promoted_significand_type = detail::promote_significand_t<decimal_fast32_t, Integer>;
    using exp_type = decimal_fast32_t::biased_exponent_type;

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (!isfinite(rhs))
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
    detail::normalize(sig_lhs, exp_lhs);
    auto final_sig_lhs {static_cast<decimal_fast32_t::significand_type>(detail::make_positive_unsigned(sig_lhs))};

    return detail::add_impl<decimal_fast32_t>(
        detail::decimal_fast32_t_components{final_sig_lhs, exp_lhs, (lhs < 0)},
        detail::decimal_fast32_t_components{rhs.significand_, rhs.biased_exponent(), !rhs.sign_}
    );
}

constexpr auto operator*(const decimal_fast32_t lhs, const decimal_fast32_t rhs) noexcept -> decimal_fast32_t
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (!isfinite(lhs) || !isfinite(rhs))
    {
        if ((isinf(lhs) && rhs == 0) || (isinf(rhs) && lhs == 0))
        {
            return direct_init(detail::d32_fast_qnan, UINT8_C(0));
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

    return detail::mul_impl<decimal_fast32_t>(lhs, rhs);
}

template <typename Integer>
constexpr auto operator*(const decimal_fast32_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t)
{
    using promoted_significand_type = detail::promote_significand_t<decimal_fast32_t, Integer>;
    using exp_type = decimal_fast32_t::biased_exponent_type;

    #ifndef BOOST_DECIMAL_FAST_MATH
    if (!isfinite(lhs))
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

    auto sig_rhs {static_cast<promoted_significand_type>(detail::make_positive_unsigned(rhs))};
    exp_type exp_rhs {0};
    detail::normalize(sig_rhs, exp_rhs);

    // We don't know if the original value of rhs fits into the decimal_fast32_t significand type
    // but once it's normalized it's guaranteed to fit
    const auto final_sig_rhs {static_cast<decimal_fast32_t::significand_type>(sig_rhs)};

    return detail::mul_impl<decimal_fast32_t>(lhs.significand_, lhs.biased_exponent(), lhs.sign_,
                                             final_sig_rhs, exp_rhs, (rhs < 0));
}

template <typename Integer>
constexpr auto operator*(const Integer lhs, const decimal_fast32_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t)
{
    return rhs * lhs;
}

constexpr auto div_impl(const decimal_fast32_t lhs, const decimal_fast32_t rhs, decimal_fast32_t& q, decimal_fast32_t& r) noexcept -> void
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    constexpr decimal_fast32_t zero {0, 0};

    const bool sign {lhs.isneg() != rhs.isneg()};
    constexpr decimal_fast32_t nan {direct_init(detail::d32_fast_qnan, UINT8_C(0), false)};
    constexpr decimal_fast32_t inf {direct_init(detail::d32_fast_inf, UINT8_C(0), false)};

    const auto lhs_fp {fpclassify(lhs)};
    const auto rhs_fp {fpclassify(rhs)};

    if (lhs_fp != FP_NORMAL || rhs_fp != FP_NORMAL)
    {
        if (lhs_fp == FP_NAN || rhs_fp == FP_NAN)
        {
            // Operations on an SNAN return a QNAN with the same payload
            decimal_fast32_t return_nan {};
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

    q = detail::generic_div_impl<decimal_fast32_t>(lhs, rhs);
}

constexpr auto operator/(const decimal_fast32_t lhs, const decimal_fast32_t rhs) noexcept -> decimal_fast32_t
{
    decimal_fast32_t q {};
    decimal_fast32_t r {};
    div_impl(lhs, rhs, q, r);

    return q;
}

template <typename Integer>
constexpr auto operator/(const decimal_fast32_t lhs, const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t)
{
    using exp_type = decimal_fast32_t::biased_exponent_type;

    #ifndef BOOST_DECIMAL_FAST_MATH
    // Check pre-conditions
    constexpr decimal_fast32_t zero {0, 0};
    constexpr decimal_fast32_t inf {direct_init(detail::d32_fast_inf, UINT8_C(0), false)};

    const bool sign {lhs.isneg() != (rhs < 0)};

    const auto lhs_fp {fpclassify(lhs)};

    switch (lhs_fp)
    {
        case FP_NAN:
            return issignaling(lhs) ? nan_conversion(lhs) : lhs;
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

    const detail::decimal_fast32_t_components lhs_components {lhs.significand_, lhs.biased_exponent(), lhs.sign_};
    exp_type exp_rhs {};
    const detail::decimal_fast32_t_components rhs_components {detail::shrink_significand<decimal_fast32_t::significand_type>(detail::make_positive_unsigned(rhs), exp_rhs), exp_rhs, rhs < 0};

    return detail::generic_div_impl<decimal_fast32_t>(lhs_components, rhs_components);
}

template <typename Integer>
constexpr auto operator/(const Integer lhs, const decimal_fast32_t rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t)
{
    using exp_type = decimal_fast32_t::biased_exponent_type;

    #ifndef BOOST_DECIMAL_FAST_MATH
    // Check pre-conditions
    constexpr decimal_fast32_t zero {0, 0};
    constexpr decimal_fast32_t inf {direct_init(detail::d32_fast_inf, UINT8_C(0), false)};

    const bool sign {(lhs < 0) != rhs.isneg()};

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

    exp_type lhs_exp {};
    const auto lhs_sig {detail::make_positive_unsigned(detail::shrink_significand<decimal_fast32_t::significand_type>(lhs, lhs_exp))};
    const detail::decimal_fast32_t_components lhs_components {lhs_sig, lhs_exp, lhs < 0};
    const detail::decimal_fast32_t_components rhs_components {rhs.significand_, rhs.biased_exponent(), rhs.isneg()};

    return detail::generic_div_impl<decimal_fast32_t>(lhs_components, rhs_components);
}

constexpr auto operator%(const decimal_fast32_t lhs, const decimal_fast32_t rhs) noexcept -> decimal_fast32_t
{
    decimal_fast32_t q {};
    decimal_fast32_t r {};
    div_impl(lhs, rhs, q, r);

    if (BOOST_DECIMAL_LIKELY(isfinite(lhs) && isfinite(rhs)))
    {
        if (rhs == 0 || isinf(q))
        {
            r = std::numeric_limits<decimal_fast32_t>::quiet_NaN();
        }
        else
        {
            detail::generic_mod_impl(lhs, lhs.to_components(), rhs, rhs.to_components(), q, r);
        }
    }
    else if (isinf(lhs) && !isnan(rhs))
    {
        // Modulo of inf is undefined
        r = std::numeric_limits<decimal_fast32_t>::quiet_NaN();
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

constexpr auto decimal_fast32_t::operator%=(const decimal_fast32_t rhs) noexcept -> decimal_fast32_t&
{
    *this = *this % rhs;
    return *this;
}

constexpr auto decimal_fast32_t::operator+=(const decimal_fast32_t rhs) noexcept -> decimal_fast32_t&
{
    *this = *this + rhs;
    return *this;
}

constexpr auto decimal_fast32_t::operator-=(const decimal_fast32_t rhs) noexcept -> decimal_fast32_t&
{
    *this = *this - rhs;
    return *this;
}

constexpr auto decimal_fast32_t::operator*=(const decimal_fast32_t rhs) noexcept -> decimal_fast32_t&
{
    *this = *this * rhs;
    return *this;
}

constexpr auto decimal_fast32_t::operator/=(const decimal_fast32_t rhs) noexcept -> decimal_fast32_t&
{
    *this = *this / rhs;
    return *this;
}

template <typename Integer>
constexpr auto decimal_fast32_t::operator+=(const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t&)
{
    *this = *this + rhs;
    return *this;
}

template <typename Integer>
constexpr auto decimal_fast32_t::operator-=(const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t&)
{
    *this = *this - rhs;
    return *this;
}

template <typename Integer>
constexpr auto decimal_fast32_t::operator*=(const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t&)
{
    *this = *this * rhs; // LCOV_EXCL_LINE : False negative
    return *this;
}

template <typename Integer>
constexpr auto decimal_fast32_t::operator/=(const Integer rhs) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_integral_v, Integer, decimal_fast32_t&)
{
    *this = *this / rhs;
    return *this;
}

constexpr auto decimal_fast32_t::operator++() noexcept -> decimal_fast32_t&
{
    constexpr decimal_fast32_t one(1, 0);
    *this = *this + one;
    return *this;
}

constexpr auto decimal_fast32_t::operator++(int) noexcept -> decimal_fast32_t
{
    const auto temp {*this};
    ++(*this);
    return temp;
}

constexpr auto decimal_fast32_t::operator--() noexcept -> decimal_fast32_t&
{
    constexpr decimal_fast32_t one(1, 0);
    *this = *this - one;
    return *this;
}

constexpr auto decimal_fast32_t::operator--(int) noexcept -> decimal_fast32_t
{
    const auto temp {*this};
    --(*this);
    return temp;
}

constexpr decimal_fast32_t::operator bool() const noexcept
{
    constexpr decimal_fast32_t zero {0, 0};
    return *this != zero;
}

constexpr decimal_fast32_t::operator int() const noexcept
{
    return to_integral<decimal_fast32_t, int>(*this);
}

constexpr decimal_fast32_t::operator unsigned() const noexcept
{
    return to_integral<decimal_fast32_t, unsigned>(*this);
}

constexpr decimal_fast32_t::operator long() const noexcept
{
    return to_integral<decimal_fast32_t, long>(*this);
}

constexpr decimal_fast32_t::operator unsigned long() const noexcept
{
    return to_integral<decimal_fast32_t, unsigned long>(*this);
}

constexpr decimal_fast32_t::operator long long() const noexcept
{
    return to_integral<decimal_fast32_t, long long>(*this);
}

constexpr decimal_fast32_t::operator unsigned long long() const noexcept
{
    return to_integral<decimal_fast32_t, unsigned long long>(*this);
}

#ifdef BOOST_DECIMAL_HAS_INT128

constexpr decimal_fast32_t::operator detail::builtin_int128_t() const noexcept
{
    return to_integral<decimal_fast32_t, detail::builtin_int128_t>(*this);
}

constexpr decimal_fast32_t::operator detail::builtin_uint128_t() const noexcept
{
    return to_integral<decimal_fast32_t, detail::builtin_uint128_t>(*this);
}

#endif

BOOST_DECIMAL_CXX20_CONSTEXPR decimal_fast32_t::operator float() const noexcept
{
    return to_float<decimal_fast32_t, float>(*this);
}

BOOST_DECIMAL_CXX20_CONSTEXPR decimal_fast32_t::operator double() const noexcept
{
    return to_float<decimal_fast32_t, double>(*this);
}

#ifndef BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE
BOOST_DECIMAL_CXX20_CONSTEXPR decimal_fast32_t::operator long double() const noexcept
{
    // The precision and range of double already exceeds what decimal_fast32_t can provide
    return static_cast<long double>(to_float<decimal_fast32_t, double>(*this));
}
#endif

#ifdef BOOST_DECIMAL_HAS_FLOAT16
constexpr decimal_fast32_t::operator std::float16_t() const noexcept
{
    return static_cast<std::float16_t>(to_float<decimal_fast32_t, float>(*this));
}
#endif
#ifdef BOOST_DECIMAL_HAS_FLOAT32
constexpr decimal_fast32_t::operator std::float32_t() const noexcept
{
    return static_cast<std::float32_t>(to_float<decimal_fast32_t, float>(*this));
}
#endif
#ifdef BOOST_DECIMAL_HAS_FLOAT64
constexpr decimal_fast32_t::operator std::float64_t() const noexcept
{
    return static_cast<std::float64_t>(to_float<decimal_fast32_t, double>(*this));
}
#endif
#ifdef BOOST_DECIMAL_HAS_BRAINFLOAT16
constexpr decimal_fast32_t::operator std::bfloat16_t() const noexcept
{
    return static_cast<std::bfloat16_t>(to_float<decimal_fast32_t, float>(*this));
}
#endif

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal, std::enable_if_t<detail::is_decimal_floating_point_v<Decimal> && (detail::decimal_val_v<Decimal> > detail::decimal_val_v<decimal_fast32_t>), bool>>
constexpr decimal_fast32_t::operator Decimal() const noexcept
{
    return to_decimal<Decimal>(*this);
}

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE Decimal, std::enable_if_t<detail::is_decimal_floating_point_v<Decimal> && (detail::decimal_val_v<Decimal> <= detail::decimal_val_v<decimal_fast32_t>), bool>>
constexpr decimal_fast32_t::operator Decimal() const noexcept
{
    return to_decimal<Decimal>(*this);
}

constexpr auto scalblnd32f(const decimal_fast32_t num, const long exp) noexcept -> decimal_fast32_t
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    constexpr decimal_fast32_t zero {0, 0};

    if (num == zero || exp == 0 || !isfinite(num))
    {
        return num;
    }
    #endif

    return decimal_fast32_t(num.significand_, num.biased_exponent() + exp, num.sign_);
}

constexpr auto scalbnd32f(const decimal_fast32_t num, const int expval) noexcept -> decimal_fast32_t
{
    return scalblnd32f(num, static_cast<long>(expval));
}

constexpr auto copysignd32f(decimal_fast32_t mag, const decimal_fast32_t sgn) noexcept -> decimal_fast32_t
{
    mag.sign_ = sgn.sign_;
    return mag;
}

// Effects: determines if the quantum exponents of x and y are the same.
// If both x and y are NaN, or infinity, they have the same quantum exponents;
// if exactly one operand is infinity or exactly one operand is NaN, they do not have the same quantum exponents.
// The samequantum functions raise no exception.
constexpr auto samequantumd32f(const decimal_fast32_t lhs, const decimal_fast32_t rhs) noexcept -> bool
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
constexpr auto quantexpd32f(const decimal_fast32_t x) noexcept -> int
{
    #ifndef BOOST_DECIMAL_FAST_MATH
    if (!isfinite(x))
    {
        return INT_MIN;
    }
    #endif

    return static_cast<int>(x.unbiased_exponent());
}

#if !defined(BOOST_DECIMAL_DISABLE_CLIB)

constexpr decimal_fast32_t::decimal_fast32_t(const char* str, const std::size_t len)
{
    if (str == nullptr || len == 0)
    {
        *this = direct_init(detail::d32_fast_qnan, UINT8_C((0)));
        BOOST_DECIMAL_THROW_EXCEPTION(std::runtime_error("Can not construct from invalid string"));
        return; // LCOV_EXCL_LINE
    }

    // Normally plus signs aren't allowed
    auto first {str};
    if (*first == '+')
    {
        ++first;
    }

    decimal_fast32_t v;
    const auto r {detail::from_chars_general_impl(first, str + len, v, chars_format::general)};
    if (r)
    {
        *this = v;
    }
    else
    {
        *this = direct_init(detail::d32_fast_qnan, UINT8_C((0)));
        BOOST_DECIMAL_THROW_EXCEPTION(std::runtime_error("Can not construct from invalid string"));
    }
}

constexpr decimal_fast32_t::decimal_fast32_t(const char* str) : decimal_fast32_t(str, detail::strlen(str)) {}

#ifndef BOOST_DECIMAL_HAS_STD_STRING_VIEW
inline decimal_fast32_t::decimal_fast32_t(const std::string& str) : decimal_fast32_t(str.c_str(), str.size()) {}
#else
constexpr decimal_fast32_t::decimal_fast32_t(std::string_view str) : decimal_fast32_t(str.data(), str.size()) {}
#endif

#endif // BOOST_DECIMAL_DISABLE_CLIB

} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_decimal_fast32_t_HPP
