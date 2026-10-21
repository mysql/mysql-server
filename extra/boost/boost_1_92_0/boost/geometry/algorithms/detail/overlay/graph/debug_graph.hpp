// Boost.Geometry

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_DEBUG_GRAPH_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_DEBUG_GRAPH_HPP

#include <boost/geometry/core/access.hpp>

#include <ostream>
#include <iostream>

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

// For debug purposes only
template <typename Turns, typename Clusters, typename Graph, typename Components, typename VertexMap>
void write_graph_viz(std::ostream& out, Turns const& turns, Clusters const& clusters,
    Graph const& g, Components const& component, VertexMap const& vertex_map,
    bool use_absolute_position = true)
{
    out << "graph A {\n  node[shape=\"circle\"]\n";

    auto add_pos = [&](auto const& point)
    {
        out << ", pos=\"" << geometry::get<0>(point) << "," << geometry::get<1>(point) << "!\"";
    };

    // List all nodes
    for (auto const& vertex_pair : vertex_map)
    {
        auto const& vertex = vertex_pair.second;

        out << vertex.node_id << "[label=\"" << vertex.node_id << "\"";
        if (use_absolute_position)
        {
            if (vertex.node_id < 0)
            {
                // Use any point from the cluster
                auto it = clusters.find(vertex.node_id);
                if (it != clusters.end())
                {
                    auto const& cluster = it->second;
                    if (! cluster.turn_indices.empty())
                    {
                        add_pos(turns[*cluster.turn_indices.begin()].point);
                    }
                }
            }
            else if (vertex.node_id < static_cast<int>(turns.size()))
            {
                add_pos(turns[vertex.node_id].point);
            }
            else if (vertex.original_node_id >= 0 && vertex.original_node_id < turns.size())
            {
                // It is an extra node. It should be placed somewhere in the neighborhood
                // of the connected node. Where depends on the situation, it is currently not worth
                // the effort to get that. Just displace it a bit to the lower left.
                auto point = turns[vertex.original_node_id].point;
                geometry::set<0>(point, geometry::get<0>(point) - 1.0);
                geometry::set<1>(point, geometry::get<1>(point) - 1.0);
                add_pos(point);
            }
        }
        out << "]\n";
    }

    typename graph_traits<Graph>::edge_iterator ei, ei_end;
    for (boost::tie(ei, ei_end) = edges(g); ei != ei_end; ++ei)
    {
        auto const source_vertex = source(*ei, g);
        auto const target_vertex = target(*ei, g);
        auto it_source = vertex_map.find(source_vertex);
        auto it_target = vertex_map.find(target_vertex);
        if (it_source == vertex_map.end() || it_target == vertex_map.end())
        {
            std::cerr << "Edge not found FOR GRAPH_VIZ "
                        << source_vertex << " -- " << target_vertex
                        << std::endl;
            continue;
        }
        auto const source_node_id = it_source->second.node_id;
        auto const target_node_id = it_target->second.node_id;

        out << source_node_id << " -- " << target_node_id
                  << "[label=\""
                    //<< source_node_id << ".." << target_node_id << " ("
                    << component[*ei]
                    // << ")"
                    << "\"]"
                  << '\n';
    }
    out << "}\n";
}

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_DEBUG_GRAPH_HPP
