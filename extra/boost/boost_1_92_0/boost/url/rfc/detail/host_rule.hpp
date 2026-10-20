//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_HOST_RULE_HPP
#define BOOST_URL_RFC_DETAIL_HOST_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/host_type.hpp>
#include <boost/url/error_types.hpp>
#include <boost/url/pct_string_view.hpp>
#include <boost/url/ipv4_address.hpp>
#include <boost/url/ipv6_address.hpp>

namespace boost {
namespace urls {
namespace detail {

/** Rule for host

    @par BNF
    @code
    host          = IP-literal / IPv4address / reg-name
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-3.2.2"
        >3.2.2. Host (rfc3986)</a>

    @see
        @ref host_type,
        @ref ipv4_address,
        @ref ipv6_address.
*/
struct host_rule_t
{
    struct value_type
    {
        urls::host_type host_type =
            urls::host_type::none;
        pct_string_view match;
        unsigned char addr[16] = {};
        pct_string_view name;
    };

    BOOST_URL_CXX20_CONSTEXPR
    auto
    parse(
        char const*& it,
        char const* end
            ) const noexcept ->
        system::result<value_type>;
};

constexpr host_rule_t host_rule{};

} // detail
} // urls
} // boost

#include <boost/url/rfc/detail/impl/host_rule.hpp>

#endif
