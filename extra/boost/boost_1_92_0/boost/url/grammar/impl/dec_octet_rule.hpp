//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_GRAMMAR_IMPL_DEC_OCTET_RULE_HPP
#define BOOST_URL_GRAMMAR_IMPL_DEC_OCTET_RULE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/grammar/charset.hpp>
#include <boost/url/grammar/digit_chars.hpp>
#include <boost/url/grammar/error.hpp>

namespace boost {
namespace urls {
namespace grammar {
namespace implementation_defined {

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
auto
dec_octet_rule_t::
parse(
    char const*& it,
    char const* const end
        ) const noexcept ->
    system::result<value_type>
{
    if(it == end)
    {
        // end
        BOOST_URL_CONSTEXPR_RETURN_EC(
            error::mismatch);
    }
    if(! digit_chars(*it))
    {
        // expected DIGIT
        BOOST_URL_CONSTEXPR_RETURN_EC(
            error::mismatch);
    }
    unsigned v = *it - '0';
    ++it;
    if( it == end ||
        ! digit_chars(*it))
    {
        return static_cast<
            value_type>(v);
    }
    if(v == 0)
    {
        // leading '0'
        BOOST_URL_CONSTEXPR_RETURN_EC(
            error::invalid);
    }
    v = (10 * v) + *it - '0';
    ++it;
    if( it == end ||
        ! digit_chars(*it))
    {
        return static_cast<
            value_type>(v);
    }
    if(v > 25)
    {
        // integer overflow
        BOOST_URL_CONSTEXPR_RETURN_EC(
            error::invalid);
    }
    v = (10 * v) + *it - '0';
    if(v > 255)
    {
        // integer overflow
        BOOST_URL_CONSTEXPR_RETURN_EC(
            error::invalid);
    }
    ++it;
    if( it != end &&
        digit_chars(*it))
    {
        // integer overflow
        BOOST_URL_CONSTEXPR_RETURN_EC(
            error::invalid);
    }
    return static_cast<
        value_type>(v);
}

} // implementation_defined
} // grammar
} // urls
} // boost

#endif
