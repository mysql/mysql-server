//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_SEGMENTS_RANGE_HPP
#define BOOST_URL_DETAIL_SEGMENTS_RANGE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/detail/url_impl.hpp>
#include <boost/url/segments_base.hpp>
#include <boost/url/segments_encoded_base.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/assert.hpp>

namespace boost {
namespace urls {
namespace detail {

struct segments_iter_access
{
    static
    segments_iter_impl const&
    impl(segments_base::iterator const& it) noexcept
    {
        return it.it_;
    }

    static
    segments_iter_impl const&
    impl(segments_encoded_base::iterator const& it) noexcept
    {
        return it.it_;
    }
};

inline
path_ref
make_subref_from_impls(
    segments_iter_impl const& first,
    segments_iter_impl const& last) noexcept
{
    BOOST_ASSERT(first.ref.alias_of(last.ref));
    path_ref const& ref = first.ref;

    std::size_t const i0 = first.index;
    std::size_t const i1 = last.index;
    BOOST_ASSERT(i0 <= i1);
    std::size_t const nseg = i1 - i0;

    bool const absolute = ref.buffer().starts_with('/');

    // Empty range
    if (nseg == 0)
    {
        std::size_t off0;
        if (i0 == 0)
        {
            // [begin, begin): don't include the leading '/'
            // for absolute, start right after the leading '/';
            if (absolute)
            {
                off0 = 1;
            }
            // for relative, start at the first segment character.
            else
            {
                off0 = first.pos;
            }
        }
        else
        {
            // [it, it) in the middle:
            // skip the separator before segment i0
            off0 = first.pos + 1;
        }

        core::string_view const sub(ref.data() + off0, 0);
        return {sub, 0, 0};
    }

    // General case: non-empty range
    // Start offset
    std::size_t off0;
    if (i0 == 0)
    {
        if (absolute)
        {
            // include leading '/'
            off0 = 0;
        }
        else
        {
            // relative: start at first segment
            off0 = first.pos;
        }
    }
    else
    {
        // include the separator preceding segment i0
        off0 = first.pos;
    }

    // End offset
    std::size_t off1;
    if(i1 == ref.nseg())
    {
        off1 = ref.size();
    }
    else
    {
        // stop before the slash preceding i1
        off1 = last.pos;
    }

    BOOST_ASSERT(off1 >= off0);
    core::string_view const sub(ref.data() + off0, off1 - off0);

    // decoded sizes reuse iterator bookkeeping instead of rescanning
    std::size_t start_dn = (i0 == 0) ? 0 : first.decoded_prefix_size();
    std::size_t const end_dn = last.decoded_prefix_size(); // already excludes segment at `last`
    BOOST_ASSERT(end_dn >= start_dn);
    std::size_t const dn_sum = end_dn - start_dn;

    return {sub, dn_sum, nseg};
}

template<class Iter>
inline
path_ref
make_subref(Iter const& first, Iter const& last) noexcept
{
    auto const& f = segments_iter_access::impl(first);
    auto const& l = segments_iter_access::impl(last);
    return make_subref_from_impls(f, l);
}

} // detail
} // urls
} // boost

#endif
