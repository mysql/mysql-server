// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_CHARCONV_DETAIL_TO_CHARS_RESULT_HPP
#define BOOST_CHARCONV_DETAIL_TO_CHARS_RESULT_HPP

#include <boost/charconv/detail/config.hpp>
#include <system_error>

// 22.13.2, Primitive numerical output conversion

namespace boost { namespace charconv {

struct to_chars_result
{
    char *ptr;
    std::errc ec;

    BOOST_CHARCONV_HOST_DEVICE constexpr friend bool operator==(const to_chars_result &lhs, const to_chars_result &rhs) noexcept
    {
        return lhs.ptr == rhs.ptr && lhs.ec == rhs.ec;
    }

    BOOST_CHARCONV_HOST_DEVICE constexpr friend bool operator!=(const to_chars_result &lhs, const to_chars_result &rhs) noexcept
    {
        return !(lhs == rhs);
    }

    BOOST_CHARCONV_HOST_DEVICE constexpr explicit operator bool() const noexcept { return ec == std::errc{}; }
};

}} // Namespaces

#endif //BOOST_CHARCONV_DETAIL_TO_CHARS_RESULT_HPP
