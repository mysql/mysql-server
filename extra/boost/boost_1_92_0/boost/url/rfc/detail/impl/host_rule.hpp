//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_IMPL_HOST_RULE_HPP
#define BOOST_URL_RFC_DETAIL_IMPL_HOST_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/rfc/ipv4_address_rule.hpp>
#include <boost/url/rfc/detail/ip_literal_rule.hpp>
#include <boost/url/rfc/detail/reg_name_rule.hpp>
#include <boost/url/grammar/parse.hpp>
#include <cstring>

namespace boost {
namespace urls {
namespace detail {

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
auto
host_rule_t::
parse(
    char const*& it,
    char const* const end
        ) const noexcept ->
    system::result<value_type>
{
    value_type t;

    if(it == end)
    {
        // empty host
        t.host_type =
            urls::host_type::name;
        return t;
    }

    auto const it0 = it;
    if(*it == '[')
    {
        // IP-literal
        auto rv = grammar::parse(
            it, end,
            detail::ip_literal_rule);
        if(! rv)
            return rv.error();
        auto v = *rv;
        if(v.is_ipv6)
        {
            // IPv6address
            auto const b =
                v.ipv6.to_bytes();
            std::memcpy(
                t.addr,
                b.data(),
                b.size());
            t.host_type =
                urls::host_type::ipv6;
            t.match = make_pct_string_view_unsafe(
                it0, it - it0, it - it0);
            return t;
        }

        // IPvFuture
        t.host_type =
            urls::host_type::ipvfuture;
        t.match = make_pct_string_view_unsafe(
            it0, it - it0, it - it0);
        return t;
    }

    // IPv4address
    {
        auto rv = grammar::parse(
            it, end, ipv4_address_rule);
        if( rv )
        {
            auto it02 = it;
            auto rv2 = grammar::parse(
                it, end,
                detail::reg_name_rule);
            if (rv2.has_value() &&
                !rv2->empty())
            {
                auto dn = static_cast<
                    std::size_t>(it02 - it0) +
                    rv2->decoded_size();
                t.name = make_pct_string_view_unsafe(
                    it0, it - it0, dn);
                t.host_type =
                    urls::host_type::name;
                t.match = t.name;
                return t;
            }
            it = it02;
            auto const b =
                rv->to_bytes();
            std::memcpy(
                t.addr,
                b.data(),
                b.size());
            t.host_type =
                urls::host_type::ipv4;
            t.match = make_pct_string_view_unsafe(
                it0, it - it0, it - it0);
            return t;
        }

        it = it0; // rewind
    }

    // reg-name
    {
        auto rv = grammar::parse(
            it, end,
            detail::reg_name_rule);
        if(! rv)
            return rv.error();
        t.name = *rv;
        t.host_type =
            urls::host_type::name;
        t.match = *rv;
        return t;
    }
}

} // detail
} // urls
} // boost


#endif
