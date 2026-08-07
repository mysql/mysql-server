// Boost.Geometry

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_POSITION_CODE_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_POSITION_CODE_HPP

#include <boost/geometry/algorithms/detail/direction_code.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

// Position coding of the point with respect to a segment.
// This is a combination of side and direction_code.
// It is counter clockwise from the segment.
// (because polygons are on the right side of a segment, and this way
// we can walk through the ranks ascending.
// 
//        3
//        |
//   4    *    2    *: p2
//        |
//        1
//        ^         ^: p1
template <typename Point, typename SideStrategy>
int get_position_code(Point const& p1, Point const& p2, Point const& point, SideStrategy const& side_strategy)
{
    using cs_tag = typename SideStrategy::cs_tag;
    auto const side = side_strategy.apply(p1, p2, point);                
    if (side == 1)
    {
        // left of [p1..p2]
        return 4;
    }
    else if (side == -1)
    {
        // right of [p1..p2]
        return 2;
    }

    // collinear with [p1..p2]
    auto const dir_code = direction_code<cs_tag>(p1, p2, point);
    if (dir_code == -1)
    {
        // collinear, on [p1..p2] or before p1
        return 1;
    }
    else if (dir_code == 1)
    {
        // collinear with [p1..p2], but farther than p2
        return 3;
    }

    // The segment is degenerate
    return 0;
}

} // namespace detail
#endif //DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_POSITION_CODE_HPP
