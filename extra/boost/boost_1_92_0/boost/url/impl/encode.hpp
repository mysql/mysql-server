//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_ENCODE_HPP
#define BOOST_URL_IMPL_ENCODE_HPP

#include <boost/url/grammar/token_rule.hpp>
#include <boost/assert.hpp>
#include <boost/core/detail/static_assert.hpp>
#include <boost/url/detail/encode.hpp>
#include <boost/url/detail/except.hpp>
#include <boost/url/encoding_opts.hpp>
#include <boost/url/grammar/charset.hpp>
#include <boost/url/grammar/hexdig_chars.hpp>
#include <boost/url/grammar/string_token.hpp>
#include <boost/url/grammar/type_traits.hpp>

namespace boost {
namespace urls {

//------------------------------------------------

template<BOOST_URL_CONSTRAINT(grammar::CharSet) CS>
std::size_t
encoded_size(
    core::string_view s,
    CS const& allowed,
    encoding_opts opt) noexcept
{
    /*
        If you get a compilation error here, it
        means that the value you passed does
        not meet the requirements stated in
        the documentation.
    */
    BOOST_CORE_STATIC_ASSERT(
        grammar::is_charset<CS>::value);

    std::size_t n = 0;
    auto it = s.data();
    auto const last = it + s.size();

    if (!opt.space_as_plus)
    {
        while (it != last)
        {
            char const c = *it;
            if (allowed(c))
            {
                ++n;
            }
            else
            {
                n += 3;
            }
            ++it;
        }
    }
    else
    {
        // '+' is always encoded (thus
        // spending 3 chars) even if
        // allowed because "%2B" and
        // "+" have different meanings
        // when space as plus is enabled
        using FNT = bool (*)(CS const& allowed, char);
        FNT takes_one_char =
            allowed('+') ?
                (allowed(' ') ?
                     FNT([](CS const& allowed, char c){ return allowed(c) && c != '+'; }) :
                     FNT([](CS const& allowed, char c){ return (allowed(c) || c == ' ') && c != '+'; })) :
                (allowed(' ') ?
                     FNT([](CS const& allowed, char c){ return allowed(c); }) :
                     FNT([](CS const& allowed, char c){ return allowed(c) || c == ' '; }));
        while (it != last)
        {
            char const c = *it;
            if (takes_one_char(allowed, c))
            {
                ++n;
            }
            else
            {
                n += 3;
            }
            ++it;
        }
    }
    return n;
}

//------------------------------------------------

template<BOOST_URL_CONSTRAINT(grammar::CharSet) CS>
std::size_t
encode(
    char* dest,
    std::size_t size,
    core::string_view s,
    CS const& allowed,
    encoding_opts opt)
{
/*  If you get a compilation error here, it
    means that the value you passed does
    not meet the requirements stated in
    the documentation.
*/
    BOOST_CORE_STATIC_ASSERT(
        grammar::is_charset<CS>::value);

    // '%' must be reserved
    BOOST_ASSERT(!allowed('%'));

    char const* const hex =
        detail::hexdigs[opt.lower_case];
    auto const encode = [hex](
        char*& dest,
        unsigned char c) noexcept
    {
        *dest++ = '%';
        *dest++ = hex[c>>4];
        *dest++ = hex[c&0xf];
    };

    auto it = s.data();
    auto const end = dest + size;
    auto const last = it + s.size();
    auto const dest0 = dest;

    if (!opt.space_as_plus)
    {
        while(it != last)
        {
            char const c = *it;
            if (allowed(c))
            {
                if(dest == end)
                    return dest - dest0;
                *dest++ = c;
                ++it;
                continue;
            }
            if (end - dest < 3)
                return dest - dest0;
            encode(dest, c);
            ++it;
        }
        return dest - dest0;
    }
    else
    {
        while (it != last)
        {
            char const c = *it;
            if (c == ' ')
            {
                if(dest == end)
                    return dest - dest0;
                *dest++ = '+';
                ++it;
                continue;
            }
            else if (
                allowed(c) &&
                c != '+')
            {
                if(dest == end)
                    return dest - dest0;
                *dest++ = c;
                ++it;
                continue;
            }
            if(end - dest < 3)
                return dest - dest0;
            encode(dest, c);
            ++it;
        }
    }
    return dest - dest0;
}

//------------------------------------------------

// unsafe encode just
// asserts on the output buffer
//
template<BOOST_URL_CONSTRAINT(grammar::CharSet) CS>
std::size_t
encode_unsafe(
    char* dest,
    std::size_t size,
    core::string_view s,
    CS const& allowed,
    encoding_opts opt)
{
    BOOST_CORE_STATIC_ASSERT(
        grammar::is_charset<CS>::value);

    // '%' must be reserved
    BOOST_ASSERT(!allowed('%'));

    auto it = s.data();
    auto const last = it + s.size();
    auto const end = dest + size;
    ignore_unused(end);

    char const* const hex =
        detail::hexdigs[opt.lower_case];
    auto const encode = [end, hex](
        char*& dest,
        unsigned char c) noexcept
    {
        ignore_unused(end);
        *dest++ = '%';
        BOOST_ASSERT(dest != end);
        *dest++ = hex[c>>4];
        BOOST_ASSERT(dest != end);
        *dest++ = hex[c&0xf];
    };

    auto const dest0 = dest;
    if (!opt.space_as_plus)
    {
        while(it != last)
        {
            BOOST_ASSERT(dest != end);
            char const c = *it;
            if(allowed(c))
            {
                *dest++ = c;
            }
            else
            {
                encode(dest, c);
            }
            ++it;
        }
    }
    else
    {
        while(it != last)
        {
            BOOST_ASSERT(dest != end);
            char const c = *it;
            if (c == ' ')
            {
                *dest++ = '+';
            }
            else if (
                allowed(c) &&
                c != '+')
            {
                *dest++ = c;
            }
            else
            {
                encode(dest, c);
            }
            ++it;
        }
    }
    return dest - dest0;
}

//------------------------------------------------

template<
    BOOST_URL_CONSTRAINT(string_token::StringToken) StringToken,
    BOOST_URL_CONSTRAINT(grammar::CharSet) CS>
BOOST_URL_STRTOK_RETURN
encode(
    core::string_view s,
    CS const& allowed,
    encoding_opts opt,
    StringToken&& token)
{
    BOOST_CORE_STATIC_ASSERT(
        grammar::is_charset<CS>::value);

    auto const n = encoded_size(
        s, allowed, opt);
    auto p = token.prepare(n);
    if(n > 0)
        encode_unsafe(
            p, n, s, allowed, opt);
    return token.result();
}

} // urls
} // boost

#endif
