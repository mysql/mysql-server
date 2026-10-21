//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_H16_RULE_HPP
#define BOOST_URL_RFC_DETAIL_H16_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/error_types.hpp>
#include <cstdint>

namespace boost {
namespace urls {
namespace detail {

/** Rule for h16

    This parses a sixteen bit unsigned
    hexadecimal number.

    @par BNF
    @code
    h16         = 1*4HEXDIG
                ; 16 bits of address represented in hexadecimal
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-3.2.2"
        >3.2.2.  Host (rfc3986)</a>
*/
struct h16_rule_t
{
    struct value_type
    {
        std::uint8_t hi;
        std::uint8_t lo;
    };

    BOOST_URL_CXX20_CONSTEXPR
    auto
    parse(
        char const*& it,
        char const* end
            ) const noexcept ->
        system::result<value_type>;
};

constexpr h16_rule_t h16_rule{};

} // detail
} // urls
} // boost

#include <boost/url/rfc/detail/impl/h16_rule.hpp>

#endif
