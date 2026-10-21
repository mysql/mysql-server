//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_PARSE_HPP
#define BOOST_URL_IMPL_PARSE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/rfc/absolute_uri_rule.hpp>
#include <boost/url/rfc/relative_ref_rule.hpp>
#include <boost/url/rfc/uri_rule.hpp>
#include <boost/url/rfc/uri_reference_rule.hpp>
#include <boost/url/rfc/origin_form_rule.hpp>

namespace boost {
namespace urls {

//------------------------------------------------
//
// parse functions
//
//------------------------------------------------

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
system::result<url_view>
parse_absolute_uri(
    core::string_view s)
{
    return grammar::parse(
        s, absolute_uri_rule);
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
system::result<url_view>
parse_origin_form(
    core::string_view s)
{
    return grammar::parse(
        s, origin_form_rule);
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
system::result<url_view>
parse_relative_ref(
    core::string_view s)
{
    return grammar::parse(
        s, relative_ref_rule);
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
system::result<url_view>
parse_uri(
    core::string_view s)
{
    return grammar::parse(
        s, uri_rule);
}

BOOST_URL_CXX20_CONSTEXPR_OR_INLINE
system::result<url_view>
parse_uri_reference(
    core::string_view s)
{
    return grammar::parse(
        s, uri_reference_rule);
}

} // urls
} // boost

#endif
