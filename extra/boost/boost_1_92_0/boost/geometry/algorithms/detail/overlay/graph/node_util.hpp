// Boost.Geometry

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_NODE_UTIL_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_NODE_UTIL_HPP

#include <boost/geometry/algorithms/detail/signed_size_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/graph/is_operation_included.hpp>
#include <boost/geometry/algorithms/detail/overlay/graph/is_target_operation.hpp>
#include <boost/geometry/algorithms/detail/overlay/segment_identifier.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_operation_id.hpp>

#include <set>
#include <tuple>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

using set_of_tois = std::set<turn_operation_id>;
using set_of_size_t = std::set<std::size_t>;

struct edge_info
{
    signed_size_type source_node_id{0};
    signed_size_type target_node_id{0};
    segment_identifier seg_id{};

    bool operator<(edge_info const& other) const
    {
        return std::tie(source_node_id, target_node_id, seg_id)
             < std::tie(other.source_node_id, other.target_node_id, other.seg_id);
    }
};

template <typename Turns, typename Clusters>
set_of_size_t get_turn_indices_by_cluster_id(Turns const& turns, Clusters const& clusters,
        signed_size_type cluster_id, bool allow_closed)
{
    set_of_size_t result;
    auto it = clusters.find(cluster_id);
    if (it == clusters.end())
    {
        return result;
    }
    if (! allow_closed && it->second.open_count == 0)
    {
        return result;
    }
    for (std::size_t turn_index : it->second.turn_indices)
    {
        result.insert(turn_index);
    }
    return result;
}

// Returns the node id of the turn:
// - if it is clustered, the negative cluster_id
// - if it is not clustered, the turn index
// - there can also be extra nodes for the round trip (>= turns.size())
//   but they are not returned by this function.
template <typename Turns>
signed_size_type get_node_id(Turns const& turns, std::size_t turn_index)
{
    auto const& turn = turns[turn_index];
    return turn.is_clustered() ? -turn.cluster_id : turn_index;
}

template <typename Turns, typename Clusters>
set_of_size_t get_turn_indices_by_node_id(Turns const& turns, Clusters const& clusters,
        signed_size_type node_id, bool allow_closed)
{
    if (node_id < 0)
    {
        return get_turn_indices_by_cluster_id(turns, clusters, -node_id, allow_closed);
    }
    auto const turn_index = static_cast<std::size_t>(node_id);
    if (turn_index >= turns.size())
    {
        // It is 'allowed' to have node_ids larger than the largest turn index (for example extra
        // nodes in a graph). But they are not related to turns.
        return {};
    }

    auto const& turn = turns[turn_index];
    if (turn.is_clustered())
    {
        return get_turn_indices_by_cluster_id(turns, clusters, turn.cluster_id, allow_closed);
    }
    return {turn_index};
}

template <operation_type TargetOperation, typename Turns>
void get_target_operations(Turns const& turns,
                      typename Turns::value_type const& turn,
                      std::size_t turn_index,
                      signed_size_type source_node_id,
                      std::set<edge_info>& edges)
{
    using is_included = is_operation_included<TargetOperation>;
    for (int j = 0; j < 2; j++)
    {
        auto const& op = turn.operations[j];
        if (is_included::apply(op)
            && is_target_operation<TargetOperation>(turns, {turn_index, j}))
        {
            auto const& target_node_id = get_node_id(turns, op.enriched.travels_to_ip_index);
            edges.insert({source_node_id, target_node_id, op.seg_id});
        }
    }
}


// Get the target nodes of a specific component_id only.
template <operation_type TargetOperation, typename Turns, typename Clusters, typename Set>
auto get_target_nodes(Turns const& turns, Clusters const& clusters,
                      Set const& turn_indices,
                      signed_size_type component_id)
{
    using is_included = is_operation_included<TargetOperation>;

    std::set<signed_size_type> result;
    for (auto turn_index : turn_indices)
    {
        auto const& turn = turns[turn_index];
        if (turn.discarded)
        {
            continue;
        }

        for (int j = 0; j < 2; j++)
        {
            auto const& op = turn.operations[j];
            if (op.enriched.component_id == component_id
                && is_included::apply(op)
                && is_target_operation<TargetOperation>(turns, {turn_index, j}))
            {
                result.insert(get_node_id(turns, op.enriched.travels_to_ip_index));
            }
        }
    }
    return result;
}

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_NODE_UTIL_HPP
