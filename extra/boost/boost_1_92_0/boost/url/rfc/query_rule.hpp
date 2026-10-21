//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_QUERY_RULE_HPP
#define BOOST_URL_RFC_QUERY_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/error_types.hpp>
#include <boost/url/params_encoded_view.hpp>
#include <cstddef>

namespace boost {
namespace urls {
namespace implementation_defined {
struct query_rule_t
{
    using value_type = params_encoded_view;

    system::result<value_type>
    parse(
        char const*& it,
        char const* end
            ) const noexcept;
};
} // implementation_defined

/** Rule for a query string

    @par Value Type
    @code
    using value_type = params_encoded_view;
    @endcode

    @par Example
    Rules are used with the function @ref grammar::parse.
    @code
    system::result< params_encoded_view > rv = grammar::parse( "format=web&id=42&compact", query_rule );
    @endcode

    @par BNF
    @code
    query           = *( pchar / "/" / "?" )

    query-params    = [ query-param ] *( "&" query-param )
    query-param     = key [ "=" value ]
    key             = *qpchar
    value           = *( qpchar / "=" )
    qpchar          = unreserved
                    / pct-encoded
                    / "!" / "$" / "'" / "(" / ")"
                    / "*" / "+" / "," / ";"
                    / ":" / "@" / "/" / "?"
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-3.4"
        >3.4. Query (rfc3986)</a>

    @see
        @ref grammar::parse,
        @ref params_encoded_view.
*/
constexpr implementation_defined::query_rule_t query_rule{};

} // urls
} // boost

#include <boost/url/rfc/impl/query_rule.hpp>

#endif