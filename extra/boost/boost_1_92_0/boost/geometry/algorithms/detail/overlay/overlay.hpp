// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2013-2017 Adam Wulkiewicz, Lodz, Poland

// This file was modified by Oracle on 2015-2024.
// Modifications copyright (c) 2015-2024, Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_OVERLAY_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_OVERLAY_HPP


#include <deque>
#include <map>

#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <boost/range/value_type.hpp>

#include <boost/geometry/algorithms/detail/overlay/graph/assign_side_counts.hpp>
#include <boost/geometry/algorithms/detail/overlay/cluster_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/enrich_intersection_points.hpp>
#include <boost/geometry/algorithms/detail/overlay/enrichment_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/get_properties_ahead.hpp>
#include <boost/geometry/algorithms/detail/overlay/get_turns.hpp>
#include <boost/geometry/algorithms/detail/overlay/handle_colocations.hpp>
#include <boost/geometry/algorithms/detail/overlay/is_self_turn.hpp>
#include <boost/geometry/algorithms/detail/overlay/needs_self_turns.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/traverse.hpp>
#include <boost/geometry/algorithms/detail/overlay/traversal_info.hpp>
#include <boost/geometry/algorithms/detail/overlay/self_turn_points.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>

#include <boost/geometry/algorithms/is_empty.hpp>
#include <boost/geometry/algorithms/reverse.hpp>

#include <boost/geometry/algorithms/detail/overlay/add_rings.hpp>
#include <boost/geometry/algorithms/detail/overlay/assign_parents.hpp>
#include <boost/geometry/algorithms/detail/overlay/ring_properties.hpp>
#include <boost/geometry/algorithms/detail/overlay/select_rings.hpp>
#include <boost/geometry/algorithms/detail/overlay/do_reverse.hpp>

#include <boost/geometry/util/condition.hpp>


namespace boost { namespace geometry
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{


//! Default visitor for overlay, doing nothing
struct overlay_null_visitor
{
    template <typename Turns>
    void visit_turns(int , Turns const& ) {}

    template <typename Clusters, typename Turns>
    void visit_clusters(Clusters const& , Turns const& ) {}

    template <typename Turns, typename Cluster, typename Connections>
    inline void visit_cluster_connections(signed_size_type cluster_id,
            Turns const& turns, Cluster const& cluster, Connections const& connections) {}

    template <typename Turns, typename Turn, typename Operation>
    void visit_traverse(Turns const& , Turn const& , Operation const& , char const*)
    {}

    template <typename Rings>
    void visit_generated_rings(Rings const& )
    {}
};

template
<
    overlay_type OverlayType,
    typename TurnInfoMap,
    typename Turns,
    typename Clusters
>
inline void get_ring_turn_info(TurnInfoMap& turn_info_map, Turns const& turns, Clusters const& clusters)
{
    static const operation_type target_operation
            = operation_from_overlay<OverlayType>::value;
    static const operation_type opposite_operation
            = target_operation == operation_union
            ? operation_intersection
            : operation_union;
    static const bool is_union = target_operation == operation_union;

    for (auto const& turn : turns)
    {
        if (turn.discarded && (turn.method == method_start || is_self_turn<OverlayType>(turn)))
        {
            // Discarded self-turns or start turns don't need to block the ring
            continue;
        }

        for (int i = 0; i < 2; i++)
        {
            auto const& op = turn.operations[i];
            auto const& other_op = turn.operations[1 - i];
            ring_identifier const ring_id = ring_id_by_seg_id(op.seg_id);

            // The next condition is necessary for just two test cases.
            // TODO: fix it in get_turn_info
            // If the turn (one of its operations) is used during traversal,
            // and it is an intersection or difference, it cannot be set to blocked.
            // This is a rare case, related to floating point precision,
            // and can happen if there is, for example, only one start turn which is
            // used to traverse through one of the rings (the other should be marked
            // as not traversed, but neither blocked).
            bool const can_block = is_union || ! (op.enriched.is_traversed || other_op.enriched.is_traversed);

            if (! is_self_turn<OverlayType>(turn) && can_block)
            {
                turn_info_map[ring_id].has_blocked_turn = true;
                continue;
            }

            if (is_union && turn.any_blocked())
            {
                turn_info_map[ring_id].has_blocked_turn = true;
            }
            if (turn_info_map[ring_id].has_traversed_turn
                    || turn_info_map[ring_id].has_blocked_turn)
            {
                continue;
            }

            // Block rings where any other turn is blocked,
            // and (with exceptions): i for union and u for intersection
            // Exceptions: don't block self-uu for intersection
            //             don't block self-ii for union
            //             don't block (for union) i/u if there is an self-ii too
            if (op.operation == opposite_operation
                    && can_block
                    && ! (turn.both(opposite_operation)
                          && is_self_turn<OverlayType>(turn)))
            {
                turn_info_map[ring_id].has_blocked_turn = true;
            }
        }
    }
}

template
<
    typename GeometryOut, overlay_type OverlayType, bool ReverseOut,
    typename Geometry1, typename Geometry2,
    typename OutputIterator, typename Strategy
>
inline OutputIterator return_if_one_input_is_empty(Geometry1 const& geometry1,
            Geometry2 const& geometry2,
            OutputIterator out, Strategy const& strategy)
{
    using ring_type = geometry::ring_type_t<GeometryOut>;
    using ring_container_type = std::deque<ring_type>;

    using properties = ring_properties
        <
            geometry::point_type_t<ring_type>,
            typename geometry::area_result<ring_type, Strategy>::type
        >;

// Silence warning C4127: conditional expression is constant
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4127)
#endif

    // Union: return either of them
    // Intersection: return nothing
    // Difference: return first of them
    if (OverlayType == overlay_intersection
        || (OverlayType == overlay_difference && geometry::is_empty(geometry1)))
    {
        return out;
    }

#if defined(_MSC_VER)
#pragma warning(pop)
#endif


    std::map<ring_identifier, ring_turn_info> empty;
    std::map<ring_identifier, properties> all_of_one_of_them;

    select_rings<OverlayType>(geometry1, geometry2, empty, all_of_one_of_them, strategy);
    ring_container_type rings;
    assign_parents<OverlayType>(geometry1, geometry2, rings, all_of_one_of_them, strategy);
    return add_rings<GeometryOut>(all_of_one_of_them, geometry1, geometry2, rings, out, strategy);
}


template
<
    typename Geometry1, typename Geometry2,
    bool Reverse1, bool Reverse2, bool ReverseOut,
    typename GeometryOut,
    overlay_type OverlayType
>
struct overlay
{
    template <typename OutputIterator, typename Strategy, typename Visitor>
    static inline OutputIterator apply(
                Geometry1 const& geometry1, Geometry2 const& geometry2,
                OutputIterator out,
                Strategy const& strategy,
                Visitor& visitor)
    {
        bool const is_empty1 = geometry::is_empty(geometry1);
        bool const is_empty2 = geometry::is_empty(geometry2);

        if (is_empty1 && is_empty2)
        {
            return out;
        }

        if (is_empty1 || is_empty2)
        {
            return return_if_one_input_is_empty
                <
                    GeometryOut, OverlayType, ReverseOut
                >(geometry1, geometry2, out, strategy);
        }

        using point_type = geometry::point_type_t<GeometryOut>;
        using turn_info = detail::overlay::traversal_turn_info
        <
            point_type,
            typename segment_ratio_type<point_type>::type
        >;
        using turn_container_type = std::deque<turn_info>;

        using ring_type = geometry::ring_type_t<GeometryOut>;
        using ring_container_type = std::deque<ring_type>;

        // Define the clusters, mapping cluster_id -> turns
        using cluster_type = std::map
            <
                signed_size_type,
                cluster_info
            >;

        turn_container_type turns;

        detail::get_turns::no_interrupt_policy policy;
        geometry::get_turns
            <
                Reverse1, Reverse2,
                assign_policy_only_start_turns
            >(geometry1, geometry2, strategy, turns, policy);

        visitor.visit_turns(1, turns);

#if ! defined(BOOST_GEOMETRY_NO_SELF_TURNS)
        if (! turns.empty() || OverlayType == overlay_dissolve)
        {
            // Calculate self turns if the output contains turns already,
            // and if necessary (e.g.: multi-geometry, polygon with interior rings)
            if (needs_self_turns<Geometry1>::apply(geometry1))
            {
                self_get_turn_points::self_turns<Reverse1, assign_policy_only_start_turns>(geometry1,
                    strategy, turns, policy, 0);
            }
            if (needs_self_turns<Geometry2>::apply(geometry2))
            {
                self_get_turn_points::self_turns<Reverse2, assign_policy_only_start_turns>(geometry2,
                    strategy, turns, policy, 1);
            }
        }
#endif

        cluster_type clusters;

        // Handle colocations, gathering clusters and (below) their properties.
        detail::overlay::handle_colocations(turns, clusters);


        detail::overlay::enrich_discard_turns<OverlayType>(
            turns, clusters, geometry1, geometry2, strategy);

        detail::overlay::enrich_turns<Reverse1, Reverse2, OverlayType>(
            turns, geometry1, geometry2, strategy);

        visitor.visit_turns(2, turns);

        detail::overlay::colocate_clusters(clusters, turns);

        // AssignCounts should be called:
        // * after "colocate_clusters"
        // * and "colocate_clusters" after "enrich_discard_turns"
        // because assigning side counts needs cluster centroids.
        //
        // For BUFFER - it is called before, to be able to block closed clusters
        // before enrichment.

        assign_side_counts
            <
                Reverse1, Reverse2, OverlayType
            >(geometry1, geometry2, turns, clusters, strategy, visitor);

        get_properties_ahead<Reverse1, Reverse2>(turns, clusters, geometry1, geometry2, strategy);

        // Traverse through intersection/turn points and create rings of them.
        // These rings are always in clockwise order.
        // In CCW polygons they are marked as "to be reversed" below.
        std::map<ring_identifier, ring_turn_info> turn_info_per_ring;
        ring_container_type rings;
        traverse<Reverse1, Reverse2, Geometry1, Geometry2, OverlayType>::apply
                (
                    geometry1, geometry2,
                    strategy,
                    turns, rings,
                    turn_info_per_ring,
                    clusters,
                    visitor
                );

        visitor.visit_clusters(clusters, turns);
        visitor.visit_turns(3, turns);

        get_ring_turn_info<OverlayType>(turn_info_per_ring, turns, clusters);

        using properties = ring_properties
            <
                point_type,
                typename geometry::area_result<ring_type, Strategy>::type
            >;

        // Select all rings which are NOT touched by any intersection point
        std::map<ring_identifier, properties> selected_ring_properties;
        // When the overlay produced no turns, allow a point on the boundary to
        // count as within the other geometry (see issue 1471).
        select_rings<OverlayType>(geometry1, geometry2, turn_info_per_ring,
                selected_ring_properties, strategy, turns.empty());

        // Add rings created during traversal
        {
            ring_identifier id(2, 0, -1);
            for (auto const& ring : rings)
            {
                selected_ring_properties[id] = properties(ring, strategy);
                selected_ring_properties[id].reversed = ReverseOut;
                id.multi_index++;
            }
        }

        assign_parents<OverlayType>(geometry1, geometry2,
            rings, selected_ring_properties, strategy);

        // NOTE: There is no need to check result area for union because
        // as long as the polygons in the input are valid the resulting
        // polygons should be valid as well.
        // By default the area is checked (this is old behavior) however this
        // can be changed with #define. This may be important in non-cartesian CSes.
        // The result may be too big, so the area is negative. In this case either
        // it can be returned or an exception can be thrown.
        return add_rings<GeometryOut>(selected_ring_properties, geometry1, geometry2, rings, out,
                                      strategy,
#if defined(BOOST_GEOMETRY_UNION_THROW_INVALID_OUTPUT_EXCEPTION)
                                      OverlayType == overlay_union ?
                                      add_rings_throw_if_reversed
                                      : add_rings_ignore_unordered
#elif defined(BOOST_GEOMETRY_UNION_RETURN_INVALID)
                                      OverlayType == overlay_union ?
                                      add_rings_add_unordered
                                      : add_rings_ignore_unordered
#else
                                      add_rings_ignore_unordered
#endif
                                      );
    }

    template <typename OutputIterator, typename Strategy>
    static inline OutputIterator apply(
                Geometry1 const& geometry1, Geometry2 const& geometry2,
                OutputIterator out,
                Strategy const& strategy)
    {
        overlay_null_visitor visitor;
        return apply(geometry1, geometry2, out, strategy, visitor);
    }
};


}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_OVERLAY_HPP
