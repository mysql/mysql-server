//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_QUERY_PART_RULE_HPP
#define BOOST_URL_RFC_DETAIL_QUERY_PART_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/error_types.hpp>
#include <boost/url/pct_string_view.hpp>
#include <boost/url/rfc/detail/charsets.hpp>
#include <boost/url/grammar/hexdig_chars.hpp>
#include <cstdlib>

namespace boost {
namespace urls {
namespace detail {

/** Rule for query-part

    @par BNF
    @code
    query-part    = [ "?" query ]
    @endcode
*/
struct query_part_rule_t
{
    struct value_type
    {
        pct_string_view query;
        std::size_t count = 0;
        bool has_query = false;
    };

    BOOST_URL_CXX14_CONSTEXPR
    auto
    parse(
        char const*& it,
        char const* end
            ) const noexcept ->
        system::result<value_type>
    {
        if( it == end ||
            *it != '?')
            return {};
        ++it;
        auto const it0 = it;
        std::size_t dn = 0;
        std::size_t nparam = 1;
        while(it != end)
        {
            if(*it == '&')
            {
                ++nparam;
                ++it;
                continue;
            }
            if(detail::query_chars(*it))
            {
                ++it;
                continue;
            }
            if(*it == '%')
            {
                if(end - it < 3 ||
                    (!grammar::hexdig_chars(it[1]) ||
                     !grammar::hexdig_chars(it[2])))
                    break;
                it += 3;
                dn += 2;
                continue;
            }
            break;
        }
        std::size_t const n(it - it0);
        value_type t;
        t.query = make_pct_string_view_unsafe(
            it0, n, n - dn);
        t.count = nparam;
        t.has_query = true;
        return t;
    }
};

constexpr query_part_rule_t query_part_rule{};

} // detail
} // urls
} // boost

#endif
