//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_PORT_RULE_HPP
#define BOOST_URL_RFC_DETAIL_PORT_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/error_types.hpp>
#include <boost/core/detail/string_view.hpp>
#include <cstdint>

namespace boost {
namespace urls {
namespace detail {

/** Rule for port

    @par BNF
    @code
    port          = *DIGIT
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-3.2.2"
        >3.2.2. Host (rfc3986)</a>

    @see
        @ref port_part_rule.
*/
struct port_rule
{
    struct value_type
    {
        core::string_view str;
        std::uint16_t number = 0;
        bool has_number = false;
    };

    BOOST_URL_CXX20_CONSTEXPR
    system::result<value_type>
    parse(
        char const*& it,
        char const* end) const noexcept;
};

//------------------------------------------------

/** Rule for port-part

    @par BNF
    @code
    port-part       = [ ":" port ]

    port            = *DIGIT
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-3.2.2"
        >3.2.2. Host (rfc3986)</a>

    @see
        @ref port_rule.
*/
struct port_part_rule_t
{
    struct value_type
    {
        bool has_port = false;
        core::string_view port;
        bool has_number = false;
        std::uint16_t port_number = 0;
    };

    BOOST_URL_CXX20_CONSTEXPR
    auto
    parse(
        char const*& it,
        char const* end
            ) const noexcept ->
        system::result<value_type>;
};

constexpr port_part_rule_t port_part_rule{};

} // detail
} // urls
} // boost

#include <boost/url/rfc/detail/impl/port_rule.hpp>

#endif
