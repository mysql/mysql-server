//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_PATH_HPP
#define BOOST_URL_DETAIL_PATH_HPP

#include <boost/url/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>

namespace boost {
namespace urls {
namespace detail {

// Return the number of characters at
// the front of the path that are reserved
inline
std::size_t
path_prefix(
    char const* p,
    std::size_t n) noexcept
{
    switch(n)
    {
    case 0:
        return 0;

    case 1:
        if(p[0] == '/')
            return 1;
        return 0;

    case 2:
        if(p[0] == '/')
            return 1;
        if( p[0] == '.' &&
            p[1] == '/')
            return 2;
        return 0;

    default:
        if(p[0] == '/')
        {
            if( p[1] == '.' &&
                p[2] == '/')
                return 3;
            return 1;
        }
        if( p[0] == '.' &&
            p[1] == '/')
            return 2;
        break;
    }
    return 0;
}

// VFALCO DEPRECATED
inline
std::size_t
path_prefix(
    core::string_view s) noexcept
{
    return path_prefix(
        s.data(), s.size());
}

// returns the number of adjusted
// segments based on the malleable prefix.
BOOST_URL_CXX14_CONSTEXPR_OR_INLINE
std::size_t
path_segments(
    core::string_view s,
    std::size_t nseg) noexcept
{
    switch(s.size())
    {
    case 0:
        BOOST_ASSERT(nseg == 0);
        return 0;

    case 1:
        BOOST_ASSERT(nseg == 1);
        if(s[0] == '/')
            return 0;
        return 1;

    case 2:
        if(s[0] == '/')
            return nseg;
        if( s[0] == '.' &&
            s[1] == '/')
        {
            BOOST_ASSERT(nseg > 1);
            return nseg - 1;
        }
        return nseg;

    default:
        if(s[0] == '/')
        {
            if( s[1] == '.' &&
                s[2] == '/')
            {
                BOOST_ASSERT(nseg > 1);
                return nseg - 1;
            }
            return nseg;
        }
        if( s[0] == '.' &&
            s[1] == '/')
        {
            BOOST_ASSERT(nseg > 1);
            return nseg - 1;
        }
        break;
    }
    return nseg;
}

// Trim reserved characters from
// the front of the path.
inline
core::string_view
clean_path(
    core::string_view s) noexcept
{
    s.remove_prefix(
        path_prefix(s));
    return s;
}

} // detail
} // urls
} // boost

#endif