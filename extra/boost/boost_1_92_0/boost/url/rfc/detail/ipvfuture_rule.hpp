//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_IPVFUTURE_RULE_HPP
#define BOOST_URL_RFC_DETAIL_IPVFUTURE_RULE_HPP

#include <boost/url/error_types.hpp>
#include <boost/core/detail/string_view.hpp>

namespace boost {
namespace urls {
namespace detail {

/** Rule for IPvFuture

    @par BNF
    @code
    IPvFuture     = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" )
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-3.2.2"
        >3.2.2. Host (rfc3986)</a>
*/
struct ipvfuture_rule_t
{
    struct value_type
    {
        core::string_view str;
        core::string_view major;
        core::string_view minor;
    };

    BOOST_URL_CXX20_CONSTEXPR
    auto
    parse(
        char const*& it,
        char const* const end
            ) const noexcept ->
        system::result<value_type>;
};

constexpr ipvfuture_rule_t ipvfuture_rule{};

} // detail
} // urls
} // boost

#include <boost/url/rfc/detail/impl/ipvfuture_rule.hpp>

#endif
