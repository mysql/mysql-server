// Boost.Geometry Index
//
// n-dimensional content (hypervolume) - 2d area, 3d volume, ...
//
// Copyright (c) 2011-2014 Adam Wulkiewicz, Lodz, Poland.
//
// This file was modified by Oracle on 2020-2023.
// Modifications copyright (c) 2020-2023 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle
//
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_INDEX_DETAIL_ALGORITHMS_CONTENT_HPP
#define BOOST_GEOMETRY_INDEX_DETAIL_ALGORITHMS_CONTENT_HPP

#include <algorithm>

#include <boost/config.hpp>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/coordinate_dimension.hpp>
#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/core/static_assert.hpp>
#include <boost/geometry/core/tag.hpp>
#include <boost/geometry/core/tags.hpp>
#include <boost/geometry/util/select_most_precise.hpp>

namespace boost { namespace geometry { namespace index { namespace detail {

#if defined(BOOST_GCC)
// The following pragmas set fp-contract=off for GCC, to prevent surprising and
// subtle numerical behaviour changes as a result of cross-statement/function
// FMA contractions which can result in e.g. content(b1) - content(b2) != 0 
// (or < 0) for content(b1) == content(b2) (or >=), due to the fp-contract=fast
// default from at least GCC 4.4 to present GCC 15.2. This avoids Github issue
// #1452. Safe to remove after numerically robust handling of degenerate cases
// is ensured at all index::detail::content(box) call sites.
#pragma GCC push_options
#pragma GCC optimize ("fp-contract=off")
#endif

template <typename Indexable>
struct default_content_result
{
    using type = typename select_most_precise
        <
            typename coordinate_type<Indexable>::type,
            double
        >::type;
};

namespace dispatch {

template <typename Box,
          std::size_t CurrentDimension = dimension<Box>::value>
struct content_box
{
    static_assert(0 < CurrentDimension, "Specialization for positive index.");

    static inline typename detail::default_content_result<Box>::type apply(Box const& b)
    {
        return content_box<Box, CurrentDimension - 1>::apply(b) *
            ( get<max_corner, CurrentDimension - 1>(b) - get<min_corner, CurrentDimension - 1>(b) );
    }
};

template <typename Box>
struct content_box<Box, 1>
{
    static inline typename detail::default_content_result<Box>::type apply(Box const& b)
    {
        return get<max_corner, 0>(b) - get<min_corner, 0>(b);
    }
};

template <typename Indexable, typename Tag>
struct content
{
    BOOST_GEOMETRY_STATIC_ASSERT_FALSE(
        "Not implemented for this Indexable and Tag.",
        Indexable, Tag);
};

template <typename Indexable>
struct content<Indexable, point_tag>
{
    static typename detail::default_content_result<Indexable>::type apply(Indexable const&)
    {
        return 0;
    }
};

template <typename Indexable>
struct content<Indexable, box_tag>
{
    static typename default_content_result<Indexable>::type apply(Indexable const& b)
    {
        return dispatch::content_box<Indexable>::apply(b);
    }
};

} // namespace dispatch

template <typename Indexable>
typename default_content_result<Indexable>::type content(Indexable const& b)
{
    return dispatch::content
            <
                Indexable,
                tag_t<Indexable>
            >::apply(b);
}

// Returns the content increase when expanding 'original' to 'expanded'.
// Precondition: 'original' is covered by 'expanded'. Under this precondition
// the result is mathematically non-negative, so clamping to 0 is valid and
// guards against floating-point contraction artifacts (e.g. GCC FMA fusion
// across function boundaries) that can make the difference negative or
// spuriously non-zero for equal boxes. See GitHub issue #1452.
template <typename Box>
typename default_content_result<Box>::type content_diff(Box const& expanded, Box const& original)
{
    using content_type = typename default_content_result<Box>::type;
    return (std::max)(content_type(0), content(expanded) - content(original));
}

#if defined(BOOST_GCC)
#pragma GCC pop_options
#endif

}}}} // namespace boost::geometry::index::detail

#endif // BOOST_GEOMETRY_INDEX_DETAIL_ALGORITHMS_CONTENT_HPP
