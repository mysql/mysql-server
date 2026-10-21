//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_IMPL_RELATIVE_PART_RULE_HPP
#define BOOST_URL_RFC_DETAIL_IMPL_RELATIVE_PART_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/rfc/detail/path_rules.hpp>
#include <boost/url/rfc/pct_encoded_rule.hpp>
#include <boost/url/rfc/pchars.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>

namespace boost {
namespace urls {
namespace detail {

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
auto
relative_part_rule_t::
parse(
    char const*& it,
    char const* const end
        ) const noexcept ->
    system::result<value_type>
{
    constexpr auto pchars_nc = pchars - ':';

    value_type t;
    if(it == end)
    {
        // path-empty
        return t;
    }
    if(end - it == 1)
    {
        if(*it == '/')
        {
            // path-absolute
            t.path = make_pct_string_view_unsafe(
                it, 1, 1);
            t.segment_count = 1;
            ++it;
            return t;
        }
        if(*it != ':')
        {
            // path-noscheme or
            // path-empty
            auto rv = grammar::parse(
                it, end, segment_rule);
            if(! rv)
                return rv.error();
            if(! rv->empty())
            {
                t.path = *rv;
                t.segment_count = 1;
            }
        }
        // path-empty
        return t;
    }
    if( it[0] == '/' &&
        it[1] == '/')
    {
        // "//" authority
        it += 2;
        auto rv = grammar::parse(
            it, end, authority_rule);
        if(! rv)
            return rv.error();
        t.authority = *rv;
        t.has_authority = true;
    }
    if(it == end)
    {
        // path-empty
        return t;
    }
    auto const it0 = it;
    std::size_t dn = 0;
    if(*it != '/')
    {
        // segment_nc
        auto rv = grammar::parse(it, end,
            pct_encoded_rule(pchars_nc));
        if(! rv)
            return rv.error();
        if(rv->empty())
            return t;
        dn += rv->decoded_size();
        ++t.segment_count;
        if( it != end &&
            *it == ':')
        {
            BOOST_URL_CONSTEXPR_RETURN_EC(
                grammar::error::mismatch);
        }
    }
    while(it != end)
    {
        if(*it == '/')
        {
            ++dn;
            ++it;
            ++t.segment_count;
            continue;
        }
        auto rv = grammar::parse(
            it, end, segment_rule);
        if(! rv)
            return rv.error();
        if(rv->empty())
            break;
        dn += rv->decoded_size();
    }
    t.path = make_pct_string_view_unsafe(
        it0, it - it0, dn);
    return t;
}

} // detail
} // urls
} // boost


#endif
