//
// Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_CHARSETS_HPP
#define BOOST_URL_RFC_DETAIL_CHARSETS_HPP

#include <boost/url/rfc/pchars.hpp>
#include <boost/url/rfc/sub_delim_chars.hpp>
#include <boost/url/rfc/unreserved_chars.hpp>

namespace boost {
namespace urls {
namespace detail {

struct empty_chars_t
{
    constexpr
    bool
    operator()(char) const noexcept
    {
        return false;
    }
};

constexpr
empty_chars_t
empty_chars{};

constexpr
auto
user_chars =
    unreserved_chars + sub_delim_chars;

constexpr
auto
password_chars =
    unreserved_chars + sub_delim_chars + ':';

constexpr
auto
userinfo_chars =
    password_chars;

constexpr
auto
host_chars =
    unreserved_chars + sub_delim_chars;

constexpr
auto
reg_name_chars =
    unreserved_chars + '-' + '.';

constexpr
auto
segment_chars =
    pchars;

constexpr
auto
path_chars =
    segment_chars + '/';

constexpr
auto
query_chars =
    pchars + '/' + '?' + '[' + ']';

constexpr
grammar::lut_chars
query_ignore_chars =
    "&=+";

constexpr
auto
param_key_chars = pchars
    + '/' + '?' + '[' + ']'
    - '&' - '=';

constexpr
auto
param_value_chars = pchars
    + '/' + '?'
    - '&';

constexpr
auto
fragment_chars =
    pchars + '/' + '?' + '#';

constexpr
auto
nocolon_pchars =
    pchars - ':';

} // detail
} // urls
} // boost

#endif
