//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_IMPL_H16_RULE_HPP
#define BOOST_URL_RFC_DETAIL_IMPL_H16_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/hexdig_chars.hpp>

namespace boost {
namespace urls {
namespace detail {

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
auto
h16_rule_t::
parse(
    char const*& it,
    char const* end
        ) const noexcept ->
    system::result<value_type>
{
    // LCOV_EXCL_START
    // h16 is only called from ipv6_address_rule
    // which ensures at least one char is available
    if(it == end)
    {
        BOOST_URL_CONSTEXPR_RETURN_EC(
            grammar::error::invalid);
    }
    // LCOV_EXCL_STOP

    std::uint16_t v;
    for(;;)
    {
        auto d = grammar::hexdig_value(*it);
        if(d < 0)
        {
            // expected HEXDIG
            BOOST_URL_CONSTEXPR_RETURN_EC(
                grammar::error::invalid);
        }
        v = d;
        ++it;
        if(it == end)
            break;
        d = grammar::hexdig_value(*it);
        if(d < 0)
            break;
        v = (16 * v) + d;
        ++it;
        if(it == end)
            break;
        d = grammar::hexdig_value(*it);
        if(d < 0)
            break;
        v = (16 * v) + d;
        ++it;
        if(it == end)
            break;
        d = grammar::hexdig_value(*it);
        if(d < 0)
            break;
        v = (16 * v) + d;
        ++it;
        break;
    }
    return value_type{
        static_cast<
            unsigned char>(v / 256),
        static_cast<
            unsigned char>(v % 256)};
}

} // detail
} // urls
} // boost


#endif
