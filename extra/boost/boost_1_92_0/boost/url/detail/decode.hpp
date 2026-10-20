//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_DECODE_HPP
#define BOOST_URL_DETAIL_DECODE_HPP

#include <boost/url/encoding_opts.hpp>
#include <boost/core/detail/string_view.hpp>
#include <cstdlib>

namespace boost {
namespace urls {
namespace detail {

// Reads two hex digits without checking bounds or validity; invalid input or
// missing digits produces garbage and may touch bytes past the buffer.
BOOST_URL_DECL
char
decode_one(
    char const* it) noexcept;

// Counts decoded bytes assuming the caller already validated escapes; a stray
// '%' still makes it skip three characters, so the reported size can be too
// small and lead to overflow when decoding.
BOOST_URL_DECL
std::size_t
decode_bytes_unsafe(
    core::string_view s) noexcept;

// Writes decoded bytes trusting the buffer is large enough and escapes are
// complete; a short buffer stops decoding early, and a malformed escape zeros
// the remaining space before returning.
BOOST_URL_DECL
std::size_t
decode_unsafe(
    char* dest,
    char const* end,
    core::string_view s,
    encoding_opts opt = {}) noexcept;

} // detail
} // urls
} // boost

#endif
