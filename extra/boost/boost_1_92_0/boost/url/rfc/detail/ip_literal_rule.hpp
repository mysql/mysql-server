//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_IP_LITERAL_RULE_HPP
#define BOOST_URL_RFC_DETAIL_IP_LITERAL_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/ipv6_address.hpp>
#include <boost/url/error_types.hpp>
#include <boost/core/detail/string_view.hpp>

namespace boost {
namespace urls {
namespace detail {

/** Rule for IP-literal

    @par BNF
    @code
    IP-literal = "[" ( IPv6address / IPv6addrz / IPvFuture  ) "]"
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-3.2.2"
        >3.2.2. Host (rfc3986)</a>

    @see
        @ref ipv6_address.
*/
struct ip_literal_rule_t
{
    struct value_type
    {
        bool is_ipv6 = false;
        ipv6_address ipv6;
        core::string_view ipvfuture;
    };

    BOOST_URL_CXX20_CONSTEXPR
    auto
    parse(
        char const*& it,
        char const* end
            ) const noexcept ->
        system::result<value_type>;
};

constexpr ip_literal_rule_t ip_literal_rule{};

} // detail
} // urls
} // boost

#include <boost/url/rfc/detail/impl/ip_literal_rule.hpp>

#endif
