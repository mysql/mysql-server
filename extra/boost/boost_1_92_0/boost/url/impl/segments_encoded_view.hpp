//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_IMPL_SEGMENTS_ENCODED_VIEW_HPP
#define BOOST_URL_IMPL_SEGMENTS_ENCODED_VIEW_HPP

#include <boost/url/detail/segments_range.hpp>

namespace boost {
namespace urls {

inline
segments_encoded_view::
segments_encoded_view() noexcept = default;

inline
segments_encoded_view::
segments_encoded_view(
    detail::path_ref const& ref) noexcept
    : segments_encoded_base(ref)
{
}

inline
segments_encoded_view::
segments_encoded_view(
    core::string_view s)
    : segments_encoded_view(
        parse_path(s).value(
            BOOST_URL_POS))
{
}

inline
segments_encoded_view::
segments_encoded_view(iterator first, iterator last) noexcept
    : segments_encoded_base(detail::make_subref(first, last))
{
}

inline
segments_encoded_view::
operator
segments_view() const noexcept
{
    return { ref_ };
}

} // urls
} // boost

#endif
