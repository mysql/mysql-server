//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_PARAMS_ENCODED_VIEW_HPP
#define BOOST_URL_IMPL_PARAMS_ENCODED_VIEW_HPP

namespace boost {
namespace urls {

inline
params_encoded_view::
params_encoded_view(
    detail::query_ref const& ref) noexcept
    : params_encoded_base(ref)
{
}

inline
params_encoded_view::
params_encoded_view(
    core::string_view s)
    : params_encoded_view(
        parse_query(s).value(
            BOOST_URL_POS))
{
}

inline
params_encoded_view::
operator
params_view() const noexcept
{
    return { ref_, encoding_opts{ true, false, false} };
}

} // urls
} // boost

#endif
