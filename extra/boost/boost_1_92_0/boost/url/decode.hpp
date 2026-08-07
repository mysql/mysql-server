//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DECODE_HPP
#define BOOST_URL_DECODE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/error_types.hpp>
#include <boost/url/encoding_opts.hpp>
#include <boost/url/grammar/string_token.hpp>
#include <boost/core/detail/string_view.hpp>

namespace boost {
namespace urls {

/** Return the buffer size needed for percent-decoding

    This function returns the exact number of bytes needed
    to store the decoded form of the specified string using
    the given options. The string is validated before the
    size is computed; malformed escapes cause the returned
    result to contain an error instead.

    @par Example
    @code
    auto n = decoded_size( "My%20Stuff" );
    assert( n && *n == 8 );
    @endcode

    @par Exception Safety
    Throws nothing. Validation errors are reported in the
    returned result.

    @return A result containing the decoded size, excluding
    any null terminator.

    @param s The string to measure.

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-2.1"
        >2.1. Percent-Encoding (rfc3986)</a>

    @see
        @ref decode,
        @ref encoding_opts,
        @ref make_pct_string_view.
*/
system::result<std::size_t>
decoded_size(core::string_view s) noexcept;

/** Apply percent-decoding to an arbitrary string

    This function percent-decodes the specified string into
    the destination buffer provided by the caller. The input
    is validated first; malformed escapes cause the returned
    result to hold an error instead of a size. If the buffer
    is too small, the output is truncated and the number of
    bytes actually written is returned.

    @par Example
    @code
    char buf[100];
    auto n = decode( buf, sizeof(buf), "Program%20Files" );
    assert( n && *n == 13 );
    @endcode

    @par Exception Safety
    Throws nothing. Validation errors are reported in the
    returned result.

    @return The number of characters written to the
    destination buffer, or an error.

    @param dest The destination buffer to write to.

    @param size The number of writable characters pointed
    to by `dest`. If this is less than the decoded size, the
    result is truncated.

    @param s The string to decode.

    @param opt The decoding options. If omitted, the
    default options are used.

    @par Specification
    @li <a href="https://datatracker.ietf.org/dodc/html/rfc3986#section-2.1"
        >2.1. Percent-Encoding (rfc3986)</a>

    @see
        @ref decoded_size,
        @ref encoding_opts,
        @ref make_pct_string_view.
*/
system::result<std::size_t>
decode(
    char* dest,
    std::size_t size,
    core::string_view s,
    encoding_opts opt = {}) noexcept;

//------------------------------------------------

/** Return a percent-decoded string

    This function percent-decodes the specified string and
    returns the result using any @ref string_token. The
    string is validated before decoding; malformed escapes
    cause the returned result to hold an error.

    @par Example
    @code
    auto plain = decode( "My%20Stuff" );
    assert( plain && *plain == "My Stuff" );
    @endcode

    @par Exception Safety
    Calls to allocate may throw. Validation errors are
    reported in the returned result.

    @return A result containing the decoded string in the
    format described by the passed string token.

    @param s The string to decode.

    @param opt The decoding options. If omitted, the
    default options are used.

    @param token A string token.

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-2.1"
        >2.1. Percent-Encoding (rfc3986)</a>

    @see
        @ref decode,
        @ref decoded_size,
        @ref encoding_opts,
        @ref string_token::return_string.
*/
template<BOOST_URL_STRTOK_TPARAM>
system::result<typename StringToken::result_type>
decode(
    core::string_view s,
    encoding_opts opt = {},
    StringToken&& token = {});

} // urls
} // boost

#include <boost/url/impl/decode.hpp>

#endif
