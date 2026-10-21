// Boost.Geometry

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_PROPERTIES_AHEAD_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_PROPERTIES_AHEAD_HPP

#include <boost/geometry/algorithms/detail/overlay/get_ring.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/copy_segment_point.hpp>
#include <boost/geometry/algorithms/detail/overlay/segment_identifier.hpp>

#if defined(BOOST_GEOMETRY_DEBUG_GET_PROPERTIES_AHEAD)
#include <boost/geometry/io/wkt/wkt.hpp>
#endif

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

// Get properties ahead of a turn. This is important for turns marked as cc (continue-continue).
// It walks over the ring (of each operation) into the direction of the next turn,
// as long as the points are collinear.
// It then reports both the collinear (comparable) distance, of the turn to the point where it bend,
// and the side of the next point. So where it bends to.
template
<
    bool Reverse1, bool Reverse2,
    typename Turns, typename Clusters,
    typename Geometry1, typename Geometry2,
    typename IntersectionStrategy
>
void get_properties_ahead(Turns& turns, Clusters const& clusters,
        Geometry1 const& geometry1, Geometry2 const& geometry2,
        IntersectionStrategy const& intersection_strategy)
{
    using point_type = typename Turns::value_type::point_type;
    auto const side_strategy = intersection_strategy.side();
    auto const comparable_distance_strategy
        = intersection_strategy.comparable_distance(point_type(), point_type());

    auto walk_ahead = [&](auto const& turn, auto& op)
    {
        auto current_ring_id = ring_id_by_seg_id(op.seg_id);
        auto const& next_turn = turns[op.enriched.travels_to_ip_index];
        auto const next_ring_id0 = ring_id_by_seg_id(next_turn.operations[0].seg_id);
        auto const next_op_index = next_ring_id0 == current_ring_id ? 0 : 1;
        auto const& next_op = next_turn.operations[next_op_index];
        signed_size_type const point_count_to_next_turn = current_ring_id.source_index == 0
                        ? segment_distance(geometry1, op.seg_id, next_op.seg_id)
                        : segment_distance(geometry2, op.seg_id, next_op.seg_id);

        int offset = 0;
        point_type point_of_segment0;
        geometry::copy_segment_point<Reverse1, Reverse2>(geometry1, geometry2, op.seg_id,
            offset++, point_of_segment0);

#if defined(BOOST_GEOMETRY_DEBUG_GET_PROPERTIES_AHEAD)
        std::cout << " EXAMINE AHEAD: ring " <<  current_ring_id
            << " segment: " << op.seg_id.segment_index
            << "  [ until turn: " << op.enriched.travels_to_ip_index
            << " at: " << next_op.seg_id.segment_index
            << " count: " << point_count_to_next_turn
            << " " << geometry::wkt(next_turn.point)
            << " ]"
            << std::endl;
#endif

        // It starts with the distance from the turn to the next point on the segment.

        point_type point_of_segment1;
        point_type side_changing_point_ahead = turn.point;
        bool found = false;
        int final_side = 0;
        for (auto i = 0; i <= point_count_to_next_turn; i++, offset++)
        {
            point_type current_point;
            if (i == point_count_to_next_turn)
            {
                current_point = next_turn.point;
            }
            else
            {
                geometry::copy_segment_point<Reverse1, Reverse2>(geometry1, geometry2,
                    op.seg_id, offset, current_point);
            }
            if (i == 0)
            {
                point_of_segment1 = current_point;
            }
            int const side = side_strategy.apply(point_of_segment0, point_of_segment1,
                current_point);
#if defined(BOOST_GEOMETRY_DEBUG_GET_PROPERTIES_AHEAD)
            std::cout << "  " << i << " " << geometry::wkt(current_point)
                << " side: " << side
                << std::endl;
#endif
            if (side != 0)
            {
                found = true;
                final_side = side;
                break;
            }
            if (! found)
            {
                side_changing_point_ahead = current_point;
            }
        }

        op.enriched.ahead_distance_of_side_change
            = comparable_distance_strategy.apply(point_of_segment0, side_changing_point_ahead);
        op.enriched.ahead_side = final_side;
#if defined(BOOST_GEOMETRY_DEBUG_GET_PROPERTIES_AHEAD)
        std::cout << "  result: " << op.enriched.ahead_distance_of_side_change
            << " side: " << op.enriched.ahead_side
            << std::endl;
#endif
    };

    // First examine all clusters (this includes cc turns)
    for (const auto& key_value : clusters)
    {
        auto const& cluster = key_value.second;
        for (auto const& index : cluster.turn_indices)
        {
            auto& turn = turns[index];
            for (auto& op : turn.operations)
            {
                if (op.enriched.travels_to_ip_index == -1)
                {
                    continue;
                }
#ifdef BOOST_GEOMETRY_DEBUG_GET_PROPERTIES_AHEAD
                std::cout << "Cluster " << turn.cluster_id
                    << " turn " << index
                    << " " << operation_char(op.operation)
                    << " travels to " << op.enriched.travels_to_ip_index
                    << std::endl;
#endif
                walk_ahead(turn, op);
            }
        }
    }

    // Then do the remaining cc turns
    for (auto& turn : turns)
    {
        if (turn.discarded || turn.is_clustered() || ! turn.both(operation_continue))
        {
            continue;
        }

        auto& op0 = turn.operations[0];
        auto& op1 = turn.operations[1];

        // IMPLEMENTATION NOTE:
        // This means: it should be called AFTER enrichment.
        // Which is called after assigning counts.
        if (op0.enriched.travels_to_ip_index == -1 || op1.enriched.travels_to_ip_index == -1)
        {
            continue;
        }

        walk_ahead(turn, op0);
        walk_ahead(turn, op1);
    }
}


}} // namespace detail::overlay
#endif //DOXYGEN_NO_DETAIL


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_PROPERTIES_AHEAD_HPP
