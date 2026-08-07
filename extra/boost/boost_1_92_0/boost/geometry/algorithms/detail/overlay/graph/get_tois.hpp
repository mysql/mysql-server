// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_TOIS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_TOIS_HPP

#include <boost/geometry/algorithms/detail/signed_size_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/segment_identifier.hpp>
#include <boost/geometry/algorithms/detail/overlay/graph/is_operation_included.hpp>
#include <boost/geometry/algorithms/detail/overlay/graph/node_util.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

template <operation_type TargetOperation, typename Turns, typename Clusters>
void add_tois(Turns const& turns, Clusters const& clusters,
        signed_size_type source_node_id, signed_size_type target_node_id,
        set_of_tois& result)
{
    using is_included = is_operation_included<TargetOperation>;

    auto get_tois_from_turns = [&](std::size_t const source_index, std::size_t const target_index)
    {
        for (int i = 0; i < 2; i++)
        {
            auto const& op = turns[source_index].operations[i];
            if (op.enriched.travels_to_ip_index == static_cast<signed_size_type>(target_index)
                && is_included::apply(op))
            {
                turn_operation_id const toi{source_index, i};
                if (is_target_operation<TargetOperation>(turns, toi))
                {
                    result.insert(std::move(toi));
                }
            }
        }
    };

    constexpr bool allow_closed = TargetOperation == operation_intersection;
    if (source_node_id >= 0 && target_node_id >= 0)
    {
        get_tois_from_turns(source_node_id, target_node_id);
    }
    else if (source_node_id < 0 && target_node_id >= 0)
    {
        const auto source_turn_indices = get_turn_indices_by_node_id(turns, clusters,
                    source_node_id, allow_closed);
        for (auto source_turn_index : source_turn_indices)
        {
            get_tois_from_turns(source_turn_index, target_node_id);
        }
    }
    else if (source_node_id >= 0 && target_node_id < 0)
    {
        const auto target_turn_indices = get_turn_indices_by_node_id(turns, clusters,
                target_node_id, allow_closed);
        for (auto target_turn_index : target_turn_indices)
        {
            get_tois_from_turns(source_node_id, target_turn_index);
        }
    }
    else
    {
        // Combine two sets together, quadratically
        const auto source_turn_indices = get_turn_indices_by_node_id(turns, clusters,
                source_node_id, allow_closed);
        const auto target_turn_indices = get_turn_indices_by_node_id(turns, clusters,
                target_node_id, allow_closed);
        for (auto source_turn_index : source_turn_indices)
        {
            for (auto target_turn_index : target_turn_indices)
            {
                get_tois_from_turns(source_turn_index, target_turn_index);
            }
        }
#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
        // This happens, for example, in multi line cases where lines are on top of each other.
        // Then there will be many turns, and many clusters with many turns.
        // It gives listings like:
        //      quadratic: -272 -> -273 sizes 55 x 55 = 110
        // It is currently probably not worth to cache these cases, as these are rare cases.
        // In the bitset_grids robustness test, the clusters are small and the listings are like:
        //     quadratic: -5 -> -1 sizes 2 x 3 = 1
        std::cout << "quadratic: "
            << source_node_id << " -> " << target_node_id
            << " sizes " << source_turn_indices.size() << " x " << target_turn_indices.size()
            << " = " << result.size()
            << std::endl;
#endif
    }
}

// Variant with one node
template <operation_type TargetOperation, typename Turns, typename Clusters>
set_of_tois get_tois(Turns const& turns, Clusters const& clusters,
        signed_size_type source_node_id, signed_size_type target_node_id)
{
    set_of_tois result;
    add_tois<TargetOperation>(turns, clusters, source_node_id, target_node_id, result);
    return result;
}

// Variant with multiple target nodes
template <operation_type TargetOperation, typename Turns, typename Clusters>
set_of_tois get_tois(Turns const& turns, Clusters const& clusters,
        signed_size_type source_node_id, std::set<signed_size_type> const& target_node_ids)
{
    set_of_tois result;
    for (auto const& target : target_node_ids)
    {
        add_tois<TargetOperation>(turns, clusters, source_node_id, target, result);
    }
    return result;
}

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GET_TOIS_HPP
