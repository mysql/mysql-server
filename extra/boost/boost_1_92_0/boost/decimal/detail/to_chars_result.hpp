// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_TO_CHARS_RESULT_HPP
#define BOOST_DECIMAL_DETAIL_TO_CHARS_RESULT_HPP

#include <boost/decimal/detail/config.hpp>

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <system_error>
#endif

// 22.13.2, Primitive numerical output conversion

namespace boost {
namespace decimal {

// This is how the STL says to implement
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpadded"
#endif

BOOST_DECIMAL_EXPORT struct to_chars_result
{
    char *ptr;
    std::errc ec;

    BOOST_DECIMAL_CUDA_CONSTEXPR friend bool operator==(const to_chars_result &lhs, const to_chars_result &rhs) noexcept
    {
        return lhs.ptr == rhs.ptr && lhs.ec == rhs.ec;
    }

    BOOST_DECIMAL_CUDA_CONSTEXPR friend bool operator!=(const to_chars_result &lhs, const to_chars_result &rhs) noexcept
    {
        return !(lhs == rhs);
    }

    BOOST_DECIMAL_CUDA_CONSTEXPR explicit operator bool() const noexcept { return ec == std::errc{}; }
};

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_TO_CHARS_RESULT_HPP
