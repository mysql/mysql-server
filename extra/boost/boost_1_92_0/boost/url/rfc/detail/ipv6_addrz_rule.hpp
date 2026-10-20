//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_IPV6ADDRZ_RULE_HPP
#define BOOST_URL_RFC_DETAIL_IPV6ADDRZ_RULE_HPP

#include <boost/url/error_types.hpp>
#include <boost/url/ipv6_address.hpp>
#include <boost/url/pct_string_view.hpp>
#include <boost/core/detail/string_view.hpp>

namespace boost {
namespace urls {
namespace detail {

/** Rule for IPvFuture

    @par BNF
    @code
    IPv6addrz = IPv6address "%25" ZoneID
    ZoneID = 1*( unreserved / pct-encoded )
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-3.2.2"
        >3.2.2. Host (rfc3986)</a>
*/
struct ipv6_addrz_rule_t
{
    struct value_type
    {
        ipv6_address ipv6;
        pct_string_view zone_id;
    };

    BOOST_URL_CXX20_CONSTEXPR
    auto
    parse(
        char const*& it,
        char const* const end
            ) const noexcept ->
        system::result<value_type>;
};

constexpr ipv6_addrz_rule_t ipv6_addrz_rule{};

} // detail
} // urls
} // boost

#include <boost/url/rfc/detail/impl/ipv6_addrz_rule.hpp>

#endif
