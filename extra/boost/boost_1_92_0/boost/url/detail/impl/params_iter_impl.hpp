//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_IMPL_PARAMS_ITER_IMPL_HPP
#define BOOST_URL_DETAIL_IMPL_PARAMS_ITER_IMPL_HPP

#include <boost/assert.hpp>

namespace boost {
namespace urls {
namespace detail {

/*  index   zero-based index of param
    pos     offset from start   0 = '?'
    nk      size of key         with '?' or '&'
    nv      size of value       with '='
    dk      decoded key size    no '?' or '&'
    dv      decoded value size  no '='
*/
inline
params_iter_impl::
params_iter_impl(
    query_ref const& ref_) noexcept
    : ref(ref_)
    , index(0)
    , pos(0)
{
    if(index < ref_.nparam())
        setup();
}

inline
params_iter_impl::
params_iter_impl(
    query_ref const& ref_,
    int) noexcept
    : ref(ref_)
    , index(ref_.nparam())
    , pos(ref_.size())
{
}

inline
params_iter_impl::
params_iter_impl(
    query_ref const& ref_,
    std::size_t pos_,
    std::size_t index_) noexcept
    : ref(ref_)
    , index(index_)
    , pos(pos_)
{
    BOOST_ASSERT(
        pos <= ref.size());
    if(index < ref_.nparam())
        setup();
}

// set up state for key/value at pos
inline
void
params_iter_impl::
setup() noexcept
{
    dk = 1;
    dv = 0;
    auto const end = ref.end();
    BOOST_ASSERT(pos != ref.size());
    auto p0 = ref.begin() + pos;
    auto p = p0;
    // key
    for(;;)
    {
        if( p == end ||
            *p == '&')
        {
            // no value
            nk = 1 + p - p0;
            dk = nk - dk;
            nv = 0;
            return;
        }
        if(*p == '=')
            break;
        if(*p == '%')
        {
            BOOST_ASSERT(
                end - p >= 3);
            dk += 2;
            p += 2;
        }
        ++p;
    }
    nk = 1 + p - p0;
    dk = nk - dk;
    p0 = p;

    // value
    for(;;)
    {
        ++p;
        if( p == end ||
            *p == '&')
            break;
        if(*p == '%')
        {
            BOOST_ASSERT(
                end - p >= 3);
            dv += 2;
            p += 2;
        }
    }
    nv = p - p0;
    dv = nv - dv - 1;
}

inline
void
params_iter_impl::
increment() noexcept
{
    BOOST_ASSERT(
        index < ref.nparam());
    pos += nk + nv;
    ++index;
    if(index < ref.nparam())
        setup();
}

inline
void
params_iter_impl::
decrement() noexcept
{
    BOOST_ASSERT(index > 0);
    --index;
    dk = 1; // for '&' or '?'
    dv = 1; // for '='
    auto const begin = ref.begin();
    BOOST_ASSERT(pos > 0);
    auto p1 = begin + (pos - 1);
    auto p = p1;
    // find key or '='
    for(;;)
    {
        if(p == begin)
        {
            // key
            nk = 1 + p1 - p; // with '?'
            dk = nk - dv;
            nv = 0;
            dv = 0;
            pos -= nk;
            return;
        }
        else if(*--p == '&')
        {
            // key
            nk = p1 - p; // with '&'
            dk = nk - dv;
            nv = 0;
            dv = 0;
            pos -= nk;
            return;
        }
        if(*p == '=')
        {
            // value
            nv = p1 - p; // with '='
            break;
        }
        if(*p == '%')
            dv += 2;
    }
    // find key and value
    for(;;)
    {
        if(p == begin)
        {
            // key and value
            nk = 1 + p1 - p - nv; // with '?'
            dk = nk - dk;
            dv = nv - dv;
            pos -= nk + nv;
            return;
        }
        if(*--p == '&')
        {
            // key and value
            nk = p1 - p - nv; // with '&'
            dk = nk - dk;
            dv = nv - dv;
            pos -= nk + nv;
            return;
        }
        if(*p == '=')
        {
            // value
            nv = p1 - p; // with '='
            dv += dk - 1;
            dk = 1;
        }
        else if(*p == '%')
        {
            dk += 2;
        }
    }
}

inline
param_pct_view
params_iter_impl::
dereference() const noexcept
{
    BOOST_ASSERT(index < ref.nparam());
    BOOST_ASSERT(pos < ref.size());
    auto const p = ref.begin() + pos;
    if(nv)
        return {
            make_pct_string_view_unsafe(
                p, nk - 1, dk),
            make_pct_string_view_unsafe(
                p + nk, nv - 1, dv)};
    return {
        make_pct_string_view_unsafe(
            p, nk - 1, dk),
        no_value};
}

inline
pct_string_view
params_iter_impl::
key() const noexcept
{
    BOOST_ASSERT(index < ref.nparam());
    BOOST_ASSERT(pos < ref.size());
    auto const p = ref.begin() + pos;
    return make_pct_string_view_unsafe(
        p, nk - 1, dk);
}

} // detail
} // urls
} // boost

#endif
