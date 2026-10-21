//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_SCHEME_HPP
#define BOOST_URL_IMPL_SCHEME_HPP

#include <boost/url/detail/config.hpp>

#include <boost/url/grammar/ci_string.hpp>

namespace boost {
namespace urls {

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
scheme
string_to_scheme(
    core::string_view s) noexcept
{
    using grammar::to_lower;
    switch(s.size())
    {
    case 0: // none
        return scheme::none;

    case 2: // ws
        if( to_lower(s[0]) == 'w' &&
            to_lower(s[1]) == 's')
            return scheme::ws;
        break;

    case 3:
        switch(to_lower(s[0]))
        {
        case 'w': // wss
            if( to_lower(s[1]) == 's' &&
                to_lower(s[2]) == 's')
                return scheme::wss;
            break;

        case 'f': // ftp
            if( to_lower(s[1]) == 't' &&
                to_lower(s[2]) == 'p')
                return scheme::ftp;
            break;

        default:
            break;
        }
        break;

    case 4:
        switch(to_lower(s[0]))
        {
        case 'f': // file
            if( to_lower(s[1]) == 'i' &&
                to_lower(s[2]) == 'l' &&
                to_lower(s[3]) == 'e')
                return scheme::file;
            break;

        case 'h': // http
            if( to_lower(s[1]) == 't' &&
                to_lower(s[2]) == 't' &&
                to_lower(s[3]) == 'p')
                return scheme::http;
            break;

        default:
            break;
        }
        break;

    case 5: // https
        if( to_lower(s[0]) == 'h' &&
            to_lower(s[1]) == 't' &&
            to_lower(s[2]) == 't' &&
            to_lower(s[3]) == 'p' &&
            to_lower(s[4]) == 's')
            return scheme::https;
        break;

    default:
        break;
    }
    return scheme::unknown;
}

inline
core::string_view
to_string(scheme s) noexcept
{
    switch(s)
    {
    case scheme::ftp:   return "ftp";
    case scheme::file:  return "file";
    case scheme::http:  return "http";
    case scheme::https: return "https";
    case scheme::ws:    return "ws";
    case scheme::wss:   return "wss";
    case scheme::none:  return {};
    default:
        break;
    }
    return "<unknown>";
}

inline
std::uint16_t
default_port(scheme s) noexcept
{
    switch(s)
    {
    case scheme::ftp:
        return 21;
    case scheme::http:
    case scheme::ws:
        return 80;
    case scheme::https:
    case scheme::wss:
        return 443;
    default:
        break;
    }
    return 0;
}

} // urls
} // boost

#endif
