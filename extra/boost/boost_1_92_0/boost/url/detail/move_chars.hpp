//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_MOVE_CHARS_HPP
#define BOOST_URL_DETAIL_MOVE_CHARS_HPP

#include <boost/core/detail/string_view.hpp>
#include <boost/assert.hpp>
#include <cstring>
#include <functional>

namespace boost {
namespace urls {
namespace detail {

// Moves characters, and adjusts any passed
// views if they point to any moved characters.

// true if s completely overlapped by buf
inline
bool
is_overlapping(
    core::string_view buf,
    core::string_view s) noexcept
{
    auto const b0 = buf.data();
    auto const e0 = b0 + buf.size();
    auto const b1 = s.data();
    auto const e1 = b1 + s.size();
    auto const less_equal =
        std::less_equal<char const*>();
    if(less_equal(e0, b1))
        return false;
    if(less_equal(e1, b0))
        return false;
    // partial overlap is undefined
    BOOST_ASSERT(less_equal(e1, e0));
    BOOST_ASSERT(less_equal(b0, b1));
    return true;
}

inline
void
move_chars_impl(
    std::ptrdiff_t,
    core::string_view const&) noexcept
{
}

template<class... Sn>
void
move_chars_impl(
    std::ptrdiff_t d,
    core::string_view const& buf,
    core::string_view& s,
    Sn&... sn) noexcept
{
    if(is_overlapping(buf, s))
        s = {s.data() + d, s.size()};
    move_chars_impl(d, buf, sn...);
}

template<class... Args>
void
move_chars(
    char* dest,
    char const* src,
    std::size_t n,
    Args&... args) noexcept
{
    move_chars_impl(
        dest - src,
        core::string_view(src, n),
        args...);
    std::memmove(
        dest, src, n);
}

} // detail
} // urls
} // boost

#endif
