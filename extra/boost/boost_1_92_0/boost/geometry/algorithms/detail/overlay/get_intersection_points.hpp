// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2017-2024.
// Modifications copyright (c) 2017-2024 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_INTERSECTION_POINTS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_INTERSECTION_POINTS_HPP


#include <cstddef>
#include <type_traits>

#include <boost/range/value_type.hpp>

#include <boost/geometry/algorithms/convert.hpp>
#include <boost/geometry/algorithms/detail/overlay/get_turns.hpp>

#include <boost/geometry/geometries/segment.hpp>

namespace boost { namespace geometry
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace get_intersection_points
{


template
<
    typename Point1,
    typename Point2,
    typename TurnInfo
>
struct get_turn_without_info
{
    template
    <
        typename UniqueSubRange1,
        typename UniqueSubRange2,
        typename Strategy,
        typename OutputIterator
    >
    static inline OutputIterator apply(UniqueSubRange1 const& range_p,
                UniqueSubRange2 const& range_q,
                TurnInfo const& ,
                Strategy const& strategy,
                OutputIterator out)
    {
        using turn_point_type = typename TurnInfo::point_type;

        using policy_type = policies::relate::segments_intersection_points
            <
                segment_intersection_points<turn_point_type>
            >;

        typename policy_type::return_type const result
            = strategy.relate().apply(range_p, range_q, policy_type());

        for (std::size_t i = 0; i < result.count; i++)
        {
            TurnInfo tp;
            geometry::convert(result.intersections[i], tp.point);
            *out++ = tp;
        }

        return out;
    }
};

}} // namespace detail::get_intersection_points
#endif // DOXYGEN_NO_DETAIL




template
<
    typename Geometry1,
    typename Geometry2,
    typename Turns,
    typename Strategy
>
inline void get_intersection_points(Geometry1 const& geometry1,
            Geometry2 const& geometry2,
            Turns& turns,
            Strategy const& strategy)
{
    concepts::check_concepts_and_equal_dimensions<Geometry1 const, Geometry2 const>();

    using turn_policy_t = detail::get_intersection_points::get_turn_without_info
        <
            point_type_t<Geometry1>,
            point_type_t<Geometry2>,
            typename boost::range_value<Turns>::type
        >;

    detail::get_turns::no_interrupt_policy interrupt_policy;

    std::conditional_t
        <
            reverse_dispatch<Geometry1, Geometry2>::type::value,
            dispatch::get_turns_reversed
            <
                tag_t<Geometry1>,
                tag_t<Geometry2>,
                Geometry1, Geometry2,
                false, false,
                turn_policy_t
            >,
            dispatch::get_turns
            <
                tag_t<Geometry1>,
                tag_t<Geometry2>,
                Geometry1, Geometry2,
                false, false,
                turn_policy_t
            >
        >::apply(0, geometry1,
                 1, geometry2,
                 strategy,
                 turns, interrupt_policy);
}




}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_INTERSECTION_POINTS_HPP
