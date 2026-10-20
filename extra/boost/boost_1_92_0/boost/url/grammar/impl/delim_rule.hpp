//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_GRAMMAR_IMPL_DELIM_RULE_HPP
#define BOOST_URL_GRAMMAR_IMPL_DELIM_RULE_HPP

#include <boost/url/detail/config.hpp>

namespace boost {
namespace urls {
namespace grammar {

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
auto
implementation_defined::ch_delim_rule::
parse(
    char const*& it,
    char const* end) const noexcept ->
        system::result<value_type>
{
    if(it == end)
    {
        // end
        BOOST_URL_CONSTEXPR_RETURN_EC(
            error::need_more);
    }
    if(*it != ch_)
    {
        // wrong character
        BOOST_URL_CONSTEXPR_RETURN_EC(
            error::mismatch);
    }
    return core::string_view{
        it++, 1 };
}

} // grammar
} // urls
} // boost

#endif
