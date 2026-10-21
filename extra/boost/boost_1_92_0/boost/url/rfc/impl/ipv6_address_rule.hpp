//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_IMPL_IPV6_ADDRESS_RULE_HPP
#define BOOST_URL_RFC_IMPL_IPV6_ADDRESS_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/rfc/ipv4_address_rule.hpp>
#include <boost/url/rfc/detail/h16_rule.hpp>
#include <boost/url/grammar/charset.hpp>
#include <boost/url/grammar/hexdig_chars.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/assert.hpp>
#include <cstring>

namespace boost {
namespace urls {

namespace detail {

// return `true` if the hex
// word could be 0..255 if
// interpreted as decimal
BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
bool
maybe_octet(
    unsigned char const* p) noexcept
{
    unsigned short word =
        static_cast<unsigned short>(
            p[0]) * 256 +
        static_cast<unsigned short>(
            p[1]);
    if(word > 0x255)
        return false;
    if(((word >>  4) & 0xf) > 9)
        return false;
    if((word & 0xf) > 9)
        return false;
    return true;
}

} // detail

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
auto
implementation_defined::ipv6_address_rule_t::
parse(
    char const*& it,
    char const* const end
        ) const noexcept ->
    system::result<ipv6_address>
{
    int n = 8;      // words needed
    int b = -1;     // value of n
                    // when '::' seen
    bool c = false; // need colon
    auto prev = it;
    ipv6_address::bytes_type bytes;
    system::result<detail::h16_rule_t::value_type> rv;
    for(;;)
    {
        if(it == end)
        {
            if(b != -1)
            {
                // end in "::"
                break;
            }
            BOOST_ASSERT(n > 0);
            // not enough words
            BOOST_URL_CONSTEXPR_RETURN_EC(
                grammar::error::invalid);
        }
        if(*it == ':')
        {
            ++it;
            if(it == end)
            {
                // expected ':'
                BOOST_URL_CONSTEXPR_RETURN_EC(
                    grammar::error::invalid);
            }
            if(*it == ':')
            {
                if(b == -1)
                {
                    // first "::"
                    ++it;
                    --n;
                    b = n;
                    if(n == 0)
                        break;
                    c = false;
                    continue;
                }
                // extra "::" found
                BOOST_URL_CONSTEXPR_RETURN_EC(
                    grammar::error::invalid);
            }
            if(c)
            {
                prev = it;
                rv = grammar::parse(
                    it, end,
                    detail::h16_rule);
                if(! rv)
                    return rv.error();
                bytes[2*(8-n)+0] = rv->hi;
                bytes[2*(8-n)+1] = rv->lo;
                --n;
                if(n == 0)
                    break;
                continue;
            }
            // expected h16
            BOOST_URL_CONSTEXPR_RETURN_EC(
                grammar::error::invalid);
        }
        if(*it == '.')
        {
            if(b == -1 && n > 1)
            {
                // not enough h16
                BOOST_URL_CONSTEXPR_RETURN_EC(
                    grammar::error::invalid);
            }
            if(! c)
            {
                // missing h16 before "."
                BOOST_URL_CONSTEXPR_RETURN_EC(
                    grammar::error::invalid);
            }
            if(! detail::maybe_octet(
                &bytes[2*(7-n)]))
            {
                // invalid octet
                BOOST_URL_CONSTEXPR_RETURN_EC(
                    grammar::error::invalid);
            }
            // rewind the h16 and
            // parse it as ipv4
            it = prev;
            auto rv1 = grammar::parse(
                it, end, ipv4_address_rule);
            if(! rv1)
                return rv1.error();
            auto v4 = *rv1;
            auto const b4 =
                v4.to_bytes();
            bytes[2*(7-n)+0] = b4[0];
            bytes[2*(7-n)+1] = b4[1];
            bytes[2*(7-n)+2] = b4[2];
            bytes[2*(7-n)+3] = b4[3];
            --n;
            break;
        }
        auto d =
            grammar::hexdig_value(*it);
        if( b != -1 &&
            d < 0)
        {
            // ends in "::"
            break;
        }
        if(! c)
        {
            prev = it;
            rv = grammar::parse(
                it, end,
                detail::h16_rule);
            if(! rv)
                return rv.error();
            bytes[2*(8-n)+0] = rv->hi;
            bytes[2*(8-n)+1] = rv->lo;
            --n;
            if(n == 0)
                break;
            c = true;
            continue;
        }
        // ':' divides a word
        BOOST_URL_CONSTEXPR_RETURN_EC(
            grammar::error::invalid);
    }
    if(b == -1)
        return ipv6_address{bytes};
    if(b == n)
    {
        // "::" last
        auto const i =
            2 * (7 - n);
        std::memset(
            &bytes[i],
            0, 16 - i);
    }
    else if(b == 7)
    {
        // "::" first
        auto const i =
            2 * (b - n);
        std::memmove(
            &bytes[16 - i],
            &bytes[2],
            i);
        std::memset(
            &bytes[0],
            0, 16 - i);
    }
    else
    {
        // "::" in middle
        auto const i0 =
            2 * (7 - b);
        auto const i1 =
            2 * (b - n);
        std::memmove(
            &bytes[16 - i1],
            &bytes[i0 + 2],
            i1);
        std::memset(
            &bytes[i0],
            0, 16 - (i0 + i1));
    }
    return ipv6_address{bytes};
}

} // urls
} // boost


#endif
