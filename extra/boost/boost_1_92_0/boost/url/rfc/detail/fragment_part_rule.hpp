//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_FRAGMENT_PART_RULE_HPP
#define BOOST_URL_RFC_DETAIL_FRAGMENT_PART_RULE_HPP

#include <boost/url/rfc/pct_encoded_rule.hpp>
#include <boost/url/rfc/detail/charsets.hpp>
#include <boost/url/grammar/parse.hpp>

namespace boost {
namespace urls {
namespace detail {

/** Rule for fragment-part

    @par BNF
    @code
    fragment-part   = [ "#" fragment ]

    fragment        = *( pchar / "/" / "?" )
    @endcode

    @par Specification
    @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-3.5"
        >3.5. Fragment (rfc3986)</a>
*/
struct fragment_part_rule_t
{
    struct value_type
    {
        pct_string_view fragment;
        bool has_fragment = false;
    };

    BOOST_URL_CXX14_CONSTEXPR
    system::result<value_type>
    parse(
        char const*& it,
        char const* end
            ) const noexcept
    {
        if( it == end ||
            *it != '#')
            return {};
        ++it;
        auto rv = grammar::parse(
            it, end, pct_encoded_rule(
                fragment_chars));
        if(! rv)
            return rv.error();
        value_type t;
        t.fragment = *rv;
        t.has_fragment = true;
        return t;
    }
};
constexpr fragment_part_rule_t fragment_part_rule{};

} // detail
} // urls
} // boost

#endif
