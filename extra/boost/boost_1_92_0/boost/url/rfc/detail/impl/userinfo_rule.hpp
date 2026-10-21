//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_IMPL_USERINFO_RULE_HPP
#define BOOST_URL_RFC_DETAIL_IMPL_USERINFO_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/rfc/pct_encoded_rule.hpp>
#include <boost/url/rfc/sub_delim_chars.hpp>
#include <boost/url/rfc/unreserved_chars.hpp>
#include <boost/url/grammar/parse.hpp>

namespace boost {
namespace urls {
namespace detail {

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
auto
userinfo_rule_t::
parse(
    char const*& it,
    char const* const end
        ) const noexcept ->
    system::result<value_type>
{
    constexpr auto uchars =
        unreserved_chars +
        sub_delim_chars;
    constexpr auto pwchars =
        uchars + ':';

    value_type t;

    // user
    auto rv = grammar::parse(
        it, end, pct_encoded_rule(
            grammar::ref(uchars)));
    if(! rv)
        return rv.error();
    t.user = *rv;

    // ':'
    if( it == end ||
        *it != ':')
    {
        t.has_password = false;
        t.password = {};
        return t;
    }
    ++it;

    // pass
    rv = grammar::parse(
        it, end, pct_encoded_rule(
            grammar::ref(pwchars)));
    if(! rv)
        return rv.error();

    t.has_password = true;
    t.password = *rv;
    return t;
}

} // detail
} // urls
} // boost


#endif
