//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_IMPL_ABSOLUTE_URI_RULE_HPP
#define BOOST_URL_RFC_IMPL_ABSOLUTE_URI_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/url_view.hpp>
#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/tuple_rule.hpp>
#include <boost/url/grammar/optional_rule.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/rfc/detail/hier_part_rule.hpp>
#include <boost/url/rfc/detail/query_part_rule.hpp>
#include <boost/url/rfc/detail/scheme_rule.hpp>

namespace boost {
namespace urls {

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
auto
implementation_defined::absolute_uri_rule_t::
parse(
    char const*& it,
    char const* const end
        ) const noexcept ->
    system::result<value_type>
{
    detail::url_impl u(detail::url_impl::from::string);
    u.cs_ = it;

    // scheme
    {
        auto rv = grammar::parse(
            it, end,
            grammar::tuple_rule(
                detail::scheme_rule(),
                grammar::squelch(
                    grammar::delim_rule(':'))));
        if(! rv)
            return rv.error();
        u.apply_scheme(rv->scheme);
    }

    // hier_part
    {
        auto rv = grammar::parse(
            it, end, detail::hier_part_rule);
        if(! rv)
            return rv.error();
        if(rv->has_authority)
            u.apply_authority(rv->authority.u_);
        u.apply_path(
            rv->path,
            rv->segment_count);
    }

    // [ "?" query ]
    {
        auto rv = grammar::parse(
            it, end, detail::query_part_rule);
        if(! rv)
            return rv.error();
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
