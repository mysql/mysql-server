//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_ENCODING_OPTS_HPP
#define BOOST_URL_ENCODING_OPTS_HPP

#include <boost/url/detail/config.hpp>

namespace boost {
namespace urls {

/** Percent-encoding options

    These options are used to customize
    the behavior of algorithms which use
    percent escapes, such as encoding
    or decoding.

    @see
        @ref encode,
        @ref encoded_size,
        @ref pct_string_view.
*/
struct encoding_opts
{
    /** True if spaces encode to and from plus signs

        Although not prescribed by RFC 3986,
        many applications decode plus signs
        in URL queries as spaces. In particular,
        the form-urlencoded Media Type in HTML
        for submitting forms uses this convention.

        This option controls whether
        the PLUS character ("+") is used to
        represent the SP character (" ") when
        encoding or decoding.

        When this option is `true`, both the
        encoded SP ("%20") and the PLUS
        character ("+") represent a space (" ")
        when decoding. To represent a plus sign,
        its encoded form ("%2B") is used.

        The @ref encode and @ref encoded_size functions
        will encode spaces as plus signs when
        this option is `true`, regardless of the
        allowed character set. They will also
        encode plus signs as "%2B" when this
        option is `true`, regardless of the
        allowed character set.

        Note that when a URL is normalized,
        all unreserved percent-encoded characters are
        replaced with their unreserved equivalents.
        However, normalizing the URL query maintains
        the decoded and encoded "&=+" as they are
        because they might have different meanings.

        This behavior is not optional because
        normalization can only mitigate false
        negatives, but it should eliminate
        false positives.
        Making it optional would allow
        a false positive because there's
        at least one very relevant schema (HTTP)
        where a decoded or encoded "&=+" has different
        meanings and represents different resources.

        The same considerations apply to URL comparison
        algorithms in the library, as they treat URLs
        as if they were normalized.

        @par Specification
        @li <a href="https://www.w3.org/TR/html401/interact/forms.html#h-17.13.4.1">
            application/x-www-form-urlencoded (w3.org)</a>
        @li <a href="https://datatracker.ietf.org/doc/html/rfc1866#section-8.2.1">
            The form-urlencoded Media Type (RFC 1866)</a>
        @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-6.2.2.2">
            Section 6.2.2.2. Percent-Encoding Normalization (RFC 3986)</a>
    */
    bool space_as_plus = false;

    /** True if hexadecimal digits are emitted as lower case

        By default, percent-encoding algorithms
        emit hexadecimal digits A through F as
        uppercase letters. When this option is
        `true`, lowercase letters are used.
    */
    bool lower_case = false;

    /** True if nulls are not allowed

        Normally all possible character values
        (from 0 to 255) are allowed, with reserved
        characters being replaced with escapes
        upon encoding. When this option is true,
        attempting to decode a null will result
        in an error.
    */
    bool disallow_null = false;

    /** Constructs an `encoding_opts` object with the specified options.

        @param space_as_plus If true, spaces will be encoded as plus signs.
        @param lower_case If true, hexadecimal digits will be emitted as lower case.
        @param disallow_null If true, null characters will not be allowed.
     */
    BOOST_CXX14_CONSTEXPR
    inline
    encoding_opts(
        bool const space_as_plus = false,
        bool const lower_case = false,
        bool const disallow_null = false) noexcept
        : space_as_plus(space_as_plus)
        , lower_case(lower_case)
        , disallow_null(disallow_null) {}
};

} // urls
} // boost

#endif
