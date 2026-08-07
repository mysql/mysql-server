//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_DETAIL_IMPL_IPVFUTURE_RULE_HPP
#define BOOST_URL_RFC_DETAIL_IMPL_IPVFUTURE_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/rfc/detail/charsets.hpp>
#include <boost/url/grammar/charset.hpp>
#include <boost/url/grammar/delim_rule.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/hexdig_chars.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/token_rule.hpp>
#include <boost/url/grammar/tuple_rule.hpp>

namespace boost {
namespace urls {
namespace detail {

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
auto
ipvfuture_rule_t::
parse(
    char const*& it,
    char const* const end
        ) const noexcept ->
    system::result<value_type>
{
    constexpr auto
        minor_chars =
            unreserved_chars +
            sub_delim_chars + ':';
    auto const it0 = it;
    auto rv = grammar::parse(
        it, end,
        grammar::tuple_rule(
            grammar::delim_rule('v'),
            grammar::token_rule(
                grammar::hexdig_chars),
            grammar::delim_rule('.'),
            grammar::token_rule(minor_chars)));
    if(! rv)
        return rv.error();
    value_type t;
    t.major = std::get<0>(*rv);
    t.minor = std::get<1>(*rv);
    // token_rule guarantees non-empty tokens,
    // so major/minor are always non-empty here.
    BOOST_ASSERT(!t.major.empty());
    BOOST_ASSERT(!t.minor.empty());
    t.str = core::string_view(
        it0, it - it0);
    return t;
}

} // detail
} // urls
} // boost


#endif
