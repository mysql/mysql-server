//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_IMPL_SCHEME_RULE_HPP
#define BOOST_URL_RFC_DETAIL_IMPL_SCHEME_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/grammar/alpha_chars.hpp>
#include <boost/url/grammar/charset.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/lut_chars.hpp>
#include <boost/url/grammar/parse.hpp>

namespace boost {
namespace urls {
namespace detail {

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
auto
scheme_rule::
parse(
    char const*& it,
    char const* end) const noexcept ->
        system::result<value_type>
{
    auto const start = it;
    if(it == end)
    {
        // end
        BOOST_URL_CONSTEXPR_RETURN_EC(
            grammar::error::mismatch);
    }
    if(! grammar::alpha_chars(*it))
    {
        // expected alpha
        BOOST_URL_CONSTEXPR_RETURN_EC(
            grammar::error::mismatch);
    }

    constexpr
    grammar::lut_chars scheme_chars(
        "0123456789" "+-."
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz");
    it = grammar::find_if_not(
        it + 1, end, scheme_chars);
    value_type t;
    t.scheme = core::string_view(
        start, it - start);
    t.scheme_id = string_to_scheme(
        t.scheme);
    return t;
}

} // detail
} // urls
} // boost


#endif
