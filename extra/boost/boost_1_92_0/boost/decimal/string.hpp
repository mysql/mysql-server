// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_STRING_HPP
#define BOOST_DECIMAL_STRING_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/charconv.hpp>

#if !defined(BOOST_DECIMAL_DISABLE_CLIB)

namespace boost {
namespace decimal {

namespace detail {

template <typename DecimalType>
auto from_string_impl(const std::string& str, std::size_t* idx)
    BOOST_DECIMAL_REQUIRES(detail::is_decimal_floating_point_v, DecimalType)
{
    DecimalType val;
    const auto r {from_chars(str, val)};

    if (r.ec == std::errc::result_out_of_range || val == std::numeric_limits<DecimalType>::infinity())
    {
        val = std::numeric_limits<DecimalType>::signaling_NaN();
        BOOST_DECIMAL_THROW_EXCEPTION(std::out_of_range("Conversion is outside the range of the type"));
    }
    else if (r.ec != std::errc{})
    {
        val = std::numeric_limits<DecimalType>::signaling_NaN();
        BOOST_DECIMAL_THROW_EXCEPTION(std::invalid_argument("Conversion could not be performed"));
    }
    else
    {
        if (idx != nullptr)
        {
            *idx = static_cast<std::size_t>(r.ptr - str.data());
        }
    }

    return val;
}

} // namespace detail

BOOST_DECIMAL_EXPORT inline auto stod32(const std::string& str, std::size_t* idx = nullptr) -> decimal32_t
{
    return detail::from_string_impl<decimal32_t>(str, idx);
}

BOOST_DECIMAL_EXPORT inline auto stod32f(const std::string& str, std::size_t* idx = nullptr) -> decimal_fast32_t
{
    return detail::from_string_impl<decimal_fast32_t>(str, idx);
}

BOOST_DECIMAL_EXPORT inline auto stod64(const std::string& str, std::size_t* idx = nullptr) -> decimal64_t
{
    return detail::from_string_impl<decimal64_t>(str, idx);
}

BOOST_DECIMAL_EXPORT inline auto stod64f(const std::string& str, std::size_t* idx = nullptr) -> decimal_fast64_t
{
    return detail::from_string_impl<decimal_fast64_t>(str, idx);
}

BOOST_DECIMAL_EXPORT inline auto stod128(const std::string& str, std::size_t* idx = nullptr) -> decimal128_t
{
    return detail::from_string_impl<decimal128_t>(str, idx);
}

BOOST_DECIMAL_EXPORT inline auto stod128f(const std::string& str, std::size_t* idx = nullptr) -> decimal_fast128_t
{
    return detail::from_string_impl<decimal_fast128_t>(str, idx);
}

BOOST_DECIMAL_EXPORT template <typename DecimalType>
auto to_string(const DecimalType value)
    BOOST_DECIMAL_REQUIRES_RETURN(detail::is_decimal_floating_point_v, DecimalType, std::string)
{
    char buffer[64];
    auto r = to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, r.ptr - buffer);
}

} //namespace decimal
} //namespace boost

#endif

#endif // BOOST_DECIMAL_STRING_HPP
