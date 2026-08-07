//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_DECODE_HPP
#define BOOST_URL_IMPL_DECODE_HPP

#include <boost/assert.hpp>
#include <boost/url/detail/decode.hpp>
#include <boost/url/detail/string_view.hpp>
#include <boost/url/pct_string_view.hpp>
#include <utility>

namespace boost {
namespace urls {

inline
system::result<std::size_t>
decoded_size(core::string_view s) noexcept
{
    auto const rv = make_pct_string_view(s);
    if(! rv)
        return rv.error();
    return rv->decoded_size();
}

inline
system::result<std::size_t>
decode(
    char* dest,
    std::size_t size,
    core::string_view s,
    encoding_opts opt) noexcept
{
    auto const rv = make_pct_string_view(s);
    if(! rv)
        return rv.error();
    return detail::decode_unsafe(
        dest,
        dest + size,
        detail::to_sv(rv.value()),
        opt);
}

template<
    BOOST_URL_CONSTRAINT(string_token::StringToken) StringToken>
system::result<typename StringToken::result_type>
decode(
    core::string_view s,
    encoding_opts opt,
    StringToken&& token)
{
    static_assert(
        string_token::is_token<
            StringToken>::value,
        "Type requirements not met");

    auto const rv = make_pct_string_view(s);
    if(! rv)
        return rv.error();

    auto const n = rv->decoded_size();
    auto p = token.prepare(n);
    // Some tokens might hand back a null/invalid buffer for n == 0, so skip the
    // decode call entirely in that case to avoid touching unspecified memory.
    if(n > 0)
        detail::decode_unsafe(
            p,
            p + n,
            detail::to_sv(rv.value()),
            opt);
    return token.result();
}

} // urls
} // boost

#endif
