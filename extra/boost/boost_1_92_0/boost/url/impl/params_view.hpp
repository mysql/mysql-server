//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_PARAMS_VIEW_HPP
#define BOOST_URL_IMPL_PARAMS_VIEW_HPP

namespace boost {
namespace urls {

inline
params_view::
params_view(
    detail::query_ref const& ref,
    encoding_opts opt) noexcept
    : params_base(ref, opt)
{
}

//------------------------------------------------

inline
params_view::
params_view(
    params_view const& other,
    encoding_opts opt) noexcept
    : params_base(other.ref_, opt)
{
}

inline
params_view::
params_view(
    core::string_view s)
    : params_view(
        parse_query(s).value(
            BOOST_URL_POS),
        {true, false, false})
{
}

inline
params_view::
params_view(
    core::string_view s,
    encoding_opts opt)
    : params_view(
        parse_query(s).value(
            BOOST_URL_POS),
        opt)
{
}

} // urls
} // boost

#endif
