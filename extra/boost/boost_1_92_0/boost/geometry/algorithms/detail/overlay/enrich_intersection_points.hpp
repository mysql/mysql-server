// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2017 Adam Wulkiewicz, Lodz, Poland.

// This file was modified by Oracle on 2017-2024.
// Modifications copyright (c) 2017-2024 Oracle and/or its affiliates.
// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ENRICH_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ENRICH_HPP

#include <cstddef>
#include <algorithm>
#include <map>
#include <set>
#include <vector>

#ifdef BOOST_GEOMETRY_DEBUG_ENRICH
#  include <iostream>
#  include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#  include <boost/geometry/io/wkt/wkt.hpp>
#  if ! defined(BOOST_GEOMETRY_DEBUG_IDENTIFIER)
#    define BOOST_GEOMETRY_DEBUG_IDENTIFIER
  #endif
#endif

#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <boost/range/value_type.hpp>

#include <boost/geometry/algorithms/detail/ring_identifier.hpp>
#include <boost/geometry/algorithms/detail/overlay/check_enrich.hpp>
#include <boost/geometry/algorithms/detail/overlay/discard_duplicate_turns.hpp>
#include <boost/geometry/algorithms/detail/overlay/graph/adapt_operations.hpp>
#include <boost/geometry/algorithms/detail/overlay/handle_self_turns.hpp>
#include <boost/geometry/algorithms/detail/overlay/is_self_turn.hpp>
#include <boost/geometry/algorithms/detail/overlay/less_by_segment_ratio.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/util/constexpr.hpp>
#include <boost/geometry/views/enumerate_view.hpp>



namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

template <typename Turns>
struct discarded_indexed_turn
{
    discarded_indexed_turn(Turns const& turns)
        : m_turns(turns)
    {}

    template <typename IndexedTurn>
    inline bool operator()(IndexedTurn const& indexed) const
    {
        return m_turns[indexed.turn_index].discarded;
    }

    Turns const& m_turns;
};

// Sorts IP-s of this ring on segment-identifier, and if on same segment,
//  on distance.
// Then assigns for each IP which is the next IP on this segment,
// plus the vertex-index to travel to, plus the next IP
// (might be on another segment)
template
<
    bool Reverse1, bool Reverse2,
    typename Operations,
    typename Turns,
    typename Geometry1, typename Geometry2,
    typename Strategy
>
inline void enrich_sort(Operations& operations,
            Turns const& turns,
            Geometry1 const& geometry1,
            Geometry2 const& geometry2,
            Strategy const& strategy)
{
    std::sort(std::begin(operations),
              std::end(operations),
              less_by_segment_ratio
                <
                    Turns,
                    typename boost::range_value<Operations>::type,
                    Geometry1, Geometry2,
                    Strategy,
                    Reverse1, Reverse2
                >(turns, geometry1, geometry2, strategy));
}

// Assign travel-to-vertex/ip index for each turn.
template <typename Operations, typename Turns>
inline void enrich_assign(Operations& operations, Turns& turns)
{
    for (auto const& item : util::enumerate(operations))
    {
        auto const& index = item.index;
        auto const& indexed = item.value;

        if (indexed.discarded)
        {
            continue;
        }

        auto& turn = turns[indexed.turn_index];
        auto& op = turn.operations[indexed.operation_index];

        std::size_t skipped_count = 0;

        auto advance = [&](auto index)
        {
            std::size_t result = (index + 1) % operations.size();
            while (operations[result].discarded)
            {
                result = (result + 1) % operations.size();
                auto const& next_turn = turns[operations[result].turn_index];
                if (! next_turn.is_traversable)
                {
                    // There might be more conditions to skip.
                    // But it should not skip ALL discarded turns.
                    skipped_count++;
#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
                    std::cout << "  -> Skip for " << operations[index].turn_index << " because of blocked "
                                << " " << operations[result].turn_index
                                << std::endl;
#endif
                }
            }
            return result;
        };

        std::size_t next_index = advance(index);

        auto next_turn = [&operations, &turns, &next_index]()
        {
            return turns[operations[next_index].turn_index];
        };
        auto next_operation = [&operations, &turns, &next_index]()
        {
            auto const& next_turn = turns[operations[next_index].turn_index];
            return next_turn.operations[operations[next_index].operation_index];
        };

        // Cluster behaviour: next should point after cluster, unless
        // their seg_ids are not the same
        // (For dissolve, this is still to be examined - TODO)
        while (turn.is_clustered()
                && turn.cluster_id == next_turn().cluster_id
                && op.seg_id == next_operation().seg_id
                && indexed.turn_index != operations[next_index].turn_index)
        {
            // In same cluster, on same segment, but not same turn
            next_index = advance(next_index);
        }

        if (skipped_count > 0)
        {
            // Don't assign travel-info, because it is not reachable. But neither discard the turn,
            // the other operation might be reachable.
            continue;
        }

        op.enriched.travels_to_ip_index
                = static_cast<signed_size_type>(operations[next_index].turn_index);
        op.enriched.travels_to_vertex_index
                = operations[next_index].subject->seg_id.segment_index;
    }

#ifdef BOOST_GEOMETRY_DEBUG_ENRICH
    for (auto const& indexed_op : operations)
    {
        auto const& op = turns[indexed_op.turn_index].operations[indexed_op.operation_index];

        std::cout << indexed_op.turn_index
            << " cl=" << turns[indexed_op.turn_index].cluster_id
            << " meth=" << method_char(turns[indexed_op.turn_index].method)
            << " seg=" << op.seg_id
            << " dst=" << op.fraction // needs define
            << " op=" << operation_char(turns[indexed_op.turn_index].operations[0].operation)
            << operation_char(turns[indexed_op.turn_index].operations[1].operation)
            << " (" << operation_char(op.operation) << ")"
            << " to=" << op.enriched.travels_to_ip_index
            << " [vx " << op.enriched.travels_to_vertex_index << "]"
            << (turns[indexed_op.turn_index].discarded ? " [discarded]" : "")
            << (op.enriched.startable ? "" : " [not startable]")
            << (indexed_op.discarded ? " DISCARDED" : "")
            << std::endl;
    }
#endif
}

template <typename Operations, typename Turns>
inline void enrich_adapt(Operations& operations, Turns& turns)
{
    // Operations is a vector of indexed_turn_operation<>
    // If it is empty, or contains one or two items, it makes no sense
    if (operations.size() < 3)
    {
        return;
    }

    bool next_phase = false;
    std::size_t previous_index = operations.size() - 1;

    for (auto const& item : util::enumerate(operations))
    {
        auto const& index = item.index;
        auto const& indexed = item.value;
        auto& turn = turns[indexed.turn_index];
        auto& op = turn.operations[indexed.operation_index];

        std::size_t const next_index = (index + 1) % operations.size();
        auto const& next_turn = turns[operations[next_index].turn_index];
        auto const& next_op = next_turn.operations[operations[next_index].operation_index];

        if (op.seg_id.segment_index == next_op.seg_id.segment_index)
        {
            auto const& prev_turn = turns[operations[previous_index].turn_index];
            auto const& prev_op = prev_turn.operations[operations[previous_index].operation_index];
            if (op.seg_id.segment_index == prev_op.seg_id.segment_index)
            {
                op.enriched.startable = false;
                next_phase = true;
            }
        }
        previous_index = index;
    }

    if (! next_phase)
    {
        return;
    }

    // Discard turns which are both non-startable
    next_phase = false;
    for (auto& turn : turns)
    {
        if (! turn.operations[0].enriched.startable
            && ! turn.operations[1].enriched.startable)
        {
            turn.discarded = true;
            next_phase = true;
        }
    }

    if (! next_phase)
    {
        return;
    }

    // Remove discarded turns from operations to avoid having them as next turn
    discarded_indexed_turn<Turns> const predicate(turns);
    operations.erase(std::remove_if(std::begin(operations),
        std::end(operations), predicate), std::end(operations));
}

struct enriched_map_default_include_policy
{
    template <typename Operation>
    static inline bool include(Operation const& )
    {
        // By default include all operations
        return true;
    }
};

// Add all (non discarded) operations on this ring
// Blocked operations or uu on clusters (for intersection)
// should be included, to block potential paths in clusters
template <typename Turns, typename IncludePolicy>
inline auto create_map(Turns const& turns, IncludePolicy const& include_policy)
{
    using turn_type = typename boost::range_value<Turns>::type;
    using indexed_turn_operation = indexed_turn_operation
        <
            typename turn_type::turn_operation_type
        >;

    std::map
    <
        ring_identifier,
        std::vector<indexed_turn_operation>
    > mapped_vector;

    for (auto const& turn_item : util::enumerate(turns))
    {
        auto const& index = turn_item.index;
        auto const& turn = turn_item.value;
        for (auto const& op_item : util::enumerate(turn.operations))
        {
            auto const& op_index = op_item.index;
            auto const& op = op_item.value;
            if (include_policy.include(op.operation))
            {
                mapped_vector[ring_id_by_seg_id(op.seg_id)].emplace_back
                    (
                         index, op_index, op, turn.operations[1 - op_index].seg_id, turn.discarded
                    );
            }
        }
    }

    return mapped_vector;
}

template
<
    overlay_type OverlayType,
    typename Turns,
    typename Clusters,
    typename Geometry1, typename Geometry2,
    typename IntersectionStrategy
>
inline void enrich_discard_turns(Turns& turns, Clusters& clusters,
    Geometry1 const& geometry1, Geometry2 const& geometry2,
    IntersectionStrategy const& strategy)
{
    constexpr operation_type target_operation = operation_from_overlay<OverlayType>::value;

    constexpr operation_type opposite_operation
            = target_operation == operation_union
            ? operation_intersection
            : operation_union;

    // Turns are often used by index (in clusters, next_index, etc)
    // and turns may therefore NOT be DELETED - they may only be flagged as discarded

    discard_duplicate_turns(turns, geometry1, geometry2);

    // Discard turns not part of target overlay
    for (auto& turn : turns)
    {
        if (turn.both(operation_none)
            || turn.both(opposite_operation)
            || turn.both(operation_blocked)
            || (is_self_turn<OverlayType>(turn)
                && ! turn.is_clustered()
                && ! turn.both(target_operation)))
        {
            // For all operations, discard xx and none/none
            // For intersections, remove uu to avoid the need to travel
            // a union (during intersection) in uu/cc clusters (e.g. #31,#32,#33)

            // Similarly, for union, discard ii and ix

            // For self-turns, only keep uu / ii

            turn.discarded = true;
            turn.cluster_id = -1;
        }

#if defined(BOOST_GEOMETRY_CONCEPT_FIX_START_TURNS)
        if (turn.is_clustered() && turn.method == method_start)
        {
            // Start turns are generated, in case the previous turn is missed.
            // But often it is not missed, and then it should be deleted.
            // TODO: only if the cluster NOT ONLY contains start turns, and there are turns left...
            turn.discarded = true;
            turn.cluster_id = -1;
        }
#endif
    }

    // Discard self turns located within the other geometry (for union)
    // or outside it (for intersection)
    // For buffer or dissolve, nothing is called.
    discard_closed_turns
        <
            OverlayType,
            target_operation
        >::apply(turns, clusters, geometry1, geometry2,  strategy);
    discard_open_turns
        <
            OverlayType,
            target_operation
        >::apply(turns, clusters, geometry1, geometry2, strategy);

    // Remove discarded turns from clusters
    cleanup_clusters(turns, clusters);
}

template
<
    bool Reverse1, bool Reverse2,
    overlay_type OverlayType,
    typename Turns,
    typename Geometry1, typename Geometry2,
    typename IntersectionStrategy
>
inline void enrich_turns(Turns& turns,
    Geometry1 const& geometry1, Geometry2 const& geometry2,
    IntersectionStrategy const& strategy)
{
    constexpr operation_type target_operation = operation_from_overlay<OverlayType>::value;

    // Create a map of vectors of indexed operation-types to be able
    // to sort intersection points PER RING
    auto mapped_vector = create_map(turns, enriched_map_default_include_policy());

    for (auto& pair : mapped_vector)
    {
        enrich_sort<Reverse1, Reverse2>(pair.second, turns, geometry1, geometry2, strategy);
    }

    for (auto& pair : mapped_vector)
    {
#ifdef BOOST_GEOMETRY_DEBUG_ENRICH
    std::cout << "ENRICH-assign Ring " << pair.first << std::endl;
#endif
        if BOOST_GEOMETRY_CONSTEXPR (OverlayType == overlay_dissolve)
        {
            enrich_adapt(pair.second, turns);
        }

        enrich_assign(pair.second, turns);
    }

    block_ux_uu_workaround(turns);

#ifdef BOOST_GEOMETRY_DEBUG_ENRICH
    constexpr bool do_check_graph = true;
#else
    constexpr bool do_check_graph = false;
#endif

    if BOOST_GEOMETRY_CONSTEXPR (do_check_graph)
    {
        check_graph(turns, target_operation);
    }
}

}} // namespace detail::overlay
#endif //DOXYGEN_NO_DETAIL

/*!
\brief All intersection points are enriched with successor information
\ingroup overlay
\tparam Turns type of intersection container
            (e.g. vector of "intersection/turn point"'s)
\tparam Clusters type of cluster container
\tparam Geometry1 \tparam_geometry
\tparam Geometry2 \tparam_geometry
\tparam PointInGeometryStrategy point in geometry strategy type
\param turns container containing intersection points
\param clusters container containing clusters
\param geometry1 \param_geometry
\param geometry2 \param_geometry
\param strategy point in geometry strategy
 */
template
<
    bool Reverse1, bool Reverse2,
    overlay_type OverlayType,
    typename Turns,
    typename Clusters,
    typename Geometry1, typename Geometry2,
    typename IntersectionStrategy
>
inline void enrich_intersection_points(Turns& turns,
    Clusters& clusters,
    Geometry1 const& geometry1, Geometry2 const& geometry2,
    IntersectionStrategy const& strategy)
{
    detail::overlay::enrich_discard_turns<OverlayType>(turns, clusters, geometry1, geometry2, strategy);
    detail::overlay::enrich_turns<Reverse1, Reverse2, OverlayType>(turns, geometry1, geometry2, strategy);
}

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ENRICH_HPP
