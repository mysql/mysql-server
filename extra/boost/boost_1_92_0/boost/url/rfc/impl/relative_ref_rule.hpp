//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_IMPL_RELATIVE_REF_RULE_HPP
#define BOOST_URL_RFC_IMPL_RELATIVE_REF_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/url_view.hpp>
#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/tuple_rule.hpp>
#include <boost/url/grammar/optional_rule.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/rfc/detail/fragment_part_rule.hpp>
#include <boost/url/rfc/detail/query_part_rule.hpp>
#include <boost/url/rfc/detail/relative_part_rule.hpp>

namespace boost {
namespace urls {

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
auto
implementation_defined::relative_ref_rule_t::
parse(
    char const*& it,
    char const* const end
        ) const noexcept ->
    system::result<value_type>
{
    detail::url_impl u(detail::url_impl::from::string);
    u.cs_ = it;

    // relative-part
    {
        auto rv = grammar::parse(
            it, end,
            detail::relative_part_rule);
        if(! rv)
            return rv.error();
        if(rv->has_authority)
            u.apply_authority(rv->authority.u_);
        u.apply_path(
            rv->path, rv->segment_count);
    }

    // [ "?" query ]
    {
        auto rv = grammar::parse(
            it, end, detail::query_part_rule);
        if(! rv)
            return rv.error();
        auto& v = *rv;
        if(v.has_query)
        {
            // map "?" to { {} }
            u.apply_query(
                v.query,
                v.count);
        }
    }

    // [ "#" fragment ]
    {
        auto rv = grammar::parse(
            it, end, detail::fragment_part_rule);
        if(! rv)
            return rv.error();
        if(rv->has_fragment)
            u.apply_frag(rv->fragment);
    }

    return url_view(u);
}

} // urls
} // boost


#endif
