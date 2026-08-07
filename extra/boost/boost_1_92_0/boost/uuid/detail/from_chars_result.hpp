#ifndef BOOST_UUID_DETAIL_FROM_CHARS_RESULT_HPP_INCLUDED
#define BOOST_UUID_DETAIL_FROM_CHARS_RESULT_HPP_INCLUDED

// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

namespace boost {
namespace uuids {

enum class from_chars_error
{
    none = 0,

    unexpected_end_of_input,
    hex_digit_expected,
    dash_expected,
    closing_brace_expected,
    unexpected_extra_input
};

template<class Ch> struct from_chars_result
{
    Ch const* ptr;
    from_chars_error ec;

    constexpr explicit operator bool() const noexcept { return ec == from_chars_error::none; }
};

}} //namespace boost::uuids

#endif // BOOST_UUID_DETAIL_FROM_CHARS_RESULT_HPP_INCLUDED
