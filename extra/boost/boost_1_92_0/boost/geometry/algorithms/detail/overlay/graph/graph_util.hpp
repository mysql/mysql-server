// Boost.Geometry

// Copyright (c) 2025 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GRAPH_UTIL_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GRAPH_UTIL_HPP

#include <boost/graph/biconnected_components.hpp>
#include <boost/graph/adjacency_list.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

struct edge_component
{
    using kind = edge_property_tag;
};

// It appears that in an undirected graph, the components for two edges are sometimes different.
// It happens a lot in the unit tests, for example in test case "#case_recursive_boxes_93"
// Fix that. To be found out why this is.
template <typename Graph, typename Components>
void fix_components(Components& components, Graph const& g)
{
    typename graph_traits<Graph>::edge_iterator ei, ei_end;
    for (boost::tie(ei, ei_end) = edges(g); ei != ei_end; ++ei)
    {
        auto& component = components[*ei];

        auto const source_vertex = source(*ei, g);
        auto const target_vertex = target(*ei, g);

        // Get the reverse edge and its component
        auto const reverse_edge_pair = edge(target_vertex, source_vertex, g);
        if (! reverse_edge_pair.second)
        {
            continue;
        }

        auto& reverse_component = components[reverse_edge_pair.first];

        if (component != reverse_component)
        {
            component = reverse_component;
        }
    }
}

}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_GRAPH_UTIL_HPP
