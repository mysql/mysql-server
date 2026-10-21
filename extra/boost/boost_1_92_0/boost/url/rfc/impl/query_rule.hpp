//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_IMPL_QUERY_RULE_HPP
#define BOOST_URL_RFC_IMPL_QUERY_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/rfc/detail/charsets.hpp>
#include <boost/url/error.hpp>
#include <boost/url/grammar/hexdig_chars.hpp>

namespace boost {
namespace urls {

inline
auto
implementation_defined::query_rule_t::
parse(
    char const*& it,
    char const* end
        ) const noexcept ->
    system::result<value_type>
{
    if(it == end)
    {
        // empty string = 1 param
        core::string_view str(it, 0);
        return params_encoded_view(
            detail::query_ref(str, 0, 1));
    }
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
            {
                // missing valid HEXDIG
                break;
            }
            it += 3;
            dn += 2;
            continue;
        }
        // got reserved character
        break;
    }
    std::size_t const n(it - it0);
    core::string_view str(it0, n);
    return params_encoded_view(
        detail::query_ref(
            str, n - dn, nparam));
}

} // urls
} // boost


#endif
