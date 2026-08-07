// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_CMATH_NAN_HPP
#define BOOST_DECIMAL_DETAIL_CMATH_NAN_HPP

#include <boost/decimal/fwd.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/concepts.hpp>
#include <boost/decimal/detail/utilities.hpp>
#include <boost/decimal/detail/promotion.hpp>
#include <boost/decimal/detail/write_payload.hpp>
#include <boost/decimal/cstdlib.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>
#endif

#if !defined(BOOST_DECIMAL_DISABLE_CLIB)

namespace boost {
namespace decimal {

namespace detail {

template <typename TargetDecimalType, bool is_snan>
constexpr auto nan_impl(const char* arg) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_fast_type_v, TargetDecimalType)
{
    using sig_type = typename TargetDecimalType::significand_type;

    constexpr TargetDecimalType nan_type {is_snan ? std::numeric_limits<TargetDecimalType>::signaling_NaN() :
                                                    std::numeric_limits<TargetDecimalType>::quiet_NaN()};

    sig_type payload_value {};
    const auto r {from_chars_integer_impl<sig_type, sig_type>(arg, arg + detail::strlen(arg), payload_value, 10)};

    if (!r)
    {
        return nan_type;
    }
    else
    {
        return write_payload<TargetDecimalType, is_snan>(payload_value);
    }
}

template <typename TargetDecimalType, bool is_snan>
constexpr auto nan_impl(const char* arg) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_ieee_type_v, TargetDecimalType)
{
    using sig_type = typename TargetDecimalType::significand_type;

    constexpr TargetDecimalType nan_type {is_snan ? std::numeric_limits<TargetDecimalType>::signaling_NaN() :
                                                    std::numeric_limits<TargetDecimalType>::quiet_NaN()};

    sig_type payload_value {};
    const auto r {from_chars_integer_impl<sig_type, sig_type>(arg, arg + detail::strlen(arg), payload_value, 10)};

    if (!r)
    {
        return nan_type;
    }
    else
    {
        return write_payload<TargetDecimalType, is_snan>(payload_value);
    }
}

} //namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto nan(const char* arg) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    return detail::nan_impl<T, false>(arg);
}

BOOST_DECIMAL_EXPORT constexpr auto nand32(const char* arg) noexcept -> decimal32_t
{
    return detail::nan_impl<decimal32_t, false>(arg);
}

BOOST_DECIMAL_EXPORT constexpr auto nand64(const char* arg) noexcept -> decimal64_t
{
    return detail::nan_impl<decimal64_t, false>(arg);
}

BOOST_DECIMAL_EXPORT constexpr auto nand128(const char* arg) noexcept -> decimal128_t
{
    return detail::nan_impl<decimal128_t, false>(arg);
}

BOOST_DECIMAL_EXPORT constexpr auto nand32f(const char* arg) noexcept -> decimal_fast32_t
{
    return detail::nan_impl<decimal_fast32_t, false>(arg);
}

BOOST_DECIMAL_EXPORT constexpr auto nand64f(const char* arg) noexcept -> decimal_fast64_t
{
    return detail::nan_impl<decimal_fast64_t, false>(arg);
}

BOOST_DECIMAL_EXPORT constexpr auto nand128f(const char* arg) noexcept -> decimal_fast128_t
{
    return detail::nan_impl<decimal_fast128_t, false>(arg);
}

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto snan(const char* arg) noexcept
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, T)
{
    return detail::nan_impl<T, true>(arg);
}

BOOST_DECIMAL_EXPORT constexpr auto snand32(const char* arg) noexcept -> decimal32_t
{
    return detail::nan_impl<decimal32_t, true>(arg);
}

BOOST_DECIMAL_EXPORT constexpr auto snand64(const char* arg) noexcept -> decimal64_t
{
    return detail::nan_impl<decimal64_t, true>(arg);
}

BOOST_DECIMAL_EXPORT constexpr auto snand128(const char* arg) noexcept -> decimal128_t
{
    return detail::nan_impl<decimal128_t, true>(arg);
}

BOOST_DECIMAL_EXPORT constexpr auto snand32f(const char* arg) noexcept -> decimal_fast32_t
{
    return detail::nan_impl<decimal_fast32_t, true>(arg);
}

BOOST_DECIMAL_EXPORT constexpr auto snand64f(const char* arg) noexcept -> decimal_fast64_t
{
    return detail::nan_impl<decimal_fast64_t, true>(arg);
}

BOOST_DECIMAL_EXPORT constexpr auto snand128f(const char* arg) noexcept -> decimal_fast128_t
{
    return detail::nan_impl<decimal_fast128_t, true>(arg);
}

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto read_payload(const T value) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_ieee_type_v, T, typename T::significand_type)
{
    const auto positive_value {signbit(value) ? -value : value};

    if (!isnan(value))
    {
        return 0U;
    }
    else if (issignaling(value))
    {
        return positive_value.bits_ ^ std::numeric_limits<T>::signaling_NaN().bits_;
    }
    else
    {
        return positive_value.bits_ ^ std::numeric_limits<T>::quiet_NaN().bits_;
    }
}

namespace detail {

template <typename T>
constexpr auto get_qnan_mask();

template <>
constexpr auto get_qnan_mask<decimal_fast32_t>()
{
    return d32_fast_qnan;
}

template <>
constexpr auto get_qnan_mask<decimal_fast64_t>()
{
    return d64_fast_qnan;
}

template <>
constexpr auto get_qnan_mask<decimal_fast128_t>()
{
    return d128_fast_qnan;
}

template <typename T>
constexpr auto get_snan_mask();

template <>
constexpr auto get_snan_mask<decimal_fast32_t>()
{
    return d32_fast_snan;
}

template <>
constexpr auto get_snan_mask<decimal_fast64_t>()
{
    return d64_fast_snan;
}

template <>
constexpr auto get_snan_mask<decimal_fast128_t>()
{
    return d128_fast_snan;
}

} // namespace detail

BOOST_DECIMAL_EXPORT template <typename T>
constexpr auto read_payload(const T value) noexcept
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_fast_type_v, T, typename T::significand_type)
{
    if (!isnan(value))
    {
        return 0U;
    }
    else if (issignaling(value))
    {
        return value.significand_ ^ detail::get_snan_mask<T>();
    }
    else
    {
        return value.significand_ ^ detail::get_qnan_mask<T>();
    }
}

} //namespace decimal
} //namespace boost

#endif //#if !defined(BOOST_DECIMAL_DISABLE_CLIB)

#endif //BOOST_DECIMAL_DETAIL_CMATH_NAN_HPP
