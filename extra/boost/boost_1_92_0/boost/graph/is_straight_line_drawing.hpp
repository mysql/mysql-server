//=======================================================================
// Copyright 2007 Aaron Windsor
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//=======================================================================
#ifndef __IS_STRAIGHT_LINE_DRAWING_HPP__
#define __IS_STRAIGHT_LINE_DRAWING_HPP__

#include <boost/config.hpp>
#include <boost/next_prior.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/planar_detail/bucket_sort.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include <algorithm>
#include <vector>
#include <map>

namespace boost
{

template<typename Int128>
int orientation2d(Int128 ax, Int128 ay,
                  Int128 bx, Int128 by,
                  Int128 cx, Int128 cy) {
    // If coordinates are in [0, 2^63], this will not overflow.
    const Int128 detleft = (bx - ax) * (cy - ay);
    const Int128 detright = (by - ay) * (cx - ax);
    return detleft > detright ? 1 : (detleft == detright ? 0 : -1);
}

template<typename Point>
bool between(const Point& a, const Point& b, const Point& c) {
    return (b.y == a.y ?
          std::min(a.x, b.x) < c.x && c.x < std::max(a.x, b.x)
        : std::min(a.y, b.y) < c.y && c.y < std::max(a.y, b.y));
}

// Crosses in the sense the e and f intersect but are not equal and the
// intersection set is not a shared endpoint.
template<typename Graph, typename GridPositionMap>
bool crosses(typename graph_traits<Graph>::edge_descriptor e,
             typename graph_traits<Graph>::edge_descriptor f,
             Graph const &g,
             GridPositionMap const &drawing)
{
    const auto& p1 = drawing[source(e, g)];
    const auto& p2 = drawing[target(e, g)];
    const auto& q1 = drawing[source(f, g)];
    const auto& q2 = drawing[target(f, g)];

    using boost::multiprecision::int128_t;
    int o1 = orientation2d<int128_t>(p1.x, p1.y, p2.x, p2.y, q1.x, q1.y);
    int o2 = orientation2d<int128_t>(p1.x, p1.y, p2.x, p2.y, q2.x, q2.y);
    int o3 = orientation2d<int128_t>(q1.x, q1.y, q2.x, q2.y, p1.x, p1.y);
    int o4 = orientation2d<int128_t>(q1.x, q1.y, q2.x, q2.y, p2.x, p2.y);

    // X-like crossing of (p1, p2), (q1, q2)
    return ((o1 * o2 < 0) && (o3 * o4 < 0))
        // T-like crossing, e.g. q1 in (p1, p2), or partial overlap
        || (o1 == 0 && between(p1, p2, q1))
        || (o2 == 0 && between(p1, p2, q2))
        || (o3 == 0 && between(q1, q2, p1))
        || (o4 == 0 && between(q1, q2, p2));
}

template < typename Graph, typename GridPositionMap, typename VertexIndexMap >
bool is_straight_line_drawing(
    const Graph& g, GridPositionMap drawing, VertexIndexMap)
{

    typedef typename graph_traits< Graph >::vertex_descriptor vertex_t;
    typedef typename graph_traits< Graph >::edge_descriptor edge_t;
    typedef typename graph_traits< Graph >::edge_iterator edge_iterator_t;

    typedef std::size_t x_coord_t;
    typedef std::size_t y_coord_t;
    typedef boost::tuple< edge_t, x_coord_t, y_coord_t > edge_event_t;
    typedef typename std::vector< edge_event_t > edge_event_queue_t;

    typedef tuple< y_coord_t, y_coord_t, x_coord_t, x_coord_t >
        active_map_key_t;
    typedef edge_t active_map_value_t;
    typedef std::map< active_map_key_t, active_map_value_t > active_map_t;
    typedef typename active_map_t::iterator active_map_iterator_t;

    edge_event_queue_t edge_event_queue;
    active_map_t active_edges;

    edge_iterator_t ei, ei_end;
    for (boost::tie(ei, ei_end) = edges(g); ei != ei_end; ++ei)
    {
        edge_t e(*ei);
        vertex_t s(source(e, g));
        vertex_t t(target(e, g));
        edge_event_queue.push_back(
            make_tuple(e, static_cast< std::size_t >(drawing[s].x),
                static_cast< std::size_t >(drawing[s].y)));
        edge_event_queue.push_back(
            make_tuple(e, static_cast< std::size_t >(drawing[t].x),
                static_cast< std::size_t >(drawing[t].y)));
    }

    // Order by edge_event_queue by first, then second coordinate
    // (bucket_sort is a stable sort.)
    bucket_sort(edge_event_queue.begin(), edge_event_queue.end(),
        property_map_tuple_adaptor< edge_event_t, 2 >());

    bucket_sort(edge_event_queue.begin(), edge_event_queue.end(),
        property_map_tuple_adaptor< edge_event_t, 1 >());

    typedef typename edge_event_queue_t::iterator event_queue_iterator_t;
    event_queue_iterator_t itr_end = edge_event_queue.end();
    for (event_queue_iterator_t itr = edge_event_queue.begin(); itr != itr_end;
         ++itr)
    {
        edge_t e(get< 0 >(*itr));
        vertex_t source_v(source(e, g));
        vertex_t target_v(target(e, g));
        if (drawing[source_v].y > drawing[target_v].y)
            std::swap(source_v, target_v);

        active_map_key_t key(get(drawing, source_v).y, get(drawing, target_v).y,
            get(drawing, source_v).x, get(drawing, target_v).x);

        active_map_iterator_t a_itr = active_edges.find(key);
        if (a_itr == active_edges.end())
        {
            active_edges[key] = e;
        }
        else
        {
            active_map_iterator_t before, after;
            if (a_itr == active_edges.begin())
                before = active_edges.end();
            else
                before = prior(a_itr);
            after = boost::next(a_itr);

            if (before != active_edges.end())
            {
                edge_t f = before->second;
                if (crosses(e, f, g, drawing))
                    return false;
            }

            if (after != active_edges.end())
            {
                edge_t f = after->second;
                if (crosses(e, f, g, drawing))
                    return false;
            }

            active_edges.erase(a_itr);
        }
    }

    return true;
}

template < typename Graph, typename GridPositionMap >
bool is_straight_line_drawing(const Graph& g, GridPositionMap drawing)
{
    return is_straight_line_drawing(g, drawing, get(vertex_index, g));
}

}

#endif // __IS_STRAIGHT_LINE_DRAWING_HPP__
