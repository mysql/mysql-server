//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_IMPL_SEGMENTS_ITER_IMPL_HPP
#define BOOST_URL_DETAIL_IMPL_SEGMENTS_ITER_IMPL_HPP

#include <boost/url/detail/decode.hpp>
#include <boost/url/detail/path.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace urls {
namespace detail {

// begin
inline
segments_iter_impl::
segments_iter_impl(
    detail::path_ref const& ref_) noexcept
    : ref(ref_)
{
    pos = path_prefix(ref.buffer());
    // begin() starts after any malleable prefix but remembers decoded chars skipped
    decoded_prefix = pos;
    update();
}

// end
inline
segments_iter_impl::
segments_iter_impl(
    detail::path_ref const& ref_,
    int) noexcept
    : ref(ref_)
    , pos(ref.size())
    , next(ref.size())
    , index(ref.nseg())
{
    // end() carries the total decoded length for O(1) range math
    decoded_prefix = ref.decoded_size();
}

inline
segments_iter_impl::
segments_iter_impl(
    url_impl const& u_,
    std::size_t pos_,
    std::size_t index_) noexcept
    : ref(u_)
    , pos(pos_)
    , index(index_)
{
    auto const total = ref.nseg();
    if(index >= total)
    {
        pos = ref.size();
        next = ref.size();
        decoded_prefix = ref.decoded_size();
        // iterator equal to end: nothing to decode
        dn = 0;
        return;
    }

    if(index == 0)
    {
        pos = path_prefix(ref.buffer());
        // first segment inherits the prefix size (including leading '/')
        decoded_prefix = pos;
        update();
        return;
    }

    BOOST_ASSERT(pos <= ref.size());
    // compute decoded prefix by scanning once up to the encoded offset
    decoded_prefix = detail::decode_bytes_unsafe(
        core::string_view(ref.data(), pos));
    if(pos != ref.size())
    {
        BOOST_ASSERT(
            ref.data()[pos] == '/');
        ++pos; // skip '/'
        update();
        --pos;
        return;
    }

    update();
}

inline
void
segments_iter_impl::
update() noexcept
{
    BOOST_ASSERT(
        pos == 0 ||
        ref.data()[pos - 1] == '/');
    auto const end = ref.end();
    char const* const p0 =
        ref.data() + pos;
    dn = 0;
    auto p = p0;
    while(p != end)
    {
        if(*p == '/')
            break;
        if(*p != '%')
        {
            ++p;
            continue;
        }
        p += 3;
        dn += 2;
    }
    next = p - ref.data();
    dn = p - p0 - dn;
    s_ = make_pct_string_view_unsafe(
        p0, p - p0, dn);
}

inline
void
segments_iter_impl::
increment() noexcept
{
    BOOST_ASSERT(
        index != ref.nseg());
    auto const old_index = index;
    auto const old_dn = dn;
    // add decoded length of previous segment
    decoded_prefix += old_dn;
    if(old_index > 0)
        // account for the '/' separator we just crossed
        ++decoded_prefix;
    ++index;
    pos = next;
    if(index == ref.nseg())
        return;
    // "/" segment
    auto const end = ref.end();
    auto p = ref.data() + pos;
    BOOST_ASSERT(p != end);
    BOOST_ASSERT(*p == '/');
    dn = 0;
    ++p; // skip '/'
    auto const p0 = p;
    while(p != end)
    {
        if(*p == '/')
            break;
        if(*p != '%')
        {
            ++p;
            continue;
        }
        p += 3;
        dn += 2;
    }
    next = p - ref.data();
    dn = p - p0 - dn;
    s_ = make_pct_string_view_unsafe(
        p0, p - p0, dn);
}

inline
void
segments_iter_impl::
decrement() noexcept
{
    BOOST_ASSERT(index != 0);
    auto const current_dn = dn;
    auto const current_index = index;
    // remove the decoded length of the segment we're leaving
    decoded_prefix -= current_dn;
    if(current_index > 0 && decoded_prefix > 0)
        // drop the '/' separator when stepping left of it
        --decoded_prefix;
    --index;
    if(index == 0)
    {
        pos = path_prefix(ref.buffer());
        decoded_prefix = pos;
        update();
        BOOST_ASSERT(! s_.ends_with('/'));
        return;
    }
    // scan backwards to find the '/' before
    // the previous segment
    auto const begin = ref.data() +
        path_prefix(ref.buffer());
    auto p = ref.data() + pos;
    BOOST_ASSERT(p != begin);
    while(p != begin)
    {
        --p;
        if(*p == '/')
            break;
    }
    pos = p - ref.data() + 1;
    update();
    --pos;
}

} // detail
} // urls
} // boost

#endif
