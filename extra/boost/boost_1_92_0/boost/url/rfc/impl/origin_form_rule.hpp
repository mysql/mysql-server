//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_IMPL_ORIGIN_FORM_RULE_HPP
#define BOOST_URL_RFC_IMPL_ORIGIN_FORM_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/url_view.hpp>
#include <boost/url/rfc/detail/path_rules.hpp>
#include <boost/url/rfc/detail/query_part_rule.hpp>
#include <boost/url/grammar/parse.hpp>

namespace boost {
namespace urls {

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
auto
implementation_defined::origin_form_rule_t::
parse(
    char const*& it,
    char const* end
        ) const noexcept ->
    system::result<value_type>
{
    detail::url_impl u(detail::url_impl::from::string);
    u.cs_ = it;

    // absolute-path = 1*( "/" segment )
    {
        if(it == end || *it != '/')
        {
            BOOST_URL_CONSTEXPR_RETURN_EC(
                grammar::error::mismatch);
        }
        auto const it0 = it;
        std::size_t dn = 0;
        std::size_t nseg = 0;
        while(it != end)
        {
            if(*it == '/')
            {
                ++dn;
                ++it;
                ++nseg;
                continue;
            }
            auto rv = grammar::parse(
                it, end, detail::segment_rule);
            if(! rv)
                return rv.error();
            if(rv->empty())
                break;
            dn += rv->decoded_size();
        }
        u.apply_path(
            make_pct_string_view_unsafe(
                it0, it - it0, dn),
            nseg);
    }

    // [ "?" query ]
    {
        // query_part_rule always succeeds
        auto rv = grammar::parse(
            it, end, detail::query_part_rule);
        if(rv->has_query)
        {
            // map "?" to { {} }
            u.apply_query(
                rv->query,
                rv->count);
        }
    }

    return url_view(u);
}

} // urls
} // boost


#endif
