// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.
// Copyright (c) 2013-2023 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2015-2024.
// Modifications copyright (c) 2015-2024 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_POINT_IS_EQUAL_OR_SPIKE_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_POINT_IS_EQUAL_OR_SPIKE_HPP

#include <boost/geometry/algorithms/detail/direction_code.hpp>
#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/strategies/side.hpp>
#include <boost/geometry/util/constexpr.hpp>
#include <boost/geometry/util/math.hpp>


namespace boost { namespace geometry
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail
{

// Checks if a point ("last_point") causes a spike w.r.t.
// the specified two other points (segment_a, segment_b)
//
//  x-------x------x
//  a       lp     b
//
// Above, lp generates a spike w.r.t. segment(a,b)
// So specify last point first, then (a,b)
// The segment's orientation does matter: if lp is to the right of b
// no spike is reported
template
<
    typename Point1, typename Point2, typename Point3,
    typename SideStrategy
>
inline bool point_is_spike_or_equal(Point1 const& last_point, // prev | back
                                    Point2 const& segment_a,  // next | back - 2
                                    Point3 const& segment_b,  // curr | back - 1 | spike's vertex
                                    SideStrategy const& strategy)
{
    using cs_tag = typename SideStrategy::cs_tag;

    int const side = strategy.apply(segment_a, segment_b, last_point);
    if (side == 0)
    {
        // Last point is collinear w.r.t previous segment.
        return direction_code<cs_tag>(segment_a, segment_b, last_point) < 1;
    }
    return false;
}

template
<
    typename Point1,
    typename Point2,
    typename Point3,
    typename SideStrategy
>
inline bool point_is_collinear(Point1 const& last_point,
            Point2 const& segment_a,
            Point3 const& segment_b,
            SideStrategy const& strategy)
{
    int const side = strategy.apply(segment_a, segment_b, last_point);
    if (side == 0)
    {
        return true;
    }
    return false;
}


//! Version with intuitive order (A, B, C). The original order was
//! unclear (C, A, B). It was used in a different way in has_spikes.
//! On longer term the C,A,B version can be deprecated
template
<
    typename Point1,
    typename Point2,
    typename Point3,
    typename SideStrategy
>
inline bool is_spike_or_equal(Point1 const& a,
            Point2 const& b,
            Point3 const& c,
            SideStrategy const& strategy)
{
    return point_is_spike_or_equal(c, a, b, strategy);
}


} // namespace detail
#endif

}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_POINT_IS_EQUAL_OR_SPIKE_HPP
