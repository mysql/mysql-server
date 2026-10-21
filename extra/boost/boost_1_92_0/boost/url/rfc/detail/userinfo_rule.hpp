//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_USERINFO_RULE_HPP
#define BOOST_URL_RFC_DETAIL_USERINFO_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/error_types.hpp>
#include <boost/url/pct_string_view.hpp>

namespace boost {
namespace urls {
namespace detail {

/** Rule for userinfo

    @par BNF
    @code
    userinfo    = user [ ":" [ password ] ]

    user        = *( unreserved / pct-encoded / sub-delims )
    password    = *( unreserved / pct-encoded / sub-delims / ":" )
    @endcode

    @par Specification
    <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-3.2.1"
        >3.2.1. User Information (3986)</a>
*/
struct userinfo_rule_t
{
    struct value_type
    {
        pct_string_view user;
        pct_string_view password;
        bool has_password = false;
    };

    BOOST_URL_CXX20_CONSTEXPR
    auto
    parse(
        char const*& it,
        char const* end
            ) const noexcept ->
        system::result<value_type>;
};

constexpr userinfo_rule_t userinfo_rule{};

} // detail
} // urls
} // boost

#include <boost/url/rfc/detail/impl/userinfo_rule.hpp>

#endif
