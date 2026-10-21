// Boost.Geometry

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ASSIGN_SIDE_COUNTS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ASSIGN_SIDE_COUNTS_HPP

#include <boost/geometry/algorithms/detail/overlay/graph/assign_counts.hpp>
#include <boost/geometry/algorithms/detail/overlay/graph/assign_clustered_counts.hpp>
#include <boost/geometry/algorithms/detail/overlay/handle_self_turns.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

template
<
    bool Reverse1,
    bool Reverse2,
    overlay_type OverlayType,
    typename Geometry1,
    typename Geometry2,
    typename Turns,
    typename Clusters,
    typename IntersectionStrategy,
    typename Visitor
>
void assign_side_counts(Geometry1 const& geometry1, Geometry2 const& geometry2,
        Turns& turns, Clusters& clusters,
        IntersectionStrategy const& intersection_strategy, Visitor& visitor)
{
    clustered_count_handler
        <
            Reverse1, Reverse2, OverlayType,
            Geometry1, Geometry2,
            Turns, Clusters,
            IntersectionStrategy
        > processor(geometry1, geometry2, turns, clusters, intersection_strategy);

    processor.apply(visitor);

    if (OverlayType != overlay_buffer)
    {
        // Increase right-count for self-intersections. This should not be called for buffers
        // (for buffers, all is self-cluster)
        assign_clustered_self_counts(turns, clusters);
    }
    assign_unclustered_counts(turns);
}

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ASSIGN_SIDE_COUNTS_HPP
