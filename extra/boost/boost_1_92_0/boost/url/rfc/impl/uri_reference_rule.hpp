//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_RFC_IMPL_URI_REFERENCE_RULE_HPP
#define BOOST_URL_RFC_IMPL_URI_REFERENCE_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/url_view.hpp>
#include <boost/url/rfc/uri_rule.hpp>
#include <boost/url/rfc/relative_ref_rule.hpp>
#include <boost/url/grammar/error.hpp>
#include <boost/url/grammar/parse.hpp>

namespace boost {
namespace urls {

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
auto
implementation_defined::uri_reference_rule_t::
parse(
    char const*& it,
    char const* const end
        ) const noexcept ->
    system::result<value_type>
{
    // Try URI first, then relative-ref.
    // Use separate variables for each parse attempt
    // to avoid variant2 cross-alternative reassignment,
    // which uses placement new (not constexpr before C++26).
    auto const it0 = it;
    auto rv1 = grammar::parse(
        it, end, uri_rule);
    if(rv1)
        return *rv1;
    it = it0;
    auto rv2 = grammar::parse(
        it, end, relative_ref_rule);
    if(rv2)
        return *rv2;
    BOOST_URL_CONSTEXPR_RETURN_EC(
        grammar::error::mismatch);
}

} // urls
} // boost


#endif
