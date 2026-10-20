//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_URL_VIEW_HPP
#define BOOST_URL_IMPL_URL_VIEW_HPP

#include <boost/url/parse.hpp>
#include <boost/url/detail/over_allocator.hpp>
#include <cstring>
#include <memory>

namespace boost {
namespace urls {

//------------------------------------------------
//
// url_view
//
//------------------------------------------------

inline
url_view::
url_view(core::string_view s)
    : url_view(parse_uri_reference(s
        ).value(BOOST_URL_POS))
{
}

//------------------------------------------------
//
// url_view_base::persist
//
//------------------------------------------------

struct url_view_base::shared_impl
    : url_view
{
    virtual
    ~shared_impl()
    {
    }

    shared_impl(
        url_view const& u) noexcept
        : url_view(u)
    {
        impl_.cs_ = reinterpret_cast<
            char const*>(this + 1);
    }
};

inline
std::shared_ptr<url_view const>
url_view_base::
persist() const
{
    using T = shared_impl;
    using Alloc = std::allocator<char>;
    Alloc a;
    auto p = std::allocate_shared<T>(
        detail::over_allocator<T, Alloc>(
            size(), a), url_view(impl()));
    if(size())
        std::memcpy(
            reinterpret_cast<char*>(
                p.get() + 1), data(), size());
    return p;
}

} // urls
} // boost

#endif
