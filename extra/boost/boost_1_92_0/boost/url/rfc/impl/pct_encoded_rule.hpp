//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_IMPL_PCT_ENCODED_RULE_HPP
#define BOOST_URL_RFC_IMPL_PCT_ENCODED_RULE_HPP

#include <boost/url/grammar/charset.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/hexdig_chars.hpp>

namespace boost {
namespace urls {

namespace detail {

template<class CharSet>
BOOST_URL_CXX20_CONSTEXPR
auto
parse_encoded(
    char const*& it,
    char const* end,
    CharSet const& cs) noexcept ->
        system::result<pct_string_view>
{
    auto const start = it;
    std::size_t n = 0;
    for(;;)
    {
        auto it0 = it;
        it = grammar::find_if_not(
            it0, end, cs);
        n += it - it0;
        if(it == end || *it != '%')
            break;
        bool at_end = false;
        for(;;)
        {
            ++it;
            if(it == end)
            {
                // expected HEXDIG
                BOOST_URL_CONSTEXPR_RETURN_EC(
                    grammar::error::invalid);
            }
            auto r = grammar::hexdig_value(*it);
            if(r < 0)
            {
                // expected HEXDIG
                BOOST_URL_CONSTEXPR_RETURN_EC(
                    grammar::error::invalid);
            }
            ++it;
            if(it == end)
            {
                // expected HEXDIG
                BOOST_URL_CONSTEXPR_RETURN_EC(
                    grammar::error::invalid);
            }
            r = grammar::hexdig_value(*it);
            if(r < 0)
            {
                // expected HEXDIG
                BOOST_URL_CONSTEXPR_RETURN_EC(
                    grammar::error::invalid);
            }
            ++n;
            ++it;
            if(it == end)
            {
                at_end = true;
                break;
            }
            if(*it != '%')
                break;
        }
        if(at_end)
            break;
    }
    return make_pct_string_view_unsafe(
        start, it - start, n);
}

} // detail

//------------------------------------------------

template<class CharSet>
BOOST_URL_CXX20_CONSTEXPR
auto
implementation_defined::pct_encoded_rule_t<CharSet>::
parse(
    char const*& it,
    char const* end) const noexcept ->
        system::result<value_type>
{
    return detail::parse_encoded(
        it, end, cs_);
}

} // urls
} // boost

#endif
