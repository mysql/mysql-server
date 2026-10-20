// Boost.Geometry

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_DETECT_ARTICULATION_POINTS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_DETECT_ARTICULATION_POINTS_HPP

#include <map>
#include <set>

#include <boost/geometry/algorithms/detail/signed_size_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/segment_identifier.hpp>
#include <boost/geometry/algorithms/detail/overlay/graph/node_util.hpp>
#include <boost/geometry/algorithms/detail/overlay/graph/graph_util.hpp>

#include <boost/geometry/algorithms/detail/overlay/graph/debug_graph.hpp>

#include <boost/graph/biconnected_components.hpp>
#include <boost/graph/adjacency_list.hpp>

#if ! defined(BOOST_GEOMETRY_OVERLAY_NO_THROW)
#include <boost/geometry/core/exception.hpp>
#include <boost/throw_exception.hpp>
#endif

#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
#include <fstream>
#endif

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

struct vertex_info
{
    signed_size_type node_id{0};

    set_of_size_t target_vertex_indices;

    bool is_extra{false};

    // For extra nodes, also store the original node
    signed_size_type original_node_id{0};
};

struct state_type
{
    // Using a flatmap changes behavior and cause errors.
    // Using an ordered map gives a just a tiny bit of performance improvement, sometimes.
    // Maps from vertex to vertex info
    std::map<std::size_t, vertex_info> vertex_map;

    // Reverse mapping. Every node (turn or cluster) has only ONE vertex,
    // but there might be so called "extra" vertices, not associated with a node.
    std::map<signed_size_type, std::size_t> node_to_vertex_index;

    // For each edge, store the segment identifier
    std::map<std::pair<std::size_t, std::size_t>, segment_identifier> edge_to_seg_id;

    // Keeps track of vertex index, which must, for Boost.Graph, be consecutive.
    // The turn index is not consecutive (because of discarded, and of clusters).
    std::size_t vertex_index{0};

    // For round-trips extra nodes are inserted.
    // Round-trips are operations returning to itself, for example in some uu cases.
    // They are numbered from turn.size() and up, such that they have a unique positive id.
    std::size_t extra_node_id{0};
};

inline void add_edge(signed_size_type source_node_id, signed_size_type target_node_id,
        segment_identifier const& seg_id, state_type& state)
{
    // Insert the source and target node (turn or cluster)
    auto it_source = state.node_to_vertex_index.find(source_node_id);
    if (it_source == state.node_to_vertex_index.end())
    {
        it_source = state.node_to_vertex_index.insert({source_node_id, state.vertex_index++}).first;
    }

    auto it_target = state.node_to_vertex_index.find(target_node_id);
    if (it_target == state.node_to_vertex_index.end())
    {
        it_target = state.node_to_vertex_index.insert({target_node_id, state.vertex_index++}).first;

        // Get the accompanying vertex info (might be a new record)
        auto& target_vertex_info = state.vertex_map[it_target->second];
        target_vertex_info.node_id = target_node_id;
    }

    // Get the accompanying vertex info (might be a new record)
    // and store node and the segment id for this edge
    auto& vertex_info = state.vertex_map[it_source->second];
    vertex_info.node_id = source_node_id;
    state.edge_to_seg_id[{it_source->second, it_target->second}] = seg_id;

    if (target_node_id != source_node_id)
    {
        // The normal case, Not a round trip
        vertex_info.target_vertex_indices.insert(it_target->second);
        return;
    }

    // For a round trip, add an extra vertex.
    // It is not necessary to add them to the node_to_vertex_index,
    // because they won't be looked up further.
    std::size_t const extra_node_id = state.extra_node_id++;
    std::size_t const extra_vertex_index = state.vertex_index++;

    // Store the segment id in both of these edges
    auto& extra_vertex_info = state.vertex_map[extra_vertex_index];
    extra_vertex_info.node_id = extra_node_id;
    state.edge_to_seg_id[{it_source->second, extra_vertex_index}] = seg_id;
    state.edge_to_seg_id[{extra_vertex_index, it_target->second}] = seg_id;

    extra_vertex_info.is_extra = true;
    extra_vertex_info.original_node_id = source_node_id;
    extra_vertex_info.target_vertex_indices.insert(it_target->second);

    vertex_info.target_vertex_indices.insert(extra_vertex_index);
}

template <operation_type TargetOperation, typename Turns, typename Clusters>
void fill_vertex_map(Turns const& turns, Clusters const& clusters, state_type& state)
{
    std::set<edge_info> edges;
    for (auto const& key_value : clusters)
    {
        // The node id is negative for clusters
        auto const cluster_node_id = -key_value.first;
        auto const& cluster = key_value.second;
        for (std::size_t turn_index : cluster.turn_indices)
        {
            auto const& turn = turns[turn_index];
            get_target_operations<TargetOperation>(turns, turn, turn_index, cluster_node_id, edges);
        }
    }
    for (std::size_t i = 0; i < turns.size(); i++)
    {
        auto const& turn = turns[i];
        if (turn.discarded || turn.is_clustered())
        {
            continue;
        }
        get_target_operations<TargetOperation>(turns, turn, i, i, edges);
    }
    for (auto const& edge : edges)
    {
        add_edge(edge.source_node_id, edge.target_node_id, edge.seg_id, state);
    }
}

// Assigns biconnected components to turns
template <typename Turns, typename Clusters, typename Graph, typename Components>
void assign_biconnected_component_ids(Turns& turns, Clusters const& clusters, bool allow_closed,
    Graph const& graph, Components const& component, state_type const& state)
{
    auto node_id_from_it = [](auto const& it)
    {
        return it->second.is_extra
            ? it->second.original_node_id
            : it->second.node_id;
    };

    typename graph_traits<Graph>::edge_iterator ei, ei_end;
    for (boost::tie(ei, ei_end) = edges(graph); ei != ei_end; ++ei)
    {
        auto it_source = state.vertex_map.find(source(*ei, graph));
        auto it_target = state.vertex_map.find(target(*ei, graph));
        if (it_source == state.vertex_map.end() || it_target == state.vertex_map.end())
        {
#if ! defined(BOOST_GEOMETRY_OVERLAY_NO_THROW)
            BOOST_THROW_EXCEPTION(logic_exception("Edge not found in vertex map"));
#endif
            continue;
        }

        auto const source_node_id = node_id_from_it(it_source);
        auto const target_node_id = node_id_from_it(it_target);
        auto const edge_seg_id = state.edge_to_seg_id.at({source(*ei, graph), target(*ei, graph)});

        auto const turn_indices = get_turn_indices_by_node_id(turns, clusters, source_node_id,
            allow_closed);

        // Assign the component to all the operations
        // going from the source node to the target node.
        for (auto const& turn_index : turn_indices)
        {
            auto& turn = turns[turn_index];
            for (std::size_t j = 0; j < 2; j++)
            {
                auto& op = turn.operations[j];
                if (op.enriched.travels_to_ip_index < 0)
                {
                    continue;
                }

                auto const travels_to_node_id = get_node_id(turns, op.enriched.travels_to_ip_index);
                if (travels_to_node_id == target_node_id && op.seg_id == edge_seg_id)
                {
                    op.enriched.component_id = static_cast<int>(component[*ei]);
                    if (turn.both(operation_continue))
                    {
                        // For cc, always set both operations (only one of them is returned by get_node_id)
                        auto& other_op = turn.operations[1 - j];
                        other_op.enriched.component_id = op.enriched.component_id;
                    }
                }
            }
        }
    }
}

template <operation_type TargetOperation, typename Turns, typename Clusters>
void detect_biconnected_components(Turns& turns, Clusters const& clusters)
{
    using graph_t = boost::adjacency_list
        <
            boost::vecS,
            boost::vecS,
            boost::undirectedS,
            boost::no_property,
            boost::property<edge_component, std::size_t>
        >;

    // Mapping to add turns to vertices, count them, and then build the graph.
    // (It is convenient if the vertex index is the same as the turn index.
    // Therefore the default mapping is made like that, extra vertices
    // are added later)

    state_type state;
    state.extra_node_id = static_cast<std::size_t>(turns.size());

    fill_vertex_map<TargetOperation>(turns, clusters, state);

    // Build the graph from the vertices
    graph_t graph(state.vertex_map.size());
    for (auto const& key_value : state.vertex_map)
    {
        auto const vertex_index = key_value.first;
        for (auto const target_vertex_index : key_value.second.target_vertex_indices)
        {
            boost::add_edge(vertex_index, target_vertex_index, graph);
        }
    }

    edge_component ec;
    auto component = boost::get(ec, graph);
    biconnected_components(graph, component);
    fix_components(component, graph);

    assign_biconnected_component_ids(turns, clusters,
        TargetOperation == operation_intersection,
        graph, component, state);

#if defined(BOOST_GEOMETRY_DEBUG_TRAVERSE_GRAPH)
    {
        std::ofstream out("/tmp/graph_viz.dot");
        write_graph_viz(out, turns, clusters, graph, component, state.vertex_map);
    }
#endif
}

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_DETECT_ARTICULATION_POINTS_HPP
