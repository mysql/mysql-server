//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_SEGMENTS_VIEW_HPP
#define BOOST_URL_IMPL_SEGMENTS_VIEW_HPP

#include <boost/url/detail/segments_range.hpp>

namespace boost {
namespace urls {

inline
segments_view::
segments_view() noexcept = default;

inline
segments_view::
segments_view(
    detail::path_ref const& ref) noexcept
    : segments_base(ref)
{
}

inline
segments_view::
segments_view(
    core::string_view s)
    : segments_base(
        parse_path(s).value(
            BOOST_URL_POS))
{
}

inline
segments_view::
segments_view(iterator first, iterator last) noexcept
    : segments_base(detail::make_subref(first, last))
{
}

} // urls
} // boost

#endif
